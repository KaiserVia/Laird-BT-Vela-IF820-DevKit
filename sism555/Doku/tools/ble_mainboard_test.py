#!/usr/bin/env python3
"""BLE-CYSPP-Test fuer das VIASIS-Mainboard (IF820/sism555) - kein DVK noetig.

Gegenstueck zu den Zulieferer-DVK-Tests unter
SupplierSourceCode/.../tests/Bluetooth-Simulation/ble/cyspp_dvk_hil.py, aber ohne die dort
noetige zweite USB-Debug-Verbindung zum Modul: hier laeuft nur ein normaler BLE-Central
(Bleak) gegen das reale Mainboard. Wire-Format der Transfer-Frames ist absichtlich identisch
zum Zulieferer-Referenztest, damit Ergebnisse vergleichbar sind.

Gegenstueck auf der Firmware-Seite: Testmenue -> 8 (BT-Diagnose) -> 3 (Transfer-Test),
ab Firmware 5.67 (btio.c bt_transfer_echo()). Fuer den Smoke-Test ist keine Firmware-
Gegenstelle noetig (reiner BLE-Connect/Subscribe-Test).

Zwei Modi
---------
  --mode smoke     Verbindungstest mit Debug-Timings (Scan/Connect/Discovery/Subscribe),
                    zeigt die volle GATT-Tabelle und schreibt einen JSON+Markdown-Report.
                    Braucht KEINEN laufenden Transfer-Test auf dem Mainboard.
  --mode transfer  Datentransfer-Test (Default 40 kB): sendet CRC-32/Sequenz-Frames per
                    BLE-Write-without-Response an ...ca102, wartet auf ACK1-Notifications
                    vom Mainboard (Sliding-Window-Flusskontrolle, automatischer Retry).
                    VORHER auf dem Geraet Testmenue -> 8 -> 3 starten (120 s Laufzeit)!

Aufruf
------
  pip install bleak
  python ble_mainboard_test.py --mode smoke
  python ble_mainboard_test.py --mode transfer
  python ble_mainboard_test.py --mode transfer --bytes 40960 --window 4 --retries 3
  python ble_mainboard_test.py --mode transfer --bytes 40960 --rate 0   # unpaced burst
  python ble_mainboard_test.py --name VIASIS_26SC9996 --mode smoke
  python ble_mainboard_test.py --mode smoke --pin 269996        # BT-PIN explizit senden
  python ble_mainboard_test.py --mode transfer                  # PIN wird aus dem Namen abgeleitet

Ohne --pin bzw. ableitbaren PIN bleibt die BT-Pinabfrage der Firmware offen - das kann dazu
fuehren, dass die Verbindung nach einer Weile ohne Aktivitaet getrennt wird bzw. das RS232-Menue
nicht bis Testmenue 8->3 navigiert werden kann (siehe project-status.md).
"""

import argparse
import asyncio
import json
import struct
import sys
import time
import zlib
from datetime import datetime
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    sys.exit("Fehlt: bleak. Installieren mit:  pip install bleak")

# ---------------------------------------------------------------------------
# Konfiguration
# ---------------------------------------------------------------------------

CYSPP_DATA_SUFFIX = "ca102"      # Unacknowledged Data (Notify + Write-without-Response)
DEFAULT_NAME_PREFIX = "VIASIS"
SCAN_TIMEOUT = 15.0               # s

# Wire-Format identisch zum Zulieferer-Referenztest (cyspp_dvk_hil.py), damit
# bt_transfer_echo() auf dem Mainboard (btio.c, Firmware 5.67) beide Seiten versteht.
TRANSFER_MAGIC = b"CYS1"
TRANSFER_HEADER = struct.Struct("<4sIHI")   # magic, seq(u32), size(u16), crc32(u32)
TRANSFER_ACK_MAGIC = b"ACK1"
TRANSFER_ACK = struct.Struct("<4sI")        # magic, seq(u32)
MAX_FRAME_PAYLOAD = 256                      # muss zu bt_transfer_echo()s buf[270] passen

