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
  --mode idle-latency  [Classic] Verbindet frisch, wartet je --idle-times-Wert GENAU diese
                    Leerlaufzeit im Hauptmenue (keine Aktion), misst dann die Reaktionszeit auf
                    eine minimale Navigation (Hauptmenue -> Testmenue -> zurueck). Prueft den
                    Verdacht "haengt/wird langsam nach ~1 Minute Leerlauf" (measure.c-Minutentick,
                    siehe project-status.md) reproduzierbar und ohne manuelles Stoppen.

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
  python ble_mainboard_test.py --transport classic --port COM5 --mode idle-latency
  python ble_mainboard_test.py --transport classic --port COM5 --mode idle-latency --idle-times 30,60,90,120,150

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
import ctypes
import html
import json
import re
import struct
import subprocess
import sys
import threading
import time
import zlib
from datetime import datetime
from pathlib import Path
from ctypes import wintypes

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
CLASSIC_INQUIRY_TIMEOUT_MULTIPLIER = 4  # Windows: je 1,28 s
CLASSIC_VISIBILITY_MAX_AGE_SECONDS = 15.0

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
TRACE = None
RUN_REPORT_DIR = None


def ts() -> str:
    ms = (time.perf_counter() - T0) * 1000.0
    return f"{datetime.now().strftime('%H:%M:%S.%f')[:-3]}  [{ms:9.1f} ms]"


def log(msg: str) -> None:
    print(f"{ts()}  {msg}", flush=True)


class TraceLogger:
    """Append-only NDJSON trace; a complete prefix remains readable after an abort."""

    def __init__(self, args):
        global RUN_REPORT_DIR
        self.run_id = datetime.now().strftime("%Y-%m-%d_%H-%M-%S_%f")
        self.run_timestamp = self.run_id[:19]
        self.test_kind = args.mode
        self.device_name = None
        self.lock = threading.RLock()  # RS232-Sniffer schreibt aus eigenem Thread
        RUN_REPORT_DIR = REPORT_DIR / f"{self.run_timestamp}_unbekannt_{self.test_kind}"
        if RUN_REPORT_DIR.exists():
            RUN_REPORT_DIR = REPORT_DIR / f"{RUN_REPORT_DIR.name}_{self.run_id[-6:]}"
        RUN_REPORT_DIR.mkdir(parents=True, exist_ok=True)
        self.path = RUN_REPORT_DIR / "trace.ndjson"
        self.file = self.path.open("a", encoding="utf-8")
        self.context = {"run_id": self.run_id, "suite": args.mode, "phase": args.mode,
                        "transport": args.transport, "cycle": None}
        self.event("run_started", command_line=sys.argv, python_version=sys.version,
                   platform=sys.platform, arguments=vars(args).copy())

    def set_context(self, **values) -> None:
        self.context.update(values)

    def set_device_name(self, device_name: str | None) -> None:
        global RUN_REPORT_DIR
        if not device_name or self.device_name is not None:
            return
        self.device_name = device_name
        self.context["device_name"] = device_name
        folder_device_name = re.sub(r'[<>:"/\\|?*]', "_", device_name).strip(". ") or "unbekannt"
        target_dir = REPORT_DIR / f"{self.run_timestamp}_{folder_device_name}_{self.test_kind}"
        if target_dir.exists():
            target_dir = REPORT_DIR / f"{target_dir.name}_{self.run_id[-6:]}"
        with self.lock:  # Sniffer-Thread darf waehrend Close/Rename nicht schreiben
            self.file.close()
            try:
                RUN_REPORT_DIR.rename(target_dir)
            except OSError as exc:
                self.file = self.path.open("a", encoding="utf-8")
                log(f"Report-Ordner konnte nicht nach Geraet umbenannt werden: {exc}")
                return
            RUN_REPORT_DIR = target_dir
            self.path = target_dir / "trace.ndjson"
            self.file = self.path.open("a", encoding="utf-8")
            self.event("report_directory_renamed", report_directory=str(target_dir))
        log(f"Report-Ordner: {target_dir}")

    def event(self, event_name: str, level: str = "info", **details) -> None:
        record = {
            "timestamp": datetime.now().astimezone().isoformat(timespec="milliseconds"),
            "monotonic_ms": round((time.perf_counter() - T0) * 1000.0, 1),
            **self.context,
            "event": event_name,
            "level": level,
            "details": {key: value for key, value in details.items() if value is not None},
        }
        with self.lock:
            self.file.write(json.dumps(record, default=str, ensure_ascii=True) + "\n")
            self.file.flush()

    def close(self, exit_code: int | None = None) -> None:
        with self.lock:
            if not self.file.closed:
                self.event("run_finished", level="error" if exit_code else "info", exit_code=exit_code)
                self.file.close()


def trace_event(event_name: str, level: str = "info", **details) -> None:
    if TRACE is not None:
        if event_name == "marker_timeout" and SNIFFER is not None:
            details["rs232_tail"] = SNIFFER.tail(10)  # was die Firmware in dem Moment selbst meldet
        TRACE.event(event_name, level, **details)


# ---------------------------------------------------------------------------
# RS232-Mitschnitt der Firmware-Seite (Diagnose parallel zum BT-Test)
# ---------------------------------------------------------------------------

SNIFFER = None
RS232_PREFLIGHT_LISTEN_S = 5.0
RS232_PARTIAL_FLUSH_S = 0.3


class Rs232Sniffer:
    """Liest passiv den RS232-Ausgang der Firmware und schreibt jede Zeile in den Trace.

    Nur lesend: es wird nie etwas an die Firmware gesendet, der Menuezustand bleibt unberuehrt.
    """

    def __init__(self, port: str, baud: int):
        self.port = port
        self.baud = baud
        self.ser = None
        self.thread = None
        self.stop_event = threading.Event()
        self.pending = bytearray()    # im Preflight gelesene Bytes, gehen nicht verloren
        self.recent = []              # letzte Zeilen fuer rs232_tail
        self.lines = 0
        self.bytes = 0
        self.lock = threading.Lock()

    def open(self) -> None:
        self.ser = serial.Serial(self.port, self.baud, timeout=0.1)

    def close_port(self) -> None:
        if self.ser is not None:
            try:
                self.ser.close()
            except (serial.SerialException, OSError):
                pass

    def preflight(self, listen_s: float) -> int:
        """Anzahl Bytes, die innerhalb listen_s ankommen (Port muss offen sein)."""
        deadline = time.monotonic() + listen_s
        while time.monotonic() < deadline and not self.pending:
            self.pending.extend(self.ser.read(256))
        return len(self.pending)

    def start(self) -> None:
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def tail(self, count: int) -> list:
        with self.lock:
            return list(self.recent[-count:])

    def _emit(self, raw: bytes, partial: bool) -> None:
        text = raw.decode("cp1252", errors="replace").strip("\r\n")
        if not text.strip():
            return
        if text.isdigit() and len(text) == 6:
            text = "<redacted-pin>"
        with self.lock:
            self.lines += 1
            self.recent.append(text)
            del self.recent[:-50]
        trace_event("rs232_line", source="rs232", text=text, partial=partial or None)

    def _run(self) -> None:
        buf = bytearray()
        last_rx = time.monotonic()
        pending, self.pending = bytes(self.pending), bytearray()
        try:
            while not self.stop_event.is_set():
                chunk = pending or self.ser.read(256)
                pending = b""
                now = time.monotonic()
                if chunk:
                    self.bytes += len(chunk)
                    last_rx = now
                    buf.extend(chunk)
                    while (idx := buf.find(b"\n")) >= 0:
                        self._emit(bytes(buf[:idx]), False)
                        del buf[:idx + 1]
                elif buf and now - last_rx > RS232_PARTIAL_FLUSH_S:
                    self._emit(bytes(buf), True)  # z. B. Prompt ohne Zeilenende ("PIN = ")
                    buf.clear()
        except (serial.SerialException, OSError) as exc:
            trace_event("rs232_error", level="error", error=str(exc))
            log(f"RS232-Mitschnitt {self.port} unterbrochen: {exc}")

    def stop(self) -> None:
        self.stop_event.set()
        if self.thread is not None:
            self.thread.join(timeout=2.0)
        self.close_port()
        trace_event("rs232_sniffer_stopped", lines=self.lines, bytes=self.bytes)


def setup_rs232_sniffer(args) -> bool:
    """Prueft vor dem Test den RS232-Port; bei Problemen Retry/Weiter/Abbruch fuer den Tester.

    Gibt False zurueck, wenn der Tester abbricht.
    """
    global SNIFFER
    if args.no_rs232:
        log("RS232-Mitschnitt ausgeschaltet (--no-rs232).")
        return True
    if serial is None:
        log("WARNUNG: pyserial fehlt - kein RS232-Mitschnitt (pip install pyserial).")
        return True
    interactive = sys.stdin.isatty()
    while True:
        sniffer = Rs232Sniffer(args.rs232_port, args.rs232_baud)
        opened = False
        try:
            sniffer.open()
            opened = True
        except (serial.SerialException, OSError) as exc:
            problem = (f"{args.rs232_port} laesst sich nicht oeffnen ({exc}). Belegt durch ein anderes "
                       "Programm (Terminal, Keil)? Kabel/Port richtig?")
        if opened:
            log(f"RS232 {args.rs232_port} @ {args.rs232_baud} offen, warte "
                f"{RS232_PREFLIGHT_LISTEN_S:.0f} s auf Daten ...")
            received = sniffer.preflight(RS232_PREFLIGHT_LISTEN_S)
            if received:
                log(f"RS232-Mitschnitt aktiv ({received} Bytes empfangen).")
                trace_event("rs232_sniffer_started", port=args.rs232_port, baud=args.rs232_baud,
                            preflight_bytes=received)
                sniffer.start()
                SNIFFER = sniffer
                return True
            problem = (f"{args.rs232_port} ist offen, aber es kamen {RS232_PREFLIGHT_LISTEN_S:.0f} s lang "
                       "keine Daten. Geraet an / Firmware aktiv? Falscher Port oder Baudrate? "
                       "(Eine ruhige Firmware kann auch ohne Fehler schweigen.)")
        log("WARNUNG RS232: " + problem)
        trace_event("rs232_preflight_problem", level="warning", port=args.rs232_port, problem=problem)
        if not interactive:
            sniffer.close_port()
            log("Kein Terminal - Test laeuft ohne RS232-Mitschnitt weiter.")
            return True
        hint = " Mitschnitt trotzdem [S]tarten," if opened else ""
        try:
            answer = input(f"[R]etry,{hint} [W]eiter ohne RS232, [A]bbrechen? ").strip().lower()
        except EOFError:  # stdin ist kein echtes Terminal (z. B. umgeleitet)
            answer = "w"
        if answer.startswith("r"):
            sniffer.close_port()
            continue
        if answer.startswith("a"):
            sniffer.close_port()
            return False
        if opened and answer.startswith("s"):
            trace_event("rs232_sniffer_started", port=args.rs232_port, baud=args.rs232_baud,
                        preflight_bytes=0)
            sniffer.start()
            SNIFFER = sniffer
            log("RS232-Mitschnitt aktiv (ohne Vorab-Daten).")
            return True
        sniffer.close_port()
        log("Weiter ohne RS232-Mitschnitt.")
        return True


def payload_summary(data: bytes) -> dict:
    if len(data) == 6 and data.isdigit():
        return {"length": len(data), "payload": "<redacted-pin>"}
    preview = data[:64]
    return {"length": len(data), "hex_preview": preview.hex(" "),
            "truncated": len(data) > len(preview)}


# ---------------------------------------------------------------------------
# Scan / Verbindungshilfen (BLE, gemeinsam fuer smoke/transfer)
# ---------------------------------------------------------------------------

