# Board 26SC9993: IF820 startet beim Verbinden neu - Fehlerbild nachstellen und messen

Stand 2026-11-09. Für die Messung mit dem Oszilloskop. Befunde und Vergleichswerte: `sism555/project-status.md`,
Abschnitt "Board 26SC9993 (5.134, CONNECTION-Draht auf P0.19), 2026-11-03".

## Das Fehlerbild in Kürze

- Das Bluetooth-Modul IF820 läuft im Leerlauf stabil. Es wirbt (am PC sichtbar mit ca. -66 dBm), empfängt andere
  Geräte und beantwortet Befehle.
- **Sobald sich ein PC oder Handy verbindet, startet das Modul neu.** Bei BLE passiert das etwa alle 10 s, solange
  der PC es weiter versucht. Bei Classic kommt der Neustart ca. 2,6 s, nachdem Verbindung und Verschlüsselung stehen.
- Den Neustart sieht man an der Modulmeldung `@E,....,BOOT,...` auf der RS232-Schnittstelle des Geräts.
- Per Software geprüft und unauffällig, gleiche Werte wie das gesunde Board 99SC9923: UART (RX/TX),
  Flusskontrolle (CTS/RTS), CYSPP-Pegel, 32-kHz-Takt. Abschalten des Schlafmodus ändert nichts.
- Das Board hat den **CONNECTION-Umbau**: IF820 Pin 33 (`CONNECTION`/P27) ist mit einem Draht an den LPC-Pin
  **P0.19** geführt. Die Firmware liest P0.19 nur (Eingang mit Pull-up) und treibt den Pin nie.
- Verdacht, nicht gemessen:
  - (1) Fehler am CONNECTION-Draht: Das Modul zieht beim Verbinden auf LOW gegen etwas, das hart HIGH liegt, oder
    gegen einen Kurzschluss.
  - (2) Die Versorgung des Moduls bricht beim Senden im Verbindungsbetrieb ein. C128/C58 (je 1 µF an
    3P0V/VDDIO, Netz `N$169`) sind nicht bestückt.
  - (3) Ein anderes Bauteil am Modul, z. B. der 32-kHz-Quarz Q3, der nur unter Last aussetzt. Auf Board 26SC9983
    war früher der Quarz defekt.

## Was man braucht

- Gerät mit Firmware ab **5.129** (Testmenü 8 -> 7 "Modulbefehl senden"). Version steht in der Einschaltmeldung.
- RS232 des Geräts am PC, Terminalprogramm (z. B. Tera Term) mit **115200 Baud, 8N1**, ohne Flusskontrolle.
- Für die Verbindung eines von beiden:
  - Handy mit einer BLE-App, z. B. "nRF Connect"
  - Windows-PC mit Bluetooth
- Optional: Python 3 mit `pip install pyserial bleak` für das Skript `Doku/tools/if820_boot_check.py`.
- Gerätename: BLE `VIASIS_26SC9993_BLE`, Classic `VIASIS_26SC9993`. BLE-Adresse `E8:05:D6:F7:F8:C3`.
  App-PIN 269993 (wird hier nicht gebraucht, weil es gar nicht so weit kommt).

**Achtung:** Im Testmenü niemals `X` drücken, das setzt alle Parameter auf Werkszustand. Niemals Strg+W, das öffnet
das Werksmenü. Immer nur **eine** Versorgung anschließen (Board oder DVK-USB, nie beides): Rückspeisung hat schon
Module zerstört.

## A. Von Hand mit dem Terminal

1. Gerät einschalten und ca. 40 s warten, bis die Einschaltmeldung durch ist.
2. Eingeben: `H` (Hauptmenü), `3` (Testfunktionen), `8` (BT-Diagnose), `7` (Modulbefehl senden).
   Es erscheint `BT> `. Jetzt zeigt das Gerät alles, was das Modul meldet, mit Zeitstempel `[+...ms]` seit dem Start
   des Modus.
3. Lebenszeichen prüfen: `/PING` + Enter. Erwartet wird eine Antwort der Form
   `< [+...ms] @R,....,/PING,0000,R=...,F=...` (R = Sekunden seit Modulstart).
4. Optional weitere Abfragen, jeweils + Enter (nur lesen, ändern nichts):

   | Befehl | Zeigt | Erwartung |
   |---|---|---|
   | `GTU` | UART-Einstellung | `B=00070800` (460800 Baud), `F=01` (Flusskontrolle an) |
   | `/QFV` | Modul-Firmware | `E=01040C0C` (1.4.12.12) |
   | `.CYSPPGP` | CYSPP-Profil | `E=02` |
   | `GSLP` | Schlafmodus | |
   | `!` | Pinzustand CYSPP/CONNECTION am LPC | im Leerlauf beide HIGH |

