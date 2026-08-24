#!/usr/bin/env python3
"""BLE/BT-Classic-Test fuer das VIASIS-Mainboard (IF820/sism555) - kein DVK noetig.

Gegenstueck zu den Zulieferer-DVK-Tests unter
SupplierSourceCode/.../tests/Bluetooth-Simulation/ble/cyspp_dvk_hil.py, aber ohne die dort
noetige zweite USB-Debug-Verbindung zum Modul: hier laeuft nur ein normaler BLE-Central
(Bleak) bzw. eine normale serielle COM-Port-Verbindung (BT-Classic SPP) gegen das reale
Mainboard. Wire-Format der Transfer-Frames ist absichtlich identisch zum
Zulieferer-Referenztest, damit Ergebnisse vergleichbar sind.

Gegenstueck auf der Firmware-Seite: Testmenue -> 8 (BT-Diagnose) -> 3 (Transfer-Test),
ab Firmware 5.67 (btio.c bt_transfer_echo()). Fuer den Smoke-Test ist keine Firmware-
Gegenstelle noetig (reiner Connect/Subscribe-Test).

Fuenf Modi
----------
  --mode smoke     [BLE] Verbindungstest mit Debug-Timings (Scan/Connect/Discovery/Subscribe),
                    zeigt die volle GATT-Tabelle und schreibt einen JSON+Markdown-Report.
                    Braucht KEINEN laufenden Transfer-Test auf dem Mainboard.
  --mode transfer  [BLE] Datentransfer-Test (Default 40 kB): sendet CRC-32/Sequenz-Frames per
                    BLE-Write-without-Response an ...ca102, wartet auf ACK1-Notifications
                    vom Mainboard (Sliding-Window-Flusskontrolle, automatischer Retry).
                    Navigiert VOLLAUTOMATISCH per BLE zu Testmenue -> 8 -> 3 (liest den ueber
                    CYSPP gespiegelten Menuetext mit, sendet PIN + Ziffern selbst) - keine
                    manuelle Bedienung am Geraet mehr noetig.
  --mode classic-smoke   [Classic] Verbindungstest ueber die BT-COM-Port (SPP): Port oeffnen,
                    Banner/PIN-Prompt lesen, PIN senden, Hauptmenue-Rueckkehr pruefen, trennen.
  --mode cycle     [BLE + Classic] Wiederholtes Verbinden/Trennen. --count legt fest wie oft,
                    --hold wie lange (Sekunden) die Verbindung dazwischen gehalten wird (0 =
                    sofort wieder trennen). Deckt sowohl den reinen An/Aus-Test als auch den
                    Test mit Wartezeit ab (nur ueber --hold gesteuert - zwei separate Modi
                    dafuer waeren reine Duplizierung derselben Schleife).
  --mode download  [BLE + Classic] Erzeugt --records Test-Messwerte im Flash (Testmenue ->
                    Werksmenue -> Bauteiltests -> Write flash data), navigiert zurueck ins
                    Hauptmenue und startet den ECHTEN Datenexport (Hauptmenue -> 1 -> 'C' fuer
                    Ymodem-Ausgabe, send_vtf_file() in btio.c/sicom.c). Empfaengt den Ymodem-
                    Batch-Transfer (128-Byte-Headerblock + 1k-CRC-Datenbloecke, exakt das
                    Protokoll aus xmodem.c) und misst den tatsaechlichen Durchsatz.

Transport
---------
  --transport ble       (Default) Bluetooth Low Energy ueber Bleak.
  --transport classic   Bluetooth Classic SPP ueber einen Windows-COM-Port (pyserial). Braucht
                        --port (z. B. COM5) - das Geraet muss bereits gepairt sein, der COM-Port
                        wird von Windows dem gepairten Geraet zugeordnet.

Aufruf
------
  pip install bleak pyserial
  python ble_mainboard_test.py --mode smoke
  python ble_mainboard_test.py --mode transfer
  python ble_mainboard_test.py --mode transfer --bytes 40960 --window 4 --retries 3
  python ble_mainboard_test.py --mode transfer --bytes 40960 --rate 0   # unpaced burst
  python ble_mainboard_test.py --name VIASIS_26SC9996 --mode smoke
  python ble_mainboard_test.py --mode smoke --pin 269996        # BT-PIN explizit senden
  python ble_mainboard_test.py --mode transfer                  # PIN wird aus dem Namen abgeleitet
  python ble_mainboard_test.py --transport classic --port COM5 --mode classic-smoke
  python ble_mainboard_test.py --transport classic --port COM5 --mode cycle --count 10
  python ble_mainboard_test.py --transport classic --port COM5 --mode cycle --count 5 --hold 30
  python ble_mainboard_test.py --transport classic --port COM5 --mode download --records 2000
  python ble_mainboard_test.py --transport ble --mode cycle --count 10
  python ble_mainboard_test.py --transport ble --mode download --records 2000

Ohne --pin bzw. ableitbaren PIN bleibt die BT-Pinabfrage der Firmware offen - das kann dazu
fuehren, dass die Verbindung nach einer Weile ohne Aktivitaet getrennt wird bzw. das RS232-Menue
nicht bis zum Zielpunkt navigiert werden kann (siehe project-status.md).

Hinweis zu --mode download: die Navigation durch Testmenue/Werksmenue/Bauteiltests wurde
anhand des Firmware-Quellcodes (sicom.c/sictst.c) nachvollzogen, aber NICHT an echter Hardware
verifiziert (kein Zugriff auf das Board von hier aus). Jeder Navigationsschritt wartet auf einen
eindeutigen Menuetext-Marker mit Timeout und bricht mit einer klaren Fehlermeldung ab, statt
blind weiterzumachen - sollte die Navigation trotzdem an einer unerwarteten Stelle haengen
bleiben, bitte den Menuestand am Geraet pruefen (i. d. R. reicht 'H' um zurueck ins Hauptmenue
zu kommen).
"""

