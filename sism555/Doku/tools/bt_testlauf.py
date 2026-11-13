#!/usr/bin/env python3
"""bt_testlauf.py - automatischer BT-Testlauf am Tisch (Strom, Neustart, Grenzfaelle) fuer ein angeschlossenes Board.

Erkennt das Board ueber RS232 (Seriennummer -> BLE-Name und PIN, LTE ja/nein), prueft, ob es in Windows fuer Classic
gekoppelt ist, und ruft die Testmodi von ble_mainboard_test.py nacheinander auf. Nach einem fehlgeschlagenen Schritt
holt ein Stromzyklus (Relais) das Geraet zurueck, damit der naechste Schritt sauber beginnt. Am Ende eine
Zusammenfassung (Markdown + JSON) unter Testreports/<Zeit>_testlauf/.

  python bt_testlauf.py                      # voller Lauf (ca. 1,5 bis 2 h)
  python bt_testlauf.py --profil kurz        # Kurzlauf (ca. 25 min)
  python bt_testlauf.py --nur nightly,gsm-mux
  python bt_testlauf.py --plan               # nur anzeigen, was laufen wuerde

Anleitung: bt_testlauf_Anleitung.md. Hintergrund: ../Testplan_BT_Erweiterung_Strom_Grenzfaelle.md.
"""
import argparse
import json
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

HERE = Path(__file__).resolve().parent
TEST = HERE / "ble_mainboard_test.py"
REPORTS = HERE / "Testreports"
sys.path.insert(0, str(HERE))


def log(msg: str) -> None:
    print(f"{datetime.now():%H:%M:%S} {msg}", flush=True)


# ---------------------------------------------------------------------------
# Board erkennen
# ---------------------------------------------------------------------------

def read_board(port: str) -> dict | None:
    """Hauptmenue -> Information ueber RS232: Seriennummer, Firmware, Schnittstellen."""
    try:
        with serial.Serial(port, 115200, timeout=0.5) as ser:
            ser.reset_input_buffer()
            out = b""
            for key in (b"\r", b"H", b"6"):
                ser.write(key)
                time.sleep(0.8)
                out += ser.read(4000)
            ser.write(b"9")
            time.sleep(0.5)
            ser.read(3000)
    except serial.SerialException as exc:
        log(f"RS232 {port} nicht nutzbar: {exc}")
        return None
    text = out.decode("cp1252", errors="replace")
    serno = re.search(r"Seriennummer:\s*(\S+)", text)
    version = re.search(r"Version (\d+\.[A-Za-z]?\d+)", text)
    interfaces = re.search(r"Schnittstellen:\s*(.+)", text)
    if not serno:
        log("Board antwortet nicht mit der Informationsseite (Hauptmenue erreichbar? Board an?).")
        return None
    sn = serno.group(1)
    digits = re.sub(r"\D", "", sn)
    return {"serno": sn, "name": f"VIASIS_{sn}", "pin": digits[:2] + digits[-4:] if len(digits) >= 6 else None,
            "firmware": version.group(1) if version else None,
            "interfaces": interfaces.group(1).strip() if interfaces else "",
            "lte": bool(interfaces and "LTE" in interfaces.group(1))}


def known_addresses(name: str) -> list[str]:
    """Bluetooth-Adressen, die Windows unter diesem Namen (mit oder ohne '_BLE') kennt (BTHPORT-Registry, nur lesend).
    Hinweis: Dual-Mode-Geraete stehen dort oft nur mit dem BLE-Namen, auch wenn Classic gekoppelt ist."""
    import winreg
    found = []
    wanted = {name.upper(), f"{name.upper()}_BLE"}
    try:
        root = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Services\BTHPORT\Parameters\Devices")
    except OSError:
        return found
    index = 0
    while True:
        try:
            address = winreg.EnumKey(root, index)
        except OSError:
            break
        index += 1
        try:
            with winreg.OpenKey(root, address) as key:
                raw, _ = winreg.QueryValueEx(key, "Name")
        except OSError:
            continue
        device_name = bytes(raw).split(b"\0")[0].decode("ascii", errors="replace").strip().upper()
        if device_name in wanted:
            found.append(address.upper())
    return found


def classic_paired(name: str) -> bool:
    """Gibt es fuer das Geraet einen SPP-COM-Port (Classic gekoppelt)? Nur lesend (Registry)."""
    import ble_mainboard_test as bmt
    for address in known_addresses(name):
        try:
            port = bmt.find_classic_port(address, None)
            log(f"Classic gekoppelt: {name} ({address}) an {port}.")
            return True
        except Exception:        # kein SPP-Port fuer diese Adresse
            continue
    log(f"Kein Classic-COM-Port fuer {name} gefunden.")
    return False


def relay_ok(port: str, relay: int) -> bool:
    try:
        with serial.Serial(port, 19200, timeout=1) as ser:
            ser.write(bytes([111 + relay - 1]))          # sicherheitshalber: Geraet unter Strom
            ser.write(bytes([0x5B]))
            return bool(ser.read(1))
    except serial.SerialException as exc:
        log(f"Relais an {port} nicht erreichbar: {exc}")
        return False


