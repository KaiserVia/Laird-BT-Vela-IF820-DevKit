# Anfrage an die MCU-Entwicklung: IF820 BLE CYSPP Kommunikation

## Ziel

Die BLE-Verbindung zum IF820 wird aufgebaut, aber auf dem Zielsystem gelangt
CYSPP-Nutzdatenverkehr nicht zuverlässig zwischen BLE und dem Mikrocontroller
an. Dieses Dokument beschreibt eine auf dem IF820-DVK reproduzierte
Referenzkonfiguration und die Punkte, die im MCU-Design und in der MCU-Firmware
zu pruefen sind.

Die zentrale, mit dem DVK nachgewiesene Anforderung lautet:

> Die MCU-besessene CYSPP-Steuerleitung muss waehrend einer transparenten
> BLE-CYSPP-Verbindung als Eingang beziehungsweise hochohmig freigegeben sein.
> Sie darf nicht aktiv HIGH oder LOW getrieben werden.

Dies ist keine Aussage allein ueber den gemessenen Pegel. Ein aktiv HIGH
getriebener Pin und ein freigegebener Pin wurden beide als HIGH gemessen, hatten
aber unterschiedliches Verhalten.

## Referenzaufbau am DVK

```text
PC mit Python/Bleak  <--- BLE CYSPP --->  IF820 DVK
                                              |
                                              +-- USB-PUART zum PC
                                              |   EZ-Serial-Kommandos und UART-Nutzdaten
                                              |
                                              `-- DVK-Debug-Probe GPIO 19
                                                  simuliert die MCU-CYSPP-Leitung