import argparse
import asyncio
import struct
import sys
import time
import zlib
from datetime import datetime
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    BleakClient = BleakScanner = None  # nur noetig fuer --transport ble

try:
    import serial  # pyserial
except ImportError:
    serial = None  # nur noetig fuer --transport classic

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
# Scan / Verbindungshilfen (BLE, gemeinsam fuer smoke/transfer)
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
    await asyncio.sleep(1.5)   # Firmware/Windows-BLE-Stack Zeit geben, bevor der naechste Write
                                # folgt - zwei write-without-response kurz hintereinander schienen
                                # sich gegenseitig zu stoeren (Smoke-Test: 1 Write, lief immer;
                                # Transfer-Test: PIN dann 'H' kurz danach, schlug wiederholt fehl)


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


# Menuetext-Marker fuer die automatische Navigation (mainmenu()/sicom.c spiegelt Menueausgaben
# auch ueber den CYSPP-Kanal an BLE, siehe Boot-Banner+PIN-Ausgabe im Smoke-Test-Log). Jeder
# Schritt: warte auf den Marker der aktuellen Menueebene, dann sende die Ziffer fuer die naechste.
# Erster Schritt (marker=None): 'H' - springt aus JEDEM Menuezustand definiert zurueck ins
# Hauptmenue (sicom.c mainmenu(): "if (c=='H') { menu=0; ... }") - so ist der Startzustand
# unabhaengig davon, wo das Geraet-Menue gerade steht.
MENU_NAV_STEPS = [(None, "H"), ("Hauptmen", "3"), ("Testmen", "8"), ("BT-Diagnose", "3")]
TRANSFER_START_SETTLE_SECONDS = 1.0


async def navigate_to_transfer_test(client: BleakClient, char, nav_buf: bytearray,
                                     nav_event: asyncio.Event, timeout: float = 5.0) -> bool:
    """Navigiert autark Hauptmenue -> 3 (Testfunktionen) -> 8 (BT-Diagnose) -> 3 (Transfer-Test),
    indem der ueber BLE-Notify gespiegelte Menuetext gelesen wird - kein manuelles Navigieren
    am Geraet mehr noetig. nav_buf/nav_event werden vom (bereits abonnierten) on_notify-Handler
    gefuellt, der auch fuer die spaetere ACK-Auswertung zustaendig ist (ein einziges Subscribe
    fuer die ganze Sitzung - kein stop_notify()/re-subscribe, das hat sich als Disconnect-Ausloeser
    erwiesen, siehe run_smoke_test)."""
    async def wait_for(marker: str) -> bool:
        deadline = time.monotonic() + timeout
        needle = marker.encode("ascii")
        while True:
            if needle in nav_buf:
                return True
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return False
            nav_event.clear()
            try:
                await asyncio.wait_for(nav_event.wait(), timeout=remaining)
            except asyncio.TimeoutError:
                return needle in nav_buf

    for marker, digit in MENU_NAV_STEPS:
        if marker is not None and not await wait_for(marker):
            log(f"Navigation: Marker '{marker}' nicht gesehen (Timeout {timeout:.0f}s) - Abbruch.")
            log(f"Navigation: tatsaechlich empfangen: {bytes(nav_buf)!r}")
            return False
        del nav_buf[:]
        if marker is None:
            log(f"Navigation: sende '{digit}' (Reset ins Hauptmenue).")
        else:
            log(f"Navigation: '{marker}' gesehen -> sende '{digit}'.")
        await client.write_gatt_char(char, digit.encode("ascii"), response=False)
        await asyncio.sleep(1.0)   # Abstand zum naechsten Write (siehe send_pin())

    # bt_transfer_echo() schreibt seine Startmeldung bewusst ohne UART1-Ausgabe, damit
    # sie nicht mit den folgenden binaeren CYS1/ACK1-Frames kollidiert. Daher kann die
    # BLE-Seite die Meldung "Transfer-Test" nicht als bestaetigenden Notify empfangen.
    await asyncio.sleep(TRANSFER_START_SETTLE_SECONDS)
    log("Navigation abgeschlossen: Transfer-Test auf dem Geraet aktiv.")
    return True


def write_report(kind: str, data: dict) -> tuple[Path, Path]:
    REPORT_DIR.mkdir(exist_ok=True)
    stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    base = REPORT_DIR / f"mainboard-{kind}-{stamp}"
    json_path = base.with_suffix(".json")
    json_path.write_text(__import__("json").dumps(data, indent=2, default=str), encoding="utf-8")
    return json_path, base.with_suffix(".md")


