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
import shutil
import signal
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
BLE_INFO: dict = {}          # z. B. rssi des gewaehlten BLE-Geraets

# Langsam-Erkennung bei Downloads (2026-10-19): Nach 4 Tagen Geraetelaufzeit / frisch gestartetem PC lief der BLE-Download mit
# konstant 3,1 statt 6,6 KB/s. Schwellen je Transport (B/s); --min-bps ueberschreibt, 0 schaltet aus.
SLOW_DEFAULT_BPS = {"ble": 4500.0, "classic": 5500.0}
SLOW_MIN_BYTES = 4096        # darunter ist die Rate wegen Anlauf/Overhead nicht aussagekraeftig
SLOW_EARLY_MIN_SIZE = 300_000   # Fruehabbruch nur bei grossen Transfers (kleine sind in < 60 s fertig)


class SlowLinkError(Exception):
    """Der Ymodem-Durchsatz liegt nach der Pruefzeit unter der Schwelle (Link/PC/Modul in schlechtem Zustand)."""


class LinkDropSimulated(Exception):
    """--drop-after-blocks: der PC hat die Verbindung mitten im Ymodem-Empfang absichtlich hart getrennt (ohne CAN)."""


DROP_AFTER_BLOCKS = 0   # --drop-after-blocks: 0 = aus


def slow_threshold(args) -> float:
    """Schwelle in B/s fuer den aktuellen Transport; 0 = Pruefung aus."""
    if getattr(args, "min_bps", None) is not None:
        return float(args.min_bps)
    return SLOW_DEFAULT_BPS.get(args.transport, 0.0)
RS232_PREFLIGHT_LISTEN_S = 5.0     # Gesamtzeit der Vorpruefung (davon die ersten 2 s passiv, dann 1x "H" senden)
RS232_PREFLIGHT_PASSIVE_S = 2.0
RS232_PARTIAL_FLUSH_S = 0.3


class Rs232Sniffer:
    """Liest passiv den RS232-Ausgang der Firmware und schreibt jede Zeile in den Trace.

    Nur lesend: es wird nie etwas an die Firmware gesendet, der Menuezustand bleibt unberuehrt.
    """

    def __init__(self, port: str, baud: int, label: str | None = None):
        self.port = port
        self.baud = baud
        self.label = label            # z. B. "A"/"B" bei zwei Geraeten; erscheint als dev= in den Trace-Zeilen
        self.rx = None                # optional: ByteStream, der die gelesenen Bytes mitbekommt (Geraeteidentifikation)
        self.loop = None
        self.ser = None
        self.thread = None
        self.stop_event = threading.Event()
        self.pending = bytearray()    # im Preflight gelesene Bytes, gehen nicht verloren
        self.recent = []              # letzte Zeilen fuer rs232_tail
        self.capture = None           # optional: Liste (monotonic, Zeile) fuer die Strom-/Neustart-Tests
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

    def attach(self, loop):
        """Gelesene Bytes zusaetzlich an einen ByteStream geben (fuer die Menue-Navigation, z. B. Geraeteinfo)."""
        self.loop = loop
        self.rx = ByteStream("rs232" if not self.label else f"rs232-{self.label}")
        return self.rx

    def detach(self) -> None:
        self.rx = None

    def write(self, data: bytes) -> None:
        """Aktives Senden an die Firmware (nur fuer die Geraeteidentifikation; der Mitschnitt selbst bleibt rein lesend)."""
        self.ser.write(data)
        self.ser.flush()

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
            if self.capture is not None:
                self.capture.append((time.monotonic(), text))
        extra = {"dev": self.label} if self.label else {}
        trace_event("rs232_line", source="rs232", text=text, partial=partial or None, **extra)

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
                    stream = self.rx
                    if stream is not None and self.loop is not None:   # Menue-Navigation (Geraeteinfo) mithoeren lassen
                        self.loop.call_soon_threadsafe(stream.feed, bytes(chunk))
                    buf.extend(chunk)
                    while (idx := buf.find(b"\n")) >= 0:
                        self._emit(bytes(buf[:idx]), False)
                        del buf[:idx + 1]
                elif buf and now - last_rx > RS232_PARTIAL_FLUSH_S:
                    self._emit(bytes(buf), True)  # z. B. Prompt ohne Zeilenende ("PIN = ")
                    buf.clear()
        except (serial.SerialException, OSError) as exc:
            trace_event("rs232_error", level="error", error=str(exc))
            env_disturbance("rs232_lost", f"RS232-/USB-Port {self.port} verloren: {exc}")
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
            log(f"RS232 {args.rs232_port} @ {args.rs232_baud} offen, warte auf Daten ...")
            received = sniffer.preflight(RS232_PREFLIGHT_PASSIVE_S)
            if not received:
                # Eine ruhige Firmware schweigt von selbst (Menue steht, nichts passiert) - das ist kein Fehler.
                # Deshalb einmal "H" (Hauptmenue ausgeben, harmlos) senden und auf die Antwort warten.
                try:
                    sniffer.write(b"H")
                except (serial.SerialException, OSError):
                    pass
                received = sniffer.preflight(RS232_PREFLIGHT_LISTEN_S - RS232_PREFLIGHT_PASSIVE_S)
            if received:
                log(f"RS232-Mitschnitt aktiv ({received} Bytes empfangen).")
                trace_event("rs232_sniffer_started", port=args.rs232_port, baud=args.rs232_baud,
                            preflight_bytes=received)
                sniffer.start()
                SNIFFER = sniffer
                register_port(args.rs232_port, "RS232-/USB-Terminal: Mitschnitt der Firmware-Ausgabe (Geraet A)")
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
            register_port(args.rs232_port, "RS232-/USB-Terminal: Mitschnitt der Firmware-Ausgabe (Geraet A)")
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
    sniff_target_known(device.address)   # --sniff: Funk-Mitschnitt jetzt auf dieses Geraet starten
    BLE_INFO["rssi"] = rssi   # fuer die Langsam-Warnung der Downloads
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
                                # Windows liefert stLastSeen manchmal in Ortszeit statt UTC (2026-11-09: -3595 s,
                                # Geraet aber sichtbar) -> auch das Alter gegen die Ortszeit gelten lassen
                                age_local = (datetime.now() - seen_at).total_seconds()
                                if not -1.0 <= age_seconds <= CLASSIC_VISIBILITY_MAX_AGE_SECONDS and \
                                        -1.0 <= age_local <= CLASSIC_VISIBILITY_MAX_AGE_SECONDS:
                                    age_seconds = age_local
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
    if DEVICE_INFO and kind in ("module-suite", "fast-suite", "interleave", "soak", "device-info"):
        data = {**data, "device_info": DEVICE_INFO, "ports_used": dict(USED_PORTS)}   # Geraete und vom Test benutzte Ports
    json_path.write_text(__import__("json").dumps(data, indent=2, default=str), encoding="utf-8")
    return json_path, base.with_suffix(".md")


# ---------------------------------------------------------------------------
# Modus 1: Smoke-Test mit Debug-Timings (BLE)
# ---------------------------------------------------------------------------

SMOKE_LAST: dict = {}          # Ergebnis des letzten Smoke-Versuchs (fuer die Wiederholung bei bekanntem PC-Verhalten)
SMOKE_SLOW_CONNECT_MS = 2000.0  # Verbindungsaufbau darueber: Windows fragt die GATT-Dienste neu ab, danach trennt das Geraet oft


async def run_smoke_test(args) -> int:
    """Smoke-Test mit einer Wiederholung: Scheitert der erste Versuch ohne PIN-Prompt nach langsamem Verbindungsaufbau
    (> 2 s), ist das das bekannte intermittierende PC-/Windows-BLE-Verhalten (Messung 2026-10-20, project-status.md,
    Diskriminierungstest) - dann einmal neu verbinden. Scheitert auch der zweite Versuch, gilt es als echter Fehler."""
    code = await _run_smoke_once(args)
    first = dict(SMOKE_LAST)
    known_pc_issue = (code != 0 and first.get("result") == "unstable" and not first.get("pin_prompt_seen")
                      and first.get("phases_ms", {}).get("connect", 0) > SMOKE_SLOW_CONNECT_MS)
    if not known_pc_issue:
        return code
    log(f"Bekanntes PC-BLE-Verhalten (Verbindungsaufbau {first['phases_ms']['connect']:.0f} ms, kein PIN-Prompt, "
        f"Geraet trennt) - wiederhole den Smoke-Test einmal.")
    await asyncio.sleep(5.0)
    code = await _run_smoke_once(args)
    SMOKE_LAST["retry_of_pc_issue"] = True
    log("Wiederholung: " + ("bestanden - Erstversuch war das bekannte PC-Verhalten." if code == 0
                            else "ebenfalls fehlgeschlagen - echter Fehler."))
    return code


async def _run_smoke_once(args) -> int:
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
    SMOKE_LAST.clear()
    SMOKE_LAST.update(result)
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
        check_target_address(address, "BLE")
        register_port(f"BLE {address}", f"Bluetooth-LE-Verbindung zu {self.device_name or '?'}")
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
            check_target_address(address, "Classic")
            self.args.classic_port = await asyncio.to_thread(find_classic_port, address, self.args.port)
            log(f"SPP-Port fuer {self.args.classic_device_name}: {self.args.classic_port}.")
            await self._step("SPP-COM-Port abgefragt, aber noch NICHT geoeffnet. RS232 beobachten.")
        self.device_name = self.args.classic_device_name
        self.port = self.args.classic_port
        register_port(self.port, f"Bluetooth-Classic-Verbindung (SPP) zu {self.device_name}")
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
                         expected_size: int | None = None, min_bps: float = 0.0,
                         slow_check_s: float = 60.0) -> dict:
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
                if DROP_AFTER_BLOCKS and stats["blocks"] >= DROP_AFTER_BLOCKS:
                    log(f"--drop-after-blocks: trenne nach {stats['blocks']} Bloecken hart (kein ACK, kein CAN).")
                    trace_event("link_drop_simulated", blocks=stats["blocks"], bytes=stats["bytes"])
                    await transport.disconnect()
                    raise LinkDropSimulated(f"Verbindung nach {stats['blocks']} Bloecken absichtlich getrennt.")
                await transport.write(bytes([XMODEM_ACK]))
                now = time.perf_counter()
                if now >= next_progress:
                    next_progress = now + 10.0
                    elapsed = now - t_start
                    size = stats["filesize"] or expected_size
                    pct = f", ca. {min(100.0, 100.0 * stats['bytes'] / size):.0f} %" if size else ""
                    log(f"Ymodem: {stats['blocks']} Bloecke / {stats['bytes']} Byte "
                        f"({stats['bytes']/elapsed:.0f} B/s{pct}) ...")
                    # Fruehe Langsam-Erkennung: bei grossen Transfers nicht 10-20 Minuten abwarten
                    if (min_bps and elapsed >= slow_check_s and size and size >= SLOW_EARLY_MIN_SIZE
                            and stats["bytes"] / elapsed < min_bps):
                        raise SlowLinkError(
                            f"Durchsatz {stats['bytes']/elapsed:.0f} B/s nach {elapsed:.0f} s liegt unter der "
                            f"Schwelle {min_bps:.0f} B/s.")
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
    except BaseException as exc:
        if isinstance(exc, LinkDropSimulated):
            raise
        try:
            await transport.write(bytes([XMODEM_CAN]) * 3)
            log("Ymodem-Empfang abgebrochen; CAN CAN CAN an Geraet gesendet.")
        except Exception as abort_exc:
            log(f"Ymodem-Abbruch konnte nicht gesendet werden: {abort_exc}")
        raise


async def _after_link_drop(args, exc) -> bool:
    """--drop-after-blocks: RS232 noch 25 s mitschneiden (Abbruch auf der Firmware-Seite), dann BLE-Sichtbarkeit pruefen."""
    log(f"{exc} RS232 wird noch 25 s mitgeschnitten, dann BLE-Sichtbarkeit.")
    await asyncio.sleep(25.0)
    found = await BleakScanner.find_device_by_address(args.address, timeout=10.0) if args.address else None
    log(f"Geraet nach dem Abbruch {'wieder sichtbar' if found else 'NICHT sichtbar'}.")
    return found is not None


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
    except LinkDropSimulated as exc:
        result["result"] = "link_drop_simulated"
        result["error"] = str(exc)
        result["visible_after_drop"] = await _after_link_drop(args, exc)
    except Exception as exc:
        result["result"] = "failed"
        result["error"] = str(exc)
        log(f"FEHLER: {exc}")
    finally:
        await transport.disconnect()

    _finish_download(result)
    return 0 if result["result"] == "passed" else 1


async def _receive_once(args, min_bps: float) -> tuple[int, dict]:
    """Ein Download-Versuch (vorhandene Messdaten, Flash wird nicht beschrieben). min_bps > 0: Fruehabbruch bei
    zu langsamem Transfer (result['result'] == 'slow_link')."""
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
                                                expected_size=expected_size, min_bps=min_bps,
                                                slow_check_s=args.slow_check_s)
        result["result"] = "passed"
        stats = result["ymodem"]
        log(f"Download abgeschlossen: {stats['bytes']} Byte in {stats['blocks']} Bloecken, "
            f"{stats['duration_s']} s, {stats['throughput_bps']:.0f} B/s "
            f"({stats['retries']} Retries).")
        threshold = slow_threshold(args)
        stats["slow"] = bool(threshold and stats["bytes"] >= SLOW_MIN_BYTES
                             and stats["throughput_bps"] < threshold)
        stats["slow_threshold_bps"] = threshold or None
        if stats["slow"]:
            log(f"WARNUNG: langsamer Link - {stats['throughput_bps']:.0f} B/s liegt unter der Schwelle "
                f"{threshold:.0f} B/s (RSSI {BLE_INFO.get('rssi')}).")
            trace_event("slow_link", level="warning", transport=args.transport,
                        throughput_bps=stats["throughput_bps"], threshold_bps=threshold,
                        rssi=BLE_INFO.get("rssi"), aborted=False)
    except SlowLinkError as exc:
        result["result"] = "slow_link"
        result["error"] = str(exc)
        log(f"LANGSAMER LINK: {exc}")
        trace_event("slow_link", level="warning", transport=args.transport, reason=str(exc),
                    threshold_bps=min_bps, rssi=BLE_INFO.get("rssi"), aborted=True)
    except LinkDropSimulated as exc:
        result["result"] = "link_drop_simulated"
        result["error"] = str(exc)
        result["visible_after_drop"] = await _after_link_drop(args, exc)
    except Exception as exc:
        result["result"] = "failed"
        result["error"] = str(exc)
        log(f"FEHLER: {exc}")
    finally:
        await transport.disconnect()

    _finish_download(result)
    return (0 if result["result"] == "passed" else 1), result


async def run_receive_test(args) -> int:
    """Empfaengt vorhandene Messdaten. Erkennt einen zu langsamen Link frueh (nach --slow-check-s), wartet
    --slow-wait-s und wiederholt bis zu --slow-retries Mal. Der letzte Versuch laeuft ohne Fruehabbruch zu Ende und
    wird bei zu niedriger Rate nur als 'langsam' markiert (Ergebnis bleibt passed)."""
    threshold = slow_threshold(args)
    retries = max(0, int(args.slow_retries)) if threshold else 0
    for attempt in range(1, retries + 2):
        last = attempt == retries + 1
        code, result = await _receive_once(args, 0.0 if last else threshold)
        if result["result"] != "slow_link":
            return code
        log(f"Link zu langsam (Versuch {attempt}/{retries + 1}) - warte {args.slow_wait_s:.0f} s und "
            "wiederhole. Hinweis: der Abbruch erzeugt am Geraet einen Protokolleintrag (F13010) und verbraucht "
            "eine Dateinummer.")
        await asyncio.sleep(args.slow_wait_s)
    return 1


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
                f"<td>{ym.get('retries', 0)}</td>"
                + ("<td class='failed'>langsam</td>" if ym.get("slow") else "<td class='passed'>ok</td>")
                + "</tr>")
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
    if USED_PORTS:                                      # welche Ports hat der Test wofuer benutzt
        header_rows.insert(-1, ("Verwendete Ports", "<br>".join(
            html.escape(f"{port}: {purpose}") for port, purpose in USED_PORTS.items())))
    for dev_label, dev_info in DEVICE_INFO.items():     # Geraeteidentifikation (Firmware, Seriennummer, ...)
        parts = [f"Seriennr. {dev_info.get('seriennummer', '?')}", f"Firmware {dev_info.get('firmware', '?')}",
                 f"HW {dev_info.get('hardware', '?')}"]
        if dev_info.get("betriebsstunden") is not None:
            parts.append(f"{dev_info['betriebsstunden']} h Betrieb")
        if dev_info.get("datum"):
            parts.append(f"Geraeteuhr {dev_info['datum']} {dev_info.get('zeit', '')}"
                         f"{' (Abweichung ' + str(dev_info['uhr_abweichung_s']) + ' s)' if 'uhr_abweichung_s' in dev_info else ''}")
        if dev_info.get("schnittstellen"):
            parts.append(", ".join(dev_info["schnittstellen"]))
        header_rows.insert(-1, (f"Geraet {html.escape(str(dev_label))}", html.escape("; ".join(parts))))
        bt_mod = dev_info.get("bt_modul") or {}
        if bt_mod and "nicht_zugaenglich" not in bt_mod:
            header_rows.insert(-1, (f"BT-Modul {html.escape(str(dev_label))}", html.escape(
                f"{bt_mod.get('modul', '?')} FW {bt_mod.get('modul_firmware', '?')} (Stack {bt_mod.get('modul_stack', '?')}); "
                f"Adresse {bt_mod.get('adresse', '?')}; {bt_mod.get('name_ble', '?')} / {bt_mod.get('name_classic', '?')}; "
                f"{bt_mod.get('baudrate', '?')} Baud")))
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
        ".failed{color:#b3261e;font-weight:700}.skipped{color:#6b7280;font-weight:700}.environment{color:#b26a00;font-weight:700}.bar{height:10px;background:#dfe7ec;width:220px}.bar span{display:block;height:100%;background:#167a9d}"
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
            "<tr><th>Phase</th><th>Transport</th><th>Bytes</th><th>Dauer (s)</th><th>Durchsatz (B/s)</th><th>Retries</th><th>Link</th></tr>", download_rows))


SUITE_SUMMARY_FILES = ("module-suite.json", "fast-suite.json")


# ---------------------------------------------------------------------------
# PC-Seite mitprotokollieren (2026-10-19): Damit Fehler dem PC oder Geraet+Modul zugeordnet werden koennen.
# - PC wachhalten (SetThreadExecutionState: kein Standby/Ruhezustand waehrend des Tests)
# - PC-Laufzeit und Stromquelle ins Trace (Warnung bei frisch gestartetem PC)
# - am Ende Windows-Systemereignisse des Testzeitraums (Kernel-Power: Standby/Netzwechsel, Bluetooth/USB/PnP)
#   als pc_events.json in den Report-Ordner; Standby-Phasen und Bluetooth-Meldungen als Trace-Warnungen.
# ---------------------------------------------------------------------------

PC_STATE: dict = {"start_wall": None, "exported": False, "awake": False}
PC_EVENT_PROVIDERS = r"Kernel-Power|bth|bluetooth|usb|Kernel-PnP|Kernel-Boot|Kernel-Processor-Power"
PC_FRESH_BOOT_S = 900.0     # PC juenger als 15 Minuten: Wartung/Treiberstart laeuft noch -> Warnung
PC_SUSPEND_GAP_S = 8.0      # Zeitsprung der Uhr-Pruefung (1 s Takt) groesser: der PC stand (Standby/Ruhezustand/Einfrieren)
ENV_EVENTS: list = []       # Stoerungen der Testumgebung (nicht des Geraets): (Wanduhr, Art, Text)


def env_disturbance(kind: str, text: str) -> None:
    """Stoerung der Testumgebung merken (Standby, verlorener COM-Port, ...). Eine Phase, die waehrend einer Stoerung
    scheitert, wird als 'environment' statt als 'failed' gewertet (siehe classify_phase_status)."""
    ENV_EVENTS.append((time.time(), kind, text))
    trace_event("env_disturbance", level="warning", kind=kind, text=text)
    log(f"TESTUMGEBUNG GESTOERT ({kind}): {text}")


def classify_phase_status(status: str, started_wall: float) -> str:
    """'failed' wird zu 'environment', wenn seit Phasenbeginn eine Stoerung der Testumgebung gemeldet wurde."""
    if status != "failed":
        return status
    kinds = sorted({kind for when, kind, _ in ENV_EVENTS if when >= started_wall - 1.0})
    if not kinds:
        return status
    trace_event("phase_environment", level="warning", kinds=kinds,
                hinweis="Phase scheiterte waehrend einer Stoerung der Testumgebung - kein Fehler von Geraet/Modul belegt")
    log(f"Phase gilt als 'Testumgebung gestoert' (nicht als Fehler): {', '.join(kinds)}")
    return "environment"


