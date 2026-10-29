# Plan: BLE-Problem mit dem nRF52840 auf die Spur kommen (Stand 2026-10-29)

## Ziel
Herausfinden, **wer die BLE-Verbindung beendet und warum**, und ob Windows, das Modul (IF820) oder die Firmware schuld ist. Bisher sehen wir nur die Folgen (PC-Log, RS232), nicht die Funkstrecke. Der nRF52840 soll die Funkstrecke sichtbar machen und einen zweiten, vom Windows-Stack unabhängigen BLE-Partner liefern.

Fragen, die der Plan beantworten soll:
1. Fehlerbild A: "verbunden, Subscribe ok, kein PIN-Prompt, Gerät trennt nach ca. 5,5 s": Wer sendet das Trennen, mit welchem Grund? Kommt der CCCD-Write am Modul an und wird er beantwortet?
2. Fehlerbild B: BLE-Link reißt mitten im Download (22.10., nach ca. 5 min): Supervision-Timeout (Funk) oder gewolltes Trennen?
3. Board B (26SC9983): Warum kommt nie ein `C`-Event (die Firmware merkt die Verbindung nicht)? Verbindet Windows überhaupt mit dem richtigen Gerät?
4. Durchsatz: Warum 60 ms Intervall bei B, 7,5 ms bei A? Wer bestimmt es, und nimmt Windows eine Änderung an?

## Was du hast
- Board 26SC9983 (B, BLE-Adresse `FF:4F:7F:9E:EE:BA`) und Board 99SC9923 (A, BLE `CD:49:1B:93:00:5F`, Classic `9C:97:1B:E9:FA:3D`).
- PC (Windows 11, Intel Wireless Bluetooth), Testskript `Doku/tools/ble_mainboard_test.py`.
- nRF52840 (Dongle oder DK; Variante bitte beim Start notieren), nRF Connect for Desktop installiert, Bluetooth Test Platform Pack 1.14.0 (MSI) vorhanden. Wozu das Pack gedacht ist, ist noch zu klären; es ist **nicht** Teil des Kernplans.

## Firmware-Stand für die Aufnahmen
Beide Boards auf **5.126** (Versionszeile `Viasis 3003M - Version 5.126` im RS232-Mitschnitt prüfen). Nichts während der Messreihe umflashen, sonst sind die Aufnahmen nicht vergleichbar. Die Gegenprobe mit 5.118 auf Board A ist ein eigener, späterer Schritt (Phase 6).

## Phase 0: Vorbereitung (einmalig, ca. 1 h)
1. In nRF Connect for Desktop die App **Programmer** öffnen und auf den nRF52840 die **nRF Sniffer for Bluetooth LE**-Firmware flashen (Hex wird von Nordic mitgeliefert: Programmer, "Nordic nRF Sniffer", je nach Hardware Dongle oder DK).
2. **Wireshark** installieren und das **nRF Sniffer-Plugin** (extcap) einrichten (Anleitung von Nordic: Python-Abhängigkeiten, Plugin-Ordner in Wireshark; Profil "nRF Sniffer" verwenden).
3. Funktionstest: Sniffer startet in Wireshark, die Schnittstelle "nRF Sniffer for Bluetooth LE" erscheint, in der Geräteliste tauchen `VIASIS_99SC9923_BLE` und `VIASIS_26SC9983_BLE` auf (sie werben dauerhaft).
4. Zeitabgleich: Die Wireshark-Zeitstempel und `trace.ndjson` nutzen beide die PC-Uhr. Beim Start einer Aufnahme die Uhrzeit notieren.
5. Ordner für Aufnahmen: `Doku/tools/Testreports/sniffer/<Datum>_<Board>_<Fall>/` (pcapng + kurze Notiz). pcapng nicht ins Git, wenn groß; Verweis in `project-status.md`.

## Automatischer Mitschnitt im Testskript (`--sniff`, ab 2026-10-29)
Statt Wireshark von Hand zu bedienen, startet `ble_mainboard_test.py` den Sniffer selbst und wertet ihn aus. Es braucht keinen Wireshark-Capture, nur den bereits geflashten nRF Sniffer (nrfutil `ble-sniffer`, Port wird automatisch gefunden) und `tshark` (liegt bei Wireshark) zum Auswerten.

