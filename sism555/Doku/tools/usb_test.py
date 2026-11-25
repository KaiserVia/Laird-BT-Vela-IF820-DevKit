#!/usr/bin/env python3
"""usb_test.py - USB-Kabelanschluss (CDC, VID 0x1FC9) im Dauertest pruefen (Branch usb-verbindung, 5.u1).

Je Zyklus: Geraet ueber das Relais stromlos (USB-Kabel bleibt am PC), wieder einschalten, dann beobachten:
  - wann erscheint der USB-COM-Port, verschwindet er wieder (An-/Abmelde-Schleife)?
  - antwortet das Menue ueber USB, Infoseite: Version, Seriennummer, Tiefschlaf-Zaehler
  - danach Port geschlossen lassen und weiter beobachten (Leerlauf: schlaeft das Geraet weg?)

  python usb_test.py --zyklen 10 --aus 5 --leerlauf 60 --out ..\\..\\..\\usb_test_5157

Relais: Devantech USB-RLY16L an COM7, Relais 2 (angezogen = Geraet stromlos), siehe relay_power_Anleitung.md.
Ergebnis: <out>/log.txt (alle Ereignisse) und <out>/ergebnis.json (je Zyklus).
"""
import argparse, json, os, re, threading, time
import serial
import serial.tools.list_ports as lp

VID = 0x1FC9
T0 = time.time()
LOG = None


def log(msg):
    line = f"{time.strftime('%H:%M:%S')} +{time.time()-T0:8.1f}s {msg}"
    print(line, flush=True)
    if LOG:
        LOG.write(line + "\n"); LOG.flush()


def usb_ports():
    return {p.device: (p.serial_number, p.hwid) for p in lp.comports() if p.vid == VID}


class Watcher(threading.Thread):
    """Protokolliert jedes Erscheinen/Verschwinden des USB-COM-Ports (Abtastung 50 ms)."""
    def __init__(self):
        super().__init__(daemon=True)
        self.cur = usb_ports(); self.events = []; self.lock = threading.Lock(); self.stop = False

    def run(self):
        while not self.stop:
            now = usb_ports()
            for d in now.keys() - self.cur.keys():
                self.add("da", d, now[d])
            for d in self.cur.keys() - now.keys():
                self.add("weg", d, self.cur[d])
            self.cur = now
            time.sleep(0.05)

    def add(self, kind, dev, info):
        with self.lock:
            self.events.append((time.time(), kind, dev, info[0]))
        log(f"USB {kind:3s} {dev} SER={info[0]}")

    def since(self, t):
        with self.lock:
            return [e for e in self.events if e[0] >= t]


def relay(port, on):
    """on=True: Relais angezogen = Geraet stromlos."""
    with serial.Serial(port, 19200, timeout=0.5) as s:
        s.write(bytes([102 if on else 112])); time.sleep(0.2)


def read_for(s, secs, until=None):
    end = time.time() + secs; buf = b""
    while time.time() < end:
        d = s.read(s.in_waiting or 1)
        if d:
            buf += d
            if until and until in buf:
                break
    return buf.decode("latin-1", "replace")


def menu_check(dev):
    """Hauptmenue -> 6 Information lesen -> 9 zurueck. Jede Taste erst, wenn das erwartete Menue sichtbar ist."""
    r = {"menu": False}
    try:
        with serial.Serial(dev, 115200, timeout=0.2) as s:
            s.dtr = True
            time.sleep(0.3); s.reset_input_buffer()
            s.write(b"H"); out = read_for(s, 1.5, b"1...6?")
            if "Hauptmen" not in out:
                s.write(b"\r"); out += read_for(s, 2.0, b"1...6?")
            if "Hauptmen" not in out:
                r["fehler"] = "kein Hauptmenue: " + out[-120:].replace("\r", " ").replace("\n", " ")
                return r
            s.write(b"6"); out = read_for(s, 3.0, b"1...9?")
            if "Information" not in out:
                r["fehler"] = "keine Infoseite"; return r
            m = re.search(r"Version (\S+),", out); r["version"] = m.group(1) if m else None
            m = re.search(r"Seriennummer: (\S+)", out); r["serno"] = m.group(1) if m else None
            m = re.search(r"Tiefschlaf seit Einschalten: (\d+)", out); r["schlaf"] = int(m.group(1)) if m else None
            s.write(b"9"); out = read_for(s, 2.0, b"1...6?")
            r["menu"] = "Hauptmen" in out
    except Exception as e:
        r["fehler"] = f"{type(e).__name__}: {e}"
    return r