# ---------------------------------------------------------------------------
# Modus 1: Smoke-Test mit Debug-Timings (BLE)
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

        pin = args.pin or derive_pin_from_name(device_name or "")
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
# Modus 2: Transfer-Test (CRC-32/Sequenz/ACK, analog zum 40-kB-DVK-Referenztest) (BLE)
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
    nav_buffer = bytearray()
    nav_event = asyncio.Event()

    def on_disconnect(_client):
        log("*** DISCONNECT ***")

    def on_notify(_sender, data: bytearray):
        nav_buffer.extend(bytes(data))   # fuer navigate_to_transfer_test() - Marker-Suche
        nav_event.set()
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
        await asyncio.sleep(0.8)   # Windows-BLE-Stack Zeit geben, bis Notify-Zustellung wirklich steht

        pin = args.pin or derive_pin_from_name(device_name or "")
        if pin:
            await send_pin(client, char, pin)
        else:
            log("Kein --pin angegeben/ableitbar - PIN-Abfrage der Firmware bleibt offen "
                "(RS232-Menue kann dann u.U. nicht bis Testmenue 8->3 navigiert werden).")

        del nav_buffer[:]
        if not await navigate_to_transfer_test(client, char, nav_buffer, nav_event):
            log("Automatische Navigation fehlgeschlagen - bitte Testmenue -> 8 -> 3 manuell "
                "pruefen/starten.")
            result["result"] = "navigation_failed"
            return 2

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
# Transport-Abstraktion fuer die neuen, transportunabhaengigen Modi (cycle/download/
# classic-smoke): ein gemeinsamer Bytestrom + Write, egal ob BLE-Notify/Write oder
# COM-Port. Die bereits bewaehrten BLE-Modi (smoke/transfer) oben bleiben UNVERAENDERT -
# dieser Teil dupliziert absichtlich ein paar Zeilen, statt die laufenden Modi anzufassen.
# ---------------------------------------------------------------------------

class ByteStream:
    """Eingehender Bytestrom mit Marker-/Blockweise-Wartefunktionen, gefuellt von einem
    Hintergrund-Callback (BLE-Notify-Handler oder Seriell-Lesethread)."""

    def __init__(self):
        self.buf = bytearray()
        self.event = asyncio.Event()

    def feed(self, data: bytes) -> None:
        self.buf.extend(data)
        self.event.set()

    def clear(self) -> None:
        self.buf.clear()

    async def wait_for_marker(self, marker: str, timeout: float) -> bool:
        needle = marker.encode("ascii")
        deadline = time.monotonic() + timeout
        while needle not in self.buf:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return False
            self.event.clear()
            try:
                await asyncio.wait_for(self.event.wait(), timeout=remaining)
            except asyncio.TimeoutError:
                pass
        return True

    async def read_exact(self, n: int, timeout: float) -> bytes | None:
        deadline = time.monotonic() + timeout
        while len(self.buf) < n:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            self.event.clear()
            try:
                await asyncio.wait_for(self.event.wait(), timeout=remaining)
            except asyncio.TimeoutError:
                pass
        data = bytes(self.buf[:n])
        del self.buf[:n]
        return data

    async def read_byte(self, timeout: float) -> int | None:
        data = await self.read_exact(1, timeout)
        return data[0] if data else None


class BleTransport:
    """CYSPP-Datenkanal per BLE (Bleak) als generischer Bytestrom - fuer cycle/download."""

    def __init__(self, args):
        self.args = args
        self.rx = ByteStream()
        self.client: BleakClient | None = None
        self.char = None
        self.device_name = None

    async def connect(self, timeout: float) -> None:
        if self.args.address:
            address = self.args.address
        else:
            device, _ = await scan_for_device(self.args.name)
            if device is None:
                raise ConnectionError("Geraet nicht gefunden (Scan).")
            address, self.device_name = device.address, device.name
            self.args.address = address  # fuer Folgezyklen (cycle) wiederverwenden, kein Re-Scan
        self.client = BleakClient(address)
        await self.client.connect(timeout=timeout)
        self.char = find_char_by_suffix(self.client, CYSPP_DATA_SUFFIX)
        if self.char is None:
            raise ConnectionError(f"Keine Characteristic mit Suffix ...{CYSPP_DATA_SUFFIX} gefunden.")
        self.rx.clear()

        def on_notify(_sender, data: bytearray):
            self.rx.feed(bytes(data))

        await self.client.start_notify(self.char, on_notify)
        await asyncio.sleep(0.8)   # Windows-BLE-Stack Zeit geben, bis Notify-Zustellung wirklich steht

    async def disconnect(self) -> None:
        if self.client and self.client.is_connected:
            await self.client.disconnect()

    @property
    def is_connected(self) -> bool:
        return bool(self.client and self.client.is_connected)

    async def write(self, data: bytes) -> None:
        await self.client.write_gatt_char(self.char, data, response=False)

    def pin(self) -> str | None:
        return self.args.pin or derive_pin_from_name(self.device_name or "")


