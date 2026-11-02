# CLAUDE.md — Projekt-Merkzettel (Kontext für Cowork/Claude-Sessions)

> Diese Datei wird zu Beginn jeder Session automatisch gelesen. Sie hält den
> Projektkontext fest, damit nichts verloren geht, wenn eine Cowork-Session
> geschlossen wird oder verschwindet. **Cowork-Chats sind flüchtig — der Ordner
> und diese Datei sind es nicht.**

## Was ist dieses Projekt?

Integration des Bluetooth-Moduls **Vela IF820 (Ezurio, EZ-Serial-Firmware)** in die
Gerätefirmware **sism555** (LPC1766, Keil-Projekt `sism555/sism.uvprojx`).
Das IF820 ist das dritte unterstützte BT-Modul neben Laird BT730 und RN4678.

- **Git-Repo:** `KaiserVia/Laird-BT-Vela-IF820-DevKit`
- **Aktueller Branch:** `LTE-Update` (Hauptlinie)
- **⚠ Nebenbranch `connection-pin` (ab 5.101) - HARDWARE-ABHAENGIG, nicht auf normale Boards
  flashen:** setzt eine Umverdrahtung voraus (IF820 `CONNECTION`-Pin direkt auf LPC-Pin **P0.19**;
  in 5.101-5.108 noch auf IC54/IC36-Pad `IO5`, dessen GND-Verbindung dafür aufgetrennt wurde).
  Details: `sism555/project-status.md`, Update 5.101 und 5.109. **Seit 5.120 (2026-10-19) braucht die
  Funktion die Verdrahtung nicht mehr:** `BT_CONN_GPIO` in `btio.h` steht auf 0, `BT_LINK` hängt allein an
  `CYSPP`; `CONNECTION` (P0.19) wird nur noch für Diagnoseanzeigen gelesen (auf unveränderten Boards dauerhaft
  HIGH, dort bedeutungslos). Mit der Weiche auf 1 (5.110-5.119) war auf Boards ohne P0.19-Draht jede
  BT-Eingabe tot (PIN nicht tippbar). OFFEN: ob die "Stray-Zeichen nach dem Trennen" (5.110) ohne die Weiche
  zurückkehren - Prüfung mit `sism555/Doku/tools/trace_stray_check.py` auf dem verdrahteten Board 99SC9923.
- **Aktuelle Firmware-Version:** siehe `sism555/project-status.md` (diese Datei hier wird nicht
  bei jeder Version nachgeführt) - `T_version` in `sism555/sprachen/sictxt.c`
- **Firmware-Quellcode:** `sism555/` (Hauptdatei BT: `btio.c` / `btio.h`)
- **Liefer-/Herstellercode:** `SupplierSourceCode/` (u. a. Vela IF820 Firmware 1.4.18.18, als Submodul)
- **Hersteller-PDFs:** `Documentation/` (Datenblatt, EZ-Serial-Guide, DVK-Guide, PCNs)

## WICHTIGSTE Datei zuerst lesen

➡ **`sism555/project-status.md`** — das laufende Projekttagebuch mit allen
Entscheidungen, Updates (aktuell bis **Update 5.59**), offenen Punkten,
Hardware-Merkern und der Diagnose-Code-Systematik. **Bei jeder neuen Session
zuerst diese Datei lesen**, bevor Änderungen gemacht werden.

## Weitere wichtige Doku (`sism555/Doku/`)

**Aktueller Arbeitsstrang (BLE-Datenkanal / CYSPP-Pin):**

- `HW_Massnahmen_Bewertung_2026-08.md` — **Bewertung aller HW-Maßnahmen mit
  Wahrscheinlichkeiten**, belegt aus Datenblatt, EZ-Serial-Guide und Schaltplan.
  Enthält den zentralen Fund (R77 unbestückt) und die Liste der Sackgassen.
- `Messprotokoll_CYSPP_Diagnose.md` — Messprotokoll zum Ausfüllen (Oszi/Logic-Analyzer)
- `tools/ble_cyspp_probe.py` + `tools/ble_cyspp_probe_Anleitung.md` — BLE-Diagnose vom PC
- `ble-retest-1-negative.md` (App-Seite) / `ble-retest-1-antwort.md` (unsere Antwort)
- `ble-chip-response.md` — frühere Antwort an die App-Seite
- `HW_Massnahmen_IF820.md` — Ursprungskatalog der HW-Maßnahmen (Pin-Abgleich Datenblatt)

**Grundlagen:**

