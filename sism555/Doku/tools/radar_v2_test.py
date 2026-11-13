#!/usr/bin/env python3
"""radar_v2_test.py - Radar-Empfaenger (Via-2 / bisheriger Transceiver) und Joachim-Integration am Tisch pruefen.

Bedient das Geraet ueber RS232 (Menues, Strg+T, Werksmenue, Protokoll), schaltet die Versorgung ueber das
USB-Relais und ruft fuer Flashen und Bluetooth die vorhandenen Werkzeuge auf. Testfaelle und Kriterien:
Doku/Testplan_Radar_V2.md, Bedienung: Doku/tools/radar_v2_test_Anleitung.md.

  python radar_v2_test.py --profil kurz                 # ca. 5 min, ein Stromzyklus, nichts wird geloescht
  python radar_v2_test.py --profil lang                 # ca. 25 min, flasht --bin neu (loescht Protokoll/Messdaten)
  python radar_v2_test.py --profil lang --ohne flash,bt # ohne Neu-Flashen und ohne Bluetooth-Test

Ergebnis: Testreports/<Zeit>_<Seriennummer>_radar/ (bericht.md, ergebnis.json, rs232.log). Exit 0 = bestanden,
1 = mindestens ein Test fehlgeschlagen, 2 = Geraet nicht erreichbar / Bedienfehler.
"""
import argparse
import json
import re
import statistics
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

HERE = Path(__file__).resolve().parent
REPORTS = HERE / "Testreports"

F_MIN_VIA2, F_MAX_VIA2 = 24150, 24200     # hard.h Min_TXF_VIA2 / Max_TXF_VIA2
F_SPREAD_MAX = 2                          # MHz, Grundstreuung (Toleranz Kennlinie)
F_OUTLIER_MAX = 3                         # MHz, einzelne Strg+T-Messung darf so weit vom Median liegen (beobachtet 2-3 MHz
                                          # nach unten, 2 von 40 mit 5.141); mehr als 10 % Ausreisser gelten als Fehler
F_TOTBAND = 1.5                           # MHz, ab 5.142: Nachfuehrung haelt die Frequenz Kanal +/- 1,5 MHz
F_TRUNC = 1.0                             # MHz, Strg+T zeigt ganze MHz (bis 5.143 abgeschnitten, ab 5.144 gerundet)
HF_DIS_P1 = 0x10                          # IC37 Port 1 Bit 4 (HF_DIS, 1 = Transceiver aus), hard.h HF_DIS = 1<<12
IC37, IC32 = 0x42, 0x5C                   # 8-Bit-Adressen wie im Geraet (Eingabe dezimal: 66, 92)
NO_DEVICE_ADR = 0x30                      # 48: dort antwortet auf allen bekannten Boards nichts
DTC_RADAR_TYPE, DTC_I2C_DPP, DTC_I2C_EXP = 17005, 20001, 20002

MAIN, INFO, WERK, TEST = "1...6?", "1...9?", "1...8?", "1...9?"
CTRL_T, CTRL_W, CTRL_F = b"\x14", b"\x17", b"\x06"


def now():
    return time.strftime("%H:%M:%S")


