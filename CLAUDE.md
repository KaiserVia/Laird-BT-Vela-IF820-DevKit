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
- **Aktueller Branch:** `LTE-Update`
- **Aktuelle Firmware-Version:** `T_version = "5.66"` (in `sism555/sprachen/sictxt.c`, Z. 27)
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

- Phase 0 (Bring-up / `/PING`-Lebenszeichen) **erreicht**.
- SPP-Datenmodus + Menü über Bluetooth Classic **funktioniert** (Meilenstein 5.92).
- Gerätename BLE + Classic einheitlich `VIASIS_<serno>`.
- Zugangsschutz über bestehende **App-Layer-PIN** (kein BT-Passkey).
- BT-Baud fix über `#define IF820_BAUD 460800` in `btio.h` (921600 verworfen — HW-Limit LPC1766).
- BT-(Re)Konfig erzwingt Command-Mode selbst; Reconfig am besten über RS232.
- BT-Init (5.16) selbstheilend: erkennt Werksmodul/Modultausch (F=00), aktiviert Flow Control
  per `cts_bypass`-Bootstrap automatisch (STU F=01 + /SCFG + /RBT), verifiziert danach den
  CTS-Handshake; bleibt CTS trotzdem HIGH → `F11009` (echter Verdrahtungsfehler).

## ⚠ OFFENER HAUPTPUNKT: BLE-Datenkanal bricht beim Subscribe ab

**Symptom (App-Seite, reproduziert auf zwei Boards):** BLE verbindet, Subscribe auf
`…ca102` wird mit ATT-Success quittiert — und sofort danach trennt das Modul.
Classic SPP läuft dagegen einwandfrei.

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

- **Aufraeumen (bewusst temporaerer Diagnose-Code):** `bt_transfer_echo()`, `crc32_calc()`,
  `bt_send_ack()` (btio.c/.h) und Testmenue-Punkt 8→3 sind reiner Diagnose-Code fuer die
  BLE-CYSPP-Untersuchung (Update 5.67), kein produktives Feature. Ebenso `comchange_count`
  (hard.c/.h, Update 5.70) - zaehlt EINT1/IC54-Interrupts zur CYSPP-Bounce-Diagnose. Nach
  Abschluss der CYSPP-Diagnose wieder entfernen (Anwenderentscheidung 2026-08-18).
- **Entkopplung fehlt:** `C128` und `C58` (je 1 µF an IF820 3P0V/VDDIO, Netz `N$169`) sind
  in **beiden** Bestückungsvarianten als nicht bestückt eingetragen. Das Datenblatt (S. 34)
  fordert 100 nF an **jedem** Power-Pin. Bester Kandidat für „läuft ~1 min, dann träge".
  Wahrscheinlichkeit 55 % gegen das Symptom, 100 % dass die Maßnahme richtig ist.
- **Pull-up auf `BLT-RXD` fehlt** → 0xFF-Junk am BT-Terminal, sobald der MUX auf GSM schaltet
  (Modul reicht Leitungsrauschen transparent weiter). 10–100 kΩ nach VDDIO, 90 %.
- 32-kHz-Quarz `Q3` + C129/C137 nicht bestückt → Modul läuft auf internem LPO. Nachrüsten
  bringt nur mit passender Modul-FW etwas (15 % ohne FW-Wechsel) → zurückgestellt.
- Antennen-Keep-out (Ausfall schon auf 40 cm) — 45 %, erst nach Entkopplung neu bewerten.
- Modul-FW ist 1.4.12.12; 1.4.18.18 behebt „lockup under heavy UART loading" (60 %).
  Setzt voraus, dass HCI-UART (Pins 24–27) + RST (Pin 34) erreichbar sind — HCI-UART ungeprüft,
  RST geklärt (siehe nächster Punkt).
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
   (nur die geänderten Quell-/Doku-Dateien unter `sism555/`, KEINE Binär-/Firmware-Dateien),
   mit Log-Nachricht `BT <version>: <Kurzbeschreibung>`. Reine Doku-Commits ohne
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