```

Auf dem DVK entspricht die simulierte MCU-Leitung `GPIO 19`
(`If820Board.CYSPP`). Die USB-Verbindung liefert Strom sowie die PUART- und
Debug-Probe-Verbindung. Es war kein zweiter UART-Adapter, keine externe
Versorgung und kein externer Mikrocontroller angeschlossen.

## Reproduzierte IF820-Konfiguration

Die Firmwareversion wird bei jedem HIL-Lauf ueber den EZ-Serial-Befehl `/QFV`
direkt aus dem IF820 gelesen und im jeweiligen JSON- sowie Matrix-Messprotokoll
protokolliert. Die MCU-Diagnose muss dieselbe Abfrage im Testlog erfassen; eine
fest hinterlegte Versionsannahme ist nicht zulaessig.

Aus dem IF820 ausgelesene CYSPP-Parameter:

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

Standard-PUART im Referenzlauf: `115200 Baud`, `8N1`, ohne RTS/CTS. Die
optionale 1-MBaud-RTS/CTS-Variante hat auf dem DVK-USB-PUART zwar einen Ping
beantwortet, aber asynchrone EZ-Serial-Statusausgaben verfaelscht. Sie ist
deshalb keine Voraussetzung fuer die Basisfunktion und soll nicht als erster
Fehlerbehebungsversuch verwendet werden.

Die PC-App verwendet die CYSPP-Characteristic:

```text
65333333-a115-11e2-9e9a-0800200ca102
```

Sie aktiviert zuerst Notifications (CCCD `0x0100`), wartet auf die Verbindung
und prueft danach bidirektional UART-Nutzdaten.

## Verifizierte Pin-Zustaende

Die folgenden Faelle wurden mit identischen IF820-CYSPP-Parametern getestet.

| MCU-CYSPP-Zustand beim BLE-Subscribe | Gemessener Pegel | Beobachtung | Ergebnis |
| --- | ---: | --- | --- |
| aktiv LOW | LOW | Kein CCCD-Ereignis und keine transparente UART-Kommunikation | Fehler |
| aktiv HIGH | HIGH | BLE-Schreiben erscheint als EZ-Serial-GATT-Ereignis (`@E,...`), nicht als transparente UART-Nutzdaten | Fehler |
| Eingang / hochohmig | HIGH | CCCD-Ereignis, MCU-Prompt ueber BLE und PC-Nutzdaten transparent ueber UART | Pass |

Folgerung: Ein gemessener HIGH-Pegel ist nicht ausreichend. Entscheidend ist,
dass der MCU-Pin elektrisch freigegeben wird und der IF820 die Leitung in der
transparenten CYSPP-Betriebsart kontrollieren kann.

## Erwartete MCU-Initialisierung

1. IF820 mit der oben genannten CYSPP-Konfiguration booten beziehungsweise die
   im Produkt verwendete, dazu aequivalente persistente Konfiguration
   verifizieren.
2. Vor BLE-Advertising, vor jedem Connect und insbesondere vor dem Schreiben
   des CYSPP-CCCD den MCU-CYSPP-Pin als `Input`/hochohmig konfigurieren.
   Keinen internen Pull-up oder Pull-down aktivieren, solange dessen Einfluss
   nicht durch den Hardwareverantwortlichen bestaetigt wurde.
3. Den Pin waehrend der kompletten transparenten BLE-CYSPP-Verbindung nicht
   auf Output umschalten und nicht aktiv HIGH oder LOW treiben.
4. Nach einem BLE-Disconnect oder MCU-Reset den Pin wieder freigeben, bevor
   ein erneuter Connect zugelassen wird.
5. Erst nach erfolgreichem Subscribe und sichtbarem CCCD-Schreiben `0x0100`
   Nutzdatenverkehr erwarten beziehungsweise senden.
6. UART-Parameter zwischen MCU und IF820 identisch einstellen: zu Beginn
   `115200`, `8N1`, ohne RTS/CTS. Keine Konfiguration waehrend einer aktiven
   BLE-CYSPP-Verbindung wechseln.

## Relevante Hardware und Kabel

Fuer den DVK-Referenztest war nur das USB-Datenkabel zwischen PC und IF820-DVK
erforderlich. Im Produktaufbau sind folgende Signale gegen Schaltplan und
Bestueckungsvariante zu pruefen:

| Verbindung | Erwartung |
| --- | --- |
| IF820 Versorgung und Masse | Gemeinsame, stabile Versorgung; GND zwischen IF820 und MCU verbunden |
| MCU TX -> IF820 RX | Gekreuzte UART-Datenleitung, korrekte IO-Spannungsdomäne |
| MCU RX <- IF820 TX | Gekreuzte UART-Datenleitung, korrekte IO-Spannungsdomäne |
| MCU-CYSPP GPIO -> IF820 CYSPP-Steuerleitung | Richtige Netzzuordnung, nicht vertauscht, keine externe aktive Treiberstufe |
| RTS/CTS, falls bestueckt | Fuer die Basisreferenz deaktiviert oder elektrisch so definiert, dass kein UART-Blockieren entsteht |

Die konkrete IF820-Pinnummer und Netzbezeichnung muessen gegen den verwendeten
IF820-Schaltplan und das Produktlayout abgeglichen werden. Sie werden hier
absichtlich nicht geraten. Besonders pruefen: falsche Netzzuordnung,
Pull-Widerstand, Pegelwandler mit festem Ausgang, Bus-Contention sowie MCU-
Bootzustand, der den CYSPP-Pin kurzzeitig als Push-Pull-Ausgang treibt.

## Geforderte MCU-Diagnose

Bitte einen Test-Build bereitstellen oder die folgenden Punkte per Debugger/
Logic Analyzer nachweisen:

1. GPIO-Modus, Output-Latch und Pull-Konfiguration des CYSPP-Pins bei Reset,
   vor Advertising, bei BLE-Connect, beim CCCD-Subscribe und nach Disconnect.
2. Elektrischen Pegel auf der MCU- und IF820-Seite derselben Leitung messen.
   Unterschiedliche Pegel deuten auf Kurzschluss, falsche Zuordnung oder zwei
   gegeneinander arbeitende Treiber hin.
3. Rohes UART-Log auf beiden Seiten fuer den Ablauf: BLE connect, CCCD `0100`,
   IF820-UART-Prompt, PC-BLE-Nutzdaten, MCU-UART-Empfang.
4. Sicherstellen, dass kein MCU-Task nach dem Subscribe den Pin auf Output
   umschaltet oder die UART neu initialisiert.
5. Den Test einmal mit freigegebenem CYSPP-Pin und einmal bewusst aktiv HIGH
   wiederholen. Das erwartete Differenzverhalten ist oben dokumentiert und
   dient als direkte Falsifikation der Pin-Hypothese.

## Akzeptanztest

Der MCU-Aufbau gilt fuer die Basisfunktion als bestaetigt, wenn dieser Ablauf
ohne Reboot und ohne Disconnect gelingt:

1. PC verbindet sich mit dem IF820-BLE-Geraet.
2. PC aktiviert Notifications auf der CYSPP-Characteristic.
3. IF820 meldet das CCCD-Schreiben `0100` an der UART-Seite.
4. MCU sendet `MCU-SIM PIN?\r\n` ueber UART; die PC-App empfaengt genau diesen
   Text ueber BLE.
5. PC schreibt `APP-SIM HELLO\r\n` ohne Response; die MCU empfaengt exakt diese
   Bytes als transparente UART-Daten und nicht als EZ-Serial-Event-Text.

Erst nach diesem Basisnachweis sollte Durchsatz untersucht werden. Fuer groessere
Payloads sind End-to-End-Frames mit Sequenznummer, CRC, ACK und begrenztem
Fenster erforderlich, da BLE Write Without Response keine bestaetigte
End-to-End-UART-Zustellung garantiert.

## Nachweise

- DVK-Testimplementierung: `cyspp_dvk_hil.py`
- Matrix-Runner: `run_cyspp_dvk_matrix.py`
- Rohartefakte: `artifacts/cyspp-*.json`
- Dokumentierte Matrix: `artifacts/cyspp-matrix-20260813T100101Z.md`

Die Matrix zeigt transparenten Verkehr ausschliesslich im Zustand
`CYSPP pin=input`. Die beiden aktiv getriebenen Zustaende reproduzieren die
Fehlerbilder. Das ist ein starker Hinweis auf MCU-GPIO-/Verdrahtungs- oder
Initialisierungsfehler, aber noch kein alleiniger Beweis gegen die MCU-Firmware;
der oben geforderte Pin- und UART-Mitschnitt entscheidet die Ursache.
