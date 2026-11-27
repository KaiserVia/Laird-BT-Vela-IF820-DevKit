#!/usr/bin/env python3
"""usb_update_loop.py - Firmware-Update ueber den USB-COM-Port mehrfach hintereinander pruefen.

Je Durchlauf: flash_xmodem.py ueber den USB-Port, NAK-Wiederholungen zaehlen, warten bis der USB-Port nach dem
Neustart wieder da ist, Version ueber das Menue lesen.

  python usb_update_loop.py --bin Testreports/usb/fw/sism_5u3.bin --n 3 --expect 5.u3 --out Testreports/usb/update_loop
"""
import argparse, json, os, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb_test as u  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--bin", required=True)
ap.add_argument("--n", type=int, default=3)
ap.add_argument("--expect", required=True)
ap.add_argument("--out", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
res = []
for i in range(1, a.n + 1):
    ports = u.usb_ports()
    if not ports:
        print("kein USB-Port"); break
    port = next(iter(ports))
    t0 = time.time()
    p = subprocess.run([sys.executable, "-I", os.path.join(os.path.dirname(__file__), "flash_xmodem.py"),
                        "--port", port, "--bin", a.bin], capture_output=True, text=True, encoding="latin-1")
    log = p.stdout + p.stderr
    open(os.path.join(a.out, f"flash_{i}.txt"), "w", encoding="utf-8").write(log)
    nak = log.count("Wiederholung")
    repro = "y gesendet" in log
    # Port weg und wieder da
    t_back = None; t1 = time.time()
    while time.time() - t1 < 120:
        if u.usb_ports() and time.time() - t1 > 3:
            t_back = round(time.time() - t1, 1); break
        time.sleep(0.3)
    time.sleep(2)
    r = u.menu_check(next(iter(u.usb_ports()))) if u.usb_ports() else {}
    e = {"lauf": i, "nak": nak, "reprogram": repro, "port_zurueck_s": t_back, "version": r.get("version"),
         "ok": repro and r.get("version") == a.expect, "dauer_s": round(time.time() - t0, 1)}
    print(json.dumps(e), flush=True)
    res.append(e)
    json.dump(res, open(os.path.join(a.out, "ergebnis.json"), "w"), indent=1)
print(f"ENDE {sum(1 for e in res if e['ok'])}/{len(res)} ok")