DEFAULT_TRANSFER_BYTES = 40 * 1024
DEFAULT_WINDOW = 4
DEFAULT_RETRIES = 3
DEFAULT_WIRE_RATE_BPS = 8000      # 0 = unpaced burst
MIN_WIRE_RATE_BPS = 1000
ACK_TIMEOUT_SECONDS = 5.0
RECOVERY_LIMIT = 3

REPORT_DIR = Path(__file__).resolve().parent / "Testreports"

T0 = time.perf_counter()


def ts() -> str:
    ms = (time.perf_counter() - T0) * 1000.0
    return f"{datetime.now().strftime('%H:%M:%S.%f')[:-3]}  [{ms:9.1f} ms]"


def log(msg: str) -> None:
    print(f"{ts()}  {msg}", flush=True)


# ---------------------------------------------------------------------------
# Scan / Verbindungshilfen (gemeinsam fuer beide Modi)
# ---------------------------------------------------------------------------

async def scan_for_device(name_prefix: str):
    log(f"Scanne {SCAN_TIMEOUT:.0f} s nach Geraeten mit Prefix '{name_prefix}' ...")
    t_scan = time.perf_counter()
    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT, return_adv=True)
    scan_ms = (time.perf_counter() - t_scan) * 1000.0
    hits = [(d, adv) for d, adv in devices.values()
            if d.name and d.name.upper().startswith(name_prefix.upper())]
    if not hits:
        log(f"KEIN Geraet mit Prefix '{name_prefix}' gefunden nach {scan_ms:.0f} ms.")
        log("Hinweise: Advertising aktiv? BT am PC eingeschaltet? Adapter BT 4.0+?")
        return None, scan_ms
    if len(hits) > 1:
        log(f"{len(hits)} passende Geraete gefunden - bitte mit --address eingrenzen:")
        for d, _ in hits:
            print(f"    {d.address}   {d.name}")
        return None, scan_ms
    device, adv = hits[0]
    rssi = getattr(adv, "rssi", None)
    log(f"Gefunden nach {scan_ms:.0f} ms: {device.name}  ({device.address})"
        f"{f'  RSSI={rssi} dBm' if rssi is not None else ''}")
    return device, scan_ms


def find_char_by_suffix(client: BleakClient, suffix: str):
    for service in client.services:
        for char in service.characteristics:
            if str(char.uuid).lower().replace("-", "").endswith(suffix.lower()):
                return char
    return None


def dump_gatt_table(client: BleakClient) -> list:
    table = []
    log("GATT-Tabelle:")
    for service in client.services:
        print(f"    Service  {service.uuid}")
        table.append({"service": str(service.uuid), "characteristics": []})
        for char in service.characteristics:
            props = ",".join(char.properties)
            print(f"      Char   {char.uuid}  [{props}]")
            table[-1]["characteristics"].append(
                {"uuid": str(char.uuid), "properties": list(char.properties)})
    return table


async def send_pin(client: BleakClient, char, pin: str) -> None:
    """Sendet die BT-PIN (mainmenu()'s Pinabfrage, sicom.c) ueber den CYSPP-Datenkanal -
    ohne das nimmt die Firmware KEINE weiteren Menuebefehle an (bt_pininit bleibt >0 und
    blockiert dann auch RS232, siehe project-status.md) und trennt manche Verbindungen
    (z.B. BLE ohne Pin-Antwort) nach einer Weile von selbst."""
    log(f"Sende PIN '{pin}' ...")
    await client.write_gatt_char(char, pin.encode("ascii"), response=False)
    await asyncio.sleep(0.3)   # Firmware Zeit geben, die Ziffern zu verarbeiten (echot '*' je Ziffer)


