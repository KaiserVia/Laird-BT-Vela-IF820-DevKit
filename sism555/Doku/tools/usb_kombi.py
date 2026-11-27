"""Schnelltest USB je Kombination (PC-Port / Kabel / Hub), ca. 5 min.

Ablauf:
  1. Geraet am USB suchen (Seriennummer), Anschlussweg anzeigen (Hub-Tiefe)
  2. Stromzyklen ueber Relais (usb_test.py): kommt der Port nach dem Einschalten von selbst?
  3. 20x COM-Port oeffnen mit Menue (usb_open_stress.py)
  4. 20x Schliessen waehrend der Ausgabe (usb_close_provoke.py)
  5. 2x Ymodem-Download 20000 Messwerte (usb_download.py)
Ergebnis je Kombination als Zeile in Testreports/usb/kombi/ergebnis.md.

Aufruf:  python usb_kombi.py --name "PC hinten, Kabel 3 m, passiver Hub"
"""
import argparse, os, re, subprocess, sys, time
import serial.tools.list_ports as lp

HERE = os.path.dirname(os.path.abspath(__file__))
SERNO = "26SC9966"


def find_dev():
    for p in lp.comports():
        if p.vid == 0x1FC9 and p.pid == 0x80F7:
            return p
    return None


def hub_chain():
    """Anschlussweg ueber Windows-PnP (pyserial liefert bei tiefen Hub-Ketten nichts)."""
    ps = (r"$id='USB\VID_1FC9&PID_80F7\%s'; $n=0; $l=(Get-PnpDeviceProperty -InstanceId $id "
          r"-KeyName DEVPKEY_Device_LocationInfo).Data; while($true){ $id=(Get-PnpDeviceProperty -InstanceId $id "
          r"-KeyName DEVPKEY_Device_Parent).Data; if($id -like 'USB\ROOT_HUB*' -or $id -notlike 'USB\*'){break}; $n++ }; "
          '"$l|$n"') % SERNO
    try:
        out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True,
                             text=True, timeout=30).stdout.strip().splitlines()[-1]
        l, n = out.split("|")
        return l, int(n)
    except Exception:
        return "?", -1


def run(cmd, logf, timeout):
    t0 = time.time()
    with open(logf, "w", encoding="utf-8") as f:
        try:
            r = subprocess.run([sys.executable] + cmd, cwd=HERE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, timeout=timeout, text=True,
                               encoding="utf-8", errors="replace")
            out = r.stdout
        except subprocess.TimeoutExpired as e:
            out = (e.stdout or "") if isinstance(e.stdout, str) else ""
            out += "\nTIMEOUT\n"
        f.write(out)
    return out, time.time() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True, help="Kombination, z. B. 'Kabel 5 m, passiver Hub'")
    ap.add_argument("--zyklen", type=int, default=2)
    ap.add_argument("--n", type=int, default=20)
    ap.add_argument("--downloads", type=int, default=2)
    ap.add_argument("--relay-port", default="COM7")
    a = ap.parse_args()

    base = os.path.join(HERE, "Testreports", "usb", "kombi")
    tag = time.strftime("%Y-%m-%d_%H%M%S")
    od = os.path.join(base, tag)
    os.makedirs(od, exist_ok=True)

    p = find_dev()
    if not p:
        print("Geraet nicht am USB gefunden - Kabel/Hub pruefen.")
        sys.exit(2)
    loc, hubs = hub_chain()
    print(f"Geraet: {p.device}, Seriennummer {p.serial_number}, Anschluss {loc} ({hubs} Hub(s) dazwischen)")

    res = {}
    print("Stromzyklen ...", flush=True)
    out, _ = run(["usb_test.py", "--zyklen", str(a.zyklen), "--relay-port", a.relay_port,
                  "--aus", "3", "--leerlauf", "20", "--out", os.path.join(od, "zyklen")], os.path.join(od, "zyklen.log"), 600)
    m = re.search(r"ENDE: (\d+)/(\d+) Zyklen ok", out)
    res["Stromzyklen"] = (int(m.group(1)), int(m.group(2))) if m else (0, a.zyklen)
    print("  ", res["Stromzyklen"])

    print("Oeffnen ...", flush=True)
    out, _ = run(["usb_open_stress.py", "--n", str(a.n), "--diag", "10", "--out",
                  os.path.join(od, "open.log")], os.path.join(od, "open.txt"), 600)
    m = re.search(r"ENDE: (\d+) Versuche, (\d+) Fehler, Port-Wechsel (\d+)", out)
    res["Oeffnen"] = (int(m.group(1)) - int(m.group(2)), int(m.group(1))) if m else (0, a.n)
    print("  ", res["Oeffnen"])

    print("Schliessen waehrend Ausgabe ...", flush=True)
    out, _ = run(["usb_close_provoke.py", "--n", str(a.n), "--out",
                  os.path.join(od, "close.log")], os.path.join(od, "close.txt"), 600)
    m = re.search(r"ENDE: (\d+) Versuche, (\d+) Oeffnungsfehler, Port-Wechsel (\d+)", out)
    res["Schliessen"] = (int(m.group(1)) - int(m.group(2)) - int(m.group(3)), int(m.group(1))) if m else (0, a.n)
    print("  ", res["Schliessen"])

    ok = 0; dauer = []
    for i in range(a.downloads):
        print(f"Download {i+1} ...", flush=True)
        out, _ = run(["usb_download.py", "--auto", "--records", "20000"],
                     os.path.join(od, f"download{i+1}.txt"), 300)
        m = re.search(r"Markdown-Report: (.+\.md)", out)
        if m and os.path.exists(m.group(1).strip()):
            md = open(m.group(1).strip(), encoding="utf-8").read()
            if "**passed**" in md:
                ok += 1
                d = re.search(r"Dauer: `([\d.]+)`", md)
                if d: dauer.append(float(d.group(1)))
    res["Download"] = (ok, a.downloads)
    print("  ", res["Download"], dauer)

    alles = all(v[0] == v[1] for v in res.values())
    zeile = (f"| {time.strftime('%Y-%m-%d %H:%M')} | {a.name} | {loc} ({hubs} Hub) | "
             + " | ".join(f"{v[0]}/{v[1]}" for v in res.values())
             + f" | {('%.1f s' % (sum(dauer)/len(dauer))) if dauer else '-'} | "
             + ("**ok**" if alles else "**FEHLER**") + f" | {tag} |\n")
    ef = os.path.join(base, "ergebnis.md")
    if not os.path.exists(ef):
        with open(ef, "w", encoding="utf-8") as f:
            f.write("# USB-Schnelltest je Kombination (usb_kombi.py)\n\n"
                    "| Zeit | Kombination | Anschluss | Stromzyklen | Oeffnen | Schliessen | Download | "
                    "Dauer Download | Ergebnis | Ordner |\n|---|---|---|---|---|---|---|---|---|---|\n")
    with open(ef, "a", encoding="utf-8") as f:
        f.write(zeile)
    print("\n" + ("ALLES OK" if alles else "FEHLER - Protokolle in " + od))
    print(zeile)


if __name__ == "__main__":
    main()
