# Testplan: CONNECTION-Pin direkt an LPC P0.19

> Board **99SC9923**, Branch `connection-pin`, Firmware **5.114**. Schrittweise abarbeiten, Ergebnis je
> Schritt eintragen (ok / Abweichung + Beobachtung). Bei einer Abweichung **nicht weitermachen**, sondern
> erst die Spalte "Bei Abweichung" lesen.

## Hardware-Stand (vom Anwender bestaetigt)

- IF820 `CONNECTION` (Pin 33) direkt mit **P0.19** verbunden, **IC54 IO5 getrennt**.
- Kein externer Pull-up auf der Leitung (nur der interne des LPC) - siehe T1.
- CONNECTION ist low aktiv: **LOW = Datenpipe steht**, HIGH = Idle.

## Was die Firmware damit macht (damit die Erwartungen verstaendlich sind)

- **Anzeige:** Testmenue 7 zeigt CYSPP (IC54 Bit7) und CONNECTION (P0.19) als Paar mit Bewertung.
- **Produktiv (ab 5.110):** Die UART1-Eingabe-Weiche (hard.c) nimmt Bytes nur dann als Tastatureingabe
  (`rbuf`), wenn `BT_LINK` gesetzt ist **und CONNECTION wirklich LOW** ist. Sonst gehen sie nach `rbuf1`
  (Modul-Events) und werden verworfen. `BT_LINK` selbst kommt weiterhin von CYSPP.
- **Folge:** Geht CONNECTION bei einer Verbindung nicht auf LOW, wird **keine** BT-Eingabe akzeptiert
  (z. B. PIN). Notausstieg: `#define BT_CONN_GPIO 0` in `btio.h`, neu bauen, flashen.

## Voraussetzungen / Werkzeuge

| Was | Wert |
|---|---|
| RS232 (Terminal) | COM3, 115200 |
| BT-Classic-Terminal | COM11 (Viagraph/HyperTerminal) |
| Firmware | 5.114 (`Hauptmenue -> 6 Information` zeigt die Version) |
| Test-Script (T9) | `Doku/tools/ble_mainboard_test.py` (pyserial, bleak) |
| Gekoppelt | Windows-PC mit dem Geraet gekoppelt (`VIASIS_99SC9923`) |

Regel fuer alle Schritte: **RS232 bleibt waehrend der BT-Tests offen** (dort sieht man das Monitor-Ergebnis
und eventuelle Fehlzeichen). Pro Schritt die Monitor-Bloecke (`Zustand [+... ms]:`) und auffaellige
RS232-Zeilen sichern.

**Monitor ab 5.115 (Hintergrund):** `Testmenue (3) -> 7` startet den Monitor und kehrt **sofort** zurueck;
er laeuft 5 min im Hintergrund und gibt jede Pegelaenderung (mit Zeitstempel `[+ms]`) am **RS232**-Terminal
aus, waehrend das Menue normal bedienbar bleibt. Nochmal `7` waehlen = beenden. Der Monitor bleibt
**nicht** im Menue stehen - nach dem Start mit `9` zurueck ins **Hauptmenue**, dort laeuft alles Weitere
(verbinden, PIN, trennen). (Der alte blockierende Monitor hielt die Hauptschleife an, in der `communication_change()`
laeuft: ein BT-Connect wurde waehrend des Monitors nicht verarbeitet, kein PIN-Prompt - Fehler von 5.110-5.114.)

**Ablauf-Muster:** RS232 -> `3` -> `7` (Monitor an) -> `9` -> Hauptmenue stehen lassen -> am PC verbinden/trennen.

---

## T0 - Vorbereitung

| | |
|---|---|
| **Aktion** | 5.114 flashen. Power-Cycle (komplett stromlos). RS232 verbinden. `Hauptmenue -> 6`: Version pruefen. |
| **Erwartung** | `Version 5.114`, `Schnittstellen: Bluetooth, USB` (kein LTE auf diesem Board). |
| **Ergebnis** | |

## T1 - Idle-Baseline (nicht verbunden)

