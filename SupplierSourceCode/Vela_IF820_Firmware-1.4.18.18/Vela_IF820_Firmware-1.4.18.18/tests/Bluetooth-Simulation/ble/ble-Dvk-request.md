# Auftrag an die Mainboard-Entwicklung: IF820 BLE CYSPP Datenpfad

## Ziel und Fehlerbild

Auf dem Mainboard wird der IF820 als BLE-CYSPP-Peripheral gefunden und eine
GATT-Verbindung kann aufgebaut werden. Trotzdem kommen die Nutzdaten zwischen
BLE und der MCU nicht sicher an. Der Fehler liegt nicht im BLE-Scan, im
GATT-Service oder in der App: Der auf dem IF820-DVK reproduzierte Datenpfad
funktioniert, wenn die CYSPP-Steuerleitung des Hosts elektrisch freigegeben
ist.

Die verbindliche Anforderung fuer Mainboard-Hardware und MCU-Firmware lautet:

> Die vom Mainboard kontrollierte IF820-CYSPP-Leitung muss von Reset bis zum
> Ende jeder transparenten CYSPP-Verbindung als Eingang/hochohmig konfiguriert
> sein. Sie darf weder aktiv HIGH noch aktiv LOW getrieben werden.

Ein gemessener HIGH-Pegel ist kein Beweis fuer den korrekten Zustand. Im
Referenztest waren aktiv HIGH und hochohmig beide HIGH messbar, hatten aber
unterschiedliches Verhalten. Nur der hochohmige Zustand oeffnet den
transparenten Datenpfad.

## Nachgewiesene Referenz auf dem IF820-DVK

```text
PC mit Python/Bleak  <--- BLE CYSPP --->  IF820 DVK
                                              |
                                              +-- USB-PUART: EZ-Serial und UART-Nutzdaten
                                              |
                                              `-- Debug-Probe GPIO 19: simulierte MCU-CYSPP-Leitung
```

- IF820-EZ-Serial-Firmware der erfolgreichen Referenz: `V1.4.12.12`.
- PUART-Basisbetrieb: `115200`, `8N1`, ohne RTS/CTS.
- Daten-Characteristic: `65333333-a115-11e2-9e9a-0800200ca102`.
- Die App aktiviert darauf Notifications. Der relevante CYSPP-CCCD-Wert ist
  `0100`; ein zuvor auftretendes Service-Changed-Indication-CCCD `0200` ist
  nicht der CYSPP-Start und muss ignoriert werden.
- Erst nach dem CYSPP-CCCD `0100` wird transparent weitergeleitet.

Ausgelesene CYSPP-Parameter der Referenz:

| Parameter | Wert |
| --- | ---: |
| `enable` | `2` |
| `role` | `0` |
| `company` | `305` |
| `local_key` | `0` |
| `remote_key` | `0` |
| `remote_mask` | `0` |
| `sleep_level` | `1` |
| `server_security` | `0` |
| `client_flags` | `2` |

## Entscheidender DVK-Nachweis

Die Debug-Probe steuert auf dem DVK `GPIO 19` (`If820Board.CYSPP`). Alle drei
Faelle wurden mit derselben CYSPP-Konfiguration getestet:

| Zustand der Host-CYSPP-Leitung beim Subscribe | Beobachtung | Ergebnis |
| --- | --- | --- |
| aktiv LOW | Kein nutzbarer transparenter CYSPP-Datenpfad | Fehler |
| aktiv HIGH | BLE-Schreiben erscheinen als EZ-Serial-GATT-Ereignisse, nicht als UART-Nutzdaten | Fehler |
| Eingang / hochohmig | CCCD `0100`, IF820-UART zu BLE und BLE zu IF820 funktionieren transparent | Pass |

Die Datenblattbeschreibung des Pins als low-aktiv darf nicht zu der Annahme
fuehren, dass das Mainboard ihn aktiv LOW halten soll. Fuer den hier verwendeten
transparenten CYSPP-Betrieb ist der gemessene und reproduzierte Sollzustand
**hochohmig**. Der IF820 kontrolliert die Leitung selbst.

## Konkrete Mainboard-Umsetzung

Die betroffene Leitung ist auf dem bestehenden Mainboard:

```text
IF820 CYSPP <----> IC54 PCA9554, Bit 7 <----> MCU / I2C
```

Die Leitung wird aktuell verwendet, um den IF820 bei einer Umschaltung der
geteilten UART1 mit GSM/GPS zu blockieren. Dieses Vorgehen ist fuer den
transparenten CYSPP-BLE-Betrieb nicht zulaessig.

Die MCU-Firmware muss Folgendes umsetzen:

1. Beim Boot und bei jedem Reset-Grund IC54 Bit 7 sofort auf Eingang/
   freigegeben setzen. Der PCA9554 kann seinen vorherigen Zustand ueber einen
   MCU-Reflash behalten; ein Reflash allein reicht daher nicht als Recovery.
2. Vor BLE-Advertising, vor jedem Connect, beim CYSPP-CCCD-Subscribe und nach
   jedem Disconnect sicherstellen, dass Bit 7 nicht aktiv treibt.
3. Beim Uebergang der UART1 zwischen IF820 und GSM/GPS darf kein Pfad Bit 7
   auf Output HIGH oder LOW setzen. Das gilt auch fuer Fehler-, Timeout- und
   Abbruchpfade.
4. Falls ein Statusmerkmal benoetigt wird, muss es den tatsaechlichen
   Treiberzustand abbilden, zum Beispiel `cyspp_line_is_driven`. Beim Freigeben
   der UART1 durch GSM/GPS ist die CYSPP-Leitung unbedingt freizugeben.
5. Eine Recovery vorsehen: Wenn keine aktive IF820-CYSPP-Verbindung besteht,
   Bit 7 aber als Output konfiguriert ist, innerhalb einer kurzen, definierten
   Frist wieder freigeben. Diese Recovery muss auch ohne Reboot funktionieren.
6. Kein interner Pull-up/Pull-down und keine externe Treiberstufe auf der
   CYSPP-Leitung aktivieren, solange deren Einfluss elektrisch nicht bewertet
   wurde.

Empfohlene Zustandsregel:

```text
Boot / Reset / Advertising / BLE verbunden / Disconnect / UART1-Muxwechsel
    -> IC54 Bit 7 = Eingang / hochohmig
