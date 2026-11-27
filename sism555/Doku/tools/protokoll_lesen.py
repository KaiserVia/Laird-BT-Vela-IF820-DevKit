#!/usr/bin/env python3
"""protokoll_lesen.py - Geraeteprotokoll ueber RS232 lesen (Hauptmenue -> 6 -> 1), Loeschfrage immer mit NEIN.

  python protokoll_lesen.py --port COM3 --out Testreports/usb/protokoll.txt

Jede Taste erst, wenn das erwartete Menue sichtbar ist.
"""
import argparse, sys, time
import serial


def read_for(s, secs, until=None):
    end = time.time() + secs; buf = b""
    while time.time() < end:
        d = s.read(s.in_waiting or 1)
        if d:
            buf += d
            if until and until in buf:
                break
    return buf.decode("latin-1", "replace")


ap = argparse.ArgumentParser()
ap.add_argument("--port", default="COM3")
ap.add_argument("--out", required=True)
a = ap.parse_args()
with serial.Serial(a.port, 115200, timeout=0.2) as s:
    s.reset_input_buffer()
    s.write(b"H"); out = read_for(s, 2.0, b"1...6?")
    if "Hauptmen" not in out:
        s.write(b"\r"); out += read_for(s, 2.0, b"1...6?")
    if "Hauptmen" not in out:
        sys.exit("kein Hauptmenue: " + out[-200:])
    s.write(b"6"); out = read_for(s, 3.0, b"1...9?")
    if "1. Protokoll" not in out:
        sys.exit("keine Infoseite: " + out[-200:])
    s.write(b"1"); prot = read_for(s, 10.0, b"(j/n)?")
    if "ausgeben" in prot:                      # "Daten ausgeben (j/n)?" -> ja
        time.sleep(0.3)
        s.write(b"j"); prot += read_for(s, 60.0, b"schen (j/n)?")
    if "schen (j/n)?" in prot:                  # "Daten loeschen (j/n)?" -> immer NEIN
        time.sleep(0.3)
        s.write(b"n"); prot += read_for(s, 3.0, b"?")
    open(a.out, "w", encoding="utf-8").write(prot)
    print(prot[-3000:])
    s.write(b"H"); read_for(s, 2.0, b"1...6?")
