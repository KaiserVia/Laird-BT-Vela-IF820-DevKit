# Testplan-Erweiterung BT: Strom, Neustart und Grenzfälle (Stand 2026-10-30, Firmware 5.127)

Ziel: Fehler finden, die die bisherigen Tests nicht abdecken. Neu verfügbar ist das Relais
(`tools/relay_power.py`, Devantech USB-RLY16L an COM7, Relais 2 schaltet die 12 V des Geräts).
Das Board hat 8 Relais, nur eines ist belegt.

## 1. Was die bisherigen Tests abdecken

| Modus (`ble_mainboard_test.py`) | Prüft |
|---|---|
| smoke, classic-smoke | Verbinden, PIN, Hauptmenü, 8 s halten |
| cycle | Verbinden/Trennen mit Haltezeiten 0 bis 300 s |
| idle-latency | Reaktion nach 20 bis 125 s Leerlauf im Menü |
| transfer, download, receive | Datenkanal, Ymodem 4,2 MB, Durchsatz |
| module-suite, fast-suite, classic-suite | Alles oben am Stück, mit Flash-Schreiben |
| interleave | Wechsel Classic/BLE |
| soak | Stundenlang Downloads, auch zwei Geräte im Wechsel |

**Gemeinsam ist allen:** Das Gerät läuft schon, ist im Leerlauf und wird nur vom PC aus angesprochen.
Nicht abgedeckt sind Stromausfall, Neustart, Bootphase, LTE-Betrieb, falsche oder fehlende PIN,
ein beschäftigtes Gerät und Abbrüche auf der PC-Seite.

## 2. Testfälle, nach Risiko geordnet

### P1. Neustart ohne Stromzyklus (Verdacht aus dem Code, höchste Priorität)

**Hintergrund:** 5.127 lässt UART1 im Leerlauf nur deshalb an, weil vorher ein `uart1_release()` lief
(nach `test_BT()` beim Einschalten, nach `init_bluetooth()`, nach jeder GSM-Sitzung). Nach einem
**Watchdog-Reset** oder einem **Reboot** (`reboot()`, Menü oder Zeitplan `BOOTTASK`) läuft laut `main.c`
weder `test_BT()` noch `init_bluetooth()`: Der Reboot-Zweig ist leer, der Watchdog-Zweig behandelt nur GSM.
UART1 bleibt dann vermutlich aus, und das IF820 sieht wieder CTS = HIGH. Das wäre derselbe Fehler wie
vor 5.127. Das IF820 selbst wird dabei nicht zurückgesetzt (`RST_L` ist nicht angeschlossen).

**Ablauf:**
1. Reboot über das Menü (oder `BOOTTASK` mit kurzer Zeit), danach 3x BLE-Smoke und 1x Classic-Smoke.
2. Watchdog-Reset auslösen: kurzer Stromausfall (siehe P2) oder, falls vorhanden, über das Testmenü.
   Danach dasselbe.
3. Gegenprobe: Stromzyklus 10 s, dann dasselbe.

**Erwartung:** Alles besteht. **Verdacht:** Nach 1 und 2 scheitert BLE wie vor 5.127 (kein `[BT-Event]`).
**Mögliche Korrektur (5.128):** am Ende des Hochlaufs bei `fp.btmodem==IF820` immer UART1 im
Leerlauf-Zustand aktivieren, unabhängig vom Reset-Grund.

### P2. Kurze Stromunterbrechungen (Relais)

**Hintergrund:** Bei Unterspannung springt `EINT0_IRQHandler` (flash.c) an, sichert Parameter,
Messdaten und Protokoll und wartet dann in `while(1)` auf das Abschalten oder den Watchdog. Kehrt der
Strom zurück, bevor das Gerät wirklich aus ist, endet das per Watchdog-Reset, also im Fall P1. Das
IF820 kann eine kurze Unterbrechung überleben, während der LPC neu startet, oder umgekehrt.
Dazu kommt der alte Befund "Modul braucht eine längere Stromlos-Zeit".