async def scan_for_device(name_prefix: str):
    log(f"Scanne {SCAN_TIMEOUT:.0f} s nach Geraeten mit Prefix '{name_prefix}' ...")
    trace_event("ble_scan_started", name_prefix=name_prefix, timeout_s=SCAN_TIMEOUT)
    t_scan = time.perf_counter()
    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT, return_adv=True)
    scan_ms = (time.perf_counter() - t_scan) * 1000.0
    hits = [(d, adv) for d, adv in devices.values()
            if d.name and d.name.upper().startswith(name_prefix.upper())]
    trace_event("ble_scan_finished", duration_ms=round(scan_ms, 1), devices_seen=len(devices),
                matching_devices=[{"name": d.name, "address": d.address,
                                   "rssi": getattr(adv, "rssi", None)} for d, adv in hits])
    if not hits:
        log(f"KEIN Geraet mit Prefix '{name_prefix}' gefunden nach {scan_ms:.0f} ms.")
        log("Hinweise: Advertising aktiv? BT am PC eingeschaltet? Adapter BT 4.0+?")
        return None, scan_ms
    if len(hits) > 1:
        hits.sort(key=lambda h: h[0].name)
        if not sys.stdin.isatty():
            log(f"{len(hits)} passende Geraete gefunden - bitte mit --address eingrenzen:")
            for d, _ in hits:
                print(f"    {d.address}   {d.name}")
            return None, scan_ms
        print(f"{len(hits)} passende BLE-Geraete gefunden:")
        for index, (d, adv) in enumerate(hits, start=1):
            rssi = getattr(adv, "rssi", None)
            print(f"  {index}: {d.name} ({d.address})"
                  f"{f'  RSSI={rssi} dBm' if rssi is not None else ''}")
        while True:
            answer = input(f"Geraet auswaehlen [1-{len(hits)}]: ").strip()
            if answer.isdigit() and 1 <= int(answer) <= len(hits):
                hits = [hits[int(answer) - 1]]
                break
            print("Ungueltige Auswahl.")
    device, adv = hits[0]
    rssi = getattr(adv, "rssi", None)
    log(f"Gefunden nach {scan_ms:.0f} ms: {device.name}  ({device.address})"
        f"{f'  RSSI={rssi} dBm' if rssi is not None else ''}")
    trace_event("ble_device_selected", name=device.name, address=device.address, rssi=rssi)
    return device, scan_ms


async def resolve_suite_target(args) -> bool:
    """Bindet eine kombinierte Suite mit dem Standardpraefix an genau ein BLE-Geraet.

    Classic-Pairings bleiben in Windows erhalten, auch wenn das zugehoerige Mainboard nicht
    mehr vorhanden ist. Die Suite darf deshalb nicht einen beliebigen alten Registry-Eintrag
    waehlen, wenn sie das aktuell beworbene BLE-Geraet eindeutig bestimmen kann.
    """
    if args.name.upper() != DEFAULT_NAME_PREFIX.upper():
        return True
    if BleakScanner is None:
        log("BLE-Scan zur Geraetezuordnung nicht verfuegbar; verwende Namenspraefix unveraendert.")
        return True
    device, _ = await scan_for_device(args.name)
    if device is None:
        log("Suite abgebrochen: Zielgeraet konnte per BLE nicht eindeutig bestimmt werden.")
        return False
    args.ble_device_name = device.name
    args.address = device.address
    log(f"BLE-Suite-Zielgeraet aus Advertising uebernommen: '{args.ble_device_name}' ({args.address}).")
    return True


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


def find_classic_devices(name_prefix: str) -> list[tuple[str, str]]:
    """Liest bekannte Classic-VIASIS-Geraete passiv aus der Windows-PnP-Registry.

    Get-PnpDevice darf hier nicht verwendet werden: Die aktive Bluetooth-Enumeration kann
    bereits eine SPP-Sitzung am Geraet ausloesen, bevor der Test COM oeffnet.
    BLE-Identitaeten haben den reservierten Namenssuffix ``_BLE`` und sind keine
    SPP-Kandidaten, auch wenn Windows sie unter BTHENUM auffuehrt.
    """
    if sys.platform != "win32":
        raise ConnectionError("BT-Classic-Autoerkennung ist nur unter Windows verfuegbar.")
    try:
        completed = subprocess.run(
            ["reg.exe", "query", r"HKLM\SYSTEM\CurrentControlSet\Enum\BTHENUM",
             "/s", "/v", "FriendlyName"],
            capture_output=True, timeout=20, check=False)
    except (OSError, subprocess.TimeoutExpired):
        raise ConnectionError("Windows-Bluetooth-Pairing-Registry konnte nicht abgefragt werden.")

    candidates = []
    current_key = ""
    output = completed.stdout.decode("utf-8", errors="replace")
    for line in output.splitlines():
        line = line.strip()
        if line.upper().startswith("HKEY_"):
            current_key = line
            continue
        match = re.match(r"FriendlyName\s+REG_SZ\s+(.+)", line, re.IGNORECASE)
        address_match = re.search(r"DEV_([0-9A-F]{12})", current_key, re.IGNORECASE)
        if match and address_match:
            candidates.append((match.group(1).strip(), address_match.group(1).upper()))
    candidates = [(name, address) for name, address in candidates
                  if (name.upper().startswith(name_prefix.upper())
                      and not name.upper().endswith("_BLE"))]
    if not candidates:
        raise ConnectionError(f"Kein gepaartes VIASIS-Geraet gefunden (Suchbegriff: '{name_prefix}').")
    return candidates


def scan_for_classic_devices(name_prefix: str) -> list[tuple[str, str]]:
    """Sucht sichtbare BT-Classic-Geraete per Windows-Inquiry.

    Die Inquiry bestimmt die Classic-Zielgeraete. Die Registry wird danach nur noch
    verwendet, um fuer das gewaehlte, bereits gepaarte Geraet den SPP-Port zu lesen.
    """
    if sys.platform != "win32":
        raise ConnectionError("BT-Classic-Scan ist nur unter Windows verfuegbar.")

    class BluetoothFindRadioParams(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD)]

    class SystemTime(ctypes.Structure):
        _fields_ = [("wYear", wintypes.WORD), ("wMonth", wintypes.WORD),
                    ("wDayOfWeek", wintypes.WORD), ("wDay", wintypes.WORD),
                    ("wHour", wintypes.WORD), ("wMinute", wintypes.WORD),
                    ("wSecond", wintypes.WORD), ("wMilliseconds", wintypes.WORD)]

    class BluetoothDeviceSearchParams(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD),
                    ("fReturnAuthenticated", wintypes.BOOL),
                    ("fReturnRemembered", wintypes.BOOL),
                    ("fReturnUnknown", wintypes.BOOL),
                    ("fReturnConnected", wintypes.BOOL),
                    ("fIssueInquiry", wintypes.BOOL),
                    ("cTimeoutMultiplier", ctypes.c_ubyte),
                    ("hRadio", wintypes.HANDLE)]

    class BluetoothDeviceInfo(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("address", ctypes.c_ulonglong),
                    ("class_of_device", wintypes.DWORD), ("fConnected", wintypes.BOOL),
                    ("fRemembered", wintypes.BOOL), ("fAuthenticated", wintypes.BOOL),
                    ("stLastSeen", SystemTime), ("stLastUsed", SystemTime),
                    ("name", ctypes.c_wchar * 248)]

    api = ctypes.WinDLL("BluetoothAPIs.dll", use_last_error=True)
    find_first_radio = api.BluetoothFindFirstRadio
    find_first_radio.argtypes = (ctypes.POINTER(BluetoothFindRadioParams), ctypes.POINTER(wintypes.HANDLE))
    find_first_radio.restype = wintypes.HANDLE
    find_next_radio = api.BluetoothFindNextRadio
    find_next_radio.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.HANDLE))
    find_next_radio.restype = wintypes.BOOL
    find_radio_close = api.BluetoothFindRadioClose
    find_radio_close.argtypes = (wintypes.HANDLE,)
    find_radio_close.restype = wintypes.BOOL
    find_first_device = api.BluetoothFindFirstDevice
    find_first_device.argtypes = (ctypes.POINTER(BluetoothDeviceSearchParams), ctypes.POINTER(BluetoothDeviceInfo))
    find_first_device.restype = wintypes.HANDLE
    find_next_device = api.BluetoothFindNextDevice
    find_next_device.argtypes = (wintypes.HANDLE, ctypes.POINTER(BluetoothDeviceInfo))
    find_next_device.restype = wintypes.BOOL
    find_device_close = api.BluetoothFindDeviceClose
    find_device_close.argtypes = (wintypes.HANDLE,)
    find_device_close.restype = wintypes.BOOL
    close_handle = ctypes.WinDLL("kernel32.dll", use_last_error=True).CloseHandle
    close_handle.argtypes = (wintypes.HANDLE,)
    close_handle.restype = wintypes.BOOL

    log(f"Scanne {CLASSIC_INQUIRY_TIMEOUT_MULTIPLIER * 1.28:.1f} s nach sichtbaren Classic-Geraeten mit Prefix '{name_prefix}' ...")
    radio_params = BluetoothFindRadioParams(ctypes.sizeof(BluetoothFindRadioParams))
    radio = wintypes.HANDLE()
    radio_finder = find_first_radio(ctypes.byref(radio_params), ctypes.byref(radio))
    if not radio_finder:
        raise ConnectionError("Kein Windows-Bluetooth-Radio fuer Classic-Inquiry gefunden.")

    devices: dict[str, tuple[str, str, datetime]] = {}
    try:
        while True:
            search = BluetoothDeviceSearchParams(
                ctypes.sizeof(BluetoothDeviceSearchParams), True, True, True, True, True,
                CLASSIC_INQUIRY_TIMEOUT_MULTIPLIER, radio)
            info = BluetoothDeviceInfo()
            info.dwSize = ctypes.sizeof(BluetoothDeviceInfo)
            device_finder = find_first_device(ctypes.byref(search), ctypes.byref(info))
            if device_finder:
                try:
                    while True:
                        name = info.name.strip()
                        if name.upper().startswith(name_prefix.upper()) and not name.upper().endswith("_BLE"):
                            address = f"{info.address:012X}"
                            try:
                                seen_at = datetime(info.stLastSeen.wYear, info.stLastSeen.wMonth,
                                                   info.stLastSeen.wDay, info.stLastSeen.wHour,
                                                   info.stLastSeen.wMinute, info.stLastSeen.wSecond,
                                                   info.stLastSeen.wMilliseconds * 1000)
                                age_seconds = (datetime.utcnow() - seen_at).total_seconds()
                            except ValueError:
                                age_seconds = float("inf")
                            if -1.0 <= age_seconds <= CLASSIC_VISIBILITY_MAX_AGE_SECONDS:
                                devices[address] = (name, address, seen_at)
                            else:
                                log(f"Classic nicht aktuell sichtbar: {name} ({address}), "
                                    f"zuletzt gesehen vor {age_seconds:.0f} s.")
                        info = BluetoothDeviceInfo()
                        info.dwSize = ctypes.sizeof(BluetoothDeviceInfo)
                        if not find_next_device(device_finder, ctypes.byref(info)):
                            break
                finally:
                    find_device_close(device_finder)
            close_handle(radio)
            radio = wintypes.HANDLE()
            if not find_next_radio(radio_finder, ctypes.byref(radio)):
                break
    finally:
        find_radio_close(radio_finder)

    candidates = [(name, address) for name, address, _ in devices.values()]
    if not candidates:
        raise ConnectionError(f"Kein sichtbares Classic-VIASIS-Geraet gefunden (Suchbegriff: '{name_prefix}').")
    for name, address, seen_at in devices.values():
        log(f"Classic gefunden: {name} ({address}), zuletzt gesehen {seen_at:%Y-%m-%d %H:%M:%S.%f} UTC.")
    return candidates


def find_classic_port(address: str, requested_port: str | None) -> str:
    """Liest den gespeicherten SPP-Port passiv aus der Windows-PnP-Registry."""
    try:
        completed = subprocess.run(
            ["reg.exe", "query", r"HKLM\SYSTEM\CurrentControlSet\Enum\BTHENUM",
             "/s", "/v", "PortName"],
            capture_output=True, timeout=20, check=False)
    except (OSError, subprocess.TimeoutExpired):
        raise ConnectionError("Windows-SPP-Port-Registry konnte nicht abgefragt werden.")
    ports = []
    current_key = ""
    output = completed.stdout.decode("utf-8", errors="replace")
    for line in output.splitlines():
        line = line.strip()
        if line.upper().startswith("HKEY_"):
            current_key = line
            continue
        match = re.match(r"PortName\s+REG_SZ\s+(COM\d+)", line, re.IGNORECASE)
        if match and address.upper() in current_key.upper():
            ports.append(match.group(1).upper())
    if requested_port:
        ports = [port for port in ports if port == requested_port.upper()]
    if len(ports) != 1:
        raise ConnectionError(f"Kein eindeutiger SPP-COM-Port fuer Bluetooth-Adresse {address} gefunden.")
    return ports[0]


def choose_classic_device(candidates: list[tuple[str, str]]) -> tuple[str, str]:
    if len(candidates) == 1:
        return candidates[0]

    print("Mehrere VIASIS-Geraete gefunden:")
    for index, (name, address) in enumerate(candidates, start=1):
        print(f"  {index}: {name} ({address})")
    while True:
        answer = input(f"Geraet auswaehlen [1-{len(candidates)}]: ").strip()
        if answer.isdigit() and 1 <= int(answer) <= len(candidates):
            return candidates[int(answer) - 1]
        print("Ungueltige Auswahl.")


