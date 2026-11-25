#!/usr/bin/env python3
"""usb_open_stress.py - USB-COM-Port wiederholt oeffnen, Menue abfragen, schliessen (ohne Stromzyklus).

Misst, wie oft das Oeffnen scheitert ("Ein an das System angeschlossenes Geraet funktioniert nicht", Fehler 31) oder
das Menue nicht antwortet. Fuer den Vergleich von Firmware-Staenden (5.u3 EP0 64 / 5.u4 EP0 16 / 5.157).

  python usb_open_stress.py --n 300 --pause 1 --out Testreports/usb/open_5u3
"""
import argparse, json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb_test as u  # noqa: E402
import serial  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--n", type=int, default=300)
ap.add_argument("--pause", type=float, default=1.0)
ap.add_argument("--out", required=True)
ap.add_argument("--stop-on-fail", action="store_true", help="beim ersten Fehler aufhoeren (Protokoll danach lesen)")
ap.add_argument("--diag", type=int, default=0, help="alle N Versuche (und nach Fehlern) Infoseite mit USB-Diag lesen")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
u.LOG = open(os.path.join(a.out, "log.txt"), "a", encoding="utf-8")
w = u.Watcher(); w.start()
res = []; fails = 0; t_start = time.time()
for i in range(1, a.n + 1):
    ports = u.usb_ports()
    if not ports:
        t = time.time()
        while not u.usb_ports() and time.time() - t < 60:
            time.sleep(0.3)
        ports = u.usb_ports()
        if not ports:
            u.log("kein Port mehr - Abbruch"); break
    dev = next(iter(ports))
    t0 = time.time(); err = None; ok = False
    try:
        with serial.Serial(dev, 115200, timeout=0.2) as s:
            t_open = time.time() - t0
            s.dtr = True
            s.reset_input_buffer()
            s.write(b"H")
            out = u.read_for(s, 2.0, b"1...6?")
            ok = "Hauptmen" in out
            if not ok:
                err = "kein Menue: " + out[-80:].replace("\r", " ").replace("\n", " ")
    except Exception as e:
        t_open = time.time() - t0
        err = f"{type(e).__name__}: {e}"
    e = {"i": i, "ok": ok, "open_s": round(t_open, 2), "fehler": err}
    res.append(e)
    if not ok:
        fails += 1
        u.log(f"FEHLER {i}: {json.dumps(e, ensure_ascii=False)}")
        if a.stop_on_fail:
            break
    if i % 50 == 0:
        u.log(f"{i} Versuche, {fails} Fehler")
    if a.diag and (i % a.diag == 0 or not ok):
        time.sleep(a.pause)
        d = u.menu_check(dev) if u.usb_ports() else {}
        u.log(f"Diag {i}: {d.get('diag')} (menu={d.get('menu')})")
    json.dump(res, open(os.path.join(a.out, "ergebnis.json"), "w"), indent=0, ensure_ascii=False)
    time.sleep(a.pause)
w.stop = True
u.log(f"ENDE: {len(res)} Versuche, {fails} Fehler, Port-Wechsel {len(w.events)}, Dauer {round((time.time()-t_start)/60,1)} min")