**Ablauf:** Unterbrechungen mit 0,05 / 0,1 / 0,2 / 0,5 / 1 / 2 / 5 / 10 / 30 s, je 5 Wiederholungen.
Nach jedem Zyklus:
- RS232: Banner mit Version innerhalb von 20 s? Reset-Grund im Protokoll?
- BT-Init-Meldungen (`BT init done` oder `F11xxx`)?
- BLE-Smoke, alle 5 Zyklen zusätzlich Classic-Smoke.
- Parameter unverändert (Seriennummer, PIN, Schnittstellen im Info-Menü)?

**Auswertung:** Tabelle Stromlos-Zeit gegen Ergebnis. Interessant sind die Zeiten, bei denen
LPC und Modul nicht gemeinsam neu starten.

### P3. Stromausfall in kritischen Momenten

| Moment | Risiko | Prüfung danach |
|---|---|---|
| Während einer BLE- bzw. Classic-Verbindung im Menü | Windows hält die alte Verbindung bzw. den COM-Port, Wiederverbinden scheitert | Smoke beider Transporte, wie lange bis zur ersten erfolgreichen Verbindung |
| Mitten im Ymodem-Download | wie oben, zusätzlich PC-Seite (`F13010`) | erneuter Download vollständig |
| Während "Flash schreiben" (Messwerte) | Messdatenzeiger kaputt (siehe `Analyseplan_Messdatenzeiger-Korruption.md`) | Anzahl Messwerte plausibel, Download ok |
| Während `init_bluetooth()` schreibt (`/SCFG`, `SDN$`, `/RBT`) | Modul-Konfiguration halb geschrieben | Hochlauf erkennt und korrigiert, `GTU`/`GAP`/Name stimmen |
| Während einer Parameteränderung im Menü | Parameterseite inkonsistent | Info-Menü, Seriennummer, PIN |

Den Zeitpunkt löst das Skript aus, wenn es auf RS232 bzw. im Datenstrom eine bestimmte Zeile sieht
(z. B. "Step 3c" für die Modul-Konfiguration), plus eine einstellbare Verzögerung.

Für den Fall "Modul-Konfiguration halb geschrieben" geht es einfacher mit dem Werksreset über das
Testmenü (8 → 4) und einem anschließenden Stromzyklus. Der Hochlauf muss das Modul dann wieder
vollständig einrichten (Selbstheilung seit 5.16), danach Smoke.

### P4. Verbinden während des Hochlaufs (Boot-Race)

**Hintergrund:** Das IF820 wirbt direkt nach dem Einschalten, die Firmware ist aber noch in
`test_BT()` oder `init_bluetooth()` (Command-Mode, eventuell `/RBT`). Ein Kunde, der sofort verbindet,
trifft genau diesen Zeitraum.

**Ablauf:** Stromzyklus, der PC verbindet sich mit fester Adresse (ohne 15-s-Scan) nach 1 / 2 / 3 / 5 /
8 / 12 s. Je Verzögerung 3 Versuche. Ein Zusatzfall: das Modul vorher per Werksreset zurücksetzen, damit
der Hochlauf ein `/RBT` braucht.

**Erwartung:** Entweder verbindet der PC sauber, oder der Versuch scheitert und der **nächste** gelingt.
Fehler ist, wenn das Gerät danach bis zum nächsten Stromzyklus nicht mehr verbindbar ist.

### P5. BT während einer LTE-Sitzung (nur Board B, LTE aktiv)

**Hintergrund:** Während einer GSM-Sitzung steht der UART1-MUX auf GSM. Die UART-Leitungen des IF820 sind
dann vom LPC getrennt, das Modul sieht CTS also nicht LOW. Das ist dieselbe Lage, die 5.127 für den
Leerlauf behoben hat. `communication_change()` läuft außerdem nicht, solange ein GSM-Thread aktiv ist
(`osActiveThread`).

**Ablauf:**
1. LTE-Aufgabe auslösen (MQTT-Status, Email bzw. SMS laut Zeitplan oder manuell über das Funkmodem-Menü).
2. Während der Sitzung per BLE verbinden, PIN-Prompt abwarten (spätestens nach Ende der Sitzung).
3. Nach Ende der Sitzung erneut BLE- und Classic-Smoke.
4. Umgekehrt: BT verbunden lassen, während der Zeitplan eine LTE-Aufgabe fällig macht.

**Erwartung:** BT verbindet spätestens nach Ende der GSM-Sitzung. Danach ist alles normal. Fehler ist,
wenn der Zustand "kein `[BT-Event]`" bis zum nächsten Stromzyklus bleibt.