- `IF820_EZSerial_Konzept_Mapping.md` — Konzept/Mapping EZ-Serial-Kommandos
- `IF820_Config_Kandidaten.md` — Config-Audit, Kandidaten A/B
- `Diagnose-Codes.csv` + `Diagnose-Codes_Konzept.md` — Fehler-/Ereignis-Katalog (Typ F/W/I)
- `BT-Evalkit-Tasks.md` — Testplan (Test 1–12), `BT_Teststrategie.md`
- `UART1_Owner_StateMachine.md` — UART1-Sharing BT/GSM
- `Zeitsync_Plan.md`, `Fehlerspeicher_Strategie.md`, `parameterspeicher.md`

## Aktueller Stand (Kurzfassung — Details in project-status.md)

- **2026-11-02 - 5.131 (TEST), Phase 2:** IF820 im Hochlauf unsichtbar (`/AX` + `SBTP C=0` ca. 3,7 s nach dem
  Einschalten, `/A` + `SBTP C=1` am Ende, nur RAM, Flash bleibt `E=2`), Schalter `BT_CYSPP_SM` in btio.h. Phase 3
  (GESPERRT bei LTE/Update/Flash, `/UCP` vor dem Abo) offen. Details `sism555/project-status.md`, Update 5.131.
- **2026-11-02 - 5.129/5.130 (TEST), CYSPP-Zustandsmaschine Phase 0+1:** Phase 0 am Geraet (Testmenue 8->7
  "Modulbefehl senden", `Doku/tools/cyspp_phase0.py`): `.CYSPPSTART` startet mit `E=1` einen Central-Scan statt
  Werben, `SAP F=00` sperrt `/A`; tragfaehig fuer Phase 2 ist `/AX`/`/A` + `SBTP C=0/1` im RAM (Konzept Abschnitt 6a).
  5.130: Ereignis-Parser (Zustand/Transport/Trenngrund, `[BT-Zustand]`-Zeilen), Ymodem-Abbruch bei Linkverlust,
  Fehler 4 behoben (keine Schein-Verbindung bei stromlosem Modul). Kurzprofil + bt-timeout auf Board B bestanden.
  TEMPORAER: `bt_module_cmd()` (Testmenue 8->7) nach Abschluss der Zustandsmaschine entfernen.
- **2026-11-02 - Testlauf am Tisch (Board B, 5.127), drei Fehler:** BT tot nach dem naechtlichen Neustart 3:15
  (`BOOTTASK`, Reboot-Zweig in main.c startet BT nicht), nach 1 s Stromunterbrechung (vermutlich Brown-out-Reset) und
  nach Verbinden waehrend der ersten ca. 30 s (Neuerkennung bei aktiver Verbindung -> F11010). Werkzeug:
  `sism555/Doku/tools/bt_testlauf.py` + Anleitung. Details: `sism555/project-status.md`, "Testlauf am Tisch 2026-11-02".
- **2026-10-29 abends - 5.127 (TEST):** Ursache für B/C gefunden: `uart1_release()` schaltete UART1 im Leerlauf ab, das IF820 sah CTS dauerhaft HIGH und hing beim CYSPP-CCCD-Write. 5.127 lässt UART1 beim IF820 im Leerlauf aktiv. Board C (26SC9994): BLE 4/4, Classic 2/2 bestanden, Gegenprobe 5.126 gescheitert. Board B (26SC9983): BLE 4/4 bestanden. Offen: Classic auf B (neu koppeln), Board A mit 5.127 prüfen. Details: `sism555/project-status.md`, Update 5.127.
- **2026-10-29:** Board A (99SC9923) mit 5.126 nach Power-Cycle stabil (module-suite bestanden). Boards B (26SC9983) und C (26SC9994) scheitern mit BLE und Classic (auch mit 5.99; Modul beantwortet CYSPP-CCCD nicht, nie ein `[BT-Event]`); Firmware-Regression ausgeschlossen, Ursache offen (Windows-Bond? Hardware?). C steht auf 5.99. Details: `sism555/project-status.md`, `sism555/Doku/Uebersicht.md`. Neue Werkzeuge: `Doku/tools/flash_xmodem.py`, `relay_power.py`, `--sniff`.


- Phase 0 (Bring-up / `/PING`-Lebenszeichen) **erreicht**.
- SPP-Datenmodus + Menü über Bluetooth Classic **funktioniert** (Meilenstein 5.92).
- **2026-08-26: Umfassend end-to-end bestätigt** (`sism555/project-status.md`, Update
  "Umfassender End-to-End-Testlauf"): 24 Connect/Disconnect-Zyklen (Classic+BLE, Haltezeiten
  0–300 s) ohne Aussetzer, plus 3× echter Ymodem-Produktionsdownload (4,2 MB) je Transport
  fehlerfrei (bis auf einen einmaligen, nicht reproduzierten Classic-Ausreißer). Ein zuvor auf
  einem neuen Board aufgetretenes "nicht sichtbar beim Pairing"-Problem wurde vom Anwender per
  Hardware-Maßnahme behoben (Details noch nicht dokumentiert).
