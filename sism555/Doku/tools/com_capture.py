#!/usr/bin/env python3
"""com_capture.py - RS232-Terminal (COM3) und USB-Port (VID 0x1FC9) gleichzeitig mitschneiden.

  python com_capture.py --sek 300 --out Testreports/usb/capture.txt

Schreibt jede RS232-Zeile und jedes Erscheinen/Verschwinden des USB-Ports mit Zeitstempel.
"""
import argparse, time
import serial, serial.tools.list_ports as lp

ap = argparse.ArgumentParser()
ap.add_argument("--port", default="COM3")
ap.add_argument("--sek", type=float, default=300)
ap.add_argument("--out", required=True)
a = ap.parse_args()
f = open(a.out, "a", encoding="utf-8")
t0 = time.time()


def w(msg):
    f.write(f"{time.strftime('%H:%M:%S')} +{time.time()-t0:7.2f} {msg}\n"); f.flush()


s = serial.Serial(a.port, 115200, timeout=0.05)
cur = None; line = b""
w(f"Start Mitschnitt {a.port}")
while time.time() - t0 < a.sek:
    ps = tuple(sorted((p.device, p.serial_number) for p in lp.comports() if p.vid == 0x1FC9))
    if ps != cur:
        w(f"USB {ps}"); cur = ps
    d = s.read(512)
    for c in d:
        if c in (10, 13):
            if line:
                w("RS232 " + line.decode("latin-1")); line = b""
        else:
            line += bytes([c])
if line:
    w("RS232 " + line.decode("latin-1"))
w("Ende")
