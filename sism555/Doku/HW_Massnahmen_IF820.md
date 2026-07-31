# Hardware-Maßnahmen Vela IF820 (Mainboard-Integration)

> Zusammenstellung der empfohlenen Hardware-Änderungen für eine zuverlässige IF820-Integration.
> Grundlage: Ezurio-Datenblatt (EZ-DS-Vela IF820_v2_2), DVK-Guide und die im Projekt beobachteten
> Symptome (Brownout-Verdacht, „läuft 1 min dann träge", Modul-Latch-up „tot bis 1 h stromlos",
> BT-Terminal-Junk beim MUX-Wechsel, schwache RF auf 40 cm).
> Angelegt: 2026-07-27.

## Kontext / beobachtete Symptome (wogegen die Maßnahmen wirken)

- **Brownout-Verdacht** auf der gemeinsamen 3,3-V-Schiene; das DVK (eigene, saubere Versorgung) läuft, das Onboard-Modul nicht zuverlässig.
- **„Läuft ~1 Minute, dann wird alles träge, BT weg"** – Backpressure/CTS-Stau bzw. Spannungseinbruch unter Sendelast.
- **Modul „tot nach Power-Cycle, erst nach ~1 h stromlos wieder da"** – klassische **Latch-up-/Back-Power**-Signatur.
- **Ausfall schon auf 40 cm** mit interner Antenne – Verdacht auf verstimmte Antenne (Layout/Keep-out).
- Zwei Module und ein DVK wurden bei externer Einspeisung beschädigt.

---

## 1. Entkopplung direkt am Modul: 100 nF pro Power-Pin + Bulk-Elko

**Was:** An **jeden** Versorgungspin je **100 nF** so nah wie möglich — beim Serienmodul (integrierte Antenne) sind das **VDDIO (Pin 2)** und **3P0V (Pin 22)** —, dazu ein **Bulk-Kondensator 10–47 µF** (Keramik oder Elko) unmittelbar an **3P0V** (Hauptversorgung). Hinweis: „VBAT" ist der Board-/DVK-Name; am Serienmodul heißt dieser Versorgungspin **3P0V (Pin 22)** — es gibt keinen separaten VBAT-Pin.

**Warum:** Das Datenblatt schreibt wörtlich „put a 100 nF capacitor on EACH power pin, as close as possible … to make sure the internal PMU working correctly". Der Bulk-Elko puffert die **Stromspitzen beim Senden** ab, sodass die Modulspannung lokal nicht einbricht.

**Vorteile:**
- Verhindert den **lokalen Spannungseinbruch** beim TX-Peak → beseitigt sehr wahrscheinlich „läuft 1 min dann träge".
- Entschärft den **Auslöser für Latch-up** (Brownout).
- Cent-Betrag, minimaler Aufwand, kein Firmware-Bezug.

**Nachteile:**
- Etwas Platinenfläche; sonst praktisch keine.

**Gefahren:**
- Zu wenig/zu weit weg platziert = wirkungslos → Platzierung **direkt am Pin** ist entscheidend.
- Sehr großer Bulk-Elko kann den Einschaltstrom (Inrush) erhöhen – bei 10–47 µF unkritisch.

---

## 2. Externen 32,768-kHz-Quarz bestücken (+ passende Firmware-Variante)

**Was:** Quarz **Y1 (32,768 kHz)** zwischen **XTALI_32K (Pin 4)** und **XTALO_32K (Pin 5)**, parallel **R1 = 10 MΩ**, je **6 pF Lastkondensatoren** (C3/C4) nach GND. (Referenz: Datenblatt Abschnitt 10.4, Figure 14.)

**Warum:** Der 32-kHz-Takt bestimmt die BT-Sleep-Clock-Genauigkeit. Datenblatt: „Highly recommend … lowest power and highest timing accuracy" und ausdrücklich „**if not, link issue may be occurred**". Der interne RC-LPO ist ungenau und temperaturabhängig.

**Vorteile:**
- **Stabilere BT-Verbindung** (präzises Aufwachen zum Connection-Event) → weniger Abbrüche.
- **Niedrigster Stromverbrauch** im Sleep.

