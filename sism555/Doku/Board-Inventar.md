# Board-Inventar

> Zweck: schneller Überblick "was weiß ich über DIESES Board", bevor ein neuer Test beginnt.
> Vor jedem Test kurz nachschauen, nach jedem Test die Zeile aktualisieren/ergänzen.
> Ausführliche Historie bleibt in `project-status.md` - hier nur der aktuelle Kurzstatus.
> Stand 2026-11-10.

## Umrüstaktion November 2026 (nur R125 nach GND)

Ab 2026-11-10 werden **alle Boards** umgerüstet (Anwenderentscheidung). Grund und Nachweis: `project-status.md`,
"Nachweis: R125 als Pull-down behebt BLE waehrend LTE" und "Gegenprobe mit alten Firmwarestaenden".

| Umbau | Vorgabe |
|---|---|
| **R125 (Modul-CTS, IF820 Pin 14)** | 10 kOhm nach **GND** statt nach N$184 (Pull-down). Ein bisher offenes R125 bekommt ebenfalls 10 kOhm nach GND. |
| ~~Pull-up BLT-RXD~~ | **Zurückgestellt** (Anwenderentscheidung 2026-11-10): vorerst **nicht** umbauen. Spezifikation für später (47 kOhm nach N$184) in `Fragen_Schaltplan_CTS_MUX.md`. Folge: die `0xFF`-Bytes vor dem Banner bleiben. |

**Warum der Ausgangszustand wichtig ist:** Die Boards waren unterschiedlich bestückt. Alle Testergebnisse **vor** dem
Umbau gelten nur für den damaligen R125-Zustand:
- **R125 Pull-up** (nach 3V): BLE hängt, sobald der LPC den Modul-CTS nicht treibt - während LTE, bei abgeschaltetem
  UART1 im Leerlauf (bis 5.126), nach Reset ohne BT-UART-Start (bis 5.127).
- **R125 offen:** Modul-CTS schwebt; funktionierte im Versuch (26SC9966), Pegel aber undefiniert.
- **R125 nach GND** (Umbau): definiert "bereit", alle obigen Fälle behoben (Nachweis auf B).

Beim Umbau bitte je Board eintragen, wie R125 **vorgefunden** wurde und wann umgebaut wurde. Nach dem Umbau mindestens
`online-bt` (BLE während LTE) und ein kurzer `interleave` als Abnahme.

| Seriennummer | R125 vorgefunden | Umbau R125 -> GND | Abnahme nach Umbau |
|---|---|---|---|
| 26SC9983 (B) | **Pull-up** (3V) | 2026-11-10 | `online-bt` BLE 3/3, Classic 3/3; 5.126/5.127-Gegenprobe bestanden; BLE während GSM-Thread 3/3, power-sweep 18/18, boot-race 12/12; module-suite: Zyklen 12/12 je Transport, Classic-Downloads bestanden, **BLE-Download bei 57 % abgebrochen, danach Modul stumm**; nach Stromlos-Zeit Wiederholung BLE-Download 3/3 bestanden (Einzelfall) (2026-11-10). **Dauerlauf 12 h (2026-11-12/13):** Downloads 44/44 (0 Retries), interleave 378/380, boot-race 19/19, BT waehrend online 13/13 (ab 20:12 MQTT-Server trennt sofort, nicht weiter pruefbar) |
| 26SC9966 | **offen** (nicht bestückt) | 2026-11-10 | BLE während GSM-Thread 3/3, `interleave` BLE 3/3 + Classic 3/3, boot-race ok; nach SIM-Tausch `online-bt` BLE 4/4, Classic 3/3 (2026-11-10) |
| 99SC9923 (A) | **offen** (nicht bestückt, Anwender) | vor 2026-11-12 | 5.137: BLE-smoke (I=48 -> `/UCP` -> 15 ms) und Classic-smoke bestanden, `interleave` 40/40 (BLE 20/20, Classic 20/20), boot-race 12/12; kein Modem, daher kein `online-bt` (2026-11-12) |
| 26SC9994 (C) | **Pull-up** 10k (Anwender) | vor 2026-11-12 | **BLE scheitert: Modul friert bei jedem BLE-Verbindungsaufbau komplett ein** (kein `/PING` mehr, wirbt nicht, nur Stromzyklus hilft), mit 5.135, 5.137 und 5.137 ohne `/UCP`. Classic-smoke ok, Classic cycle 12/12. Vor dem Umbau (5.134, 2026-11-04) lief BLE. Siehe project-status.md (2026-11-12) |
| 26SC9993 | **offen** (nicht bestückt, Anwender) | vor 2026-11-12 | **Abnahme abgebrochen:** Board läuft nicht hoch (RS232 nur 1 Müllbyte, kein BLE-Werben) und **pfeift hörbar** - Versorgungsfehler vermutet (2026-11-12) |
| 11SC1234 | **Pull-up** 10k (Anwender) | vor 2026-11-12 | 5.137: **BLE friert das Modul beim Verbinden ein** (wie 26SC9994, hier schon vor dem Umbau am 05.11.); auch mit Modul-Schlaf aus (`SSLP,L=00`). Classic-smoke ok, Classic cycle 6/6. Moduleinstellungen identisch mit funktionierenden Boards. Mit gelöteter Brücke (Anwender) weiter BLE 0/2, Classic ok. Nach Bauteiltausch (Anwender, 15 Uhr) weiter BLE 0/2, Modul danach wieder eingefroren (wirbt nicht mehr). **Danach defekt: Board pfeift hörbar** (wie 26SC9993); Hardware-Prüfung durch die HW-Experten am Montag 2026-11-16 (2026-11-12) |
| 12SC3456 | ? | offen | - |