def suite_result(phases: list) -> str:
    """passed = alle bestanden; environment = Rest nur durch gestoerte Umgebung; sonst failed."""
    bad = [phase["result"] for phase in phases if phase["result"] not in ("passed", "skipped")]
    if not bad:
        return "passed" if all(phase["result"] == "passed" for phase in phases) else "failed"
    return "environment" if all(result == "environment" for result in bad) else "failed"


def _suspend_watch() -> None:
    last = time.time()
    while True:
        time.sleep(1.0)
        now = time.time()
        if now - last > PC_SUSPEND_GAP_S:
            env_disturbance("pc_suspend", f"PC stand ca. {now - last:.0f} s still (Standby/Ruhezustand)")
        last = now


class _SystemPowerStatus(ctypes.Structure):
    _fields_ = [("ACLineStatus", ctypes.c_ubyte), ("BatteryFlag", ctypes.c_ubyte),
                ("BatteryLifePercent", ctypes.c_ubyte), ("SystemStatusFlag", ctypes.c_ubyte),
                ("BatteryLifeTime", ctypes.c_ulong), ("BatteryFullLifeTime", ctypes.c_ulong)]


def pc_uptime_s() -> float | None:
    try:
        fn = ctypes.windll.kernel32.GetTickCount64
        fn.restype = ctypes.c_ulonglong
        return fn() / 1000.0
    except Exception:
        return None


def pc_power_status() -> dict:
    try:
        status = _SystemPowerStatus()
        ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(status))
        return {"stromquelle": {0: "Akku", 1: "Netz"}.get(status.ACLineStatus, "unbekannt"),
                "akku_prozent": None if status.BatteryLifePercent == 255 else status.BatteryLifePercent}
    except Exception:
        return {}


PC_BT: dict = {}        # Bluetooth-Adapter des PCs (Name, Hersteller, Treiber, Windows-Stack, Dienst)


def pc_bluetooth_adapter() -> dict:
    """Auf welchem Bluetooth laeuft der Test? Funkadapter des PCs, Treiberversion, Windows-Stack und Dienststatus (nur lesend)."""
    script = (
        "[Console]::OutputEncoding=[Text.Encoding]::UTF8; "
        "$d=Get-CimInstance Win32_PnPSignedDriver -ErrorAction SilentlyContinue | Where-Object { $_.DeviceClass -eq 'BLUETOOTH' }; "
        r"$a=$d | Where-Object { $_.DeviceID -match '^(USB|PCI|ACPI|UART)\\' } | Select-Object -First 1; "
        "$s=$d | Where-Object { $_.InfName -eq 'bth.inf' } | Select-Object -First 1; "
        "$v=(Get-Service bthserv -ErrorAction SilentlyContinue).Status; "
        "[pscustomobject]@{name=$a.DeviceName; hersteller=$a.Manufacturer; treiber=$a.DriverVersion; "
        "datum=$(if ($a.DriverDate) { $a.DriverDate.ToString('yyyy-MM-dd') }); id=$a.DeviceID; stack=$s.DriverVersion; "
        "dienst=\"$v\"} | ConvertTo-Json -Compress")
    try:
        done = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                              capture_output=True, timeout=60)
        data = json.loads(done.stdout.decode("utf-8", "replace").strip() or "{}")
    except Exception:
        return {}
    if data.get("name"):
        match = re.search(r"VID_([0-9A-F]{4})&PID_([0-9A-F]{4})", data.get("id") or "")
        data["usb_id"] = f"{match.group(1)}:{match.group(2)}" if match else None
    PC_BT.clear()
    PC_BT.update({key: value for key, value in data.items() if value})
    return PC_BT


def pc_logging_start(args) -> None:
    """Beim Teststart: PC wachhalten, PC-Zustand ins Trace."""
    PC_STATE["start_wall"] = time.time()
    adapter = pc_bluetooth_adapter()
    if adapter:                                          # nice to know: nur im Trace, nicht im Testlog/-bericht
        trace_event("pc_bluetooth", **adapter)
    try:   # ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED: System und Bildschirm bleiben an
        fn = ctypes.windll.kernel32.SetThreadExecutionState
        fn.argtypes = [ctypes.c_uint]
        fn.restype = ctypes.c_uint
        PC_STATE["awake"] = fn(0x80000003) != 0     # + ES_DISPLAY_REQUIRED: Bildschirm bleibt an, sonst Standby nach Sperren
    except Exception:
        PC_STATE["awake"] = False
    threading.Thread(target=_suspend_watch, name="suspend-watch", daemon=True).start()
    uptime = pc_uptime_s()
    power = pc_power_status()
    fresh = uptime is not None and uptime < PC_FRESH_BOOT_S
    trace_event("pc_info", level="warning" if fresh else "info", uptime_s=uptime, keep_awake=PC_STATE["awake"],
                **power)
    log(f"PC: laeuft seit {uptime / 60:.0f} min, {power.get('stromquelle', '?')}"
        f"{f', Akku {power['akku_prozent']} %' if power.get('akku_prozent') is not None else ''}; "
        f"Wachhalten {'aktiv' if PC_STATE['awake'] else 'NICHT moeglich'}."
        if uptime is not None else "PC: Laufzeit nicht ermittelbar.")
    if fresh:
        log("WARNUNG: PC ist erst vor wenigen Minuten gestartet - Wartung/Treiberstart kann den Bluetooth-Link "
            "beeinflussen (Messwerte mit Vorsicht deuten).")


def pc_logging_stop() -> None:
    """Wachhalten wieder aufheben."""
    try:
        fn = ctypes.windll.kernel32.SetThreadExecutionState
        fn.argtypes = [ctypes.c_uint]
        fn.restype = ctypes.c_uint
        fn(0x80000000)   # ES_CONTINUOUS ohne Anforderung
    except Exception:
        pass


def pc_events_export() -> None:
    """Windows-Systemereignisse des Testzeitraums exportieren und auswerten (einmal je Lauf)."""
    if PC_STATE["exported"] or PC_STATE["start_wall"] is None:
        return
    PC_STATE["exported"] = True
    start = datetime.fromtimestamp(PC_STATE["start_wall"] - 60).strftime("%Y-%m-%dT%H:%M:%S")
    script = (
        "[Console]::OutputEncoding=[Text.Encoding]::UTF8; "
        f"$s=[datetime]'{start}'; "
        "Get-WinEvent -FilterHashtable @{LogName='System'; StartTime=$s} -ErrorAction SilentlyContinue | "
        f"Where-Object {{ $_.ProviderName -match '{PC_EVENT_PROVIDERS}' }} | Sort-Object TimeCreated | "
        "Select-Object @{n='time';e={$_.TimeCreated.ToString('s')}},ProviderName,Id,LevelDisplayName,"
        "@{n='msg';e={($_.Message -split \"`n\")[0]}} | ConvertTo-Json -Compress")
    try:
        done = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                              capture_output=True, timeout=90)
        text = done.stdout.decode("utf-8", "replace").strip()
        events = json.loads(text) if text else []
        if isinstance(events, dict):
            events = [events]
    except Exception as exc:
        log(f"PC-Ereignisexport nicht moeglich: {exc}")
        trace_event("pc_events_error", level="warning", error=str(exc))
        return
    report_dir = RUN_REPORT_DIR or REPORT_DIR
    try:
        report_dir.mkdir(parents=True, exist_ok=True)
        (report_dir / "pc_events.json").write_text(json.dumps(events, indent=2, ensure_ascii=False), encoding="utf-8")
    except OSError:
        pass
    standby_enter = None
    standby_phases = 0
    for event in events:
        provider, event_id, when = event.get("ProviderName", ""), event.get("Id"), event.get("time")
        if "Kernel-Power" in provider and event_id == 506:      # Wechsel in den modernen Standby
            standby_enter = when
        elif "Kernel-Power" in provider and event_id == 507:    # Standby beendet
            standby_phases += 1
            trace_event("pc_standby", level="warning", entered=standby_enter, exited=when,
                        hinweis="PC war im modernen Standby - Bluetooth kann dabei getrennt/gedrosselt werden")
            standby_enter = None
        elif "Kernel-Power" in provider and event_id in (42, 107):   # Ruhezustand / Fortsetzen
            trace_event("pc_sleep_event", level="warning", id=event_id, time=when, msg=event.get("msg"))
        elif "Kernel-Power" in provider and event_id == 105:    # Stromquellenwechsel
            trace_event("pc_power_source_change", level="warning", time=when)
        elif (re.search("bth|bluetooth|usb", provider, re.I) and event.get("LevelDisplayName") in
              ("Fehler", "Warnung", "Error", "Warning", "Kritisch", "Critical")):
            trace_event("pc_bluetooth_usb_event", level="warning", provider=provider, id=event_id, time=when,
                        msg=(event.get("msg") or "")[:200])
    if standby_enter is not None:
        trace_event("pc_standby", level="warning", entered=standby_enter, exited=None,
                    hinweis="PC war am Testende noch/ erneut im Standby")
        standby_phases += 1
    log(f"PC-Ereignisse im Testzeitraum: {len(events)} (pc_events.json), Standby-Phasen: {standby_phases}.")


def collect_phase_report(phase_name: str, started_wall: float, index: int) -> dict | None:
    """Liest den Bericht, den die Phase geschrieben hat (neueste JSON-Datei, die seit Phasenstart geaendert wurde),
    und sichert ihn zusaetzlich unter eigenem Namen (phase_NN_<Phase>.json).

    write_report() nutzt je Art immer denselben Dateinamen (download.json, cycle.json, ...) und ueberschreibt ihn.
    Die frueher verwendete Mengendifferenz der Dateien fand deshalb nur den jeweils ersten Bericht je Art: alle
    weiteren Downloads/Zyklen/BLE-Phasen hatten 'report: null' im Gesamtbericht."""
    report_dir = RUN_REPORT_DIR or REPORT_DIR
    try:
        fresh = [path for path in report_dir.glob("*.json")
                 if path.name not in SUITE_SUMMARY_FILES and not path.name.startswith(("phase_", "sniffer_"))
                 and path.stat().st_mtime >= started_wall]   # streng: kein Spielraum, sonst wird der Bericht der
                                                             # Vorphase (z. B. cycle.json) einer Phase ohne eigenen
                                                             # Bericht (Flash schreiben) zugeschrieben
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


# ---------------------------------------------------------------------------
# BLE-Funk-Mitschnitt mit nRF52840 (optional, --sniff; Plan: Doku/Plan_BLE_Sniffer_nRF52840.md)
#
# Startet "nrfutil ble-sniffer sniff" (nRF Sniffer for Bluetooth LE auf COM-Port, wird automatisch gefunden) parallel
# zum Test und wertet den Mitschnitt danach mit tshark aus: Verbindungsparameter, Parameter-Updates, ATT-Ablauf
# (CCCD-Write/Antwort, Notifications), Trennen (wer, Grund) und Funkqualitaet. Ein Mitschnitt je BLE-Phase der Suiten,
# bei Einzeltests einer fuer den ganzen Lauf. Ergebnis: sniffer_<label>.pcapng (nur Zielgeraet, oeffnet in Wireshark)
# und sniffer_<label>.json im Report-Ordner, dazu das Trace-Ereignis "sniffer_summary" und eine Zeile im Phasenbericht.
# Ohne --sniff aendert sich nichts.
# ---------------------------------------------------------------------------

SNIFF_ADV_ACCESS_ADDRESS = "0x8e89bed6"
SNIFF_WARMUP_S = 4.0
SNIFF_PER_PHASE_MODES = ("module-suite", "fast-suite", "interleave", "soak", "classic-suite")
SNIFF_TMP_DIR = REPORT_DIR / "_sniffer_tmp"
SNIFF_STATE: dict = {"proc": None, "follow": None, "label": None, "raw": None, "started": None, "log": None,
                     "port": None, "failed": False}
LL_CONTROL_NAMES = {0x00: "LL_CONNECTION_UPDATE_IND", 0x01: "LL_CHANNEL_MAP_IND", 0x02: "LL_TERMINATE_IND",
                    0x03: "LL_ENC_REQ", 0x04: "LL_ENC_RSP", 0x05: "LL_START_ENC_REQ", 0x06: "LL_START_ENC_RSP",
                    0x07: "LL_UNKNOWN_RSP", 0x08: "LL_FEATURE_REQ", 0x09: "LL_FEATURE_RSP",
                    0x0A: "LL_PAUSE_ENC_REQ", 0x0B: "LL_PAUSE_ENC_RSP", 0x0C: "LL_VERSION_IND",
                    0x0D: "LL_REJECT_IND", 0x0E: "LL_PERIPHERAL_FEATURE_REQ", 0x0F: "LL_CONNECTION_PARAM_REQ",
                    0x10: "LL_CONNECTION_PARAM_RSP", 0x11: "LL_REJECT_EXT_IND", 0x12: "LL_PING_REQ",
                    0x13: "LL_PING_RSP", 0x14: "LL_LENGTH_REQ", 0x15: "LL_LENGTH_RSP", 0x16: "LL_PHY_REQ",
                    0x17: "LL_PHY_RSP", 0x18: "LL_PHY_UPDATE_IND"}
LL_ERROR_NAMES = {0x08: "Verbindungs-Timeout (Supervision)", 0x13: "Gegenstelle hat getrennt",
                  0x14: "Gegenstelle: wenig Ressourcen", 0x15: "Gegenstelle: Strom aus", 0x16: "lokal getrennt",
                  0x1A: "nicht unterstuetzt", 0x28: "Zeitpunkt verpasst (Instant)", 0x3B: "Parameter nicht akzeptabel",
                  0x3D: "MIC-Fehler (Verschluesselung)", 0x3E: "Verbindungsaufbau fehlgeschlagen"}


def _sniff_tool(*candidates) -> str | None:
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found
    return None


def sniff_tshark() -> str | None:
    return _sniff_tool("tshark") or next((str(p) for p in (Path(r"C:\Program Files\Wireshark\tshark.exe"),) if p.exists()), None)


def sniff_executable() -> list | None:
    """Befehl fuer den nRF-Sniffer; bevorzugt die direkte Programmdatei (ein Prozess, sauber beendbar)."""
    home = Path.home() / ".nrfutil" / "bin"
    direct = home / "nrfutil-ble-sniffer.exe"
    if direct.exists():
        return [str(direct)]
    wrapper = _sniff_tool("nrfutil") or (str(home / "nrfutil.exe") if (home / "nrfutil.exe").exists() else None)
    return [wrapper, "ble-sniffer"] if wrapper else None


def sniff_find_port(requested: str | None) -> str | None:
    """COM-Port des nRF-Sniffers: --sniff-port, sonst 'nrfutil device list' (Produkt 'nRF Sniffer ...')."""
    if requested:
        return requested
    home = Path.home() / ".nrfutil" / "bin" / "nrfutil.exe"
    exe = _sniff_tool("nrfutil") or (str(home) if home.exists() else None)
    if not exe:
        return None
    try:
        text = subprocess.run([exe, "device", "list"], capture_output=True, timeout=30).stdout.decode("utf-8", "replace")
    except Exception:
        return None
    for block in re.split(r"\r?\n\s*\r?\n", text):
        if re.search(r"Product\s+.*sniffer", block, re.I):
            match = re.search(r"Ports\s+(COM\d+)", block)
            if match:
                return match.group(1)
    return None


