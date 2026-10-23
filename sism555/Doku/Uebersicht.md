# Übersicht: Wo steht was (Stand 2026-10-23, Firmware 5.126, Branch `connection-pin`)

## Zuerst lesen
- `../../CLAUDE.md`: Projektkontext, Regeln (Commits, Binaries), offene Punkte, Temporär-Code zum Aufräumen.
- `../project-status.md`: das Projekttagebuch, neueste Einträge ganz unten (5.117 bis 5.126, Dauertest 5.118, Prüfrunde 5.124, Diskriminierungstest BLE, Vergleichsläufe A/B vom 21.10., Lauf vom 22.10.).

## Firmware (`sism555/`)
- Quellcode Bluetooth: `btio.c`, `btio.h`; `sio.c` (`putb`, CTS); `USB_tools.c`; `hard.c`. Version: `sprachen/sictxt.c` (`T_version`).
- Binaries: `sism.hex`, `sism.bin` = neueste Version (5.126). Ältere, ungetrackt: `fw_5.118/`, `fw_5.120/`, `fw_5.99/`.
- Schalter in `btio.h`: `BT_CONN_GPIO` (0), `BT_UCP_ENABLE` (0, das `/UCP` aus 5.125).
- Build: `"/c/Keil_v5/UV4/UV4.exe" -b sism.uvprojx -o build_log.txt -j0` im Ordner `sism555`.

## Tests und Werkzeuge (`Doku/tools/`)
- `ble_mainboard_test.py`: Testskript, Modi smoke, cycle, module-suite, fast-suite, soak, interleave, info.
- `comports.bat`: welcher COM-Port gehört wozu. `trace_stray_check.py`: Prüfung auf Stray-Zeichen.
- Reports: `Doku/tools/Testreports/<Datum>_<Name>_<Modus>/` mit `*.html`, `trace.ndjson`, JSON.

## Anleitungen und Erklärungen (`Doku/`)
- `Win11_PC_Einstellungen_Dauertest.md`: PC-Einstellungen gegen Standby und USB-Suspend.
- `BT_CONNECTION_vs_CYSPP.md`: Erklärung CONNECTION-Pin gegen CYSPP für Kollegen.
- `Testplan_CONNECTION_P0-19.md`: Testplan CONNECTION-Pin.
- Herstellerunterlagen: `../../Documentation/` (EZ-Serial-Guide, Abschnitt BLE-Durchsatz mit `/UCP`).

## Stand der Untersuchung
- Gut belegt: 5.118 lief am 15.10. auf Board A (99SC9923) zweimal komplett durch. Classic läuft überall stabil. Ymodem-CTS-Fehler seit 5.117 behoben, USB-Hänger seit 5.121 bis 5.124.
- BLE intermittierend: "verbunden, kein PIN-Prompt, Gerät trennt nach ca. 5,5 s", auf beiden Boards, auch mit 5.124. Mit 5.118 und 5.126 auf A liefen die smoke-Läufe am 21.10. 7/7; am 22.10. riss in der module-suite der Link nach ca. 5 min im BLE-Download 1/3, danach war das Gerät für BLE nicht auffindbar. Firmware ist als Ursache nicht belegt.
- BLE-Durchsatz hängt am vom PC gewählten Intervall: Board B 60 ms (3,1 KB/s), Board A 7,5 ms (6,6 KB/s). `/UCP`-Versuch (5.125) zurückgenommen (5.126).
- Board B (26SC9983): mit 5.125 und 5.126 BLE nie durchgegangen (nie ein `C`-Event auf RS232); seit der Classic-Neukopplung vom 21.10. offen.

## Offene Entscheidungen
1. 5.118 auf Board A flashen und dieselbe module-suite (ca. 75 min) laufen lassen: beantwortet die Firmware-Frage.
2. Board B: 5.126 flashen, Versionszeile prüfen, Classic-Kopplung entfernen, smoke 3x.
3. 5.127: Ymodem-Senden bei Linkverlust sofort abbrechen (jetzt sendet die Firmware minutenlang ins Leere).
4. Aufräumen der temporären Diagnose (5.115/5.116 Monitor, 5.118 `bt_session_summary`, 5.119 Eventzeilen), sobald die Untersuchung abgeschlossen ist.
5. Allgemeine Änderungen (5.104/5.105/5.111 bis 5.114/5.117/5.121 bis 5.124) auf `LTE-Update` übernehmen.