**Nachteile:**
- Drei zusätzliche Bauteile + Platz; Layout am Quarz sauber (kurze Leitungen, GND-Guard).

**Gefahren:**
- **Falsche Lastkapazität** → Oszillator startet nicht oder läuft off-frequency → schlimmer als ohne Quarz. Werte aus dem Datenblatt (6 pF) bzw. Quarz-Spezifikation übernehmen.
- **Firmware-Variante muss passen:** mit Quarz die **normale** EZ-Serial-Firmware, ohne Quarz die **int-lpo**-Variante. Falsche Paarung = Timing-/Link-Probleme.

---

## 3. Serienwiderstände in die Signalleitungen zum Modul (Latch-up-/Back-Power-Schutz)

**Was:** In die vom LPC **ins Modul** getriebenen Leitungen je einen Serienwiderstand **330 Ω – 1 kΩ**: Modul-**RXD** (von LPC-TXD), Modul-**CTS** (von LPC-RTS), **CYSPP** (von LPC/IC54). Sinnvoll auch symmetrisch auf Modul-**TXD → LPC-RXD**.

**Warum:** Genau das ist der Kern der „tot nach Power-Cycle, wieder da nach 1 h"-Story: Werden Pegel in das Modul getrieben, während dessen VBAT/VDDIO fehlt oder einbricht, fließt Strom über die IO-/ESD-Dioden in den Chip → **Back-Powering / Latch-up**. Ein Serienwiderstand **begrenzt diesen Rückstrom** unter die Latch-up-Schwelle.

**Vorteile:**
- Wirksamer, **billiger Schutz gegen Latch-up** und gegen das „Phantom-Speisen" über die Eingänge.
- Dämpft nebenbei Flanken (weniger EMI/Überschwinger).
- Wirkt **firmware-unabhängig**.

**Nachteile:**
- Bei sehr hoher Leitungskapazität und 460800 Baud könnten zu große Werte die Flanken verschleifen. (In der Praxis unkritisch: 1 kΩ × ~30 pF ≈ 30 ns gegenüber ~2,17 µs Bitzeit.)

**Gefahren:**
- Werte **≤ 1 kΩ** halten und bei langen/kapazitiven Leitungen die Signalqualität einmal am Oszi prüfen. Zu hochohmig → Datenfehler auf der UART.

---

## 4. Stabile, ausreichend dimensionierte Modul-Versorgung (ggf. eigener LDO)

**Was:** Für das Modul eine **belastbare 3,3-V-Quelle** vorsehen – idealerweise einen **eigenen LDO** nur fürs BT-Teil (so wie das DVK), mindestens aber die gemeinsame Schiene so auslegen, dass die BT-TX-Peaks keinen Einbruch verursachen.

**Warum:** Das DVK mit sauberer, eigener Versorgung läuft; das Onboard-Modul auf der geteilten Schiene nicht zuverlässig. Der Brownout ist der gemeinsame Auslöser von Trägheit **und** Latch-up.

**Vorteile:**
- Beseitigt die **Wurzel** vieler Symptome (Einbruch → Träge/Latch-up/Link-Abbruch).
- Entkoppelt das BT-Teil von Lastspitzen der übrigen Elektronik.

**Nachteile:**
- Zusätzlicher LDO = Kosten/Platz; ggf. Layout-Änderung.

**Gefahren:**
- Unterdimensionierte Schiene → wiederkehrende Instabilität; im Zusammenspiel mit Maßnahme 1 (Entkopplung) betrachten – beide gehören zusammen.

---

## 5. Antennen-Layout für die INT-ANT-Variante

**Was:** Datenblatt Abschnitt 12: **Modulkante bündig mit der Platinenkante**, seitlich **≥ 15 mm GND-Fläche freihalten**, **unter der Antenne keine Bauteile / keine High-Speed-Leitungen / kein durchgehendes Kupfer**, **kein Metall über der Antenne**.

