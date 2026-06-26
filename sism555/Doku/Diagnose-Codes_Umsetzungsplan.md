# Diagnose-Codes – Umsetzungsplan (Migration vom zeilenbasierten DTC)

Ziel: das zeilenbasierte DTC-Schema (`dtc.h`, `DTCBASE + __LINE__`) durch **feste,
lesbare, stabile** Codes ersetzen. Grundlage: `Diagnose-Codes_Konzept.md` und der
Katalog `Diagnose-Codes.csv` (5-stellig, 11xxx–99xxx, Subsystem + laufende Nummer).

## Konventionen (für ALLE Schritte beachten)

- Quelldateien sind **ISO-8859-1 / CRLF** → nur **byte-sicher** editieren (kein UTF-8,
  keine Umlaut-Korruption). Ausnahme: `sprachen/sictxt.c` (UTF-8), Doku/CSV (UTF-8).
- Nach jeder Code-Änderung **SW-Version +0.01** (`sprachen/sictxt.c`, `T_version`).
- Pro Schritt einzeln bauen/testen, nicht alles auf einmal.

## Zu treffende Entscheidungen (vorab bestätigen)

- **D1 – Geräteausgabe:** `Fehler <Code> <Subsystem-Text>` (z. B. `Fehler 11001 Bluetooth`).
  Der Subsystem-Text wird aus dem Code abgeleitet (Code/1000 → 11 = Bluetooth) über eine
  kleine Tabelle. *Empfehlung: so.* (Voll-Beschreibung steht im CSV, nicht im Flash.)
- **D2 – Single Source:** CSV ist Quelle; ein **Generator-Skript** erzeugt `dtc_codes.h`.
  *Empfehlung: ja* (kein Drift). Alternativ Header von Hand pflegen.
- **D3 – Aufrufschnittstelle:** Aufrufstelle übergibt den **Code** statt der bisherigen
  Kategorie, z. B. `puterror(DTC_BT_PING, -1)` bzw. `dtcerr(DTC_BT_NAME_SET)`.

---

## Phase 1 – Katalog vervollständigen

1. **Alle Fehlerstellen inventarisieren** (aktueller Stand), je Datei:
   ```
   grep -rnE "puterror|puterrstr|dtcerr|put2str\(T_err" *.c
   ```
   Liste: Datei, Funktion, bisherige Meldung/Kategorie.
2. Jeder Stelle einen **festen Code** zuweisen: Subsystem-Bereich wählen (11=btio, 12=gsmio,
   13=sicom, 14=mqtt, 15=gpsio, 16=flash, 17=main, 18=libtool, 19=USB_tools, 20=sictst),
   nächste freie laufende Nummer.
3. Jede Stelle als Zeile in `Diagnose-Codes.csv` eintragen:
   `Code;Subsystem;Kurztext;Ursache;Abhilfe;SeitVersion;Status` (SeitVersion = aktuelle Version).
4. **Review:** jeder Code genau einmal, lückenlose Zuordnung Code ↔ Quellstelle.

## Phase 2 – Firmware-Header `dtc_codes.h`

5. Datei `dtc_codes.h` anlegen mit benannten Konstanten je Katalog-Eintrag:
   ```c
   #define DTC_BT_PING        11001   // PING keine Antwort
   #define DTC_BT_FLOWCTL     11002   // Flow Control setzen fehlgeschlagen
   ...
   ```
   (Reihenfolge wie im CSV; Kommentar = Kurztext.)
6. Kleine **Subsystem→Text-Tabelle** definieren (11→"Bluetooth", 12→"GSM/GPRS", …) für die
   Geräteausgabe (D1).

## Phase 3 – Ausgabe-Funktionen anpassen (`sio.c` / `sio.h`)

7. `puterror_dtc()` / `puterrstr_dtc()` / `dctext()` so umstellen, dass sie den **Code direkt**
   bekommen (nicht mehr `DTCBASE+__LINE__`) und ausgeben:
   `Fehler <Code> <Subsystem-Text>` + Protokoll-Logeintrag (`protocol(code)`).
   Subsystem-Text aus Code ableiten (Code/1000 → Tabelle aus Schritt 6).
