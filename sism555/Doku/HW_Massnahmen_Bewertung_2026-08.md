# HW-Maßnahmen IF820 — Bewertung mit Wahrscheinlichkeiten

> Bewertung der Hardware-Maßnahmen aus `HW_Massnahmen_IF820.md` gegen die Primärquellen
> (Datenblatt `EZ-DS-Vela IF820_v2_2.pdf`, `EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf`) und
> gegen den Schaltplan `Mainboard-V2-A1_13_07_2026.sch`.
> Jede Maßnahme mit **Wahrscheinlichkeit**, dass sie das genannte Symptom behebt.
> Angelegt: 2026-08-05. Stand Firmware: 5.59.

---

## 0. Der entscheidende Fund: R77 ist nicht bestückt

Der Schaltplan hat auf der CYSPP-Leitung **drei** Bauteile, nicht eines:

| Bauteil | Wert | Lage | Bestückt in aktueller Variante? |
|---|---|---|---|
| **R77** | **10 kΩ** | IC36 IO7 (Net `N$133`) → **GND** | **NEIN** |
| C77 | 10 nF | IC36 IO7 (Net `N$133`) → GND | ja |
| R78 | 100 Ω | IC36 IO7 → `BLT-LINK` → IC37 P13 | ja |

Die aktuelle Bestückungsvariante ist `Abwärtskompatibel mit LTE` (im Eagle-File mit
`current="yes"` markiert). In dieser Variante ist **R77 explizit als `populate="no"`
eingetragen** — der Pull-down-Widerstand auf der CYSPP-Leitung fehlt also.

**Das Pad existiert bereits auf der Platine.** Es muss nichts getrennt und kein Draht
gezogen werden — nur ein 0603-Widerstand eingelötet.

### Warum das die BLE-Ursache sein kann

`bt_release()` konfiguriert IC36 IO7 als **Eingang**. Der PCA9554 hat in diesem Zustand
laut TI-Datenblatt einen **schwachen internen Pull-up von typ. 100 kΩ nach VCC**. Ohne R77
gibt es keinen konkurrierenden DC-Pfad → die Leitung liegt auf **HIGH**.

HIGH bedeutet laut EZ-Serial-Guide (S. 15) exakt: **Command-Mode / Datenkanal beenden.**

> `CYSPP | Input/output | CYSPP mode control. Assert (LOW) for CYSPP data mode,
> de-assert (HIGH) for command mode.`

Damit passt das App-Symptom lückenlos: Die BLE-Verbindung steht, das Subscribe auf `…ca102`
öffnet die CYSPP-Datenpipe, das Modul liest den Pin, sieht HIGH → Datenkanal wird sofort
wieder abgebaut → `⚡`.

Dass die frühere Messung `bt_cyspp_monitor()` im Leerlauf **HIGH** zeigte, ist genau dieser
100-kΩ-Pull-up des PCA9554 — nicht das Modul.

**Wahrscheinlichkeit, dass R77 (bzw. der fehlende definierte LOW-Ruhepegel) die Ursache des
BLE-Abbruchs ist: 60 %.**

Restunsicherheit: Warum funktioniert Classic SPP dann? Der Guide formuliert die beiden Fälle
unterschiedlich — bei SPP wird der Pin „**set to** LOW by EZ-Serial firmware" (getriebener
Ausgang, überfährt 100 kΩ mühelos), bei CYSPP „EZ-Serial **pulls** this pin" (schwächer). Die
Asymmetrie ist plausibel, aber nicht belegt. Deshalb 60 % und nicht 90 %.

Zweitkandidat: **C77 (10 nF)** auf der Leitung. Zieht das Modul nur schwach LOW, muss es
10 nF entladen; bei einem internen Pull von ~40 kΩ sind das ~400 µs. Sampelt das Modul früher,
liest es HIGH. **Wahrscheinlichkeit: 15 %.**

### Nebenwirkung: BT_LINK-Erkennung bricht