class Report:
    def __init__(self, folder: Path):
        self.folder = folder
        self.items = []
        self.meta = {}
        self.t0 = time.time()

    def log(self, msg):
        line = f"{now()} {msg}"
        print(line, flush=True)
        with open(self.folder / "ablauf.log", "a", encoding="utf-8") as f:
            f.write(line + "\n")

    def check(self, tid, name, ok, detail=""):
        """ok: True = bestanden, False = fehlgeschlagen, None = nicht anwendbar/uebersprungen."""
        self.items.append({"id": tid, "name": name, "ok": ok, "detail": detail})
        mark = {True: "OK  ", False: "FEHL", None: "--  "}[ok]
        self.log(f"[{mark}] {tid} {name}: {detail}")
        return ok

    @property
    def failed(self):
        return [i for i in self.items if i["ok"] is False]

    def write(self):
        dur = (time.time() - self.t0) / 60
        res = "bestanden" if not self.failed else "FEHLGESCHLAGEN"
        self.meta.update({"ergebnis": res, "dauer_min": round(dur, 1)})
        (self.folder / "ergebnis.json").write_text(
            json.dumps({"meta": self.meta, "tests": self.items}, indent=2, ensure_ascii=False), encoding="utf-8")
        lines = [f"# Radar-Test {self.meta.get('profil')} - {self.meta.get('serno', '?')} - {res}", "",
                 f"- Start: {self.meta.get('start')}, Dauer {dur:.1f} min",
                 f"- Firmware: {self.meta.get('version')}, Infoseite: {self.meta.get('info_line')}",
                 f"- Erkannter Empfaenger: **{self.meta.get('typ')}**", "",
                 "| Nr | Test | Ergebnis | Detail |", "|---|---|---|---|"]
        for i in self.items:
            mark = {True: "ok", False: "**FEHLER**", None: "-"}[i["ok"]]
            lines.append(f"| {i['id']} | {i['name']} | {mark} | {str(i['detail']).replace('|', '/')} |")
        (self.folder / "bericht.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        return res


class Device:
    """RS232-Bedienung. Jede Ausgabe des Geraets landet zusaetzlich in rs232.log."""

    def __init__(self, port, baud, rep: Report):
        self.port, self.baud, self.rep = port, baud, rep
        self.ser = None
        self.trace = open(rep.folder / "rs232.log", "a", encoding="utf-8")

    def open(self):
        if self.ser is None:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)

    def close(self):
        if self.ser is not None:
            self.ser.close()
            self.ser = None

    def _read(self):
        d = self.ser.read(self.ser.in_waiting or 1)
        if d:
            t = d.decode("cp1252", errors="replace").replace("\r", "")
            self.trace.write(t)
            self.trace.flush()
            return t
        return ""

    def drain(self, secs=0.3):
        end = time.time() + secs
        buf = ""
        while time.time() < end:
            buf += self._read()
        return buf

    def wait(self, pattern, timeout, buf=""):
        """Liest, bis pattern (Text oder Regex) im Gelesenen steht. Rueckgabe (gefunden, Text)."""
        rx = pattern if isinstance(pattern, re.Pattern) else re.compile(re.escape(pattern))   # Text = woertlich
        end = time.time() + timeout
        while time.time() < end:
            buf += self._read()
            if rx.search(buf):
                return True, buf
        return False, buf

    def send(self, data: bytes):
        self.trace.write(f"\n>>> {data!r}\n")
        self.ser.write(data)

    def ask(self, data: bytes, pattern, timeout=5.0):
        self.ser.reset_input_buffer()
        self.send(data)
        return self.wait(pattern, timeout)

    # --- Navigation ---------------------------------------------------------------------------------------------
    def main(self):
        for attempt in range(4):
            ok, _ = self.ask(b"H" if attempt % 2 == 0 else b"\rH", MAIN, 4.0)
            if ok:
                self.drain(0.2)
                return True
            time.sleep(1.0)
        raise RuntimeError("Hauptmenue nicht erreicht")

    def testmenu(self):
        self.main()
        ok, _ = self.ask(b"3", TEST, 4.0)
        if not ok:
            raise RuntimeError("Testmenue nicht erreicht")

    def werk(self):
        self.testmenu()
        ok, buf = self.ask(CTRL_W, re.compile(r"Change configuration[\s\S]*1\.\.\.8\?"), 6.0)
        if not ok:
            raise RuntimeError("Werksmenue nicht erreicht")
        return buf

    def leave_werk(self):
        self.ask(b"8", TEST, 4.0)
        self.main()

    def comp(self):
        self.werk()
        ok, _ = self.ask(b"6", re.compile(r"Write flash data[\s\S]*1\.\.\.8\?"), 5.0)
        if not ok:
            raise RuntimeError("Component tests nicht erreicht")

    def leave_comp(self):
        self.ask(b"8", WERK, 4.0)
        self.leave_werk()

    # --- Funktionen -------------------------------------------------------------------------------------------------
    def werk_config(self):
        """Werksmenue-Kopf (Configuration: ... Language ...)."""
        buf = self.werk()
        self.leave_werk()
        return buf

    TRANS_MENU = re.compile(r"1\.\.\.[359]\?")   # bisher 1...3?, Via-2 5.L1-5.141 1...5?, Via-2 ab 5.142 1...9?

    def trans_open(self):
        """Werksmenue -> 4 "Amplifier & Transceiver". Rueckgabe (ok, Text bis zur Menueabfrage)."""
        self.werk()
        return self.ask(b"4", self.TRANS_MENU, 10.0)

    def trans_leave(self):
        # 'R' beendet jedes putmenu() mit -1 und damit die Menueschleife, unabhaengig von der Zahl der Punkte.
        # Nicht die Ziffer des letzten Punkts senden: bei Via-2 5.142 ist '5' "Abgleich auf Kanal".
        self.ask(b"R", WERK, 6.0)
        self.leave_werk()

    @staticmethod
    def parse_trans(buf):
        """Transceiver-Menue auswerten. typ 'VIA2' / 'TRX' / None; f, ziel in MHz (float) oder None."""
        r = {"typ": None, "f": None, "ziel": None, "kanal": None, "kanal_modus": None, "stufe": None}
        if "1...5?" in buf or "1...9?" in buf:
            r["typ"] = "VIA2"
        elif "1...3?" in buf:
            r["typ"] = "TRX"
        m = re.search(r"Frequency:\s*(\d+)\s*MHz", buf)                     # 5.L1-5.141
        if m:
            r["f"] = float(m.group(1))
        m = re.search(r"Frequenz\s+(\d+[.,]\d)\s*MHz", buf)                 # ab 5.142 (hf_info, Median)
        if m:
            r["f"] = float(m.group(1).replace(",", "."))
        m = re.search(r"Kanal\s+(\d+)\s+\((automatisch|fest)\),\s*Ziel\s+(\d+[.,]\d)\s*MHz", buf)
        if m:
            r["kanal"], r["kanal_modus"], r["ziel"] = int(m.group(1)), m.group(2), float(m.group(3).replace(",", "."))
        m = re.search(r"Stufe\s+(\d+)", buf)
        if m:
            r["stufe"] = int(m.group(1))
        return r

    def transmenu(self):
        """Transceiver-Menue oeffnen, auswerten, verlassen. Rueckgabe dict siehe parse_trans() plus 'text'."""
        ok, buf = self.trans_open()
        r = self.parse_trans(buf) if ok else self.parse_trans("")
        r["text"] = buf
        self.trans_leave()
        return r

    def ctrl_t(self):
        """Strg+T im Hauptmenue. Rueckgabe Frequenz in MHz, 0 bei Messfehler, None ohne Ausgabe (kein Via-2)."""
        self.main()
        ok, buf = self.ask(CTRL_T, re.compile(r"\n(\d+) MHz"), 2.0)
        if ok:
            return int(re.search(r"\n(\d+) MHz", buf).group(1))
        return None

    def i2c_scan(self):
        self.comp()
        ok, buf = self.ask(b"5", re.compile(r"\d+ devices found[\s\S]*1\.\.\.8\?"), 8.0)
        self.leave_comp()
        return re.findall(r"^([0-9A-F]{2})h\s+\d+d\s+(.*)$", buf, re.M) if ok else None

    def i2c_regs(self, adr):
        """Component tests -> 6 I2C Devices. Rueckgabe (antwortet, {reg: wert}, text)."""
        self.comp()
        ok, _ = self.ask(b"6", "address", 4.0)
        if not ok:
            raise RuntimeError("I2C Devices: keine Adressabfrage")
        self.ser.reset_input_buffer()
        self.send(f"{adr}\r".encode())
        ok, buf = self.wait(re.compile(r"\(j/n\)\?|1\.\.\.8\?"), 8.0)
        # Format je Baustein (i2cdev[].format): Expander binaer "00010000b", Digitalpotis hex "1FFh"
        regs = {int(r): int(v, 2 if suf == "b" else 16)
                for r, v, suf in re.findall(r"^(\d+)\t\t([0-9A-F]+)([hb]?)\s*$", buf, re.M)}
        answered = "(j/n)?" in buf
        if answered:
            self.ask(b"n", WERK, 4.0)
        self.leave_comp()
        return answered, regs, buf

    def hf_dis(self):
        """IC37 lesen: (Ausgang P1 Bit 4, Eingang P1 Bit 4) - True = Transceiver aus."""
        answered, regs, _ = self.i2c_regs(IC37)
        if not answered or 3 not in regs or 1 not in regs:
            return None, None
        return bool(regs[3] & HF_DIS_P1), bool(regs[1] & HF_DIS_P1)

    def info(self):
        self.main()
        ok, buf = self.ask(b"6", INFO, 5.0)
        self.ask(b"9", MAIN, 4.0)
        m = re.search(r"Version (\S+?)(?:,\s*(\S+))?\s*$", buf, re.M)
        s = re.search(r"Seriennummer:\s*(\S+)", buf)
        line = re.search(r"^.*Version.*$", buf, re.M)
        return {"version": m.group(1) if m else None, "hw": m.group(2) if m else None,
                "serno": s.group(1) if s else None, "line": line.group(0).strip() if line else None,
                "text": buf if ok else ""}

    def protocol(self, need=None):
        """Information -> 1 Protokoll. Rueckgabe (Anzahl, Eintraege neueste zuerst). need = so viele Eintraege lesen."""
        self.main()
        self.ask(b"6", INFO, 5.0)
        ok, buf = self.ask(b"1", re.compile(r"\(j/n\)\?"), 6.0)
        m = re.search(r"Anzahl Protokolldaten\s*=\s*(\d+)", buf)
        count = int(m.group(1)) if m else None
        entries = []
        if count and need != 0:
            self.ser.reset_input_buffer()
            self.send(b"j")
            text = ""
            while True:
                ok, part = self.wait(re.compile(r"Weiter mit Return|Daten l|1\.\.\.9\?"), 20.0)
                text += part
                entries = re.findall(r"^([FWI]\d+ .*?;\d\d\.\d\d\.\d\d;\d\d:\d\d:\d\d)\s*$", text, re.M)
                if "Weiter mit Return" in part:
                    if need is not None and len(entries) >= need:
                        self.send(b"x")
                        ok, part = self.wait(re.compile(r"Daten l|1\.\.\.9\?"), 6.0)
                        text += part
                    else:
                        self.send(b"\r")
                        continue
                break
            if "Daten l" in text:
                self.ask(b"n", INFO, 5.0)          # Protokoll NIE loeschen
        else:
            self.ask(b"n", INFO, 4.0)
        self.ask(b"9", MAIN, 4.0)
        return count, entries

    def online(self, secs):
        self.main()
        ok, _ = self.ask(b"2", "Ende mit Return", 5.0)
        buf = self.drain(secs)
        self.ask(b"\r", MAIN, 6.0)
        return ok, buf

    def light(self, n=3):
        self.testmenu()
        ok, buf = self.ask(b"6", "Ende mit Return", 5.0)
        buf += self.drain(0.8 * n + 1.0)                # VEML7700: eine Zeile je ca. 0,65 s
        self.send(b"\r")
        self.drain(1.0)
        self.main()
        return re.findall(r"(\d+)\.(\d+) lx\s+roh\s+(\d+)\s+Verst\.\s+(\S+)\s+Dimm\s+(\d+)", buf), buf

    def radiostatus(self):
        self.main()
        self.ser.reset_input_buffer()
        self.send(CTRL_F)
        return self.drain(4.0)

    def set_time(self, hhmmss):
        self.main()
        self.ask(b"6", INFO, 5.0)
        ok, _ = self.ask(b"4", "hh:mm:ss", 4.0)
        if ok:
            self.ser.reset_input_buffer()
            self.send(hhmmss.encode() + b"\r")
            ok, _ = self.wait(INFO, 5.0)
        self.ask(b"9", MAIN, 4.0)
        return ok

    def kennlinie(self):
        """Transceiver-Menue -> 2: Frequenz ueber alle Wiper-Stufen von IC32. Rueckgabe [(stufe, MHz)]."""
        ok, buf = self.trans_open()
        if not ok or self.parse_trans(buf)["typ"] != "VIA2":
            self.trans_leave()
            return None
        ok, buf = self.ask(b"2", self.TRANS_MENU, 150.0)   # endet selbst nach Stufe 256, dann Menue erneut
        self.trans_leave()
        return [(int(a), int(b)) for a, b in re.findall(r"^\s*(\d+)\s+(\d+)\s*$", buf, re.M)]

    def wait_boot(self, timeout=90.0):
        """Nach Stromzyklus oder Neustart den Hochlauf abwarten. Rueckgabe (Sekunden, Version) oder None.

        Nur der Kaltstart (POR) gibt den Banner "Version x.y" aus. Nach kurzem Aussetzer (Brown-out/Watchdog, beobachtet
        bei 0,1-1 s stromlos) und nach dem Neustart 3:15 (reboot()) kommt kein Banner; dort zeigt die IF820-Zeile
        "[BT-Zustand] BEREIT (Hochlauf fertig" den Hochlauf an. Version dann "ohne Banner"."""
        t0 = time.time()
        ok, buf = self.wait(re.compile(r"Version \S+\s*\n|BEREIT \(Hochlauf fertig"), timeout)
        if not ok:
            return None
        secs = round(time.time() - t0, 1)
        buf += self.drain(5.0)                      # beim Kaltstart folgt der Banner kurz nach BEREIT
        ver = re.findall(r"Version (\S+)\s*\n", buf)
        return secs, ver[-1] if ver else "ohne Banner"