### P6. Langer Leerlauf ohne Verbindung (5.127-Pfad)

**Hintergrund:** Seit 5.127 empfängt UART1 im Leerlauf dauerhaft in `rbuf1`. Läuft `rbuf1` voll, schaltet die
ISR den Interrupt ab (RTS geht HIGH). Die Wiederfreigabe hängt an der Hauptschleife. Bisher gibt es nur
Tests mit Leerlauf im Menü bei bestehender Verbindung (idle-latency, max. 125 s).

**Ablauf:** Gerät 1 / 4 / 12 h unverbunden laufen lassen, stündlich einmal BLE-Smoke (mit fester Adresse,
damit der Scan nicht stört), jeden 4. Lauf Classic. Ohne LTE (A, C) und mit LTE (B).

### P7. Verbinden, während das Gerät beschäftigt ist

**Hintergrund:** Befund B6 (nach "Flash schreiben" keine `[BT-Event]` mehr bis zum Neustart) ist nicht
geklärt. Solange die Hauptschleife blockiert, wird `rbuf1` nicht geleert.

**Ablauf:** Über RS232 eine lange Aktion starten (Flash schreiben mit vielen Messwerten, Ymodem-Download
über RS232, Online-Messung) und währenddessen per BLE verbinden. Danach erneut verbinden.
Prüfen: `[BT-Event]`, PIN-Prompt, Zustand nach Ende der Aktion.

### P8. PIN-Abfrage und Bedienung

| Fall | Erwartung |
|---|---|
| Falsche PIN (6 Ziffern) | neue Abfrage, keine Sperre (Hinweis: es gibt kein Limit, 6 Ziffern sind also durchprobierbar) |
| Keine PIN, Verbindung halten bis zum Zeitlimit `BT_TIMEOUT` (6 min) | Gerät trennt sauber (`bt_disconnect()`), danach wieder verbindbar |
| Trennen mitten in der PIN-Eingabe | nächste Verbindung beginnt sauber mit "PIN =" |
| Während offener PIN-Abfrage über RS232 bedienen | RS232 bleibt bedienbar (laut früherer Notiz blockiert `bt_pininit` auch RS232) |
| Gleichzeitig Eingaben über RS232 und BT im Menü | keine verlorenen oder fremden Zeichen |

### P9. Abbrüche auf der PC-Seite

- Python-Prozess mitten im BLE- bzw. Classic-Download hart beenden.
- Classic-COM-Port während der Übertragung schließen, ohne vorher zu trennen.
- Gerät außer Reichweite bzw. Funkstrecke unterbrechen (Abschirmung, z. B. Metallbox, statt Software).

Erwartung: Das Gerät erkennt die Trennung, bricht Ymodem ab (bisher nicht: `F13010` erst nach Minuten,
geplante Korrektur "Ymodem bei Linkverlust abbrechen") und ist danach wieder verbindbar. Gemessen wird die
Zeit bis zur nächsten erfolgreichen Verbindung.

### P10. Zwei Gegenstellen gleichzeitig

- Classic verbunden, gleichzeitig BLE-Verbindung von einem zweiten Central (nRF52840-Dongle oder Handy).
- BLE verbunden, zweiter BLE-Central versucht zu verbinden.

Erwartung: Der zweite Versuch wird abgewiesen, die erste Sitzung läuft ungestört weiter.

## 3. Umsetzung im Testskript

**Umgesetzt (2026-10-30): `reboot-check`, `power-sweep`, `boot-race`.** Offline geprüft (Hochlauf-Erkennung,
Relais-Zeitmessung), am Gerät noch nicht gelaufen. Voraussetzungen: Relais an COM7 (Relais 2), RS232-Mitschnitt
über den **externen Adapter** (COM3), nicht über den USB-Port des Boards (der verschwindet beim Stromlos-Schalten).
Das Skript setzt das Relais am Ende und bei Abbruch immer auf "Strom an".

```
python ble_mainboard_test.py --mode reboot-check --name VIASIS_26SC9994 --pin 269994 --rs232-port COM3 --reboot-bin ..\..\sism.bin
python ble_mainboard_test.py --mode power-sweep  --name VIASIS_26SC9994 --pin 269994 --rs232-port COM3
python ble_mainboard_test.py --mode boot-race    --name VIASIS_26SC9994 --pin 269994 --rs232-port COM3
```

