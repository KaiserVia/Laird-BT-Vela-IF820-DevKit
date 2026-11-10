# Übersicht: Wo steht was (Stand 2026-11-10, Firmware 5.137, Branch `connection-pin`)

## Zuerst lesen
- `../../CLAUDE.md`: Projektkontext, Regeln (Commits, Binaries), offene Punkte, Temporär-Code zum Aufräumen.
- `../project-status.md`: das Projekttagebuch, neueste Einträge ganz unten (bis 5.137, Test auf B vom 09.11., R125-Befund vom 10.11.).
- `Board-Inventar.md`: welches Board welche Hardware und Firmware hat.

## NEU 2026-11-10: Umrüstung aller Boards
Alle Boards bekommen R125 nach GND und 47 kΩ Pull-up auf BLT-RXD. Vorher waren sie unterschiedlich bestückt (R125 Pull-up oder offen). Stand je Board, vorgefundener R125-Zustand und Abnahme: `Board-Inventar.md`, Abschnitt "Umrüstaktion November 2026".

## NEU 2026-11-10: BLE während LTE - Ursache R125
- **Fehlerbild:** Gerät online (MQTT, UART-MUX auf LTE), BLE verbindet, Abo wird quittiert, aber kein PIN-Prompt, kein `/leave`, MQTT bleibt. Classic geht.
- **Ursache:** R125 (10 kΩ Pull-up am Modul-CTS, IF820 Pin 14). Bei MUX auf LTE ist die Leitung vom LPC getrennt, R125 zieht sie auf HIGH = "Host nicht bereit". Das Modul schließt den BLE-Handshake nicht ab und zieht CYSPP nicht auf LOW, die Firmware merkt nichts.
- **Belege:** B (26SC9983, R125 bestückt) mit 5.137: BLE 0/3, Classic 3/3. 26SC9966 (R125 nicht bestückt): BLE während LTE in allen Läufen bestanden.
- **Abhilfe:** R125 entfernen (Versuch auf B läuft), für V2 10 kΩ nach GND (`Fragen_Schaltplan_CTS_MUX.md`, Wunschliste). Zusätzlich 5.137: CONNECTION auf P0.19 als zweiter Auslöser, braucht den Draht.
- Bewertung aller Pull-Widerstände: `IF820_Pullups_Bewertung.md`. Messanleitung für das Board mit Modulneustart beim Verbinden: `Anleitung_26SC9993_Neustart_beim_Verbinden.md`.

## Firmware (`sism555/`)
- Quellcode Bluetooth: `btio.c`, `btio.h`; `sio.c` (`putb`, CTS); `USB_tools.c`; `hard.c`. Version: `sprachen/sictxt.c` (`T_version`).
- Binaries: `sism.hex`, `sism.bin` = neueste Version (5.137). Ältere, ungetrackt: `fw_5.99/`, `fw_5.118/`, `fw_5.120/`, `fw_5.129/` bis `fw_5.137/`.
- Schalter in `btio.h`: `BT_CONN_GPIO` (0), `BT_UCP_ENABLE` (1 seit 5.132), `BT_CYSPP_SM` (1, Zustandsmaschine seit 5.131), `BT_CONN_TRIGGER` (1, 5.137).
- Build: `"/c/Keil_v5/UV4/UV4.exe" -b sism.uvprojx -o build_log.txt -j0` im Ordner `sism555`.

## Tests und Werkzeuge (`Doku/tools/`)
- `ble_mainboard_test.py`: Testskript, Modi smoke, cycle, module-suite, fast-suite, soak, interleave, info, power-sweep, boot-race, reboot-check, nightly, gsm-mux, bt-timeout, bt-update, **online-bt** (BLE/Classic während MQTT online, prüft `/leave` und `/join`).
- `flash_xmodem.py --port COM3 --bin fw_5.137/sism.bin --expect-version 5.137`: Flashen über RS232.
- `lte_ble_thread.py`: BLE während der GSM-Threads (Registrierung, MQTT-Connect). `if820_boot_check.py`: zählt Modulneustarts beim Verbinden.
- `comports.bat`: welcher COM-Port gehört wozu. `trace_stray_check.py`: Prüfung auf Stray-Zeichen.
- Reports: `Doku/tools/Testreports/<Datum>_<Name>_<Modus>/` mit `*.html`, `trace.ndjson`, JSON.