Mit R77 bestückt liest IC36 Bit 7 **immer LOW**. Da `BT_LINK` low-aktiv ist, meldet
`communication_change()` dann dauerhaft „verbunden". R77 allein ist deshalb **kein**
vollständiger Fix — die Verbindungserkennung muss vorher entkoppelt werden (siehe Maßnahme 2).

---

## 1. Korrektur: Die Bus-Contention-Sorge war um Faktor 1000 falsch

Die frühere Ablehnung von S2 („Bit 7 dauerhaft LOW treiben") beruhte auf der Annahme, das
Modul treibe P13 aktiv HIGH → 3,3 V / 100 Ω = 33 mA.

Der Guide (S. 25, §2.4.7.8) sagt jedoch ausdrücklich:

> „EZ-Serial **pulls** this pin to the appropriate logic level based on internal CYSPP state
> changes when CYSPP is enabled, but **you can override the pulled state with an external host
> or hardware design feature**."

Der Hersteller sieht das Übersteuern durch den Host also **ausdrücklich vor**. Dazu passt das
Datenblatt: P13 ist mit „Pin When Not Used = **floating**" gelistet, **ohne** dokumentierten
internen Pull.

Wenn das im Leerlauf gemessene HIGH vom PCA9554-Pull-up kommt, kostet aktives LOW-Treiben
**33 µA statt 33 mA**.

**Wahrscheinlichkeit, dass aktives LOW-Treiben von IO7 elektrisch unkritisch ist: 80 %.**

→ **Messung, die das in 2 Minuten klärt:** IO7 als Input konfigurieren, dann 4,7 kΩ von der
CYSPP-Leitung nach GND halten und die Spannung messen.
- Bricht sie auf **< 0,5 V** ein → HIGH kam vom PCA9554 → keine Contention, LOW-Treiben ist frei.
- Bleibt sie **> 2 V** → das Modul treibt aktiv HIGH → Finger weg vom LOW-Treiben.

---

## 2. P27 (CONNECTION, Pin 33) → IC36 IO5 — **empfohlen**

**Was:** IF820 Pin 33 auf IC36 Pad 10 (IO5). GND-Leiterbahn zu Pad 10 trennen, 10 kΩ Pull-up
nach 3,3 V, optional 100 nF nach GND.

**Warum:** Löst die **Doppelrolle** von Bit 7 auf. Danach ist Bit 7 reiner Steuerausgang
(CYSPP) und Bit 5 reiner Eingang (Verbindungsstatus). Beide vorherigen Fixes (R77 bestücken
*oder* aktiv LOW treiben) werden erst dadurch überhaupt gefahrlos möglich.

**Belege aus den Primärquellen:**
- Guide S. 74 (§4.2.3): „use the module's CONNECTION pin. This signal is asserted (LOW) **only
  when the CYSPP data pipe is fully established**." — besser als ein Link-Layer-Signal: es meldet
  genau den Zustand, den wir brauchen.
- Guide S. 73 (§4.1.3): „Monitor the CONNECTION signal for a simple indicator of **BLE/BT**
  connectivity … especially helpful when using **CYSPP/SPP** mode." — gilt für beide Transporte.
- Datenblatt S. 31: P27 ist echter Push-Pull-Ausgang mit **16 mA Sink**, kein Open-Drain.
  Ein externer Pull-up ist damit **nicht zwingend** nötig, aber unschädlich und definiert den
  Pegel, solange das Modul noch nicht gebootet ist. Guide S. 73 empfiehlt „high-impedance
  digital input" auf Host-Seite — genau ein PCA9554-Eingang.

**Wahrscheinlichkeit, dass P27 ein brauchbares Connect-Signal für BLE *und* Classic liefert: 85 %.**

**Wahrscheinlichkeit, dass IO5 dafür frei ist: 98 %.** Nachweis: `MQTT_LINK (1<<5)` wird in
`communication_change()` **nie** aus `get_com_status()` gelesen; der Bit wird ausschließlich
softwareseitig in `mqtt.c` (Z. 1438/1474/1491) und `libtool.c` (Z. 564/607) gesetzt und gelöscht.
Im Schaltplan hängt `IC36 IO5` am GND-Netz.

**Gefahr:** Das Trennen der GND-Leiterbahn ist der einzige irreversible Schritt. Vorher mit dem
Multimeter sicherstellen, dass Pad 10 danach wirklich keinen GND-Durchgang mehr hat.

---

## 3. Entkopplung 100 nF pro Power-Pin + Bulk — **C128 und C58 sind nicht bestückt!**

Der Schaltplan zeigt am Versorgungsnetz `N$169` (IC37 3P0V **und** VDDIO — beide am selben Netz):

| Bauteil | Wert | Bestückt? |
|---|---|---|
| C128 | 1 µF | **NEIN** (in beiden Varianten) |
| C58 | 1 µF | **NEIN** (in beiden Varianten) |
| R107 | 0 Ω | **NEIN** (in beiden Varianten) |

Das Datenblatt ist an der Stelle unmissverständlich (S. 34, §12.1):

> „Be sure to put a 100nF capacitor on **EACH** power pin. Also, place that capacitor to the pin
> **as close as possible** to make sure the internal PMU working correctly."

und S. 33: „It is recommended to put bypass capacitors 100nF in the 3P0V (pin22) and VDDIO (pin2)".

Wenn C128/C58 tatsächlich fehlen, hat das Modul **keine lokale Entkopplung**. Das ist der
plausibelste Kandidat für „läuft ~1 min, dann wird alles träge" und für die Latch-up-Signatur.

Stromaufnahme laut Datenblatt Tabelle 12 (integrierte Antenne): TX 2DH3/2DH5 **20,4 mA**,
LE1M 16,4 mA — aber ausdrücklich „measured at **average** of the TX-on time". Ein Peak-Wert
wird nicht angegeben, die Spitzen liegen deutlich höher.

**Wahrscheinlichkeit, dass Entkopplung „träge nach 1 min" behebt: 55 %.**
**Wahrscheinlichkeit, dass die Maßnahme richtig und ohne Risiko ist: 100 %** — sie ist vom
Hersteller vorgeschrieben. Sie gehört gemacht, unabhängig vom Symptom.

**Empfehlung:** 100 nF direkt an Pin 2 und Pin 22, zusätzlich 10–47 µF Bulk am Netz `N$169`.
C128/C58-Pads nutzen, wenn sie nah genug am Modul liegen.

---

## 4. Pull-up auf IF820-RXD (P37, Pin 15) — **sehr empfohlen**

**Symptom:** 0xFF-Junk am BT-Terminal, sobald GSM initialisiert (project-status Update 5.48).

**Mechanismus:** Der MUX IC18 schaltet `BLT-RXD` weg vom BT → die Modul-RXD floatet → das
Modul interpretiert Leitungsrauschen als Daten und reicht sie transparent über Bluetooth weiter.

**Was:** 10–100 kΩ von `BLT-RXD` nach VDDIO. **Pull-up, nicht Pull-down** — ein Pull-down
erzeugt Dauer-Break (0x00 + Framing-Fehler), also genau das Gegenteil.

**Wahrscheinlichkeit, dass das den Junk beseitigt: 90 %.** Der Mechanismus ist eindeutig und
die Maßnahme wirkt firmware-unabhängig (die Software-Alternative braucht Modul-FW 1.4.18.18).

---

## 5. Serienwiderstände 330 Ω – 1 kΩ in die Modul-Eingänge

**Was:** Je ein Serienwiderstand in Modul-RXD (P37), Modul-CTS (P10), CYSPP (P13 — dort sitzt
mit R78 = 100 Ω schon einer, ggf. auf 470 Ω erhöhen).

**Warum:** Begrenzt den Rückstrom über die IO-/ESD-Dioden, wenn der LPC Pegel treibt während
VDDIO/3P0V fehlt oder einbricht → Back-Powering/Latch-up.

**Belegstärke:** Das Datenblatt enthält **keine** Latch-up- oder ESD-Spezifikation und keine
Aussage zum Treiben unversorgter Pins. Die einzige ableitbare Regel ist die Fußnote auf S. 10
(„Maximum rating for signals follows the supply domain of the signals") — die bei abgeschaltetem
VDDIO zwangsläufig verletzt wird. Die Maßnahme ist also allgemeine gute Praxis, nicht
herstellerdokumentiert.

**Wahrscheinlichkeit, dass sie weitere Modulschäden verhindert: 65 %.**
**Wahrscheinlichkeit, dass sie bei 460800 Baud unschädlich ist: 95 %** — 1 kΩ × ~30 pF ≈ 30 ns
gegenüber 2,17 µs Bitzeit.

---

## 6. 32,768-kHz-Quarz Q3 bestücken — **NICHT jetzt**

Der Schaltplan hat den Quarz **Q3 (QTC332.76812B2R)** mit C129/C137 = 22 pF vorgesehen — alle
drei **in beiden Varianten nicht bestückt**.

Das Datenblatt (S. 33) empfiehlt den Quarz nachdrücklich („if not, **link issue may be
occurred**"), nennt aber **keine ppm-Angabe** für den internen LPO — die Aussage ist damit nicht
quantifiziert.

**Der Haken:** Laut `Ezurio_PCN #7C-2024` ist ab Modul-FW 1.4.16.16 der **interne LPO die
Werkseinstellung**. Ein bestückter Quarz bringt nur etwas, wenn gleichzeitig die passende
(nicht-int-lpo) Modul-Firmware geflasht wird. Falsche Paarung ist schlechter als kein Quarz.

**Wahrscheinlichkeit, dass Quarz allein (ohne FW-Wechsel) eine Verbesserung bringt: 15 %.**
**Mit passender FW-Variante: 40 %.**

Anmerkung: 22 pF passt zu einem Quarz mit CL ≈ 12,5 pF und widerspricht dem Datenblatt-Beispiel
(6 pF) nur scheinbar — das Datenblatt zeigt einen anderen Quarztyp. Kein Fehler im Schaltplan.

**Einordnung:** Zurückstellen bis Entkopplung und CYSPP geklärt sind.

---

## 7. Eigener LDO für das BT-Teil

**Wahrscheinlichkeit, dass er die verbleibenden Instabilitäten behebt: 50 %.**

Begründung für die mittlere Zahl: Das DVK mit eigener Versorgung läuft, das Onboard-Modul nicht
— ein starkes Indiz. Aber solange **C128/C58 fehlen** (Maßnahme 3), ist unklar, ob überhaupt die
Schiene das Problem ist oder nur die fehlende lokale Pufferung. **Erst Maßnahme 3, dann neu
bewerten.** Ein LDO ist die teurere Antwort auf möglicherweise dieselbe Ursache.

---

## 8. Antennen-Keep-out

Datenblatt §12.2 (S. 35) nennt nur **zwei** Regeln:

> 1. „Align module edge with PCB edge."
> 2. „To maintain antenna radiation efficiency, keep at least **15 mm** width of the GND plane
>    to the sides the module."

Dazu §12.1: keine Bauteile und keine High-Speed-Leitungen unter dem Funkteil, kein Metall über
der Antenne. Eine numerische Keep-out-Fläche **vor** der Antenne wird **nicht** angegeben.

**Wahrscheinlichkeit, dass das Layout die Ursache des Ausfalls auf 40 cm ist: 45 %.**

Der Ausfall auf 40 cm ist auffällig — aber Brownout unter TX-Last (Maßnahme 3) erzeugt dasselbe
Bild, und beide Effekte verstärken sich (schlechte Abstrahlung → mehr Retransmits → mehr
TX-Strom → mehr Einbruch). **Erst Maßnahme 3 und 4 abarbeiten, dann Reichweite neu messen.**
Ein Redesign ist zu teuer für eine 45-%-Vermutung.

---

## 9. Modul-Firmware 1.4.18.18 + HCI-Header

Die Onboard-Module laufen auf 1.4.12.12. `PCN #2E-2026` behebt „a lockup issue due in heavy UART
loading" und verbessert den SPP-Durchsatz.

**Voraussetzung Hardware:** HCI-UART (Pins 24/25/26/27) **und** Reset (Pin 34) müssen erreichbar
sein, Flow Control ist beim Flashen zwingend, und **CTS muss beim Reset LOW sein** (Datenblatt
§11, S. 34, 115200 8N1). Zum Reset-Pfad siehe Abschnitt 9a — er ist geklärt; die HCI-UART-Pins
sind weiterhin offen.

**Wahrscheinlichkeit, dass 1.4.18.18 die Lockups unter UART-Last behebt: 60 %.**

---

## 9a. RST_L (Pin 34) — auf dem Mainboard an nichts angeschlossen

### Was der Pin ist

Reiner **Hardware-Systemreset des Chips**. Eingang, aktiv LOW, mit **internem Pull-up**.
Er startet das Modul neu, löscht aber **keine** Konfiguration — ein Werksreset ist
ausschließlich Software (`system_factory_reset` / `/RFAC`, Guide S. 37 und S. 92).
Nach einem Reset sendet EZ-Serial das Boot-Event `system_boot (BOOT, ID=2/1)` (Guide S. 13).

**Nicht verwechseln mit P14** („GPIO for peripheral RST_L", MHF4 Pin 28). Das ist ein
**Ausgang**, um externe Peripherie zurückzusetzen — nicht der Modulreset.

### Belegstellen

Datenblatt Tabelle 21, **S. 31** (integrierte Antenne — unsere Variante), Zeile wörtlich:

> `34 | RST | RST_L | floating | I | VDDIO | Active-low system reset with internal pull-up resistor.`
> `NOTE - Required for flashing EZ-Serial and HCI firmware.`

Dieselbe Zeile in Tabelle 18, **S. 22**, für die MHF4-Variante mit **Pin 24**.
Spaltenüberschriften auf S. 20: *Pin # | Name | EZ-Serial FW Default Function | **Pin When Not
Used** | I/O | Voltage Ref. | Description*.

### Wie muss er beschaltet werden? Gar nicht

Die Spalte „Pin When Not Used" sagt **`floating`** — das Datenblatt erlaubt ausdrücklich, den
Pin offen zu lassen. Die Referenzschaltungen **§9.4 (S. 25)** und **§10.4 (S. 33)** nennen je
nur drei Punkte (100 nF an 3P0V/VDDIO, 32,768-kHz-Quarz, „BT_UART signals (x4) are required for
flashing") — **RST kommt dort nicht vor.** In keinem der drei Herstellerdokumente gibt es eine
Empfehlung für externen Pull-up, RC-Glied oder Host-GPIO.

**Timing: existiert nicht.** Keine minimale Pulsbreite, keine Anstiegszeit, keine
Boot-Verzögerung — in keinem der drei PDFs. Einzige verwandte Zahl: Guide S. 38,
„Short delay while chipset reset and boot process occurs, **~150 ms**" (nach `/RFAC`).

### Wann er gebraucht wird: beim Flashen

Datenblatt **§11, S. 34**, wörtlich:

> • Flashing uses BT_UART_TXD, BT_UART_RXD, BT_UART_CTS, BT_UART_RTS
> • Flow control is mandatory.
> • **Reset line is also required to be able to enter programming mode.**
> • UART settings are 1152008N1
> • **CTS needs to be low during a reset to enter programming mode.**

### Befund am Mainboard V2

Das Netz `N$160` enthält **genau zwei** Anschlüsse: `IC37.RST` und `R124` Pad 1.
R124 (10 kΩ) führt auf `N$169`, also die Modulversorgung — es wäre ein externer Pull-up.
**R124 ist in beiden Bestückungsvarianten als `populate="no"` eingetragen.**

Damit hängt RST_L auf dem Mainboard an **nichts**. Zwei Konsequenzen:

**1. Der LPC1766 kann das Modul nicht zurücksetzen.** Es existiert kein Pfad vom MCU zum
Reset-Pin. Das erklärt den Eintrag in `project-status.md`, dass bei einem hängenden Modul nur
ein echter Power-Cycle des ganzen Geräts hilft und ein Reflash des LPC das IF820 nicht mitnimmt.
Zusammen mit „das IF820 hat keinen Enable-Pin" ist das der Grund für den offenen Backlog-Punkt
„Power/Enable-Konzept".

**2. Das freie R124-Pad ist der Zugang zu RST.** Pad 1 liegt auf `N$160`. Wer onboard flashen
oder gezielt einen Reset auslösen will, lötet dort an — kein Feinpitch am Modul nötig.
Ein bestücktes R124 (10 kΩ nach VDDIO) schadet nicht, ist aber nicht erforderlich, weil der
interne Pull-up laut Datenblatt vorhanden ist.

**Empfehlung:** Wenn ohnehin am Board gearbeitet wird, RST über das R124-Pad auf einen freien
LPC-GPIO führen. Das macht das Modul aus der Firmware heraus resetbar und beseitigt die
Abhängigkeit vom Power-Cycle des Gesamtgeräts.
**Wahrscheinlichkeit, dass das die „Modul hängt, hilft nur stromlos"-Fälle auflöst: 75 %.**

### Am DVK (`EZ-GUIDE-DVK-IF820_v2_0.pdf`)

| Fundstelle | Inhalt |
|---|---|
| S. 10, Tabelle 4 | `J3.5 → Modulpin 34 → RST_L → RP2040 und Reset-Taster (SW301)` (Antennenvariante) |
| S. 9, Tabelle 3 | `J2.23 → Modulpin 24 → RST_L` (MHF4-Variante) |
| S. 8, Tabelle 2 | `SW301 Reset button for Vela IF820`; `SW2 Recovery button, used in combination with the reset button to ensure user can program the Vela IF820 over the HCI UART` |
| S. 9 | `J2.11 / 36 / BT_UART_CTS / RP2040 and recovery button (SW2)` — SW2 setzt CTS LOW, genau die Regel „CTS low during reset" |
| S. 16, Tabelle 6 | mikroBUS `J201.2 RST_L`, Jumper auf J3 Pin 5–6 (Antennenvariante) |

**Kein dokumentiertes Pin-Kombination-Werksreset beim Booten.** Guide S. 37 erwähnt „a
hardware-triggered factory reset" beiläufig, definiert dafür aber nirgends einen Pin.
Ein Firmware-Reflash setzt dagegen alles zurück — Guide S. 67: „Updating firmware via this
method **always returns to factory default settings** and removes any bonding data and custom
GATT structure."

---

## 10. Was die Doku als Option nennt, was aber nachweislich NICHT geht

Damit kein Aufwand in Sackgassen läuft:

- **CYSPP per Konfiguration abschalten und rein per Software fahren: geht nicht.**
  Guide S. 22: „Externally asserting (LOW) the CYSPP pin will **always** begin CYSPP operation,
  **even if the profile has been disabled**". Und umgekehrt, S. 26/38: Pin HIGH + E=1 oder E=2 →
  „CYSPP data from the remote host is **never** raw/transparent unless the host asserts the CYSPP
  pin." Es gibt in `.CYSPPSP` **keinen** Parameter, der die Pin-Abhängigkeit aufhebt.
  **Sicherheit: 95 %** (zwei unabhängige Belegstellen).

- **GPIO-Funktionen umbelegen (CONNECTION auf einen anderen Pin, CYSPP-Funktion abschalten):
  geht in EZ-Serial nicht.** Die GPIO-Kommandogruppe (ID 9) enthält nur `/QADC`, `SIOD`, `GIOD`,
  `SIOL`, `GIOL`; `SPWM`/`GPWM` sind „Not implemented". Guide S. 13: „All GPIO pins are
  **pre-defined** with EZ-Serial firmware." Die im Datenblatt erwähnte Supermux-Umbelegung gilt
  für rohe CYW20820-SDK-Firmware, nicht für EZ-Serial. **Sicherheit: 90 %.**

  → Damit ist **S1** (`.CYSPPSP E=01` + manuelles `.CYSPPS`) als Lösung des Pin-Problems
  entwertet: auch bei E=01 entscheidet am Ende der Pin-Pegel.

---

## Empfohlene Reihenfolge

| # | Maßnahme | Aufwand | Wahrscheinlichkeit | Warum zuerst |
|---|---|---|---|---|
| 1 | **Messung** CYSPP-Pegelquelle (4,7 kΩ nach GND) | 2 min | — | Entscheidet, ob LOW-Treiben erlaubt ist |
| 2 | **Bestückung verifizieren** (R77, C128, C58, Q3) | 15 min | — | Eagle-Variantenflags sind nicht immer gepflegt |
| 3 | **C128/C58 bzw. 100 nF + Bulk** am Modul | 3 Bauteile | 55 % gegen „träge" / 100 % korrekt | Herstellervorschrift, Wurzel mehrerer Symptome |
| 4 | **P27 → IC36 IO5** | Draht + 1–2 Bauteile, 1 Trennung | 85 % | Voraussetzung für alles Weitere am CYSPP |
| 5 | **R77 bestücken** *oder* IO7 aktiv LOW treiben | 1 Bauteil / 4 Zeilen Code | 60 % gegen BLE-Abbruch | Der eigentliche BLE-Fix |
| 6 | **Pull-up auf BLT-RXD** | 1 Bauteil | 90 % gegen 0xFF-Junk | Billig, klar verstanden |
| 7 | Serienwiderstände Modul-Eingänge | 2–3 Bauteile | 65 % gegen Latch-up | Vorsorge |
| 8 | Reichweite neu messen → dann über Antenne entscheiden | — | 45 % | Erst nach 3 sinnvoll bewertbar |
| 9 | Modul-FW 1.4.18.18 (HCI-Header prüfen) | offen | 60 % | Aufwand hängt an Header-Verfügbarkeit |
| — | Quarz Q3 | 3 Bauteile + FW | 15 % ohne FW-Wechsel | zurückstellen |
| — | Eigener LDO | Redesign | 50 % | erst nach 3 neu bewerten |

**Betriebsregel, kostet nichts, sofort gültig:** immer nur **eine** Versorgungsquelle — entweder
DVK-USB **oder** Board, nie beides; beim Umstecken das Modul stromlos machen. Das hat bereits
zwei Module und ein DVK beschädigt. **Wahrscheinlichkeit, dass die Regel weitere Schäden
verhindert: 90 %.**

---

## Quellen

- `Documentation/EZ-DS-Vela IF820_v2_2.pdf` — Tabellen 4/5/6 (S. 10), Tabelle 12 (S. 12),
  Pin-Tabelle 21 (S. 27–31), §10.4 + Figure 14 (S. 33), §11 (S. 34), §12.1/12.2 (S. 34–35)
- `Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf` — Pin-Tabelle (S. 15–16), §2.4.7.8
  (S. 25), Pin-/Enable-Wahrheitstabelle (S. 22, 26), §4.1.3/§4.2.1/§4.2.3 (S. 73–74),
  `.CYSPPSP` (S. 169–170), GPIO-Gruppe (S. 164–167), §8.1 (S. 216)
- `Documentation/Ezurio_PCN #7C-2024_…Internal LPO.pdf`, `Ezurio_PCN #2E-2026_…1.4.18.18.pdf`
- `Documentation/EZ-GUIDE-DVK-IF820_v2_0.pdf` — Tabelle 2 (S. 8), Tabelle 3 (S. 9),
  Tabelle 4 (S. 10), Tabelle 6 (S. 16)
- `Mainboard-V2-A1_13_07_2026.sch` — Netze `N$133`, `BLT-LINK`, `N$169`, `N$147`, `N$155`,
  `N$160`; Variantendefinition `Abwärtskompatibel mit LTE` (current)
- `sism555/Doku/HW_Massnahmen_IF820.md`, `sism555/project-status.md` (Updates 5.03, 5.14–5.16,
  5.41–5.43, 5.47–5.49, 5.51, 5.92), `sism555/Doku/BT_Teststrategie.md`,
  `sism555/Doku/IF820_Config_Kandidaten.md`, `sism555/Doku/ble-retest-1-negative.md`
- TI PCA9554 Datenblatt (Eingangsmodus: hochohmig mit schwachem Pull-up, typ. 100 kΩ)