## Boards

| Seriennummer | Kurzname | HW-Besonderheiten | Zuletzt geflasht | Bekannter Status | Zuletzt getestet |
|---|---|---|---|---|---|
| **26SC9983** | B | R125 nach GND umgebaut (vorher Pull-up), **kein** P0.19-Draht; früher Quarztausch am IF820 (+10 MΩ parallel) | 5.142 | Mit Pull-down `online-bt`: BLE während LTE **3/3**, Classic 3/3 (vorher mit Pull-up BLE 0/3). Stromzyklen bestanden. **module-suite 2026-11-10: Modul hing nach BLE-Download-Abbruch (CYSPP LOW, kein `/PING`), `BT_TIMEOUT` holte es nicht zurück.** Nach Stromlos-Zeit BLE-Download 3/3 bestanden. **Dauerlauf 12 h mit 5.137 bestanden** (Downloads 44/44, Download-Haenger nicht wieder aufgetreten). Modul-FW 1.4.12.12. **5.138 (2026-11-13):** BLE- und Classic-smoke bestanden, Lichtsensor VEML7700 und Via-2-Empfaenger (24175 MHz) erkannt. **5.142 (2026-11-13):** Via-2 Kanal 4 (automatisch), IC32 abgeglichen und nachgefuehrt (Stufe 122), BLE/Classic-smoke bestanden. | 2026-11-13 |
| **26SC9966** | - | R125 nach GND umgebaut 2026-11-10 (vorher nicht bestückt/offen), CONNECTION -> P0.19 verdrahtet, LTE + SIM | 5.137 | BLE und Classic während LTE in allen Läufen bestanden, 5.137-Auslöser mit `/leave`/`/join` bestanden; Robustheit (gsm-mux, boot-race 18/18, power-sweep 27/27, cycle 12/12) bestanden. Nach dem Umbau: `online-bt` BLE 4/4, Classic 3/3. | 2026-11-10 |
| **99SC9923** | A | CONNECTION -> P0.19 verdrahtet (früher über IC54-Pad IO5); R125 nach GND (vorher offen); kein Modem | 5.137 | Abnahme nach Umbau bestanden (BLE/Classic-smoke, interleave 40/40, boot-race 12/12). Früher: module-suite 5.135 bis zum Abbruch ohne Fehler (Classic im langen Lauf langsam, ca. 2,6 KB/s). | 2026-11-12 |
| **26SC9994** | C | HW V2; R125 nach GND umgebaut (vorher Pull-up 10k); kein P0.19-Draht; keine LTE-Antenne | 5.137 | **Nach Umbau: BLE friert das Modul beim Verbinden ein** (siehe Umrüsttabelle), Classic ok. Uhr stand auf 3856, neu gestellt. Vorher: umfassender Test 5.134 bestanden. | 2026-11-12 |
| **26SC9993** | - | ? | ? | IF820 startet bei jeder Verbindung neu (`BOOT`). Verdacht CONNECTION-Draht, nicht Versorgung. In Reparatur; Messanleitung `Anleitung_26SC9993_Neustart_beim_Verbinden.md`. | 2026-11-09 |
| **11SC1234** | - | früher als 12SC1234 bezeichnet; **Modem abgeschaltet** (Konfiguration Modem/Email = n); R125 nach GND (vorher Pull-up) | 5.137 | Classic geht, **BLE friert das Modul ein** (vor und nach dem Umbau). PC als Ursache ausgeschlossen (99SC9923 geht mit gleichen Parametern). Hardware-Ursache vermutet, gleiches Bild wie 26SC9994. Bauteiltausch half nicht; **seitdem defekt, Board pfeift hörbar**. Liegt bei den HW-Experten (Montag 2026-11-16), bis dahin nicht testen. | 2026-11-12 |
| **12SC3456** | - | Hand-Umbau IF820 Pin 33/CONNECTION -> IC54 Pad IO5 (GND-Bahn aufgetrennt, 18 kΩ Pull-up) | 5.103 | Alter Umbau, Spannungsversorgungs-Verdacht seit 2026-06. Nicht ohne Prüfung weiterverwenden. | 2026-09-29 |
| **269996** | - | - | - | **Hardwaredefekt, bestätigt** (nicht reparabel ohne Bauteil-Nacharbeit) | 2026-08 |

## Hinweise

- **P0.19-Draht:** nur nötig für den 5.137-Auslöser (CONNECTION). Ohne Draht liest P0.19 dauerhaft HIGH, der Auslöser
  bleibt stumm, sonst keine Wirkung (`BT_CONN_GPIO` = 0 seit 5.120). Alle Boards dürfen `connection-pin`-Firmware bekommen.
  Mit R125 nach GND wird der Auslöser für BLE während LTE nicht mehr gebraucht, bleibt aber als Absicherung.
- **26SC9983** hieß in `project-status.md` bis 2026-09-30 fälschlich "269983" (Tippfehler, durchgängig korrigiert).
- Bei neuen Boards: Zeile hier ergänzen, sobald der erste Test/Befund vorliegt, inkl. R125-Zustand.
