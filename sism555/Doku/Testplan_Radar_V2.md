# Testplan Radar-Empfänger / Joachim-Integration (5.L1 ... 5.138), automatisch am Tisch

Stand 2026-11-13. Ziel: Die Änderungen aus der Integration von Joachims Stand 5.60 (Via-2-Empfänger,
Typ-Erkennung `fp.radar`, Mainboard V2) mit den vorhandenen Mitteln automatisch prüfen:

- RS232 am Board (COM3, 115200), darüber Menüs bedienen und Ausgaben auswerten
- Relais für die Versorgung (COM7, `relay_power.py`)
- PC-Bluetooth (BLE und Classic) über die vorhandenen Werkzeuge `bt_testlauf.py` / `ble_mainboard_test.py`
- Flashen über `flash_xmodem.py`

Umgesetzt in `Doku/tools/radar_v2_test.py` mit den Profilen `kurz` (ca. 2-3 min) und `lang` (ca. 25-30 min),
Bedienung in `Doku/tools/radar_v2_test_Anleitung.md`, Bericht unter `Doku/tools/Testreports/`. Alle Werte, die das
Skript prüft, liest es über RS232 aus dem Gerät. Die Nummern unten sind dieselben wie im Bericht des Skripts.

**Nicht im Skript** (siehe jeweilige Zeile): A4 (setzt alle Parameter zurück), B5 (braucht ein Doppler-Signal),
C5 (Deep Sleep findet mit angeschlossenem RS232 nicht statt), D2 (eigener Modus `bt_testlauf.py --nur bt-update`).

## Woran der erkannte Typ sichtbar ist

`fp.radar` wird nirgends direkt angezeigt. Indirekt und eindeutig:

| Merkmal | Via-2 | bisheriger Transceiver |
|---|---|---|
| Werksmenü (Testmenü 3, Strg+W) -> 4 "Amplifier & Transceiver" | Zeile `Frequency: ... MHz`, 5 Menüpunkte | keine Frequenz, 3 Menüpunkte |
| Strg+T im Hauptmenü | Zeile `<f> MHz` | keine Ausgabe |
| I2C-Scan (Werksmenü -> 6 -> 5) | `5Ch MCP4561, IC32` vorhanden | kein 0x5C |

## A. Erkennung und Speicherung

| Nr | Test | Ablauf | Bestanden, wenn |
|---|---|---|---|
| A1 | Erkennung nach Kaltstart | Relais 5 s aus, Hochlauf mitschneiden, dann Transceiver-Menü öffnen | Banner mit Version, Via-2-Menü mit Frequenz, kein F-Code im Protokoll |
| A2 | Typ bleibt über viele Starts gleich | 10 Stromzyklen (Relais), jeweils Strg+T | jedes Mal Frequenz im Bereich, Protokoll enthält nur I1 (Eingeschaltet), **kein I17005** |
| A3 | Erkennung bei allen Stromlos-Zeiten | `power-sweep` 0,1 / 0,5 / 1 / 10 s (Brown-out und POR) | wie A2; Strg+T und BT danach in Ordnung |
| A4 | Parameter auf Werkseinstellung (Testmenü 3, `X`) | danach Neustart | wieder Via-2 erkannt, kein I17005 (Typ war danach unbekannt) |

## B. Via-2-Funktionen

| Nr | Test | Ablauf | Bestanden, wenn |
|---|---|---|---|
| B1 | HF-Frequenzmessung wiederholbar | 10 x (kurz) bzw. 20 x (lang) Strg+T | alle Werte 24150-24200 MHz, keiner 0; jeder Wert max. 3 MHz vom Median, höchstens 10 % mehr als 1 MHz daneben. Ab 5.142: Median = Kanalfrequenz ± 2,5 MHz (Totband 1,5 + Abschneiden auf MHz) |
| B2 | Frequenz im Transceiver-Menü | Menü öffnen, Statuszeilen lesen | ab 5.142: `Frequenz` = `Ziel` ± 1,5 MHz (Totband); vorher nur Band |
| B3 | Frequenzkennlinie IC32 | Transceiver-Menü -> 2, Ablauf bis Schritt 257 laufen lassen (ca. 1 min), Ausgabe parsen | 257 Zeilen, Frequenz steigt (oder fällt) über den Bereich durchgehend, keine 0, Sprünge nur klein |
| B4 | Kennlinie stellt das Poti wieder her | IC32-Register 0 vor und nach B3 lesen ("I2C Devices" -> 92) | Register 0 vorher = nachher |
| B5 | Strg+T während der Online-Messung | Online messen starten, Strg+T, IC37 lesen geht dort nicht, daher: weiter messen lassen und mit einem Doppler-Signal (Hand vor dem Radar) prüfen, ob noch Messwerte kommen | Messung läuft nach Strg+T weiter. **Verdacht aus dem Code:** Strg+T wird in `mainmenu()` vor der Abfrage `!online` behandelt und schaltet den Transceiver danach aus. Möglich, dass das die laufende Messung bis zum nächsten Zyklus abschaltet |