## Anleitungen und Erklärungen (`Doku/`)
- `Win11_PC_Einstellungen_Dauertest.md`: PC-Einstellungen gegen Standby und USB-Suspend.
- `BT_CONNECTION_vs_CYSPP.md`: Erklärung CONNECTION-Pin gegen CYSPP für Kollegen.
- `Testplan_CONNECTION_P0-19.md`: Testplan CONNECTION-Pin.
- Herstellerunterlagen: `../../Documentation/` (EZ-Serial-Guide, Abschnitt BLE-Durchsatz mit `/UCP`).

## NEU 2026-10-29 abends: 5.127 (TEST) behebt Board C
- Ursache: Im Leerlauf schaltete `uart1_release()` UART1 ab, das IF820 sah CTS dauerhaft HIGH und hing beim CCCD-Write. 5.127 laesst UART1 beim IF820 im Leerlauf aktiv. Board C: BLE 4/4 und Classic 2/2 bestanden, Gegenprobe 5.126 wieder 2/2 gescheitert. C laeuft jetzt auf **5.127**. Board B: mit 5.127 BLE 4/4 bestanden (vorher mit 5.126 gescheitert), Classic offen (B in Windows nicht gekoppelt); B laeuft auf **5.127**. Details: `project-status.md`, Update 5.127. Offen: Classic auf B, Board A mit 5.127 (module-suite).

## Stand der Untersuchung (2026-10-29, vor 5.127)
- **Board A (99SC9923, umverdrahtet):** nach Power-Cycle läuft 5.126 stabil, module-suite komplett bestanden (BLE und Classic, 6 Downloads, 0 Retries). Der Ausfall vom 22.10. ist nicht reproduziert, Auslöser des "komischen Modulzustands" unbekannt.
- **Boards B (26SC9983) und C (26SC9994), unverdrahtet:** BLE und Classic scheitern (kein PIN-Prompt, Trennung nach ca. 5 s, nie ein `[BT-Event]` auf RS232), auch mit 5.99 (Bisektion auf C), auch nach Werksreset, Neuprogrammierung, Stromzyklus. Sniffer auf C: Modul beantwortet den CYSPP-CCCD-Write (0x0012) nicht. Firmware-Regression ausgeschlossen. **Gerät C läuft derzeit auf 5.99** (nicht 5.126).
- Offene Hypothesen: Windows-Bond veraltet (nicht geprüft), Hardwareunterschied zu A (IO5/CYSPP-Pegel; die IO5-Idee ist schwach begründet, kein Lötversuch), Modulzustand. Gegenproben: Windows-Geräte entfernen, nRF52840 als zweiter Central (Plan Phase 4), CYSPP-Pegel auf C im Testmenü messen.
- Die früher notierten "8/8 und 10/10 Fehler" auf A waren ein Messfehler (`--address` ohne `--pin`).

## Werkzeuge neu (2026-10-29)
- `tools/flash_xmodem.py --port COM3 --bin <datei>`: Flash per RS232/USB (3, Strg+U, XMODEM-1K, y). `tools/relay_power.py zyklus 10` (Relais COM7): Stromzyklus. `ble_mainboard_test.py --sniff`: nRF52840-Mitschnitt (Plan: `Plan_BLE_Sniffer_nRF52840.md`). Bei `--address` immer `--pin` angeben (Seriennummer ohne `SC`).

## NEU 2026-11-02: Testlauf am Tisch, drei Fehler gefunden (Board B, 5.127)
Automatischer Testlauf: `tools/bt_testlauf.py` (Anleitung `tools/bt_testlauf_Anleitung.md`, Doppelklick `tools/bt_testlauf.bat`).
1. **BT tot nach dem naechtlichen Neustart um 3:15** (3/3, Gegenprobe Stromzyklus 3/3 ok). Jedes Geraet im Feld, jede Nacht.
2. **BT tot nach 1 s Stromunterbrechung** (2/2; 0,05-0,5 s und 2-30 s ok). Vermutlich Brown-out-Reset, den main.c nicht behandelt.
3. **Verbinden in den ersten ca. 30 s nach dem Einschalten legt BT bis zum naechsten Stromzyklus lahm** (F11010; dabei gehen `AT&F1`/`$$$` als Daten an die Gegenstelle).
1 und 2: der BT-UART wird nur nach echtem Einschalten/Watchdog gestartet. 3: Neuerkennung bei aktiver Verbindung. Details und Vorschlag 5.128: `project-status.md`, "Testlauf am Tisch 2026-11-02".

