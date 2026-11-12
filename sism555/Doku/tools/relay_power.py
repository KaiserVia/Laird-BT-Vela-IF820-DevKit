#!/usr/bin/env python3
"""Versorgung des Geraets ueber das Devantech USB-RLY16L schalten.

Verdrahtung: Netzteil-Plus an "relay" (COM), Geraet an "0" (NC). Relais aus = Geraet hat Strom,
Relais ein = Geraet stromlos.

  python relay_power.py status
  python relay_power.py aus  [--port COM7] [--relay 2]    # Geraet stromlos (Relais ein)
  python relay_power.py ein  [--port COM7] [--relay 2]    # Geraet unter Strom (Relais aus)
  python relay_power.py zyklus 15                          # 15 s stromlos, dann wieder ein

Befehle laut Devantech: 100 alle ein, 110 alle aus, 101..108 Relais 1..8 ein, 111..118 aus,
0x5B Zustand lesen (Bit n-1 = Relais n angezogen).
"""
import argparse
import sys
import time

import serial


def state(s):
    s.reset_input_buffer()
    s.write(bytes([0x5B]))
    r = s.read(2)
    return r[0] if r else None


def set_relay(s, relay, on):
    s.write(bytes([(101 if on else 111) + relay - 1]))
    time.sleep(0.2)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("cmd", choices=["status", "aus", "ein", "zyklus"])
    p.add_argument("sekunden", nargs="?", type=float, default=15.0)
    p.add_argument("--port", default="COM7")
    p.add_argument("--relay", type=int, default=2)
    a = p.parse_args()

    with serial.Serial(a.port, 19200, timeout=1) as s:
        if a.cmd == "status":
            st = state(s)
            on = bool(st is not None and st & (1 << (a.relay - 1)))
            print(f"Zustand={st} -> Geraet {'STROMLOS' if on else 'unter Strom'}")
        elif a.cmd == "aus":
            set_relay(s, a.relay, True)
            print("Geraet stromlos")
        elif a.cmd == "ein":
            set_relay(s, a.relay, False)
            print("Geraet unter Strom")
        else:
            set_relay(s, a.relay, True)
            print(f"stromlos fuer {a.sekunden:g} s ...")
            try:
                time.sleep(a.sekunden)
            finally:
                set_relay(s, a.relay, False)
            print("Geraet wieder unter Strom")


if __name__ == "__main__":
    sys.exit(main())