def zyklus(a, w, n, aus):
    res = {"zyklus": n, "aus_s": aus}
    relay(a.relay_port, True); log(f"--- Zyklus {n}: Geraet stromlos ({aus} s)")
    time.sleep(aus)
    res["port_blieb_stromlos"] = bool(usb_ports())
    t_on = time.time(); relay(a.relay_port, False); log("Geraet ein")
    # warten, bis der Port da ist und 3 s stabil bleibt
    t_da = None
    while time.time() - t_on < a.hochlauf:
        p = usb_ports()
        if p and t_da is None:
            t_da = time.time()
        if not p:
            t_da = None
        if t_da and time.time() - t_da >= 3:
            break
        time.sleep(0.1)
    ev = w.since(t_on)
    first = next((e for e in ev if e[1] == "da"), None)
    res["t_erst_da_s"] = round(first[0] - t_on, 1) if first else None
    res["wechsel_hochlauf"] = len(ev)
    if not usb_ports():
        res["ok"] = False; res["fehler"] = "Port nicht stabil da"; log("FEHLER: Port nicht stabil da"); return res
    dev, (ser, hw) = next(iter(usb_ports().items()))
    res.update({"port": dev, "usb_ser": ser})
    res.update(menu_check(dev))
    # Leerlauf: Port zu, beobachten
    t_idle = time.time(); time.sleep(a.leerlauf)
    ev2 = w.since(t_idle)
    res["wechsel_leerlauf"] = len(ev2)
    if a.leerlauf and usb_ports():
        dev2 = next(iter(usb_ports()))
        r2 = menu_check(dev2)
        res["menu_nach_leerlauf"] = r2.get("menu"); res["schlaf_nach_leerlauf"] = r2.get("schlaf")
    res["ok"] = bool(res.get("menu")) and res["wechsel_hochlauf"] == 1 and res["wechsel_leerlauf"] == 0 \
        and (not a.leerlauf or res.get("menu_nach_leerlauf"))
    log(f"Zyklus {n}: {json.dumps(res, ensure_ascii=False)}")
    return res


def main():
    global LOG
    ap = argparse.ArgumentParser()
    ap.add_argument("--zyklen", type=int, default=10)
    ap.add_argument("--aus", default="5", help="Sekunden stromlos, auch Liste 0.5,2,10 (reihum)")
    ap.add_argument("--hochlauf", type=float, default=120.0, help="max. Sekunden bis der Port stabil da ist")
    ap.add_argument("--leerlauf", type=float, default=60.0, help="Sekunden Port geschlossen beobachten")
    ap.add_argument("--relay-port", default="COM7")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    LOG = open(os.path.join(a.out, "log.txt"), "a", encoding="utf-8")
    log(f"Start {vars(a)}  USB-Ports jetzt: {usb_ports()}")
    w = Watcher(); w.start()
    out = []
    try:
        for n in range(1, a.zyklen + 1):
            liste = [float(x) for x in a.aus.split(",")]
            out.append(zyklus(a, w, n, liste[(n - 1) % len(liste)]))
            json.dump(out, open(os.path.join(a.out, "ergebnis.json"), "w"), indent=1, ensure_ascii=False)
    finally:
        relay(a.relay_port, False)          # Geraet am Ende immer mit Strom
        w.stop = True
    ok = sum(1 for r in out if r.get("ok"))
    log(f"ENDE: {ok}/{len(out)} Zyklen ok")


if __name__ == "__main__":
    main()