class ClassicTransport:
    """BT-Classic SPP ueber einen Windows-COM-Port (pyserial) als generischer Bytestrom -
    fuer classic-smoke/cycle/download. Ein Hintergrundthread liest blockierend vom Port und
    speist die Bytes threadsicher (loop.call_soon_threadsafe) in denselben ByteStream ein,
    den auch BleTransport verwendet - Navigation/Ymodem-Code ist dadurch transportneutral."""

    def __init__(self, args):
        if serial is None:
            sys.exit("Fehlt: pyserial. Installieren mit:  pip install pyserial")
        self.args = args
        self.rx = ByteStream()
        self.ser: "serial.Serial | None" = None
        self._reader_thread = None
        self._stop = None

    async def connect(self, timeout: float) -> None:
        import threading
        self.rx.clear()
        self.ser = serial.Serial(self.args.port, self.args.baud, timeout=0.2)
        self._stop = threading.Event()
        loop = asyncio.get_running_loop()

        def reader():
            while not self._stop.is_set():
                try:
                    data = self.ser.read(4096)
                except (OSError, serial.SerialException):
                    break
                if data:
                    loop.call_soon_threadsafe(self.rx.feed, data)

        self._reader_thread = threading.Thread(target=reader, daemon=True)
        self._reader_thread.start()

    async def disconnect(self) -> None:
        if self._stop:
            self._stop.set()
        if self._reader_thread:
            await asyncio.to_thread(self._reader_thread.join, 2.0)
        if self.ser and self.ser.is_open:
            await asyncio.to_thread(self.ser.close)

    @property
    def is_connected(self) -> bool:
        return bool(self.ser and self.ser.is_open)

    async def write(self, data: bytes) -> None:
        await asyncio.to_thread(self.ser.write, data)

    def pin(self) -> str | None:
        return self.args.pin


async def make_transport(args):
    if args.transport == "ble":
        return BleTransport(args)
    return ClassicTransport(args)


# Menuetext-Marker fuer die transportneutrale Navigation - alle aus dem Firmware-Quellcode
# entnommen (sictxt.h #defines bzw. Sprachtabellen), transportunabhaengig gueltig.
MARK_HAUPTMENU = "Hauptmen"
MARK_TESTMENU = "Testmen"
MARK_WERKMENUE = "Change configuration"       # T_cconf, werkmenue()'s Menuepunkt 1
MARK_COMPMENU = "Write flash data"            # T_comp, compmenu()'s Menuepunkt 1
MARK_SELECT_DATA = "Select Data"              # T_flash_w_d (write_flash())
MARK_ANZMW = "Messwerte"                      # T_anzmw ("Anzahl Messwerte")
MARK_DAUS = "Daten ausgeben"                  # T_daus (messdaten() Abfrage)


async def send_and_settle(transport, data: bytes, settle: float = 1.0) -> None:
    await transport.write(data)
    await asyncio.sleep(settle)


async def navigate(transport, steps: list[tuple[str | None, bytes]], timeout: float) -> bool:
    """Generische Menuenavigation: (marker, bytes_to_send) Paare, marker=None ueberspringt das
    Warten (fester Settle-Delay stattdessen). Bricht mit False ab, wenn ein Marker fehlt."""
    for marker, payload in steps:
        if marker is not None:
            if not await transport.rx.wait_for_marker(marker, timeout):
                log(f"Navigation: Marker '{marker}' nicht gesehen (Timeout {timeout:.0f}s) - Abbruch.")
                log(f"Navigation: zuletzt empfangen: {bytes(transport.rx.buf)!r}")
                return False
            transport.rx.clear()
        await send_and_settle(transport, payload)
    return True


# ---------------------------------------------------------------------------
# Modus 3: Cycle-Test (wiederholtes Verbinden/Trennen, mit optionaler Haltezeit) -
# BLE und Classic
# ---------------------------------------------------------------------------

async def run_cycle_test(args) -> int:
    result = {
        "mode": "cycle",
        "transport": args.transport,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "requested_cycles": args.count,
        "hold_seconds": args.hold,
        "cycles": [],
    }

    for i in range(1, args.count + 1):
        log(f"--- Zyklus {i}/{args.count} ---")
        cycle = {"index": i, "connect_ms": None, "pin_menu_ok": False,
                  "disconnect_ms": None, "status": "failed", "error": None}
        transport = await make_transport(args)
        t0 = time.perf_counter()
        try:
            await transport.connect(args.connect_timeout)
            cycle["connect_ms"] = round((time.perf_counter() - t0) * 1000.0, 1)
            log(f"Verbunden nach {cycle['connect_ms']:.0f} ms.")

            pin = transport.pin()
            if pin:
                await send_and_settle(transport, pin.encode("ascii"), settle=1.5)
            transport.rx.clear()
            await transport.write(b"H")
            if await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
                cycle["pin_menu_ok"] = True
                log("Hauptmenue erreichbar (PIN akzeptiert).")
            else:
                log("Kein Hauptmenue-Echo gesehen (PIN falsch/fehlt oder Verbindung instabil).")

            if args.hold > 0:
                log(f"Halte Verbindung {args.hold:.0f} s ...")
                await asyncio.sleep(args.hold)

            cycle["status"] = "passed" if cycle["pin_menu_ok"] or not pin else "unstable"
        except Exception as exc:
            cycle["error"] = str(exc)
            log(f"FEHLER: {exc}")
        finally:
            t_disc = time.perf_counter()
            try:
                await asyncio.wait_for(transport.disconnect(), timeout=15.0)
            except Exception as exc:
                log(f"WARNUNG: Trennen hing/schlug fehl ({exc}) - naechster Zyklus versucht es trotzdem.")
            cycle["disconnect_ms"] = round((time.perf_counter() - t_disc) * 1000.0, 1)

        result["cycles"].append(cycle)
        if i < args.count:
            await asyncio.sleep(args.cooldown)

    _finish_cycle(result)
    passed = sum(1 for c in result["cycles"] if c["status"] == "passed")
    return 0 if passed == args.count else 1


