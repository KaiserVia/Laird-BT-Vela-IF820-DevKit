#!/usr/bin/env python3
"""device_setup.py - sism555-Geraet ueber RS232 fuer Mobilfunk/MQTT einstellen und pruefen.

Liest die Sollwerte aus device_setup.json (APN, Roaming, Servertyp, MQTT-Server/-Port, Uhr) und
vergleicht sie mit dem Geraet. Nur was abweicht, wird geaendert. MQTT-User/-Passwort und die
Zertifikate im Modem werden nicht angefasst.

  python device_setup.py --check                 # nur lesen und vergleichen, nichts aendern
  python device_setup.py                         # abweichende Werte einstellen, danach erneut pruefen
  python device_setup.py --online                # zusaetzlich: MQTT-Debug an, warten bis online, wieder aus
  python device_setup.py --persist               # zusaetzlich: stromlos schalten (Relais) und nachpruefen
  python device_setup.py --net                   # zusaetzlich: Netzregistrierung abfragen (langsam, nur offline)

Hinweise:
- Die Firmware schreibt die Parameter erst beim Ausschalten (Power-Down-Interrupt) oder bei einem
  Neustart ins Flash. --persist prueft, dass sie einen Stromzyklus ueberleben.
- "Change configuration" im versteckten Konfigurationsmenue mit "Modem/Email j" setzt Servertyp, APN
  und Roaming zurueck. Dieses Skript ruft das Menue nie auf; fehlt das Modem in "Schnittstellen",
  bricht es mit einem Hinweis ab.
- Im Testmenue setzt die Taste X alle Parameter auf Werkszustand. Das Skript sendet nur Ziffern,
  'H', 'y'/'n', Strg+F und Strg+G.

Rueckgabe: 0 = alles wie gewuenscht, 1 = Abweichung/Fehler, 2 = Geraet antwortet nicht.
"""
import argparse
import datetime
import json
import os
import re
import sys
import time

import serial

STRF, STRG = b"\x06", b"\x07"
PROMPT = re.compile(r"\.\.\.\d\?\s*$")           # "Ihre Auswahl 1...9?" (sprachneutral)
JN = re.compile(r"\((?:j|y|o|s|t|a)/n\)\?\s*$")  # "(j/n)?" am Zeilenende
HERE = os.path.dirname(os.path.abspath(__file__))


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


class Dev:
    def __init__(self, port, baud, trace):
        self.s = serial.Serial(port, baud, timeout=0.1)
        self.trace = open(trace, "a", encoding="utf-8") if trace else None

    def close(self):
        self.s.close()
        if self.trace:
            self.trace.close()

    def _t(self, tag, b):
        if self.trace and b:
            self.trace.write(f"{time.strftime('%H:%M:%S')} {tag} {b.decode('latin-1', 'replace')!r}\n")

    def send(self, b, gap=0.15):
        if isinstance(b, str):
            b = b.encode("latin-1")
        self._t("TX", b)
        for ch in b:                              # Zeichen einzeln: die Firmware liest per getc() mit Echo
            self.s.write(bytes([ch]))
            time.sleep(0.02)
        time.sleep(gap)

    def read_until(self, pat, timeout, quiet=0.3):
        """Liest, bis pat (Regex) im Puffer passt und danach 'quiet' s Ruhe ist. Gibt Text oder None."""
        buf = b""
        end = time.time() + timeout
        hit_at = None
        while time.time() < end:
            d = self.s.read(self.s.in_waiting or 1)
            if d:
                self._t("RX", d)
                buf += d
                hit_at = None
            txt = buf.decode("latin-1", "replace").replace("\r", "")
            if pat.search(txt):
                if hit_at is None:
                    hit_at = time.time()
                elif time.time() - hit_at >= quiet:
                    return txt
        txt = buf.decode("latin-1", "replace").replace("\r", "")
        return txt if pat.search(txt) else None

    def cmd(self, keys, pat=PROMPT, timeout=8, quiet=0.3):
        self.s.reset_input_buffer()
        self.send(keys)
        return self.read_until(pat, timeout, quiet)

    def main_menu(self):
        """Zum Hauptmenue. 'H' wirkt aus jedem Menue; ein offener Dialog bekommt vorher CR."""
        for _ in range(8):                       # die Firmware kann in einer AT-Abfrage einige Zeit blockieren
            out = self.cmd("H", timeout=6)
            if out and re.search(r"\n1\. .*\n2\. .*\n.*\n.*\n5\. .*\n6\. .*\n.*1\.\.\.6\?\s*$", out):
                return out
            self.send("\r", gap=1.0)
        return None


