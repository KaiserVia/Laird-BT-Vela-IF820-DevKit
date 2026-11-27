#!/usr/bin/env python3
"""usb_ab_provoke.py - Firmware-Staende abwechselnd mit dem Schliess-Ausloeser vergleichen (A/B, gleiche PC-Bedingungen).

  python usb_ab_provoke.py --bins Testreports/usb/fw/sism_5u21.bin:5.u21,Testreports/usb/fw/sism_5u18.bin:5.u18 --runden 2 --n 300 --out Testreports/usb/ab

Je Runde und Stand: per USB flashen, Version pruefen, usb_close_provoke.py mit --n Versuchen, RS232 mitschneiden
(Neustarts/HardFault zaehlen). Ergebnis in <out>/ergebnis.json.
"""
import argparse, json, os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb_test as u  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument("--bins", required=True)
ap.add_argument("--runden", type=int, default=2)
ap.add_argument("--n", type=int, default=300)
ap.add_argument("--out", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
staende = [b.split(":") for b in a.bins.split(",")]
res = []


def py(args, **kw):
    return subprocess.run([sys.executable, "-I"] + args, cwd=HERE, capture_output=True, text=True, encoding="latin-1", **kw)


for r in range(1, a.runden + 1):
    for binf, ver in staende:
        tag = f"r{r}_{ver}"
        ok = False
        for _ in range(4):
            port = next(iter(u.usb_ports()), None)
            if port:
                p = py(["flash_xmodem.py", "--port", port, "--bin", binf], timeout=300)
                if "y gesendet" in p.stdout + p.stderr:
                    ok = True; break
            time.sleep(8)
        t = time.time(); time.sleep(3)
        while time.time() - t < 120 and not u.usb_ports():
            time.sleep(0.5)
        time.sleep(10)
        m = {}
        for _ in range(4):
            if u.usb_ports():
                m = u.menu_check(next(iter(u.usb_ports())))
                if m.get("menu"):
                    break
            time.sleep(3)
        if m.get("version") != ver:
            res.append({"tag": tag, "fehler": f"Version {m.get('version')} statt {ver}"}); continue
        cap = os.path.join(a.out, f"capture_{tag}.txt")
        cp = subprocess.Popen([sys.executable, "-I", "com_capture.py", "--sek", str(a.n * 3 + 120), "--out", cap], cwd=HERE)
        time.sleep(2)
        logf = os.path.join(a.out, f"provoke_{tag}.txt")
        py(["usb_close_provoke.py", "--n", str(a.n), "--out", logf])
        cp.terminate(); time.sleep(1)
        txt = open(logf, encoding="utf-8").read()
        mm = re.search(r"ENDE: (\d+) Versuche, (\d+) Oeffnungsfehler", txt)
        c = open(cap, encoding="utf-8").read() if os.path.exists(cap) else ""
        e = {"tag": tag, "version": ver, "versuche": int(mm.group(1)) if mm else None,
             "oeffnungsfehler": int(mm.group(2)) if mm else None,
             "neustarts": c.count("unsichtbar bis Hochlauf"), "hardfault": c.count("HARDFAULT")}
        print(json.dumps(e), flush=True)
        res.append(e)
        json.dump(res, open(os.path.join(a.out, "ergebnis.json"), "w"), indent=1)
print("ENDE")
