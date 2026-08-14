#!/usr/bin/env python3
"""
ble_cyspp_probe.py — BLE-CYSPP-Diagnose fuer Vela IF820 / sism555

Zweck
-----
Reproduziert das App-Symptom "Subscribe auf ...ca102 -> sofortiger Disconnect"
vom PC aus, mit Millisekunden-Zeitstempeln, damit sich das Log neben eine
Oszilloskop-/Logic-Analyzer-Aufzeichnung der CYSPP-Leitung legen laesst.

Gehoert zu: sism555/Doku/Messprotokoll_CYSPP_Diagnose.md (Schritt 4 und 5)

Ablauf pro Durchlauf
--------------------
  1. Scannen, Geraet per Namensprefix finden
  2. Verbinden
  3. GATT-Tabelle einlesen (beim ersten Durchlauf vollstaendig ausgeben)
  4. start_notify() auf ...ca102  <-- das ist der CCCD-Write, der den Abbruch ausloest
  5. Lauschen: kommt ein unaufgeforderter PIN-Prompt? Oder kommt der Disconnect?
  6. Zeit zwischen Subscribe und Disconnect protokollieren

Erfolgskriterium (aus ble-retest-1-negative.md, Abschnitt 5):
    subscribe ...ca102 ok  -> und DANACH KEIN Disconnect
    <= [..] ascii="... PIN ..."   (unaufgefordert = RX/Datenmodus steht)

Aufruf
------
  python ble_cyspp_probe.py
  python ble_cyspp_probe.py --runs 3
  python ble_cyspp_probe.py --name VIASIS_26SC9996 --runs 3 --csv ergebnis.csvy
  python ble_cyspp_probe.py --address AA:BB:CC:DD:EE:FF
  python ble_cyspp_probe.py --scan-only

Voraussetzung
-------------
  pip install bleak
"""

import argparse
import asyncio
import csv
import sys
import time
from datetime import datetime

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    sys.exit("Fehlt: bleak. Installieren mit:  pip install bleak")

# ---------------------------------------------------------------------------
# Konfiguration
# ---------------------------------------------------------------------------

# CYSPP-Characteristics werden ueber den UUID-Suffix gefunden, nicht ueber die
# vollstaendige UUID. Das ist robust gegen abweichende Basis-UUIDs.
SUFFIX_ACK_DATA = "ca101"     # Acknowledged Data
SUFFIX_DATA = "ca102"         # Unacknowledged Data  <- hier subscriben
SUFFIX_RX_FLOW = "ca103"      # RX Flow Control

DEFAULT_NAME_PREFIX = "VIASIS"
SCAN_TIMEOUT = 12.0           # s
LISTEN_TIME = 8.0             # s nach dem Subscribe auf Daten warten
PAUSE_BETWEEN_RUNS = 6.0      # s, damit das Modul in den Ruhezustand zurueckfaellt

T0 = time.perf_counter()


def ts() -> str:
    """Zeitstempel: Wanduhr + Millisekunden seit Programmstart."""
    ms = (time.perf_counter() - T0) * 1000.0
    return f"{datetime.now().strftime('%H:%M:%S.%f')[:-3]}  [{ms:9.1f} ms]"


def log(msg: str) -> None:
    print(f"{ts()}  {msg}", flush=True)


def pretty(data: bytes) -> str:
    hexpart = " ".join(f"{b:02X}" for b in data)
    ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in data)
    return f'len={len(data):3d}  hex=[{hexpart}]  ascii="{ascii_part}"'


def find_by_suffix(client, suffix: str):
    """Characteristic anhand des UUID-Suffix suchen."""
    services = getattr(client, "services", None)
    if services is None:                                  # aeltere bleak-Versionen
        services = client.get_services()
    for service in services:
        for char in service.characteristics:
            if str(char.uuid).lower().replace("-", "").endswith(suffix.lower()):
                return char
    return None


def dump_gatt(client) -> None:
    services = getattr(client, "services", None)
    if services is None:
        services = client.get_services()
    log("GATT-Tabelle:")
    for service in services:
        print(f"    Service  {service.uuid}")
        for char in service.characteristics:
            props = ",".join(char.properties)
            mark = ""
            u = str(char.uuid).lower().replace("-", "")
            if u.endswith(SUFFIX_DATA):
                mark = "   <== Subscribe-Ziel"
            elif u.endswith(SUFFIX_ACK_DATA):
                mark = "   (Acknowledged Data)"
            elif u.endswith(SUFFIX_RX_FLOW):
                mark = "   (RX Flow Control)"
            print(f"      Char   {char.uuid}  [{props}]{mark}")


# ---------------------------------------------------------------------------
# Scan
# ---------------------------------------------------------------------------

