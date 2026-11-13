# radar_v2_test.py - Radar-Empfänger automatisch prüfen (Stand 2026-11-13)

Prüft am Tisch, ob die Firmware den Radar-Empfänger richtig erkennt (Via-2 oder bisheriger Transceiver, `fp.radar`)
und ob die Funktionen aus der Integration von Joachims Stand 5.60 laufen. Das Skript bedient das Gerät nur über
RS232, schaltet die Versorgung über das USB-Relais und ruft für Flashen und Bluetooth die vorhandenen Werkzeuge auf.
Testfälle und Kriterien im Einzelnen: `../Testplan_Radar_V2.md`.

## Voraussetzungen

- RS232-Adapter am Board (Standard `COM3`, 115200 Baud), Relais an `COM7`, Relais 2 schaltet die Versorgung
  (`relay_power_Anleitung.md`). Pro Board nur eine Versorgungsquelle.
- Python mit `pyserial`. Für den Bluetooth-Teil des langen Profils dieselben Voraussetzungen wie `bt_testlauf.py`
  (Board in Windows gekoppelt).
- Board und Ports dürfen von keiner anderen Sitzung belegt sein. Vorher in den anderen Chats Bescheid geben.
- Firmware ab 5.L1 bzw. 5.138 (Typ-Erkennung). Das Skript läuft auf Boards mit beiden Empfängern; Tests, die nur für
  Via-2 gelten, erscheinen beim bisherigen Transceiver als "-" (nicht anwendbar).

## Aufruf

```bash
python radar_v2_test.py --profil kurz
```

```bash
python radar_v2_test.py --profil lang
```

```bash
python radar_v2_test.py --profil lang --ohne flash,bt
```

Nur die Zusatzteile des langen Profils (Stromlos-Zeiten, Kennlinie, Neustart 3:15) ohne die 10 Stromzyklen, ca. 10 min:

```bash
python radar_v2_test.py --profil lang --ohne flash,bt --zyklen 0
```

| Option | Standard | Bedeutung |
|---|---|---|
| `--profil kurz/lang` | kurz | siehe unten |
| `--port`, `--baud` | COM3, 115200 | RS232 am Board |
| `--relay-port`, `--relay` | COM7, 2 | USB-Relais |
| `--bin` | `../../sism.bin` | Firmware für das Neu-Flashen im langen Profil. Sollte die Version sein, die schon drauf ist |
| `--ohne` | - | lang: `flash` (nicht neu flashen), `bt` (kein `bt_testlauf.py`), `nightly` (kein Neustart 3:15) |
| `--zyklen` | 10 | lang: Anzahl Stromzyklen (A2) |
| `--off-times` | 0.1,0.5,1,10 | lang: Stromlos-Zeiten in s (A3) |
| `--online-s` | 30 | Dauer Online-Messen (C3) |

## Profile

**kurz (ca. 2-3 min, löscht nichts):** Typ und I2C-Scan (A0), Frequenz im Transceiver-Menü (B2), ein Stromzyklus
5 s (A1), 10 Frequenzmessungen mit Strg+T (B1), Transceiver aus nach Hochlauf, Strg+T, Online-Messen und I2C-Test
(C1-C3, E2a), "I2C Devices" ohne Baustein 3x (E1, Korrektur 5.L2), IC32 lesen (E2), Lichtsensor (F3), neue
Protokolleinträge ohne I17005 und ohne unerwartete F/W-Codes (A2x, F4).

**lang (ca. 25-30 min):** zuerst Neu-Flashen über RS232 (D1, **löscht Protokoll und Messdaten**) und Prüfung ohne
Stromzyklus (Pfad `FIRM_UP_RES`: D1a, C1a) sowie Sprache/Seriennummer/HW unverändert (D3). Dann alles aus "kurz",
dazu 20 Frequenzmessungen (B1b), 10 Stromzyklen (A2.x), Stromlos-Zeiten 0,1/0,5/1/10 s (A3.x), Frequenzkennlinie
über alle 257 Stufen von IC32 mit Rückstellung des Potis (B3, B4, `kennlinie.csv`), nächtlicher Neustart 3:15 (Uhr
wird auf 03:14:30 und danach auf PC-Zeit gestellt; C4a/b, C4), Funkmodem-Status Strg+F (F2, nur Anzeige) und
`bt_testlauf.py --profil kurz --nur reboot-check` (F1).

## Ergebnis

Ordner `Testreports/<Datum_Zeit>_radar_<profil>/`:

- `bericht.md` - Tabelle aller Tests (ok / **FEHLER** / - nicht anwendbar) mit Messwerten
- `ergebnis.json` - dasselbe maschinenlesbar (für andere Skripte und Chats), dazu `meta` mit Version, Seriennummer,
  erkanntem Empfänger, I2C-Scan und den neuen Protokolleinträgen
- `ablauf.log` - Ablauf mit Zeitstempeln, auch die Ausgaben der aufgerufenen Werkzeuge
- `rs232.log` - kompletter RS232-Mitschnitt, gesendete Tasten als `>>> b'...'`
- `kennlinie.csv` - nur lang, Via-2: Stufe; MHz

Exit-Code 0 = bestanden, 1 = mindestens ein Test fehlgeschlagen, 2 = Ablauf abgebrochen (Gerät nicht erreichbar,
Menü nicht wie erwartet; Test `X` im Bericht).

## Wie das Skript prüft

- **Typ:** Werksmenü (Testmenü 3, Strg+W) -> 4. Via-2 zeigt `Frequency: ... MHz` und 5 Menüpunkte, der bisherige
  Transceiver 3. Muss zum I2C-Scan passen (IC32 auf 0x5C nur bei Via-2).
- **Frequenz (B1):** Strg+T im Hauptmenü, jeder Wert im Band 24150-24200 MHz (`Min_TXF_VIA2`/`Max_TXF_VIA2`).
  Bewertet wird der Median: einzelne Werte dürfen bis 3 MHz davon abweichen, höchstens 10 % der Werte um mehr als
  1 MHz (beobachtet: einzelne Messungen 2-3 MHz zu tief). Ab 5.142 zusätzlich Median = Kanalfrequenz ± 2,5 MHz
  (Totband der Nachführung 1,5 MHz plus 1 MHz, weil Strg+T auf ganze MHz abschneidet).
- **Frequenz im Transceiver-Menü (B2):** ab 5.142 die Zeilen `Kanal n (...), Ziel x MHz` und `Frequenz x MHz`,
  Kriterium Frequenz = Ziel ± 1,5 MHz; Kanal, Ziel und Stufe von IC32 stehen in `ergebnis.json` (`meta`). Bei älterer
  Firmware (`Frequency: x MHz`) nur das Band. Das Menü verlässt das Skript immer mit `R` (eine Ebene zurück), nie mit
  einer Ziffer: bei 5.142 ist `5` "Abgleich auf Kanal".
- **Transceiver aus:** Werksmenü -> 6 -> 6 "I2C Devices", Adresse 66 (IC37). Register 3 (Ausgang Port 1) und
  Register 1 (Pegel Port 1), Bit 4 = HF_DIS muss 1 sein.
- **Hochlauf:** Nur der Kaltstart gibt den Banner `Version x.y` aus. Nach einem kurzen Aussetzer (0,1-1 s stromlos,
  Neustart ohne Banner und ohne Protokolleintrag "Eingeschaltet", einmal "I31 Watchdog reset" beobachtet) und nach
  dem Neustart 3:15 (Protokoll "I7 Reboot") erkennt das Skript den Hochlauf an der IF820-Zeile
  `[BT-Zustand] BEREIT (Hochlauf fertig`. Im Bericht steht dann "(ohne Banner)". Bei A3 zählt nur, dass danach der
  Typ stimmt; ob das Gerät neu startet oder durchläuft, ist beides zulässig.
- **Protokoll:** Information -> 1, nur lesen, auf "Daten löschen" immer `n`. Bewertet werden nur die Einträge, die
  während des Laufs dazugekommen sind. Die Fehler F20002 aus E1 erzeugt der Test selbst, sie zählen nicht.

## Bekannte Grenzen

- Deep Sleep (`SLEEP_RES`) lässt sich mit angeschlossenem RS232 nicht prüfen: Das Gerät erkennt das Terminal
  (`RS232_LINK`) und schläft dann nicht.
- Typwechsel mit Meldung I17005 und das Board mit dem bisherigen Transceiver lassen sich nur mit einem solchen Board
  prüfen.
- Messung mit echtem Doppler-Signal, Verstärkerabgleich und "Set Module Frequency" werden nicht automatisch getestet
  (verändern gespeicherte Werte bzw. brauchen ein Signal).
- Parameter auf Werkseinstellung (Testmenü, `X`) wird nicht ausgeführt: setzt alle Parameter zurück.