- **2026-10-15: Dauertest auf Board 99SC9923 mit 5.118 zweimal komplett bestanden** (je ~99 min, Classic +
  BLE, 12 Zyklen je Transport, 419430-Messwerte-Flash, 6 Ymodem-Downloads je Lauf, Retries 0, `ctsDrop=0`).
  Ursache des vorher scheiternden Downloads (5.116: F13010, Block am Ende zu kurz) war das 50-ms-CTS-Limit
  in `putb()`; behoben in 5.117. Details: `sism555/project-status.md`, Updates 5.117/5.118 und "Dauertest
  5.118 am Geraet". Das Testskript hat bekannte Berichts-Unstimmigkeiten (dort aufgelistet).
- **2026-10-20: Prüfrunde mit 5.124 auf zwei Boards** (`sism555/project-status.md`, "Prüfrunde mit Firmware 5.124"): Weiche aus (5.120) ohne Stray-Zeichen in 30
  Zyklen, Board B (26SC9983) funktioniert per BLE und Classic. **BLE-Durchsatz hängt am vom PC gewählten Verbindungsintervall:** B lief mit 60 ms (3,1 KB/s, Modul bremst per CTS
  ~1430x), A mit 7,5 ms (6,6 KB/s). Offen: warum Windows für B ein langes Intervall wählt (Gerät in Windows entfernen und neu koppeln als Gegenprobe).
- Gerätename BLE + Classic einheitlich `VIASIS_<serno>`.
- Zugangsschutz über bestehende **App-Layer-PIN** (kein BT-Passkey).
- BT-Baud fix über `#define IF820_BAUD 460800` in `btio.h` (921600 verworfen — HW-Limit LPC1766).
- BT-(Re)Konfig erzwingt Command-Mode selbst; Reconfig am besten über RS232.
- BT-Init (5.16) selbstheilend: erkennt Werksmodul/Modultausch (F=00), aktiviert Flow Control
  per `cts_bypass`-Bootstrap automatisch (STU F=01 + /SCFG + /RBT), verifiziert danach den
  CTS-Handshake; bleibt CTS trotzdem HIGH → `F11009` (echter Verdrahtungsfehler).

## ✅ BEHOBEN (2026-08-20): BLE-Datenkanal brach beim Subscribe ab

**Ursprüngliches Symptom (App-Seite, reproduziert auf zwei Boards):** BLE verbindet, Subscribe
auf `…ca102` wird mit ATT-Success quittiert — und sofort danach trennt das Modul.
Classic SPP lief dagegen einwandfrei.

**✅ BEWIESEN BEHOBEN (2026-08-20, Board 269993):** Nach Umsetzung des 5-Schritte-Plans (CYSPP-
Pin nie mehr aktiv treiben außer beim bewussten Trennen, Firmware 5.65–5.87) wurde mit
gleichzeitigem Event-Sniff (RS232) + PC-BLE-Test (`ble_mainboard_test.py`) der komplette
DVK-geforderte Ablauf bestätigt: CCCD-Write wird vom Modul gemeldet (`@E,...,W,...,D=0100`),
CYSPP-Bridge aktiviert sich (`.CYSPP,S=01`), Banner+PIN-Prompt kommen per BLE-Notify an, eine per
BLE-Write gesendete PIN kommt tatsächlich in der Firmware an (sichtbar auf RS232) — **bidirektionaler
Datenfluss über BLE funktioniert.** Details/Testverlauf: `sism555/project-status.md`, Abschnitt
"BLE-CYSPP-Bug ENDGÜLTIG BEWIESEN BEHOBEN".

**✅ URSACHE GEKLÄRT (2026-08-10, unabhängig auf IF820-DVK nachgewiesen — Details/Quelle:
`sism555/Doku/ble-Dvk-request.md`, dort auch die konkrete Umsetzungsanforderung):**

Der CYSPP-Pin muss für einen funktionierenden transparenten BLE-Datenkanal **hochohmig/
Eingang** sein — **nicht aktiv HIGH und nicht aktiv LOW getrieben.** Auf dem DVK wurden alle
drei Zustände bei identischer Konfiguration reproduzierbar getestet:

| Zustand der Host-CYSPP-Leitung | Ergebnis |
|---|---|
| aktiv LOW | Fehler — kein nutzbarer Datenpfad |
| aktiv HIGH | Fehler — BLE-Schreiben erscheinen als EZ-Serial-GATT-Events, nicht als UART-Nutzdaten |
| **Eingang / hochohmig** | **Pass** — CCCD `0100`, UART↔BLE transparent in beide Richtungen |

Wichtig: Ein gemessenes HIGH ist **kein Beweis** für den korrekten Zustand — aktiv HIGH und
hochohmig sind beide als HIGH messbar, verhalten sich am Modul aber nachweislich
unterschiedlich. Die früher vermutete Ursache "R77 (Pull-down) fehlt, daher liegt die
Leitung nur über den PCA9554-Pull-up auf HIGH" ist damit **nicht der Kernfehler** — das
eigentliche Problem ist, dass unsere **Firmware** die Leitung über `bt_cmdmode()`
(IC54 Bit 7 als Ausgang HIGH) an mehreren Stellen aktiv treibt:
- `bt_disconnect()` (btio.c) — SPP-Trennung erzwingen
- `init_bluetooth()` (btio.c) — Reconfig-Command-Mode erzwingen
- `uart1_request()` (hard.c) — beim Verdrängen von BT durch GSM/GPS (`bt_displaced`)
- Bluetooth-Deinstallieren-Menüpunkt (sicom.c case -1)

Alle vier Stellen widersprechen der jetzt bestätigten Anforderung "Leitung nie aktiv treiben,
auch nicht beim UART1-Muxwechsel oder bei Fehler-/Timeout-Pfaden" und müssen überarbeitet
werden. **Voraussichtlich ist KEINE Hardware-Änderung nötig** (R77, CONNECTION-Pin-
Umverdrahtung) — reine Firmware-Korrektur. `HW_Massnahmen_Bewertung_2026-08.md` und
`Messprotokoll_CYSPP_Diagnose.md` (Wege A/B/C, R77/P27-Umverdrahtung) sind als *Lösungsweg*
überholt, bleiben aber als Herleitungs-/Messhistorie erhalten.

**Zwei Sackgassen — nicht nochmal untersuchen:**

- **CYSPP per `.CYSPPSP` pin-unabhängig machen: geht nicht.** Guide S. 22: „Externally
  asserting (LOW) the CYSPP pin will **always** begin CYSPP operation, even if the profile
  has been disabled." Umgekehrt S. 26/38: Pin HIGH → „CYSPP data … is **never** raw/transparent
  unless the host asserts the CYSPP pin." Kein Parameter hebt die Pin-Abhängigkeit auf.
- **GPIO-Umbelegung in EZ-Serial: geht nicht.** Die GPIO-Gruppe (ID 9) kennt nur `/QADC`,
  `SIOD`, `GIOD`, `SIOL`, `GIOL`; `SPWM`/`GPWM` sind „Not implemented". Guide S. 13: „All GPIO
  pins are **pre-defined**." Die Supermux-Option im Datenblatt gilt nur für rohe
  CYW20820-SDK-Firmware.

## Offene / zu beobachtende Punkte

- **BACKLOG-Idee (2026-08-18, NICHT umgesetzt, bewusst zurueckgestellt) - Classic und BLE
  getrennt behandeln (Transport-Erkennung):** Aktuell unterscheidet die Firmware nicht, ob eine
  BT-Verbindung Classic SPP oder BLE ist - `BT_LINK`/CYSPP-Pin ist ein einziges, transport-
  neutrales Signal ("irgendein transparenter Datenkanal ist aktiv"). Um das zu unterscheiden,
  muesste die Firmware die `@E,...`-Ereignisse des Moduls auswerten (gap_connected fuer BLE vs.
  ein eigenes Classic-Connect-Ereignis) - das erfordert, CYSPP-Autostart abzuschalten
  (`.CYSPPSP E=1` statt `E=2`) und den Uebergang in den Datenmodus selbst per Zustandsmaschine
  zu steuern (`.CYSPPSTART`). Das ist inhaltlich dasselbe groessere Vorhaben wie "Option B" im
  BLE-Durchsatz-Backlog (project-status.md) - kein kleiner Zusatz, sondern ein Umbau in der
  Groessenordnung der gesamten bisherigen CYSPP-Untersuchung, mit echtem Regressionsrisiko fuer
  das aktuell zuverlaessig funktionierende Classic SPP (Phase 1 legt BEIDE Transporte lahm, bis
  die Zustandsmaschine fertig ist). Fuer das aktuelle Problem (CYSPP-Treiben bei bewusstem
  Trennen) nicht noetig - dort spielt der Transport keine Rolle, weil die Verbindung ohnehin
  beendet wird. Nur bei einem spaeteren, konkreten Bedarf (z. B. der BLE-Durchsatz-Wunsch) neu
  bewerten.