def find_classic_match_for_ble(ble_name: str | None,
                               candidates: list[tuple[str, str]]) -> tuple[str, str] | None:
    """Gibt den eindeutigen Classic-Partner zu VIASIS_<ser>_BLE zurueck."""
    match = re.fullmatch(r"(VIASIS_[^_]+)_BLE", ble_name or "", re.IGNORECASE)
    if not match:
        return None
    classic_name = match.group(1)
    matches = [candidate for candidate in candidates if candidate[0].casefold() == classic_name.casefold()]
    return matches[0] if len(matches) == 1 else None


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
    report_dir = RUN_REPORT_DIR or REPORT_DIR
    report_dir.mkdir(parents=True, exist_ok=True)
    base = report_dir / kind
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
        "pin_prompt_seen": False,
        "pin_menu_ok": False,
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
    device_name = device.name if device else getattr(args, "ble_device_name", None)
    result["phases_ms"]["scan"] = round(scan_ms, 1)

    disconnected = asyncio.Event()
    closing = {"by_us": False}   # True ab dem gewollten Trennen am Testende: der Callback feuert dann auch

    def on_disconnect(_client):
        if closing["by_us"]:
            return               # eigenes, gewolltes Trennen - weder "unerwartet" noch "vom Geraet ausgeloest"
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

        notify_rx = ByteStream()

        def on_notify(_sender, data: bytearray):
            payload = bytes(data)
            log(f"<== unaufgeforderte Daten  {payload!r}")
            notify_rx.feed(payload)

        t_sub = time.perf_counter()
        await client.start_notify(char, on_notify)
        sub_ms = (time.perf_counter() - t_sub) * 1000.0
        result["phases_ms"]["subscribe"] = round(sub_ms, 1)
        result["subscribe_ok"] = True
        log(f"Subscribe (CCCD-Write) quittiert nach {sub_ms:.0f} ms.")

        pin = args.pin or derive_pin_from_name(device_name or "")
        if pin:
            if await notify_rx.wait_for_marker(MARK_PIN_PROMPT, args.menu_timeout):
                result["pin_prompt_seen"] = True
                log("PIN-Prompt empfangen.")
                await send_pin(client, char, pin)
                result["pin_menu_ok"] = await notify_rx.wait_for_marker(
                    MARK_HAUPTMENU, args.menu_timeout)
                if result["pin_menu_ok"]:
                    log("Hauptmenue nach PIN erreicht.")
                else:
                    log("Kein Hauptmenue nach PIN gesehen.")
            else:
                log("PIN-Prompt nicht gesehen; PIN wird nicht gesendet.")
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
        result["result"] = ("passed" if result["subscribe_ok"] and result["pin_menu_ok"]
                            and not result["unexpected_disconnect"] else "unstable")
    finally:
        closing["by_us"] = True
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

    def __init__(self, source: str = "unknown"):
        self.buf = bytearray()
        self.event = asyncio.Event()
        self.source = source

    def feed(self, data: bytes) -> None:
        self.buf.extend(data)
        trace_event("rx_data", source=self.source, buffered_bytes=len(self.buf), **payload_summary(data))
        self.event.set()

    def clear(self) -> None:
        self.buf.clear()

    async def wait_for_marker(self, marker: str, timeout: float) -> bool:
        needle = marker.encode("ascii")
        deadline = time.monotonic() + timeout
        while needle not in self.buf:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                trace_event("marker_timeout", level="warning", source=self.source,
                            marker=marker, timeout_s=timeout, buffered_bytes=len(self.buf))
                return False
            self.event.clear()
            try:
                await asyncio.wait_for(self.event.wait(), timeout=remaining)
            except asyncio.TimeoutError:
                pass
        trace_event("marker_found", source=self.source, marker=marker, buffered_bytes=len(self.buf))
        return True

    async def wait_for_any_marker(self, markers: tuple[str, ...], timeout: float) -> str | None:
        needles = [(marker, marker.encode("ascii")) for marker in markers]
        deadline = time.monotonic() + timeout
        while True:
            for marker, needle in needles:
                if needle in self.buf:
                    return marker
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                trace_event("marker_timeout", level="warning", source=self.source,
                            markers=list(markers), timeout_s=timeout, buffered_bytes=len(self.buf))
                return None
            self.event.clear()
            try:
                await asyncio.wait_for(self.event.wait(), timeout=remaining)
            except asyncio.TimeoutError:
                pass

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
        self.rx = ByteStream("ble")
        self.client: BleakClient | None = None
        self.char = None
        self.device_name = None

    async def connect(self, timeout: float) -> None:
        if self.args.address:
            address = self.args.address
            self.device_name = getattr(self.args, "ble_device_name", None)
        else:
            device, _ = await scan_for_device(self.args.name)
            if device is None:
                raise ConnectionError("Geraet nicht gefunden (Scan).")
            address, self.device_name = device.address, device.name
            self.args.address = address  # fuer Folgezyklen (cycle) wiederverwenden, kein Re-Scan
            self.args.ble_device_name = self.device_name
        if TRACE is not None:
            TRACE.set_device_name(self.device_name)
        trace_event("connect_started", transport="ble", address=address, timeout_s=timeout)
        started = time.perf_counter()
        self.client = BleakClient(address)
        await self.client.connect(timeout=timeout)
        trace_event("connect_succeeded", transport="ble", address=address,
                    duration_ms=round((time.perf_counter() - started) * 1000.0, 1))
        self.char = find_char_by_suffix(self.client, CYSPP_DATA_SUFFIX)
        if self.char is None:
            trace_event("cyspp_characteristic_missing", level="error", transport="ble")
            raise ConnectionError(f"Keine Characteristic mit Suffix ...{CYSPP_DATA_SUFFIX} gefunden.")
        trace_event("cyspp_characteristic_found", transport="ble", uuid=str(self.char.uuid),
                    properties=list(self.char.properties))
        self.rx.clear()

        def on_notify(_sender, data: bytearray):
            self.rx.feed(bytes(data))

        await self.client.start_notify(self.char, on_notify)
        trace_event("notify_subscribed", transport="ble", uuid=str(self.char.uuid))
        await asyncio.sleep(0.8)   # Windows-BLE-Stack Zeit geben, bis Notify-Zustellung wirklich steht

    async def disconnect(self) -> None:
        if self.client and self.client.is_connected:
            trace_event("disconnect_started", transport="ble")
            await self.client.disconnect()
            trace_event("disconnect_succeeded", transport="ble")

    @property
    def is_connected(self) -> bool:
        return bool(self.client and self.client.is_connected)

    async def write(self, data: bytes) -> None:
        trace_event("tx_data", transport="ble", **payload_summary(data))
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
        self.rx = ByteStream("classic")
        self.ser: "serial.Serial | None" = None
        self._reader_thread = None
        self._stop = None
        self.device_name = None

        self.port = None

    async def _step(self, message: str, force: bool = False) -> None:
        if self.args.classic_step and (force or not self.args.download_count_step):
            await asyncio.to_thread(input, f"[Classic-Schritt] {message}  Enter zum Fortsetzen: ")

    async def connect(self, timeout: float) -> None:
        import threading
        self.rx.clear()
        if not hasattr(self.args, "classic_device_name"):
            await self._step("Noch keine BT-Classic-Inquiry ausgefuehrt. RS232 beobachten.")
            candidates = await asyncio.to_thread(scan_for_classic_devices, self.args.name)
            classic_match = find_classic_match_for_ble(
                getattr(self.args, "ble_device_name", None), candidates)
            if classic_match:
                self.args.classic_device_name, address = classic_match
                log(f"Classic-Geraet passend zum BLE-Ziel automatisch gewaehlt: "
                    f"'{self.args.classic_device_name}' ({address}).")
            else:
                self.args.classic_device_name, address = await asyncio.to_thread(
                    choose_classic_device, candidates)
                log(f"Gefunden: BT-Geraet '{self.args.classic_device_name}' ({address}).")
            await self._step("Bluetooth-Geraetesuche beendet, SPP-COM-Port noch NICHT abgefragt. RS232 beobachten.")
            self.args.classic_port = await asyncio.to_thread(find_classic_port, address, self.args.port)
            log(f"SPP-Port fuer {self.args.classic_device_name}: {self.args.classic_port}.")
            await self._step("SPP-COM-Port abgefragt, aber noch NICHT geoeffnet. RS232 beobachten.")
        self.device_name = self.args.classic_device_name
        self.port = self.args.classic_port
        if TRACE is not None:
            TRACE.set_device_name(self.device_name)
        await self._step(f"Jetzt wird {self.port} geoeffnet und damit SPP angefordert. RS232 beobachten.")
        trace_event("connect_started", transport="classic", port=self.port,
                    device_name=self.device_name, timeout_s=timeout)
        started = time.perf_counter()
        try:
            self.ser = serial.Serial(self.port, self.args.baud, timeout=0.2)
        except OSError as exc:
            if getattr(exc, "winerror", None) == 121:
                raise ConnectionError(
                    f"Windows konnte keine SPP-Verbindung zu {self.device_name} ueber "
                    f"{self.port} aufbauen (Semaphore-Timeout 121).") from exc
            raise
        trace_event("connect_succeeded", transport="classic", port=self.port,
                    duration_ms=round((time.perf_counter() - started) * 1000.0, 1))
        self._stop = threading.Event()
        loop = asyncio.get_running_loop()

        def reader():
            while not self._stop.is_set():
                try:
                    first = self.ser.read(1)
                    data = first + self.ser.read(self.ser.in_waiting) if first else b""
                except (OSError, serial.SerialException):
                    break
                if data:
                    if self.args.classic_trace:
                        loop.call_soon_threadsafe(
                            log, f"{self.port} RX: len={len(data)} hex={data.hex(' ')} ascii={data!r}")
                    loop.call_soon_threadsafe(self.rx.feed, data)

        self._reader_thread = threading.Thread(target=reader, daemon=True)
        self._reader_thread.start()
        trace_event("classic_reader_started", port=self.port)
        await self._step("COM-Port offen, Reader aktiv, aber keine PIN gesendet. RS232 beobachten.")

    async def disconnect(self) -> None:
        trace_event("disconnect_started", transport="classic", port=self.port)
        if self._stop:
            self._stop.set()
        if self._reader_thread:
            await asyncio.to_thread(self._reader_thread.join, 2.0)
        if self.ser and self.ser.is_open:
            await asyncio.to_thread(self.ser.close)
        trace_event("disconnect_succeeded", transport="classic", port=self.port)

    @property
    def is_connected(self) -> bool:
        return bool(self.ser and self.ser.is_open)

    async def write(self, data: bytes) -> None:
        trace_event("tx_data", transport="classic", port=self.port, **payload_summary(data))
        written = await asyncio.to_thread(self.ser.write, data)
        if written != len(data):
            raise ConnectionError(f"Nur {written}/{len(data)} Byte an {self.port} geschrieben.")
        await asyncio.to_thread(self.ser.flush)
        if data != bytes([XMODEM_ACK]):
            log(f"{self.port} TX: len={written} hex={data.hex(' ')} ascii={data!r} "
                "(an Windows uebergeben).")

    def pin(self) -> str | None:
        return self.args.pin or derive_pin_from_name(self.device_name or "")


async def make_transport(args):
    if args.transport == "ble":
        return BleTransport(args)
    return ClassicTransport(args)


# Menuetext-Marker fuer die transportneutrale Navigation - alle aus dem Firmware-Quellcode
# entnommen (sictxt.h #defines bzw. Sprachtabellen), transportunabhaengig gueltig.
MARK_HAUPTMENU = "Hauptmen"
MARK_PIN_PROMPT = "PIN = "
MARK_TESTMENU = "Testmen"
MARK_WERKMENUE = "Change configuration"       # T_cconf, werkmenue()'s Menuepunkt 1
MARK_COMPMENU = "Write flash data"            # T_comp, compmenu()'s Menuepunkt 1
MARK_SELECT_DATA = "Select Data"              # T_flash_w_d (write_flash())
MARK_ANZMW = "Anzahl Messwerte="              # getnumber(T_anzmw) prompt
MARK_DAUS = "Daten ausgeben"                  # T_daus (messdaten() Abfrage)
MARK_DELETE_DATA = "Daten l"                  # T_delete, ASCII-Praefix vor dem Umlaut
MARK_MENU_PROMPT = "Ihre Auswahl"              # gemeinsame Eingabeaufforderung der Menues


async def send_and_settle(transport, data: bytes, settle: float = 1.0) -> None:
    await transport.write(data)
    await asyncio.sleep(settle)


async def wait_for_menu_step(args, message: str) -> None:
    if args.menu_step:
        await asyncio.to_thread(input, f"[Menue-Schritt] {message}  Enter zum Senden: ")