8. Signaturen/Namen festlegen (z. B. `puterror(code,val)`, `dtcerr(code)`); Prototypen in
   `sio.h` aktualisieren.

## Phase 4 – Aufrufstellen migrieren (datei­weise)

Für **jede** .c-Datei (btio, gsmio, sicom, mqtt, gpsio, flash, main, libtool, USB_tools, sictst):

9. `#define DTCBASE n` + `#include "dtc.h"` entfernen, `#include "dtc_codes.h"` einsetzen.
10. Jede Fehlerstelle auf die **feste Konstante** umstellen, z. B.
    `puterror(BLUETOOTH_ERROR,-1)` → `puterror(DTC_BT_PING,-1)`,
    `dtcerr("BT-Name: setzen fehlgeschlagen")` → `dtcerr(DTC_BT_NAME_SET)`.
11. Datei einzeln bauen (soweit möglich) / Review, dann nächste Datei.

## Phase 5 – Altes System entfernen

12. `dtc.h` (zeilenbasiert) löschen. Alle `#define DTCBASE` entfernt? (grep prüfen.)
13. Tote Reste in `sio.c/.h` aufräumen.

## Phase 6 – Verifikation

14. **Eindeutigkeit:** Skript prüft, dass jeder Code in `dtc_codes.h` genau einmal vorkommt
    und mit dem CSV übereinstimmt (Header ↔ CSV, keine Lücken/Dubletten).
    ```
    grep -oE "1[1-9][0-9]{3}|[2-9][0-9]{4}" dtc_codes.h | sort | uniq -d   # darf nichts zeigen
    ```
15. **Build** in Keil µVision (richtige Schriftart/Encoding!), Warnungen prüfen.
16. **Funktionstest:** 2–3 Fehler gezielt auslösen (z. B. BT ohne Modul), Ausgabe
    `Fehler <Code> <Text>` mit CSV abgleichen; Flash-Log prüfen.
17. **Encoding-Check:** geänderte .c/.h weiterhin `ISO-8859 text, with CRLF`, kein `\r\r\n`,
    Anzahl Nicht-ASCII-Bytes unverändert.

## Phase 7 – Werkzeuge (optional, empfohlen)

18. **Generator** `tools/gen_dtc.py`: liest `Diagnose-Codes.csv` → schreibt `dtc_codes.h`
    (und prüft Eindeutigkeit). Damit ist die CSV alleinige Quelle.
19. **Service-PDF**: `Diagnose-Codes.csv` → druckbares PDF (Code, Kurztext, Ursache, Abhilfe)
    für Reparaturteam/Werkstatt.

## Phase 8 – Dokumentation & Prozess

20. `project-status.md`: alten DTC-Abschnitt als „abgelöst ab Version x.xx" markieren,
    auf `Diagnose-Codes_Konzept.md` + `.csv` verweisen.
21. Prozess „neuen Code anlegen" (steht im Konzept): Nummer hochzählen → CSV-Zeile →
    Header (bzw. Generator) → Konstante an der Stelle verwenden.

---

## Risiken & Rollback

- **Risiko:** viele Aufrufstellen → Tippfehler/Encoding. Gegenmaßnahme: datei­weise,
  byte-sicher, nach jeder Datei Encoding-Check + Build.
- **Rollback:** Migration in einem eigenen Git-Branch/Commit-Set; bei Problemen
  zurückrollen. Altes `dtc.h` erst in Phase 5 löschen (vorher nur ungenutzt).

## Aufwand (grobe Schätzung)

- Phase 1 (Katalog): mittel – einmaliges Sichten aller Stellen.
- Phase 2–3 (Header + Ausgabe): klein.
- Phase 4 (Migration): Hauptaufwand, proportional zur Zahl der Fehlerstellen.
- Phase 6 (Verifikation): klein. Phase 7 (Tooling): optional, klein.

## Reihenfolge-Empfehlung

Phase 1 → 2 → 3 → 4 (eine Datei als Pilot, z. B. btio.c, dann Rest) → 6 → 5 → 7 → 8.
(Erst Pilotdatei migrieren und testen, dann ausrollen; altes `dtc.h` zuletzt entfernen.)