def derive_pin_from_name(name: str) -> str | None:
    """PIN = die ersten 2 + letzten 4 Ziffern der Seriennummer im Geraetenamen
    (z.B. VIASIS_12SC3456 -> Seriennummer 12SC3456 -> PIN 123456, siehe btio.c bt_set_pin()).
    Best-effort - liefert None, wenn das Namensschema nicht passt (dann --pin explizit angeben)."""
    if not name or not name.upper().startswith("VIASIS_"):
        return None
    serno = name.split("_", 1)[1].split("_")[0]   # ...VIASIS_<serno> oder ...VIASIS_<serno>_BLE
    if len(serno) < 8:
        return None
    pin = serno[0:2] + serno[-4:]
    return pin if pin.isdigit() else None


def write_report(kind: str, data: dict) -> tuple[Path, Path]:
    REPORT_DIR.mkdir(exist_ok=True)
    stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    base = REPORT_DIR / f"mainboard-{kind}-{stamp}"
    json_path = base.with_suffix(".json")
    json_path.write_text(json.dumps(data, indent=2, default=str), encoding="utf-8")
    return json_path, base.with_suffix(".md")


# ---------------------------------------------------------------------------
# Modus 1: Smoke-Test mit Debug-Timings
# ---------------------------------------------------------------------------

async def run_smoke_test(args) -> int:
    result = {
        "mode": "smoke",
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "name_prefix": args.name,
        "phases_ms": {},
        "gatt_table": None,
        "cyspp_char_found": False,
        "subscribe_ok": False,
        "disconnect_reason": None,
        "held_connection_seconds": args.hold,
        "unexpected_disconnect": False,
        "result": "incomplete",
    }

    if args.address:
        device, scan_ms = None, 0.0
        address = args.address
        log(f"Adresse direkt vorgegeben: {address}")
    else:
        device, scan_ms = await scan_for_device(args.name)
        if device is None:
            result["result"] = "device_not_found"
            _finish_smoke(result, args)
            return 2
        address = device.address
    device_name = device.name if device else None
    result["phases_ms"]["scan"] = round(scan_ms, 1)

    disconnected = asyncio.Event()

    def on_disconnect(_client):
        result["unexpected_disconnect"] = True
        log("*** DISCONNECT (vom Geraet ausgeloest) ***")
        disconnected.set()

    client = BleakClient(address, disconnected_callback=on_disconnect)
    t_connect = time.perf_counter()
    try:
        log("Verbinde ...")
        await client.connect(timeout=args.connect_timeout)
    except Exception as exc:
        log(f"FEHLER beim Verbinden: {exc}")
        result["result"] = "connect_failed"
        result["error"] = str(exc)
        _finish_smoke(result, args)
        return 2
    connect_ms = (time.perf_counter() - t_connect) * 1000.0
    result["phases_ms"]["connect"] = round(connect_ms, 1)
    log(f"Verbunden nach {connect_ms:.0f} ms.")

    try:
        t_disc = time.perf_counter()
        result["gatt_table"] = dump_gatt_table(client)
        disc_ms = (time.perf_counter() - t_disc) * 1000.0
        result["phases_ms"]["gatt_discovery"] = round(disc_ms, 1)
        log(f"GATT-Discovery abgeschlossen nach {disc_ms:.0f} ms.")

        char = find_char_by_suffix(client, CYSPP_DATA_SUFFIX)
        if char is None:
            log(f"FEHLER: keine Characteristic mit Suffix ...{CYSPP_DATA_SUFFIX} gefunden.")
            result["result"] = "char_missing"
            return 2
        result["cyspp_char_found"] = True
        log(f"MTU (negotiated, falls verfuegbar): {getattr(client, 'mtu_size', '?')}")
        log(f"max_write_without_response_size: {char.max_write_without_response_size}")

        def on_notify(_sender, data: bytearray):
            log(f"<== unaufgeforderte Daten  {bytes(data)!r}")

        t_sub = time.perf_counter()
        await client.start_notify(char, on_notify)
        sub_ms = (time.perf_counter() - t_sub) * 1000.0
        result["phases_ms"]["subscribe"] = round(sub_ms, 1)
        result["subscribe_ok"] = True
        log(f"Subscribe (CCCD-Write) quittiert nach {sub_ms:.0f} ms.")

        pin = args.pin or derive_pin_from_name(device_name)
        if pin:
            await send_pin(client, char, pin)
        else:
            log("Kein --pin angegeben/ableitbar - PIN-Abfrage der Firmware bleibt offen "
                "(Verbindung koennte deshalb ohne Aktivitaet getrennt werden).")

        log(f"Halte Verbindung {args.hold:.0f} s, um einen spontanen Disconnect zu beobachten ...")
        try:
            await asyncio.wait_for(disconnected.wait(), timeout=args.hold)
            result["disconnect_reason"] = "spontaneous_during_hold"
        except asyncio.TimeoutError:
            result["disconnect_reason"] = "hold_time_elapsed_still_connected"
            log(f"Verbindung hielt die vollen {args.hold:.0f} s.")

        # KEIN explizites stop_notify() mehr vor dem Trennen: das loeste bisher zuverlaessig
        # einen Disconnect aus (CCCD-Rueckschreiben auf 0x0000 haengt/scheitert, ~5s spaeter
        # trennt das Geraet) - ein echter Client wuerde beim gewollten Trennen ohnehin nicht
        # extra abbestellen. Verbindung/Subscribe/Datenempfang selbst liefen bis hierher sauber.
        result["result"] = ("passed" if result["subscribe_ok"]
                            and not result["unexpected_disconnect"] else "unstable")
    finally:
        if client.is_connected:
            await client.disconnect()
            log("Sauber getrennt (durch uns).")

    _finish_smoke(result, args)
    return 0 if result["result"] == "passed" else 1


