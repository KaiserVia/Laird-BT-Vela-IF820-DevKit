#!/usr/bin/env python3
"""Sucht im Trace eines Testlaufs nach "Stray-Zeichen": Menue-Navigation oder Steuerzeichen auf RS232/USB, die der Test
nicht ausgeloest hat (Hintergrund: 5.110 - Event-Geschwaetz des Moduls beim Trennen wurde als Tastendruck gelesen und
fuehrte das Menue spontan weiter; die Weiche BT_CONN_GPIO sollte das verhindern, 5.120 schaltet sie ab).

Aufruf:  python trace_stray_check.py <Report-Ordner>/trace.ndjson [...]

Sinnvoll fuer Laeufe, in denen der Test selbst NICHT navigiert (cycle, interleave, smoke): dort darf auf dem Terminal nur
Banner, PIN-Abfrage und das Hauptmenue erscheinen. In Phasen mit Navigation oder Ymodem-Export (download, receive, soak)
wird nichts gemeldet (dort ist Menue-Navigation und der gespiegelte Binaerstrom normal). Das Skript liest nur.
"""
import json
import re
import sys

# Menuetitel, die ohne Navigation nie erscheinen duerfen (Hauptmenue-Eintraege wie "5. Funkmodems" enthalten keinen Doppelpunkt)
NAV_TITLES = re.compile(r"^(Testmen|Einstellungen:|Funkmodems:|Bluetooth Men|Werksmen|Bauteiltests|Anzahl Messwerte|"
                        r"Daten ausgeben|Daten l|Online messen|Zeitplanung)")
CONTROL = re.compile(r"[\x00-\x08\x0b-\x1f\x7f]")
NO_NAVIGATION_TESTS = {"run_cycle_test", "run_smoke_test", "run_classic_smoke_test"}


def check(path: str) -> int:
    phase_test = {}
    findings = []
    lines_seen = 0
    with open(path, encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            if '"rs232_line"' not in raw and '"phase_started"' not in raw:
                continue
            row = json.loads(raw)
            if row["event"] == "phase_started":
                phase_test[row.get("phase")] = (row.get("details") or {}).get("test")
                continue
            details = row.get("details") or {}
            text = details.get("text", "")
            lines_seen += 1
            phase = row.get("phase")
            navigates = phase_test.get(phase) not in NO_NAVIGATION_TESTS
            # Bei Downloads (Ymodem) wird der Binaerstrom auf RS232 gespiegelt: dort sind Steuerzeichen normal und
            # werden nicht geprueft. Nur in Phasen ohne Navigation/Binaerdaten zaehlen sie als Auffaelligkeit.
            if not navigates and CONTROL.search(text):
                findings.append((row["timestamp"][11:23], phase, "Steuerzeichen", text[:70]))
            elif not navigates and NAV_TITLES.match(text):
                findings.append((row["timestamp"][11:23], phase, "Menue ohne Navigation", text[:70]))
    print(f"{path}: {lines_seen} RS232-Zeilen geprueft, {len(findings)} Auffaelligkeiten")
    for finding in findings[:30]:
        print("  ", *finding)
    return len(findings)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    total = sum(check(arg) for arg in sys.argv[1:])
    sys.exit(1 if total else 0)
