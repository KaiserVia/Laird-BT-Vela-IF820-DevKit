# LTE_Fix — Änderungsprotokoll LTE/GSM-Firmware

> Fortlaufendes Protokoll konkreter Code-Änderungen am LTE/GSM-Teil.
> Format je Eintrag: Symptom / Ursache / Fix / geänderte Dateien / Test.

---

## Fix 1 — SIM-Karte nicht erkannt → Fehlercode F12009 (BT 5.50)

**Datum:** 2026-07-27

### Symptom
Steckt keine SIM-Karte (oder ist sie defekt/schlecht kontaktiert), gab es **keine
eindeutige Rückmeldung**. Das Modem wurde still abgeschaltet; gesetzt wurde nur die
interne Marke `mqtt_con_err=41` (bzw. Durchfall in `test_registration()`), aber
**kein Diagnose-Code** und keine gezielte Meldung in der Kommunikation.

### Ursache
`atcpin()` unterschied bisher nur:
- `+CPIN: READY` → PIN ok (`simpinset=1`)
- `incorrect` (`+CME ERROR: incorrect password`) → falsche PIN (`SIM_ERROR`)

Der Fall **„keine SIM erkannt"** fiel durch beide Raster und blieb ohne eigene
Kennung.

### Fix
Neuer Diagnose-Code **`DTC_GSM_SIM_NF = 12009` „SIM-Karte nicht erkannt"** (Typ F,
Subsystem 12 GSM/LTE). Erkennung über das dedizierte Quectel-Kommando
**`AT+QSIMSTAT?`**, das den physischen Steckstatus unabhängig vom PIN-Zustand
liefert (`+QSIMSTAT: <enable>,<insert>`, `insert=0` → keine SIM).

Neue statische Hilfsfunktion `sim_inserted()` in `gsmio.c`:
- sendet `AT+QSIMSTAT?`, sucht `+QSIMSTAT:`, liest die Ziffer nach dem Komma;
- Rückgabe `1` = SIM erkannt, `0` = keine SIM, `-1` = unbekannt.
- **`-1` (z. B. altes Modem ohne QSIMSTAT) löst bewusst KEINEN Fehler aus** →
  kein Fehlalarm. Nur ein explizites `insert=0` gilt als „keine SIM"; `insert=2`
  („wird erkannt") wird als vorhanden gewertet.

Aufruf in `atcpin()` direkt nach `modem_start()` (Modem ist da noch an, deckt sowohl
den PIN- als auch den Kein-PIN-Pfad ab). Bei „keine SIM":

```c
result=modem_start ();
if (result>0 && sim_inserted()==0)     // Modem antwortet, aber QSIMSTAT meldet keine SIM
{
 connect=concpy;
 dtcerr(DTC_GSM_SIM_NF); newline();    // "F12009 SIM-Karte nicht erkannt" -> Kommunikation + Protokoll
 mqtt_con_err=45; simpinset=0; fp.pinerr=0;
 gsm_power(0);                         // Modem abschalten
 return;
}
```

`dtcerr()` → `dctext()` gibt „F12009 SIM-Karte nicht erkannt" an die aktive
Schnittstelle (RS232/USB/BT) aus **und** protokolliert den Code (Fehlerspeicher).
Damit ist die Anforderung „Fehlercode + Anzeige in der Kommunikation" erfüllt.

### Geänderte Dateien
| Datei | Änderung |
|-------|----------|
| `dtc_codes.h` | `#define DTC_GSM_SIM_NF 12009` ergänzt |
| `dtc_text.h` | Tabelleneintrag `{12009,'F',"SIM-Karte nicht erkannt"}` ergänzt |
| `Doku/Diagnose-Codes.csv` | Katalogzeile `12009;F;GSM/LTE;SIM-Karte nicht erkannt;…;5.50;aktiv` |
| `gsmio.h` | Tokens `T_qsimstat "AT+QSIMSTAT?"`, `T_qsimresp "+QSIMSTAT:"` |
| `gsmio.c` | Hilfsfunktion `sim_inserted()` + Aufruf/Erkennung in `atcpin()` |
| `sprachen/sictxt.c` | `T_version` 5.49 → **5.50** |

> **Hinweis zu `dtc_text.h`:** Die Datei ist eigentlich generiert
> (`tools/gen_dtc.py` aus `Diagnose-Codes.csv`, Kopf „NICHT von Hand ändern").
> Der Generator liegt aktuell nicht im Repo, daher wurden CSV **und** Header
> **parallel von Hand** gepflegt. Beim nächsten Generatorlauf bleibt der Stand
> konsistent (gleicher Code/Text).

### Noch am Gerät zu verifizieren (kein Compiler/HW in dieser Session)
1. **QSIMSTAT-Antwortformat** des konkret verbauten EG91 prüfen
   (`+QSIMSTAT: <enable>,<insert>`), insbesondere ob `insert` direkt nach dem
   ersten Komma steht — die Parsing-Logik (`compare(",",…)`) hängt daran.
2. Verhalten ohne SIM: erscheint „F12009 SIM-Karte nicht erkannt" am Terminal/BT
   und steht der Code im Fehlerspeicher?
3. Verhalten mit SIM (mit/ohne PIN): **kein** F12009, normaler Ablauf.
4. Timing frischer Power-on: QSIMSTAT-Abfrage inmitten der Boot-URCs
   (`RDY`/`+CFUN`/`+CPIN`/`PB DONE`) — Token wird gesucht, sollte robust sein;
   bei Bedarf Timeout (`XMODEM_BLOCK_TIMEOUT`) anpassen.
5. Optional: F12009 zusätzlich im Funkstatus (`radiostatus()`, STRF) anzeigen.

### Konvention
`T_version`-Erhöhung auf **5.50** → gemäß `CLAUDE.md` als eigener Commit sichern
(`BT 5.50: SIM nicht erkannt -> F12009`), nur geänderte Quell-/Doku-Dateien.
