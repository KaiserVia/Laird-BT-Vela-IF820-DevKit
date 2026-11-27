#!/usr/bin/env python3
"""usb_nacht.py - USB-Dauerlauf ueber Nacht (Branch usb-verbindung).

Phase A (bis --a-bis, Uhrzeit): Runden aus
  - Stromzyklus (Relais, stromlos 0,5/1/2/5/10/30 s reihum), Port muss stabil kommen, Menue/Info ueber USB
  - jede 3. Runde: Ymodem-Download ueber USB (usb_download.py, --records)
  - jede 5. Runde: Firmware-Update ueber USB (--bin), danach Port und Version pruefen
Phase B (bis --b-bis): Geraet laeuft ohne Stromzyklus, alle 2 min Menue/Info ueber USB (Port oeffnen/schliessen),
  jedes Verschwinden des Ports wird protokolliert, Tiefschlaf-Zaehler muss 0 bleiben.

  python usb_nacht.py --bin Testreports/usb/fw/sism_5u3.bin --expect 5.u3 --a-bis 04:30 --b-bis 06:40 --out Testreports/usb/nacht
"""
import argparse, datetime, json, os, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb_test as u  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def until(hhmm):
    now = datetime.datetime.now()
    h, m = map(int, hhmm.split(":"))
    t = now.replace(hour=h, minute=m, second=0, microsecond=0)
    if t <= now:
        t += datetime.timedelta(days=1)
    return t.timestamp()


def run(cmd, logfile, timeout):
    try:
        p = subprocess.run([sys.executable, "-I"] + cmd, capture_output=True, text=True, encoding="latin-1",
                           timeout=timeout, cwd=HERE)
        out = p.stdout + p.stderr; rc = p.returncode
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or "") if isinstance(e.stdout, str) else ""; rc = -999
    open(logfile, "w", encoding="utf-8").write(out)
    return rc, out


def wait_port(secs):
    t = time.time()
    while time.time() - t < secs:
        if u.usb_ports():
            return round(time.time() - t, 1)
        time.sleep(0.3)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--expect", required=True)
    ap.add_argument("--a-bis", required=True)
    ap.add_argument("--b-bis", required=True)
    ap.add_argument("--records", type=int, default=5000)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    u.LOG = open(os.path.join(a.out, "log.txt"), "a", encoding="utf-8")
    w = u.Watcher(); w.start()
    res = {"A": [], "B": []}
    save = lambda: json.dump(res, open(os.path.join(a.out, "ergebnis.json"), "w"), indent=1, ensure_ascii=False)

    class Args: pass
    za = Args(); za.relay_port = "COM7"; za.hochlauf = 120.0; za.leerlauf = 10.0
    aus_liste = [0.5, 1, 2, 5, 10, 30]
    t_a = until(a.a_bis); n = 0
    u.log(f"Phase A bis {a.a_bis}")
    while time.time() < t_a:
        n += 1
        r = u.zyklus(za, w, n, aus_liste[(n - 1) % len(aus_liste)])
        if n % 3 == 0 and r.get("ok"):
            rc, out = run(["usb_download.py", "--auto", "--records", str(a.records)],
                          os.path.join(a.out, f"download_{n}.txt"), 1500)
            r["download"] = "passed" if "Ergebnis: passed" in out else f"failed rc={rc}"
            r["download_retries"] = next((l.split()[-1] for l in out.splitlines() if l.strip().startswith("Retries:")), None)
            u.log(f"Runde {n} Download: {r['download']} Retries={r['download_retries']}")
            r["ok"] = r["ok"] and r["download"] == "passed"
        if n % 5 == 0 and u.usb_ports():
            port = next(iter(u.usb_ports()))
            rc, out = run(["flash_xmodem.py", "--port", port, "--bin", a.bin], os.path.join(a.out, f"flash_{n}.txt"), 300)
            back = wait_port(120)
            time.sleep(5)
            m = u.menu_check(next(iter(u.usb_ports()))) if u.usb_ports() else {}
            r["update"] = {"reprogram": "y gesendet" in out, "nak": out.count("Wiederholung"), "port_zurueck_s": back,
                           "version": m.get("version")}
            ok_up = r["update"]["reprogram"] and m.get("version") == a.expect
            u.log(f"Runde {n} Update: {r['update']}")
            r["ok"] = r["ok"] and ok_up
        res["A"].append(r); save()
    u.relay(za.relay_port, False)
    u.log(f"Phase A Ende: {sum(1 for r in res['A'] if r.get('ok'))}/{len(res['A'])} Runden ok")

    t_b = until(a.b_bis); k = 0; t_start = time.time()
    u.log(f"Phase B bis {a.b_bis}")
    while time.time() < t_b:
        k += 1
        t0 = time.time()
        ports = u.usb_ports()
        m = u.menu_check(next(iter(ports))) if ports else {"fehler": "kein Port"}
        e = {"pruefung": k, "laufzeit_min": round((time.time() - t_start) / 60, 1), "menu": m.get("menu"),
             "schlaf": m.get("schlaf"), "fehler": m.get("fehler")}
        res["B"].append(e); save()
        if not m.get("menu"):
            u.log(f"Phase B Pruefung {k}: FEHLER {e}")
        time.sleep(max(0, 120 - (time.time() - t0)))
    ev = [x for x in w.events if x[0] >= t_start]
    u.log(f"Phase B Ende: {sum(1 for e in res['B'] if e['menu'])}/{len(res['B'])} Pruefungen ok, "
          f"Port-Wechsel {len(ev)}, max Schlafzaehler {max([e['schlaf'] or 0 for e in res['B']] or [0])}")
    res["B_port_wechsel"] = len(ev); save()
    w.stop = True


if __name__ == "__main__":
    main()