def _finish_cycle(result: dict) -> None:
    cycles = result["cycles"]
    passed = sum(1 for c in cycles if c["status"] == "passed")
    connect_times = [c["connect_ms"] for c in cycles if c["connect_ms"] is not None]
    print()
    print("=" * 70)
    print(f"  CYCLE-TEST ZUSAMMENFASSUNG ({result['transport']}, hold={result['hold_seconds']}s)")
    print("=" * 70)
    print(f"  Zyklen bestanden:  {passed}/{len(cycles)}")
    if connect_times:
        print(f"  Connect-Zeit min/avg/max (ms): "
              f"{min(connect_times):.0f} / {sum(connect_times)/len(connect_times):.0f} / {max(connect_times):.0f}")
    for c in cycles:
        if c["status"] != "passed":
            print(f"    Zyklus {c['index']}: {c['status']}  error={c['error']}")
    print()
    json_path, md_path = write_report("cycle", result)
    lines = [
        "# Cycle-Test (Connect/Disconnect)",
        "",
        f"- Transport: `{result['transport']}`",
        f"- Bestanden: **{passed}/{len(cycles)}**",
        f"- Haltezeit je Zyklus: `{result['hold_seconds']}` s",
        "",
        "| Zyklus | Connect (ms) | PIN/Menue ok | Disconnect (ms) | Status |",
        "| ---: | ---: | :--: | ---: | --- |",
    ]
    for c in cycles:
        lines.append(f"| {c['index']} | {c['connect_ms']} | {c['pin_menu_ok']} | "
                      f"{c['disconnect_ms']} | {c['status']} |")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    log(f"JSON-Report: {json_path}")
    log(f"Markdown-Report: {md_path}")


# ---------------------------------------------------------------------------
# Modus 4: Classic-Smoke-Test (BT-Classic-Aequivalent zu --mode smoke)
# ---------------------------------------------------------------------------

async def run_classic_smoke_test(args) -> int:
    result = {"mode": "classic-smoke", "port": args.port,
              "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
              "connect_ms": None, "banner_seen": False, "pin_menu_ok": False, "result": "incomplete"}
    transport = ClassicTransport(args)
    t0 = time.perf_counter()
    try:
        await transport.connect(args.connect_timeout)
        result["connect_ms"] = round((time.perf_counter() - t0) * 1000.0, 1)
        log(f"Port {args.port} offen nach {result['connect_ms']:.0f} ms.")

        log("Warte auf Banner/PIN-Prompt ...")
        await asyncio.sleep(3.0)   # Boot-Banner + Pin-Abfrage brauchen etwas Zeit
        if transport.rx.buf:
            result["banner_seen"] = True
            log(f"Empfangen: {bytes(transport.rx.buf)!r}")
        transport.rx.clear()

        if args.pin:
            await send_and_settle(transport, args.pin.encode("ascii"), settle=1.5)
        transport.rx.clear()
        await transport.write(b"H")
        if await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
            result["pin_menu_ok"] = True
            log("Hauptmenue erreichbar.")
        else:
            log(f"Kein Hauptmenue-Echo gesehen: {bytes(transport.rx.buf)!r}")

        result["result"] = "passed" if result["banner_seen"] and (result["pin_menu_ok"] or not args.pin) else "unstable"
    except Exception as exc:
        result["result"] = "failed"
        result["error"] = str(exc)
        log(f"FEHLER: {exc}")
    finally:
        await transport.disconnect()

    print()
    print("=" * 70)
    print("  CLASSIC-SMOKE-TEST ZUSAMMENFASSUNG")
    print("=" * 70)
    print(f"  Ergebnis: {result['result']}")
    print()
    json_path, md_path = write_report("classic-smoke", result)
    md_path.write_text(
        f"# BT-Classic Smoke-Test\n\n- Ergebnis: **{result['result']}**\n"
        f"- Port: `{args.port}`\n- Connect: `{result['connect_ms']}` ms\n"
        f"- Banner gesehen: `{result['banner_seen']}`\n- PIN/Menue ok: `{result['pin_menu_ok']}`\n",
        encoding="utf-8")
    log(f"JSON-Report: {json_path}")
    log(f"Markdown-Report: {md_path}")
    return 0 if result["result"] == "passed" else 1


# ---------------------------------------------------------------------------
# Modus 5: Download-Test (Testdaten erzeugen + echten Ymodem-Export empfangen) -
# BLE und Classic
# ---------------------------------------------------------------------------