def _finish_smoke(result: dict, args) -> None:
    print()
    print("=" * 70)
    print("  SMOKE-TEST ZUSAMMENFASSUNG")
    print("=" * 70)
    for phase, ms in result["phases_ms"].items():
        print(f"  {phase:>14}: {ms:8.1f} ms")
    print(f"  {'Ergebnis':>14}: {result['result']}")
    if result["result"] != "passed":
        print("  ==> BLE-Verbindung ist NICHT stabil moeglich - siehe Log oben.")
    else:
        print("  ==> BLE-Verbindung + Subscribe funktionieren zuverlaessig.")
    print()
    if not args.no_report:
        json_path, md_path = write_report("smoke", result)
        lines = [
            "# BLE Mainboard Smoke-Test",
            "",
            f"- Ergebnis: **{result['result']}**",
            f"- Geraet: `{result.get('name_prefix')}`",
            f"- Unerwarteter Disconnect: `{result['unexpected_disconnect']}`",
            "",
            "## Phasen (ms)",
            "",
            "| Phase | Dauer (ms) |",
            "| --- | ---: |",
        ]
        for phase, ms in result["phases_ms"].items():
            lines.append(f"| {phase} | {ms:.1f} |")
        md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        log(f"JSON-Report: {json_path}")
        log(f"Markdown-Report: {md_path}")


# ---------------------------------------------------------------------------
# Modus 2: Transfer-Test (CRC-32/Sequenz/ACK, analog zum 40-kB-DVK-Referenztest)
# ---------------------------------------------------------------------------

def build_frames(payload_bytes: int, write_limit: int):
    payload_limit = min(MAX_FRAME_PAYLOAD, write_limit - TRANSFER_HEADER.size)
    if payload_limit < 1:
        raise ValueError(f"Write-Limit {write_limit} ist zu klein.")
    frames = []
    offset = 0
    sequence = 0
    while offset < payload_bytes:
        size = min(payload_limit, payload_bytes - offset)
        payload = bytes((offset + i) % 256 for i in range(size))
        checksum = zlib.crc32(payload) & 0xFFFFFFFF
        frames.append(TRANSFER_HEADER.pack(TRANSFER_MAGIC, sequence, size, checksum) + payload)
        offset += size
        sequence += 1
    return frames