def run_tool(rep, args_list, timeout):
    rep.log("Starte " + " ".join(str(a) for a in args_list))
    p = subprocess.run([sys.executable] + [str(a) for a in args_list], cwd=HERE, capture_output=True, text=True,
                       timeout=timeout, encoding="utf-8", errors="replace")
    with open(rep.folder / "ablauf.log", "a", encoding="utf-8") as f:
        f.write(p.stdout + p.stderr)
    return p.returncode, p.stdout + p.stderr


class Tester:
    def __init__(self, a, rep: Report, dev: Device):
        self.a, self.rep, self.dev = a, rep, dev
        self.typ = None
        self.ziel = None                   # Kanalfrequenz in MHz (ab 5.142), sonst None
        self.f_ref = []
        self.expected_codes = set()        # vom Test selbst erzeugte F/W-Codes (E1)

    def power_cycle(self, off):
        self.dev.ser.reset_input_buffer()
        rc, out = run_tool(self.rep, ["relay_power.py", "zyklus", off, "--port", self.a.relay_port,
                                      "--relay", self.a.relay], 60)
        if rc != 0:
            raise RuntimeError("Relais: " + out.strip()[-200:])
        return self.dev.wait_boot()

    # --- Bausteine ------------------------------------------------------------------------------------------------
    def check_type(self, tid, label):
        r = self.dev.transmenu()
        ok = r["typ"] == self.typ
        detail = f"Transceiver-Menue {r['typ']}" + (f", Frequenz {r['f']} MHz" if r["f"] is not None else "")
        if r["typ"] == "VIA2":
            ok = ok and r["f"] is not None and F_MIN_VIA2 <= r["f"] <= F_MAX_VIA2
        return self.rep.check(tid, label, ok, detail)

    def check_ctrl_t(self, tid, label):
        f = self.dev.ctrl_t()
        if self.typ == "VIA2":
            ok = f is not None and F_MIN_VIA2 <= f <= F_MAX_VIA2
            if ok:
                self.f_ref.append(f)
        else:
            ok = f is None
        return self.rep.check(tid, label, ok, f"Strg+T -> {f if f is not None else 'keine Ausgabe'}")

    def check_off(self, tid, label):
        if self.typ != "VIA2":
            return self.rep.check(tid, label, None, "nur Via-2 (HF_DIS auf IC37)")
        out_bit, in_bit = self.dev.hf_dis()
        return self.rep.check(tid, label, bool(out_bit and in_bit),
                              f"IC37 P1.4 Ausgang={out_bit} Pegel={in_bit} (True = Transceiver aus)")

    def freq_series(self, tid, n, pause):
        vals = []
        for _ in range(n):
            f = self.dev.ctrl_t()
            vals.append(f)
            time.sleep(pause)
        good = [v for v in vals if v]
        if len(good) != n:
            return self.rep.check(tid, f"HF-Frequenz {n}x wiederholbar", False, f"Messfehler/keine Ausgabe: {vals}")
        med = statistics.median(good)
        outl = [v for v in good if abs(v - med) > 1]
        ok = all(F_MIN_VIA2 <= v <= F_MAX_VIA2 for v in good) and all(abs(v - med) <= F_OUTLIER_MAX for v in good) \
            and len(outl) <= n // 10
        target = ""
        if self.ziel is not None:                              # ab 5.142: Kanal +/- Totband (+ Abschneiden auf MHz)
            ok = ok and abs(med - self.ziel) <= F_TOTBAND + F_TRUNC
            target = f", Ziel {self.ziel} MHz (+/- {F_TOTBAND + F_TRUNC})"
        self.f_ref += [v for v in good if v not in outl]
        detail = f"{n} Messungen, Median {med} MHz, Min {min(good)} Max {max(good)}, Ausreisser > 1 MHz: {outl} " \
                 f"(erlaubt {n // 10}, je max. {F_OUTLIER_MAX} MHz){target}"
        return self.rep.check(tid, f"HF-Frequenz {n}x wiederholbar", ok, detail)

    # --- Testgruppen ----------------------------------------------------------------------------------------------
    def identify(self):
        info = self.dev.info()
        self.rep.meta.update({"version": info["version"], "serno": info["serno"], "info_line": info["line"]})
        if not info["version"]:
            raise RuntimeError("Infoseite nicht lesbar")
        r = self.dev.transmenu()
        typ, f = r["typ"], r["f"]
        self.ziel = r["ziel"]                                  # ab 5.142 Kanalfrequenz, sonst None
        self.rep.meta.update({"kanal": r["kanal"], "kanal_modus": r["kanal_modus"], "ziel_mhz": r["ziel"],
                              "ic32_stufe": r["stufe"]})
        scan = self.dev.i2c_scan() or []
        has_ic32 = any(adr == "5C" for adr, _ in scan)
        self.typ = typ
        self.rep.meta["typ"] = {"VIA2": "Via-2", "TRX": "bisheriger Transceiver"}.get(typ, "unbekannt")
        self.rep.meta["i2c_scan"] = [f"{a}h {n.strip()}" for a, n in scan]
        self.rep.check("A0", "Typ-Erkennung passt zum I2C-Scan", typ is not None and has_ic32 == (typ == "VIA2"),
                       f"Menue {typ}, IC32 (0x5C) {'vorhanden' if has_ic32 else 'fehlt'}; Scan: "
                       + ", ".join(self.rep.meta["i2c_scan"]))
        if typ == "VIA2":
            if self.ziel is not None:                          # ab 5.142: Median-Messung gegen Kanal +/- Totband
                ok = f is not None and abs(f - self.ziel) <= F_TOTBAND
                detail = f"{f} MHz, Kanal {r['kanal']} ({r['kanal_modus']}) Ziel {self.ziel} MHz +/- {F_TOTBAND}, " \
                         f"Stufe {r['stufe']}"
            else:
                ok = f is not None and F_MIN_VIA2 <= f <= F_MAX_VIA2
                detail = f"{f} MHz (Firmware ohne Kanal, nur Band {F_MIN_VIA2}-{F_MAX_VIA2})"
            self.rep.check("B2", "Frequenz im Transceiver-Menue", ok, detail)

    def flash_update(self):
        """D1/D3 und C1 im Pfad FIRM_UP_RES: gleiche Firmware neu flashen, danach ohne Stromzyklus pruefen."""
        before = self.dev.werk_config()
        lang_before = re.search(r"Language:\s*(\S+)", before)
        info_before = self.dev.info()
        self.dev.close()
        rc, out = run_tool(self.rep, ["flash_xmodem.py", "--port", self.a.port, "--bin", self.a.bin], 300)
        self.dev.open()
        m = re.findall(r"Version (\S+)", out)
        ok = rc == 0 and bool(m)
        self.rep.check("D1", "Firmware-Update RS232, Neustart", ok,
                       f"flash_xmodem rc={rc}, Banner {m[-1] if m else 'nicht gesehen'}")
        self.dev.drain(3.0)
        info_after = self.dev.info()
        after = self.dev.werk_config()
        lang_after = re.search(r"Language:\s*(\S+)", after)
        same = (lang_before and lang_after and lang_before.group(1) == lang_after.group(1)
                and info_before["serno"] == info_after["serno"] and info_before["hw"] == info_after["hw"])
        self.rep.check("D3", "Sprache/Seriennummer/HW nach Update unveraendert", bool(same),
                       f"Language {lang_before and lang_before.group(1)} -> {lang_after and lang_after.group(1)}, "
                       f"{info_before['line']} -> {info_after['line']}")
        if info_before["version"] != info_after["version"]:
            self.rep.log(f"Hinweis: Version vorher {info_before['version']}, nachher {info_after['version']}")
        self.rep.meta["version"] = info_after["version"]
        self.check_ctrl_t("D1a", "nach Update ohne Stromzyklus: Strg+T (Pfad FIRM_UP_RES)")
        self.check_off("C1a", "nach Update: Transceiver aus")

    def kurz(self):
        r = self.power_cycle(5)
        self.rep.check("A1", "Kaltstart (5 s stromlos)", r is not None, f"Hochlauf nach {r[0]} s, Version {r[1]}"
                       if r else "kein Banner")
        self.check_type("A1a", "nach Kaltstart gleicher Typ")
        self.check_off("C1", "nach Hochlauf: Transceiver aus")
        if self.typ == "VIA2":
            self.freq_series("B1", self.a.n_freq, 1.0)
            self.check_off("C2", "nach Strg+T und Transceiver-Menue: Transceiver aus")
        ok, _ = self.dev.online(self.a.online_s)
        self.rep.check("C3a", f"Online messen {self.a.online_s} s", ok, "Start/Ende ueber RS232")
        self.check_off("C3", "nach Online-Messen: Transceiver aus")
        self.test_e()
        vals, raw = self.dev.light(3)
        if vals:
            lux = [int(a) + int(b) / 100 for a, b, *_ in vals]
            self.rep.check("F3", "Lichtsensor VEML7700", all(l > 0 for l in lux), f"{lux} lx, Dimm "
                           + ",".join(v[4] for v in vals))
        else:
            self.rep.check("F3", "Lichtsensor", None, "kein VEML7700 (A/D-Fotodiode), " + raw.strip()[-40:])

    def test_e(self):
        n_err = 0
        for _ in range(3):
            answered, _, buf = self.dev.i2c_regs(NO_DEVICE_ADR)
            codes = re.findall(r"F(\d{5})", buf)
            if answered or codes != [str(DTC_I2C_EXP)]:
                n_err += 1
        self.expected_codes.add(f"F{DTC_I2C_EXP}")
        self.rep.check("E1", f"I2C Devices ohne Baustein ({NO_DEVICE_ADR}) 3x", n_err == 0,
                       f"jedes Mal F{DTC_I2C_EXP} erwartet, Abweichungen {n_err}")
        answered, regs, _ = self.dev.i2c_regs(IC32)
        if self.typ == "VIA2":
            self.rep.check("E2", "I2C Devices IC32 lesen", answered and len(regs) >= 5, f"Register {regs}")
            self.check_off("E2a", "nach I2C Devices IC32: Transceiver aus")
        else:
            self.rep.check("E2", "I2C Devices IC32 (nur Via-2)", not answered, "antwortet nicht, wie erwartet"
                           if not answered else "antwortet - widerspricht der Erkennung")

    def lang(self):
        for i in range(1, self.a.zyklen + 1):
            r = self.power_cycle(5)
            if r is None:
                self.rep.check(f"A2.{i}", "Stromzyklus", False, "kein Hochlauf gesehen")
                continue
            self.check_ctrl_t(f"A2.{i}", f"Stromzyklus {i}/{self.a.zyklen}, Hochlauf nach {r[0]} s ({r[1]})")
        for off in self.a.off_times:
            # Kurze Aussetzer: das Geraet kann neu starten (ohne Banner) oder durchlaufen - beides zulaessig,
            # entscheidend ist, dass danach der Empfaenger-Typ stimmt.
            r = self.power_cycle(off)
            how = f"Hochlauf nach {r[0]} s ({r[1]})" if r else "kein Neustart erkannt"
            self.check_ctrl_t(f"A3.{off}", f"{off} s stromlos, {how}")
        if self.typ == "VIA2":
            self.kennlinie()
        if "nightly" not in self.a.ohne:
            self.nightly()
        status = self.dev.radiostatus()
        self.rep.check("F2", "Funkmodem-Status (Strg+F)", None, " / ".join(
            l.strip() for l in status.splitlines() if l.strip())[:300])

    def kennlinie(self):
        _, before, _ = self.dev.i2c_regs(IC32)
        pts = self.dev.kennlinie() or []
        _, after, _ = self.dev.i2c_regs(IC32)
        freqs = [f for _, f in pts]
        tol = max(F_SPREAD_MAX, 2 * (max(self.f_ref) - min(self.f_ref)) if self.f_ref else F_SPREAD_MAX)
        if len(freqs) >= 2:
            rising = freqs[-1] >= freqs[0]
            back = [(pts[i][0], freqs[i - 1], freqs[i]) for i in range(1, len(freqs))
                    if (freqs[i] < freqs[i - 1] - tol if rising else freqs[i] > freqs[i - 1] + tol)]
        else:
            back = []
        ok = len(pts) >= 250 and 0 not in freqs and not back
        self.rep.check("B3", "Frequenzkennlinie IC32", ok,
                       f"{len(pts)} Stufen, {freqs[0] if freqs else '-'} -> {freqs[-1] if freqs else '-'} MHz, "
                       f"Min {min(freqs) if freqs else '-'} Max {max(freqs) if freqs else '-'}, "
                       f"Rueckspruenge > {tol} MHz: {back[:5]}")
        (self.rep.folder / "kennlinie.csv").write_text("stufe;MHz\n" + "".join(f"{s};{f}\n" for s, f in pts),
                                                       encoding="utf-8")
        self.rep.check("B4", "Kennlinie stellt IC32-Wiper wieder her", before.get(0) == after.get(0) is not None,
                       f"Register 0 vorher {before.get(0)}, nachher {after.get(0)}")
        self.check_off("C2a", "nach Kennlinie: Transceiver aus")

    def nightly(self):
        self.dev.set_time("03:14:30")
        self.rep.log("Uhr auf 03:14:30, warte auf den Neustart um 3:15 ...")
        r = self.dev.wait_boot(150.0)
        self.rep.check("C4a", "naechtlicher Neustart 3:15", r is not None,
                       f"Hochlauf nach {r[0]} s ({r[1]})" if r else "kein Neustart gesehen")
        self.check_ctrl_t("C4b", "nach Neustart 3:15: Strg+T")
        self.check_off("C4", "nach Neustart 3:15: Transceiver aus")
        self.dev.set_time(datetime.now().strftime("%H:%M:%S"))

    def protocol_check(self, base):
        count, _ = self.dev.protocol(need=0)
        new = (count or 0) - (base or 0)
        _, entries = self.dev.protocol(need=max(new, 0))
        entries = entries[:max(new, 0)]
        codes = [e.split()[0] for e in entries]
        radar = [e for e in entries if e.startswith(f"I{DTC_RADAR_TYPE} ")]
        bad = [e for e in entries if e[0] in "FW" and e.split()[0] not in self.expected_codes]
        self.rep.meta["protokoll_neu"] = entries
        self.rep.check("A2x", "kein Radar-Typwechsel (I17005) im Protokoll", not radar, f"{len(radar)} x I17005")
        self.rep.check("F4", "keine unerwarteten F/W-Codes im Protokoll", not bad,
                       f"{new} neue Eintraege ({', '.join(sorted(set(codes)))}); unerwartet: {bad[:5]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profil", choices=("kurz", "lang"), default="kurz")
    ap.add_argument("--port", default="COM3", help="RS232 am Board")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--relay-port", default="COM7")
    ap.add_argument("--relay", default="2")
    ap.add_argument("--bin", default=str(HERE.parent.parent / "sism.bin"), help="fuer D1 (nur Profil lang)")
    ap.add_argument("--ohne", default="", help="Komma-Liste: flash, bt, nightly (nur Profil lang)")
    ap.add_argument("--zyklen", type=int, default=10, help="A2: Stromzyklen (lang)")
    ap.add_argument("--off-times", default="0.1,0.5,1,10", help="A3: Stromlos-Zeiten in s (lang)")
    ap.add_argument("--online-s", type=int, default=30)
    a = ap.parse_args()
    a.ohne = {x.strip() for x in a.ohne.split(",") if x.strip()}
    a.off_times = [float(x) for x in a.off_times.split(",") if x.strip()]
    a.n_freq = 10 if a.profil == "kurz" else 20

    folder = REPORTS / f"{datetime.now():%Y-%m-%d_%H-%M-%S}_radar_{a.profil}"
    folder.mkdir(parents=True, exist_ok=True)
    rep = Report(folder)
    rep.meta.update({"profil": a.profil, "start": datetime.now().isoformat(timespec="seconds"), "port": a.port,
                     "ohne": sorted(a.ohne)})
    dev = Device(a.port, a.baud, rep)
    t = Tester(a, rep, dev)
    rc = 0
    try:
        dev.open()
        if a.profil == "lang" and "flash" not in a.ohne:
            t.identify()
            t.flash_update()                 # loescht Protokoll; danach neue Basis
        else:
            t.identify()
        base, _ = dev.protocol(need=0)
        rep.log(f"Protokoll: {base} Eintraege vor dem Test")
        t.kurz()
        if a.profil == "lang":
            if t.typ == "VIA2":
                t.freq_series("B1b", 20, 2.0)
            t.lang()
        t.protocol_check(base)
        if a.profil == "lang" and "bt" not in a.ohne:
            dev.close()
            code, out = run_tool(rep, ["bt_testlauf.py", "--profil", "kurz", "--nur", "reboot-check",
                                       "--rs232-port", a.port, "--relay-port", a.relay_port, "--relay", a.relay], 900)
            rep.check("F1", "Bluetooth reboot-check (bt_testlauf.py)", code == 0, out.strip().splitlines()[-1]
                      if out.strip() else f"rc={code}")
    except Exception as e:                   # Bedienfehler: Bericht trotzdem schreiben
        rep.check("X", "Ablauf abgebrochen", False, repr(e))
        rc = 2
    finally:
        dev.close()
        if dev.trace:
            dev.trace.close()
    res = rep.write()
    rep.log(f"Ergebnis: {res}, {len(rep.failed)} Fehler. Bericht: {folder / 'bericht.md'}")
    if rc == 0 and rep.failed:
        rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