XMODEM_SOH, XMODEM_STX, XMODEM_EOT = 0x01, 0x02, 0x04
XMODEM_ACK, XMODEM_NAK, XMODEM_CAN = 0x06, 0x15, 0x18


def crc16_xmodem(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (Poly 0x1021, Init 0x0000) - identisch zu crcgen()/crc_table in
    sism555/libtool.c (dort per Lookup-Table, hier bitweise - gleiches Ergebnis)."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc <<= 1
            if crc & 0x10000:
                crc ^= 0x1021
            crc &= 0xFFFF
    return crc


async def _read_ymodem_block(rx: ByteStream, timeout: float):
    """Liest einen einzelnen Xmodem/Ymodem-Block aus rx. Rueckgabe: ('EOT', None, None),
    ('DATA', blocknr, payload) oder wirft eine Exception (CAN/Timeout/Fehler)."""
    first = await rx.read_byte(timeout)
    if first is None:
        raise TimeoutError("Timeout beim Warten auf Blockstart/EOT.")
    if first == XMODEM_EOT:
        return "EOT", None, None
    if first == XMODEM_CAN:
        raise ConnectionAbortedError("Geraet hat mit CAN abgebrochen.")
    if first not in (XMODEM_SOH, XMODEM_STX):
        raise ValueError(f"Unerwartetes Blockstart-Byte 0x{first:02X}.")
    blen = 128 if first == XMODEM_SOH else 1024
    header = await rx.read_exact(2, timeout)
    if header is None:
        raise TimeoutError("Timeout bei Blocknummer.")
    blkno, blkno_inv = header
    if blkno != (0xFF - blkno_inv) & 0xFF:
        raise ValueError("Blocknummer/Komplement stimmen nicht ueberein.")
    payload = await rx.read_exact(blen + 2, timeout)
    if payload is None:
        raise TimeoutError("Timeout beim Blockinhalt.")
    data, crc_bytes = payload[:blen], payload[blen:]
    if crc16_xmodem(data) != ((crc_bytes[0] << 8) | crc_bytes[1]):
        raise ValueError("CRC-Fehler im Datenblock.")
    return "DATA", blkno, data


async def ymodem_receive(transport, block_timeout: float, retry_limit: int = 5) -> dict:
    """Empfaengt einen Ymodem-Batch-Transfer, exakt wie ihn sendblock()/send_vtf_file()/
    ymodem_close() in sism555/xmodem.c + sicom.c erzeugen: 128-Byte-SOH-Headerblock (Dateiname
    + Groesse), dann 1024-Byte-STX-CRC-Datenbloecke, EOT, abschliessender Null-Headerblock."""
    stats = {"blocks": 0, "bytes": 0, "filename": None, "filesize": None, "retries": 0}
    t_start = time.perf_counter()

    async def read_with_retry():
        last_exc = None
        for attempt in range(retry_limit):
            try:
                return await _read_ymodem_block(transport.rx, block_timeout)
            except Exception as exc:
                last_exc = exc
                stats["retries"] += 1
                log(f"Ymodem-Blockfehler ({exc}) - sende NAK, Versuch {attempt+1}/{retry_limit}.")
                await transport.write(bytes([XMODEM_NAK]))
        raise last_exc

    kind, blkno, data = await read_with_retry()
    if kind != "DATA" or blkno != 0:
        raise ValueError(f"Erwartete Ymodem-Headerblock 0, bekam {kind}/{blkno}.")
    name_end = data.find(b"\x00")
    header_str = data[:name_end].decode("ascii", "ignore") if name_end > 0 else ""
    parts = header_str.split(" ") if header_str else []
    stats["filename"] = parts[0] if parts else None
    stats["filesize"] = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
    log(f"Ymodem-Header: Datei='{stats['filename']}' Groesse={stats['filesize']} Byte.")
    await transport.write(bytes([XMODEM_ACK]))
    await transport.write(b"C")   # fordert ersten Datenblock (CRC-Modus) an - sendblock() mit get_C=1

    expected = 1
    while True:
        kind, blkno, data = await read_with_retry()
        if kind == "EOT":
            await transport.write(bytes([XMODEM_ACK]))
            break
        if blkno == (expected & 0xFF):
            stats["blocks"] += 1
            stats["bytes"] += len(data)
            expected += 1
            await transport.write(bytes([XMODEM_ACK]))
            if stats["blocks"] % 20 == 0:
                elapsed = time.perf_counter() - t_start
                log(f"Ymodem: {stats['blocks']} Bloecke / {stats['bytes']} Byte "
                    f"({stats['bytes']/elapsed:.0f} B/s) ...")
        elif blkno == ((expected - 1) & 0xFF):
            await transport.write(bytes([XMODEM_ACK]))   # Duplikat/erneut gesendeter Block
        else:
            raise ValueError(f"Ausser Sync: erwartet {expected & 0xFF}, erhalten {blkno}.")

    await transport.write(b"C")   # keine weitere Batch-Datei anfordern -> Geraet sendet Nullblock
    kind, blkno, data = await read_with_retry()
    if kind == "DATA" and blkno == 0:
        await transport.write(bytes([XMODEM_ACK]))

    stats["duration_s"] = round(time.perf_counter() - t_start, 3)
    stats["throughput_bps"] = round(stats["bytes"] / stats["duration_s"], 1) if stats["duration_s"] else 0
    return stats


async def run_download_test(args) -> int:
    result = {
        "mode": "download",
        "transport": args.transport,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "requested_records": args.records,
        "ymodem": None,
        "result": "incomplete",
    }
    transport = await make_transport(args)
    try:
        await transport.connect(args.connect_timeout)
        log("Verbunden.")
        pin = transport.pin()
        if pin:
            await send_and_settle(transport, pin.encode("ascii"), settle=1.5)
        elif args.transport == "ble":
            log("Kein --pin angegeben/ableitbar - Navigation kann daran scheitern.")

        log(f"Erzeuge {args.records} Test-Messwerte im Flash "
            "(Testmenue -> Werksmenue -> Bauteiltests -> Write flash data) ...")
        steps_generate = [
            (None, b"H"),
            (MARK_HAUPTMENU, b"3"),                       # -> Testmenue
            (MARK_TESTMENU, bytes([0x17])),                # Ctrl-W (STRW) -> werkmenue()
            (MARK_WERKMENUE, b"6"),                        # -> compmenu()
            (MARK_COMPMENU, b"1"),                         # -> write_flash()
            (MARK_SELECT_DATA, b"1"),                      # 1 = Messdaten (nicht Protokoll)
            (MARK_ANZMW, f"{args.records}\r".encode("ascii")),
        ]
        if not await navigate(transport, steps_generate, args.menu_timeout):
            result["result"] = "generate_failed"
            _finish_download(result)
            return 2

        # write_flash() kehrt nach md_simulation() zu compmenu() zurueck, das sein Menue
        # (T_comp, Marker "Write flash data") erneut ausgibt - das ist unser fertig-Signal,
        # unabhaengig davon wie lange md_simulation() fuer --records Saetze braucht.
        generate_timeout = max(args.menu_timeout, 5.0 + args.records / 200.0)
        transport.rx.clear()
        if not await transport.rx.wait_for_marker(MARK_COMPMENU, generate_timeout):
            log("Kein Fertig-Signal von write_flash() gesehen (Timeout) - Abbruch.")
            result["result"] = "generate_timeout"
            _finish_download(result)
            return 2
        log(f"{args.records} Test-Messwerte erzeugt.")

        # Zurueck ins Hauptmenue (2x '8' verlaesst compmenu()/werkmenue(), 'H' geht sicher):
        transport.rx.clear()
        await send_and_settle(transport, b"8")
        await send_and_settle(transport, b"8")
        await send_and_settle(transport, b"H")
        if not await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
            log("Konnte Rueckkehr ins Hauptmenue nicht bestaetigen - bitte Geraet pruefen.")
            result["result"] = "return_to_main_failed"
            _finish_download(result)
            return 2

        log("Starte Datenexport (Hauptmenue -> 1 -> Ymodem) ...")
        steps_download = [
            (None, None),  # wir sind schon im Hauptmenue, kein erneutes 'H' noetig
        ]
        transport.rx.clear()
        await send_and_settle(transport, b"1")
        if not await transport.rx.wait_for_marker(MARK_DAUS, args.menu_timeout):
            log(f"Kein 'Daten ausgeben?'-Prompt gesehen: {bytes(transport.rx.buf)!r}")
            result["result"] = "export_prompt_failed"
            _finish_download(result)
            return 2
        transport.rx.clear()
        await transport.write(b"C")   # 'C' = Ymodem-Ausgabe waehlen (send_vtf_file(), sicom.c)
        await asyncio.sleep(0.3)

        log("Empfange Ymodem-Transfer ...")
        stats = await ymodem_receive(transport, block_timeout=args.ymodem_timeout)
        result["ymodem"] = stats
        result["result"] = "passed"
        log(f"Download abgeschlossen: {stats['bytes']} Byte in {stats['blocks']} Bloecken, "
            f"{stats['duration_s']} s, {stats['throughput_bps']:.0f} B/s "
            f"({stats['retries']} Retries).")
    except Exception as exc:
        result["result"] = "failed"
        result["error"] = str(exc)
        log(f"FEHLER: {exc}")
    finally:
        await transport.disconnect()

    _finish_download(result)
    return 0 if result["result"] == "passed" else 1


def _finish_download(result: dict) -> None:
    print()
    print("=" * 70)
    print("  DOWNLOAD-TEST ZUSAMMENFASSUNG")
    print("=" * 70)
    print(f"  Ergebnis: {result['result']}")
    ym = result.get("ymodem")
    if ym:
        print(f"  Datei:       {ym['filename']}  ({ym['filesize']} Byte laut Header)")
        print(f"  Empfangen:   {ym['bytes']} Byte in {ym['blocks']} Bloecken")
        print(f"  Dauer:       {ym['duration_s']} s")
        print(f"  Durchsatz:   {ym['throughput_bps']:.0f} B/s")
        print(f"  Retries:     {ym['retries']}")
    print()
    json_path, md_path = write_report("download", result)
    lines = [
        "# Download-Test (echter Ymodem-Export)",
        "",
        f"- Transport: `{result['transport']}`",
        f"- Ergebnis: **{result['result']}**",
        f"- Angeforderte Test-Messwerte: `{result['requested_records']}`",
    ]
    if ym:
        lines += [
            f"- Ymodem-Datei: `{ym['filename']}` ({ym['filesize']} Byte laut Header)",
            f"- Empfangen: `{ym['bytes']}` Byte in `{ym['blocks']}` Bloecken",
            f"- Dauer: `{ym['duration_s']}` s",
            f"- Durchsatz: `{ym['throughput_bps']}` B/s",
            f"- Retries: `{ym['retries']}`",
        ]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    log(f"JSON-Report: {json_path}")
    log(f"Markdown-Report: {md_path}")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    p = argparse.ArgumentParser(description="BLE/BT-Classic-Test fuer das VIASIS-Mainboard.")
    p.add_argument("--mode", choices=("smoke", "transfer", "classic-smoke", "cycle", "download"),
                   default="smoke")
    p.add_argument("--transport", choices=("ble", "classic"), default="ble",
                   help="Uebertragungsweg fuer --mode cycle/download (classic-smoke/smoke/"
                        "transfer legen den Transport implizit fest).")
    p.add_argument("--name", default=DEFAULT_NAME_PREFIX,
                   help=f"[BLE] Namensprefix beim Scannen (Default: {DEFAULT_NAME_PREFIX})")
    p.add_argument("--address", default=None, help="[BLE] BT-Adresse direkt angeben, ueberspringt Scan")
    p.add_argument("--port", default=None, help="[Classic] Windows-COM-Port, z. B. COM5")
    p.add_argument("--baud", type=int, default=115200,
                   help="[Classic] Baudrate fuer den COM-Port (virtueller SPP-Port - Wert meist "
                        "ohne Wirkung, aber von pyserial verlangt)")
    p.add_argument("--pin", default=None,
                   help="BT-PIN (6 Ziffern) nach dem Verbinden senden - ohne das bleibt die "
                        "Firmware-Pinabfrage offen. [BLE] ohne Angabe wird versucht, sie aus dem "
                        "Geraetenamen abzuleiten (VIASIS_<serno> -> erste 2 + letzte 4 Ziffern).")
    p.add_argument("--connect-timeout", type=float, default=30.0)
    p.add_argument("--menu-timeout", type=float, default=8.0,
                   help="Sekunden, die auf einen erwarteten Menuetext-Marker gewartet wird")
    p.add_argument("--hold", type=float, default=8.0,
                   help="[smoke] Sekunden verbunden bleiben, um Spontan-Disconnect zu pruefen. "
                        "[cycle] Haltezeit je Zyklus (0 = sofort wieder trennen).")
    p.add_argument("--no-report", action="store_true", help="[smoke] keinen Report schreiben")
    p.add_argument("--bytes", type=int, default=DEFAULT_TRANSFER_BYTES,
                   help="[transfer] Nutzdaten-Bytes (Default 40 kB, wie DVK-Referenztest)")
    p.add_argument("--window", type=int, default=DEFAULT_WINDOW,
                   help="[transfer] max. unbestaetigte Frames (0 = unpaced Burst)")
    p.add_argument("--retries", type=int, default=DEFAULT_RETRIES)
    p.add_argument("--rate", type=int, default=DEFAULT_WIRE_RATE_BPS,
                   help="[transfer] Ziel-Bitrate in B/s, 0 = unpaced Burst")
    p.add_argument("--count", type=int, default=5, help="[cycle] Anzahl Verbinden/Trennen-Zyklen")
    p.add_argument("--cooldown", type=float, default=2.0,
                   help="[cycle] Pause zwischen zwei Zyklen in Sekunden")
    p.add_argument("--records", type=int, default=500,
                   help="[download] Anzahl Test-Messwerte, die vor dem Download erzeugt werden")
    p.add_argument("--ymodem-timeout", type=float, default=10.0,
                   help="[download] Timeout je Ymodem-Block in Sekunden")
    args = p.parse_args()

    if args.mode in ("smoke", "transfer"):
        args.transport = "ble"
    elif args.mode == "classic-smoke":
        args.transport = "classic"
    if args.transport == "classic" and not args.port:
        sys.exit("--transport classic braucht --port (z. B. --port COM5).")
    if args.transport == "ble" and BleakClient is None:
        sys.exit("Fehlt: bleak. Installieren mit:  pip install bleak")

    print("=" * 70)
    print("  BLE/BT-Classic-Mainboard-Test - VIASIS / sism555 (IF820)")
    print(f"  Modus: {args.mode}   Transport: {args.transport}   "
          f"Start: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)

    if args.mode == "smoke":
        return asyncio.run(run_smoke_test(args))
    if args.mode == "transfer":
        return asyncio.run(run_transfer_test(args))
    if args.mode == "classic-smoke":
        return asyncio.run(run_classic_smoke_test(args))
    if args.mode == "cycle":
        return asyncio.run(run_cycle_test(args))
    return asyncio.run(run_download_test(args))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nAbgebrochen.")
        raise SystemExit(130)