async def scan(name_prefix: str, show_all: bool = False):
    log(f"Scanne {SCAN_TIMEOUT:.0f} s nach Geraeten ...")
    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT)

    if show_all:
        log(f"{len(devices)} Geraet(e) gefunden:")
        for d in sorted(devices, key=lambda x: (x.name or "")):
            print(f"    {d.address}   {d.name or '(kein Name)'}")

    hits = [d for d in devices if d.name and d.name.upper().startswith(name_prefix.upper())]
    if not hits:
        log(f"KEIN Geraet mit Prefix '{name_prefix}' gefunden.")
        log("Hinweise: Advertising aktiv? BT am PC eingeschaltet? Adapter BT 4.0+?")
        return None
    if len(hits) > 1:
        log(f"{len(hits)} passende Geraete gefunden — bitte mit --address eingrenzen:")
        for d in hits:
            print(f"    {d.address}   {d.name}")
        return None

    log(f"Gefunden: {hits[0].name}   ({hits[0].address})")
    return hits[0]


# ---------------------------------------------------------------------------
# Ein Durchlauf
# ---------------------------------------------------------------------------

async def one_run(address: str, run_no: int, dump: bool, do_write: bool) -> dict:
    result = {
        "run": run_no,
        "connected": False,
        "subscribed": False,
        "rx_bytes": 0,
        "rx_first_ms": None,
        "disconnect_ms": None,
        "verdict": "",
    }

    disconnected = asyncio.Event()
    t_subscribe = None

    def on_disconnect(_client):
        if t_subscribe is not None:
            dt = (time.perf_counter() - t_subscribe) * 1000.0
            result["disconnect_ms"] = dt
            log(f"*** DISCONNECT — {dt:.1f} ms nach dem Subscribe ***")
        else:
            log("*** DISCONNECT (vor dem Subscribe) ***")
        disconnected.set()

    def on_notify(_sender, data: bytearray):
        b = bytes(data)
        result["rx_bytes"] += len(b)
        if result["rx_first_ms"] is None and t_subscribe is not None:
            result["rx_first_ms"] = (time.perf_counter() - t_subscribe) * 1000.0
        log(f"<== EMPFANGEN  {pretty(b)}")

    print()
    log(f"{'=' * 62}")
    log(f"DURCHLAUF {run_no}")
    log(f"{'=' * 62}")

    client = BleakClient(address, disconnected_callback=on_disconnect)

    try:
        log("Verbinde ...")
        await client.connect()
        result["connected"] = True
        log("Verbunden.")
    except Exception as exc:
        log(f"FEHLER beim Verbinden: {exc}")
        result["verdict"] = "connect-fehler"
        return result

    try:
        if dump:
            dump_gatt(client)

        char = find_by_suffix(client, SUFFIX_DATA)
        if char is None:
            log(f"FEHLER: keine Characteristic mit Suffix ...{SUFFIX_DATA} gefunden.")
            result["verdict"] = "char-fehlt"
            return result

        log(f"Ziel-Characteristic: {char.uuid}  [{','.join(char.properties)}]")

        # ---- Der entscheidende Moment: CCCD-Write --------------------------
        log(">>> SUBSCRIBE (CCCD-Write) — jetzt Oszi beobachten <<<")
        t_subscribe = time.perf_counter()
        await client.start_notify(char, on_notify)
        result["subscribed"] = True
        dt = (time.perf_counter() - t_subscribe) * 1000.0
        log(f"Subscribe mit ATT-Success quittiert nach {dt:.1f} ms.")

        # ---- Lauschen -----------------------------------------------------
        log(f"Lausche {LISTEN_TIME:.0f} s auf unaufgeforderte Daten (PIN-Prompt) ...")
        try:
            await asyncio.wait_for(disconnected.wait(), timeout=LISTEN_TIME)
        except asyncio.TimeoutError:
            pass

        if do_write and client.is_connected:
            for payload in (b"H", b"V"):
                try:
                    await client.write_gatt_char(char, payload, response=False)
                    log(f"==> GESENDET  {pretty(payload)}")
                    await asyncio.sleep(1.0)
                except Exception as exc:
                    log(f"==> Schreibfehler {payload!r}: {exc}")
                    break

    finally:
        if client.is_connected:
            try:
                await client.disconnect()
                log("Sauber getrennt (durch uns).")
            except Exception:
                pass

    # ---- Bewertung --------------------------------------------------------
    if not result["subscribed"]:
        result["verdict"] = "subscribe-fehlgeschlagen"
    elif result["rx_bytes"] > 0 and result["disconnect_ms"] is None:
        result["verdict"] = "ERFOLG — Datenkanal steht"
    elif result["rx_bytes"] > 0:
        result["verdict"] = "Daten empfangen, dann Abbruch"
    elif result["disconnect_ms"] is not None:
        d = result["disconnect_ms"]
        result["verdict"] = ("Abbruch SOFORT (Pin-Verdacht)" if d < 150
                             else f"Abbruch nach {d:.0f} ms (eher Timeout)")
    else:
        result["verdict"] = "Verbindung haelt, aber keine Daten"

    log(f"Ergebnis Durchlauf {run_no}: {result['verdict']}")
    return result


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