**Warum:** Ausfall schon auf 40 cm deutet auf eine durch das Layout **verstimmte interne Antenne** hin. Schlechte Abstrahlung → viele Retransmits → Pufferstau → Flow-Control-Bremse (= „1 min dann träge") und mehr Sendestrom (→ Brownout).

**Vorteile:**
- Volle Reichweite/Durchsatz, weniger Retransmits, dadurch **weniger Sendestrom** und weniger Brownout-Stress.

**Nachteile:**
- Schränkt die Platzierung des Moduls stark ein; kann ein **Redesign** der Platine bedeuten.

**Gefahren:**
- Lässt sich der Keep-out nicht einhalten → besser die **EXT-ANT-Variante** mit externer Antenne verwenden. Keep-out zu ignorieren = chronische RF-Probleme, die man softwareseitig nicht lösen kann.

---

## 6. Flusssteuerungs-Pfad (CTS/RTS über MUX IC13) prüfen und absichern

**Was:** Verifizieren, dass **Modul-RTS → MCU-CTS (P2.2)** und **MCU-RTS → Modul-CTS** sauber über den Analog-MUX **IC13 (TS5A23157DGSR)** laufen, und dass dessen Versorgung **V+D stabil** ist. CTS/RTS-Leitungen kurz und definiert.

**Warum:** Das CTS, das die Firmware pollt, läuft beim Board durch IC13. Sackt V+D ab oder ist der Pfad marginal, bleibt **CTS ungewollt HIGH** → die Ausgabe wird ausgebremst (das „RS232 tröpfelt / System träge").

**Vorteile:**
- Zuverlässige **Hardware-Flusssteuerung** → kein Throttle/Freeze durch hängendes CTS.

**Nachteile:**
- Keine nennenswerten; ggf. zusätzliche Entkopplung an IC13.

**Gefahren:**
- Marginaler CTS-Pfad reproduziert exakt das Trägheits-Symptom; V+D des Switches mit in die Entkopplungs-Betrachtung (Maßnahme 1/4) nehmen.

---

## 7. Definierte Ruhepegel: CYSPP-Pull und IF820-RXD-Pull-up

**Was:**
- **CYSPP-Leitung** mit einem definierten Ruhepegel versehen (Pull), damit sie im unverbundenen Zustand nicht floatet.
- **Pull-up (10–100 kΩ) auf der IF820-RXD** (BLT-RXD) nach VDDIO.

**Warum:**
- Eine **floatende CYSPP** → erratisches `BT_LINK` (offener Punkt im Projekt).
- Schaltet der UART1-MUX (IC18) auf GSM, wird die **IF820-RXD abgeklemmt und floatet** → das Modul leitet Leitungsrauschen als **Junk über Bluetooth** weiter. Ein Pull-**up** hält die Leitung auf Idle-High → kein Rauschen. Wirkt **firmware-unabhängig** (die Software-Lösung dafür braucht Modul-FW 1.4.18.18).

**Vorteile:**
- Stabiles `BT_LINK`-Signal; **kein BT-Terminal-Junk** beim Modem-/GPS-Betrieb; zusätzlich etwas Back-Power-Entschärfung an dem Pin.

**Nachteile:**
- Zwei Widerstände; sonst keine.

**Gefahren:**
- **Pull-up, nicht Pull-down** an RXD – ein Pull-down erzeugt Dauer-Break (0x00/Framing-Fehler) = erst recht Müll.
- Pull-Richtung/Stärke an CYSPP zur Logik des Moduls passend wählen (aktiv-Pegel beachten).

---

## 8. Power-Sequencing und nur EINE Versorgungsquelle

**Was:**
- Sicherstellen, dass **VBAT/VDDIO des Moduls anliegen, bevor** der LPC Signale in das Modul treibt (Einschalt-Reihenfolge). Optional einen **Load-Switch** fürs Modul, der sauber schaltet.
- **Betriebsregel:** immer nur **eine** Quelle – **entweder** DVK-USB **oder** Board, **nie beides**; VBAT/VDDIO **nicht extern gegen** die Board-Versorgung einspeisen; beim Umstecken das Modul **stromlos** machen.
- Optional **Verpolschutz/Reverse-Schutz** auf der Modulversorgung.

**Warum:** Genau die verletzte Reihenfolge (Signale treiben, während Modul unversorgt) bzw. die externe Doppel-Einspeisung hat **DVK und ein Modul beschädigt** und das Latch-up ausgelöst.

**Vorteile:**
- Verhindert **Back-Powering/Latch-up** an der Wurzel und schützt die Module vor dauerhaftem Schaden.

**Nachteile:**
- Load-Switch = ein Bauteil mehr; Sequencing evtl. in Firmware/Hardware zu berücksichtigen.

**Gefahren:**
- Wird die Regel „nur eine Quelle" nicht eingehalten, riskierst du bei jedem Bring-up ein weiteres Modul. **Wiederholter Latch-up kann ein Modul endgültig zerstören.**

---

## Empfohlenes Minimalpaket (wenn nicht alles auf einmal geht)

Diese vier bringen den größten Nutzen bei kleinstem Aufwand und adressieren die beobachteten Ausfälle direkt:

1. **Entkopplung** (100 nF/Pin + Bulk-Elko) — gegen Brownout/Trägheit. *(Maßnahme 1)*
2. **Serienwiderstände** in die Modul-Eingänge — gegen Latch-up/Back-Power. *(Maßnahme 3)*
3. **Stabile Modul-Versorgung** (eigener LDO) — Wurzel vieler Symptome. *(Maßnahme 4)*
4. **Betriebsregel „nur eine Quelle" + Sequencing** — sofort umsetzbar, kostet nichts. *(Maßnahme 8)*

Quarz (2), Antennen-Layout (5), CTS/RTS-Pfad (6) und die Pull-Widerstände (7) sind die nächste Stufe für dauerhafte Serienreife.

---

## Pin-Abgleich mit Datenblatt — NUR Serienboard (integrierte Antenne, 453-00171, Table 21)

> Wichtig: Die Pin-Nummern der **integrierten Antennenvariante** (Serienboard) unterscheiden sich
> von der **MHF4-Variante** (Datenblatt Abschnitt 9.3). Die folgende Tabelle gilt ausschließlich
> für das **Serienmodul (Table 21, Abschnitt 10.3)**. Jeder Pin ist mit allen im Umlauf befindlichen
> Namen aufgeführt.

Namens-Systeme: **Pin#** = Modul-Pin (Serienboard) · **Datenblatt-Name** = Spalte „Name" (meist
CYW20820-GPIO oder Pad-Name) · **EZ-Serial** = „EZ-Serial FW Default Function" · **CYW20820** =
SoC-GPIO / Alt-Funktion · **Board/Doku** = unsere Netz-/Doku-Namen.

| Pin# | Datenblatt-Name | EZ-Serial-Funktion | Board/Projekt-Doku | Rolle |
|---|---|---|---|---|---|
| 2  | VDDIO      | —                        | VDDIO                    | Digital-I/O-Versorgung 1,71–3,3 V |
| 4  | XTALI_32K  | XTALI_32K                | XTALI_32K                | 32,768-kHz-Quarz Eingang |
| 5  | XTALO_32K  | XTALO_32K                | XTALO_32K                | 32,768-kHz-Quarz Ausgang |
| 12 | P13        | CYSPP/SPP                | CYSPP (BLT-CYSPP)        | Modus/Status: LOW=Datenmodus, HIGH=Command-Mode |
| 14 | P10        | P_UART_CTS *(optional)*  | Modul-CTS / BLT-CTS      | Flow-Control-Eingang (von LPC-RTS) |
| 15 | P37        | P_UART_RXD *(REQUIRED)*  | Modul-RXD / BLT-RXD      | Dateneingang (von LPC-TXD) |
| 16 | P11        | P_UART_RTS *(optional)*  | Modul-RTS / BLT-RTS      | Flow-Control-Ausgang (zu LPC-CTS) |
| 17 | P32        | P_UART_TXD *(REQUIRED)*  | Modul-TXD / BLT-TXD      | Datenausgang (zu LPC-RXD) |
| 22 | 3P0V       | —                        | „VBAT" (Board-/DVK-Name) | Hauptversorgung 3,0 V (interne Regler) |
| 24 | BT_UART_RTS| HCI_UART_RTS             | (nur Flashen)            | HCI-UART RTS — nur Firmware-Flashen |
| 25 | BT_UART_TXD| HCI_UART_TXD             | (nur Flashen)            | HCI-UART TXD — nur Firmware-Flashen |
| 26 | BT_UART_RXD| HCI_UART_RXD             | (nur Flashen)            | HCI-UART RXD — nur Firmware-Flashen |
| 27 | BT_UART_CTS| HCI_UART_CTS             | (nur Flashen)            | HCI-UART CTS — nur Firmware-Flashen |
| 33 | P27        | Connection Indication GPIO|(CONNECTION)             | Verbindungsanzeige (optional) |
| 34 | RST        | RST_L                    | RST_L                    | Aktiv-Low System-Reset (fürs Flashen nötig) |
| 18 | P14        | GPIO for peripheral RST_L| —                        | **ACHTUNG: NICHT der Modul-Reset** (nur GPIO) |
| 1,3,21,23,38–54 | GND | — | — | GND | Masse |

### Ergebnis des Abgleichs (Maßnahme für Maßnahme)

- **Maßnahme 1 (Entkopplung):** Versorgungspins am Serienboard sind **VDDIO = Pin 2** und
  **3P0V = Pin 22**. Es gibt **keinen separaten VBAT-Pin** — „VBAT" (Spec-/DVK-Name) = **3P0V (Pin 22)**.
  Textstelle oben entsprechend korrigiert.
- **Maßnahme 2 (Quarz):** **XTALI_32K = Pin 4**, **XTALO_32K = Pin 5** — bestätigt (Table 21 + Figure 14).
- **Maßnahme 3 / 6 (Signal-/Flow-Leitungen):** Unser Board nutzt die **PUART** (nicht die HCI-UART):
  **P_UART_TXD = Pin 17 (P32)**, **P_UART_RXD = Pin 15 (P37)**, **P_UART_RTS = Pin 16 (P11)**,
  **P_UART_CTS = Pin 14 (P10)**. RXD (15) und TXD (17) sind Pflicht; RTS/CTS optional und nur wirksam,
  wenn RTS/CTS in der EZ-Serial-FW aktiviert ist (bei uns F=01). Zu unserer Doku konsistent
  (TXD=P32, RXD=P37, RTS=P11, CTS=P10).
- **Maßnahme 7 (CYSPP-Pull, RXD-Pull-up):** **CYSPP = Pin 12 (P13)**, „CYSPP/SPP". Der zu ziehende
  RXD ist **P_UART_RXD = Pin 15 (P37)**. Datenblatt bestätigt: CYSPP LOW = Datenmodus, HIGH = Command-Mode.
- **Reset:** Der Modul-Reset ist **Pin 34 (Name RST / EZ-Serial RST_L)** — **nicht** Pin 18 (P14,
  „GPIO for peripheral RST_L", nur ein GPIO). Beim Flashen wird Pin 34 gebraucht.
- **HCI-UART (nur Flashen):** BT_UART_RTS/TXD/RXD/CTS = **Pin 24/25/26/27** (= HCI_UART_*). Im Normalbetrieb ungenutzt.

### Offener Punkt (MCU-Seite, außerhalb des IF820-Datenblatts)

Für die **MCU-Seite (LPC1766)** gibt es in unseren Unterlagen widersprüchliche Angaben zum RTS-Pin
(`BT-Evalkit-Tasks.md`: „P1.15 RTS" vs. `project-status`/Doku: „RTS1 = P2.7"; CTS1 = P2.2). Das
betrifft nicht das IF820, sollte aber am Schaltplan/LPC verifiziert und vereinheitlicht werden.

## Ergänzender Software-Hebel (kein HW-Punkt, aber verwandt)

Unabhängig von der Hardware bleibt die **Modul-Firmware 1.4.18.18** empfehlenswert (behebt „Lockup unter UART-Last", verbessert SPP-Durchsatz, ermöglicht CYSPP-HIGH-Command-Mode und sauberes `/DIS`). HW-Maßnahmen und Firmware-Update ergänzen sich.