def power_cycle(port: str, relay: int, off_s: float = 10.0, boot_s: float = 45.0) -> None:
    log(f"Erholung: Stromzyklus {off_s:g} s, dann {boot_s:g} s Hochlauf ...")
    with serial.Serial(port, 19200, timeout=1) as ser:
        ser.write(bytes([101 + relay - 1]))
        time.sleep(off_s)
        ser.write(bytes([111 + relay - 1]))
    time.sleep(boot_s)


# ---------------------------------------------------------------------------
# Plan
# ---------------------------------------------------------------------------

def build_plan(profile: str, board: dict, paired: bool, bin_path: Path | None, repeat: int | None) -> list[dict]:
    """Reihenfolge ist wichtig: nightly liest das Geraeteprotokoll, bt-update loescht es -> bt-update zuletzt."""
    def rep(default):
        return str(repeat if repeat else default)

    full = profile == "voll"
    plan = [
        {"mode": "nightly", "args": ["--repeat", rep(2 if full else 1), "--nightly-recover"],
         "why": "BT nach dem taeglichen Neustart um 3:15"},
        {"mode": "reboot-check", "args": ["--reboot-methods", "dropout,power", "--repeat", rep(3 if full else 1)],
         "why": "BT nach kurzer Unterbrechung und nach Stromzyklus"},
        {"mode": "power-sweep",
         "args": ["--repeat", rep(2 if full else 1)] + ([] if full else ["--off-times", "0.1,1,10"]),
         "why": "Stromlos-Zeiten 0,05 bis 30 s"},
        {"mode": "boot-race",
         "args": ["--repeat", rep(2 if full else 1)] + ([] if full else ["--race-delays", "2,8"]),
         "why": "Verbinden waehrend des Hochlaufs"},
    ]
    if board["lte"]:
        plan.append({"mode": "gsm-mux", "args": ["--repeat", rep(2 if full else 1)],
                     "why": "UART1-MUX auf GSM (Modem-Terminal)"})
        plan.append({"mode": "online-bt", "args": ["--repeat", rep(2 if full else 1)],
                     "why": "BT verbinden, waehrend das Geraet online ist (MQTT); Einrichtung: device_setup.py"})
    if full:
        plan.append({"mode": "bt-timeout", "args": ["--repeat", "1", "--transport", "ble"],
                     "why": "Trennung nach 6 min ohne Eingabe (BLE)"})
        if paired:
            plan.append({"mode": "bt-timeout", "args": ["--repeat", "1", "--transport", "classic"],
                         "why": "Trennung nach 6 min ohne Eingabe (Classic)"})
    if paired and bin_path is not None:
        plan.append({"mode": "bt-update", "args": ["--repeat", "1", "--reboot-bin", str(bin_path)],
                     "why": "Firmware-Update ueber Bluetooth Classic (loescht Protokoll und Messdaten)"})
    return plan


# ---------------------------------------------------------------------------
# Ausfuehren
# ---------------------------------------------------------------------------

def run_step(step: dict, common: list[str], logfile: Path) -> dict:
    cmd = [sys.executable, str(TEST), "--mode", step["mode"], *common, *step["args"]]
    log(f"Start {step['mode']}: {' '.join(cmd[2:])}")
    started = time.time()
    report = None
    with open(logfile, "w", encoding="utf-8") as out:
        proc = subprocess.Popen(cmd, cwd=HERE, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
                                env={**__import__("os").environ, "PYTHONIOENCODING": "utf-8"})
        for line in proc.stdout:
            out.write(line)
            if any(key in line for key in ("Ergebnis", "Hochlauf", "Geraeteprotokoll", "Trennung durch", "ok ",
                                            "FEHLER", "Gegenprobe", "Nach dem Neustart")):
                print("   " + line.rstrip(), flush=True)
            match = re.search(r"Gesamtbericht JSON: (.+\.json)", line)
            if match:
                report = match.group(1).strip()
        code = proc.wait()
    summary = None
    result = None
    if report and Path(report).exists():
        data = json.loads(Path(report).read_text(encoding="utf-8"))
        summary = data.get("summary")
        result = data.get("result")
    entry = {"mode": step["mode"], "args": step["args"], "why": step["why"], "exit_code": code,
             "result": result or ("passed" if code == 0 else "failed"), "summary": summary,
             "report": report, "log": str(logfile), "duration_min": round((time.time() - started) / 60, 1)}
    log(f"Ende {step['mode']}: {entry['result']} ({entry['duration_min']} min)")
    return entry