- **⚠ NEU (2026-08-18) - BT-Modul faellt gelegentlich komplett aus** (separates Problem, NICHT
  Teil der BLE-CYSPP-Untersuchung): Anwenderbeobachtung "manchmal geht das einfach nicht mehr".
  **Fehlerbild:** `/PING` scheitert (Modul antwortet gar nicht mehr, kein Baud-Scan findet es).
  **Recovery:** nur ein Power-Cycle hilft, und zwar mit **laengerer Stromlos-Zeit** (kurzes
  Aus/Ein reicht nicht) - deutet auf einen internen Kondensator/Reset-Schwellwert im Modul oder
  auf dem Board, der Zeit zum Entladen braucht. Passt zum bereits dokumentierten `RST_L`-Punkt
  weiter unten (LPC kann das IF820 nicht selbst zuruecksetzen, da RST an nichts haengt - R124-Pad
  waere der Zugang) - noch nicht bestaetigt, aber ein nahliegender Zusammenhang: mit einem
  funktionierenden RST koennte das Modul per Software-Reset statt per langem Power-Cycle
  wiederbelebt werden. Auf der Liste, noch nicht tiefer untersucht.
  **Nachtrag (2026-08-18) - vermutlich derselbe Mechanismus, mildere Auspraegung:** Windows-
  seitiger Bluetooth-Classic-Disconnect (COM-Port "Trennen" in HyperTerminal) hing sehr lange,
  endete mit Windows-Fehlern ("Kein Prozess am anderen Ende der Pipe", "Zeitlimit fuer die
  Semaphore erreicht"). Danach lief das Geraet auf RS232 extrem langsam (5 s fuer ein paar
  Menuezeilen), obwohl im Hauptmenue nichts Blockierendes lief. `bt_cyspp_monitor()` zeigte
  `CYSPP bit7 = LOW` (das MODUL haelt die Leitung selbst noch aktiv) - das Modul denkt also noch,
  es sei verbunden, obwohl der Windows-seitige Link laengst tot ist (Split-Brain). **Unser
  bestehendes Sicherheitsnetz (Minutentick in measure.c) greift hier NICHT**, weil es nur bei
  `!(connect&BT_LINK)` eingreift - hier liest die Leitung ja "aktiv", weil das MODUL sie treibt,
  nicht unsere eigene Firmware. Die Langsamkeit auf RS232 erklaert sich dadurch, dass `putb()`
  bei gesetztem `BT_LINK`/`connect&UART1` jede Terminal-Ausgabe zusaetzlich Richtung UART1
  spiegelt und dort auf CTS/THRE-Timeouts wartet (bis zu ~1 s pro Byte), solange das Modul nicht
  mehr sauber reagiert. Noch offen: ob/nach wie langer Zeit sich das von selbst aufloest
  (Classic-Link-Supervision-Timeout des Moduls) oder ob nur Power-Cycle hilft.
  **AUFGELOEST (2026-08-18) - Ursache gefunden: Board 269996 ist defekt.** Systematischer
  Vergleichstest (Konfig-vs-Code, siehe project-status.md Update "5.86/5.50-Vergleich"): auf
  Board 123456 tritt der 40s-Idle-Haenger unter keiner Firmware/Konfig-Kombination auf (5.50 alt,
  5.50+aktuelle Konfig, 5.86 aktuell - alles stabil, mehrfach getestet). Nur Board 269996 zeigte
  den Haenger, unabhaengig von Firmware-Version. **Board 269996 ist defekt und nicht reparabel
  ohne Bauteil-Nacharbeit** (Anwenderbefund) - der 40s-Haenger war also die ganze Zeit ein
  **Hardware-Defekt auf diesem einen Board**, kein allgemeiner Firmware-Bug. Die im Rahmen dieser
  Untersuchung gefundenen und behobenen Firmware-Bugs (PIN-Endlosschleife 5.79, Sniffer-Redesign
  5.81, Selbstnavigation beim Trennen 5.82, UART-Puffer-Recovery 5.83, Reboot-Verify-Timing 5.85)
  bleiben trotzdem gueltige, eigenstaendige Verbesserungen - sie haben nur nicht den 40s-Haenger
  selbst verursacht/behoben, da der auf einem defekten Board sitzt.
  **Nachtrag (2026-08-20) - derselbe "Modul antwortet gar nicht mehr"-Ausfall auch auf Board
  269993 beobachtet** (bisher als gesund geltendes Board, nicht 269996): nach mehreren
  BT-Classic-Verbindungszyklen hintereinander (5. Versuch) reagierte das Modul auf `/PING` auf
  KEINER Baudrate mehr (voller Scan durchprobiert). CYSPP-Monitor zeigte dabei `bit7 = HIGH`
  (Modul selbst meldet "nicht verbunden" - anderes Bild als der LOW-Split-Brain bei 269996, aber
  gleiches Endergebnis: Modul stumm). Nur langer Stromlos-Power-Cycle hat geholfen, danach lief
  alles wieder normal (volle Erkennung + Konfiguration erfolgreich, PIN-Eingabe sauber auf
  RS232 UND BT-Terminal sichtbar).
  **KORREKTUR (2026-08-20, noch am selben Tag):** Die obige Einschaetzung "kein Zusammenhang mit
  den 5.89-5.91-Puffer-Aenderungen" war falsch und zu voreilig. Ursache gefunden und in 5.92
  behoben (siehe project-status.md, Update 5.92): die 5.91-Ergaenzung hielt einen Diagnose-Index
  (`sniff_i`) aktuell, der mit der eigentlichen Ueberlauf-/Interrupt-Wiederfreigabe-Logik (haengt
  an `bxi1`) nichts zu tun hatte - der neue Command-Mode-Puffer `rbuf1` konnte aus `bxi1`s Sicht
  nach ein paar BT-Verbindungszyklen trotzdem volllaufen und den UART1-Empfangsinterrupt
  dauerhaft abschalten. **Das Modul war nicht wirklich tot - unsere eigene Firmware hatte sich
  selbst taub gemacht.** Mit 5.92 (ein gemeinsamer Index `bxi1` fuer `rbuf1`) behoben. Der
  AELTERE, vor der CYSPP-Investigation dokumentierte "Modul faellt gelegentlich aus"-Punkt
  (RST_L/Power-Cycle) bleibt als eigenstaendiges, ungeklaertes Thema bestehen - aber zumindest
  die heute wiederholt reproduzierten Faelle waren dieser eigene 5.89-5.91-Bug, kein Hardware-
  Defekt.