```
python ble_mainboard_test.py --mode smoke --name VIASIS_99SC9923 --no-identify --rs232-port COM3 --sniff
python ble_mainboard_test.py --mode module-suite --name VIASIS_99SC9923 --sniff
```
- **Einzeltests** (smoke, cycle, download, receive ...): ein Mitschnitt für den ganzen Lauf. **Suiten** (module-suite, fast-suite, interleave, soak): je BLE-Phase ein eigener Mitschnitt. Classic-Phasen werden nicht mitgeschnitten (kann der Sniffer nicht).
- Der Sniffer startet, sobald die Zieladresse bekannt ist (aus `--address`, `--sniff-follow` oder dem Scan), und wartet 4 s auf den Sync. Wird er kürzer vor dem Verbinden gestartet, verpasst er das `CONNECT_IND`.
- Ergebnis im Report-Ordner: `sniffer_<label>.pcapng` (nur Zielgerät, öffnet in Wireshark) und `sniffer_<label>.json`; dazu im Trace das Ereignis `sniffer_summary`, im Phasenbericht der Eintrag `sniffer` und eine Befundzeile im Log.
- Befund enthält: Verbindungsparameter (`CONNECT_IND`), Parameter-Updates, alle CCCD-Writes (Notify/Indicate) mit Antwortzeit, Zahl der Notifications/Datenpakete/CRC-Fehler, `LL_TERMINATE_IND` mit Sender und Grund, RSSI.
- Weitere Optionen: `--sniff-port COMx`, `--sniff-follow <Adresse>`, `--sniff-keep-raw` (ungefilterter Mitschnitt aller Geräte, groß). Ohne `--sniff` ändert sich nichts. Fehlt Sniffer oder tshark, läuft der Test ohne Mitschnitt weiter.
- **Voraussetzung:** Wireshark/nRF Connect dürfen den Sniffer-Port nicht gleichzeitig belegen. Der Sniffer-Dongle steht in Reichweite der Boards (am besten zwischen PC und Board).

**Bekannte Grenze (erster Test 2026-10-29, Board A):** Der Sniffer erfasst das `CONNECT_IND`, die Service-Discovery und den CCCD-Write, **verliert aber nach 0,2 bis 2 s den Sync**, nachdem der PC `LL_CONNECTION_PARAM_REQ`/`LL_CONNECTION_UPDATE_IND` sendet (Wechsel 7,5 ms auf 15 ms, danach Anforderung 7,5 ms mit Timeout 1000 ms). Das spätere Trennen nach ca. 5,5 s sieht er deshalb bisher nicht. Der Befund meldet das als "Sync verloren". Gegenmaßnahmen siehe Ergebnisse der Phasen 1 und 2 (näher an die Boards, Intervall-Update beobachten).

## Phase 1: Baseline mit funktionierendem Lauf (Board A)
- Sniffer auf die Werbe-Adresse von A einstellen ("Follow"), dann `python ble_mainboard_test.py --mode smoke --name VIASIS_99SC9923 --no-identify --rs232-port COM3` laufen lassen (bestandener Lauf).
- Im Mitschnitt notieren: `CONNECT_IND` (Intervall, Slave-Latenz, Timeout), Intervall-Updates (`LL_CONNECTION_UPDATE_IND`, ggf. auf 15 ms wie im `CU`-Event), ATT-Ablauf (Service-Discovery, **Write auf CCCD `...ca102`**, Write Response), erste Notifications (Banner, `PIN = `), Ende (`LL_TERMINATE_IND`, Richtung und Reason).
- Ergebnis: Referenz-Ablauf, mit dem alle Fehlerfälle verglichen werden.

## Phase 2: Fehlerbild A einfangen (A und B)
- Gleiche Aufnahme wie in Phase 1, aber so lange wiederholen, bis der Fehler auftritt (smoke mehrfach, `--mode cycle --transport ble --count 6`). Jeder Lauf einzeln speichern, Fehlerläufe markieren (Uhrzeit, Reportordner).
- Auswertung im Fehlerlauf:
  - Kommt der **CCCD-Write** am Modul an und gibt es eine **ATT Write Response**? Wenn nein: Problem vor dem Modul (Windows/Funk).
  - Sendet das Modul **Notifications** (Banner)? Wenn ja und der PC zeigt sie nicht: Windows-Seite. Wenn nein: Modul/Firmware hat den Datenkanal nicht geöffnet (CYSPP).
  - Wer sendet `LL_TERMINATE_IND` (Central oder Peripheral) und mit welchem **Reason** (0x08 Supervision-Timeout, 0x13 Gegenstelle beendet, 0x16 lokal, 0x3D MIC-Fehler)? Das entscheidet "Windows trennt" gegen "Modul trennt" gegen "Funk".
  - **Funkqualität:** viele Wiederholungen/CRC-Fehler vor dem Abbruch (Störung) oder sauberer Abbruch?
- Parallel RS232 mitschneiden (macht das Testskript), damit man sieht, ob die Firmware ein `C`-Event bekam.