def write_summary(folder: Path, board: dict, paired: bool, results: list[dict]) -> Path:
    data = {"board": board, "classic_paired": paired, "steps": results,
            "finished_at": datetime.now().isoformat(timespec="seconds")}
    (folder / "testlauf.json").write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    lines = [f"# BT-Testlauf {board['name']} (Firmware {board['firmware']})", "",
             f"- Schnittstellen: {board['interfaces']}",
             f"- Classic in Windows gekoppelt: {'ja' if paired else 'nein (nur BLE geprueft)'}",
             f"- Ende: {data['finished_at']}", "",
             "| Schritt | Prueft | Ergebnis | Dauer | Erfolg je Wert | Bericht |", "|---|---|---|---|---|---|"]
    for entry in results:
        summary = ", ".join(f"{k}: {v['ok']}/{v['ok'] + v['fehler']}" for k, v in (entry["summary"] or {}).items())
        report = Path(entry["report"]).parent.name if entry["report"] else "-"
        lines.append(f"| {entry['mode']} {' '.join(a for a in entry['args'] if not a.endswith('.bin'))} | {entry['why']} | "
                     f"**{entry['result']}** | {entry['duration_min']} min | {summary or '-'} | {report} |")
    path = folder / "testlauf.md"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def main() -> int:
    p = argparse.ArgumentParser(description="Automatischer BT-Testlauf am Tisch (siehe bt_testlauf_Anleitung.md).")
    p.add_argument("--rs232-port", default="COM3", help="RS232-Adapter am Board (Default COM3)")
    p.add_argument("--relay-port", default="COM7", help="USB-Relais (Default COM7)")
    p.add_argument("--relay", type=int, default=2, help="Relais, das die Geraeteversorgung schaltet (Default 2)")
    p.add_argument("--profil", choices=("kurz", "voll"), default="voll", help="kurz ca. 25 min, voll ca. 1,5-2 h")
    p.add_argument("--nur", default=None, help="nur diese Modi (Komma-Liste), z. B. nightly,boot-race")
    p.add_argument("--ohne", default=None, help="diese Modi auslassen (Komma-Liste)")
    p.add_argument("--wiederholungen", type=int, default=None, help="Wiederholungen je Modus (ueberschreibt das Profil)")
    p.add_argument("--bin", default=str(HERE.parent.parent / "sism.bin"),
                   help="Firmware fuer bt-update (Default sism555/sism.bin); 'keins' = bt-update auslassen")
    p.add_argument("--no-classic", action="store_true", help="Classic nicht pruefen, auch wenn gekoppelt")
    p.add_argument("--plan", action="store_true", help="nur den Plan anzeigen, nichts ausfuehren")
    args = p.parse_args()

    log("BT-Testlauf: Vorpruefung ...")
    board = read_board(args.rs232_port)
    if board is None:
        return 2
    log(f"Board {board['serno']}, Firmware {board['firmware']}, PIN {board['pin']}, Schnittstellen: {board['interfaces']}.")
    if not relay_ok(args.relay_port, args.relay):
        return 2
    paired = not args.no_classic and classic_paired(board["name"])
    log(f"Classic: {'gekoppelt, wird mitgeprueft' if paired else 'nicht gekoppelt bzw. abgeschaltet, nur BLE'}.")
    bin_path = None if args.bin.lower() == "keins" else Path(args.bin)
    if bin_path is not None and not bin_path.exists():
        log(f"{bin_path} nicht gefunden - bt-update entfaellt.")
        bin_path = None

    plan = build_plan(args.profil, board, paired, bin_path, args.wiederholungen)
    if args.nur:
        wanted = {m.strip() for m in args.nur.split(",")}
        plan = [s for s in plan if s["mode"] in wanted]
    if args.ohne:
        skip = {m.strip() for m in args.ohne.split(",")}
        plan = [s for s in plan if s["mode"] not in skip]
    log("Plan:")
    for number, step in enumerate(plan, 1):
        log(f"  {number}. {step['mode']:13} {' '.join(step['args'])}   ({step['why']})")
    if args.plan or not plan:
        return 0

    common = ["--name", board["name"], "--pin", board["pin"], "--rs232-port", args.rs232_port,
              "--relay-port", args.relay_port, "--relay", str(args.relay), "--no-identify"]
    if not paired:
        common.append("--no-classic")
    folder = REPORTS / f"{datetime.now():%Y-%m-%d_%H-%M-%S}_{board['serno']}_testlauf"
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        for number, step in enumerate(plan, 1):
            entry = run_step(step, common, folder / f"{number:02d}_{step['mode']}.log")
            results.append(entry)
            write_summary(folder, board, paired, results)
            if entry["result"] not in ("passed", "skipped") and number < len(plan):
                power_cycle(args.relay_port, args.relay)
    except KeyboardInterrupt:
        log("Abgebrochen.")
    finally:
        relay_ok(args.relay_port, args.relay)            # Geraet immer unter Strom zuruecklassen
    path = write_summary(folder, board, paired, results)
    log(f"Zusammenfassung: {path}")
    for entry in results:
        log(f"  {entry['mode']:13} {entry['result']}")
    return 0 if all(e["result"] in ("passed", "skipped") for e in results) else 1


if __name__ == "__main__":
    sys.exit(main())