- **Aufraeumen (bewusst temporaerer Diagnose-Code):** `bt_transfer_echo()`, `crc32_calc()`,
  `bt_send_ack()` (btio.c/.h) und Testmenue-Punkt 8→3 sind reiner Diagnose-Code fuer die
  BLE-CYSPP-Untersuchung (Update 5.67), kein produktives Feature. Ebenso `comchange_count`
  (hard.c/.h, Update 5.70) - zaehlt EINT1/IC54-Interrupts zur CYSPP-Bounce-Diagnose. Nach
  Abschluss der CYSPP-Diagnose wieder entfernen (Anwenderentscheidung 2026-08-18).
- **Aufraeumen - TEMPORAER seit 5.119 (Anwenderentscheidung 2026-10-19):** `ev_line`/`ev_n`/`bt_event_line()`
  und der Aufruf im else-Zweig von `bt_sniff_poll()` (btio.c) geben die Modul-Events `C` (Verbindungsparameter
  I=/L=/O=) und `DIS` (Trenngrund R=) immer als Zeile `[BT-Event] @E,...` auf RS232 aus. Nur fuer die Ursachensuche
  bei BLE-Abbruechen/Langsamlauf (PC vs. Modul). **Wieder entfernen**, sobald die Untersuchung abgeschlossen ist.
- **Aufraeumen - TEMPORAER seit 5.118 (Anwenderentscheidung 2026-10-14):** `bt_session_summary()`
  (btio.c/.h) gibt beim BT-Trennen eine Zeile `[BT-Sitzung beendet] TX=... ctswait=... ctsDrop=...` ans Terminal,
  Aufruf in `libtool.c` (`communication_change()`, Trennen-Zweig). Nur fuer die Ymodem-/CTS-Fehlersuche (5.117),
  damit der RS232-Sniffer die Zaehler je Sitzung im Trace sieht. **Wieder entfernen**, sobald der Fehler
  abgeschlossen ist. Der Zaehler `bt_cts_drop` (sio.c/.h) und die Anzeige `ctsDrop=` in `bt_show_counters()`
  duerfen bleiben.