## Phase 3: Board B (kein `C`-Event)
- Sniffer auf B: Welche **Adresse** wirbt B tatsächlich (statisch/zufällig, ändert sie sich nach Reset)? Vergleich mit der Adresse, die Windows und das Testskript benutzen (`FF:4F:7F:9E:EE:BA`). Verdacht: Windows hat eine alte Adresse oder Kopplung zwischengespeichert.
- Im Verbindungsaufbau: Kommt die Verbindung bis zur Firmware, oder bricht sie schon in der Service-Discovery ab?
- Vergleich mit Board A (Phase 1). Zusätzlich RS232 prüfen: Kommt trotz stehender Verbindung kein `[BT-Event] ... C`?
- Danach (falls noch nötig): Classic-Kopplung von B in Windows entfernen, Bluetooth aus/ein, Phase 3 wiederholen.

## Phase 4: Zweiter Central statt Windows
- Dongle auf die **nRF Connect-Bluetooth-Low-Energy-Konnektivitätsfirmware** (App "Bluetooth Low Energy" in nRF Connect for Desktop) umflashen. Mit nur einem Dongle geht Sniffen **oder** Zentral-Spielen, nicht beides gleichzeitig.
- Verbinden mit A und B direkt aus der App: Services ansehen, CCCD von `65333333-a115-11e2-9e9a-0800200ca102` auf Notify setzen, Banner/PIN-Prompt beobachten, die PIN schreiben (Seriennummer ohne die Buchstaben `SC`, z. B. 26SC9983 = `269983`, 99SC9923 = `999923`) (Write without response).
- Ergebnis: Funktioniert es mit dem nRF-Central stabil, ist **Windows** der Auslöser. Scheitert es auch dort, sind Modul/Firmware/Board verdächtig.
- Intervall: In der App Verbindungsparameter (7,5 ms) anfordern und prüfen, ob das Modul sie annimmt und der Durchsatz steigt.

## Phase 5: Durchsatz (Intervall 60 gegen 7,5 ms)
- Aufnahme eines Downloads (`--mode receive --transport ble`) mit Sniffer: Intervall im Verlauf, Pakete je Verbindungsereignis, wann das Modul per CTS bremst. Ziel: belegen, dass es am vom PC gewählten Intervall liegt, und welcher Wert bei A/B gewählt wird.

## Phase 6: Firmware-Gegenprobe
- Erst wenn Phasen 1 bis 4 Hinweise geben: 5.118 auf Board A flashen (`sism555/fw_5.118/sism.hex`) und dieselbe module-suite (ca. 75 min) laufen lassen. Gleiches mit B ist wegen der CONNECTION-Weiche der 5.118 nicht möglich (nur 5.120 oder neuer).

## Entscheidungshilfe
| Befund im Mitschnitt | Bedeutung | Nächster Schritt |
|---|---|---|
| CCCD-Write kommt an, Modul sendet keine Notifications | Modul/Firmware öffnet den Datenkanal nicht (CYSPP-Pin) | Firmware/CYSPP-Pfad prüfen, Event-Sniff am Gerät |
| Notifications kommen, Windows zeigt sie nicht | Windows-Stack | PC-Seite (Treiber, Kopplung), nRF-Central dagegenhalten |
| Terminate mit Reason 0x08 nach vielen Wiederholungen/CRC-Fehlern | Funk/Störung | Antennenlage, Entfernung, Störquellen (WLAN-Kanal, USB3) |
| Terminate durch Peripheral (Modul) ohne Funkprobleme | Modul beendet gewollt | Modul-Konfiguration, Firmware (UART-Fluss, CTS) |
| Terminate durch Central (PC) | Windows beendet | Windows-Treiber/GATT-Zeitüberschreitung |
| B wirbt mit anderer Adresse als im Test | Adress-/Cache-Problem | Windows-Gerät entfernen, Adresse im Test prüfen |
| nRF-Central funktioniert dauerhaft stabil | Windows ist der Auslöser | Windows-Workaround, Test mit zweitem PC/Adapter |

## Mitbringen, wenn du zurückkommst
- Die pcapng-Dateien (oder Screenshots der Wireshark-Ansicht mit Terminate-Reason) für **mindestens je einen guten und einen schlechten Lauf**, zu Board A und zu Board B.
- Reportordner der gleichzeitig laufenden Tests (`Testreports/...`) und die Uhrzeit.
- Welche Hardware genau der nRF52840 ist (Dongle `PCA10059` oder Dev-Kit) und welche Version der Sniffer-Firmware läuft.
- Antwort auf: Hat der Sniffer beide Boards im Scan gesehen?

## Regeln
- Pro Board **immer nur eine Versorgungsquelle** (Netzteil **oder** USB-Programmierung, nie beides).
- Nichts an der Firmware ändern, solange die Messreihe läuft.
- Während einer Aufnahme PC nicht sperren (Wachhalten läuft im Testskript, `Win11_PC_Einstellungen_Dauertest.md` gilt weiter).
- Jeder Messlauf wird kurz in `project-status.md` eingetragen (Datum, Board, Version, Ergebnis).