| Modus | Ablauf je Schritt | Wichtige Optionen |
|---|---|---|
| `reboot-check` (P1) | Neustart per `firmware` (Neu-Flashen derselben Version, Weg ohne Stromzyklus; **löscht Protokoll und Messdaten**), `dropout` (kurze Unterbrechung, evtl. Watchdog-Reset) oder `power` (Kontrolle); danach `--checks` BLE-Smokes und 1 Classic-Smoke | `--reboot-methods`, `--reboot-bin`, `--dropout 0.1`, `--power-off 10`, `--repeat 3` |
| `power-sweep` (P2) | Strom `t` s weg, Hochlauf über RS232 abwarten (Banner, `BT init done`, Fehlercodes), BLE-Smoke, jeder n-te Schritt Classic | `--off-times`, `--repeat`, `--classic-every 3` |
| `boot-race` (P4) | Stromzyklus, nach `d` s ab Einschalten **ein** BLE-Versuch mit fester Adresse; bei Fehlschlag ein Folgeversuch | `--race-delays`, `--race-off 10`, `--race-retry-wait 10` |

Gemeinsam: `--boot-timeout 45`, `--boot-quiet 3` (Hochlauf fertig, wenn RS232 so lange ruhig ist), `--step-hold 3`,
`--max-failures`, `--relay-port`, `--relay`. Ergebnis je Schritt (Stromlos-Zeit, Banner-Zeit, BT-Init, Fehlercodes,
Zahl der `[BT-Event]`-Zeilen, Smoke-Ergebnisse) in der JSON, Erfolg je Wert im Log und als Suite-Bericht (HTML).

Hinweis zu P1: Einen Reboot-Menüpunkt gibt es nicht (`STRT` in `sicom.c` auskommentiert), `BOOTTASK` hängt am
Zeitplan. Der Watchdog-Weg wird daher über `dropout` angestrebt. Ob eine Unterbrechung wirklich als Watchdog-Reset
endet, zeigt das Geräteprotokoll (Information → Protokoll, Eintrag "Watchdog reset").

Noch nicht umgesetzt (ursprünglicher Vorschlag):

| Modus | Inhalt |
|---|---|
| `power-sweep` | P2: Liste von Stromlos-Zeiten, je N Wiederholungen, danach RS232-Banner prüfen und BLE/Classic-Smoke |
| `boot-race` | P4: Stromzyklus, Verbinden mit fester Adresse nach einer Liste von Verzögerungen |
| `power-cut` | P3: Strom wegschalten, sobald eine Markierung (RS232 oder Datenstrom) erscheint, plus Verzögerung; danach Prüfung |
| `reboot-check` | P1: Reboot über das Menü, danach Smoke-Tests; optional Watchdog über kurzen Stromausfall |
| `idle-soak` | P6: lange unverbunden, in festen Abständen Smoke |

Gemeinsame Regeln: Nach jedem Schritt Gerätezustand über RS232 lesen (Version, Reset-Grund aus dem
Protokoll, `[BT-Event]`-Zeilen). Bei einem Fehler zuerst einen Stromzyklus mit 30 s, um zwischen "Gerät
hängt" und "Modul hängt" zu unterscheiden. Bei Abbruch das Relais immer wieder auf "Strom an" setzen
(das Relais rastet).

## 5. Tests am Tisch ohne Handarbeit (umgesetzt 2026-10-30)

Weitere Modi in `ble_mainboard_test.py`, offline mit nachgebildeter Firmware geprüft (XMODEM mit NAK, Uhr stellen,
Protokoll lesen), **am Gerät noch nicht gelaufen**. Alle brauchen den RS232-Mitschnitt (externer Adapter, COM3).
Nach jedem Schritt laufen `--checks` BLE-Smokes (Default 2) und ein Classic-Smoke.