| | |
|---|---|
| **Aktion** | `Testmenue (3) -> 7` (Monitor an), `9` zurueck ins Hauptmenue, **30 s warten**, nichts verbinden. Dann wieder `3 -> 7` (Monitor aus) und den `Monitor beendet:`-Block ansehen. |
| **Erwartung** | Ein Block `Zustand [+... ms]:` mit `CYSPP bit7: HIGH`, `CONNECTION P0.19: HIGH`, `Bewertung: beide HIGH -> idle, nicht verbunden (konsistent)`, `Bit7-Richtung: Eingang`, `bt_cyspp_out: 0`. **Danach kein weiterer Block**, am Ende `comchange_count` Delta 0. |
| **Ergebnis** | |
| **Bei Abweichung** | P0.19 liest LOW oder **flackert** (mehrere Bloecke ohne Aktion): Leitung schwebt/Stoerung -> externen Pull-up 10-18 kOhm nach 3V3 an die Leitung loeten, T1 wiederholen. P0.19 dauerhaft LOW: Kurzschluss/Verdrahtung/Polaritaet pruefen. |

## T2 - Classic verbinden, Monitor offen

| | |
|---|---|
| **Aktion** | Monitor im Hintergrund an (siehe oben), RS232 im Hauptmenue. Am PC `COM11` oeffnen (Verbinden). |
| **Erwartung** | Neuer Block (am RS232): beide **LOW**, `Bewertung: beide LOW -> verbunden, Datenpipe steht (konsistent)`. Banner + `PIN = ` erscheinen auf **beiden** Terminals. *(Vorab bestaetigt am 2026-10-14 mit dem alten Monitor und bereits stehender Verbindung: beide LOW, konsistent.)* |
| **Ergebnis** | |
| **Bei Abweichung** | CONNECTION bleibt HIGH (CYSPP LOW): Weiche verwirft die Eingabe -> T3 nicht moeglich. Pin/Polaritaet pruefen; zum Weitermachen `BT_CONN_GPIO 0` (dann ohne die 5.110-Weiche). Beide HIGH trotz Verbindung: BT nicht wirklich verbunden. |

## T3 - PIN und normaler Betrieb ueber BT

| | |
|---|---|
| **Aktion** | Im BT-Terminal (COM11) die PIN eingeben, ins Hauptmenue, 3-4 Menuepunkte durchklicken (`6`, `9`, ...). **Testmenue 7 hier nicht auf BT benutzen** (schaltet den Monitor aus). Monitor laeuft im Hintergrund weiter. |
| **Erwartung** | PIN wird akzeptiert, Hauptmenue erscheint, Eingaben reagieren sofort. Waehrend der Sitzung: beide LOW, **kein Flackern**, keine weiteren Bloecke am RS232. |
| **Ergebnis** | |
| **Bei Abweichung** | PIN wird nicht akzeptiert / Eingaben fehlen: CONNECTION war LOW nur kurz? Dann Monitor-Block + Zeit notieren. |

## T4 - Sauber trennen bei offenem RS232 (Test des 5.110-Fixes)

| | |
|---|---|
| **Aktion** | Monitor laeuft im Hintergrund. Im Terminalprogramm sauber **Trennen**. RS232 offen im **Hauptmenue** lassen, **nichts tippen**. 3x wiederholen (jeweils neu verbinden, PIN, trennen). Monitor-Laufzeit max. 5 min - bei Bedarf `3 -> 7` neu starten. |
| **Erwartung** | Monitor-Block: beide wieder **HIGH**. Auf RS232 erscheinen **keine** Zeichen wie `0`/`1`, das Menue springt **nicht** von selbst. Es entsteht kein "PIN = ****** ..."-Dauerfeuer. |
| **Ergebnis (Lauf 1/2/3)** | |
| **Bei Abweichung** | Zeichen auf RS232: das Modul sendet das `BTDIS`-Event **vor** dem Loeschen der Pipe (CONNECTION noch LOW). Genau aufschreiben, welche Zeichen und wann (RS232-Mitschnitt sichern). |

