#!/usr/bin/env python3
"""usb_close_provoke.py - USB-Port oeffnen, Taste senden und SOFORT schliessen, waehrend das Geraet noch ausgibt.

Loest den Watchdog-Haenger beim Schliessen des Ports gezielt aus (ca. 5 % je Versuch mit 5.u3-5.u11).

  python usb_close_provoke.py --n 200 --out Testreports/usb/provoke.txt
"""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb_test as u  # noqa: E402
import serial  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--n", type=int, default=200)
ap.add_argument("--pause", type=float, default=1.0)
ap.add_argument("--out", required=True)
a = ap.parse_args()
u.LOG = open(a.out, "a", encoding="utf-8")
w = u.Watcher(); w.start()
fails = 0; t0 = time.time()
for i in range(1, a.n + 1):
    ps = u.usb_ports()
    if not ps:
        t = time.time()
        while not u.usb_ports() and time.time() - t < 90:
            time.sleep(0.3)
        ps = u.usb_ports()
        if not ps:
            u.log(f"kein Port mehr bei {i} - Abbruch"); break
    dev = next(iter(ps))
    try:
        with serial.Serial(dev, 115200, timeout=0.2) as s:
            s.dtr = True
            s.write(b"H")
    except Exception as e:
        fails += 1
        u.log(f"FEHLER {i}: {type(e).__name__} {str(e)[:90]}")
    time.sleep(a.pause)
u.log(f"ENDE: {i} Versuche, {fails} Oeffnungsfehler, Port-Wechsel {len(w.events)}, {round((time.time()-t0)/60,1)} min")
w.stop = True