def parse_info(t):
    r = {}
    m = re.search(r"Version (\S+?),", t)
    r["version"] = m.group(1) if m else None
    m = re.search(r"(?:Seriennummer|Serial number|N° de série)\s*:\s*(\S+)", t)
    r["serno"] = m.group(1) if m else None
    m = re.search(r"(?:Schnittstellen|Interfaces)\s*:\s*(.*)", t)
    r["interfaces"] = m.group(1).strip() if m else ""
    m = re.search(r"\n3\. .*?(\d\d)\.(\d\d)\.(\d{4})", t)
    n = re.search(r"\n4\. .*?(\d\d):(\d\d):(\d\d)", t)
    if m and n:
        r["clock"] = datetime.datetime(int(m.group(3)), int(m.group(2)), int(m.group(1)),
                                       int(n.group(1)), int(n.group(2)), int(n.group(3)))
    return r


def onoff(word):
    return word.strip().lower() in ("ein", "on", "activé", "attiva", "an")


def parse_gsm_menu(t):
    m = re.search(r"\n3\. [^:\n]*:\s+(\S+)", t)
    return {"servertyp": m.group(1) if m else None}


def parse_apn(t):
    def f(label):
        m = re.search(label + r":\s*(.*)", t)
        v = m.group(1).strip() if m else ""
        return "" if re.match(r"(nicht definiert|not defined|non défini|non definito)", v, re.I) else v
    m = re.search(r"ROAMING:\s*(\S+)", t)
    return {"apn": f("APN"), "user": f("USER"), "pass": f("PASS"), "roaming": onoff(m.group(1)) if m else None}


def parse_server(t):
    m = re.search(r"MQTT-SERVER:\s*(\S*)", t)
    p = re.search(r"MQTT-PORT:\s*(\d+)", t)
    return {"server": m.group(1) if m else None, "port": int(p.group(1)) if p else None,
            "connected": "MQTT Server connected" in t}


def parse_report(t):
    r = {}
    for k, pat in (("connect", r"Connect=([0-9A-Fa-f]+)h"), ("csq", r"CSQ=(\d+)"),
                   ("state", r"State=(\d+)"), ("gsmpwr", r"GSM: PWR_REG=(\d+)")):
        m = re.search(pat, t)
        if m:
            r[k] = int(m.group(1), 16 if k == "connect" else 10)
    return r