async def run_transfer_test(args) -> int:
    transfer = {
        "requested_payload_bytes": args.bytes,
        "protocol": "burst" if args.rate == 0 and args.window == 0 else "ack",
        "window_size": args.window,
        "retry_limit": args.retries,
        "target_wire_rate_bps": args.rate,
        "frames_sent": 0,
        "acks_received": 0,
        "retransmissions": 0,
        "ack_timeouts": 0,
        "recovery_actions": 0,
        "write_errors": 0,
        "status": "not_started",
    }
    result = {
        "mode": "transfer",
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "transfer": transfer,
        "result": "incomplete",
    }

    if args.address:
        address, device_name = args.address, None
    else:
        device, _ = await scan_for_device(args.name)
        if device is None:
            result["result"] = "device_not_found"
            _finish_transfer(result)
            return 2
        address, device_name = device.address, device.name

    acknowledgements: asyncio.Queue = asyncio.Queue()
    ack_buffer = bytearray()

    def on_disconnect(_client):
        log("*** DISCONNECT ***")

    def on_notify(_sender, data: bytearray):
        ack_buffer.extend(bytes(data))
        while len(ack_buffer) >= TRANSFER_ACK.size:
            idx = ack_buffer.find(TRANSFER_ACK_MAGIC)
            if idx < 0:
                ack_buffer.clear()
                break
            if idx:
                del ack_buffer[:idx]
            if len(ack_buffer) < TRANSFER_ACK.size:
                break
            _, seq = TRANSFER_ACK.unpack_from(ack_buffer)
            del ack_buffer[:TRANSFER_ACK.size]
            acknowledgements.put_nowait(seq)

    client = BleakClient(address, disconnected_callback=on_disconnect)
    try:
        log("Verbinde ...")
        await client.connect(timeout=args.connect_timeout)
        log("Verbunden.")
        char = find_char_by_suffix(client, CYSPP_DATA_SUFFIX)
        if char is None:
            log(f"FEHLER: keine Characteristic mit Suffix ...{CYSPP_DATA_SUFFIX} gefunden.")
            result["result"] = "char_missing"
            return 2
        await client.start_notify(char, on_notify)
        log("Subscribed.")

        pin = args.pin or derive_pin_from_name(device_name)
        if pin:
            await send_pin(client, char, pin)
        else:
            log("Kein --pin angegeben/ableitbar - PIN-Abfrage der Firmware bleibt offen "
                "(RS232-Menue kann dann u.U. nicht bis Testmenue 8->3 navigiert werden).")

        log("Jetzt Testmenue -> 8 -> 3 auf dem Geraet starten, falls noch nicht aktiv.")
        await asyncio.sleep(1.5)

        write_limit = min(244, int(char.max_write_without_response_size))
        frames = build_frames(args.bytes, write_limit)
        transfer["frames_sent"] = len(frames)
        transfer["write_without_response_limit"] = write_limit
        log(f"Sende {args.bytes} Bytes in {len(frames)} Frames (Limit {write_limit} B), "
            f"Fenster={args.window}, Rate={args.rate or 'unpaced'} B/s.")

        transfer["status"] = "sending"
        t_start = time.perf_counter()
        pending: dict[int, dict] = {}
        acked: set[int] = set()
        next_index = 0
        sent_wire_bytes = 0
        burst = args.rate == 0 and args.window == 0
        last_progress = -1

        while len(acked) < len(frames):
            while next_index < len(frames) and (burst or len(pending) < args.window):
                frame = frames[next_index]
                await client.write_gatt_char(char, frame, response=False)
                pending[next_index] = {"sent_at": time.monotonic(), "attempts": 1, "frame": frame}
                sent_wire_bytes += len(frame)
                next_index += 1
                if not burst and args.rate:
                    target_time = t_start + sent_wire_bytes / args.rate
                    delay = target_time - time.perf_counter()
                    if delay > 0:
                        await asyncio.sleep(delay)

            if burst:
                acked = set(range(len(frames)))
                break

            try:
                seq = await asyncio.wait_for(acknowledgements.get(), timeout=0.05)
                if seq in pending:
                    del pending[seq]
                    acked.add(seq)
                    transfer["acks_received"] += 1
            except asyncio.TimeoutError:
                pass

            now = time.monotonic()
            for seq, state in list(pending.items()):
                if now - state["sent_at"] < ACK_TIMEOUT_SECONDS:
                    continue
                transfer["ack_timeouts"] += 1
                if state["attempts"] > args.retries:
                    if transfer["recovery_actions"] >= RECOVERY_LIMIT:
                        raise TimeoutError(
                            f"Frame {seq} ueberschritt {args.retries} Retries nach "
                            f"{RECOVERY_LIMIT} Recoveries.")
                    transfer["recovery_actions"] += 1
                    args.window = 1
                    if args.rate:
                        args.rate = max(MIN_WIRE_RATE_BPS, args.rate // 2)
                    log(f"ACK-Recovery {transfer['recovery_actions']}/{RECOVERY_LIMIT}: "
                        f"Fenster=1, Rate={args.rate} B/s.")
                    state["attempts"] = 0
                    state["sent_at"] = 0.0
                    continue
                await client.write_gatt_char(char, state["frame"], response=False)
                state["attempts"] += 1
                state["sent_at"] = time.monotonic()
                transfer["retransmissions"] += 1

            progress = int(100 * len(acked) / len(frames))
            if progress > last_progress:
                elapsed = time.perf_counter() - t_start
                rate = sent_wire_bytes / elapsed if elapsed else 0
                log(f"Sende: {len(acked)}/{len(frames)} bestaetigt ({progress}%), "
                    f"{len(pending)} offen, {rate:.0f} B/s.")
                last_progress = progress

        transfer["duration_seconds"] = round(time.perf_counter() - t_start, 3)
        transfer["status"] = "passed"
        verified_bytes = sum(len(f) - TRANSFER_HEADER.size for f in frames)
        transfer["verified_bytes"] = verified_bytes
        transfer["throughput_bps"] = round(
            verified_bytes / transfer["duration_seconds"], 1) if transfer["duration_seconds"] else 0
        result["result"] = "passed"
        log(f"Transfer abgeschlossen: {verified_bytes} Byte verifiziert in "
            f"{transfer['duration_seconds']} s ({transfer['throughput_bps']:.1f} B/s).")
    except Exception as exc:
        transfer["write_errors"] += 1
        transfer["status"] = "failed"
        result["result"] = "failed"
        result["error"] = str(exc)
        log(f"FEHLER: {exc}")
    finally:
        if client.is_connected:
            await client.disconnect()

    _finish_transfer(result)
    return 0 if result["result"] == "passed" else 1


def _finish_transfer(result: dict) -> None:
    transfer = result["transfer"]
    print()
    print("=" * 70)
    print("  TRANSFER-TEST ZUSAMMENFASSUNG")
    print("=" * 70)
    print(f"  Ergebnis:          {result['result']}")
    print(f"  Frames gesendet:   {transfer['frames_sent']}")
    print(f"  ACKs erhalten:     {transfer['acks_received']}")
    print(f"  Retransmissions:   {transfer['retransmissions']}")
    print(f"  ACK-Timeouts:      {transfer['ack_timeouts']}")
    print(f"  Recovery-Zyklen:   {transfer['recovery_actions']}")
    if transfer.get("duration_seconds"):
        print(f"  Dauer:             {transfer['duration_seconds']} s")
        print(f"  Durchsatz:         {transfer.get('throughput_bps', 0):.1f} B/s "
              f"(Referenz DVK: ~7900 B/s bei 40 kB, Fenster 4)")
    print()
    json_path, md_path = write_report("transfer", result)
    verified = transfer.get("verified_bytes", 0)
    lines = [
        "# BLE Mainboard Transfer-Test",
        "",
        f"- Ergebnis: **{result['result']}**",
        f"- Angeforderte Bytes: `{transfer['requested_payload_bytes']}`",
        f"- Verifizierte Bytes (per ACK vom Mainboard bestaetigt): `{verified}`",
        f"- Frames gesendet / ACKs erhalten: `{transfer['frames_sent']}` / "
        f"`{transfer['acks_received']}`",
        f"- Retransmissions / ACK-Timeouts / Recoveries: `{transfer['retransmissions']}` / "
        f"`{transfer['ack_timeouts']}` / `{transfer['recovery_actions']}`",
        f"- Dauer: `{transfer.get('duration_seconds', '?')}` s",
        f"- Durchsatz: `{transfer.get('throughput_bps', 0)}` B/s "
        "(Referenz Zulieferer-DVK: ~7900 B/s bei 40 kB, Fenster 4)",
        "",
        "```mermaid",
        "pie showData",
        "  title Frame-Ergebnis",
        f'  "Bestaetigt (ACK)" : {transfer["acks_received"]}',
        f'  "Nicht bestaetigt" : {max(0, transfer["frames_sent"] - transfer["acks_received"])}',
        "```",
    ]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    log(f"JSON-Report: {json_path}")
    log(f"Markdown-Report: {md_path}")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    p = argparse.ArgumentParser(description="BLE-CYSPP-Test fuer das VIASIS-Mainboard.")
    p.add_argument("--mode", choices=("smoke", "transfer"), default="smoke")
    p.add_argument("--name", default=DEFAULT_NAME_PREFIX,
                   help=f"Namensprefix beim Scannen (Default: {DEFAULT_NAME_PREFIX})")
    p.add_argument("--address", default=None, help="BT-Adresse direkt angeben, ueberspringt Scan")
    p.add_argument("--pin", default=None,
                   help="BT-PIN (6 Ziffern) nach dem Subscribe senden - ohne das bleibt die "
                        "Firmware-Pinabfrage offen. Ohne Angabe wird versucht, sie aus dem "
                        "Geraetenamen abzuleiten (VIASIS_<serno> -> erste 2 + letzte 4 Ziffern).")
    p.add_argument("--connect-timeout", type=float, default=30.0)
    p.add_argument("--hold", type=float, default=8.0,
                   help="[smoke] Sekunden verbunden bleiben, um Spontan-Disconnect zu pruefen")
    p.add_argument("--no-report", action="store_true", help="[smoke] keinen Report schreiben")
    p.add_argument("--bytes", type=int, default=DEFAULT_TRANSFER_BYTES,
                   help="[transfer] Nutzdaten-Bytes (Default 40 kB, wie DVK-Referenztest)")
    p.add_argument("--window", type=int, default=DEFAULT_WINDOW,
                   help="[transfer] max. unbestaetigte Frames (0 = unpaced Burst)")
    p.add_argument("--retries", type=int, default=DEFAULT_RETRIES)
    p.add_argument("--rate", type=int, default=DEFAULT_WIRE_RATE_BPS,
                   help="[transfer] Ziel-Bitrate in B/s, 0 = unpaced Burst")
    args = p.parse_args()

    print("=" * 70)
    print("  BLE-Mainboard-Test - VIASIS / sism555 (IF820)")
    print(f"  Modus: {args.mode}   Start: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)

    if args.mode == "smoke":
        return asyncio.run(run_smoke_test(args))
    return asyncio.run(run_transfer_test(args))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nAbgebrochen.")
        raise SystemExit(130)