async def authenticate_or_sync_main_menu(transport, args, operation: str) -> None:
    pin = transport.pin()
    if await transport.rx.wait_for_marker(MARK_PIN_PROMPT, args.menu_timeout):
        if not pin:
            raise ConnectionError(f"PIN-Prompt empfangen, aber keine PIN fuer {operation} ableitbar.")
        log("PIN-Prompt empfangen.")
        log(f"Sende PIN einmal ({len(pin)} Ziffern).")
        await send_and_settle(transport, pin.encode("ascii"), settle=1.5)
        if not await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
            raise ConnectionError(f"Kein Hauptmenue nach PIN gesehen; {operation} wird nicht gestartet.")
        log("Hauptmenue erreichbar (PIN akzeptiert).")
        return

    log("Kein PIN-Prompt; pruefe bestehende Sitzung mit 'H'.")
    transport.rx.clear()
    await wait_for_menu_step(args, "Kein PIN-Prompt. Sende 'H' und pruefe das Hauptmenue.")
    await send_and_settle(transport, b"H", settle=0.2)
    if not await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
        raise ConnectionError(f"Kein PIN-Prompt und kein Hauptmenue nach 'H'; {operation} wird nicht gestartet.")
    log("Hauptmenue einer bestehenden Sitzung erreichbar.")


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
    hold_times = args.cycle_holds or [args.hold] * args.count
    result = {
        "mode": "cycle",
        "transport": args.transport,
        "port": None,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "requested_cycles": len(hold_times),
        "hold_seconds": hold_times,
        "cycles": [],
    }

    for i, hold_seconds in enumerate(hold_times, start=1):
        if TRACE is not None:
            TRACE.set_context(cycle=i, transport=args.transport)
        trace_event("cycle_started", hold_seconds=hold_seconds)
        log(f"--- Zyklus {i}/{len(hold_times)} (halte {hold_seconds:g} s) ---")
        cycle = {"index": i, "connect_ms": None, "pin_menu_ok": False,
                  "device_feedback_after_menu": None, "device_feedback_after_hold": None,
                  "disconnect_ms": None, "status": "failed", "error": None}
        transport = await make_transport(args)
        t0 = time.perf_counter()
        try:
            await transport.connect(args.connect_timeout)
            result["port"] = getattr(transport, "port", None)
            cycle["connect_ms"] = round((time.perf_counter() - t0) * 1000.0, 1)
            log(f"Verbunden nach {cycle['connect_ms']:.0f} ms.")

            pin = transport.pin()
            pin_sent = False
            if pin:
                if await transport.rx.wait_for_marker(MARK_PIN_PROMPT, args.menu_timeout):
                    log("PIN-Prompt empfangen.")
                    trace_event("pin_prompt_received", transport=args.transport)
                else:
                    log("PIN-Prompt nicht gesehen; sende bekannte PIN trotzdem (Prompt kann beim Reconnect verloren gehen).")
                    trace_event("pin_prompt_missing_fallback", level="warning", transport=args.transport,
                                timeout_s=args.menu_timeout)
                log(f"Sende PIN einmal ({len(pin)} Ziffern).")
                trace_event("pin_sent", transport=args.transport,
                            source="argument" if args.pin else "device_name", length=len(pin))
                await send_and_settle(transport, pin.encode("ascii"), settle=1.5)
                pin_sent = True
            else:
                log("Keine PIN ableitbar; PIN wird nicht gesendet.")
                trace_event("pin_unavailable", level="warning", transport=args.transport)
            if pin_sent and await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
                cycle["pin_menu_ok"] = True
                log("Hauptmenue erreichbar (PIN akzeptiert).")
            elif not pin_sent:
                log("PIN/Menue nicht geprueft (PIN nicht gesendet).")
            else:
                log("Kein Hauptmenue nach PIN gesehen (PIN abgelehnt oder Verbindung instabil).")
            feedback = bytes(transport.rx.buf)
            cycle["device_feedback_after_menu"] = feedback.hex(" ")
            log(f"Geraeteantwort nach PIN/Menuetest: {feedback!r}")

            if hold_seconds > 0:
                log(f"Halte Verbindung {hold_seconds:g} s ...")
                await asyncio.sleep(hold_seconds)
            feedback = bytes(transport.rx.buf)
            cycle["device_feedback_after_hold"] = feedback.hex(" ")
            if feedback.hex(" ") != cycle["device_feedback_after_menu"]:
                log(f"Geraeteantwort nach Haltezeit: {feedback!r}")

            cycle["status"] = "passed" if cycle["pin_menu_ok"] else (
                "pin-not-tested" if not pin else "unstable")
        except Exception as exc:
            cycle["error"] = str(exc)
            log(f"FEHLER: {exc}")
            trace_event("cycle_error", level="error", exception_type=type(exc).__name__, error=str(exc))
        finally:
            t_disc = time.perf_counter()
            try:
                await asyncio.wait_for(transport.disconnect(), timeout=15.0)
            except Exception as exc:
                log(f"WARNUNG: Trennen hing/schlug fehl ({exc}) - naechster Zyklus versucht es trotzdem.")
            cycle["disconnect_ms"] = round((time.perf_counter() - t_disc) * 1000.0, 1)

        result["cycles"].append(cycle)
        trace_event("cycle_finished", status=cycle["status"], connect_ms=cycle["connect_ms"],
                    disconnect_ms=cycle["disconnect_ms"], pin_menu_ok=cycle["pin_menu_ok"], error=cycle["error"])
        if i < len(hold_times):
            await asyncio.sleep(args.cooldown)

    _finish_cycle(result)
    passed = sum(1 for c in result["cycles"] if c["status"] == "passed")
    return 0 if passed == len(hold_times) else 1


def _finish_cycle(result: dict) -> None:
    cycles = result["cycles"]
    passed = sum(1 for c in cycles if c["status"] == "passed")
    connect_times = [c["connect_ms"] for c in cycles if c["connect_ms"] is not None]
    print()
    print("=" * 70)
    print(f"  CYCLE-TEST ZUSAMMENFASSUNG ({result['transport']})")
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
        f"- COM-Port: `{result['port']}`" if result["port"] else None,
        f"- Bestanden: **{passed}/{len(cycles)}**",
        f"- Haltezeiten je Zyklus: `{result['hold_seconds']}` s",
        "",
        "| Zyklus | Connect (ms) | PIN/Menue ok | Geraeteantwort (Hex) | Disconnect (ms) | Status |",
        "| ---: | ---: | :--: | --- | ---: | --- |",
    ]
    lines = [line for line in lines if line is not None]
    for c in cycles:
        feedback = c["device_feedback_after_hold"] or c["device_feedback_after_menu"] or "-"
        lines.append(f"| {c['index']} | {c['connect_ms']} | {c['pin_menu_ok']} | "
                  f"`{feedback}` | {c['disconnect_ms']} | {c['status']} |")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    log(f"JSON-Report: {json_path}")
    log(f"Markdown-Report: {md_path}")


# ---------------------------------------------------------------------------
# Idle-Latency-Test: prueft den Verdacht "haengt/wird langsam, wenn man laenger als
# ~1 Minute in einem Menue nichts tut" (project-status.md, connection-pin-Untersuchung
# 2026-09-30). Verbindet frisch (--transport classic), erreicht das Hauptmenue, wartet dann
# GENAU die angegebene Leerlaufzeit (--idle-times, mehrere Werte rund um 60/120s zum Vergleich),
# und misst danach die Reaktionszeit auf EINE minimale Navigation (Hauptmenue -> Testmenue -> zurueck).
# Auffaellig langsame Reaktionszeiten just bei Vielfachen von ~60s wuerden den im Code
# gefundenen Minutentick-Verdacht (measure.c, bt_release() bei jeder RTC-Minute) direkt belegen.
# ---------------------------------------------------------------------------

async def run_idle_latency_test(args) -> int:
    idle_times = args.idle_times or [20.0, 55.0, 65.0, 90.0, 125.0]
    result = {
        "mode": "idle-latency",
        "transport": args.transport,
        "port": None,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "idle_seconds_tested": idle_times,
        "runs": [],
    }

    for i, idle_seconds in enumerate(idle_times, start=1):
        if TRACE is not None:
            TRACE.set_context(run=i, transport=args.transport, idle_seconds=idle_seconds)
        trace_event("idle_latency_run_started", idle_seconds=idle_seconds)
        log(f"--- Lauf {i}/{len(idle_times)} (Leerlaufzeit {idle_seconds:g} s) ---")
        run = {"index": i, "idle_seconds": idle_seconds, "connect_ms": None,
               "menu_ready_ok": False, "into_testmenu_ms": None, "back_to_hauptmenu_ms": None,
               "status": "failed", "error": None}
        transport = await make_transport(args)
        t0 = time.perf_counter()
        try:
            await transport.connect(args.connect_timeout)
            result["port"] = getattr(transport, "port", None)
            run["connect_ms"] = round((time.perf_counter() - t0) * 1000.0, 1)
            log(f"Verbunden nach {run['connect_ms']:.0f} ms.")

            await authenticate_or_sync_main_menu(transport, args, "idle-latency")
            run["menu_ready_ok"] = True

            log(f"Halte Verbindung {idle_seconds:g} s im Hauptmenue, keine Aktion ...")
            transport.rx.clear()
            await asyncio.sleep(idle_seconds)

            t1 = time.perf_counter()
            await transport.write(b"3")  # Hauptmenue -> Testmenue (rein lesende Navigation)
            if await transport.rx.wait_for_marker(MARK_TESTMENU, args.menu_timeout):
                run["into_testmenu_ms"] = round((time.perf_counter() - t1) * 1000.0, 1)
                log(f"Testmenue erreicht nach {run['into_testmenu_ms']:.0f} ms.")
            else:
                log(f"KEIN Testmenue-Marker innerhalb {args.menu_timeout:.0f}s gesehen - haengt/extrem langsam.")

            transport.rx.clear()
            t2 = time.perf_counter()
            await transport.write(b"9")  # Testmenue -> zurueck ins Hauptmenue
            if await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
                run["back_to_hauptmenu_ms"] = round((time.perf_counter() - t2) * 1000.0, 1)
                log(f"Zurueck im Hauptmenue nach {run['back_to_hauptmenu_ms']:.0f} ms.")
            else:
                log(f"KEIN Hauptmenue-Marker innerhalb {args.menu_timeout:.0f}s gesehen - haengt/extrem langsam.")

            run["status"] = ("passed" if (run["into_testmenu_ms"] is not None
                                           and run["back_to_hauptmenu_ms"] is not None) else "unstable")
        except Exception as exc:
            run["error"] = str(exc)
            log(f"FEHLER: {exc}")
            trace_event("idle_latency_run_error", level="error", exception_type=type(exc).__name__, error=str(exc))
        finally:
            try:
                await asyncio.wait_for(transport.disconnect(), timeout=15.0)
            except Exception as exc:
                log(f"WARNUNG: Trennen hing/schlug fehl ({exc}).")

        result["runs"].append(run)
        trace_event("idle_latency_run_finished", status=run["status"], idle_seconds=idle_seconds,
                    into_testmenu_ms=run["into_testmenu_ms"], back_to_hauptmenu_ms=run["back_to_hauptmenu_ms"])
        if i < len(idle_times):
            await asyncio.sleep(args.cooldown)

    _finish_idle_latency(result)
    passed = sum(1 for r in result["runs"] if r["status"] == "passed")
    return 0 if passed == len(idle_times) else 1