`set_single_dpp` (Menüpunkt 3) und der Verstärkerabgleich (Punkt 1) verändern gespeicherte Abgleichwerte. Sie
werden nicht automatisch getestet.

## C. Transceiver nach jedem Ablauf wirklich aus

Bei Via-2 schaltet IC37 P1.4 (HF_DIS) den Transceiver ab; 1 = aus. Lesbar über Werksmenü -> 6 -> 6 -> 66 (IC37):
Register 3 (Ausgang P1) Bit 4, Register 1 (Eingang P1) Bit 4 zeigt den tatsächlichen Pegel.

| Nr | Zeitpunkt | Bestanden, wenn |
|---|---|---|
| C1 | direkt nach dem Hochlauf | Bit 4 in Register 3 und 1 gesetzt |
| C2 | nach Strg+T, nach dem Transceiver-Menü, nach der Kennlinie (B3) | gesetzt |
| C3 | nach 60 s Online-Messen und Return | gesetzt |
| C4 | nach dem nächtlichen Neustart 3:15 (Uhr auf 03:14:30 stellen, wie `bt_testlauf.py --nur nightly`) | gesetzt, Strg+T geht |
| C5 | nach Aufwachen aus dem Deep Sleep | gesetzt, Strg+T geht. Prüft den Pfad `SLEEP_RES`, in dem `Init_Radar_Ports()` den Typ aus dem RAM nimmt. **Nicht automatisch:** Mit angeschlossenem RS232-Adapter ist `RS232_LINK` gesetzt und das Gerät schläft nicht (measure.c). Nur prüfbar mit abgezogenem Adapter, danach über BT |

## D. Reset- und Update-Pfade

| Nr | Test | Bestanden, wenn |
|---|---|---|
| D1 | Firmware-Update über RS232 (`flash_xmodem.py`, gleiche Version noch einmal) | Banner nach dem Update, Strg+T geht **ohne** Stromzyklus (Pfad `FIRM_UP_RES`), C1 erfüllt |
| D2 | Firmware-Update über Bluetooth Classic (`bt_testlauf.py --nur bt-update`) | wie D1, BT danach in Ordnung |
| D3 | Sprache/OEM nach Update (`fp.sprache` jetzt mit OEM- und Sprachpaket-Bits) | Infoseite vor und nach D1/D2 gleich (Sprache "German"), Menütexte deutsch, Werkskonfiguration "Language: German" |

## E. Korrektur 5.L2 (`test_i2c_device`)

| Nr | Test | Bestanden, wenn |
|---|---|---|
| E1 | "I2C Devices" mit Adresse ohne Baustein (z. B. 48 = 0x30), 5 x | jedes Mal Fehler 20002 (Expander), nie 20001, Gerät reagiert danach |
| E2 | "I2C Devices" mit 92 (IC32) und 66 (IC37), mit "n" verlassen | Register werden gelesen, danach C1 erfüllt (Transceiver aus) |

## F. Regression außerhalb des Radars

| Nr | Test | Werkzeug | Bestanden, wenn |
|---|---|---|---|
| F1 | Bluetooth nach Neustart | `bt_testlauf.py --profil kurz --nur reboot-check,boot-race` | passed |
| F2 | LTE-Modem (geänderter PWRKEY-Puls 600 ms, CTS-Wartezeit) | Funkmodem-Status (Strg+F) nach Hochlauf, `online-bt` | Modem erkannt und registriert. MQTT trennt derzeit serverseitig nach 1-8 s, das gilt nicht als Fehler |
| F3 | Lichtsensor (5.138) | Testmenü -> 6, 5 Messungen | VEML7700 erkannt, Lux > 0 |
| F4 | Protokoll am Ende | Information -> 1 | keine F- und W-Codes aus dem Lauf |

## Nicht mit diesen Mitteln prüfbar

- **Board mit dem bisherigen Transceiver:** Erkennung `RADAR_TRX`, kein Zugriff auf 0x5C, EN_ANA-Pfad, alte Menüs.
  Dafür braucht es ein solches Board. A1, A2, C (dann mit EN_ANA statt HF_DIS) und E laufen dort unverändert.
- **Meldung I17005 bei Typwechsel:** braucht entweder beide Boards (Mainboard tauschen ist nicht vorgesehen) oder
  einen kleinen Test-Hook in der Firmware (z. B. versteckte Taste im Werksmenü: `fp.radar` auf den anderen Typ setzen
  und sichern; beim nächsten Start muss I17005 mit dem erkannten Typ kommen).
- Echte Messung mit Fahrzeugen, Verstärkerabgleich, Pegel an DIS_IF / DIS_MESS / FMin (Oszi), Stromaufnahme.

## Laufzeit

Profil `kurz`: A0, A1, B1 (10x), B2, C1-C3, E1, E2, F3, F4 - ca. 2-3 min, löscht nichts.
Profil `lang`: zusätzlich D1/D3 (Neu-Flashen, löscht Protokoll und Messdaten), B1 (20x), A2 (10 Stromzyklen),
A3, B3/B4, C4 (Neustart 3:15), F1, F2 - ca. 25-30 min.
