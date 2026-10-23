"""Filter all trace.ndjson files down to the events relevant for BLE/Classic connection analysis."""
import json, os, re, sys

ROOT = sys.argv[1]
OUT = sys.argv[2]
KEEP_RS = re.compile(r"\[BT-|Version 5\.|F1\d{4}|W1\d{4}|Sitzung|BT_LINK|CYSPP|getrennt|verbunden", re.I)
SKIP = {"rx_data", "tx_data", "soak_sample"}

with open(OUT, "w", encoding="utf-8") as out:
    for d in sorted(os.listdir(ROOT)):
        p = os.path.join(ROOT, d, "trace.ndjson")
        if not os.path.isfile(p):
            continue
        with open(p, "rb") as f:
            for raw in f:
                if b'"rs232_line"' in raw:
                    if not KEEP_RS.search(raw.decode("utf-8", "replace")):
                        continue
                elif any(('"%s"' % s).encode() in raw[:400] for s in SKIP):
                    continue
                try:
                    e = json.loads(raw)
                except Exception:
                    continue
                det = e.get("details", {})
                if e["event"] == "rs232_line":
                    det = {"text": det.get("text", "")[:200]}
                rec = {"run": d, "ts": e.get("timestamp"), "phase": e.get("phase"),
                       "tr": e.get("transport"), "cyc": e.get("cycle"), "ev": e["event"],
                       "lvl": e.get("level"), "d": det}
                s = json.dumps(rec, ensure_ascii=False)
                out.write(s[:3000] + "\n")
        print(d, flush=True)