class Setup:
    def __init__(self, dev, cfg, check_only):
        self.d = dev
        self.c = cfg
        self.check_only = check_only
        self.diffs = []
        self.errors = []
        self.changed = []

    # --- Navigation ---------------------------------------------------------------------------
    def gsm_menu(self):
        if not self.d.main_menu():
            raise RuntimeError("Hauptmenue nicht erreichbar")
        out = self.d.cmd("5")
        out = self.d.cmd("2", timeout=10)
        if not out or not re.search(r"\n3\. ", out):
            raise RuntimeError("Mobilfunk-Menue nicht erreichbar (Modem nicht eingetragen?): " + repr((out or "")[-200:]))
        return out

    def report(self):
        self.d.main_menu()
        self.d.s.reset_input_buffer()
        self.d.send(STRF)
        out = self.d.read_until(re.compile(r"Report:.*\n(?:.*\n){2,}"), 5, quiet=0.6) or ""
        return parse_report(out), out

    def debug_menu(self, key, wait_pat=PROMPT, timeout=10):
        """Strg+G im Mobilfunk-Menue, dann Menuepunkt key. Gibt (Menuetext, Ausgabe nach key)."""
        self.gsm_menu()
        menu = self.d.cmd(STRG, timeout=6) or ""
        if "MQTT debug mode" not in menu:
            raise RuntimeError("Debug-Menue (Strg+G) nicht erreichbar")
        out = self.d.cmd(key, pat=wait_pat, timeout=timeout) if key else ""
        return menu, out or ""

    def mqtt_debug_state(self):
        menu, _ = self.debug_menu(None)
        self.d.send("9", gap=0.5)
        m = re.search(r"MQTT debug mode:\s*(\S+)", menu)
        return onoff(m.group(1)) if m else None

    # --- Einzelschritte -----------------------------------------------------------------------
    def go_offline(self):
        """Aenderungen an Servertyp/APN sind gesperrt, solange GSM/MQTT UART1 halten."""
        rep, _ = self.report()
        if not rep.get("connect", 0) & 0x22:
            return
        log(f"Geraet ist online (Connect={rep['connect']:X}h) - MQTT-Debug aus und vom Server trennen")
        if self.mqtt_debug_state():
            self.debug_menu("4")
        self.debug_menu("8", timeout=20)
        end = time.time() + 90
        while time.time() < end:
            rep, _ = self.report()
            if not rep.get("connect", 0) & 0x22:
                return
            time.sleep(5)
        raise RuntimeError("Geraet bleibt online, Einstellungen sind dann gesperrt")

    def step_info_clock(self):
        if not self.d.main_menu():
            raise RuntimeError("Hauptmenue nicht erreichbar")
        out = self.d.cmd("6") or ""
        info = parse_info(out)
        log(f"Geraet: Version {info['version']}, Seriennummer {info['serno']}, Schnittstellen: {info['interfaces']}")
        if not re.search(r"Modem|LTE|GSM", info["interfaces"]):
            self.errors.append("Modem nicht in der Geraetekonfiguration eingetragen (Testmenue -> versteckte "
                               "Konfiguration). ACHTUNG: danach APN, Servertyp und Roaming neu einstellen.")
        if self.c.get("set_clock", True) and info.get("clock"):
            now = datetime.datetime.now()
            dev = info["clock"]
            delta = abs((dev - now).total_seconds())
            if delta > self.c.get("clock_tolerance_s", 60):
                self.diffs.append(f"Uhr: Geraet {dev:%d.%m.%Y %H:%M:%S}, PC {now:%d.%m.%Y %H:%M:%S}")
                if not self.check_only:
                    if dev.date() != now.date() or abs((dev - now).total_seconds()) > 43200:
                        self.d.cmd("3", pat=re.compile(r"= $"), timeout=4)
                        self.d.cmd(datetime.datetime.now().strftime("%d.%m.%Y"), timeout=6)
                    self.d.cmd("4", pat=re.compile(r"= $"), timeout=4)
                    out = self.d.cmd(datetime.datetime.now().strftime("%H:%M:%S"), timeout=6) or ""
                    info2 = parse_info(out)
                    log(f"Uhr gestellt: {info2.get('clock')}")
                    self.changed.append("Uhr")
            else:
                log(f"Uhr: ok (Abweichung {delta:.0f} s)")
        self.d.send("9", gap=0.5)
        return info

    def step_servertyp(self):
        want = self.c["servertyp"].upper()
        out = self.gsm_menu()
        have = (parse_gsm_menu(out)["servertyp"] or "").upper()
        if have == want:
            log(f"Servertyp: {have} ok")
            return
        self.diffs.append(f"Servertyp: {have} statt {want}")
        if self.check_only:
            return
        idx = {"SMTP": "1", "MQTT": "2"}[want]
        self.d.cmd("3", timeout=5)
        out = self.d.cmd(idx, timeout=6) or ""
        if (parse_gsm_menu(out)["servertyp"] or "").upper() != want:
            self.errors.append("Servertyp liess sich nicht setzen: " + repr(out[-200:]))
        else:
            log(f"Servertyp auf {want} gesetzt (Firmware setzt dabei Server/Port auf Standard zurueck)")
            self.changed.append("Servertyp")

    def step_apn(self):
        self.gsm_menu()
        out = self.d.cmd("4", pat=JN, timeout=5) or ""
        have = parse_apn(out)
        want = {"apn": self.c["apn"], "user": self.c.get("apn_user", ""), "pass": self.c.get("apn_pass", ""),
                "roaming": bool(self.c.get("roaming", True))}
        d = [k for k in want if (have.get(k) if k != "pass" else have.get(k, "").strip("*")) != want[k]
             and not (k == "pass" and want[k] == "" and have.get(k) == "")]
        if not d:
            log(f"APN: {have['apn']}, Roaming {'ein' if have['roaming'] else 'aus'} ok")
            self.d.send("n", gap=0.5)
            return
        self.diffs.append("APN: " + ", ".join(f"{k}={have.get(k)!r} statt {want[k]!r}" for k in d))
        if self.check_only:
            self.d.send("n", gap=0.5)
            return
        self.d.send("y", gap=0.5)
        for k in ("apn", "user", "pass"):
            v = want[k]
            # leere Eingabe behaelt den alten Wert, ein einzelnes Leerzeichen loescht ihn (readline())
            self.d.send((v if v else (" " if have.get(k) else "")) + "\r", gap=0.4)
        self.d.s.reset_input_buffer()
        self.d.send("2" if want["roaming"] else "1")
        out = self.d.read_until(PROMPT, 240, quiet=1.0) or ""   # Modem einschalten + PDP-Aktivierung: >90 s gemessen
        if "APN ok" in out or re.search(r"\bok\b", out.split("APN")[-1]):
            log("APN gesetzt und im Modem aktiviert (APN ok)")
        elif "12005" in out:
            # gespeichert ist der APN trotzdem; ob das Geraet damit online kommt, zeigt --online
            log("WARNUNG: APN gespeichert, aber der Modemtest scheiterte (F12005) - Netzzugang mit --online pruefen")
        else:
            log("APN gesetzt (keine Modem-Rueckmeldung erkannt): " + repr(out[-200:]))
        self.changed.append("APN")

    def step_server(self):
        if self.c["servertyp"].upper() != "MQTT":
            return
        self.gsm_menu()
        out = self.d.cmd("6", pat=JN, timeout=5) or ""
        have = parse_server(out)
        if have["connected"]:
            log("MQTT-Server ist verbunden - Pruefung ohne Aenderung")
            self.d.send("n", gap=0.5)
        want_s, want_p = self.c["mqtt_server"], int(self.c.get("mqtt_port", 8883))
        if have["server"] == want_s and have["port"] == want_p:
            log(f"MQTT-Server: {want_s}:{want_p} ok")
            if not have["connected"]:
                self.d.send("n", gap=0.5)
            return
        self.diffs.append(f"MQTT-Server: {have['server']}:{have['port']} statt {want_s}:{want_p}")
        if self.check_only or have["connected"]:
            if not have["connected"]:
                self.d.send("n", gap=0.5)
            return
        self.d.send("y", gap=0.5)
        self.d.send(want_s + "\r", gap=0.4)
        self.d.send(f"{want_p}\r", gap=0.4)
        self.d.send("\r", gap=0.4)                 # USER behalten
        self.d.s.reset_input_buffer()
        self.d.send("\r")                          # PASS behalten -> Firmware verbindet testweise
        out = self.d.read_until(PROMPT, 120, quiet=1.0) or ""
        if "MQTT Server connected" in out or re.search(r"Server\s*ok", out, re.I):
            log("MQTT-Server gesetzt, Testverbindung ok")
        else:
            log("MQTT-Server gesetzt, Testverbindung ohne Erfolgsmeldung: " + repr(out[-250:]))
            self.errors.append("MQTT-Testverbindung nach dem Einstellen fehlgeschlagen")
        self.changed.append("MQTT-Server")

    def step_sim(self):
        self.gsm_menu()
        self.d.cmd("2", pat=re.compile(r"= ?$"), timeout=5)
        out = self.d.cmd("\r", timeout=30, quiet=0.8) or ""   # leere Eingabe: PIN bleibt, nur Status abfragen
        if "SIM not inserted" in out or "SIM failure" in out:
            self.errors.append("SIM-Karte nicht erkannt (+CME ERROR)")
            log("SIM: NICHT erkannt")
        elif "ERROR" in out:
            self.errors.append("SIM-Abfrage: " + out.strip().splitlines()[0][:80])
        else:
            log("SIM: ok")

    def step_registration(self):
        # test_registration() fragt bis zu 16x ab und blockiert dabei das Menue (>60 s gemessen)
        _, out = self.debug_menu("2", wait_pat=re.compile(r"(NOT REGISTERED|REGISTERED|ROAMING)"), timeout=240)
        m = re.search(r"(NOT REGISTERED|REGISTERED|ROAMING)", out)
        res = m.group(1) if m else "keine Antwort"
        log(f"Netz: {res}")
        if res not in ("REGISTERED", "ROAMING"):
            self.errors.append(f"Netzregistrierung: {res}")
        time.sleep(1)

    def step_online(self, timeout):
        """MQTT-Debug an, auf 'MQTT Server connected' warten, MQTT-Debug wieder aus und trennen."""
        if self.mqtt_debug_state():
            log("MQTT-Debug war schon an")
        else:
            self.debug_menu("4")
        log(f"MQTT-Debug an - warte bis zu {timeout} s auf die Serververbindung ...")
        self.d.main_menu()
        t0 = time.time()
        out = self.d.read_until(re.compile(r"MQTT Server connected[\s\S]*PUBACK"), timeout, quiet=1.0)
        if out:
            log(f"online nach {time.time() - t0:.0f} s (MQTT Server connected, PUBACK erhalten)")
        else:
            self.errors.append(f"nicht online innerhalb {timeout} s")
            log("NICHT online")
        rep, _ = self.report()
        log(f"Bericht: {rep}")
        if not self.c.get("keep_online", False):
            self.debug_menu("4")
            self.debug_menu("8", timeout=20)
            log("MQTT-Debug aus, vom Server getrennt")
        return bool(out)