def sniff_start(args, label: str) -> bool:
    """Startet den Mitschnitt fuer das Zielgeraet (args.address). Ohne bekannte Adresse wird bis scan_for_device() gewartet."""
    if not getattr(args, "sniff", False) or SNIFF_STATE["failed"]:
        return False
    SNIFF_STATE["args"] = args
    follow = getattr(args, "sniff_follow", None) or getattr(args, "address", None)
    if SNIFF_STATE["proc"] is not None:
        if follow and follow.upper() != (SNIFF_STATE["follow"] or "").upper():
            sniff_stop(args, quiet=True)     # anderes Zielgeraet: neu starten
        else:
            return True
    SNIFF_STATE["label"] = label
    if not follow:
        SNIFF_STATE["pending"] = True        # Adresse noch unbekannt, scan_for_device() holt den Start nach
        return False
    SNIFF_STATE["pending"] = False
    exe, port = sniff_executable(), sniff_find_port(getattr(args, "sniff_port", None))
    if not exe or not port:
        log(f"Sniffer: {'nrfutil' if not exe else 'nRF-Sniffer-COM-Port'} nicht gefunden - Test laeuft ohne Funk-Mitschnitt.")
        trace_event("sniffer_unavailable", level="warning", nrfutil=bool(exe), port=port)
        SNIFF_STATE["failed"] = True
        return False
    SNIFF_TMP_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    raw = SNIFF_TMP_DIR / f"raw_{stamp}.pcap"
    log_path = SNIFF_TMP_DIR / f"raw_{stamp}.log"
    log_file = log_path.open("wb")
    try:
        proc = subprocess.Popen(exe + ["sniff", "--port", port, "--follow", follow, "--output-pcap-file", str(raw)],
                                stdout=log_file, stderr=subprocess.STDOUT,
                                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
    except OSError as exc:
        log_file.close()
        log(f"Sniffer: Start nicht moeglich ({exc}) - Test laeuft ohne Funk-Mitschnitt.")
        SNIFF_STATE["failed"] = True
        return False
    time.sleep(SNIFF_WARMUP_S)       # Sniffer muss auf die Werbung des Ziels synchronisieren, sonst verpasst er das CONNECT_IND
    if proc.poll() is not None:
        log_file.close()
        tail = log_path.read_text(errors="replace").strip()[-300:]
        log(f"Sniffer: beendete sich sofort (Port {port} belegt? Wireshark offen?): {tail}")
        trace_event("sniffer_unavailable", level="warning", port=port, output=tail)
        SNIFF_STATE["failed"] = True
        return False
    SNIFF_STATE.update(proc=proc, follow=follow, raw=raw, started=time.time(), log=log_file, port=port, log_path=log_path)
    log(f"Sniffer: Mitschnitt gestartet ({port}, Ziel {follow}, '{label}').")
    trace_event("sniffer_started", port=port, follow=follow, label=label, raw=str(raw))
    return True


def sniff_target_known(address: str) -> None:
    """Aus scan_for_device(): Zieladresse ist jetzt bekannt, ausstehenden Mitschnitt starten."""
    args = SNIFF_STATE.get("args")
    if args is not None and SNIFF_STATE["proc"] is None and SNIFF_STATE.get("pending"):
        sniff_start_with(args, address)


def sniff_start_with(args, address: str) -> bool:
    saved = getattr(args, "sniff_follow", None)
    args.sniff_follow = address
    try:
        return sniff_start(args, SNIFF_STATE["label"] or "run")
    finally:
        args.sniff_follow = saved


def _sniff_stop_process() -> None:
    proc = SNIFF_STATE["proc"]
    if proc is None:
        return
    try:
        if proc.poll() is None:
            try:
                proc.send_signal(signal.CTRL_BREAK_EVENT)    # sauber beenden, damit der letzte Puffer geschrieben wird
                proc.wait(timeout=6)
            except (subprocess.TimeoutExpired, OSError, ValueError):
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
    finally:
        try:
            SNIFF_STATE["log"].close()
        except Exception:
            pass
        SNIFF_STATE["proc"] = None


def sniff_stop(args, quiet: bool = False) -> dict | None:
    """Beendet den Mitschnitt, wertet ihn aus und legt pcapng + JSON im Report-Ordner ab. Liefert die Kurzfassung."""
    if SNIFF_STATE["proc"] is None:
        return None
    raw, follow, label, started = SNIFF_STATE["raw"], SNIFF_STATE["follow"], SNIFF_STATE["label"], SNIFF_STATE["started"]
    crashed = SNIFF_STATE["proc"].poll() is not None
    _sniff_stop_process()
    ended = time.time()
    if crashed:
        log("Sniffer: Prozess war vorzeitig beendet - Mitschnitt unvollstaendig.")
    try:
        summary = sniff_analyze(args, raw, follow, label, started, ended, quiet)
    except Exception as exc:     # Auswertung darf den Test nie stoeren
        log(f"Sniffer: Auswertung fehlgeschlagen: {exc}")
        trace_event("sniffer_error", level="warning", error=str(exc), raw=str(raw))
        return None
    return summary


def _sniff_int(value: str) -> int | None:
    value = (value or "").split(",")[0].strip()
    if not value:
        return None
    try:
        return int(value, 0)
    except ValueError:
        return None


def sniff_analyze(args, raw: Path, follow: str, label: str, started: float, ended: float, quiet: bool) -> dict:
    tshark = sniff_tshark()
    report_dir = RUN_REPORT_DIR or REPORT_DIR
    report_dir.mkdir(parents=True, exist_ok=True)
    safe = re.sub(r"[^A-Za-z0-9]+", "_", label).strip("_") or "run"
    summary: dict = {"label": label, "target": follow, "start": datetime.fromtimestamp(started).isoformat(timespec="seconds"),
                     "duration_s": round(ended - started, 1), "befund": [], "ereignisse": []}
    if not raw.exists() or raw.stat().st_size < 100:
        summary["befund"].append("Mitschnitt leer (Sniffer lieferte keine Pakete).")
    elif not tshark:
        keep = report_dir / f"sniffer_{safe}.pcap"
        shutil.move(str(raw), str(keep))
        summary["pcap"] = keep.name
        summary["befund"].append("tshark nicht gefunden - keine automatische Auswertung (pcap in Wireshark oeffnen).")
    else:
        reduced = report_dir / f"sniffer_{safe}.pcapng"
        target = follow.lower()
        keep_filter = f"btle.advertising_address == {target} || btle.access_address != {SNIFF_ADV_ACCESS_ADDRESS}"
        done = subprocess.run([tshark, "-r", str(raw), "-Y", keep_filter, "-w", str(reduced)], capture_output=True, timeout=300)
        if done.returncode == 0 and reduced.exists():
            summary["pcap"] = reduced.name
            if not getattr(args, "sniff_keep_raw", False):
                raw.unlink(missing_ok=True)
        else:
            summary["befund"].append("tshark konnte den Mitschnitt nicht filtern; Rohdatei bleibt erhalten.")
            reduced = raw
        _sniff_extract(tshark, reduced, summary, started, ended)
    for stale in SNIFF_TMP_DIR.glob("raw_*.log"):
        stale.unlink(missing_ok=True)
    try:
        (report_dir / f"sniffer_{safe}.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    except OSError:
        pass
    short = {key: summary.get(key) for key in ("label", "target", "pcap", "connect", "terminate", "cccd", "notifications",
                                              "crc_fehler", "befund")}
    trace_event("sniffer_summary", **short)
    if not quiet:
        log(f"Sniffer '{label}': " + " | ".join(summary["befund"] or ["keine Auffaelligkeit"]))
    return short


SNIFF_FIELDS = ("frame.time_epoch", "btle.advertising_header.pdu_type", "btle.initiator_address",
                "btle.link_layer_data.interval", "btle.link_layer_data.latency", "btle.link_layer_data.timeout",
                "btle.access_address", "btle.data_header.llid", "btle.control_opcode", "btle.control.error_code",
                "btle.control.interval", "btle.control.interval.min", "btle.control.interval.max",
                "btle.data_header.length", "btle.control.latency", "btle.control.timeout", "btatt.opcode", "btatt.handle",
                "btatt.value", "nordic_ble.rssi", "nordic_ble.crc.bad", "btle.crc.incorrect", "_ws.col.Source",
                "nordic_ble.direction")


def _sniff_sender(source: str, direction: str) -> str:
    text = (source or "").lower()
    if "central" in text or "master" in text:
        return "PC (Central)"
    if "periph" in text or "slave" in text:
        return "Modul (Peripheral)"
    if direction.strip() in ("True", "1"):        # nordic_ble.direction: True = Central -> Peripheral
        return "PC (Central)"
    if direction.strip() in ("False", "0"):
        return "Modul (Peripheral)"
    return "unklar"


def _sniff_extract(tshark: str, pcap: Path, summary: dict, started: float, ended: float) -> None:
    cmd = [tshark, "-r", str(pcap), "-Y", "btle.access_address != " + SNIFF_ADV_ACCESS_ADDRESS + " || btle.advertising_header.pdu_type == 0x05",
           "-T", "fields", "-E", "separator=/t"]
    for field in SNIFF_FIELDS:
        cmd += ["-e", field]
    done = subprocess.run(cmd, capture_output=True, timeout=300)
    if done.returncode != 0:
        summary["befund"].append("tshark-Auswertung fehlgeschlagen: " + done.stderr.decode("utf-8", "replace").strip()[:200])
        return
    rows = done.stdout.decode("utf-8", "replace").splitlines()
    events = summary["ereignisse"]
    befund = summary["befund"]
    connect_t = last_data_t = first_t = None
    data_packets = total = crc_bad = 0
    notifications = writes_noresp = 0
    rssis: list = []
    cccd: list = []              # alle Writes auf ein CCCD, je mit Antwortzeit
    terminate = None

    for row in rows:
        col = (row.split("\t") + [""] * len(SNIFF_FIELDS))[:len(SNIFF_FIELDS)]
        rec = dict(zip(SNIFF_FIELDS, col))
        try:
            when = float(rec["frame.time_epoch"])
        except ValueError:
            continue
        rel = (lambda t: round(t - connect_t, 3) if connect_t is not None else None)
        if rec["btle.advertising_header.pdu_type"].split(",")[0].strip().lower() in ("0x05", "5"):
            connect_t = when
            interval, timeout = _sniff_int(rec["btle.link_layer_data.interval"]), _sniff_int(rec["btle.link_layer_data.timeout"])
            summary["connect"] = {"zeit": datetime.fromtimestamp(when).isoformat(timespec="milliseconds"),
                                  "initiator": rec["btle.initiator_address"],
                                  "intervall_ms": interval * 1.25 if interval is not None else None,
                                  "latenz": _sniff_int(rec["btle.link_layer_data.latency"]),
                                  "timeout_ms": timeout * 10 if timeout is not None else None}
            events.append({"t": 0.0, "was": "CONNECT_IND", **summary["connect"]})
            continue
        if connect_t is None:
            continue
        total += 1
        first_t = first_t or when
        if rec["nordic_ble.crc.bad"].strip() in ("1", "True") or rec["btle.crc.incorrect"].strip() in ("1", "True"):
            crc_bad += 1
        rssi = _sniff_int(rec["nordic_ble.rssi"])
        if rssi is not None:
            rssis.append(rssi)
        llid = _sniff_int(rec["btle.data_header.llid"])
        if llid == 2 or (llid == 1 and (_sniff_int(rec["btle.data_header.length"]) or 0) > 0):   # L2CAP-Nutzdaten
            data_packets += 1
        if llid is not None and not (llid == 1 and not (_sniff_int(rec["btle.data_header.length"]) or 0)):
            last_data_t = when                                  # letztes Paket, das nicht leer war (Lebenszeichen)
        opcode = _sniff_int(rec["btle.control_opcode"])
        if opcode is not None:
            name = LL_CONTROL_NAMES.get(opcode, f"LL_0x{opcode:02X}")
            ev = {"t": rel(when), "was": name, "von": _sniff_sender(rec["_ws.col.Source"], rec["nordic_ble.direction"])}
            if opcode == 0x02:
                code = _sniff_int(rec["btle.control.error_code"])
                ev["grund"] = f"0x{code:02X} {LL_ERROR_NAMES.get(code, '?')}" if code is not None else "?"
                terminate = {"t": ev["t"], "von": ev["von"], "grund": ev["grund"]}
            elif opcode in (0x00, 0x0F, 0x10):
                interval = _sniff_int(rec["btle.control.interval"])
                if interval is not None:
                    ev["intervall_ms"] = interval * 1.25
                lo, hi = _sniff_int(rec["btle.control.interval.min"]), _sniff_int(rec["btle.control.interval.max"])
                if lo is not None and hi is not None:
                    ev["intervall_ms"] = f"{lo * 1.25:g}-{hi * 1.25:g}"
                ev["latenz"] = _sniff_int(rec["btle.control.latency"])
                timeout = _sniff_int(rec["btle.control.timeout"])
                ev["timeout_ms"] = timeout * 10 if timeout is not None else None
            events.append(ev)
        att = _sniff_int(rec["btatt.opcode"])
        if att is not None:
            handle, value = rec["btatt.handle"].split(",")[0], rec["btatt.value"].replace(":", "").lower()
            if att == 0x12:        # Write Request
                if value in ("0100", "0200", "0300", "0000") and len(value) == 4:
                    cccd.append({"write_t": rel(when), "wert": value, "handle": handle,
                                 "art": {"0100": "Notify", "0200": "Indicate", "0300": "Notify+Indicate"}.get(value, "aus")})
                    events.append({"t": rel(when), "was": "ATT Write Request (CCCD)", "handle": handle, "wert": value})
                elif len(events) < 400:
                    events.append({"t": rel(when), "was": "ATT Write Request", "handle": handle, "bytes": len(value) // 2})
            elif att == 0x13 and cccd and "antwort_t" not in cccd[-1]:
                cccd[-1]["antwort_t"] = rel(when)
                events.append({"t": rel(when), "was": "ATT Write Response"})
            elif att == 0x1B:      # Handle Value Notification
                notifications += 1
                if notifications <= 3:
                    events.append({"t": rel(when), "was": "ATT Notification", "handle": handle, "bytes": len(value) // 2})
            elif att == 0x52:
                writes_noresp += 1
            elif att == 0x01:
                events.append({"t": rel(when), "was": "ATT Error Response", "handle": handle})

    connect = summary.get("connect")
    summary.update(pakete=total, datenpakete=data_packets, crc_fehler=crc_bad, notifications=notifications,
                   write_ohne_antwort=writes_noresp, cccd=cccd or None, terminate=terminate,
                   rssi_mittel=round(sum(rssis) / len(rssis), 1) if rssis else None, rssi_min=min(rssis) if rssis else None)
    if connect is None:
        befund.append("Keine Verbindung im Mitschnitt (CONNECT_IND nicht gesehen: Adresse, Reichweite, Sniffer-Position?).")
        return
    befund.append(f"Verbindung: Intervall {connect['intervall_ms']} ms, Latenz {connect['latenz']}, "
                  f"Timeout {connect['timeout_ms']} ms.")
    for entry in cccd:
        befund.append(f"CCCD-Write {entry['art']} ({entry['wert']}, Handle {entry['handle']}) bei {entry['write_t']} s, "
                      f"Antwort {'bei ' + str(entry['antwort_t']) + ' s' if 'antwort_t' in entry else 'FEHLT'}.")
    if not cccd:
        befund.append("Kein CCCD-Write im Mitschnitt.")
    befund.append(f"{notifications} Notifications, {writes_noresp} Write-ohne-Antwort, {data_packets} Datenpakete, "
                  f"{crc_bad} CRC-Fehler.")
    if terminate:
        befund.append(f"TRENNEN durch {terminate['von']} nach {terminate['t']} s, Grund {terminate['grund']}.")
    elif last_data_t is not None and ended - last_data_t > 2.0 and ended - connect_t > 5:
        befund.append(f"Mitschnitt der Verbindung endet {last_data_t - connect_t:.1f} s nach CONNECT_IND ohne "
                      "LL_TERMINATE_IND: Sniffer hat den Sync verloren (z. B. nach einem Parameter-Update) oder Funkverlust.")
    elif last_data_t is None:
        befund.append("Verbindung ohne jedes Datenpaket mitgeschnitten (Sniffer hat nach CONNECT_IND nichts mehr gehoert).")
    else:
        befund.append("Verbindung bis Mitschnittende aktiv.")


def sniff_phase_begin(args, name: str, transport: str, index: int) -> bool:
    """Phase einer Suite: je BLE-Phase ein eigener Mitschnitt (Classic kann der Sniffer nicht)."""
    if not getattr(args, "sniff", False) or transport != "ble":
        return False
    sniff_start(args, f"phase_{index:02d}_{name}")
    return True


def sniff_phase_end(args, active: bool) -> dict | None:
    if not active:
        return None
    summary = sniff_stop(args)
    SNIFF_STATE["pending"] = False
    return summary


# ---------------------------------------------------------------------------
# Geraete-Identifikation (2026-10-20): Hauptmenue -> Information -> zurueck, daraus Firmware, Seriennummer, Hardware,
# Betriebsstunden, Schnittstellen, Uhrzeit. Laeuft als erster Schritt jedes Tests (siehe preflight_identify) und geht
# ueber jeden Kanal mit einem Eingangsstrom (.rx, ByteStream) und write(): Bluetooth (BLE/Classic-Transport),
# RS232 und USB (ueber den Mitschnitt-Thread, Rs232Sniffer.attach). Erkennt die Menues sprachunabhaengig an den
# Auswahlbereichen "1...6?" (Hauptmenue) und "1...9?" (Informationsmenue).
# ---------------------------------------------------------------------------

DEVICE_INFO: dict = {}                 # Label ("A", "B", "RS232", "bt" ...) -> Info-Dict (inkl. Rohtext)
MARK_MAIN_RANGE = "1...6?"             # Hauptmenue: "Ihre Auswahl 1...6?"
MARK_INFO_RANGE = "1...9?"             # Informationsmenue: "9. zurueck", "Ihre Auswahl 1...9?"
SERIAL_RE = re.compile(r"\b(\d{2}[A-Z]{2}\d{4})\b")
CLOCK_WARN_S = 600                     # Geraeteuhr weicht mehr als 10 Minuten von der PC-Uhr ab -> Hinweis

# Stichwoerter der Anzeigetexte (Sprachtabellen sictxt_*.c); alles, was nicht erkannt wird, bleibt im Rohtext erhalten.
_KW_HOURS = r"(Betriebsstunden|Operating hours|Heures de fonctionnement|Orario d'esercizio|Roboczogodzin|Horas de funcionamiento|Provozn|Obratovalnih)"
_KW_PSET = r"(Aktiver Parametersatz|Active parameter set|Jeu de param|Set di parametri|Aktivni|Conjunto de par|Aktywny zestaw)"
_KW_IFACE = r"(Schnittstellen|Interfaces|Interfacce|Rozhran|Vmesnik|Interfaz)"
_KW_TZONE = r"(Zeitzone|Time zone|Fuseau horaire|Fuso orario|Strefa czasu|Casov|Zona horaria)"


def parse_info_screen(text: str) -> dict:
    """Zerlegt den Text des Informationsmenues in Felder. Alle Felder sind optional; der Rohtext bleibt in 'raw'."""
    info: dict = {"raw": text}
    lines = [line.strip() for line in re.split(r"[\r\n]+", text)]
    lines = [line for line in lines if line]
    for line in lines:                                          # "Viasis 3003M - Version 5.124: J2"
        if re.search(r"3003|viasis", line, re.I) and re.search(r"\d+\.\d{2,3}", line):
            version = re.search(r"(\d+\.\d{2,3})", line)
            info["firmware"] = version.group(1)
            model = re.split(r"\s+-\s+", line, maxsplit=1)[0].strip()
            if model and not re.search(r"\d+\.\d{2,3}", model):
                info["modell"] = model
            hardware = re.search(r"\d+\.\d{2,3}\s*[,:]\s*(\S+)", line)       # echte Seite: "Version 5.124, J2"
            if hardware:
                info["hardware"] = hardware.group(1)
            break
    serial = SERIAL_RE.search(text)
    if serial:
        info["seriennummer"] = serial.group(1)
    for line in lines:
        if re.search(_KW_HOURS, line, re.I):
            number = re.search(r"(\d+)\s*h\b", line)
            if number:
                info["betriebsstunden"] = int(number.group(1))
        elif re.search(_KW_PSET, line, re.I):
            number = re.search(r":\s*(\d+)", line)
            if number:
                info["parametersatz"] = int(number.group(1))
        elif re.search(_KW_TZONE, line, re.I) and "zeitzone" not in info:
            number = re.search(r":\s*(-?\d+)", line)
            if number:
                info["zeitzone"] = int(number.group(1))
        elif re.match(r"Zeitquelle\s*:", line, re.I):
            info["zeitquelle"] = line.split(":", 1)[1].strip()
            info["uhr_gestellt"] = info["zeitquelle"].lower() not in ("ungestellt", "unset", "")
        elif re.search(r"Automatische Zeit|Automatic time", line, re.I):
            info["automatische_zeit"] = line.split(":", 1)[1].strip() if ":" in line else None
    for index, line in enumerate(lines):                        # "Schnittstellen: Bluetooth, USB, GSM/GPRS-LTE, GPS"
        if re.match(_KW_IFACE + r"\s*:", line, re.I):
            collected = line.split(":", 1)[1]
            for follow in lines[index + 1:index + 3]:           # Fortsetzungszeilen ohne Menuenummer/Label
                if re.match(r"\d\.|[A-Za-zÀ-ÿ /-]+:", follow):
                    break
                collected += " " + follow
            info["schnittstellen"] = [part.strip() for part in re.split(r"[,;]", collected) if part.strip()]
            break
    date = re.search(r"\b(\d{1,2}\.\d{1,2}\.\d{2,4})\b", text)
    clock = re.search(r"\b(\d{1,2}:\d{2}:\d{2})\b", text)
    if date:
        info["datum"] = date.group(1)
    if clock:
        info["zeit"] = clock.group(1)
    return info


def device_clock_offset_s(info: dict) -> float | None:
    """Geraeteuhr minus PC-Uhr in Sekunden (nur Datum/Zeit, keine Zeitzonenumrechnung); None wenn nicht lesbar."""
    if "datum" not in info or "zeit" not in info:
        return None
    for fmt in ("%d.%m.%y %H:%M:%S", "%d.%m.%Y %H:%M:%S"):
        try:
            device = datetime.strptime(f"{info['datum']} {info['zeit']}", fmt)
        except ValueError:
            continue
        return (device - datetime.now()).total_seconds()
    return None


async def read_device_info(channel, timeout: float = 8.0) -> dict | None:
    """Hauptmenue -> Information -> zurueck ueber einen Kanal (.rx ByteStream, async write). Liefert das geparste Info-Dict
    (mit 'raw') oder None, wenn das Hauptmenue nicht erreichbar war. Voraussetzung: das Geraet ist ansprechbar, bei
    Bluetooth ist die PIN bereits eingegeben."""
    rx = channel.rx
    rx.clear()
    await channel.write(b"H")                                    # Hauptmenue ausgeben
    if not await rx.wait_for_marker(MARK_MAIN_RANGE, timeout):
        return None
    await asyncio.sleep(0.3)
    rx.clear()
    await channel.write(b"6")                                    # 6 = Information und Zeit
    shown = await rx.wait_for_marker(MARK_INFO_RANGE, timeout)
    await asyncio.sleep(0.2)
    text = bytes(rx.buf).decode("cp1252", errors="replace")
    rx.clear()
    await channel.write(b"9")                                    # 9 = zurueck
    back = await rx.wait_for_marker(MARK_MAIN_RANGE, timeout)
    info = parse_info_screen(text)
    info["info_menue_gesehen"] = bool(shown)
    info["zurueck_im_hauptmenue"] = bool(back)
    return info


class SnifferChannel:
    """Kanal-Adapter fuer RS232/USB: nutzt den ohnehin laufenden Mitschnitt-Thread (Lesen) und dessen Schnittstelle (Schreiben)."""

    def __init__(self, sniffer):
        self.sniffer = sniffer
        self.rx = sniffer.attach(asyncio.get_running_loop())

    async def write(self, data: bytes) -> None:
        await asyncio.to_thread(self.sniffer.write, data)

    def close(self) -> None:
        self.sniffer.detach()


def _record_device_info(label: str, info: dict, args, source: str) -> None:
    """Meldung, Trace und Plausibilitaetspruefungen (Name/Seriennummer, Firmware, Uhr)."""
    info["quelle"] = source
    offset = device_clock_offset_s(info)
    if offset is not None:
        info["uhr_abweichung_s"] = round(offset)
    DEVICE_INFO[label] = info
    shown = {key: info.get(key) for key in ("modell", "firmware", "hardware", "seriennummer", "betriebsstunden",
                                            "parametersatz", "schnittstellen", "datum", "zeit", "zeitquelle")}
    trace_event("device_info", label=label, source=source, **shown, uhr_abweichung_s=info.get("uhr_abweichung_s"),
                info_menue_gesehen=info.get("info_menue_gesehen"), zurueck=info.get("zurueck_im_hauptmenue"))
    log(f"Geraet {label} ({source}): {info.get('modell', '?')} FW {info.get('firmware', '?')} "
        f"HW {info.get('hardware', '?')} Serien-Nr. {info.get('seriennummer', '?')}, "
        f"{info.get('betriebsstunden', '?')} h, Uhr {info.get('datum', '?')} {info.get('zeit', '?')}"
        f"{f', Schnittstellen: ' + ', '.join(info['schnittstellen']) if info.get('schnittstellen') else ''}.")
    wanted = getattr(args, "expect_fw", None)
    if wanted and info.get("firmware") and info["firmware"] != wanted:
        trace_event("device_firmware_mismatch", level="warning", label=label, found=info["firmware"], expected=wanted)
        log(f"WARNUNG: Firmware {info['firmware']} auf Geraet {label}, erwartet {wanted}.")
    name = str(getattr(args, "name", "") or "")
    serial = info.get("seriennummer")
    if serial and SERIAL_RE.search(name) and SERIAL_RE.search(name).group(1) != serial:
        trace_event("device_serial_mismatch", level="warning", label=label, serial=serial, name=name)
        log(f"WARNUNG: Seriennummer {serial} passt nicht zum Zielnamen '{name}'.")
    if offset is not None and abs(offset) > CLOCK_WARN_S:
        trace_event("device_clock_off", level="warning", label=label, abweichung_s=round(offset))
        log(f"HINWEIS: Geraeteuhr weicht {offset / 3600:+.1f} h von der PC-Uhr ab (Zeitquelle: "
            f"{info.get('zeitquelle', '?')}); Zeitstempel in Protokoll und Dateinamen sind dann nicht verlaesslich.")
    if not info.get("firmware"):
        log(f"WARNUNG: Firmware auf Geraet {label} nicht aus der Info-Seite lesbar (Rohtext im Bericht).")


def _version_from_hex(value: str) -> str:
    """'01040C0C' -> '1.4.12.12' (je ein Byte pro Teil)."""
    try:
        return ".".join(str(int(value[index:index + 2], 16)) for index in range(0, len(value), 2))
    except ValueError:
        return value


def parse_bt_info(text: str) -> dict:
    """Zerlegt die Ausgabe von Funkmodems -> Bluetooth -> Information (EZ-Serial-Antworten des IF820) in Felder."""
    info: dict = {"raw": text}
    if re.search(r"IF820", text):
        info["modul"] = "IF820"
    found = re.search(r"/QFV[^\n]*?E=(\w+),S=(\w+),P=(\w+),H=(\w+)", text)
    if found:
        info["modul_firmware"] = _version_from_hex(found.group(1))      # E=01040C0C -> 1.4.12.12
        info["modul_stack"] = _version_from_hex(found.group(2))         # S=03010000 -> 3.1.0.0
        info["modul_protokoll"] = _version_from_hex(found.group(3))     # P=0104 -> 1.4
        info["modul_hardware"] = found.group(4)
    found = re.search(r"GBA[^\n]*?A=([0-9A-Fa-f]{12})", text)
    if found:
        raw_address = found.group(1).upper()
        info["adresse"] = ":".join(raw_address[index:index + 2] for index in range(0, 12, 2))
    for key, label in (("00", "name_ble"), ("01", "name_classic")):
        found = re.search(rf"GDN,T={key}[^\n]*?N=([^\s,]+)", text)
        if found:
            info[label] = found.group(1)
    found = re.search(r"GTU[^\n]*?B=(\w+)", text)
    if found:
        try:
            info["baudrate"] = int(found.group(1), 16)
        except ValueError:
            pass
    found = re.search(r"\.CYSPPGP[^\n]*?E=(\w+)", text)
    if found:
        info["cyspp_aktiv_modus"] = found.group(1)
    found = re.search(r"GBTP[^\n]*?T=(\w+)", text)
    if found:
        try:
            info["classic_supervision_s"] = int(found.group(1), 16) * 0.625 / 1000.0   # Einheit 0,625 ms
        except ValueError:
            pass
    found = re.search(r"GSBP[^\n]*?M=(\w+)", text)
    if found:
        info["sniff_modus"] = found.group(1)
    found = re.search(r"TX bytes=(\d+)", text)
    if found:
        info["zaehler_zeile"] = found.group(0)
    return info


async def read_bt_module_info(channel, timeout: float = 30.0) -> dict | None:
    """Hauptmenue -> 5 Funkmodems -> 1 Bluetooth Modem -> 1 Information -> zurueck zum Hauptmenue (nur ueber RS232/USB:
    das Bluetooth-Menue ist bei aktiver BT-Verbindung gesperrt). Liefert die geparsten Moduldaten, {'nicht_zugaenglich': ...}
    oder None, wenn das Hauptmenue nicht erreichbar war."""
    rx = channel.rx
    rx.clear()
    await channel.write(b"H")
    if not await rx.wait_for_marker(MARK_MAIN_RANGE, 8.0):
        return None
    result: dict | None = None
    await asyncio.sleep(0.3)
    rx.clear()
    await channel.write(b"5")                                          # 5 = Funkmodems
    if await rx.wait_for_any_marker(("1...3?", "1...4?"), 6.0):
        await asyncio.sleep(0.3)
        rx.clear()
        await channel.write(b"1")                                      # 1 = Bluetooth Modem
        if await rx.wait_for_any_marker(("1...3?",), 6.0):
            await asyncio.sleep(0.3)
            rx.clear()
            await channel.write(b"1")                                  # 1 = Information (ca. 8-10 s, Modembefehle)
            if await rx.wait_for_marker("TX bytes=", timeout):
                await rx.wait_for_marker("1...3?", 6.0)                # Menue-Prompt nach der Ausgabe
                await asyncio.sleep(0.2)
                result = parse_bt_info(bytes(rx.buf).decode("cp1252", errors="replace"))
        else:
            result = {"nicht_zugaenglich": "Bluetooth-Menue nicht erreichbar (aktive BT-Verbindung?)"}
    await asyncio.sleep(0.3)
    rx.clear()
    await channel.write(b"H")                                          # zurueck ins Hauptmenue
    await rx.wait_for_marker(MARK_MAIN_RANGE, 8.0)
    return result


USED_PORTS: dict = {}                  # vom TEST tatsaechlich benutzte Ports/Kanaele -> wofuer (fuer Log, JSON und Bericht)


def register_port(port: str, purpose: str) -> None:
    """Merkt, welchen Port/Kanal der Test benutzt hat und wofuer (spaetere Eintraege ueberschreiben, z. B. mit Seriennummer)."""
    USED_PORTS[str(port)] = purpose


def log_used_ports() -> None:
    """Am Laufende: welche Ports hat der Test wofuer benutzt (einmal ins Log und in den Trace)."""
    if not USED_PORTS:
        return
    log("Vom Test benutzte Ports:")
    for port, purpose in USED_PORTS.items():
        log(f"  {port:24} {purpose}")
    trace_event("ports_used", ports=dict(USED_PORTS))


COM_PORTS: list = []                   # Uebersicht ALLER COM-Ports des PCs (nur fuer --mode info; im Alltag comports.bat)
_BT_NAMES: dict = {}                   # Bluetooth-Adresse (12 Hex) -> Windows-Geraetename, einmal je Lauf geladen
USB_ROLES = {(0x1FC9, 0x80F7): "USB-Anschluss Mainboard (LPC1766)", (0x067B, 0x23A3): "USB-RS232-Adapter (Prolific)"}


def _load_bt_names() -> None:
    """Windows-Namen der gekoppelten Bluetooth-Geraete (Adresse -> Name) per PowerShell, nur lesend; Fehler werden ignoriert."""
    if _BT_NAMES:
        return
    script = ("[Console]::OutputEncoding=[Text.Encoding]::UTF8; Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue | "
              "Where-Object { $_.InstanceId -match 'DEV_[0-9A-F]{12}' } | ForEach-Object { $_.InstanceId + '|' + $_.FriendlyName }")
    try:
        done = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                              capture_output=True, timeout=30)
        for line in done.stdout.decode("utf-8", "replace").splitlines():
            found = re.search(r"DEV_([0-9A-F]{12}).*\|(.+)$", line.strip())
            if found:
                _BT_NAMES[found.group(1)] = found.group(2).strip()
    except Exception:
        pass


def build_com_overview() -> list:
    """Welcher COM-Port ist womit verbunden? Quellen: pyserial (USB-Kennung, Bluetooth-Adresse im Hardware-Pfad) und die
    identifizierten Geraete (DEVICE_INFO: Terminal-Port und Bluetooth-Adresse des Moduls). Ergebnis in COM_PORTS."""
    COM_PORTS.clear()
    if serial is None:
        return COM_PORTS
    from serial.tools import list_ports
    _load_bt_names()
    by_address = {str((info.get("bt_modul") or {}).get("adresse", "")).replace(":", "").upper(): label
                  for label, info in DEVICE_INFO.items() if (info.get("bt_modul") or {}).get("adresse")}
    by_terminal = {info.get("terminal_port"): label for label, info in DEVICE_INFO.items() if info.get("terminal_port")}

    def device_text(label: str) -> str:
        info = DEVICE_INFO.get(label, {})
        return f"Board {info.get('seriennummer', label)}"

    ports = sorted(list_ports.comports(), key=lambda port: int(re.sub(r"\D", "", port.device) or 0))
    unused = []
    for port in ports:
        entry = {"port": port.device, "art": port.description.split("(")[0].strip(), "zuordnung": ""}
        hwid = port.hwid or ""
        spp = re.search(r"&([0-9A-F]{12})_C00000000", hwid)
        if "BTHENUM" in hwid and "LOCALMFG" in hwid:
            unused.append(port.device)
            continue
        if spp:                                              # Bluetooth-SPP-Port eines gekoppelten Geraets (Classic)
            address = spp.group(1)
            entry["art"] = "Bluetooth Classic (SPP)"
            entry["bt_adresse"] = address
            if address in by_address:
                entry["zuordnung"] = f"{device_text(by_address[address])} ({_BT_NAMES.get(address, address)})"
            else:
                entry["zuordnung"] = _BT_NAMES.get(address, f"Bluetooth-Geraet {address}")
        elif (port.vid, port.pid) in USB_ROLES:
            entry["art"] = USB_ROLES[(port.vid, port.pid)]
            if port.device in by_terminal:
                kind = "RS232" if (port.vid, port.pid) == (0x067B, 0x23A3) else "USB"
                entry["zuordnung"] = f"{kind}-Terminal von {device_text(by_terminal[port.device])} (Mitschnitt/Geraeteinfo)"
            else:
                entry["zuordnung"] = "nicht belegt/ unbekannt"
        else:
            entry["zuordnung"] = port.description
        COM_PORTS.append(entry)
    if unused:
        COM_PORTS.append({"port": ", ".join(unused), "art": "Bluetooth (ausgehend)", "zuordnung": "ungenutzt"})
    return COM_PORTS


def log_com_overview() -> None:
    """Uebersicht ins Log und in den Trace."""
    build_com_overview()
    if not COM_PORTS:
        return
    log("COM-Ports:")
    for entry in COM_PORTS:
        log(f"  {entry['port']:18} {entry['art']:36} {entry['zuordnung']}")
    trace_event("com_ports", ports=COM_PORTS)


_WARNED_ADDRESSES: set = set()


def check_target_address(address, transport: str) -> None:
    """Prueft, ob die Bluetooth-Adresse des Verbindungsziels zu einem identifizierten Geraet passt (Adresse aus dem
    Bluetooth-Modul, gelesen ueber RS232/USB). Verhindert, dass der Test ein anderes Board anspricht als dasjenige,
    dessen RS232-/USB-Ausgabe mitgeschnitten wird. Ohne identifizierte Module passiert nichts."""
    known = {str((info.get("bt_modul") or {}).get("adresse", "")).replace(":", "").upper(): label
             for label, info in DEVICE_INFO.items() if (info.get("bt_modul") or {}).get("adresse")}
    if not known or not address:
        return
    normalized = str(address).replace(":", "").upper()
    if normalized in known:
        trace_event("target_address_ok", transport=transport, address=str(address), device=known[normalized])
        return
    if normalized in _WARNED_ADDRESSES:
        return
    _WARNED_ADDRESSES.add(normalized)
    trace_event("target_address_unknown", level="warning", transport=transport, address=str(address),
                known=sorted(known))
    log(f"WARNUNG: Bluetooth-Ziel {address} ({transport}) gehoert zu keinem der identifizierten Geraete "
        f"({', '.join(f'{label}={addr}' for addr, label in known.items())}) - Mitschnitt und Test beziehen sich "
        "womoeglich auf verschiedene Boards.")


async def identify_channel(channel, label: str, args, source: str, bt_module: bool = False) -> dict | None:
    """Liest die Info-Seite ueber den Kanal und registriert das Ergebnis (None, wenn das Menue nicht erreichbar war).
    bt_module=True liest zusaetzlich die Daten des Bluetooth-Moduls (nur ueber RS232/USB moeglich, ca. 10 s)."""
    try:
        info = await read_device_info(channel)
    except Exception as exc:           # Kommunikationsfehler duerfen den eigentlichen Test nie verhindern
        log(f"Geraeteinfo {label} ({source}) nicht lesbar: {exc}")
        trace_event("device_info_error", level="warning", label=label, source=source, error=str(exc))
        return None
    if info is None:
        log(f"Geraeteinfo {label} ({source}): Hauptmenue nicht erreichbar (Marker {MARK_MAIN_RANGE}).")
        trace_event("device_info_error", level="warning", label=label, source=source, error="kein Hauptmenue")
        return None
    if bt_module and any("bluetooth" in part.lower() for part in info.get("schnittstellen", [])) \
            and not getattr(args, "no_bt_module_info", False):
        try:
            module = await read_bt_module_info(channel)
        except Exception as exc:
            module = {"nicht_zugaenglich": f"Fehler: {exc}"}
        if module:
            info["bt_modul"] = module
    sniffer = getattr(channel, "sniffer", None)
    if sniffer is not None:
        info["terminal_port"] = sniffer.port                           # RS232/USB-Port, ueber den die Seite gelesen wurde
        register_port(sniffer.port, f"RS232-/USB-Terminal: Mitschnitt Board {info.get('seriennummer', '?')} "
                                    f"(Firmware {info.get('firmware', '?')}, Geraet {label})")
    _record_device_info(label, info, args, source)
    module = info.get("bt_modul") or {}
    if module and "nicht_zugaenglich" not in module:
        trace_event("bt_module_info", label=label, **{key: module.get(key) for key in (
            "modul", "modul_firmware", "modul_stack", "modul_protokoll", "modul_hardware", "adresse", "name_ble",
            "name_classic", "baudrate", "cyspp_aktiv_modus", "classic_supervision_s", "sniff_modus")})
        log(f"Bluetooth-Modul {label}: {module.get('modul', '?')} FW {module.get('modul_firmware', '?')} "
            f"(Stack {module.get('modul_stack', '?')}), Adresse {module.get('adresse', '?')}, "
            f"Namen {module.get('name_ble', '?')} / {module.get('name_classic', '?')}, "
            f"{module.get('baudrate', '?')} Baud, CYSPP-Modus {module.get('cyspp_aktiv_modus', '?')}.")
        wanted = getattr(args, "expect_module_fw", None)
        if wanted and module.get("modul_firmware") and module["modul_firmware"] != wanted:
            trace_event("bt_module_firmware_mismatch", level="warning", label=label,
                        found=module["modul_firmware"], expected=wanted)
            log(f"WARNUNG: Modul-Firmware {module['modul_firmware']} auf Geraet {label}, erwartet {wanted}.")
    elif module:
        log(f"Bluetooth-Modul {label}: {module['nicht_zugaenglich']}")
    return info


async def identify_via_bluetooth(args, label: str = "bt") -> dict | None:
    """Verbindet einmal ueber den gewaehlten Bluetooth-Transport, gibt ggf. die PIN ein und liest die Info-Seite."""
    transport = await make_transport(args)
    try:
        await transport.connect(args.connect_timeout)
        await authenticate_or_sync_main_menu(transport, args, "Geraeteinfo")
        return await identify_channel(transport, label, args, f"Bluetooth {args.transport}")
    except Exception as exc:
        log(f"Geraeteinfo ueber Bluetooth nicht lesbar: {exc}")
        trace_event("device_info_error", level="warning", label=label, source="bluetooth", error=str(exc))
        return None
    finally:
        await transport.disconnect()


async def preflight_identify(args) -> None:
    """Erster Schritt jedes Tests: Gerat identifizieren. --via auto: RS232/USB (Mitschnitt-Port), sonst Bluetooth."""
    via = getattr(args, "via", "auto")
    if via == "auto":
        via = "rs232" if SNIFFER is not None else "bt"
    if via == "rs232":
        if SNIFFER is None:
            log("Geraeteidentifikation uebersprungen: kein RS232-/USB-Port aktiv (--no-rs232).")
            return
        channel = SnifferChannel(SNIFFER)
        try:
            await identify_channel(channel, "A" if getattr(args, "devices", "") else "RS232", args,
                                   f"RS232/USB {SNIFFER.port}",
                                   bt_module=(getattr(args, "mode", None) == "info" or getattr(args, "bt_module_info", False)))
        finally:
            channel.close()
    else:
        await identify_via_bluetooth(args)


def report_device_info(args) -> int:
    """Modus info: Ergebnis der Geraeteidentifikation ausgeben und als device-info.json (mit Rohtext) ablegen."""
    print("\n" + "=" * 70 + "\n  GERAETEINFO\n" + "=" * 70)
    if not DEVICE_INFO:
        print("  Keine Geraeteinformation lesbar (siehe Log: Hauptmenue nicht erreichbar?).")
        return 1
    if PC_BT:
        print(f"  PC-Bluetooth: {PC_BT.get('name')} ({PC_BT.get('hersteller')}), Treiber {PC_BT.get('treiber')} vom "
              f"{PC_BT.get('datum')}, USB {PC_BT.get('usb_id')}, Windows-Stack {PC_BT.get('stack')}, Dienst {PC_BT.get('dienst')}")
    build_com_overview()                                # --mode info: komplette Uebersicht (im Alltag: comports.bat)
    if COM_PORTS:
        print("  COM-Ports:")
        for entry in COM_PORTS:
            print(f"    {entry['port']:18} {entry['art']:36} {entry['zuordnung']}")
    for label, info in DEVICE_INFO.items():
        print(f"  Geraet {label} (via {info.get('quelle')}):")
        for key in ("modell", "firmware", "hardware", "seriennummer", "betriebsstunden", "parametersatz",
                    "schnittstellen", "datum", "zeit", "zeitzone", "zeitquelle", "automatische_zeit", "uhr_abweichung_s"):
            if info.get(key) is not None:
                print(f"    {key:18}: {info[key]}")
        module = info.get("bt_modul") or {}
        if module:
            print("    Bluetooth-Modul:")
            for key in ("modul", "modul_firmware", "modul_stack", "modul_protokoll", "modul_hardware", "adresse",
                        "name_ble", "name_classic", "baudrate", "cyspp_aktiv_modus", "classic_supervision_s",
                        "sniff_modus", "nicht_zugaenglich"):
                if module.get(key) is not None:
                    print(f"      {key:22}: {module[key]}")
        print(f"    {'Info-Seite Rohtext':18}:")
        for line in info.get("raw", "").splitlines():
            if line.strip():
                print(f"      | {line.rstrip()}")
    json_path, _ = write_report("device-info", {"mode": "device-info",
                                                "started_at": datetime.now().astimezone().isoformat(timespec="seconds")})
    log(f"Bericht: {json_path}")
    return 0


def make_phase_runner(args, result: dict, label: str):
    """Liefert phase(name, transport, test, **overrides) fuer eigene Suiten (gleiche Logik wie in module-/fast-suite)."""
    async def phase(name, transport, test, **overrides):
        old_values = {key: getattr(args, key, None) for key in overrides}
        started_wall = time.time()
        started = time.perf_counter()
        if TRACE is not None:
            TRACE.set_context(phase=name, transport=transport, cycle=None)
        trace_event("phase_started", test=test.__name__)
        sniff_on = sniff_phase_begin(args, name, transport, len(result["phases"]) + 1)
        args.transport = transport
        for key, value in overrides.items():
            setattr(args, key, value)
        print(f"\n{'=' * 70}\n  {label}: {name}\n{'=' * 70}")
        try:
            code = await test(args)
            status = "passed" if code == 0 else "failed"
        except Exception as exc:
            log(f"FEHLER in Phase {name}: {exc}")
            status = "failed"
        finally:
            for key, value in old_values.items():
                setattr(args, key, value)
        sniffer = sniff_phase_end(args, sniff_on)
        status = classify_phase_status(status, started_wall)
        report_data = collect_phase_report(name, started_wall, len(result["phases"]) + 1)
        result["phases"].append({"name": name, "transport": transport, "port": (report_data or {}).get("port"),
                                 "result": status, "duration_s": round(time.perf_counter() - started, 3),
                                 "report": report_data, "sniffer": sniffer})
        if TRACE is not None:
            TRACE.set_context(cycle=None)
        trace_event("phase_finished", status=status, duration_s=round(time.perf_counter() - started, 3))
        return status
    return phase


def write_suite_report(kind: str, result: dict, title: str, subtitle: str) -> None:
    json_path, html_path = write_report(kind, result)
    html_path = html_path.with_suffix(".html")
    html_path.write_text(render_suite_html(
        result, title, subtitle,
        {"JSON": json_path.name, "Trace": TRACE.path.name if TRACE else "-", "HTML": html_path.name}),
        encoding="utf-8")
    log(f"Gesamtbericht JSON: {json_path}")
    log(f"Gesamtbericht HTML: {html_path}")


async def run_interleave(args) -> int:
    """Classic und BLE im strikten Wechsel (je Runde ein Classic- und ein BLE-Zyklus, Haltezeiten rotierend).

    Prueft, ob der Transportwechsel selbst Fehler ausloest (z. B. 'BLE ohne Daten direkt nach Classic'). Je Schritt wird
    der vorherige Transport mitprotokolliert; am Ende steht eine Erfolgsquote je Transport und je Uebergang."""
    holds = args.cycle_holds or [1.0, 2.0, 5.0, 10.0, 20.0]
    result = {"mode": "interleave", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "rounds": args.rounds, "cycle_holds": holds, "phases": []}
    if not await resolve_suite_target(args):
        result["result"] = "device_not_found"
        write_report("interleave", result)
        return 2
    phase = make_phase_runner(args, result, "WECHSEL")
    steps = []                      # (transport, vorheriger Transport, Status)
    previous = None
    failures = 0
    for round_no in range(1, args.rounds + 1):
        hold = holds[(round_no - 1) % len(holds)]
        for transport in ("classic", "ble"):
            name = f"Runde {round_no:03d} {transport} (Halten {hold:g} s, vorher {previous or '-'})"
            status = await phase(name, transport, run_cycle_test, cycle_holds=[hold])
            trace_event("interleave_step", level="info" if status == "passed" else "warning", round=round_no,
                        transport=transport, previous=previous, hold_s=hold, status=status)
            steps.append((transport, previous, status))
            previous = transport
            if status != "passed":
                failures += 1
        if args.max_failures and failures >= args.max_failures:
            log(f"Abbruch: {failures} fehlgeschlagene Schritte erreicht (--max-failures).")
            break
    summary = {}
    for transport, prev, status in steps:
        key = f"{transport} nach {prev or 'Start'}"
        entry = summary.setdefault(key, {"ok": 0, "fehler": 0})
        entry["ok" if status == "passed" else "fehler"] += 1
    result["summary"] = summary
    log("Erfolg je Uebergang:")
    for key, entry in sorted(summary.items()):
        log(f"  {key:22} ok {entry['ok']:4}   Fehler {entry['fehler']:4}")
    trace_event("interleave_summary", level="warning" if failures else "info", failures=failures, summary=summary)
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("interleave", result, "VIASIS Transportwechsel-Test",
                       f"Classic/BLE im Wechsel, {args.rounds} Runden, Haltezeiten {', '.join(f'{h:g}' for h in holds)} s.")
    return 0 if failures == 0 else 1


def _last_session_line(lines_after: int, sniffer=None) -> dict | None:
    """Letzte Zeile '[BT-Sitzung beendet] TX=.. ctswait=.. ctsDrop=.. TXstuck=.. lost_ms=..' aus den neuesten RS232-Zeilen."""
    sniffer = sniffer or SNIFFER
    if sniffer is None or lines_after <= 0:
        return None
    for text in reversed(sniffer.tail(min(lines_after, 60))):
        match = re.search(r"\[BT-Sitzung beendet\] TX=(\d+) ctswait=(\d+) ctsDrop=(\d+) TXstuck=(\d+) lost_ms=(\d+)", text)
        if match:
            keys = ("tx", "ctswait", "ctsdrop", "txstuck", "lost_ms")
            return dict(zip(keys, map(int, match.groups())))
    return None


USB_BOARD_VIDPID = {(0x1FC9, 0x80F7)}   # LPC1766-USB-CDC der Mainboards (an COM13 gemessen); ohne eigene Seriennummer


def find_usb_boards(exclude: set) -> list:
    """USB-COM-Ports von Mainboards (VID/PID des LPC-CDC), ohne die ausgeschlossenen Ports (z. B. den RS232-Adapter)."""
    if serial is None:
        return []
    from serial.tools import list_ports
    ports = [p.device for p in list_ports.comports()
             if (p.vid, p.pid) in USB_BOARD_VIDPID and p.device not in exclude]
    return sorted(ports, key=lambda name: (len(name), name))


async def verify_device_ports(devices: list, sniffer_a, sniffer_b) -> list:
    """Ordnet die Geraetenamen den Mitschnitt-Ports zu: je Geraet kurz per BLE verbinden und abonnieren, die Firmware gibt
    dann das Banner ('Viasis 3003M ... PIN =') auf ihrem Terminal aus - auf RS232 (A) oder USB (B). Liefert die
    Geraeteliste so sortiert, dass Eintrag 0 zu sniffer_a und Eintrag 1 zu sniffer_b gehoert (bei Mehrdeutigkeit
    unveraendert, mit Warnung)."""
    where = {}
    for device in devices[:2]:
        found, _ = await scan_for_device(device)
        if found is None:
            log(f"Portzuordnung: {device} per BLE nicht gefunden - Reihenfolge bleibt.")
            return devices
        before = {"A": sniffer_a.lines, "B": sniffer_b.lines}
        client = BleakClient(found.address)
        try:
            await client.connect(timeout=20.0)
            char = find_char_by_suffix(client, "ca102")
            log(f"Portzuordnung: {device} verbunden, Daten-Characteristic {'gefunden' if char is not None else 'NICHT gefunden'}.")
            if char is not None:
                await client.start_notify(char, lambda _sender, _data: None)

            def evidence(sniffer, key):
                new = sniffer.lines - before[key]
                return new > 0 and any("Viasis 3003M" in line or "PIN = " in line or "[BT-Event]" in line
                                       for line in sniffer.tail(min(new, 30)))

            hits = []
            for _ in range(20):                       # bis zu 10 s auf Banner oder Verbindungs-Event warten
                await asyncio.sleep(0.5)
                hits = [key for key, sniffer in (("A", sniffer_a), ("B", sniffer_b)) if evidence(sniffer, key)]
                if hits:
                    break
        finally:
            if client.is_connected:
                await client.disconnect()
        trace_event("port_assignment_probe", device=device, banner_on=hits)
        if len(hits) != 1:
            log(f"Portzuordnung: {device} nicht eindeutig (Banner auf {hits or 'keinem Port'}) - Reihenfolge bleibt.")
            return devices
        where[device] = hits[0]
        await asyncio.sleep(2.0)
    if where.get(devices[0]) == "B" and where.get(devices[1]) == "A":
        log(f"Portzuordnung: {devices[0]} haengt am USB-Port, {devices[1]} am RS232-Port - Reihenfolge getauscht.")
        return [devices[1], devices[0]] + devices[2:]
    if where.get(devices[0]) == "A" and where.get(devices[1]) == "B":
        log("Portzuordnung bestaetigt (erstes Geraet RS232, zweites USB).")
        return devices
    log(f"Portzuordnung widerspruechlich ({where}) - Reihenfolge bleibt.")
    return devices


async def run_soak(args) -> int:
    """Dauer-Messreihe: schreibt einmal Testdaten und laedt sie dann bis --soak-hours Stunden lang im Wechsel ueber BLE
    und Classic herunter (Pause --soak-pause-s dazwischen). Je Download eine CSV-Zeile (soak.csv): Rate, Retries,
    Langsam-Markierung, RSSI, PC-Laufzeit und die Sitzungszaehler der Firmware (ctswait, lost_ms, ctsDrop).
    Zeigt Verlangsamung ueber die Geraete-Laufzeit, Pufferlecks und Wechselfehler.

    Mit --devices NAME1,NAME2 laufen zwei (oder mehr) Geraete abwechselnd unter denselben PC-Bedingungen
    (Kontrollmessung: wird nur ein Geraet langsam -> Geraet/Modul, beide gleichzeitig -> PC/Funk). Der RS232-Mitschnitt
    gehoert zum ersten Geraet (--rs232-port); fuer das zweite --rs232-port-b (z. B. dessen USB-COM-Port)."""
    devices = [item.strip() for item in (args.devices or "").split(",") if item.strip()]
    multi = len(devices) >= 2
    if not multi:
        devices = [args.name]
    result = {"mode": "soak", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "soak_hours": args.soak_hours, "records": args.soak_records, "devices": devices, "phases": []}
    if not multi and not await resolve_suite_target(args):
        result["result"] = "device_not_found"
        write_report("soak", result)
        return 2

    sniffers = {devices[0]: SNIFFER}
    sniffer_b = None
    if multi:
        if SNIFFER is not None:
            SNIFFER.label = "A"
        port_b = args.rs232_port_b
        if port_b and port_b.lower() == "auto":      # USB-COM des zweiten Boards selbst finden
            usb_boards = find_usb_boards({args.rs232_port})
            if len(usb_boards) == 1:
                port_b = usb_boards[0]
                log(f"USB-Board automatisch erkannt: {port_b}.")
            else:
                log(f"WARNUNG: --rs232-port-b auto fand {len(usb_boards)} USB-Boards ({usb_boards}) - "
                    "bitte den Port mit --rs232-port-b COMxx angeben. Zweites Geraet laeuft ohne Mitschnitt.")
                port_b = None
        if port_b and serial is not None:
            sniffer_b = Rs232Sniffer(port_b, args.rs232_baud, label="B")
            try:
                sniffer_b.open()
                sniffer_b.start()
                register_port(port_b, "RS232-/USB-Terminal: Mitschnitt der Firmware-Ausgabe (Geraet B)")
                trace_event("rs232_sniffer_started", port=port_b, baud=args.rs232_baud, dev="B")
                log(f"Mitschnitt fuer das zweite Geraet auf {port_b} aktiv.")
            except (serial.SerialException, OSError) as exc:
                log(f"WARNUNG: {port_b} laesst sich nicht oeffnen ({exc}) - keine Sitzungszaehler fuer das zweite Geraet.")
                sniffer_b = None
        if sniffer_b is not None and SNIFFER is not None and not args.no_verify_ports:
            # 1. Wahl: Seriennummer aus der Info-Seite beider Geraete (RS232 fuer A liegt aus dem Preflight vor, USB fuer B
            # wird jetzt gelesen) gegen die Geraetenamen pruefen - eindeutig und ohne Bluetooth.
            channel_b = SnifferChannel(sniffer_b)
            try:
                await identify_channel(channel_b, "B", args, f"USB {sniffer_b.port}",
                                       bt_module=getattr(args, "bt_module_info", False))
            finally:
                channel_b.close()
            serial_a = (DEVICE_INFO.get("A") or {}).get("seriennummer")
            serial_b = (DEVICE_INFO.get("B") or {}).get("seriennummer")
            matched = False
            if serial_a and serial_b:
                if serial_a in devices[1] and serial_b in devices[0]:
                    log(f"Portzuordnung per Seriennummer: {devices[0]} haengt am USB-Port ({serial_b}), "
                        f"{devices[1]} am RS232-Port ({serial_a}) - Reihenfolge getauscht.")
                    devices = [devices[1], devices[0]] + devices[2:]
                    matched = True
                elif serial_a in devices[0] and serial_b in devices[1]:
                    log(f"Portzuordnung per Seriennummer bestaetigt ({serial_a} RS232, {serial_b} USB).")
                    matched = True
                else:
                    log(f"WARNUNG: Seriennummern {serial_a}/{serial_b} passen nicht zu den Namen {devices[:2]}.")
            if not matched:                                # 2. Wahl: Banner beim Bluetooth-Connect (bisheriges Verfahren)
                devices = await verify_device_ports(devices, SNIFFER, sniffer_b)
            result["devices"] = devices
        if sniffer_b is not None:
            sniffers[devices[1]] = sniffer_b
        sniffers[devices[0]] = SNIFFER
    state = {device: {} for device in devices}      # je Geraet gemerkte Zieldaten (BLE-Adresse, Classic-Name/-Port)
    target_keys = ("address", "ble_device_name", "classic_device_name", "classic_port")

    def select_device(device: str) -> None:
        """Zieldaten des Geraets in args setzen; unbekannte Felder entfernen, damit neu gesucht wird."""
        if not multi:
            return
        args.name = device
        for key in target_keys:
            if key in state[device]:
                setattr(args, key, state[device][key])
            elif hasattr(args, key):
                delattr(args, key)
        if "address" not in state[device]:
            args.address = None

    def remember_device(device: str) -> None:
        if multi:
            for key in target_keys:
                if getattr(args, key, None):
                    state[device][key] = getattr(args, key)

    phase = make_phase_runner(args, result, "DAUERREIHE")
    try:
        if not args.soak_skip_write:
            for device in list(devices):
                select_device(device)
                written = await phase(f"Flash schreiben ({device})", "ble", run_download_test, write_only=True,
                                      records=args.soak_records) == "passed"
                remember_device(device)
                if not written and multi and len(devices) > 1:
                    log(f"WARNUNG: Flash-Schreiben auf {device} fehlgeschlagen - Geraet wird aus der Reihe genommen, "
                        "die Messreihe laeuft mit den uebrigen weiter (kein Vergleich moeglich).")
                    result.setdefault("ausgefallen", []).append(device)
                    trace_event("soak_device_dropped", level="warning", device=device)
                    devices.remove(device)
                    continue
                if not written:
                    log(f"Flash-Schreiben fehlgeschlagen ({device}) - Dauerreihe abgebrochen.")
                    result["result"] = "failed"
                    write_suite_report("soak", result, "VIASIS Dauer-Messreihe", "Flash-Schreiben fehlgeschlagen.")
                    return 1
        def soak_csv():
            # Pfad jedes Mal neu bestimmen: Der Report-Ordner heisst anfangs "..._unbekannt_soak" und wird beim
            # ersten Geraetekontakt umbenannt (Inhalt wandert mit). Mit --soak-skip-write passiert das erst im
            # ersten Download - ein einmal gemerkter Pfad zeigte danach ins Leere (FileNotFoundError, 2026-11-13).
            return (RUN_REPORT_DIR or REPORT_DIR) / "soak.csv"
        csv_path = soak_csv()
        columns = ["zeit", "verstrichen_h", "pc_laufzeit_h", "nr", "geraet", "transport", "status", "bytes",
                   "dauer_s", "bps", "retries", "langsam", "rssi", "ctswait", "lost_ms", "ctsdrop", "txstuck"]
        csv_path.write_text(";".join(columns) + "\n", encoding="utf-8")
        t_begin = time.time()
        deadline = t_begin + args.soak_hours * 3600.0
        combos = [(device, transport) for device in devices for transport in ("ble", "classic")]
        number = 0
        failures = 0
        stats: dict = {}
        while time.time() < deadline:
            device, transport = combos[number % len(combos)]
            number += 1
            select_device(device)
            sniffer = sniffers.get(device)
            lines_before = sniffer.lines if sniffer is not None else 0
            name = f"Soak {number:03d} {transport}" + (f" ({device})" if multi else "")
            status = await phase(name, transport, run_receive_test)
            remember_device(device)
            await asyncio.sleep(args.soak_settle_s)   # die Firmware meldet die Sitzungszeile erst nach dem Trennen
            lines_after = (sniffer.lines - lines_before) if sniffer is not None else 0
            session = _last_session_line(lines_after, sniffer) or {}
            report = result["phases"][-1].get("report") or {}
            ym = report.get("ymodem") or {}
            uptime = pc_uptime_s()
            row = [datetime.now().strftime("%Y-%m-%d %H:%M:%S"), f"{(time.time() - t_begin) / 3600:.3f}",
                   f"{uptime / 3600:.2f}" if uptime is not None else "", number, device, transport, status,
                   ym.get("bytes", ""), ym.get("duration_s", ""), ym.get("throughput_bps", ""),
                   ym.get("retries", ""), int(bool(ym.get("slow"))) if ym else "",
                   BLE_INFO.get("rssi", "") if transport == "ble" else "", session.get("ctswait", ""),
                   session.get("lost_ms", ""), session.get("ctsdrop", ""), session.get("txstuck", "")]
            csv_path = soak_csv()
            with csv_path.open("a", encoding="utf-8") as handle:
                handle.write(";".join(str(item) for item in row) + "\n")
            entry = stats.setdefault((device, transport), {"n": 0, "fehler": 0, "bps": []})
            entry["n"] += 1
            if status == "passed" and ym.get("throughput_bps"):
                entry["bps"].append(ym["throughput_bps"])
            elif status != "passed":
                entry["fehler"] += 1
            trace_event("soak_sample", level="info" if status == "passed" else "warning", nr=number, device=device,
                        transport=transport, status=status, bps=ym.get("throughput_bps"),
                        ctswait=session.get("ctswait"), lost_ms=session.get("lost_ms"))
            if status != "passed":
                failures += 1
                if args.max_failures and failures >= args.max_failures:
                    log(f"Abbruch: {failures} fehlgeschlagene Downloads erreicht (--max-failures).")
                    break
            if time.time() + args.soak_pause_s < deadline:
                await asyncio.sleep(args.soak_pause_s)
        summary = {}
        log("Auswertung je Geraet/Transport:")
        for (device, transport), entry in sorted(stats.items()):
            rates = entry["bps"]
            line = {"n": entry["n"], "fehler": entry["fehler"],
                    "bps_mittel": round(sum(rates) / len(rates)) if rates else None,
                    "bps_min": round(min(rates)) if rates else None, "bps_max": round(max(rates)) if rates else None}
            summary[f"{device} {transport}"] = line
            log(f"  {device:20} {transport:8} n={line['n']:3} Fehler={line['fehler']:2} "
                f"B/s mittel/min/max = {line['bps_mittel']}/{line['bps_min']}/{line['bps_max']}")
        result["summary"] = summary
        trace_event("soak_summary", level="warning" if failures else "info", failures=failures, summary=summary)
        pc_events_export()
        result["samples"] = number
        result["result"] = "passed" if failures == 0 else "failed"
        log(f"Dauerreihe beendet: {number} Downloads, {failures} fehlgeschlagen, CSV: {csv_path}")
        write_suite_report("soak", result, "VIASIS Dauer-Messreihe",
                           f"{args.soak_hours:g} h, {', '.join(devices)}, Wechsel BLE/Classic, {args.soak_records} "
                           f"Messwerte je Download, Pause {args.soak_pause_s:g} s.")
        return 0 if failures == 0 else 1
    finally:
        if sniffer_b is not None:
            sniffer_b.stop()


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
        sniff_on = sniff_phase_begin(args, name, transport, len(result["phases"]) + 1)
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
        sniffer = sniff_phase_end(args, sniff_on)
        status = classify_phase_status(status, started_wall)
        report_data = collect_phase_report(name, started_wall, len(result["phases"]) + 1)
        result["phases"].append({"name": name, "transport": transport, "port": (report_data or {}).get("port"), "result": status,
                                 "duration_s": round(time.perf_counter() - started, 3), "report": report_data, "sniffer": sniffer})
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

    pc_events_export()   # PC-Ereignisse (Standby, Bluetooth/USB) vor dem Bericht ins Trace, damit sie im HTML erscheinen
    result["result"] = suite_result(result["phases"])
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
        sniff_on = sniff_phase_begin(args, name, transport, len(result["phases"]) + 1)
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
        sniffer = sniff_phase_end(args, sniff_on)
        status = classify_phase_status(status, started_wall)
        report_data = collect_phase_report(name, started_wall, len(result["phases"]) + 1)
        result["phases"].append({"name": name, "transport": transport, "port": (report_data or {}).get("port"), "result": status,
                                 "duration_s": round(time.perf_counter() - started, 3), "report": report_data, "sniffer": sniffer})
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

    pc_events_export()   # PC-Ereignisse (Standby, Bluetooth/USB) vor dem Bericht ins Trace, damit sie im HTML erscheinen
    result["result"] = suite_result(result["phases"])
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
# Strom-/Neustart-Tests mit USB-Relais (power-sweep, boot-race, reboot-check), 2026-10-30
# Plan: Doku/Testplan_BT_Erweiterung_Strom_Grenzfaelle.md (P1, P2, P4). Relais: Doku/tools/relay_power_Anleitung.md.
# Der RS232-Mitschnitt muss ueber einen externen Adapter laufen (nicht den USB-Port des Boards), sonst verschwindet
# der Port beim Stromlos-Schalten.
# ---------------------------------------------------------------------------

RELAY = None                          # PowerRelay; wird bei Programmende immer auf "Strom an" gesetzt
BOOT_BANNER_RE = re.compile(r"Version \d+\.\d+\s*$")   # Einschaltmeldung; die Info-Seite haengt ", V2" an (kein Treffer)
DTC_RE = re.compile(r"\b[FWI]1\d{4}\b")


class PowerRelay:
    """Devantech USB-RLY16L: Relais angezogen = Geraet stromlos (NC-Kontakt), siehe relay_power.py."""

    def __init__(self, port: str, relay: int):
        self.port = port
        self.relay = relay
        self.ser = serial.Serial(port, 19200, timeout=1)

    def _set(self, energized: bool) -> None:
        self.ser.write(bytes([(101 if energized else 111) + self.relay - 1]))
        self.ser.flush()

    def powered(self) -> bool | None:
        self.ser.reset_input_buffer()
        self.ser.write(bytes([0x5B]))
        reply = self.ser.read(1)
        return None if not reply else not (reply[0] & (1 << (self.relay - 1)))

    def cycle(self, off_s: float) -> float:
        """Strom off_s Sekunden weg; liefert die tatsaechliche Stromlos-Zeit (Befehl aus bis Befehl ein)."""
        started = time.perf_counter()
        self._set(True)
        try:
            while (remaining := off_s - (time.perf_counter() - started)) > 0:
                time.sleep(min(remaining, 0.01))
        finally:
            self._set(False)
        return time.perf_counter() - started

    def close(self) -> None:
        try:
            self._set(False)
            self.ser.close()
        except (serial.SerialException, OSError):
            pass


def rs232_capture_start() -> list | None:
    if SNIFFER is None:
        return None
    with SNIFFER.lock:
        SNIFFER.capture = []
        return SNIFFER.capture


def rs232_capture_stop() -> None:
    if SNIFFER is not None:
        with SNIFFER.lock:
            SNIFFER.capture = None


async def wait_capture(capture: list | None, pattern: re.Pattern, t0: float, timeout: float) -> float | None:
    """Sekunden ab t0 bis zur ersten RS232-Zeile mit pattern (None = nicht gesehen)."""
    if capture is None:
        return None
    deadline = t0 + timeout
    seen = 0
    while True:                                   # mindestens ein Durchlauf, auch wenn die Frist schon vorbei ist
        for when, text in capture[seen:]:
            if when >= t0 and pattern.search(text):
                return round(when - t0, 2)
        seen = len(capture)
        if time.monotonic() >= deadline:
            return None
        await asyncio.sleep(0.1)


async def wait_boot(args, capture: list | None, t_on: float) -> dict:
    """Nach dem Einschalten: Banner abwarten, dann bis die RS232-Ausgabe boot_quiet s ruhig ist (BT-Init fertig)."""
    banner = await wait_capture(capture, BOOT_BANNER_RE, t_on, args.boot_timeout)
    if capture is None or banner is None:
        await asyncio.sleep(max(0.0, args.boot_timeout - (time.monotonic() - t_on)))
    else:
        last = len(capture)
        quiet_since = time.monotonic()
        while time.monotonic() - t_on < args.boot_timeout:
            await asyncio.sleep(0.2)
            if len(capture) != last:
                last = len(capture)
                quiet_since = time.monotonic()
            elif time.monotonic() - quiet_since >= args.boot_quiet:
                break
    lines = [text for _, text in (capture or [])]
    joined = "\n".join(lines)
    done = next((round(when - t_on, 2) for when, text in (capture or []) if "BT init done" in text), None)
    info = {"banner_s": banner, "boot_s": round(time.monotonic() - t_on, 2),
            "bt_init_seen": "BT init" in joined, "bt_init_done_s": done,
            "dtc": sorted(set(DTC_RE.findall(joined))), "rs232_lines": len(lines)}
    log(f"Hochlauf: Banner {'nach ' + str(banner) + ' s' if banner is not None else 'NICHT gesehen'}, "
        f"BT-Init {'fertig nach ' + str(done) + ' s' if done is not None else ('gesehen' if info['bt_init_seen'] else 'nicht gesehen')}"
        f"{', Codes ' + ', '.join(info['dtc']) if info['dtc'] else ''}.")
    return info


def count_bt_events(capture: list | None, since: float) -> int | None:
    if capture is None:
        return None
    return sum(1 for when, text in capture if when >= since and "[BT-Event]" in text)


async def resolve_power_target(args) -> bool:
    """BLE-Adresse einmal per Scan bestimmen (die Schritte verbinden danach direkt, ohne 15-s-Scan); Classic-Name
    = BLE-Name ohne '_BLE'."""
    if not args.address:
        device, _ = await scan_for_device(args.name)
        if device is None:
            return False
        args.address = device.address
        args.ble_device_name = device.name
        args.name = re.sub(r"_BLE$", "", device.name, flags=re.IGNORECASE)
    log(f"Zielgeraet: {getattr(args, 'ble_device_name', None) or args.name} ({args.address}), Classic-Name '{args.name}'.")
    return True


async def run_smoke_single(args) -> int:
    """Ein Smoke-Versuch ohne die automatische Wiederholung (fuer boot-race: der Erstversuch zaehlt)."""
    return await _run_smoke_once(args)


async def bt_step_checks(args, phase, label: str, step: dict, capture, classic: bool, ble_count: int = 1) -> bool:
    """BLE-Smoke (ble_count Mal) und optional Classic-Smoke; Ergebnisse und [BT-Event]-Zahl in step. True = alles bestanden."""
    since = time.monotonic()
    classic = classic and not args.no_classic
    step["ble"] = []
    t_ble = time.monotonic()
    for n in range(1, ble_count + 1):
        t_ble = time.monotonic()
        step["ble"].append(await phase(f"{label} BLE {n}", "ble", run_smoke_test, hold=args.step_hold))
    if classic and ble_count > 0:
        # 2026-11-04: Das Modul meldet das Ende der BLE-Sitzung erst ca. 3 s nach dem Trennen durch Windows; ein
        # Classic-Verbindungsaufbau in dieser Zeit scheitert (COM6: Zeitlimit der Semaphore, OSError 121). Warten,
        # bis die Firmware "[BT-Sitzung beendet]" meldet (max. 10 s), dann noch 1 s.
        gone = await wait_capture(capture, re.compile(r"\[BT-Sitzung beendet\]"), t_ble, 10.0)
        log(f"BLE-Sitzungsende auf RS232 {'nach ' + str(gone) + ' s' if gone is not None else 'nicht gesehen'} "
            f"(ab Start BLE-Versuch), Classic folgt.")
        await asyncio.sleep(1.0)
    if classic:
        step["classic"] = await phase(f"{label} Classic", "classic", run_classic_smoke_test, hold=args.step_hold)
    step["bt_events"] = count_bt_events(capture, since)
    ok = all(status == "passed" for status in step["ble"]) and step.get("classic", "passed") == "passed"
    step["result"] = "passed" if ok else "failed"
    trace_event("power_step", level="info" if ok else "warning", **{k: v for k, v in step.items() if k != "label"},
                label=label)
    return ok


def power_summary(result: dict, key: str) -> None:
    """Erfolg je Wert von key (z. B. Stromlos-Zeit) ins Log und nach result['summary']."""
    summary = {}
    for step in result["steps"]:
        if step.get("result") == "skipped":
            continue
        entry = summary.setdefault(str(step[key]), {"ok": 0, "fehler": 0})
        entry["ok" if step["result"] == "passed" else "fehler"] += 1
    result["summary"] = summary
    log(f"Erfolg je {key}:")
    for value, entry in summary.items():
        log(f"  {value:>10}   ok {entry['ok']:3}   Fehler {entry['fehler']:3}")


async def run_power_sweep(args) -> int:
    """P2: Strom fuer verschiedene Zeiten wegschalten (je --repeat Mal), nach jedem Hochlauf BLE-Smoke und jedes
    --classic-every-te Mal Classic-Smoke. Zeigt, bei welchen Stromlos-Zeiten LPC und IF820 nicht sauber gemeinsam
    starten (kurze Unterbrechungen enden ueber EINT0 -> while(1) evtl. im Watchdog-Reset ohne Modul-Reset)."""
    off_times = args.off_times or [0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0]
    result = {"mode": "power-sweep", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "off_times_s": off_times, "repeat": args.repeat, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("power-sweep", result)
        return 2
    phase = make_phase_runner(args, result, "STROM")
    failures = 0
    number = 0
    for off_s in off_times:
        for rep in range(1, args.repeat + 1):
            number += 1
            label = f"#{number:03d} aus {off_s:g} s ({rep}/{args.repeat})"
            print(f"\n{'#' * 70}\n  POWER-SWEEP {label}\n{'#' * 70}")
            capture = rs232_capture_start()
            actual = await asyncio.to_thread(RELAY.cycle, off_s)
            t_on = time.monotonic()
            log(f"Strom {actual:.3f} s weg, wieder an.")
            step = {"label": label, "off_s": off_s, "off_actual_s": round(actual, 3), "repeat": rep}
            step.update(await wait_boot(args, capture, t_on))
            classic = bool(args.classic_every) and number % args.classic_every == 0
            if not await bt_step_checks(args, phase, label, step, capture, classic):
                failures += 1
            rs232_capture_stop()
            result["steps"].append(step)
            if args.max_failures and failures >= args.max_failures:
                log(f"Abbruch: {failures} fehlgeschlagene Schritte (--max-failures).")
                break
        else:
            continue
        break
    power_summary(result, "off_s")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("power-sweep", result, "VIASIS Stromunterbrechungs-Test",
                       f"Stromlos-Zeiten {', '.join(f'{t:g}' for t in off_times)} s, je {args.repeat} Mal.")
    return 0 if failures == 0 else 1


async def run_boot_race(args) -> int:
    """P4: Stromzyklus, dann nach --race-delays Sekunden ab Einschalten per BLE verbinden (feste Adresse, ein Versuch).
    Scheitert der Erstversuch, nach --race-retry-wait ein Folgeversuch. Fehler ist nur, wenn auch der scheitert
    (Geraet bleibt unerreichbar)."""
    delays = args.race_delays or [1.0, 2.0, 3.0, 5.0, 8.0, 12.0]
    result = {"mode": "boot-race", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "delays_s": delays, "repeat": args.repeat, "off_s": args.race_off, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("boot-race", result)
        return 2
    phase = make_phase_runner(args, result, "BOOT-RACE")
    failures = 0
    number = 0
    for delay in delays:
        for rep in range(1, args.repeat + 1):
            number += 1
            label = f"#{number:03d} verbinden {delay:g} s nach Ein ({rep}/{args.repeat})"
            print(f"\n{'#' * 70}\n  BOOT-RACE {label}\n{'#' * 70}")
            capture = rs232_capture_start()
            await asyncio.to_thread(RELAY.cycle, args.race_off)
            t_on = time.monotonic()
            await asyncio.sleep(max(0.0, delay - (time.monotonic() - t_on)))
            step = {"label": label, "delay_s": delay, "repeat": rep}
            first = await phase(f"{label} Erstversuch", "ble", run_smoke_single, hold=args.step_hold)
            step["first"] = first
            step["first_started_s"] = delay
            if first != "passed":
                await asyncio.sleep(args.race_retry_wait)
                step["second"] = await phase(f"{label} Folgeversuch", "ble", run_smoke_single, hold=args.step_hold)
            banner = await wait_capture(capture, BOOT_BANNER_RE, t_on, 0.1)
            step["banner_s"] = banner
            step["bt_init_done_s"] = next((round(w - t_on, 2) for w, t in (capture or []) if "BT init done" in t), None)
            step["dtc"] = sorted(set(DTC_RE.findall("\n".join(t for _, t in (capture or [])))))
            step["bt_events"] = count_bt_events(capture, t_on)
            ok = first == "passed" or step.get("second") == "passed"
            step["result"] = "passed" if ok else "failed"
            step["first_attempt_ok"] = first == "passed"
            trace_event("boot_race_step", level="info" if ok else "warning",
                        **{k: v for k, v in step.items() if k != "label"}, label=label)
            log(f"Ergebnis: Erstversuch {first}, Folgeversuch {step.get('second', '-')}, "
                f"Banner {banner} s, BT-Init fertig {step['bt_init_done_s']} s.")
            rs232_capture_stop()
            result["steps"].append(step)
            if not ok:
                failures += 1
            if args.max_failures and failures >= args.max_failures:
                break
        else:
            continue
        break
    power_summary(result, "delay_s")
    first_ok = {}
    for step in result["steps"]:
        entry = first_ok.setdefault(str(step["delay_s"]), [0, 0])
        entry[0] += step["first_attempt_ok"]
        entry[1] += 1
    result["first_attempt"] = {key: f"{ok}/{total}" for key, (ok, total) in first_ok.items()}
    log("Erstversuch erfolgreich je Verzoegerung: " + ", ".join(f"{k} s: {v}" for k, v in result["first_attempt"].items()))
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("boot-race", result, "VIASIS Verbinden waehrend des Hochlaufs",
                       f"Verzoegerungen {', '.join(f'{d:g}' for d in delays)} s nach Einschalten, je {args.repeat} Mal, "
                       f"vorher {args.race_off:g} s stromlos.")
    return 0 if failures == 0 else 1


async def firmware_reboot(args) -> dict:
    """Neustart ohne Stromzyklus: dieselbe Firmware per flash_xmodem.py neu flashen (Weg FIRM_UP_RES in main.c).
    Der RS232-Mitschnitt gibt den Port dafuer kurz frei. ACHTUNG: das Geraet loescht dabei Protokoll und Messdaten."""
    if SNIFFER is not None:
        SNIFFER.stop()
    globals()["SNIFFER"] = None
    cmd = [sys.executable, str(Path(__file__).with_name("flash_xmodem.py")), "--port", args.rs232_port,
           "--bin", args.reboot_bin]
    log("Neustart per Firmware-Update: " + " ".join(cmd[1:]))
    completed = await asyncio.to_thread(subprocess.run, cmd, capture_output=True, text=True, timeout=600)
    tail = (completed.stdout or "").strip().splitlines()[-3:]
    for line in tail:
        log("  flash: " + line)
    version = re.search(r"Version (\d+\.\d+)", completed.stdout or "")
    await asyncio.to_thread(setup_rs232_sniffer, args)
    return {"flash_rc": completed.returncode, "version": version.group(1) if version else None, "flash_tail": tail}


async def run_reboot_check(args) -> int:
    """P1: Neustart auf verschiedenen Wegen, danach --checks BLE-Smokes und ein Classic-Smoke. Methoden (--reboot-methods):
    firmware = Firmware-Update ohne Stromzyklus (braucht --reboot-bin), dropout = sehr kurze Stromunterbrechung
    (--dropout, endet evtl. im Watchdog-Reset), power = normaler Stromzyklus (--power-off, Kontrolle)."""
    methods = [m.strip() for m in args.reboot_methods.split(",") if m.strip()]
    if "firmware" in methods and not args.reboot_bin:
        log("Methode 'firmware' braucht --reboot-bin (z. B. ..\\..\\sism.bin).")
        return 2
    result = {"mode": "reboot-check", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "methods": methods, "repeat": args.repeat, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("reboot-check", result)
        return 2
    phase = make_phase_runner(args, result, "NEUSTART")
    failures = 0
    number = 0
    for rep in range(1, args.repeat + 1):
        for method in methods:
            number += 1
            label = f"#{number:03d} {method} ({rep}/{args.repeat})"
            print(f"\n{'#' * 70}\n  REBOOT-CHECK {label}\n{'#' * 70}")
            step = {"label": label, "method": method, "repeat": rep}
            if method == "firmware":
                step.update(await firmware_reboot(args))
                capture = rs232_capture_start()
                await asyncio.sleep(args.boot_quiet)
            elif method in ("dropout", "power"):
                capture = rs232_capture_start()
                off_s = args.dropout if method == "dropout" else args.power_off
                step["off_actual_s"] = round(await asyncio.to_thread(RELAY.cycle, off_s), 3)
                step.update(await wait_boot(args, capture, time.monotonic()))
            else:
                log(f"Unbekannte Methode '{method}' - uebersprungen.")
                continue
            if not await bt_step_checks(args, phase, label, step, capture, classic=True, ble_count=args.checks):
                failures += 1
            rs232_capture_stop()
            result["steps"].append(step)
    power_summary(result, "method")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("reboot-check", result, "VIASIS BT nach Neustart",
                       f"Methoden {', '.join(methods)}, je {args.repeat} Mal; je {args.checks} BLE-Smoke und 1 Classic-Smoke.")
    return 0 if failures == 0 else 1


# ---------------------------------------------------------------------------
# Tests am Tisch ohne Handarbeit (nightly, gsm-mux, bt-timeout, bt-update), 2026-10-30
# Plan: Doku/Testplan_BT_Erweiterung_Strom_Grenzfaelle.md, Abschnitt 5.
# ---------------------------------------------------------------------------

MARK_TIME_PROMPT = "hh:mm:ss"                 # T_ftime, Eingabe der Uhrzeit (Information -> 4)
MARK_PROTO_ASK = "Daten ausgeben"             # T_daus, Abfrage vor der Protokollausgabe
MARK_WERET = "Weiter mit Return"              # T_weret, Seitenende der Protokollausgabe
MARK_RANGE_5 = "1...5?"                       # modem_com(): Baudrate
MARK_RANGE_4 = "1...4?"                       # modem_com(): BT/GSM/GPS
MARK_RANGE_8 = "1...8?"                       # werkmenue()
BT_DISCONNECT_TEXTS = ("Bluetoothverbindung getrennt", "Bluetooth disconnected")   # T_btdis (measure.c, BT_TIMEOUT)
XMODEM_SUB = 0x1A                             # Fuellbyte des letzten XMODEM-Blocks (uebrige XMODEM_* siehe oben)


def need_rs232(mode: str) -> bool:
    if SNIFFER is None:
        log(f"FEHLER: {mode} braucht den RS232-Mitschnitt (--rs232-port, externer Adapter).")
        return False
    return True


async def rs232_navigate(steps: list, timeout: float = 8.0) -> bool:
    """Menuenavigation ueber RS232 (gleiche Schrittliste wie navigate())."""
    channel = SnifferChannel(SNIFFER)
    try:
        return await navigate(channel, steps, timeout)
    finally:
        channel.close()


async def rs232_set_time(hhmmss: str) -> bool:
    """Hauptmenue -> 6 Information -> 4 Zeit -> hh:mm:ss -> 9 zurueck. Setzt timeupdate in der Firmware (neue Aufgaben)."""
    log(f"Stelle Geraeteuhr ueber RS232 auf {hhmmss} ...")
    ok = False
    for attempt in range(3):
        if attempt:
            # Direkt nach dem Ende einer BT-Sitzung wird das erste "H" auf RS232 manchmal verschluckt
            # (nightly 2026-11-05: '1...6?' nie gesehen) - kurz warten, mit ESC neu ansetzen.
            log(f"Uhr stellen: Menue nicht erreicht, Versuch {attempt + 1}/3 ...")
            await asyncio.sleep(2.0)
        ok = await rs232_navigate([(None, b"\x1bH" if attempt else b"H"), (MARK_MAIN_RANGE, b"6"),
                                   (MARK_INFO_RANGE, b"4"),
                                   (MARK_TIME_PROMPT, hhmmss.encode("ascii") + b"\r"), (MARK_INFO_RANGE, b"9")])
        if ok:
            break
    trace_event("device_clock_set", level="info" if ok else "warning", time=hhmmss, ok=ok)
    if not ok:
        log("Uhr stellen fehlgeschlagen (Menue nicht wie erwartet).")
    return ok


async def rs232_read_protocol() -> str | None:
    """Information -> 1 Protokoll -> j: erste Seite (neueste Eintraege zuerst, put_protocol()), dann abbrechen und zurueck."""
    channel = SnifferChannel(SNIFFER)
    try:
        if not await navigate(channel, [(None, b"H"), (MARK_MAIN_RANGE, b"6"), (MARK_INFO_RANGE, b"1"),
                                        (MARK_PROTO_ASK, b"j")], 8.0):
            return None
        await asyncio.sleep(0.5)
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline and MARK_WERET.encode() not in channel.rx.buf \
                and MARK_INFO_RANGE.encode() not in channel.rx.buf:
            await asyncio.sleep(0.2)
        text = bytes(channel.rx.buf).decode("cp1252", errors="replace")
        if MARK_WERET in text:
            channel.rx.clear()
            await channel.write(b"x")                     # keine weitere Seite
        # put_protocol() fragt danach "Daten loeschen (j/n)?" - immer ausdruecklich NEIN (beobachtet 2026-11-02)
        if await channel.rx.wait_for_marker(MARK_DELETE_DATA, 4.0) or MARK_DELETE_DATA in text:
            channel.rx.clear()
            await channel.write(b"n")
        await channel.rx.wait_for_marker(MARK_INFO_RANGE, 8.0)
        await channel.write(b"9")
        await channel.rx.wait_for_marker(MARK_MAIN_RANGE, 8.0)
        return text
    finally:
        channel.close()


async def run_nightly(args) -> int:
    """P1 im Originalweg: Uhr auf --nightly-set (Default 03:13:30) stellen, den Neustart um 3:15 (BOOTTASK, rtc.c) abwarten,
    im Geraeteprotokoll nachweisen, danach --checks BLE-Smokes und ein Classic-Smoke; zum Schluss Uhr auf PC-Zeit zurueck.
    Verdacht: Nach reboot() laeuft weder test_BT() noch init_bluetooth() (main.c, Zweig BOOT_RES leer), UART1 bleibt aus."""
    if not need_rs232("nightly"):
        return 2
    result = {"mode": "nightly", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "repeat": args.repeat, "set_time": args.nightly_set, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("nightly", result)
        return 2
    phase = make_phase_runner(args, result, "NACHT-NEUSTART")
    failures = 0
    for rep in range(1, args.repeat + 1):
        label = f"#{rep:02d} Neustart 3:15"
        print(f"\n{'#' * 70}\n  NIGHTLY {label}\n{'#' * 70}")
        step = {"label": label, "method": "nightly", "repeat": rep}
        capture = rs232_capture_start()
        step["clock_set"] = await rs232_set_time(args.nightly_set)
        t_set = time.monotonic()
        h, m, s = (int(x) for x in args.nightly_set.split(":"))
        until_reboot = (3 * 3600 + 15 * 60) - (h * 3600 + m * 60 + s)
        wait_s = max(0, until_reboot) + args.nightly_margin
        log(f"Warte {wait_s:.0f} s auf den Neustart um 3:15 ...")
        banner = await wait_capture(capture, BOOT_BANNER_RE, t_set, wait_s)
        if banner is not None:
            log(f"Banner {banner} s nach dem Uhrstellen gesehen.")
        await asyncio.sleep(max(0.0, wait_s - (time.monotonic() - t_set)))
        step["banner_after_s"] = banner
        protocol = await rs232_read_protocol()
        first = "\n".join((protocol or "").splitlines()[:12])
        step["reboot_logged"] = bool(protocol) and re.search(r"Reboot.*03:1[56]", first) is not None
        step["protocol_head"] = first
        log(f"Geraeteprotokoll: Reboot um 3:15 {'gefunden' if step['reboot_logged'] else 'NICHT gefunden'}.")
        ok = await bt_step_checks(args, phase, label, step, capture, classic=True, ble_count=args.checks)
        if not step["reboot_logged"]:
            step["result"] = "no_reboot"
            ok = False
        rs232_capture_stop()
        await rs232_set_time(datetime.now().strftime("%H:%M:%S"))
        if not ok and RELAY is not None:
            # --nightly-recover: Stromzyklus, damit der naechste Durchlauf von einem funktionierenden BT ausgeht
            capture = rs232_capture_start()
            await asyncio.to_thread(RELAY.cycle, args.power_off)
            await wait_boot(args, capture, time.monotonic())
            step["recover"] = await phase(f"{label} nach Stromzyklus", "ble", run_smoke_test, hold=args.step_hold)
            rs232_capture_stop()
            await rs232_set_time(datetime.now().strftime("%H:%M:%S"))
            log(f"Gegenprobe nach Stromzyklus: {step['recover']}.")
        result["steps"].append(step)
        if not ok:
            failures += 1
    power_summary(result, "method")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("nightly", result, "VIASIS BT nach dem naechtlichen Neustart (3:15)",
                       f"Uhr auf {args.nightly_set}, {args.repeat} Mal; je {args.checks} BLE-Smoke und 1 Classic-Smoke.")
    return 0 if failures == 0 else 1


async def run_gsm_mux(args) -> int:
    """P5 ohne SIM/Zeitplan (Board mit LTE-Modem): ueber RS232 Testmenue -> Strg+W -> 7 Modem-Kommunikation -> 9600 -> GSM.
    Der UART1-MUX steht dann auf GSM. Waehrend --gsm-hold ein BLE-Versuch (nur Information, die Hauptschleife steht im
    Modem-Terminal), dann Strg+Z, Werksmenue verlassen; danach --checks BLE-Smokes und ein Classic-Smoke."""
    if not need_rs232("gsm-mux"):
        return 2
    result = {"mode": "gsm-mux", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "repeat": args.repeat, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("gsm-mux", result)
        return 2
    phase = make_phase_runner(args, result, "GSM-MUX")
    failures = 0
    for rep in range(1, args.repeat + 1):
        label = f"#{rep:02d} Modem-Terminal GSM"
        print(f"\n{'#' * 70}\n  GSM-MUX {label}\n{'#' * 70}")
        step = {"label": label, "method": "gsm-mux", "repeat": rep}
        capture = rs232_capture_start()
        channel = SnifferChannel(SNIFFER)
        try:
            entered = await navigate(channel, [(None, b"H"), (MARK_MAIN_RANGE, b"3"), (MARK_TESTMENU, bytes([0x17])),
                                               (MARK_RANGE_8, b"7"),
                                               (MARK_RANGE_5, b"1" if args.gsm_baud == 9600 else b"2"),
                                               (MARK_RANGE_4, b"2")], 8.0)
            await asyncio.sleep(args.gsm_settle)                  # Modem einschalten, Startmeldungen
            text = bytes(channel.rx.buf).decode("cp1252", errors="replace")
            step["terminal_entered"] = entered
            step["modem_text"] = text[-300:]
            step["modem_error"] = bool(DTC_RE.search(text))
            log(f"Modem-Terminal {'offen' if entered and not step['modem_error'] else 'NICHT sauber geoeffnet'}.")
            if not entered or step["modem_error"]:
                # Modem antwortet nicht (z. B. F18001 auf 26SC9983, 2026-11-02): Firmware ist zurueck im Werksmenue,
                # der MUX stand nie dauerhaft auf GSM -> Schritt sagt nichts aus
                # Firmware verlaesst das Werksmenue nach F18001 schon selbst (Testmenue steht) -> kein '8' senden,
                # das waere Testmenue 8 = BT-Diagnose (wartet 10 min stumm, 2026-11-09). 'H' geht aus beiden Menues.
                await channel.write(b"H")
                await asyncio.sleep(1.0)
                await channel.write(b"H")
                step["main_menu_back"] = await channel.rx.wait_for_marker(MARK_MAIN_RANGE, 8.0)
                step["result"] = "skipped"
                log("Modem antwortet nicht - Schritt uebersprungen (kein Fehler des BT-Teils).")
                rs232_capture_stop()
                result["steps"].append(step)
                continue
            step["during"] = await phase(f"{label} BLE waehrend GSM", "ble", run_smoke_single, hold=args.step_hold)
            await asyncio.sleep(max(0.0, args.gsm_hold - args.gsm_settle))
            channel.rx.clear()
            await channel.write(bytes([0x1A]))                    # Strg+Z beendet modem_terminal()
            step["left_terminal"] = await channel.rx.wait_for_marker(MARK_RANGE_8, 15.0)
            # Nur 'H' senden, NICHT '8': Beim Verlassen des Modem-Terminals schaltet UART1 von GSM zurueck auf BT,
            # der Schnittstellenwechsel laesst getc() im Werksmenue sofort -1 liefern - das Werksmenue verlaesst sich
            # dann selbst. Ein '8' landete so im Testmenue (Punkt 8 = BT-Diagnose), das bis zu 20x30 s auf eine
            # Ziffer wartet und alles andere stumm verwirft (2026-11-09, 26SC9966: "RS232 stumm"). 'H' fuehrt aus
            # beiden Menues ins Hauptmenue (putmenu(): menu=0).
            await channel.write(b"H")
            await asyncio.sleep(1.0)
            await channel.write(b"H")
            step["main_menu_back"] = await channel.rx.wait_for_marker(MARK_MAIN_RANGE, 8.0)
        finally:
            channel.close()
        await asyncio.sleep(3.0)
        ok = await bt_step_checks(args, phase, label, step, capture, classic=True, ble_count=args.checks)
        if not step.get("main_menu_back"):
            log("WARNUNG: Hauptmenue nach dem Modem-Terminal nicht gesehen.")
        rs232_capture_stop()
        result["steps"].append(step)
        if not ok:
            failures += 1
    power_summary(result, "method")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    if result["steps"] and all(s.get("result") == "skipped" for s in result["steps"]):
        result["result"] = "skipped"
    write_suite_report("gsm-mux", result, "VIASIS BT nach UART1-MUX auf GSM",
                       f"Modem-Terminal (GSM) {args.gsm_hold:g} s offen, {args.repeat} Mal.")
    return 0 if failures == 0 else 1


MQTT_CONNECTED_RE = re.compile(r"MQTT Server connected")      # T_cmqtt (gsmio.c/mqtt.c)
MQTT_LEAVE_RE = re.compile(r"/leave")                         # send_leave() im MQTT-Debug (MQTT_fast_disconnect(1))
MQTT_REFUSED_RE = re.compile(r"connection refused")             # CONNACK abgelehnt (Serverseite), Firmware versucht es erneut
MQTT_LINK_BIT = 0x20                                          # connect & MQTT_LINK im Strg+F-Bericht ("Connect=..h")


async def rs232_debug_menu(channel) -> str | None:
    """Hauptmenue -> 5 Funkmodems -> 2 Mobilfunk -> Strg+G: GSM/GPS/MQTT-Debugmenue. Gibt den Menuetext oder None."""
    channel.rx.clear()
    if not await navigate(channel, [(None, b"H"), (MARK_MAIN_RANGE, b"5"), (MARK_RANGE_4, b"2"),
                                    (MARK_RANGE_8, bytes([0x07]))], 10.0):
        return None
    if not await channel.rx.wait_for_marker("1...9?", 8.0):
        return None
    text = bytes(channel.rx.buf).decode("cp1252", errors="replace")
    return text if "MQTT debug mode" in text else None


async def rs232_mqtt_debug(on: bool, disconnect: bool = False) -> bool | None:
    """MQTT-Debug (nur RAM, endet wenn das RS232-Terminal getrennt wird) ein-/ausschalten; optional danach Menuepunkt 8
    'Disconnect from Server'. Gibt den Zustand vor dem Umschalten (None = Menue nicht erreicht)."""
    channel = SnifferChannel(SNIFFER)
    try:
        text = await rs232_debug_menu(channel)
        if text is None:
            log("FEHLER: Debugmenue (Strg+G) nicht erreicht.")
            return None
        match = re.search(r"MQTT debug mode:\s*(\S+)", text)
        was_on = bool(match) and match.group(1).lower() in ("ein", "on")
        channel.rx.clear()
        await channel.write(b"4" if was_on != on else b"9")
        await asyncio.sleep(1.0)
        if disconnect:
            if await rs232_debug_menu(channel) is not None:
                await channel.write(b"8")
                await asyncio.sleep(5.0)
        await channel.write(b"H")
        await channel.rx.wait_for_marker(MARK_MAIN_RANGE, 15.0)
        return was_on
    finally:
        channel.close()


async def rs232_report() -> dict:
    """Strg+F (radiostatus()) im Hauptmenue: Connect-Bits, MQTT-State, CSQ.
    Bis zu 3 Versuche: das erste 'H' nach dem Ende einer BT-Sitzung geht oft verloren (wie beim Uhrstellen);
    ohne Wiederholung fehlte "Connect=" und online-bt wertete das als "MQTT NOCH VERBUNDEN" (2026-11-09)."""
    text = ""
    for attempt in range(3):
        channel = SnifferChannel(SNIFFER)
        try:
            channel.rx.clear()
            await channel.write(b"\x1bH" if attempt else b"H")
            await channel.rx.wait_for_marker(MARK_MAIN_RANGE, 15.0)
            channel.rx.clear()
            await channel.write(bytes([0x06]))
            await channel.rx.wait_for_marker("GPS:", 8.0)
            await asyncio.sleep(0.5)
            text = bytes(channel.rx.buf).decode("cp1252", errors="replace")
        finally:
            channel.close()
        if re.search(r"Connect=[0-9A-Fa-f]+h", text):
            break
        log(f"Statusabfrage (Strg+F) ohne Connect-Zeile, Versuch {attempt + 2}/3 ..." if attempt < 2
            else "Statusabfrage (Strg+F) dreimal ohne Connect-Zeile.")
        await asyncio.sleep(2.0)
    report = {}
    for key, pattern, base in (("connect", r"Connect=([0-9A-Fa-f]+)h", 16), ("mqtt_state", r"State=(\d+)", 10),
                               ("csq", r"CSQ=(\d+)", 10)):
        match = re.search(pattern, text)
        if match:
            report[key] = int(match.group(1), base)
    if "connect" in report:
        report["mqtt_link"] = bool(report["connect"] & MQTT_LINK_BIT)
    return report


async def run_online_bt(args) -> int:
    """5.134: BT-Verbindung, waehrend das Geraet online ist (MQTT). MQTT-Debug ueber RS232 an (Modem dauerhaft an), auf
    'MQTT Server connected' warten, dann BLE- bzw. Classic-Smoke (sichtbar, Banner, PIN). Danach muss MQTT beendet sein
    (BT hat Vorrang: communication_change() -> MQTT_fast_disconnect(1)) und das Geraet nach dem Trennen von selbst wieder
    online gehen (wait_min=3 -> ca. 3-5 min). Zum Schluss MQTT-Debug aus und vom Server trennen.
    Voraussetzung: Geraet eingerichtet (device_setup.py), RS232 bleibt waehrend des ganzen Laufs offen."""
    if not need_rs232("online-bt"):
        return 2
    result = {"mode": "online-bt", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "repeat": args.repeat, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("online-bt", result)
        return 2
    phase = make_phase_runner(args, result, "ONLINE-BT")
    transports = ["ble"] + ([] if args.no_classic else ["classic"])
    failures = 0
    capture = rs232_capture_start()
    t_on = time.monotonic()
    before = await rs232_mqtt_debug(True)
    if before is None:
        result["result"] = "failed"
        result["error"] = "Debugmenue nicht erreicht (Mobilfunk-Menue fehlt? Servertyp MQTT? device_setup.py --check)"
        rs232_capture_stop()
        write_report("online-bt", result)
        return 1
    log(f"MQTT-Debug an (vorher {'an' if before else 'aus'}) - warte bis {args.online_timeout:g} s auf die Serververbindung ...")
    online_s = await wait_capture(capture, MQTT_CONNECTED_RE, t_on, args.online_timeout)
    result["first_online_s"] = online_s
    result["refused_before_online"] = sum(1 for _, text in capture if MQTT_REFUSED_RE.search(text)) if capture else None
    if result["refused_before_online"]:
        log(f"Hinweis: Server hat {result['refused_before_online']} Verbindungsversuch(e) abgelehnt (Serverseite).")
    if online_s is None:
        log("FEHLER: Geraet geht nicht online - Einrichtung pruefen (device_setup.py --online). Kein BT-Fehler.")
        result["result"] = "not_online"
        await rs232_mqtt_debug(False, disconnect=True)
        rs232_capture_stop()
        write_suite_report("online-bt", result, "VIASIS BT waehrend MQTT online",
                           "Geraet ging nicht online - Test nicht durchfuehrbar.")
        return 1
    log(f"Online nach {online_s} s.")
    await asyncio.sleep(args.online_settle)                       # Join/Status/Parameter abwarten
    number = 0
    for rep in range(1, args.repeat + 1):
        for transport in transports:
            number += 1
            label = f"#{number:02d} {transport.upper()} waehrend online ({rep}/{args.repeat})"
            print(f"\n{'#' * 70}\n  ONLINE-BT {label}\n{'#' * 70}")
            step = {"label": label, "transport": transport, "repeat": rep}
            pre = await rs232_report()
            step["report_before"] = pre
            if not pre.get("mqtt_link"):
                # zwischen zwei Schritten noch nicht wieder online: warten (sonst prueft der Schritt nichts Neues)
                t_wait = time.monotonic()
                again = await wait_capture(capture, MQTT_CONNECTED_RE, t_wait, args.online_timeout)
                await asyncio.sleep(args.online_settle)
                pre = await rs232_report()
                step["report_before"] = pre
                step["waited_online_s"] = again
            if not pre.get("mqtt_link"):
                step["result"] = "not_online"
                log("Geraet vor dem BT-Schritt nicht online - Schritt nicht aussagekraeftig.")
                result["steps"].append(step)
                failures += 1
                continue
            t_bt = time.monotonic()
            test = run_smoke_test if transport == "ble" else run_classic_smoke_test
            step["smoke"] = await phase(label, transport, test, hold=args.step_hold)
            step["leave_seen"] = await wait_capture(capture, MQTT_LEAVE_RE, t_bt, 0) is not None
            post = await rs232_report()
            step["report_after"] = post
            step["mqtt_ended"] = post.get("mqtt_link") is False
            log(f"Nach der BT-Sitzung: MQTT {'beendet' if step['mqtt_ended'] else 'NOCH VERBUNDEN'}"
                f" (Connect={post.get('connect', 0):X}h, /leave {'gesehen' if step['leave_seen'] else 'nicht gesehen'}).")
            t_off = time.monotonic()
            step["reconnect_s"] = await wait_capture(capture, MQTT_CONNECTED_RE, t_off, args.online_reconnect)
            log(f"Wieder online: {str(step['reconnect_s']) + ' s nach der BT-Sitzung' if step['reconnect_s'] is not None else 'NEIN'}.")
            ok = step["smoke"] == "passed" and step["mqtt_ended"] and step["reconnect_s"] is not None
            step["result"] = "passed" if ok else "failed"
            trace_event("online_bt_step", level="info" if ok else "warning",
                        **{k: v for k, v in step.items() if k != "label"}, label=label)
            result["steps"].append(step)
            if not ok:
                failures += 1
            await asyncio.sleep(args.online_settle)
    await rs232_mqtt_debug(False, disconnect=True)
    log("MQTT-Debug aus, vom Server getrennt.")
    rs232_capture_stop()
    power_summary(result, "transport")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("online-bt", result, "VIASIS BT waehrend MQTT online",
                       f"MQTT-Debug, je Transport {args.repeat} Mal: BT verbinden, MQTT beendet, wieder online.")
    return 0 if failures == 0 else 1


async def run_bt_timeout(args) -> int:
    """BT_TIMEOUT (6 min ohne Empfang, measure.c): verbinden, PIN, dann nichts mehr senden. Erwartet: die Firmware trennt
    nach 360 bis 420 s selbst (bt_disconnect(), Meldung 'Bluetoothverbindung getrennt'). Danach muss das Geraet wieder
    verbindbar sein (BLE- und Classic-Smoke)."""
    result = {"mode": "bt-timeout", "transport": args.transport,
              "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "repeat": args.repeat, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("bt-timeout", result)
        return 2
    phase = make_phase_runner(args, result, "BT-TIMEOUT")
    failures = 0
    for rep in range(1, args.repeat + 1):
        label = f"#{rep:02d} {args.transport} ohne Eingabe"
        print(f"\n{'#' * 70}\n  BT-TIMEOUT {label}\n{'#' * 70}")
        step = {"label": label, "method": f"timeout-{args.transport}", "repeat": rep}
        capture = rs232_capture_start()
        transport = await make_transport(args)
        disconnected_s = None
        try:
            await transport.connect(args.connect_timeout)
            await authenticate_or_sync_main_menu(transport, args, "bt-timeout")
            t0 = time.monotonic()
            log(f"Verbunden, Hauptmenue erreicht. Warte bis zu {args.timeout_wait:.0f} s ohne Eingabe ...")
            while time.monotonic() - t0 < args.timeout_wait:
                await asyncio.sleep(1.0)
                texts = [text for _, text in (capture or [])]
                if not transport.is_connected or any(m in t for t in texts for m in BT_DISCONNECT_TEXTS):
                    disconnected_s = round(time.monotonic() - t0, 1)
                    break
        except Exception as exc:
            log(f"FEHLER beim Verbinden: {exc}")
            step["error"] = str(exc)
        finally:
            await transport.disconnect()
            if hasattr(transport, "close"):
                transport.close()
        step["disconnected_after_s"] = disconnected_s
        in_window = disconnected_s is not None and 300 <= disconnected_s <= 440
        log(f"Trennung durch das Geraet: {'nach ' + str(disconnected_s) + ' s' if disconnected_s else 'KEINE'}"
            f" ({'im erwarteten Fenster 360-420 s' if in_window else 'ausserhalb des Fensters'}).")
        # Nach dem Zeitlimit schaltet measure.c das Modul ab (PINMODE3 Pull-up) und erst beim naechsten Minutentakt
        # wieder ein -> bis zu 60 s nicht auffindbar (beobachtet 2026-11-02). Erst danach pruefen.
        log(f"Warte {args.timeout_recover:g} s (Modul nach dem Zeitlimit bis zu 1 min aus) ...")
        await asyncio.sleep(args.timeout_recover)
        ok = await bt_step_checks(args, phase, label, step, capture, classic=True, ble_count=1)
        if not in_window:
            step["result"] = "timeout_not_seen"
            ok = False
        rs232_capture_stop()
        result["steps"].append(step)
        if not ok:
            failures += 1
    power_summary(result, "method")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("bt-timeout", result, "VIASIS BT-Zeitlimit (6 min ohne Eingabe)",
                       f"Transport {args.transport}, {args.repeat} Mal, Wartezeit bis {args.timeout_wait:g} s.")
    return 0 if failures == 0 else 1


async def xmodem_send(transport, data: bytes) -> bool:
    """XMODEM-1K (CRC) ueber einen Transport (ByteStream rx). Gegenstelle: firmware() im Testmenue (Strg+U)."""
    def crc16(chunk: bytes) -> int:
        crc = 0
        for byte in chunk:
            crc ^= byte << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        return crc

    async def reply(timeout: float) -> int | None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for index, byte in enumerate(transport.rx.buf):
                if byte in (XMODEM_ACK, XMODEM_NAK, XMODEM_CAN):
                    del transport.rx.buf[:index + 1]
                    return byte
            await asyncio.sleep(0.01)
        return None

    total = (len(data) + 1023) // 1024
    for number in range(1, total + 1):
        chunk = data[(number - 1) * 1024:number * 1024].ljust(1024, bytes([XMODEM_SUB]))
        packet = bytes([XMODEM_STX, number & 0xFF, 0xFF - (number & 0xFF)]) + chunk + crc16(chunk).to_bytes(2, "big")
        for attempt in range(10):
            transport.rx.clear()
            await transport.write(packet)
            answer = await reply(10.0)
            if answer == XMODEM_ACK:
                break
            if answer == XMODEM_CAN:
                log(f"XMODEM: Abbruch durch das Geraet bei Block {number}.")
                return False
            log(f"XMODEM: Block {number} Antwort {answer!r}, Wiederholung {attempt + 1}.")
        else:
            await transport.write(bytes([XMODEM_CAN, XMODEM_CAN]))
            return False
        if number % 20 == 0 or number == total:
            log(f"XMODEM: Block {number}/{total}")
    for _ in range(2):
        transport.rx.clear()
        await transport.write(bytes([XMODEM_EOT]))
        if await reply(10.0) == XMODEM_ACK:
            return True
    return False


async def run_bt_update(args) -> int:
    """Firmware-Update ueber Bluetooth Classic (wie ein Servicetechniker): verbinden, PIN, Testmenue, Strg+U, XMODEM-1K,
    'y'. Der LPC startet neu, das IF820 haelt die Verbindung (RST nicht angeschlossen). Geprueft: Meldet sich das Geraet
    danach ueber dieselbe Verbindung (PIN-Prompt/Banner), Version auf RS232, danach BLE- und Classic-Smoke.
    ACHTUNG: loescht im Geraet Protokoll und Messdaten."""
    if not args.reboot_bin:
        log("bt-update braucht --reboot-bin (z. B. ..\\..\\sism.bin, gleiche oder neue Version).")
        return 2
    image = Path(args.reboot_bin).read_bytes()
    result = {"mode": "bt-update", "started_at": datetime.now().astimezone().isoformat(timespec="seconds"),
              "bin": args.reboot_bin, "bin_bytes": len(image), "repeat": args.repeat, "steps": [], "phases": []}
    if not await resolve_power_target(args):
        result["result"] = "device_not_found"
        write_report("bt-update", result)
        return 2
    phase = make_phase_runner(args, result, "BT-UPDATE")
    failures = 0
    for rep in range(1, args.repeat + 1):
        label = f"#{rep:02d} Update ueber Classic"
        print(f"\n{'#' * 70}\n  BT-UPDATE {label}\n{'#' * 70}")
        step = {"label": label, "method": "bt-update", "repeat": rep}
        capture = rs232_capture_start()
        args.transport = "classic"
        transport = await make_transport(args)
        try:
            await transport.connect(args.connect_timeout)
            await authenticate_or_sync_main_menu(transport, args, "bt-update")
            started = time.monotonic()
            step["menu_ok"] = await navigate(transport, [(None, b"H"), (MARK_HAUPTMENU, b"3"),
                                                        (MARK_TESTMENU, bytes([0x15]))], args.menu_timeout)
            step["xmodem_ready"] = step["menu_ok"] and await transport.rx.wait_for_marker("XModem", 10.0)
            if step["xmodem_ready"]:                               # erst nach dem Hinweistext auf das 'C' warten
                transport.rx.clear()
                step["xmodem_ready"] = await transport.rx.wait_for_marker("C", 30.0)
            step["xmodem_ok"] = step["xmodem_ready"] and await xmodem_send(transport, image)
            step["transfer_s"] = round(time.monotonic() - started, 1)
            if step["xmodem_ok"]:
                step["verify_ok"] = await transport.rx.wait_for_marker("?", 60.0)
                text = bytes(transport.rx.buf).decode("cp1252", errors="replace")
                step["verify_text"] = text[-200:]
                if step["verify_ok"] and "ok" in text.lower():
                    transport.rx.clear()
                    await transport.write(b"y")
                    t_reboot = time.monotonic()
                    # Nach dem Neustart laeuft die Sitzung weiter (beobachtet 2026-11-02 mit 5.128): die Firmware schickt
                    # Banner + Hauptmenue ohne neue PIN-Abfrage (Sitzung war schon angemeldet). PIN-Prompt gilt ebenso.
                    seen = None
                    while time.monotonic() - t_reboot < 60.0 and seen is None:
                        if MARK_PIN_PROMPT.encode() in transport.rx.buf:
                            seen = "PIN-Prompt"
                        elif MARK_MAIN_RANGE.encode() in transport.rx.buf:
                            seen = "Hauptmenue"
                        else:
                            await asyncio.sleep(0.2)
                    step["prompt_after_reboot_s"] = round(time.monotonic() - t_reboot, 1) if seen else None
                    step["after_reboot"] = seen
                    log(f"Nach dem Neustart: {seen + ' ueber dieselbe Classic-Verbindung' if seen else 'nichts ueber Classic (Verbindung tot)'}.")
                else:
                    log("Pruefung des Images nicht 'ok' - kein 'y' gesendet.")
            version = next((m.group(0) for _, t in (capture or []) if (m := BOOT_BANNER_RE.search(t))), None)
            step["version_rs232"] = version
        except Exception as exc:
            log(f"FEHLER: {exc}")
            step["error"] = str(exc)
        finally:
            await transport.disconnect()
            if hasattr(transport, "close"):
                transport.close()
        await asyncio.sleep(5.0)
        ok = await bt_step_checks(args, phase, label, step, capture, classic=True, ble_count=args.checks)
        if not step.get("xmodem_ok") or step.get("prompt_after_reboot_s") is None:
            step["result"] = "update_failed" if not step.get("xmodem_ok") else "no_prompt_after_reboot"
            ok = False
        rs232_capture_stop()
        result["steps"].append(step)
        if not ok:
            failures += 1
    power_summary(result, "method")
    pc_events_export()
    result["result"] = "passed" if failures == 0 else "failed"
    write_suite_report("bt-update", result, "VIASIS Firmware-Update ueber Bluetooth",
                       f"{Path(args.reboot_bin).name} ({len(image)} Byte) ueber Classic, {args.repeat} Mal.")
    return 0 if failures == 0 else 1


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    global TRACE
    p = argparse.ArgumentParser(description="BLE/BT-Classic-Test fuer das VIASIS-Mainboard.")
    p.add_argument("--mode", choices=("smoke", "transfer", "classic-smoke", "cycle", "download", "receive",
                                      "classic-suite", "module-suite", "fast-suite", "idle-latency",
                                      "interleave", "soak", "info", "power-sweep", "boot-race", "reboot-check",
                                      "nightly", "gsm-mux", "bt-timeout", "bt-update", "online-bt"),
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
    p.add_argument("--drop-after-blocks", type=int, default=0,
                   help="[download/receive] Test Linkverlust: nach N Ymodem-Bloecken hart trennen (kein ACK, kein CAN)")
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
    p.add_argument("--via", choices=("auto", "rs232", "bt"), default="auto",
                   help="Geraeteidentifikation (Info-Seite) ueber RS232/USB (--rs232-port) oder Bluetooth (--transport); "
                        "auto = RS232/USB, wenn der Mitschnitt-Port aktiv ist, sonst Bluetooth")
    p.add_argument("--no-identify", action="store_true",
                   help="Geraeteidentifikation (Hauptmenue -> Information) zu Beginn nicht ausfuehren")
    p.add_argument("--expect-fw", default=None,
                   help="erwartete Firmware-Version (z. B. 5.124); weicht die Info-Seite ab, gibt es eine Warnung")
    p.add_argument("--expect-module-fw", default=None,
                   help="erwartete Firmware des Bluetooth-Moduls (z. B. 1.4.12.12); Abweichung gibt eine Warnung")
    p.add_argument("--bt-module-info", action="store_true",
                   help="bei der Geraeteidentifikation zusaetzlich das Bluetooth-Modul auslesen (Funkmodems -> Bluetooth -> Information, "
                        "ca. 10 s, nur RS232/USB). Standard AUS in Tests: am 2026-10-20 blieb danach auf beiden Boards das BLE-Banner aus; "
                        "im Modus info ist es immer an")
    p.add_argument("--no-bt-module-info", action="store_true",
                   help="im Modus info das Bluetooth-Modul nicht auslesen")
    p.add_argument("--rounds", type=int, default=20,
                   help="[interleave] Runden (je Runde ein Classic- und ein BLE-Zyklus)")
    p.add_argument("--max-failures", type=int, default=10,
                   help="[interleave/soak] Abbruch nach so vielen fehlgeschlagenen Schritten (0 = nie)")
    p.add_argument("--soak-hours", type=float, default=8.0, help="[soak] Dauer der Messreihe in Stunden")
    p.add_argument("--soak-pause-s", type=float, default=60.0, help="[soak] Pause zwischen zwei Downloads")
    p.add_argument("--soak-records", type=int, default=50000,
                   help="[soak] Messwerte je Download (50000 = ca. 500 KB, ~80 s)")
    p.add_argument("--devices", default="",
                   help="[soak] zwei Geraete im Wechsel messen (Kontrollmessung), z. B. "
                        "VIASIS_99SC9923,VIASIS_26SC9994 (Namenspraefix je Geraet, BLE-Name ohne _BLE reicht)")
    p.add_argument("--rs232-port-b", default=None,
                   help="[soak mit --devices] RS232/USB-COM-Port des zweiten Geraets fuer dessen Mitschnitt; "
                        "'auto' sucht den USB-Port des Mainboards (VID/PID 1FC9:80F7)")
    p.add_argument("--no-verify-ports", action="store_true",
                   help="[soak mit --devices] Zuordnung Geraet <-> Mitschnitt-Port nicht per BLE-Banner pruefen")
    p.add_argument("--soak-skip-write", action="store_true",
                   help="[soak] Flash nicht neu beschreiben (vorhandene Messdaten verwenden)")
    p.add_argument("--soak-settle-s", type=float, default=8.0,
                   help="[soak] Wartezeit nach dem Download, bis die Sitzungszeile der Firmware vorliegt")
    p.add_argument("--min-bps", type=float, default=None,
                   help="Schwelle in B/s fuer 'langsamer Link' bei Downloads (Standard: BLE 4500, Classic 5500; 0 = aus)")
    p.add_argument("--slow-check-s", type=float, default=60.0,
                   help="nach dieser Zeit wird der Durchsatz grosser Downloads gegen --min-bps geprueft")
    p.add_argument("--slow-retries", type=int, default=1,
                   help="Wiederholungen nach Fruehabbruch bei langsamem Link (0 = nie abbrechen)")
    p.add_argument("--slow-wait-s", type=float, default=120.0,
                   help="Wartezeit vor der Wiederholung nach einem Fruehabbruch")
    p.add_argument("--ymodem-timeout", type=float, default=2.0,
                   help="[download/receive] Timeout je Ymodem-Block in Sekunden; muss unter dem "
                        "Firmware-ACK-Timeout von 6 s liegen")
    p.add_argument("--sniff", action="store_true",
                   help="BLE-Funk-Mitschnitt mit nRF52840-Sniffer (nrfutil ble-sniffer + tshark): Verbindungsparameter, "
                        "ATT-Ablauf, Trennen mit Grund, Funkqualitaet; Ergebnis sniffer_*.pcapng/.json im Report-Ordner")
    p.add_argument("--sniff-port", default=None, help="COM-Port des nRF-Sniffers (Default: automatisch finden)")
    p.add_argument("--sniff-follow", default=None,
                   help="BLE-Adresse des Zielgeraets fuer den Sniffer (Default: Adresse aus --address bzw. dem Scan)")
    p.add_argument("--sniff-keep-raw", action="store_true",
                   help="ungefilterten Mitschnitt (alle Geraete in Reichweite) zusaetzlich behalten (gross)")
    floats = lambda value: [float(item) for item in value.split(",")]   # noqa: E731
    p.add_argument("--relay-port", default="COM7", help="[power-sweep/boot-race/reboot-check] COM-Port des USB-Relais")
    p.add_argument("--relay", type=int, default=2, help="Relais-Nummer, die die Geraeteversorgung schaltet (Default 2)")
    p.add_argument("--repeat", type=int, default=3, help="[power-sweep/boot-race/reboot-check] Wiederholungen je Wert")
    p.add_argument("--off-times", type=floats, default=None,
                   help="[power-sweep] Stromlos-Zeiten in s (Default 0.05,0.1,0.2,0.5,1,2,5,10,30)")
    p.add_argument("--classic-every", type=int, default=3,
                   help="[power-sweep] jeden n-ten Schritt zusaetzlich Classic-Smoke (0 = nie)")
    p.add_argument("--boot-timeout", type=float, default=45.0, help="max. Wartezeit auf den Hochlauf nach dem Einschalten")
    p.add_argument("--boot-quiet", type=float, default=3.0,
                   help="Hochlauf gilt als fertig, wenn RS232 so viele Sekunden ruhig ist")
    p.add_argument("--step-hold", type=float, default=3.0, help="Haltezeit der Smoke-Tests in den Strom-/Neustart-Modi")
    p.add_argument("--race-delays", type=floats, default=None,
                   help="[boot-race] Verbinden so viele s nach dem Einschalten (Default 1,2,3,5,8,12)")
    p.add_argument("--race-off", type=float, default=10.0, help="[boot-race] Stromlos-Zeit vor jedem Versuch")
    p.add_argument("--race-retry-wait", type=float, default=10.0,
                   help="[boot-race] Pause vor dem Folgeversuch, wenn der Erstversuch scheitert")
    p.add_argument("--reboot-methods", default="firmware,dropout,power",
                   help="[reboot-check] firmware (Neu-Flashen, loescht Protokoll+Messdaten), dropout, power")
    p.add_argument("--reboot-bin", default=None, help="[reboot-check] .bin fuer die Methode firmware (gleiche Version)")
    p.add_argument("--dropout", type=float, default=0.1, help="[reboot-check] Dauer der kurzen Unterbrechung in s")
    p.add_argument("--power-off", type=float, default=10.0, help="[reboot-check] Stromlos-Zeit der Methode power")
    p.add_argument("--checks", type=int, default=2, help="[reboot-check/nightly/gsm-mux/bt-update] BLE-Smokes je Schritt")
    p.add_argument("--no-classic", action="store_true",
                   help="[Strom-/Neustart-/Tisch-Modi] keine Classic-Smokes (z. B. Geraet in Windows nicht gekoppelt)")
    p.add_argument("--nightly-set", default="03:13:30",
                   help="[nightly] Uhrzeit, auf die die Geraeteuhr gestellt wird (Neustart um 03:15, rtc.c)")
    p.add_argument("--nightly-recover", action="store_true",
                   help="[nightly] nach einem Fehlschlag Stromzyklus ueber das Relais und Gegenprobe (BLE-Smoke)")
    p.add_argument("--nightly-margin", type=float, default=45.0,
                   help="[nightly] Sekunden nach 03:15, bis der Neustart als erfolgt gilt und geprueft wird")
    p.add_argument("--gsm-settle", type=float, default=20.0, help="[gsm-mux] Wartezeit nach dem Oeffnen des Modem-Terminals")
    p.add_argument("--gsm-hold", type=float, default=60.0, help="[gsm-mux] Sekunden im Modem-Terminal (MUX auf GSM)")
    p.add_argument("--gsm-baud", type=int, choices=(9600, 115200), default=115200,
                   help="[gsm-mux] Baudrate im Modem-Terminal (EG91 antwortet nur mit 115200; 9600 ergab F18001)")
    p.add_argument("--online-timeout", type=float, default=600.0,
                   help="[online-bt] max. Wartezeit auf 'MQTT Server connected' nach dem Einschalten von MQTT-Debug")
    p.add_argument("--online-reconnect", type=float, default=480.0,
                   help="[online-bt] max. Wartezeit bis wieder online nach der BT-Sitzung (Firmware wartet 3 min)")
    p.add_argument("--online-settle", type=float, default=20.0,
                   help="[online-bt] Pause nach dem Verbinden (Join/Status/Parameter) vor dem BT-Schritt")
    p.add_argument("--timeout-wait", type=float, default=480.0,
                   help="[bt-timeout] max. Wartezeit ohne Eingabe; erwartet wird die Trennung nach 360-420 s")
    p.add_argument("--timeout-recover", type=float, default=90.0,
                   help="[bt-timeout] Wartezeit nach der Trennung bis zur Pruefung (Modul ist bis zu 60 s aus)")
    args = p.parse_args()
    global DROP_AFTER_BLOCKS
    DROP_AFTER_BLOCKS = args.drop_after_blocks

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
    pc_logging_start(args)
    if args.mode == "info" or not args.no_identify:        # erster Schritt jedes Tests: Geraet identifizieren
        asyncio.run(preflight_identify(args))
    if args.mode == "info":
        return report_device_info(args)
    if args.sniff and args.mode not in SNIFF_PER_PHASE_MODES and args.mode != "classic-smoke" \
            and args.mode != "idle-latency" and args.transport == "ble":
        sniff_start(args, "run")           # Einzeltest: ein Mitschnitt fuer den ganzen Lauf (Suiten: je BLE-Phase)

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
    if args.mode == "interleave":
        return asyncio.run(run_interleave(args))
    if args.mode == "soak":
        return asyncio.run(run_soak(args))
    if args.mode == "cycle":
        return asyncio.run(run_cycle_test(args))
    if args.mode == "idle-latency":
        return asyncio.run(run_idle_latency_test(args))
    if args.mode == "receive":
        return asyncio.run(run_receive_test(args))
    bench_tests = {"power-sweep": run_power_sweep, "boot-race": run_boot_race, "reboot-check": run_reboot_check,
                   "nightly": run_nightly, "gsm-mux": run_gsm_mux, "bt-timeout": run_bt_timeout, "bt-update": run_bt_update,
                   "online-bt": run_online_bt}
    if args.mode in bench_tests:
        global RELAY
        needs_relay = (args.mode in ("power-sweep", "boot-race")
                       or (args.mode == "reboot-check" and any(m in args.reboot_methods for m in ("dropout", "power")))
                       or (args.mode == "nightly" and args.nightly_recover))
        if needs_relay:
            try:
                RELAY = PowerRelay(args.relay_port, args.relay)
            except (serial.SerialException, OSError) as exc:
                log(f"FEHLER: USB-Relais an {args.relay_port} nicht erreichbar ({exc}). Angeschlossen? --relay-port?")
                return 2
            log(f"Relais {args.relay} an {args.relay_port}: Geraet {'unter Strom' if RELAY.powered() else 'STROMLOS'}.")
            register_port(args.relay_port, f"USB-Relais, Relais {args.relay} schaltet die Geraeteversorgung")
        if SNIFFER is None:
            log("WARNUNG: kein RS232-Mitschnitt - Hochlauf wird nur ueber feste Wartezeiten erkannt.")
        return asyncio.run(bench_tests[args.mode](args))
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
        if RELAY is not None:
            RELAY.close()      # Relais rastet: Geraet bei Ende/Abbruch immer wieder unter Strom setzen
        sniff_stop(SNIFF_STATE.get("args"))   # nur aktiv, wenn ein Mitschnitt noch laeuft (Einzeltest/Abbruch)
        pc_events_export()     # greift nur, falls die Suite es nicht schon getan hat
        log_used_ports()       # welche Ports hat der Test wofuer benutzt
        pc_logging_stop()
        if SNIFFER is not None:
            SNIFFER.stop()
        if TRACE is not None:
            TRACE.close(exit_code)