async def main_async(args) -> int:
    print("=" * 78)
    print("  BLE-CYSPP-Probe — Vela IF820 / sism555")
    print(f"  Start: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 78)

    address = args.address
    if address is None:
        dev = await scan(args.name, show_all=args.scan_only or args.verbose)
        if args.scan_only:
            return 0
        if dev is None:
            return 2
        address = dev.address
    else:
        log(f"Adresse direkt vorgegeben: {address}")

    results = []
    for i in range(1, args.runs + 1):
        res = await one_run(address, i, dump=(i == 1), do_write=args.probe_write)
        results.append(res)
        if i < args.runs:
            log(f"Pause {PAUSE_BETWEEN_RUNS:.0f} s, damit das Modul zurueckfaellt ...")
            await asyncio.sleep(PAUSE_BETWEEN_RUNS)

    # ---- Zusammenfassung -------------------------------------------------
    print()
    print("=" * 78)
    print("  ZUSAMMENFASSUNG")
    print("=" * 78)
    print(f"  {'#':>2}  {'verb.':>5}  {'subscr.':>7}  {'RX Bytes':>8}  "
          f"{'1. RX':>9}  {'Disconn.':>9}   Bewertung")
    print("  " + "-" * 74)
    for r in results:
        rx1 = f"{r['rx_first_ms']:.1f} ms" if r["rx_first_ms"] is not None else "-"
        dis = f"{r['disconnect_ms']:.1f} ms" if r["disconnect_ms"] is not None else "-"
        print(f"  {r['run']:>2}  {'ja' if r['connected'] else 'nein':>5}  "
              f"{'ja' if r['subscribed'] else 'nein':>7}  {r['rx_bytes']:>8}  "
              f"{rx1:>9}  {dis:>9}   {r['verdict']}")
    print()

    ok = sum(1 for r in results if r["verdict"].startswith("ERFOLG"))
    if ok == len(results):
        print("  ==> Datenkanal steht. Der Fix greift.")
    elif ok:
        print(f"  ==> Uneinheitlich: {ok} von {len(results)} erfolgreich —")
        print("      spricht fuer einen undefinierten Pegel oder Versorgungseinbruch.")
    else:
        fast = [r["disconnect_ms"] for r in results
                if r["disconnect_ms"] is not None and r["disconnect_ms"] < 150]
        print("  ==> Kein Durchlauf erfolgreich.")
        if len(fast) == len(results):
            print("      Abbruch immer < 150 ms nach dem Subscribe: passt zum")
            print("      CYSPP-Pin-Pegel. Oszi-Aufzeichnung dazulegen (CH2).")
        else:
            print("      Abbruchzeiten streuen: eher Supervision-Timeout oder RF.")
    print()

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=list(results[0].keys()))
            w.writeheader()
            w.writerows(results)
        log(f"CSV geschrieben: {args.csv}")

    return 0


def main() -> int:
    p = argparse.ArgumentParser(
        description="BLE-CYSPP-Diagnose fuer Vela IF820 (sism555).")
    p.add_argument("--name", default=DEFAULT_NAME_PREFIX,
                   help=f"Namensprefix beim Scannen (Default: {DEFAULT_NAME_PREFIX})")
    p.add_argument("--address", default=None,
                   help="BT-Adresse direkt angeben, ueberspringt den Scan")
    p.add_argument("--runs", type=int, default=3,
                   help="Anzahl Durchlaeufe (Default: 3)")
    p.add_argument("--probe-write", action="store_true",
                   help="Nach dem Subscribe 'H' und 'V' senden")
    p.add_argument("--scan-only", action="store_true",
                   help="Nur scannen und alle Geraete auflisten")
    p.add_argument("--csv", default=None, help="Ergebnisse als CSV speichern")
    p.add_argument("-v", "--verbose", action="store_true",
                   help="Beim Scan alle Geraete auflisten")
    args = p.parse_args()

    try:
        return asyncio.run(main_async(args))
    except KeyboardInterrupt:
        print("\nAbgebrochen.")
        return 130


if __name__ == "__main__":
    sys.exit(main())