def load_cfg(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def run_pass(dev, cfg, check_only, online, online_timeout, net=False):
    s = Setup(dev, cfg, check_only)
    info = s.step_info_clock()
    if not s.errors:
        if not check_only:
            s.go_offline()
        rep, _ = s.report()
        online_now = bool(rep.get("connect", 0) & 0x20)     # MQTT_LINK
        s.step_servertyp()
        s.step_server()                 # nach dem Servertyp: ein Wechsel setzt den Server zurueck
        s.step_apn()
        if online_now:
            # SIM-/Netzabfrage schickt AT-Kommandos in die laufende MQTT-Sitzung (Fehlanzeige, stoert die Sitzung)
            log(f"Geraet ist online (Connect={rep['connect']:X}h, CSQ={rep.get('csq')}) - SIM und Netz damit ok, "
                "Abfrage uebersprungen")
        else:
            s.step_sim()
            if net:
                s.step_registration()
        if online:
            s.step_online(online_timeout)
    dev.main_menu()
    return s, info


def power_cycle(relay_port, relay, off_s):
    with serial.Serial(relay_port, 19200, timeout=1) as r:
        r.write(bytes([101 + relay - 1]))
        time.sleep(off_s)
        r.write(bytes([111 + relay - 1]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--config", default=os.path.join(HERE, "device_setup.json"))
    ap.add_argument("--check", action="store_true", help="nur pruefen, nichts aendern")
    ap.add_argument("--online", action="store_true", help="zusaetzlich Online-Test (MQTT-Debug)")
    ap.add_argument("--online-timeout", type=int, default=300)
    ap.add_argument("--net", action="store_true", help="Netzregistrierung abfragen (langsam, bis 4 min; nur offline)")
    ap.add_argument("--persist", action="store_true", help="nach dem Einstellen Stromzyklus und Nachpruefung")
    ap.add_argument("--relay-port", default="COM7")
    ap.add_argument("--relay", type=int, default=2)
    ap.add_argument("--trace", default=None, help="Rohmitschnitt in diese Datei")
    a = ap.parse_args()
    cfg = load_cfg(a.config)
    trace = a.trace or os.path.join(HERE, "Testreports", time.strftime("%Y-%m-%d_%H-%M-%S") + "_device_setup.log")
    os.makedirs(os.path.dirname(trace), exist_ok=True)
    dev = Dev(a.port, a.baud, trace)
    log(f"Mitschnitt: {trace}")
    rc = 0
    try:
        if not dev.main_menu():
            log("FEHLER: Geraet antwortet nicht auf RS232")
            return 2
        s, info = run_pass(dev, cfg, a.check, a.online, a.online_timeout, a.net)
        if s.changed and not a.check:
            log("Geaendert: " + ", ".join(s.changed) + " - Nachpruefung")
            s2, _ = run_pass(dev, cfg, True, False, 0)
            s.diffs = s2.diffs
            s.errors += [e for e in s2.errors if e not in s.errors]
        if a.persist:
            log("Stromzyklus 5 s (Relais), damit die Parameter im Flash landen und nachgeprueft werden")
            dev.s.close()
            power_cycle(a.relay_port, a.relay, 5)
            time.sleep(40)
            dev.s.open()
            if not dev.main_menu():
                log("FEHLER: Geraet nach dem Stromzyklus nicht erreichbar")
                return 2
            s3, _ = run_pass(dev, cfg, True, False, 0)
            if s3.diffs:
                s.diffs += ["nach Stromzyklus: " + x for x in s3.diffs]
        print()
        log("=== Ergebnis ===")
        for x in s.diffs:
            log(("ABWEICHUNG: " if a.check else "NOCH ABWEICHEND: ") + x)
        for x in s.errors:
            log("FEHLER: " + x)
        if not s.diffs and not s.errors:
            log("alles wie gewuenscht")
        else:
            rc = 1
    except Exception as e:
        log(f"FEHLER: {e}")
        rc = 1
    finally:
        dev.close()
    return rc


if __name__ == "__main__":
    sys.exit(main())