```

Die Leitung ist kein allgemeines UART-Enable-Signal. Datenrauschen bei einer
GSM/GPS-Umschaltung muss an der UART-Mux-/UART-Logik behandelt werden, nicht
ueber ein dauerhaftes Treiben von CYSPP.

## Hardware-Pruefung

Bitte den Schaltplan, die Bestueckungsvariante und das Layout gegen folgende
Punkte pruefen:

| Signal | Pruefung |
| --- | --- |
| IF820-CYSPP zu IC54 Bit 7 | Gleiche Netzbezeichnung an beiden Pins, keine Vertauschung mit Status- oder Altmodul-Signalen |
| IC54 Bit 7 | Richtung, Output-Latch und Pull-Zustand nach Power-on, MCU-Reset und Firmware-Update |
| Pegelwandler / Schutzbeschaltung | Kein Push-Pull-Ausgang, kein fester Pull und keine Bus-Contention auf CYSPP |
| MCU TX -> IF820 RX | Gekreuzt, korrekte IO-Spannungsdomäne |
| MCU RX <- IF820 TX | Gekreuzt, korrekte IO-Spannungsdomäne |
| Versorgung und GND | Gemeinsame stabile Masse und Versorgung zwischen MCU und IF820 |
| RTS/CTS | Fuer den Basisnachweis deaktiviert oder eindeutig elektrisch definiert; kein Blockieren der PUART |

Die konkrete IF820-Pinnummer und Netzbezeichnung sind aus dem verwendeten
IF820-Schaltplan zu uebernehmen, nicht zu raten.

## UART-Anforderungen

1. Zum Basisnachweis `115200`, `8N1`, ohne RTS/CTS auf beiden Seiten nutzen.
2. Die UART-Parameter nicht waehrend einer aktiven BLE-CYSPP-Verbindung aendern.
3. Bei groesseren Datenmengen darf die MCU die IF820-PUART nicht als Burst ohne
   Flusskontrolle ueberfahren. Daten in handhabbaren Bloecken senden oder
   Hardware-RTS/CTS bewusst implementieren und testen.
4. Erst BLE-Subscribe abschliessen und den CYSPP-Datenpfad bestaetigen, dann
   Nutzdaten ueber die UART senden.

## App- und Protokollreihenfolge

Die Mainboard-Firmware soll diese Reihenfolge im UART-Mitschnitt erkennen
koennen:

1. App verbindet sich mit dem IF820.
2. App aktiviert Notifications auf `...ca102`.
3. IF820 meldet den CYSPP-CCCD-Write mit `0100`.
4. Erst jetzt ist transparentes UART <-> BLE aktiv.
5. Das Mainboard kann nun eine Meldung ueber UART senden; diese muss als BLE
   Notification bei der App ankommen.
6. Die App schreibt Nutzdaten ohne Response; die MCU muss exakt diese Bytes an
   der UART erhalten, nicht ein EZ-Serial-Event wie `@E,...`.

Die Characteristic `...ca102` ist bidirektional: Client schreibt mit
Write-Without-Response zum Geraet, und der IF820 sendet mit Notifications zur
App. Nicht parallel die Data-Characteristic `...ca101` verwenden. Die
Characteristic `...ca103` ist optionaler Flow-Control-Hinweis fuer groessere
Transfers.

## Geforderter Nachweis vom Mainboard-Team

Bitte einen Test-Build oder einen Logic-Analyzer-/Debugger-Mitschnitt liefern,
der die folgenden Punkte fuer einen kompletten BLE-Zyklus zeigt:

1. IC54 Bit 7: Richtung, Output-Latch und Pegel bei Power-on, MCU-Reset,
   Advertising, BLE-Connect, CYSPP-Subscribe, UART1-Muxwechsel und Disconnect.
2. Pegel direkt an MCU/IC54 und direkt am IF820-CYSPP-Pin. Abweichende Pegel
   weisen auf falsches Routing, einen Pull, Kurzschluss oder zwei Treiber hin.
3. UART-Rohdaten fuer Connect, CYSPP-CCCD `0100`, Mainboard-UART-Meldung,
   BLE-App-Empfang, App-Schreiben und MCU-UART-Empfang.
4. Einen Negativtest, bei dem Bit 7 bewusst aktiv HIGH getrieben wird. Dann
   muss der transparente Datenpfad ausfallen. Anschliessend Bit 7 freigeben
   und ohne Power-cycle erfolgreich erneut verbinden.
5. Einen Regressionstest mit einem GSM/GPS-UART1-Muxwechsel vor und waehrend
   eines BLE-Verbindungszyklus. Danach muss Bit 7 weiterhin hochohmig und ein
   neuer CYSPP-Subscribe erfolgreich sein.

## Abnahmekriterium

Der Fehler gilt als behoben, wenn der folgende Ablauf auf dem Mainboard ohne
IF820- oder MCU-Reboot mindestens zehnmal hintereinander erfolgreich ist:

1. BLE-Scan und Connect.
2. Notifications auf `...ca102` aktivieren.
3. IF820 meldet an der UART den CYSPP-CCCD-Wert `0100`.
4. MCU sendet `MCU-SIM PIN?\r\n`; die App empfaengt exakt diese Bytes.
5. App sendet `APP-SIM HELLO\r\n`; die MCU empfaengt exakt diese Bytes als
   transparente UART-Nutzdaten.
6. BLE trennen, optional GSM/GPS-UART1-Muxwechsel ausfuehren und erneut bei
   Schritt 1 beginnen.

Erst nach diesem Basisnachweis Durchsatz testen. Fuer Transfers sind
Sequenznummern, CRC, ACKs und ein begrenztes Sendefenster erforderlich, da
BLE Write-Without-Response keine Ende-zu-Ende-Zustellung garantiert.

## Referenzen im Repository

- DVK-HIL-Test: `tests/Bluetooth-Simulation/ble/cyspp_dvk_hil.py`
- BLE-Smoke-Test: `tests/Bluetooth-Simulation/ble/ble_smoke_test.py`
- BLE-Transfer-Test: `tests/Bluetooth-Simulation/ble/ble_transfer_test.py`
- Vorherige Analyse und GATT-Details: `tests/Bluetooth-Simulation/ble/ble-chip-response.md`
- IF820-EZ-Serial Dokumentation: `Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf`