## Konzept CYSPP-Zustandsmaschine (2026-11-02, Entwurf)
`Konzept_CYSPP_Zustandsmaschine.md`: Firmware steuert Werben/Verbindbarkeit selbst (`.CYSPPSP E=1`, `.CYSPPSTART`), sichtbar erst wenn bereit, Classic/BLE unterscheidbar, `/UCP`, Trenngrund. Phasen 0-4, Phase 0 = Machbarkeit am Geraet ohne Umbau.

## Plan fuer Montag (vereinbart 2026-10-30)
1. **Abnahme 5.127:** module-suite auf Board A (inkl. Download BLE+Classic); B in Windows neu koppeln, Classic-Smoke auf B; optional LTE-Sitzung auf B ausloesen, danach BLE verbinden.
2. Stand taggen (z. B. `bt-5.127`) als Referenz.
3. Radar-Update des Kollegen mergen. Vorher klaeren: liegt es auf `LTE-Update` oder `main`, und wird `connection-pin` ganz zusammengefuehrt oder nur der 5.127-Fix uebernommen?
4. Danach komplette BT-Testreihe auf dem Merge-Stand, alle drei Boards.
5. Neue Testmodi `nightly` (Neustart 3:15), `bt-update`, `gsm-mux`, `bt-timeout`, `reboot-check`, `power-sweep`, `boot-race`; Beschreibung und Aufrufe in `Testplan_BT_Erweiterung_Strom_Grenzfaelle.md` (Abschnitte 3 und 5). **Zuerst `nightly`**: Verdacht, dass BT nach dem taeglichen Neustart um 3:15 bis zum naechsten Stromzyklus ausfaellt.
6. Neue Testfaelle (Strom, Neustart, Grenzfaelle): `Testplan_BT_Erweiterung_Strom_Grenzfaelle.md`. **Vor dem Merge P1 pruefen** (Verdacht: nach Reboot/Watchdog bleibt UART1 aus, 5.127-Fix greift dann nicht).

## Offene Entscheidungen
1. ~~Windows-Geräte B/C entfernen und neu koppeln~~ (Ursache war 5.127-Fehler, siehe oben; B muss fuer Classic trotzdem neu gekoppelt werden).
2. ~~Auf C wieder 5.126 flashen~~ (C laeuft auf 5.127).
3. Ymodem-Senden bei Linkverlust abbrechen (war als 5.127 geplant, kommt in eine spaetere Version).
4. Aufräumen der temporären Diagnose (5.115/5.116, 5.118, 5.119), wenn die Untersuchung abgeschlossen ist.
5. Allgemeine Änderungen auf `LTE-Update` übernehmen.

## Online-/MQTT-Test: Mobilfunk-Konfiguration am Gerät (Stand 2026-11-03, Board B)
- **APN: `datamobile.ag`**, USER und PASS leer, ROAMING ein (Funkmodems, Mobilfunk Modem, Konfiguriere APN). `datamobile.de` ist falsch.
- SIM in den Halter **auf der Platine** stecken (über das Bedienteil kam sie nicht beim Modem an), Antennen für LTE und GPS anschliessen.
- Servertyp MQTT einstellen (Mobilfunk Menue, Server), danach MQTT-Server eintragen. Cloud-Anlage und Zertifikate (`Zertifikate/`) sind fuer Board B offen.
- Pruefen: Mobilfunk Menue, Strg+G, Punkt 2 (Registrierung, `NOT REGISTERED` = kein Netz) und Punkt 7 (ICCID); Strg+F im Hauptmenue: `SIMPIN=1`, `CSQ` ca. 10 bis 31, `;State=` bei MQTT 5/6.
- `F12005` direkt nach dem APN-Eintrag: die Firmware baut sofort eine GPRS-Verbindung auf; es scheitert, solange das Modem nicht registriert ist.
- Dauer-Online mit Terminal: Strg+G, Punkt 4 "MQTT debug mode" ein. Normal geht das Geraet nur im Zeitfenster (Zeitplanung, Mo-Fr 08:00-18:00) und nur ohne RS232-Terminal online.