def _finish_idle_latency(result: dict) -> None:
    runs = result["runs"]
    passed = sum(1 for r in runs if r["status"] == "passed")
    print()
    print("=" * 70)
    print(f"  IDLE-LATENCY-TEST ZUSAMMENFASSUNG ({result['transport']})")
    print("=" * 70)
    print(f"  Laeufe bestanden:  {passed}/{len(runs)}")
    print(f"  {'Idle (s)':>10}  {'->Testmenue (ms)':>18}  {'->Hauptmenue (ms)':>18}  Status")
    for r in runs:
        into = f"{r['into_testmenu_ms']:.0f}" if r["into_testmenu_ms"] is not None else "TIMEOUT"
        back = f"{r['back_to_hauptmenu_ms']:.0f}" if r["back_to_hauptmenu_ms"] is not None else "TIMEOUT"
        print(f"  {r['idle_seconds']:>10.0f}  {into:>18}  {back:>18}  {r['status']}")
    print()
    json_path, md_path = write_report("idle-latency", result)
    lines = [
        "# Idle-Latency-Test (Reaktionszeit nach Leerlauf im Menue)",
        "",
        f"- Transport: `{result['transport']}`",
        f"- COM-Port: `{result['port']}`" if result["port"] else None,
        f"- Bestanden: **{passed}/{len(runs)}**",
        "",
        "| Idle (s) | ->Testmenue (ms) | ->Hauptmenue (ms) | Status |",
        "| ---: | ---: | ---: | --- |",
    ]
    lines = [line for line in lines if line is not None]
    for r in runs:
        into = f"{r['into_testmenu_ms']:.0f}" if r["into_testmenu_ms"] is not None else "TIMEOUT"
        back = f"{r['back_to_hauptmenu_ms']:.0f}" if r["back_to_hauptmenu_ms"] is not None else "TIMEOUT"
        lines.append(f"| {r['idle_seconds']:.0f} | {into} | {back} | {r['status']} |")
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
        result["port"] = transport.port
        log(f"Port {transport.port} offen nach {result['connect_ms']:.0f} ms.")

        log("Warte auf Banner/PIN-Prompt ...")
        await asyncio.sleep(3.0)   # Boot-Banner + Pin-Abfrage brauchen etwas Zeit
        if transport.rx.buf:
            result["banner_seen"] = True
            log(f"Empfangen: {bytes(transport.rx.buf)!r}")
        pin = transport.pin()
        if pin and await transport.rx.wait_for_marker(MARK_PIN_PROMPT, args.menu_timeout):
            log("PIN-Prompt empfangen.")
            log(f"Sende PIN einmal ({len(pin)} Ziffern).")
            await send_and_settle(transport, pin.encode("ascii"), settle=1.5)
        elif pin:
            log("PIN-Prompt nicht gesehen; PIN wird nicht gesendet.")
            pin = None
        else:
            log("Keine PIN ableitbar; PIN wird nicht gesendet.")

        if pin and await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
            result["pin_menu_ok"] = True
            log("Hauptmenue erreichbar.")
        else:
            log(f"Kein Hauptmenue-Echo gesehen: {bytes(transport.rx.buf)!r}")

        result["result"] = "passed" if result["banner_seen"] and result["pin_menu_ok"] else "unstable"
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
    ('DATA', blocknr, payload) oder wirft eine Exception (CAN/Blockfehler).

    Fuehrende Menueechos koennen nach dem Start-C eintreffen. Sie gehoeren zu keinem
    Ymodem-Block und werden bis zum Blockstart ohne NAK und ohne Retryverbrauch verworfen.
    """
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Timeout beim Warten auf Blockstart/EOT.")
        first = await rx.read_byte(remaining)
        if first is None:
            raise TimeoutError("Timeout beim Warten auf Blockstart/EOT.")
        if first in (XMODEM_SOH, XMODEM_STX, XMODEM_EOT, XMODEM_CAN):
            break
        log(f"Ymodem: fuehrendes Fremdbyte 0x{first:02X} verworfen.")
    if first == XMODEM_EOT:
        return "EOT", None, None
    if first == XMODEM_CAN:
        raise ConnectionAbortedError("Geraet hat mit CAN abgebrochen.")
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


VTF_BYTES_PER_RECORD = 10.0118   # gemessen: 419430 Messwerte -> 4199424 Byte Ymodem-Datei (mehrere Laeufe)


def estimate_vtf_size(menu_text: bytes) -> int | None:
    """Schaetzt die Ymodem-Dateigroesse aus 'Anzahl Messwerte = N' im Menuetext (Ymodem-Header enthaelt keine Groesse)."""
    match = re.search(rb"Anzahl Messwerte\s*=\s*(\d+)", menu_text)
    return round(int(match.group(1)) * VTF_BYTES_PER_RECORD) if match else None


async def ymodem_receive(transport, block_timeout: float, retry_limit: int = 5,
                         expected_size: int | None = None) -> dict:
    """Empfaengt einen Ymodem-Batch-Transfer, exakt wie ihn sendblock()/send_vtf_file()/
    ymodem_close() in sism555/xmodem.c + sicom.c erzeugen: 128-Byte-SOH-Headerblock (Dateiname
    + Groesse), dann 1024-Byte-STX-CRC-Datenbloecke, EOT, abschliessender Null-Headerblock."""
    stats = {"blocks": 0, "bytes": 0, "filename": None, "filesize": None, "retries": 0}
    t_start = time.perf_counter()
    next_progress = t_start + 10.0   # Fortschrittszeile alle 10 s, unabhaengig von der Uebertragungsrate

    async def read_with_retry():
        last_exc = None
        for attempt in range(retry_limit):
            try:
                return await _read_ymodem_block(transport.rx, block_timeout)
            except Exception as exc:
                last_exc = exc
                stats["retries"] += 1
                log(f"Ymodem-Blockfehler ({exc}) - sende NAK, Versuch {attempt+1}/{retry_limit}.")
                # Ein unvollstaendiger Block kann Restbytes enthalten. Diese duerfen nicht als
                # Beginn der vom Geraet nach NAK wiederholten Uebertragung interpretiert werden.
                transport.rx.clear()
                await transport.write(bytes([XMODEM_NAK]))
        raise last_exc

    try:
        kind, blkno, data = await read_with_retry()
        if kind != "DATA" or blkno != 0:
            raise ValueError(f"Erwartete Ymodem-Headerblock 0, bekam {kind}/{blkno}.")
        name_end = data.find(b"\x00")
        header_str = data[:name_end].decode("ascii", "ignore") if name_end > 0 else ""
        parts = header_str.split(" ") if header_str else []
        stats["filename"] = parts[0] if parts else None
        # Ymodem-Header: "Dateiname\0Groesse ...\0" - die Groesse steht NACH dem ersten Nullbyte
        # (frueher wurde nur der Text vor dem Nullbyte geteilt -> filesize blieb immer None).
        size_token = data[name_end + 1:].split(b"\x00")[0].split(b" ")[0] if name_end > 0 else b""
        if size_token.isdigit():
            stats["filesize"] = int(size_token)
        else:
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
                now = time.perf_counter()
                if now >= next_progress:
                    next_progress = now + 10.0
                    elapsed = now - t_start
                    size = stats["filesize"] or expected_size
                    pct = f", ca. {min(100.0, 100.0 * stats['bytes'] / size):.0f} %" if size else ""
                    log(f"Ymodem: {stats['blocks']} Bloecke / {stats['bytes']} Byte "
                        f"({stats['bytes']/elapsed:.0f} B/s{pct}) ...")
            elif blkno == ((expected - 1) & 0xFF):
                await transport.write(bytes([XMODEM_ACK]))   # Duplikat/erneut gesendeter Block
            else:
                raise ValueError(f"Ausser Sync: erwartet {expected & 0xFF}, erhalten {blkno}.")

        await transport.write(b"C")   # keine weitere Batch-Datei anfordern -> Geraet sendet Nullblock
        kind, blkno, data = await read_with_retry()
        if kind == "DATA" and blkno == 0:
            await transport.write(bytes([XMODEM_ACK]))

        # Gegenprobe gegen die im Header gemeldete Dateigroesse: die Block-CRC erkennt keinen fehlenden
        # Schluss. Ymodem fuellt den letzten Block auf 1024 Byte auf, also gilt filesize <= bytes < filesize+1024.
        if stats["filesize"] is not None:
            stats["size_ok"] = stats["filesize"] <= stats["bytes"] < stats["filesize"] + 1024
            if stats["bytes"] < stats["filesize"]:
                raise ValueError(f"Datei abgeschnitten: {stats['bytes']} Byte empfangen, "
                                 f"Header meldet {stats['filesize']}.")
            if not stats["size_ok"]:
                log(f"WARNUNG: empfangen {stats['bytes']} Byte, Header meldet {stats['filesize']} "
                    "(mehr als eine Blockauffuellung Unterschied).")

        stats["duration_s"] = round(time.perf_counter() - t_start, 3)
        stats["throughput_bps"] = round(stats["bytes"] / stats["duration_s"], 1) if stats["duration_s"] else 0
        return stats
    except BaseException:
        try:
            await transport.write(bytes([XMODEM_CAN]) * 3)
            log("Ymodem-Empfang abgebrochen; CAN CAN CAN an Geraet gesendet.")
        except Exception as abort_exc:
            log(f"Ymodem-Abbruch konnte nicht gesendet werden: {abort_exc}")
        raise


async def run_download_test(args) -> int:
    result = {
        "mode": "download",
        "transport": args.transport,
        "port": None,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "requested_records": args.records,
        "generation_feedback": None,
        "generation_counter_ok": None,
        "ymodem": None,
        "result": "incomplete",
    }
    transport = await make_transport(args)

    async def download_step(marker: str | None, payload: bytes, description: str) -> bool:
        if marker is not None:
            if not await transport.rx.wait_for_marker(marker, args.menu_timeout):
                log(f"Navigation: Marker '{marker}' nicht gesehen: {bytes(transport.rx.buf)!r}")
                return False
            transport.rx.clear()
        should_pause = (args.transport == "classic" and args.classic_step
                        and (not args.download_count_step or "Anzahl Test-Messwerte" in description))
        if should_pause:
            shown = payload.decode("ascii", "backslashreplace")
            await transport._step(
                f"{description}; sende danach {shown!r}. RS232 beobachten.", force=True)
        await send_and_settle(transport, payload)
        return True

    async def send_record_count() -> bool:
        if not await transport.rx.wait_for_marker(MARK_ANZMW, args.menu_timeout):
            log(f"Navigation: Marker '{MARK_ANZMW}' nicht gesehen: {bytes(transport.rx.buf)!r}")
            return False
        transport.rx.clear()
        count_text = str(args.records)
        if args.transport == "classic" and args.classic_step:
            await transport._step(
                f"Anzahl Test-Messwerte bestaetigen; sende danach {count_text!r} ziffernweise und LF. "
                "RS232 nicht bedienen.", force=True)
        for digit in count_text:
            await transport.write(digit.encode("ascii"))
            if not await transport.rx.wait_for_marker(digit, args.menu_timeout):
                log(f"Kein Echo fuer Ziffer {digit!r}: {bytes(transport.rx.buf)!r}")
                return False
            transport.rx.clear()
            await asyncio.sleep(0.15)
        await transport.write(b"\n")
        return True

    try:
        await transport.connect(args.connect_timeout)
        result["port"] = getattr(transport, "port", None)
        log("Verbunden.")
        await authenticate_or_sync_main_menu(transport, args, "Download-Navigation")

        log("Loesche vorhandene Messdaten (Hauptmenue -> 1 -> n -> j) ...")
        transport.rx.clear()
        await wait_for_menu_step(args, "Hauptmenue sichtbar. Sende '1' fuer Datenausgabe.")
        await transport.write(b"1")
        if not await transport.rx.wait_for_marker("Anzahl Messwerte", args.menu_timeout):
            raise ConnectionError(f"Messwertanzahl nicht gesehen: {bytes(transport.rx.buf)!r}")
        count_feedback = bytes(transport.rx.buf)
        count_match = re.search(rb"Anzahl Messwerte\s*=\s*(\d+)", count_feedback)
        if count_match:
            log(f"Vorhandene Messwerte: {int(count_match.group(1))}.")
        if not await transport.rx.wait_for_marker(MARK_DAUS, args.menu_timeout):
            raise ConnectionError(f"'Daten ausgeben?'-Prompt nicht gesehen: {bytes(transport.rx.buf)!r}")
        transport.rx.clear()
        await wait_for_menu_step(args, "'Daten ausgeben?' sichtbar. Sende 'n'.")
        await transport.write(b"n")
        if not await transport.rx.wait_for_marker(MARK_DELETE_DATA, args.menu_timeout):
            raise ConnectionError(f"'Daten loeschen?'-Prompt nicht gesehen: {bytes(transport.rx.buf)!r}")
        transport.rx.clear()
        await wait_for_menu_step(args, "'Daten loeschen?' sichtbar. Sende 'y'.")
        await transport.write(b"y")
        if not await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
            raise ConnectionError(f"Hauptmenue nach Datenloeschung nicht gesehen: {bytes(transport.rx.buf)!r}")
        log("Vorhandene Messdaten geloescht; Hauptmenue erneut erreicht.")

        log("Pruefe Messwertanzahl nach Datenloeschung ...")
        transport.rx.clear()
        await wait_for_menu_step(args, "Hauptmenue nach Loeschung sichtbar. Sende '1' zur Kontrolle.")
        await transport.write(b"1")
        if not await transport.rx.wait_for_marker("Anzahl Messwerte", args.menu_timeout):
            raise ConnectionError(f"Messwertanzahl nach Datenloeschung nicht gesehen: {bytes(transport.rx.buf)!r}")
        count_feedback = bytes(transport.rx.buf)
        count_match = re.search(rb"Anzahl Messwerte\s*=\s*(\d+)", count_feedback)
        if count_match is None:
            raise ConnectionError(f"Messwertanzahl nach Datenloeschung nicht lesbar: {count_feedback!r}")
        remaining_records = int(count_match.group(1))
        if remaining_records != 0:
            raise ConnectionError(f"Datenloeschung nicht bestaetigt: {remaining_records} Messwerte noch vorhanden.")
        log("Messwertanzahl nach Datenloeschung: 0.")
        if not await transport.rx.wait_for_marker(MARK_DAUS, args.menu_timeout):
            raise ConnectionError(f"'Daten ausgeben?'-Prompt nach Datenloeschung nicht gesehen: {bytes(transport.rx.buf)!r}")
        transport.rx.clear()
        await transport.write(b"n")
        if not await transport.rx.wait_for_marker(MARK_DELETE_DATA, args.menu_timeout):
            raise ConnectionError(f"'Daten loeschen?'-Prompt nach Datenloeschung nicht gesehen: {bytes(transport.rx.buf)!r}")
        transport.rx.clear()
        await transport.write(b"n")
        if not await transport.rx.wait_for_marker(MARK_HAUPTMENU, args.menu_timeout):
            raise ConnectionError(f"Hauptmenue nach Datenloesch-Pruefung nicht gesehen: {bytes(transport.rx.buf)!r}")

        log(f"Erzeuge {args.records} Test-Messwerte im Flash "
            "(Testmenue -> Werksmenue -> Bauteiltests -> Write flash data) ...")
        steps_generate = [
            (None, b"H", "Hauptmenue erneut ausgeben"),
            (MARK_HAUPTMENU, b"3", "Testmenue waehlen"),
            (MARK_TESTMENU, bytes([0x17]), "Werksmenue oeffnen"),
            (MARK_WERKMENUE, b"6", "Bauteiltests oeffnen"),
            (MARK_COMPMENU, b"1", "Flash-Testdaten schreiben waehlen"),
            (MARK_SELECT_DATA, b"1", "Messdaten auswaehlen"),
        ]
        for marker, payload, description in steps_generate:
            if not await download_step(marker, payload, description):
                result["result"] = "generate_failed"
                _finish_download(result)
                return 2
        if not await send_record_count():
            result["result"] = "generate_failed"
            _finish_download(result)
            return 2

        # write_flash() kehrt nach md_simulation() zu compmenu() zurueck, das sein Menue
        # (T_comp, Marker "Write flash data") erneut ausgibt - das ist unser fertig-Signal,
        # unabhaengig davon wie lange md_simulation() fuer --records Saetze braucht.
        generate_timeout = args.generation_timeout or max(
            args.menu_timeout, 30.0 + args.records * 0.25)
        transport.rx.clear()
        # Selbst pollen statt wait_for_marker(), damit waehrend des langen Schreibens alle 10 s
        # eine Fortschrittszeile (aktueller Zaehlerstand des Geraets) ins Log kommt.
        progress_needle = MARK_COMPMENU.encode("ascii")
        progress_start = time.monotonic()
        progress_deadline = progress_start + generate_timeout
        next_progress = progress_start + 10.0
        stall_limit_s = 30.0   # Abbruch, wenn das Geraet so lange keinen neuen Zaehlerstand mehr sendet
        last_buf_len = len(transport.rx.buf)
        last_growth = progress_start
        while progress_needle not in transport.rx.buf and time.monotonic() < progress_deadline:
            await asyncio.sleep(0.2)
            if len(transport.rx.buf) != last_buf_len:
                last_buf_len = len(transport.rx.buf)
                last_growth = time.monotonic()
            elif time.monotonic() - last_growth > stall_limit_s:
                log(f"Flash-Erzeugung: seit {stall_limit_s:.0f} s kein Fortschritt - Abbruch.")
                break
            if time.monotonic() >= next_progress:
                next_progress += 10.0
                seen = re.findall(rb"\d+", bytes(transport.rx.buf).replace(b"\x08", b" "))
                last_seen = int(seen[-1]) if seen else 0
                elapsed_s = time.monotonic() - progress_start
                log(f"Flash-Erzeugung laeuft: {last_seen}/{args.records} "
                    f"({100.0 * last_seen / max(args.records, 1):.0f} %), {elapsed_s:.0f} s")
        if progress_needle not in transport.rx.buf:
            log("Kein Fertig-Signal von write_flash() gesehen (Timeout/Stillstand) - Abbruch.")
            result["result"] = "generate_timeout"
            _finish_download(result)
            return 2
        generation_feedback = bytes(transport.rx.buf)
        result["generation_feedback"] = generation_feedback.decode("latin-1", "replace")
        final_counter = re.search(rf"(?<!\d){args.records}(?!\d)", result["generation_feedback"])
        result["generation_counter_ok"] = bool(final_counter)
        # Fortschrittsanzeige des Geraets (Zaehler mit Backspaces) verdichten: volle Antwort bleibt im JSON-Report.
        cleaned_feedback = re.sub(r"\x08+", " ", result["generation_feedback"])
        counters = re.findall(r"\d+", cleaned_feedback)
        if counters:
            log(f"Flash-Erzeugung, Geraeteantwort: {len(generation_feedback)} Bytes, "
                f"{len(counters)} Zaehlerschritte, erster={counters[0]}, letzter={counters[-1]}")
        else:
            log(f"Flash-Erzeugung, Geraeteantwort: {cleaned_feedback.strip()!r}")
        if not result["generation_counter_ok"]:
            log(f"WARNUNG: Zielzaehler {args.records} nicht in der Geraeteantwort gesehen.")
            result["result"] = "generation_counter_missing"
            _finish_download(result)
            return 2
        log(f"{args.records} Test-Messwerte erzeugt.")
        if getattr(args, "write_only", False):
            result["result"] = "passed"
            log("Flash-Schreibtest abgeschlossen; Ymodem-Export ist fuer diesen Schritt deaktiviert.")
            return 0

        # Nach write_flash() bis zu dreimal zum Hauptmenue synchronisieren.
        main_menu_seen = False
        for menu_attempt in range(1, 4):
            transport.rx.clear()
            if not await download_step(None, b"H", f"Hauptmenue ausgeben ({menu_attempt}/3)"):
                raise ConnectionError("Rueckkehr ins Hauptmenue fehlgeschlagen.")
            marker = await transport.rx.wait_for_any_marker(
                (MARK_HAUPTMENU, MARK_MENU_PROMPT), args.menu_timeout)
            if marker == MARK_HAUPTMENU:
                main_menu_seen = True
                break
            if marker == MARK_MENU_PROMPT:
                log(f"Hauptmenue nach H ({menu_attempt}/3) noch nicht erreicht; naechstes H folgt.")
            else:
                log(f"Keine Menueantwort nach H ({menu_attempt}/3); naechstes H folgt.")
        if not main_menu_seen:
            log("Konnte Hauptmenue nach drei H-Befehlen nicht bestaetigen - bitte Geraet pruefen.")
            result["result"] = "return_to_main_failed"
            _finish_download(result)
            return 2

        log("Starte Datenexport (Hauptmenue -> 1 -> Ymodem) ...")
        steps_download = [
            (None, None),  # wir sind schon im Hauptmenue, kein erneutes 'H' noetig
        ]
        transport.rx.clear()
        if not await download_step(None, b"1", "Datenexport waehlen"):
            raise ConnectionError("Datenexport konnte nicht gestartet werden.")
        if not await transport.rx.wait_for_marker(MARK_DAUS, args.menu_timeout):
            log(f"Kein 'Daten ausgeben?'-Prompt gesehen: {bytes(transport.rx.buf)!r}")
            result["result"] = "export_prompt_failed"
            _finish_download(result)
            return 2
        expected_size = estimate_vtf_size(bytes(transport.rx.buf))
        await asyncio.sleep(0.2)
        transport.rx.clear()
        if args.transport == "classic" and args.classic_step:
            await transport._step("Ymodem-Ausgabe waehlen; sende danach 'C'. RS232 beobachten.")
        await transport.write(b"C")   # 'C' = Ymodem-Ausgabe waehlen (send_vtf_file(), sicom.c)
        await asyncio.sleep(0.3)

        log("Empfange Ymodem-Transfer ...")
        stats = await ymodem_receive(transport, block_timeout=args.ymodem_timeout,
                                     expected_size=expected_size)
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


async def run_receive_test(args) -> int:
    """Empfaengt vorhandene Messdaten, ohne den Flash zu beschreiben."""
    result = {
        "mode": "receive",
        "transport": args.transport,
        "port": None,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "requested_records": None,
        "generation_feedback": None,
        "generation_counter_ok": None,
        "ymodem": None,
        "result": "incomplete",
    }
    transport = await make_transport(args)

    try:
        await transport.connect(args.connect_timeout)
        result["port"] = getattr(transport, "port", None)
        log("Verbunden; vorhandene Messdaten werden nur heruntergeladen.")
        await authenticate_or_sync_main_menu(transport, args, "Download-Navigation")

        transport.rx.clear()
        if args.transport == "classic" and args.classic_step:
            await transport._step("Datenexport waehlen; sende danach '1'. RS232 beobachten.")
        await wait_for_menu_step(args, "Hauptmenue sichtbar. Sende '1' fuer Datenausgabe.")
        await transport.write(b"1")
        if not await transport.rx.wait_for_marker(MARK_DAUS, args.menu_timeout):
            raise ConnectionError(f"Kein 'Daten ausgeben?'-Prompt gesehen: {bytes(transport.rx.buf)!r}")
        expected_size = estimate_vtf_size(bytes(transport.rx.buf))

        await asyncio.sleep(0.2)
        transport.rx.clear()
        if args.transport == "classic" and args.classic_step:
            await transport._step("Ymodem-Empfang starten; sende danach 'C'. RS232 beobachten.")
        await wait_for_menu_step(args, "'Daten ausgeben?' sichtbar. Sende 'C' fuer Ymodem.")
        await transport.write(b"C")
        log("Empfange Ymodem-Transfer ...")
        result["ymodem"] = await ymodem_receive(transport, block_timeout=args.ymodem_timeout,
                                                expected_size=expected_size)
        result["result"] = "passed"
        stats = result["ymodem"]
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
        f"- COM-Port: `{result['port']}`" if result["port"] else None,
        f"- Ergebnis: **{result['result']}**",
        f"- Angeforderte Test-Messwerte: `{result['requested_records']}`",
        f"- Zielzaehler in Geraeteantwort gesehen: `{result.get('generation_counter_ok')}`",
    ]
    lines = [line for line in lines if line is not None]
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


async def run_classic_suite(args) -> int:
    """Fuehrt alle nicht-destruktiven BT-Classic-Tests nacheinander aus."""
    result = {
        "mode": "classic-suite",
        "transport": "classic",
        "port": None,
        "started_at": datetime.now().astimezone().isoformat(timespec="milliseconds"),
        "tests": [],
    }
    tests = (
        ("classic-smoke", run_classic_smoke_test),
        ("cycle", run_cycle_test),
        ("receive", run_receive_test),
    )

    for name, test in tests:
        print()
        print("=" * 70)
        print(f"  CLASSIC-SUITE: {name}")
        print("=" * 70)
        exit_code = await test(args)
        result["port"] = getattr(args, "classic_port", None)
        result["tests"].append({"name": name, "result": "passed" if exit_code == 0 else "failed"})

    passed = all(test["result"] == "passed" for test in result["tests"])
    result["result"] = "passed" if passed else "failed"
    print()
    print("=" * 70)
    print("  CLASSIC-SUITE ZUSAMMENFASSUNG")
    print("=" * 70)
    for test in result["tests"]:
        print(f"  {test['name']}: {test['result']}")
    print()

    json_path, md_path = write_report("classic-suite", result)
    lines = [
        "# BT-Classic-Test-Suite",
        "",
        "- Flash-Testdaten werden in dieser Suite nicht erzeugt.",
        f"- COM-Port: `{result['port']}`" if result["port"] else None,
        f"- Ergebnis: **{result['result']}**",
        "",
        "| Test | Ergebnis |",
        "| --- | --- |",
    ]
    lines = [line for line in lines if line is not None]
    lines.extend(f"| {test['name']} | {test['result']} |" for test in result["tests"])
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    log(f"JSON-Report: {json_path}")
    log(f"Markdown-Report: {md_path}")
    return 0 if passed else 1


def _trace_entries() -> list[dict]:
    if TRACE is None or not TRACE.path.exists():
        return []
    entries = []
    for line in TRACE.path.read_text(encoding="utf-8").splitlines():
        try:
            entries.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return entries


def _trace_details(entry: dict) -> str:
    return html.escape(json.dumps(entry.get("details", {}), ensure_ascii=True, default=str))


def _report_time(value: str) -> str:
    try:
        return datetime.fromisoformat(value).strftime("%Y-%m-%d %H:%M:%S")
    except (TypeError, ValueError):
        return value


def render_suite_html(result: dict, title: str, subtitle: str, report_paths: dict[str, str]) -> str:
    phases = result["phases"]
    trace_entries = _trace_entries()
    run_started = next((entry for entry in trace_entries if entry["event"] == "run_started"), {})
    ble_device = next((entry.get("details", {}) for entry in trace_entries
                       if entry["event"] == "ble_device_selected"), {})
    classic_connection = next((entry.get("details", {}) for entry in trace_entries
                               if entry["event"] == "connect_started" and
                               entry.get("transport") == "classic"), {})
    started = run_started.get("timestamp", result.get("started_at", "-"))
    finished = trace_entries[-1].get("timestamp", "noch aktiv") if trace_entries else "-"
    total_seconds = ((trace_entries[-1]["monotonic_ms"] - trace_entries[0]["monotonic_ms"]) / 1000
                     if len(trace_entries) > 1 else 0)
    failed_phases = {phase["name"] for phase in phases if phase["result"] != "passed"}
    diagnostics = [entry for entry in trace_entries
                   if entry.get("level") in ("warning", "error")]
    diagnostic_keys = {(entry.get("phase"), entry.get("cycle")) for entry in diagnostics}
    diagnostic_keys.update((phase, None) for phase in failed_phases)
    timeline_events = {"connect_started", "connect_succeeded", "classic_reader_started", "notify_subscribed",
                       "marker_found", "marker_timeout", "pin_prompt_received",
                       "pin_prompt_missing_fallback", "pin_sent", "disconnect_started",
                       "disconnect_succeeded", "cycle_finished", "phase_finished"}
    timeline = [entry for entry in trace_entries
                if entry.get("event") in timeline_events and
                ((entry.get("phase"), entry.get("cycle")) in diagnostic_keys or
                 entry.get("phase") in failed_phases)]
    max_duration = max((phase.get("duration_s", 0) for phase in phases), default=1) or 1
    phase_rows = []
    cycle_rows = []
    download_rows = []
    smoke_rows = []
    for phase in phases:
        duration = phase.get("duration_s", 0)
        width = duration / max_duration * 100
        phase_rows.append(
            f"<tr><td>{phase['name']}</td><td>{phase['transport']}</td><td>{phase.get('port') or '-'}</td>"
            f"<td class='{phase['result']}'>{phase['result']}</td><td>{duration:.1f} s</td>"
            f"<td><div class='bar'><span style='width:{width:.1f}%'></span></div></td></tr>")
        report = phase.get("report") or {}
        if report.get("mode") == "cycle":
            holds = report.get("hold_seconds", [])
            for cycle in report.get("cycles", []):
                index = cycle.get("index", 0) - 1
                hold = holds[index] if index < len(holds) else "-"
                connect_ms = cycle.get("connect_ms")
                disconnect_ms = cycle.get("disconnect_ms")
                connect_display = f"{connect_ms:.0f}" if connect_ms is not None else "-"
                disconnect_display = f"{disconnect_ms:.0f}" if disconnect_ms is not None else "-"
                cycle_rows.append(
                    f"<tr><td>{phase['transport']}</td><td>{cycle.get('index')}</td><td>{hold}</td>"
                    f"<td>{connect_display}</td><td>{disconnect_display}</td>"
                    f"<td class='{cycle.get('status', 'failed')}'>{cycle.get('status')}</td></tr>")
        if report.get("ymodem"):
            ym = report["ymodem"]
            download_rows.append(
                f"<tr><td>{phase['name']}</td><td>{phase['transport']}</td><td>{ym.get('bytes', 0):,}</td>"
                f"<td>{ym.get('duration_s', 0):.3f}</td><td>{ym.get('throughput_bps', 0):,.0f}</td>"
                f"<td>{ym.get('retries', 0)}</td></tr>")
        if report.get("mode") == "smoke":
            timings = report.get("phases_ms", {})
            smoke_rows.append(
                f"<tr><td>{phase['transport']}</td><td>{timings.get('scan', 0):.0f}</td>"
                f"<td>{timings.get('connect', 0):.0f}</td><td>{timings.get('gatt_discovery', 0):.0f}</td>"
                f"<td>{timings.get('subscribe', 0):.0f}</td></tr>")

    def table(headers: str, rows: list[str]) -> str:
        return f"<table><thead>{headers}</thead><tbody>{''.join(rows) or '<tr><td colspan=6>Keine Daten</td></tr>'}</tbody></table>"

    report_links = " ".join(
        f"<a href='{html.escape(path)}'>{html.escape(label)}</a>"
        for label, path in report_paths.items())
    header_rows = [
        ("Lauf-ID", html.escape(run_started.get("run_id", "-"))),
        ("Start", html.escape(_report_time(started))),
        ("Ende", html.escape(_report_time(finished))),
        ("Gesamtdauer", f"{total_seconds:.1f} s"),
        ("Kommando", html.escape(" ".join(run_started.get("details", {}).get("command_line", [])) or "-")),
        ("BLE", html.escape(" / ".join(str(ble_device.get(key, "-")) for key in ("name", "address", "rssi")))),
        ("Classic", html.escape(" / ".join(str(classic_connection.get(key, "-")) for key in ("device_name", "port")))),
        ("Dateien", report_links),
    ]
    header_html = "".join(f"<tr><th>{label}</th><td>{value}</td></tr>" for label, value in header_rows)
    diagnostic_html = ""
    if diagnostics or failed_phases:
        warning_rows = [
            f"<tr><td>{html.escape(entry.get('timestamp', '-'))}</td><td>{html.escape(str(entry.get('phase', '-')))}</td>"
            f"<td>{html.escape(str(entry.get('cycle', '-')))}</td><td>{html.escape(entry.get('event', '-'))}</td>"
            f"<td><code>{_trace_details(entry)}</code></td></tr>"
            for entry in diagnostics
        ]
        if not warning_rows and failed_phases:
            warning_rows.append("<tr><td colspan=5>Phase fehlgeschlagen; keine Trace-Warnung erfasst.</td></tr>")
        timeline_rows = [
            f"<tr><td>{entry['monotonic_ms']:.1f}</td><td>{html.escape(str(entry.get('phase', '-')))}</td>"
            f"<td>{html.escape(str(entry.get('cycle', '-')))}</td><td>{html.escape(entry['event'])}</td>"
            f"<td><code>{_trace_details(entry)}</code></td></tr>"
            for entry in timeline
        ]
        diagnostic_html = (
            "<h2>Warnungen und Fehler</h2>" + table(
                "<tr><th>Zeit</th><th>Phase</th><th>Zyklus</th><th>Ereignis</th><th>Details</th></tr>", warning_rows) +
            "<h2>Connection-Timeline</h2>" + table(
                "<tr><th>seit Start (ms)</th><th>Phase</th><th>Zyklus</th><th>Ereignis</th><th>Details</th></tr>", timeline_rows))
    return (
        "<!doctype html><meta charset='utf-8'><title>" + title + "</title>"
        "<style>body{font-family:Segoe UI,sans-serif;margin:36px;background:#f3f6f8;color:#17212b}"
        "h1{margin:0}h2{margin-top:34px}table{border-collapse:collapse;background:#fff;width:100%;max-width:1050px}"
        "th,td{padding:10px;border-bottom:1px solid #d9e1e6;text-align:left;vertical-align:top}.passed{color:#137333;font-weight:700}"
        ".failed{color:#b3261e;font-weight:700}.skipped{color:#6b7280;font-weight:700}.bar{height:10px;background:#dfe7ec;width:220px}.bar span{display:block;height:100%;background:#167a9d}"
        "code{white-space:pre-wrap;word-break:break-word}.run-header{max-width:1050px}.run-header th{width:140px}</style>"
        f"<h1>{title}</h1><p>{subtitle}</p><p>Ergebnis: <b class='{result['result']}'>{result['result']}</b></p>"
        "<h2>Laufdetails</h2><table class='run-header'><tbody>" + header_html + "</tbody></table>" +
        "<h2>Phasen und Laufzeiten</h2>" + table(
            "<tr><th>Phase</th><th>Transport</th><th>COM-Port</th><th>Ergebnis</th><th>Dauer</th><th>relativ</th></tr>", phase_rows) + diagnostic_html +
        "<h2>Smoke-Timings (ms)</h2>" + table(
            "<tr><th>Transport</th><th>Scan</th><th>Connect</th><th>GATT</th><th>Subscribe</th></tr>", smoke_rows) +
        "<h2>Reconnects (ms)</h2>" + table(
            "<tr><th>Transport</th><th>Zyklus</th><th>Haltezeit (s)</th><th>Connect</th><th>Disconnect</th><th>Ergebnis</th></tr>", cycle_rows) +
        "<h2>Ymodem-Downloads</h2>" + table(
            "<tr><th>Phase</th><th>Transport</th><th>Bytes</th><th>Dauer (s)</th><th>Durchsatz (B/s)</th><th>Retries</th></tr>", download_rows))


SUITE_SUMMARY_FILES = ("module-suite.json", "fast-suite.json")


def collect_phase_report(phase_name: str, started_wall: float, index: int) -> dict | None:
    """Liest den Bericht, den die Phase geschrieben hat (neueste JSON-Datei, die seit Phasenstart geaendert wurde),
    und sichert ihn zusaetzlich unter eigenem Namen (phase_NN_<Phase>.json).

    write_report() nutzt je Art immer denselben Dateinamen (download.json, cycle.json, ...) und ueberschreibt ihn.
    Die frueher verwendete Mengendifferenz der Dateien fand deshalb nur den jeweils ersten Bericht je Art: alle
    weiteren Downloads/Zyklen/BLE-Phasen hatten 'report: null' im Gesamtbericht."""
    report_dir = RUN_REPORT_DIR or REPORT_DIR
    try:
        fresh = [path for path in report_dir.glob("*.json")
                 if path.name not in SUITE_SUMMARY_FILES and not path.name.startswith("phase_")
                 and path.stat().st_mtime >= started_wall - 1.0]
        if not fresh:
            return None
        newest = max(fresh, key=lambda path: path.stat().st_mtime)
        text = newest.read_text(encoding="utf-8")
        data = json.loads(text)
    except (OSError, json.JSONDecodeError):
        return None
    slug = re.sub(r"[^A-Za-z0-9]+", "_", phase_name).strip("_")
    try:
        (report_dir / f"phase_{index:02d}_{slug}.json").write_text(text, encoding="utf-8")
    except OSError:
        pass
    return data


async def run_module_suite(args) -> int:
    """Kompletter, fortsetzbarer Classic-/BLE-Modultest mit einem Flash-Schreibvorgang."""
    hold_times = [0, 1, 2, 5, 10, 20, 30, 60, 120, 180, 240, 300]
    result = {"mode": "module-suite", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "flash_records": args.module_records, "cycle_holds": hold_times, "phases": []}

    if not await resolve_suite_target(args):
        result["result"] = "device_not_found"
        json_path, _ = write_report("module-suite", result)
        log(f"Gesamtbericht JSON: {json_path}")
        return 2

    async def phase(name, transport, test, **overrides):
        old_values = {key: getattr(args, key, None) for key in overrides}
        started_wall = time.time()
        started = time.perf_counter()
        if TRACE is not None:
            TRACE.set_context(phase=name, transport=transport, cycle=None)
        trace_event("phase_started", test=test.__name__)
        args.transport = transport
        for key, value in overrides.items():
            setattr(args, key, value)
        print(f"\n{'=' * 70}\n  MODUL-SUITE: {name}\n{'=' * 70}")
        try:
            code = await test(args)
            status = "passed" if code == 0 else "failed"
        except Exception as exc:
            log(f"FEHLER in Suite-Phase {name}: {exc}")
            status = "failed"
        finally:
            for key, value in old_values.items():
                setattr(args, key, value)
        report_data = collect_phase_report(name, started_wall, len(result["phases"]) + 1)
        result["phases"].append({"name": name, "transport": transport, "port": (report_data or {}).get("port"), "result": status,
                                 "duration_s": round(time.perf_counter() - started, 3), "report": report_data})
        if TRACE is not None:
            TRACE.set_context(cycle=None)
        trace_event("phase_finished", status=status,
                duration_s=round(time.perf_counter() - started, 3))
        return status

    def skipped(name, transport, reason: str | None = None):
        reason = reason or f"{transport}-Smoke nicht bestanden"
        log(f"Suite-Phase uebersprungen: {name} ({reason}).")
        result["phases"].append({"name": name, "transport": transport, "result": "skipped", "duration_s": 0})

    classic_ok = await phase("Classic Smoke", "classic", run_classic_smoke_test) == "passed"
    flash_ok = False
    if classic_ok:
        await phase("Classic Verbindung", "classic", run_cycle_test, cycle_holds=hold_times)
    else:
        skipped("Classic Verbindung", "classic")
    ble_ok = await phase("BLE Smoke", "ble", run_smoke_test) == "passed"
    if ble_ok:
        await phase("BLE Verbindung", "ble", run_cycle_test, cycle_holds=hold_times)
        flash_ok = await phase("Flash schreiben", "ble", run_download_test,
                               write_only=True, records=args.module_records) == "passed"
    else:
        skipped("BLE Verbindung", "ble")
        skipped("Flash schreiben", "ble")
    # Fallback: Flash ueber Classic schreiben, wenn der BLE-Weg ausgefallen ist, damit die
    # Downloads trotzdem mit echten Daten laufen (BLE-Downloads bleiben dann uebersprungen).
    if not flash_ok and classic_ok:
        log("Flash-Schreiben ueber BLE nicht moeglich - versuche es ueber Classic.")
        flash_ok = await phase("Flash schreiben (Classic)", "classic", run_download_test,
                               write_only=True, records=args.module_records) == "passed"
    # Ohne erfolgreich geschriebene Messwerte waere ein Download nur ein leerer Export
    # (5 KB) und ein scheinbar bestandener, aber wertloser Test - deshalb nur nach Flash-Schreiben.
    no_flash = "Flash-Schreibvorgang nicht bestanden oder nicht ausgefuehrt"
    for index in range(1, 4):
        if classic_ok and flash_ok:
            await phase(f"Classic Download {index}/3", "classic", run_receive_test)
        else:
            skipped(f"Classic Download {index}/3", "classic", no_flash if not flash_ok else None)
    for index in range(1, 4):
        if ble_ok and flash_ok:
            await phase(f"BLE Download {index}/3", "ble", run_receive_test)
        else:
            skipped(f"BLE Download {index}/3", "ble", no_flash if not flash_ok else None)

    result["result"] = "passed" if all(phase["result"] == "passed" for phase in result["phases"]) else "failed"
    json_path, html_path = write_report("module-suite", result)
    html_path = html_path.with_suffix(".html")
    html_path.write_text(render_suite_html(result, "VIASIS Modul-Test",
                         f"Flash-Schreibvorgang: einmalig, {args.module_records} Messwerte. Haltezeiten: {', '.join(map(str, hold_times))} s.",
                         {"JSON": json_path.name, "Trace": TRACE.path.name if TRACE else "-", "HTML": html_path.name}),
                         encoding="utf-8")
    log(f"Gesamtbericht JSON: {json_path}")
    log(f"Gesamtbericht HTML: {html_path}")
    return 0 if result["result"] == "passed" else 1


async def run_fast_suite(args) -> int:
    """Kurzer Vorabtest fuer beide Transporte vor dem mehrstuendigen Modultest."""
    hold_times = [1, 3]
    result = {"mode": "fast-suite", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "flash_records": 100, "cycle_holds": hold_times, "phases": []}

    if not await resolve_suite_target(args):
        result["result"] = "device_not_found"
        json_path, _ = write_report("fast-suite", result)
        log(f"Fast-Suite JSON: {json_path}")
        return 2

    async def phase(name, transport, test, **overrides):
        old_values = {key: getattr(args, key, None) for key in overrides}
        started_wall = time.time()
        started = time.perf_counter()
        if TRACE is not None:
            TRACE.set_context(phase=name, transport=transport, cycle=None)
        trace_event("phase_started", test=test.__name__)
        args.transport = transport
        for key, value in overrides.items():
            setattr(args, key, value)
        print(f"\n{'=' * 70}\n  FAST-SUITE: {name}\n{'=' * 70}")
        try:
            code = await test(args)
            status = "passed" if code == 0 else "failed"
        except Exception as exc:
            log(f"FEHLER in Fast-Suite-Phase {name}: {exc}")
            status = "failed"
        finally:
            for key, value in old_values.items():
                setattr(args, key, value)
        report_data = collect_phase_report(name, started_wall, len(result["phases"]) + 1)
        result["phases"].append({"name": name, "transport": transport, "port": (report_data or {}).get("port"), "result": status,
                                 "duration_s": round(time.perf_counter() - started, 3), "report": report_data})
        if TRACE is not None:
            TRACE.set_context(cycle=None)
        trace_event("phase_finished", status=status,
                duration_s=round(time.perf_counter() - started, 3))
        return status

    def skipped(name, transport, reason: str | None = None):
        reason = reason or f"{transport}-Smoke nicht bestanden"
        log(f"Fast-Suite-Phase uebersprungen: {name} ({reason}).")
        result["phases"].append({"name": name, "transport": transport, "result": "skipped", "duration_s": 0})

    classic_ok = await phase("Classic Smoke", "classic", run_classic_smoke_test) == "passed"
    flash_ok = False
    if classic_ok:
        await phase("Classic Verbindung", "classic", run_cycle_test, cycle_holds=hold_times)
    else:
        skipped("Classic Verbindung", "classic")
    ble_ok = await phase("BLE Smoke", "ble", run_smoke_test) == "passed"
    if ble_ok:
        await phase("BLE Verbindung", "ble", run_cycle_test, cycle_holds=hold_times)
        flash_ok = await phase("Flash schreiben", "ble", run_download_test,
                               write_only=True, records=100) == "passed"
        if flash_ok:
            await phase("BLE Download", "ble", run_receive_test)
        else:
            skipped("BLE Download", "ble", "Flash-Schreibtest nicht bestanden")
    else:
        skipped("BLE Verbindung", "ble")
        skipped("Flash schreiben", "ble")
        skipped("BLE Download", "ble")
    if classic_ok and flash_ok:
        await phase("Classic Download", "classic", run_receive_test)
    else:
        skipped("Classic Download", "classic",
                "Flash-Schreibtest nicht bestanden" if not flash_ok else None)
    if ble_ok:
        if flash_ok:
            await phase("BLE Download", "ble", run_receive_test)
        else:
            skipped("BLE Download", "ble", "Flash-Schreibtest nicht bestanden")
    else:
        skipped("BLE Download", "ble")

    result["result"] = "passed" if all(phase["result"] == "passed" for phase in result["phases"]) else "failed"
    json_path, md_path = write_report("fast-suite", result)
    lines = ["# BT-Fast-Suite", "", f"- Ergebnis: **{result['result']}**", "",
             "| Phase | Transport | COM-Port | Ergebnis |", "| --- | --- | --- | --- |"]
    lines.extend(f"| {phase['name']} | {phase['transport']} | {phase.get('port') or '-'} | {phase['result']} |" for phase in result["phases"])
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    html_path = md_path.with_suffix(".html")
    html_path.write_text(render_suite_html(result, "VIASIS BT-Fast-Suite",
                         f"Flash-Schreibvorgang: einmalig, 100 Messwerte. Haltezeiten: {', '.join(map(str, hold_times))} s.",
                         {"JSON": json_path.name, "Trace": TRACE.path.name if TRACE else "-", "HTML": html_path.name}),
                         encoding="utf-8")
    log(f"Fast-Suite JSON: {json_path}")
    log(f"Fast-Suite Markdown: {md_path}")
    log(f"Fast-Suite HTML: {html_path}")
    return 0 if result["result"] == "passed" else 1


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    global TRACE
    p = argparse.ArgumentParser(description="BLE/BT-Classic-Test fuer das VIASIS-Mainboard.")
    p.add_argument("--mode", choices=("smoke", "transfer", "classic-smoke", "cycle", "download", "receive",
                                      "classic-suite", "module-suite", "fast-suite", "idle-latency"),
                   default="smoke")
    p.add_argument("--transport", choices=("ble", "classic"), default="ble",
                   help="Uebertragungsweg fuer --mode cycle/download/receive (classic-smoke/classic-suite/smoke/"
                        "transfer legen den Transport implizit fest).")
    p.add_argument("--name", default=DEFAULT_NAME_PREFIX,
                    help=f"VIASIS-Namenspraefix beim Suchen (Default: {DEFAULT_NAME_PREFIX}); "
                        "bei mehreren Geraeten vollstaendigen Namen angeben")
    p.add_argument("--address", default=None, help="[BLE] BT-Adresse direkt angeben, ueberspringt Scan")
    p.add_argument("--port", default=None,
                   help="[Classic] erwarteter SPP-COM-Port zur zusaetzlichen Eingrenzung; optional")
    p.add_argument("--classic-step", action="store_true",
                   help="[Classic] nach Suche und vor/nach COM-Port-Open auf Enter warten")
    p.add_argument("--menu-step", action="store_true",
                   help="[download] Loesch- und Kontrollbefehle einzeln erst nach Enter senden (BLE und Classic)")
    p.add_argument("--classic-trace", action="store_true",
                   help="[Classic] jeden COM-TX/RX-Chunk mit Laenge, Hex und ASCII protokollieren")
    p.add_argument("--baud", type=int, default=115200,
                   help="[Classic] Baudrate fuer den COM-Port (virtueller SPP-Port - Wert meist "
                        "ohne Wirkung, aber von pyserial verlangt)")
    p.add_argument("--pin", default=None,
                   help="BT-PIN (6 Ziffern) nach dem Verbinden senden - ohne das bleibt die "
                        "Firmware-Pinabfrage offen. Ohne Angabe wird versucht, sie aus einem vollstaendigen "
                        "VIASIS_<serno>-Namen abzuleiten (erste 2 + letzte 4 Ziffern).")
    p.add_argument("--connect-timeout", type=float, default=30.0)
    p.add_argument("--menu-timeout", type=float, default=8.0,
                   help="Sekunden, die auf einen erwarteten Menuetext-Marker gewartet wird")
    p.add_argument("--hold", type=float, default=8.0,
                   help="[smoke] Sekunden verbunden bleiben, um Spontan-Disconnect zu pruefen. "
                        "[cycle] Haltezeit je Zyklus (0 = sofort wieder trennen).")
    p.add_argument("--cycle-holds", type=lambda value: [float(item) for item in value.split(",")], default=None,
                   help="[cycle] komma-getrennte Haltezeiten je Zyklus in Sekunden")
    p.add_argument("--idle-times", type=lambda value: [float(item) for item in value.split(",")], default=None,
                   help="[idle-latency] komma-getrennte Leerlaufzeiten in Sekunden, je einmal frisch "
                        "verbunden getestet (Default: 20,55,65,90,125 - rund um die vermutete "
                        "60s-Minutentick-Grenze)")
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
    p.add_argument("--write-only", action="store_true",
                   help="[download] nach dem Flash-Schreiben beenden, keinen Ymodem-Export starten")
    p.add_argument("--module-records", type=int, default=419430,
                   help="[module-suite] Messwerte fuer rund 4 MiB Flash-Nutzdaten (Default 419430)")
    p.add_argument("--download-count-step", action="store_true",
                   help="[Classic download] mit --classic-step nur vor der Anzahl-Eingabe anhalten")
    p.add_argument("--generation-timeout", type=float, default=None,
                   help="[download] maximaler Timeout fuer das synchrone Schreiben der Testdaten; "
                        "Default: 30 s plus 0,25 s je Messwert")
    p.add_argument("--rs232-port", default="COM3",
                   help="RS232-Port der Firmware fuer den passiven Mitschnitt (Default COM3)")
    p.add_argument("--rs232-baud", type=int, default=115200)
    p.add_argument("--no-rs232", action="store_true", help="keinen RS232-Mitschnitt starten")
    p.add_argument("--ymodem-timeout", type=float, default=2.0,
                   help="[download/receive] Timeout je Ymodem-Block in Sekunden; muss unter dem "
                        "Firmware-ACK-Timeout von 6 s liegen")
    args = p.parse_args()

    if args.mode in ("smoke", "transfer"):
        args.transport = "ble"
    elif args.mode in ("classic-smoke", "classic-suite", "idle-latency"):
        args.transport = "classic"
    if args.transport == "ble" and BleakClient is None:
        sys.exit("Fehlt: bleak. Installieren mit:  pip install bleak")

    TRACE = TraceLogger(args)

    print("=" * 70)
    print("  BLE/BT-Classic-Mainboard-Test - VIASIS / sism555 (IF820)")
    print(f"  Modus: {args.mode}   Transport: {args.transport}   "
          f"Start: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)
    log(f"Strukturiertes Trace: {TRACE.path}")

    if not setup_rs232_sniffer(args):
        log("Abbruch durch Tester (RS232).")
        return 2

    if args.mode == "smoke":
        return asyncio.run(run_smoke_test(args))
    if args.mode == "transfer":
        return asyncio.run(run_transfer_test(args))
    if args.mode == "classic-smoke":
        return asyncio.run(run_classic_smoke_test(args))
    if args.mode == "classic-suite":
        args.transport = "classic"
        return asyncio.run(run_classic_suite(args))
    if args.mode == "module-suite":
        return asyncio.run(run_module_suite(args))
    if args.mode == "fast-suite":
        return asyncio.run(run_fast_suite(args))
    if args.mode == "cycle":
        return asyncio.run(run_cycle_test(args))
    if args.mode == "idle-latency":
        return asyncio.run(run_idle_latency_test(args))
    if args.mode == "receive":
        return asyncio.run(run_receive_test(args))
    return asyncio.run(run_download_test(args))


if __name__ == "__main__":
    exit_code = 1
    try:
        exit_code = main()
        raise SystemExit(exit_code)
    except KeyboardInterrupt:
        print("\nAbgebrochen.")
        raise SystemExit(130)
    finally:
        if SNIFFER is not None:
            SNIFFER.stop()
        if TRACE is not None:
            TRACE.close(exit_code)
