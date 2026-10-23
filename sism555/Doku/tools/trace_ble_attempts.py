"""Build one record per BLE connection attempt from events.ndjson and print statistics."""
import json, re, sys, collections
from datetime import datetime

EV = sys.argv[1]
OUT = sys.argv[2]

def t(s):
    return datetime.fromisoformat(s)

runs = collections.OrderedDict()
for l in open(EV, encoding="utf-8"):
    e = json.loads(l)
    runs.setdefault(e["run"], []).append(e)

recs = []
addr_name = {}
for run, evs in runs.items():
    fw = None
    mode = None
    att = None
    last_disc = None
    for e in evs:
        ev, d = e["ev"], e["d"]
        if ev == "run_started":
            a = d.get("arguments", {}) or {}
            mode = a.get("mode")
        if ev == "rs232_line":
            m = re.search(r"Version (5\.\d+)", d["text"])
            if m:
                fw = m.group(1)
            if att and "[BT-Event]" in d["text"]:
                m = re.search(r",(C|CU|DIS),", d["text"])
                if m:
                    att["events"].append((round((t(e["ts"]) - att["t0"]).total_seconds(), 2), m.group(1), d["text"][16:90]))
        if ev == "device_info" and d.get("firmware"):
            fw = d["firmware"]
        if ev == "ble_device_selected":
            addr_name[d.get("address")] = d.get("name")
        if e["tr"] != "ble":
            continue
        start = ev == "ble_device_selected" or ev == "connect_started"
        if start:
            if ev == "connect_started" and att and att.get("sel_only") and not att["outcome"] \
                    and (t(e["ts"]) - att["t0"]).total_seconds() < 60:
                att["sel_only"] = False
                att["addr"] = d.get("address") or att["addr"]
                continue
            if att and not att["outcome"]:
                att["outcome"] = "none"
            att = {"run": run, "mode": mode, "phase": e["phase"], "t0": t(e["ts"]), "fw": fw,
                   "addr": d.get("address"), "sel_only": ev == "ble_device_selected",
                   "connect_ms": None, "subscribed": False, "outcome": None, "events": [],
                   "since_disc_s": round((t(e["ts"]) - last_disc).total_seconds(), 1) if last_disc else None,
                   "scan": ev == "ble_device_selected", "t_out": None}
            recs.append(att)
            continue
        if not att:
            continue
        if ev == "connect_succeeded":
            att["connect_ms"] = d.get("duration_ms")
        elif ev == "notify_subscribed":
            att["subscribed"] = True
        elif ev in ("pin_prompt_received",) or (ev == "marker_found" and d.get("marker") == "PIN = "):
            if not att["outcome"]:
                att["outcome"] = "pin"
                att["t_out"] = round((t(e["ts"]) - att["t0"]).total_seconds(), 2)
        elif ev == "marker_timeout" and d.get("marker") == "PIN = ":
            if not att["outcome"]:
                att["outcome"] = "nopin"
                att["t_out"] = round((t(e["ts"]) - att["t0"]).total_seconds(), 2)
        elif ev == "disconnect_succeeded":
            last_disc = t(e["ts"])
    # keep collecting BT events after outcome is set: handled above while att is current

for r in recs:
    r["t0"] = r["t0"].isoformat(timespec="seconds")
    r["board"] = {"CD:49:1B:93:00:5F": "A_99SC9923"}.get(r["addr"], addr_name.get(r["addr"], r["addr"]))
with open(OUT, "w", encoding="utf-8") as f:
    for r in recs:
        f.write(json.dumps(r, ensure_ascii=False) + "\n")
print(len(recs), "attempts")
print(addr_name)