5. **Jetzt verbinden:**
   - BLE: in nRF Connect nach `VIASIS_26SC9993_BLE` suchen und "Connect" drücken.
   - Classic: Board in Windows koppeln (Einstellungen -> Bluetooth -> Gerät hinzufügen). Dann den **ausgehenden**
     Bluetooth-COM-Port (Geräte-Manager -> Anschlüsse, bzw. "Weitere Bluetooth-Optionen" -> COM-Anschlüsse) mit einem
     zweiten Terminalfenster öffnen.
6. Im RS232-Fenster beobachten:
   - Gesund: `@E,....,C,...` (BLE verbunden) bzw. `@E,....,ENC,C=00,S=00` (Classic verschlüsselt), danach
     `[CYSPP] LOW`, kein BOOT.
   - 26SC9993: kurz nach `C`/`ENC` erscheint `@E,....,BOOT,...` = **Modul hat neu gestartet**. Bei BLE wiederholt
     sich das, solange die App es weiter versucht.
7. Beenden: **leere Zeile** (nur Enter) beendet den Modulbefehl-Modus, danach `H`.

**Zum Vergleich: Leerlauf.** Schritte 1-3, dann 40 s nichts tun. Auf 26SC9993 kam dabei **kein** BOOT. Das Modul
fällt also nur beim Verbinden aus.

## B. Mit dem Skript (wiederholt auslösen, gut fürs Oszi)

Im Ordner `sism555/Doku/tools`. Das Skript öffnet den Modulbefehl-Modus selbst, löst die Verbindungsversuche vom PC
aus, zeigt alle Modulmeldungen mit Zeit und zählt die Neustarts. Am Gerät ändert es nichts.

Leerlauf, 40 s:

```bash
python if820_boot_check.py --port COM3 --idle 40
```

BLE, 2 Versuche (Name oder Adresse):

```bash
python if820_boot_check.py --port COM3 --ble VIASIS_26SC9993
```

Classic (Board vorher in Windows koppeln, COM-Port anpassen; bei uns war es COM17):

```bash
python if820_boot_check.py --port COM3 --classic COM17
```

Dauerschleife zum Triggern am Oszi, 20 BLE-Versuche mit je 5 s Pause:

```bash
python if820_boot_check.py --port COM3 --ble E8:05:D6:F7:F8:C3 --runden 20 --pause 5
```

`--port` ist die RS232 des Geräts. Abbruch mit Strg+C, dann wird der Modus sauber beendet. Rückgabe 1, wenn
mindestens ein Neustart gesehen wurde.

## C. Was am Oszi messen

Trigger: fallende Flanke an **CONNECTION** (geht beim Verbindungsaufbau auf LOW, auf dem gesunden Board 99SC9923 schon
beim BLE-Connect). Oder fallende Flanke an **RST_L**, falls der Neustart über den Reset-Pin kommt.

| Kanal | Messpunkt | Was man sucht |
|---|---|---|
| 1 | **CONNECTION**: IF820 Pin 33 bzw. der Draht an P0.19 | Geht er sauber auf LOW (nahe 0 V)? Oder bleibt er hängen, z. B. bei halber Spannung = Treiberkonflikt? Fließt dabei Strom (Draht kurz auftrennen und Gegenprobe)? |
| 2 | **3P0V/VDDIO am Modul** (Netz `N$169`, z. B. an den leeren Pads von C128/C58) | Einbruch beim Verbindungsaufbau/Senden direkt vor dem Neustart |
| 3 | **RST_L** (IF820 Pin 34, erreichbar an R124 Pad 1; R124 ist nicht bestückt) | Geht RST kurz auf LOW? Dann kommt der Reset von außen bzw. von der Versorgung |
| 4 | optional CYSPP (IF820 -> IC54 Bit 7) oder BLT-TXD | Zeitbezug: CYSPP LOW = Datenkanal offen. Auf BLT-TXD erscheint der BOOT-Text nach dem Neustart |

Hinweise:
- Den 32-kHz-Quarz Q3 nur mit 10:1-Tastkopf antasten. Schon die Tastkopfkapazität kann ihn verstimmen oder
  anhalten.
- Einfache Gegenprobe ohne Oszi: **CONNECTION-Draht an P0.19 ablöten** und erneut verbinden. Ist der Neustart dann
  weg, liegt es am Draht oder an seiner Gegenstelle. Die Firmware braucht den Draht nicht (seit 5.120 nur Anzeige).
- Widerstand von CONNECTION gegen GND und gegen 3 V im ausgeschalteten Zustand messen und mit 99SC9923 vergleichen.

## Ergebnis zurückmelden

Bitte notieren: welcher Kanal was zeigte (Bild), ob der Neustart mit abgelötetem Draht weg ist, und die Ausgabe von
Skript oder Terminal. Ergänzung dann in `sism555/project-status.md` beim Abschnitt zu 26SC9993.