| Modus | Was er prüft | Ablauf | Bestanden, wenn |
|---|---|---|---|
| `nightly` | **Nächtlicher Neustart um 3:15** (`BOOTTASK`, `rtc.c`). Verdacht: Der Reboot-Zweig in `main.c` ist leer, UART1 bleibt aus, BT geht bis zum nächsten Stromzyklus nicht. Betrifft jedes Gerät im Feld täglich. | Uhr über RS232 auf `--nightly-set` (03:13:30), bis 03:15 + 45 s warten, Neustart im Geräteprotokoll nachweisen (`Reboot;..;03:15`), BT prüfen, Uhr auf PC-Zeit zurück | Neustart im Protokoll und alle Smokes bestanden |
| `gsm-mux` | UART1-MUX auf GSM (LTE-Sitzung), ohne SIM und Zeitplan. Nur auf einem Board mit LTE-Modem (B). | Testmenü → Strg+W → 7 → 9600 → GSM, `--gsm-hold` 60 s offen (dabei ein BLE-Versuch, nur zur Information), Strg+Z, zurück | Smokes nach dem Schließen bestanden |
| `bt-timeout` | Zeitlimit `BT_TIMEOUT` (6 min ohne Empfang, `measure.c`): Gerät trennt selbst über `bt_disconnect()`. | verbinden (`--transport ble` oder `classic`), PIN, dann nichts senden, bis zu `--timeout-wait` 480 s | Trennung nach 300 bis 440 s (Meldung "Bluetoothverbindung getrennt" oder Link weg) und danach wieder verbindbar |
| `bt-update` | **Firmware-Update über Bluetooth Classic** wie ein Servicetechniker. Der LPC startet neu, das IF820 hält die Verbindung. | Classic verbinden, PIN, Testmenü, Strg+U, XMODEM-1K mit `--reboot-bin`, `y` | Übertragung ok, PIN-Prompt über dieselbe Verbindung nach dem Neustart, danach Smokes bestanden |

```
python ble_mainboard_test.py --mode nightly    --name VIASIS_26SC9994 --pin 269994 --rs232-port COM3 --repeat 2
python ble_mainboard_test.py --mode gsm-mux    --name VIASIS_26SC9983 --pin 269983 --rs232-port COM3
python ble_mainboard_test.py --mode bt-timeout --name VIASIS_26SC9994 --pin 269994 --rs232-port COM3 --repeat 1
python ble_mainboard_test.py --mode bt-update  --name VIASIS_26SC9994 --pin 269994 --rs232-port COM3 --reboot-bin ..\..\sism.bin --repeat 1
```

**Achtung:** `bt-update` und `reboot-check` mit Methode `firmware` löschen im Gerät Protokoll und Messdaten. `nightly`
liest das Protokoll; deshalb `nightly` vor diesen beiden laufen lassen. `nightly` setzt nur voraus, dass die Batterie-
spannung über der Grenze liegt (sonst unterdrückt `Aufgabenzeit()` den Neustart).

Bekannte Grenzen: Nach dem 3:15-Neustart gibt die Firmware auf RS232 vermutlich kein Banner aus (`connect` bleibt im
Zweig `BOOT_RES` unverändert), deshalb der Nachweis über das Protokoll. `gsm-mux` auf einem Board ohne Modem öffnet
kein Terminal (Fehlercode im Bericht) und sagt dann nichts aus.

### Zurückgestellt: Handarbeit (später)

- Handy (nRF Connect) als zweite Gegenstelle, Android und iOS.
- Gerät in Windows entfernen und neu koppeln (Ablauf wie beim Neukunden).
- RS232 bzw. USB abstecken, während BT verbunden ist (geht auch per Relais, braucht Verdrahtung).
- Schwache Funkstrecke (Blechdose, Abstand) mit Sniffer.
- "Online messen" und Anzeige-/Schaltersimulation bei bestehender BT-Verbindung (Ausgabe hängt an simulierten
  Messwerten, noch nicht automatisiert).
- Parameteränderung über BT im Einstellungsmenü.

## 4. Empfohlene Reihenfolge

1. **P1** sofort von Hand prüfen (Reboot über das Menü, dann Smoke). Bestätigt sich der Verdacht, 5.128
   vor dem Merge.
2. **P2 und P4** als Skript-Modi, laufen unbeaufsichtigt (etwa 1 bis 2 h je Board).
3. **P5** auf Board B (braucht LTE und SIM).
4. **P3, P6, P7** danach; P8 bis P10 bei Gelegenheit.