- **Aufraeumen - TEMPORAER seit 5.115 (Anwenderentscheidung 2026-10-14):** Der
  **CYSPP/CONNECTION-Hintergrund-Monitor** (`bt_monitor_poll()` + Zustandsvariablen `mon_*` in
  `btio.c`, Aufruf in `measure.c` neben `bt_sniff_poll()`, Deklaration `btio.h`, Zusatz "(ein/aus)"
  im Testmenue-Text `sicom.c`) existiert nur fuer den CONNECTION-Test
  (`sism555/Doku/Testplan_CONNECTION_P0-19.md`). **Rueckbau nach Abschluss des Tests:**
  `bt_cyspp_monitor()` wieder auf die blockierende Fassung (Commit `d6f8bb8`, 5.114) zuruecksetzen -
  oder, zusammen mit dem uebrigen CYSPP-Diagnosecode, ganz entfernen; ebenso `bt_monitor_active()` (btio.c/.h) und die zustandsabhaengige Menuezeile 7 in
  `sicom.c` (5.116). Grund fuer den Hintergrundbetrieb:
  der blockierende Monitor haelt die Hauptschleife an, ein BT-Connect wird dann nicht verarbeitet.
- **Entkopplung fehlt:** `C128` und `C58` (je 1 µF an IF820 3P0V/VDDIO, Netz `N$169`) sind
  in **beiden** Bestückungsvarianten als nicht bestückt eingetragen. Das Datenblatt (S. 34)
  fordert 100 nF an **jedem** Power-Pin. Bester Kandidat für „läuft ~1 min, dann träge".
  Wahrscheinlichkeit 55 % gegen das Symptom, 100 % dass die Maßnahme richtig ist.
- **Pull-up auf `BLT-RXD` fehlt** → 0xFF-Junk am BT-Terminal, sobald der MUX auf GSM schaltet
  (Modul reicht Leitungsrauschen transparent weiter). 10–100 kΩ nach VDDIO, 90 %.
- **KORREKTUR (2026-08-18):** 32-kHz-Quarz `Q3` ist doch bestückt (Anwender hat es am Board
  geprüft) — der Punkt „Modul läuft auf internem LPO, Timing ungenau" entfällt damit als
  Erklärungsansatz.
- Antennen-Keep-out (Ausfall schon auf 40 cm) — 45 %, erst nach Entkopplung neu bewerten.
- **KORREKTUR (2026-08-18):** Modul-FW-Unterschied als Erklärung für die BLE-Verbindungsprobleme
  ist entkräftet — das Eval-Kit, mit dem der Anwender erfolgreich über BLE kommuniziert hat
  (Grundlage von `ble-Dvk-request.md`), lief ebenfalls auf **1.4.12.12**, nicht 1.4.18.18 (das
  war ein *anderes* Eval-Kit als das in Update 5.47 verglichene). Damit läuft sowohl das
  erfolgreiche Referenzboard als auch unser Mainboard auf derselben Modul-Firmware. Der
  „lockup under heavy UART loading"-Fix in 1.4.18.18 bleibt trotzdem grundsätzlich sinnvoll,
  ist aber nicht mehr der Hauptverdächtige für dieses Symptom.
- **KORREKTUR (2026-08-18) - reine HW/RF-Erklärung insgesamt unwahrscheinlich:** Bluetooth
  Classic läuft auf demselben Board sauber (gleicher Funkteil, gleiche Antenne, gleiche
  Stromversorgung) - eine reine Antennen-/Entkopplungs-/Stromversorgungs-Erklärung müsste
  Classic genauso treffen, tut es aber nicht. Zusätzlich war GSM/LTE waehrend der fehlgeschlagenen
  BLE-Tests durchgehend AUS - GSM-Funkinterferenz scheidet fuer diese konkreten Fehlschläge damit
  aus. Der Unterschied liegt vermutlich in BLE-spezifischen Protokoll-/Timing-Eigenschaften, nicht
  in gemeinsam genutzter Hardware: BLE hat einen deutlich kürzeren, beobachteten Supervision-
  Timeout (teils nur 1 s, siehe project-status.md Update 5.63) und einen zeitkritischeren
  Verbindungsaufbau als Classic (reifere, fehlertolerantere Baseband-Prozedur mit längerem
  Timeout ~20 s). Ein kurzer Aussetzer, den Classic wegsteckt, kann eine BLE-Verbindung mit
  1-Sekunden-Timeout sofort beenden. Fokus liegt daher auf den bereits angegangenen BLE-
  Verbindungsparametern (PPCP/Supervision-Timeout, Update 5.66), nicht auf Antenne/Stromversorgung.