## T5 - Direkt wieder verbinden

| | |
|---|---|
| **Aktion** | Unmittelbar nach dem Trennen (T4) erneut verbinden, 3 Zyklen. |
| **Erwartung** | Jedes Mal beide LOW, PIN-Prompt **einmal**, PIN wird akzeptiert. |
| **Ergebnis** | |

## T6 - Unsauber trennen (Split-Brain-Fall)

| | |
|---|---|
| **Aktion** | Verbinden, PIN. Dann **hart trennen**: (a) Terminalfenster/COM-Port einfach schliessen, (b) in einem zweiten Lauf Bluetooth am PC ausschalten. Nach je 10 s Monitor pruefen. |
| **Erwartung / Fragestellung** | Gehen beide Bits wieder HIGH? Oder bleibt CYSPP LOW haengen und **CONNECTION weicht ab**? (Das war bisher der offene Punkt: bei IO5 verhielt sich CONNECTION immer wie CYSPP.) RS232 darf nicht langsam werden. |
| **Ergebnis (a)** | |
| **Ergebnis (b)** | |
| **Falls haengend** | `Testmenue 8 -> 5` (Force-Disconnect) ausfuehren, `Vorher:`/`Nachher:`-Bloecke sichern. |

## T7 - Leerlauf in einer Sitzung

| | |
|---|---|
| **Aktion** | Verbinden, PIN, Hauptmenue. **60 s nichts tun**, dann einen Menuepunkt klicken. |
| **Erwartung** | Reaktion sofort. Monitor: beide LOW, unveraendert. |
| **Ergebnis** | |

## T8 - BLE (optional, wenn BLE-Tool bereit)

| | |
|---|---|
| **Aktion** | Mit `ble_mainboard_test.py --mode smoke` oder nRF Connect per BLE verbinden, **zuerst ohne Subscribe**, Monitor beobachten. Dann auf `...ca102` subscriben, PIN senden. |
| **Erwartung** | Vor Subscribe: vermutlich `CYSPP LOW / CONNECTION HIGH` (Datenpipe noch nicht komplett), nach Subscribe **beide LOW**, PIN kommt an. |
| **Ergebnis** | |
| **Bei Abweichung** | PIN kommt per BLE nicht an: Weiche verwirft (CONNECTION erst nach CCCD-Write LOW) - Zeitpunkt der beiden Pegelwechsel notieren. |

## T9 - Automatischer Lauf

| | |
|---|---|
| **Aktion** | `python ble_mainboard_test.py --transport classic --port COM11 --mode cycle --count 10 --hold 20` |
| **Erwartung** | 10/10 Zyklen bestanden (PIN/Menue ok). |
| **Ergebnis** | |

## T10 - Abschluss

| | |
|---|---|
| **Aktion** | Monitor einmal starten und beenden, `UART0-RX verworfen` ablesen. `Hauptmenue -> 6 -> 1` Protokoll ansehen. |
| **Erwartung** | `UART0-RX verworfen: 0`. Keine neuen Fehlereintraege durch die Tests. |
| **Ergebnis** | |

---

## Entscheidungshilfe nach dem Test

| Befund | Naechster Schritt |
|---|---|
| T1-T5 ok, T4 ohne Fehlzeichen | CONNECTION-Pin ist produktiv brauchbar; Stufe B (BT_LINK aus CONNECTION statt CYSPP) neu bewerten. |
| T4 Fehlzeichen | `BTDIS`-Event kommt vor Pipe-Ende: Weiche reicht nicht, zusaetzlichen Filter im Menue-Eingang erwaegen. |
| T6: CONNECTION weicht von CYSPP ab | CONNECTION ist die zuverlaessigere Statusquelle -> Stufe B lohnt sich (Split-Brain erkennbar). |
| T6: weiterhin identisch | Kein Zusatznutzen bei Split-Brain; Watchdog-Ansatz (host-seitig) bleibt der Weg. |
| T2 CONNECTION bleibt HIGH | Hardware/Polaritaet klaeren, bis dahin `BT_CONN_GPIO 0`. |
