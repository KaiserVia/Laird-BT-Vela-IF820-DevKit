# Automatische Zeitsynchronisation – Plan (mehrstufig)

Ziel: Die Uhr soll sich **selbst** stellen und gestellt halten, ohne dass ein normaler
Nutzer etwas tun muss. Mehrere Quellen, klare Rangfolge, ein gemeinsamer Eintrittspunkt.

Reihenfolge (Vertrauen, hoch → niedrig):
**Backend/MQTT (vorhanden) → GPS-RMC (ergänzen) → LTE-NITZ (ergänzen) → Build-Datum (Fallback, vorhanden).**

## Was „normale User zufrieden" konkret heißt

- **Nichts einstellen müssen:** kurz nach dem Einschalten steht eine plausible Zeit, sobald
  irgendeine Quelle verfügbar ist. Solange noch keine da ist, zeigt das Gerät das
  Build-Datum (nie mehr „2015").
- **Bleibt richtig:** die Uhr wird im Betrieb regelmäßig nachgezogen (Drift-Korrektur),
  ohne sichtbar herumzuspringen.
- **Nachvollziehbar:** im Menü *Information und Zeit* steht **woher** die Zeit stammt
  (Backend / GPS / Netz / manuell / ungestellt) und wie alt der letzte Abgleich ist.
- **Robust:** offensichtlich falsche Zeiten (Jahr < 2024, ungültige Werte) werden verworfen,
  damit eine schlechte Quelle die gute nicht kaputtmacht.
- **Manuell bleibt möglich:** wer von Hand stellt, dessen Wert hält – wird aber von einer
  genaueren Quelle (GPS/Backend) später still korrigiert.

## Architektur: EIN zentraler Eintrittspunkt

Heute schreibt die MQTT-Routine die RTC-Register direkt (in `mqtt.c`). Das wird **eine**
zentrale Funktion in `rtc.c`, durch die **alle** Quellen laufen – im Sinne des bisherigen
„nur an einer Stelle"-Prinzips:

```c
/* Rueckgabe: 1 = uebernommen, 0 = verworfen/uebersteuert.
   flags Bit0 = Eingabe ist UTC (per fp.utc + DST auf Lokalzeit umrechnen)
   flags Bit1 = Quelle liefert autoritativen UTC-Offset -> fp.utc aktualisieren
   flags Bit2 = manuell ausgeloest (Rang-/Drift-Pruefung ueberspringen, immer stellen) */
uchar clock_set (uchar dd, uchar mm, uint yyyy,
                 uchar hh, uchar mi, uchar ss,
                 schar utc_off, uchar src, uchar flags);
```

Diese Funktion macht **immer dasselbe**:
1. **Validieren** (`dayofweek()` prüft Datum; Jahr 2024–2099; hh<24, mi<60, ss<60). Ungültig → 0.
2. **Rang-/Vertrauensprüfung** (siehe unten): darf diese Quelle jetzt überhaupt stellen?
3. **UTC→Lokal** falls `flags Bit0`: `fp.utc` (+ DST über die vorhandene Sommer-/Winterlogik) addieren.
4. **Nur bei relevanter Abweichung schreiben** (Delta > ~3 s) → RTC-Register setzen,
   `DOW = dayofweek()-1`.
5. **Buchführung:** `fp.timesrc = src`, `fp.timesync_h = fp.hcount` (Zeitstempel des Abgleichs).
6. Optional Info-Ereignis ins Protokoll (z. B. „Zeit gestellt via GPS").

Alle Quellen (MQTT, GPS, NITZ, manuell, Build-Default) rufen nur noch `clock_set(...)`.

## Vertrauensmodell (Rang + Drift-Fenster)

Jede Quelle bekommt einen Rang:

| Rang | Quelle | `src` |
|------|--------|-------|
| 3 | Backend/MQTT | `SRC_MQTT` |
| 2 | GPS (RMC) | `SRC_GPS` |
| 2 | manuell gestellt | `SRC_MANUAL` |
| 1 | LTE-Netzzeit (NITZ) | `SRC_LTE` |
| 0 | Build-Datum (ungestellt) | `SRC_NONE` |

Regel in `clock_set` (Schritt 2):
- **Ungestellt** (`fp.timesrc==SRC_NONE`) → immer übernehmen.
- **Gleicher oder höherer Rang** → übernehmen (hält die Uhr aus der besten Quelle frisch).
- **Niedrigerer Rang** → nur übernehmen, wenn der letzte Abgleich **älter als
  `RESYNC_STUNDEN`** ist (z. B. 12 h). So zieht z. B. GPS nach, wenn das Backend länger
  weg ist; das Build-Datum wird von jeder echten Quelle sofort abgelöst.

Das verhindert Herumspringen (eine schwächere Quelle übersteuert eine frische bessere nicht)
und sichert trotzdem die Drift-Korrektur, wenn die beste Quelle fehlt.

Manuell = Rang 2 (entschieden): überlebt NITZ und Build, wird aber von GPS/Backend verfeinert.
`RESYNC_STUNDEN` = 12 h (entschieden).

## Die vier Stufen im Detail

### Stufe 1 – Backend/MQTT (vorhanden, nur umhängen)
- `mqtt.c` (~Z. 1045–1062) ersetzt den Inline-RTC-Block durch
  `clock_set(DD,MM,20YY,hh,mm,ss, offset, SRC_MQTT, FLAG_SET_UTC)`.
  Eingabe ist bereits Lokalzeit + Offset → kein UTC-Bit, aber `fp.utc` setzen.
- Verhalten unverändert, nur zentralisiert.

### Stufe 2 – GPS-RMC (ergänzen)
- Im RMC-Parser (`gpsio.c`) bei **gültigem Fix** (Statusfeld „A") Zeit + Datum auslesen:
  RMC-Feld 1 = `hhmmss`, Feld 9 = `ddmmyy` (Jahr 20yy).
- Aufruf `clock_set(dd,mm,2000+yy, hh,mm,ss, fp.utc, SRC_GPS, FLAG_FROM_UTC)`
  (GPS liefert UTC → über `fp.utc`/DST auf Lokal rechnen).
- Cadence: einmal pro gültigem Fix genügt (Rang-/Drift-Logik drosselt das Schreiben ohnehin).

### Stufe 3 – LTE-NITZ (ergänzen)
- Neue kleine Routine in `gsmio.c`, aufgerufen **nach erfolgreicher Netz-Registrierung**
  (im bestehenden LTE-Connect-Ablauf, einmal pro Verbindung).
- `AT+QLTS=2` (Quectel: letzte Netzzeit als Lokalzeit + Zeitzone in Viertelstunden) parsen;
  Fallback `AT+CCLK?` falls NITZ aktiv. Format `"yy/MM/dd,hh:mm:ss±zz"`.
- `clock_set(dd,mm,20yy, hh,mm,ss, zz/4, SRC_LTE, FLAG_SET_UTC)` (Netz liefert Offset → `fp.utc` setzen).
- Wenn das Netz keine Zeit liefert (Antwort leer/Fehler) → still überspringen.

### Stufe 4 – Build-Datum (vorhanden)
- `InitRTC()` setzt bei ungestellter Uhr das Build-Datum (`__DATE__/__TIME__`) und
  `fp.timesrc=SRC_NONE`. Greift nur, bis irgendeine echte Quelle kommt.

### Manuell (Menü)
- `set_time`/`set_date` (rtc.c) am Ende `clock_set(...,SRC_MANUAL,0)` aufrufen bzw.
  `fp.timesrc=SRC_MANUAL; fp.timesync_h=fp.hcount` setzen, damit die Buchführung stimmt.

## Manuelles Auslösen & Auto-Zeit ein/aus

Der Nutzer soll die Zeit-Findung **gezielt selbst anstoßen** können:

- **Menüpunkt „Zeit jetzt synchronisieren"** (in *Information und Zeit*): stößt einen
  **erzwungenen** Abgleich an – fragt der Reihe nach die beste verfügbare Quelle ab
  (aktueller GPS-Fix → `AT+QLTS` ans Modem → ggf. Backend-Anstoß) und ruft `clock_set(...)`
  mit `FLAG_FORCE` auf. „Force" überspringt das 12-h-Drift-Fenster und die Rangprüfung,
  d. h. der Abgleich greift sofort, egal wann zuletzt synchronisiert wurde.
  Rückmeldung am Gerät: „Zeit gestellt via <Quelle>" bzw. „Keine Zeitquelle erreichbar".

- **Einstellung „Automatische Zeit: ein/aus"** (neues `fp`-Flag, Default **ein**): erlaubt,
  die automatische Findung ganz abzuschalten – z. B. wenn jemand eine fest von Hand gestellte
  Zeit unangetastet lassen will. Bei „aus" laufen die Hintergrund-Quellen nicht; der
  Menüpunkt „Zeit jetzt synchronisieren" funktioniert trotzdem (bewusster Einzelabruf).

So hat der Nutzer beides: „läuft von allein" (Default) und „ich stoße es selbst an / ich
schalte es ab", ohne tief ins Menü zu müssen.

## UTC / Lokalzeit / Sommerzeit

- RTC läuft weiterhin in **Lokalzeit** (wie heute), plus `fp.utc` als Offset.
- UTC-Quellen (GPS, ggf. NITZ) rechnen **zentral in `clock_set`** um – nur dort, eine Stelle.
- Die vorhandene Sommer-/Winterzeit-Logik (`fp.sz`, `protocol(SOMMERZEIT/WINTERZEIT)`)
  bleibt zuständig; `clock_set` stellt die *Standardzeit*, die DST-Umstellung bleibt wie gehabt
  obendrauf. (Doppel-Umstellung vermeiden: bei aktiver Auto-DST den DST-Versatz in der
  Umrechnung berücksichtigen – Detail in der Umsetzung festzurren.)

## Benutzeransicht

- Menü *Information und Zeit*: neue Zeile **„Zeitquelle: <Backend|GPS|Netz|manuell|ungestellt>"**
  und optional **„letzter Abgleich vor N h"**.
- Beim ersten erfolgreichen Abgleich nach „ungestellt" ein **Info-Ereignis** ins Protokoll
  (neuer I-Code, Subsystem System), z. B. „Zeit synchronisiert (GPS)". Hilft im Service,
  ist für den Nutzer unaufdringlich.

## Persistenz & Parameter

- Neue `fp`-Felder: `uchar timesrc;` und `uint timesync_h;` (Betriebsstunden beim letzten
  Abgleich). In der bestehenden `fp`-Flashstruktur ablegen (battery-/flash-gepuffert).
- Konstanten (an einer Stelle, z. B. `rtc.h`):
  `RESYNC_STUNDEN 12`, `CLOCK_MIN_DELTA_S 3`, Jahresgrenzen `2024..2099`.

## Umsetzungsschritte (phasenweise, je einzeln bauen/testen)

1. `clock_set()` + `fp.timesrc/timesync_h` + Quellen-Enum + Validierung anlegen (rtc.c/.h/hard.h).
2. MQTT auf `clock_set` umstellen (Verhalten gleich) → bauen, gegen Backend testen.
3. GPS-RMC → `clock_set` (UTC). Test: ohne Netz, nur GPS-Fix → Uhr stimmt.
4. LTE-NITZ-Routine + Aufruf nach Registrierung. Test: drinnen ohne GPS → Uhr stimmt.
5. Manuelles Stellen durch `clock_set` führen (Buchführung).
6. Menüanzeige „Zeitquelle"/Abgleichalter + optionales Info-Ereignis.
7. Menüpunkt „Zeit jetzt synchronisieren" (erzwungener Abgleich, `FLAG_FORCE`) +
   Einstellung „Automatische Zeit: ein/aus" (`fp`-Flag, Default ein).
8. Drift-/Rang-Logik mit gestelltem `RESYNC_STUNDEN` durchspielen (Quelle entziehen → Nachzug prüfen).

## Konventionen (für alle Schritte)

- Quelldateien **ISO-8859-1 / CRLF** → **nur byte-sicher per Skript** editieren
  (das Edit-Tool zerstört sonst Umlaute). Doku/CSV = UTF-8.
- Nach jeder Code-Änderung **SW-Version +0.01**.
- Encoding nach jeder Datei prüfen (ISO-8859, kein `\r\r\n`, Nicht-ASCII-Zahl unverändert).
- Neue Info-/Diagnose-Codes: Zeile in `Diagnose-Codes.csv` + `tools/gen_dtc.py` neu laufen lassen.

## Entscheidungen (festgelegt)

- **Rang „manuell" = 2** – GPS/Backend dürfen eine manuelle Eingabe verfeinern.
- **`RESYNC_STUNDEN` = 12 h.**
- **NITZ best-effort:** liefert das Netz keine Zeit, bleibt die Stufe wirkungslos (kein Fehler).
- **Nutzer-Auslöser:** Menüpunkt „Zeit jetzt synchronisieren" (erzwungen) + Schalter
  „Automatische Zeit: ein/aus" (Default ein).

## Noch offen

- **Info-Ereignis:** bei **jedem** Abgleich ins Protokoll oder nur beim Wechsel
  „ungestellt → gestellt" (bzw. Quellenwechsel)? *Vorschlag: nur bei Wechsel, sonst zu viele Einträge.*
