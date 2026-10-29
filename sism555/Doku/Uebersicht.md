# Übersicht: Wo steht was (Stand 2026-10-29, Firmware 5.126, Branch `connection-pin`)

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

## Stand der Untersuchung (2026-10-29)
- **Board A (99SC9923, umverdrahtet):** nach Power-Cycle läuft 5.126 stabil, module-suite komplett bestanden (BLE und Classic, 6 Downloads, 0 Retries). Der Ausfall vom 22.10. ist nicht reproduziert, Auslöser des "komischen Modulzustands" unbekannt.
- **Boards B (26SC9983) und C (26SC9994), unverdrahtet:** BLE und Classic scheitern (kein PIN-Prompt, Trennung nach ca. 5 s, nie ein `[BT-Event]` auf RS232), auch mit 5.99 (Bisektion auf C), auch nach Werksreset, Neuprogrammierung, Stromzyklus. Sniffer auf C: Modul beantwortet den CYSPP-CCCD-Write (0x0012) nicht. Firmware-Regression ausgeschlossen. **Gerät C läuft derzeit auf 5.99** (nicht 5.126).
- Offene Hypothesen: Windows-Bond veraltet (nicht geprüft), Hardwareunterschied zu A (IO5/CYSPP-Pegel; die IO5-Idee ist schwach begründet, kein Lötversuch), Modulzustand. Gegenproben: Windows-Geräte entfernen, nRF52840 als zweiter Central (Plan Phase 4), CYSPP-Pegel auf C im Testmenü messen.
- Die früher notierten "8/8 und 10/10 Fehler" auf A waren ein Messfehler (`--address` ohne `--pin`).

## Werkzeuge neu (2026-10-29)
- `tools/flash_xmodem.py --port COM3 --bin <datei>`: Flash per RS232/USB (3, Strg+U, XMODEM-1K, y). `tools/relay_power.py zyklus 10` (Relais COM7): Stromzyklus. `ble_mainboard_test.py --sniff`: nRF52840-Mitschnitt (Plan: `Plan_BLE_Sniffer_nRF52840.md`). Bei `--address` immer `--pin` angeben (Seriennummer ohne `SC`).

## Offene Entscheidungen
1. Windows-Geräte B/C entfernen und neu koppeln (Gegenprobe) oder nRF-Central.
2. Auf C wieder 5.126 flashen (derzeit 5.99).
3. 5.127: Ymodem-Senden bei Linkverlust abbrechen.
4. Aufräumen der temporären Diagnose (5.115/5.116, 5.118, 5.119), wenn die Untersuchung abgeschlossen ist.
5. Allgemeine Änderungen auf `LTE-Update` übernehmen.