- **`RST_L` (Pin 34) hängt am Mainboard an nichts.** Netz `N$160` enthält nur `IC37.RST` und
  `R124` Pad 1; **R124 (10 kΩ nach VDDIO) ist in beiden Varianten nicht bestückt**. Folgen:
  (a) der LPC kann das Modul **nicht** zurücksetzen — deshalb hilft bei einem hängenden Modul
  nur ein Power-Cycle des Gesamtgeräts, und ein LPC-Reflash nimmt das IF820 nicht mit;
  (b) das **freie R124-Pad ist der Zugang zu RST** (kein Feinpitch nötig) — für Onboard-Flashen
  oder um RST auf einen freien LPC-GPIO zu führen (75 %, dass das die „Modul hängt"-Fälle löst).
  Beschaltung ist laut Datenblatt **nicht erforderlich**: Tabelle 21 S. 31 listet RST als
  „Pin When Not Used: **floating**", mit internem Pull-up; die Referenzschaltungen §9.4/§10.4
  erwähnen RST gar nicht. Kein Timing spezifiziert. Details in
  `HW_Massnahmen_Bewertung_2026-08.md` Abschnitt 9a.
- **Betriebsregel:** immer nur **eine** Versorgungsquelle (Board **oder** DVK-USB, nie beides).
  Verstöße haben bereits zwei Module und ein DVK zerstört (Back-Powering).
- BLE-Durchsatz-Optionen A/B (Backlog, noch nicht umgesetzt).

## ⚠ Repo-Hygiene: 141 Dateien zeigen Scheinänderungen

`git status` meldet ~141 modifizierte Dateien, die **niemand inhaltlich geändert** hat —
`git diff --ignore-all-space` liefert für sie einen leeren Diff. Es sind reine
Zeilenende-Änderungen (CRLF/LF), vermutlich durch Tool-Zugriffe entstanden.

**Konsequenz:** Beim Committen **niemals `git add -A` oder `git commit -a`** verwenden,
sondern immer die geänderten Dateien **explizit** einzeln angeben. Sonst landen 141
Scheinänderungen plus Binär-/Firmware-Dateien im Commit.

Sinnvoll wäre, das dauerhaft über `.gitattributes` (`* text=auto`) zu bereinigen — bisher
nicht gemacht, damit der Fix nicht mit den offenen BT-Themen vermischt wird.

## Arbeitsweise / Kontext-Erhalt (damit nichts verloren geht)

1. **Alle Ergebnisse in diesen Ordner speichern**, nicht nur im Chat lassen.
2. **project-status.md fortschreiben** — jede substanzielle Änderung als neues
   `## Update <Version>`-Kapitel mit Symptom / Ursache / Fix.
3. **Git committen** — sicherste dauerhafte Sicherung, unabhängig von Cowork.
   **Konvention: JEDE Erhöhung von `T_version` wird als eigener Commit gesichert**
   (die geänderten Quell-/Doku-Dateien unter `sism555/` **plus `sism555/sism.bin` und `sism555/sism.hex`**
   der frisch gebauten Version - nur diese beiden; sonst KEINE Binär-/Firmware-Dateien, auch nicht
   `fw_*/`-Ordner, `Objects/` o. ä.; Anwenderentscheidung 2026-10-21, damit jede Version als Binary
   wiederholbar ist), mit Log-Nachricht `BT <version>: <Kurzbeschreibung>`. Vor dem Commit prüfen,
   dass die Version in `sism.bin` zu `T_version` passt (Build ohne Fehler/Warnungen). Reine Doku-Commits ohne
   Versionssprung: `BT Doku: <Kurzbeschreibung>`.
   **Wegen der Scheinänderungen (siehe oben) immer explizite Dateipfade angeben.**
4. Diese `CLAUDE.md` aktuell halten (Branch, aktuelle Version, große offene Punkte).

## Werkzeug-Hinweis: Cowork vs. Claude Code

- **Cowork** eignet sich gut für Hardware-Analyse, Datenblatt-/Schaltplanauswertung und
  erklärende Diagramme. Die Shell läuft aber in einer **separaten Linux-Sandbox** —
  Keil/ARM-Toolchain lässt sich dort **nicht** ausführen.
- **Claude Code** ist die bessere Wahl für die Firmware-Umsetzung: echtes Repo, echtes Git,
  Build-Loop mit der Keil-Toolchain.
- Übergabe zwischen beiden läuft über **diese Datei plus die Doku unter `sism555/Doku/`**.

---
*Angelegt am 2026-07-14 als dauerhafter Session-Kontext. Zuletzt aktualisiert 2026-08-05.*
