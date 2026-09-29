---
title: "IF820 — Pin-Referenz: was macht welcher Pin, wie nutzt man ihn richtig, wo steht das"
document_type: "Dokumentation"
subtitle: ""
version: "0.0.2"
date: "02.09.2026"
author: "KaiserVia"
classification: "Intern"
---
# IF820 — Pin-Referenz: was macht welcher Pin, wie nutzt man ihn richtig, wo steht das

> Kurzreferenz für die Steuer-, Status- und Versorgungspins des Vela IF820 unter
> **EZ-Serial-Firmware**. Pin-Nummern gelten für die **Variante mit integrierter Antenne**
> (Datenblatt Tabelle 21) — das ist unsere Bestückung. Die MHF4-Variante hat andere
> Nummern (Tabelle 18), die Funktionsnamen sind identisch.
>
> Quellenkürzel:
> **DS** = `Documentation/EZ-DS-Vela IF820_v2_2.pdf` (Datenblatt, elektrisch)
> **GD** = `Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf` (EZ-Serial-Firmware-Guide)
>
> Wichtig: **funktionale** Pin-Beschreibungen stehen im **Guide**, nicht im Datenblatt. Das
> Datenblatt liefert nur Pinnummer, Richtung, Spannungsdomäne und Treiberstärke.
>
> Angelegt: 2026-09-02.

---

## 1. `CYSPP` — P13, Pin 12 — Datenmodus ein/aus

**Was er macht:** Schaltet zwischen transparentem Datenmodus und Command-Mode. Low-aktiv.
**Ein einziger Pin, aber bidirektional (I/O)** — das ist der Kern aller Verwirrung:

- **Als Ausgang:** Das Modul zieht ihn selbst LOW, wenn ein transparenter Datenkanal steht
  (Classic SPP verbunden, bzw. BLE-CYSPP-Datenpipe nach dem Subscribe des Clients).
- **Als Eingang:** Der Host darf diesen Zustand übersteuern — LOW erzwingt Datenmodus,
  HIGH beendet ihn (bei Classic wird damit die SPP-Verbindung geschlossen).

**Richtig nutzen:**

- Will man den Datenmodus **einfach nur benutzen**: Leitung **hochohmig lassen** (Eingang, kein
  Treiber, kein fester Pull). Das Modul erledigt alles selbst.
- **Nie aktiv treiben, solange ein Datenkanal laufen soll** — weder HIGH noch LOW.
- Aktiv HIGH treiben ist ein **gezielter Trenn-Befehl**, kein neutraler Ruhezustand.
- **Messfalle:** hochohmig und aktiv HIGH sind beide als HIGH messbar, verhalten sich am Modul
  aber nachweislich unterschiedlich. Ein gemessenes HIGH beweist nichts.
- Der Pin **schlägt die Software-Konfiguration** — es gibt keinen `.CYSPPSP`-Parameter, der die
  Pin-Abhängigkeit aufhebt.

**Wo steht das:**

- DS Tabelle 21, **S. 28**: `12 | P13 | CYSPP/SPP | floating | I/O | VDDIO`, Fußnote 10:
  „CYSPP **Input/output**. CYSPP mode control. Assert (LOW) for CYSPP data mode, de-assert (HIGH)
  for command mode."
- GD **S. 15** (Pin-Funktionstabelle): „When SPP is established, CYSPP pin is **set to LOW by
  EZ-Serial firmware**. When host sets CYSPP pin to HIGH, **SPP connection will be closed**."
- GD **S. 25** (§2.4.7.8): „EZ-Serial ... **pulls this pin** to the appropriate logic level based
  on internal CYSPP state changes ... but you can **override the pulled state** with an external
  host or hardware design feature."
- GD **S. 26** (Tabelle Pin-Zustand × Konfiguration): floating + Autostart → „CYSPP pin **is
  pulled LOW** when this occurs"; extern HIGH → „CYSPP data ... is **never** raw/transparent
  unless the host asserts the CYSPP pin"; extern LOW → „Active **regardless of firmware
  configuration**".
- GD **S. 22**: „Externally asserting (LOW) the CYSPP pin will **always** begin CYSPP operation,
  even if the profile has been disabled."
- GD **S. 81** (§6.3): „...ensure that the external device is not also attempting to **drive or
  strongly pull the pin in the opposite direction** at the same time."

**Warum WIR ihn nutzen — der einzige Pin, den wir wirklich brauchen:**

- **Als Eingang (Hauptzweck): unsere komplette Verbindungserkennung.** Er ist unser `BT_LINK`.
  Daran hängen: Terminal-Routing auf BT, Ausgabe von Banner + PIN-Prompt, PIN-Rücksetzung beim
  Trennen, das 6-Minuten-Inaktivitätstimeout. Wir lesen ihn per I²C über IC54 Bit 7, getriggert
  vom EINT1-Interrupt des Port-Expanders.
- **Wir haben keine Alternative:** CONNECTION ist unverdrahtet (Punkt 2), und wir parsen die
  `@E,...`-Ereignisse des Moduls produktiv **nicht** (nur im Diagnose-Testmenü). Damit ist dieser
  Pin unsere einzige Informationsquelle über den Verbindungszustand — und zwar transport-neutral,
  d. h. die Firmware weiß nicht, ob Classic oder BLE anliegt.
- **Als Ausgang nutzen wir ihn an genau einer Stelle:** `bt_disconnect()` treibt ihn bewusst HIGH,
  um beim Inaktivitätstimeout die Verbindung zu beenden (danach `/DIS`, dann sofort wieder
  freigeben). Das ist der vom Hersteller vorgesehene Trennmechanismus.
- **Warum wir ihn sonst NICHT treiben:** Genau das war der BLE-Bug. Bis 5.65–5.87 trieben wir ihn
  an vier weiteren Stellen HIGH (Reconfig, UART-Umschaltung auf GSM/GPS, Deinstallieren) — jedes
  Mal war damit der BLE-Datenkanal tot.

**Auf unserem Board:** IC37 P13 → `BLT-LINK` → R78 (100 Ω) → IC54 (PCA9554) Bit 7. Bit 7 hat bei
uns die **Doppelrolle** Steuerausgang *und* Verbindungserkennung — Ursache des BLE-Bugs.
Zusätzlich hängen BT_DEV_WAKE und BT_HOST_WAKE an derselben Leitung (siehe Punkt 5/6).

---

## 2. `CONNECTION` — P27, Pin 33 — Statusausgang „Datenpipe steht"

**Was er macht:** **Ausgang** des Moduls. Geht LOW, sobald der CYSPP-Datenkanal *vollständig*
steht (Peer verbunden **und** GATT-Subscription abgeschlossen). Meldet also nicht nur „Link da",
sondern „du darfst jetzt Daten senden".

**Der Hersteller nennt zwei Verwendungen — das ist wichtig, weil oft nur die erste bekannt ist:**

1. **Als LED-Treiber** — im Beispiel „Dumb terminal host" (Host ohne eigene Logik/GPIOs):
   GD **S. 73** (§4.2.1): „Optional for connectivity status: Module CONNECTION pin **to LED
   (active LOW)**."
2. **Als echtes Host-Eingangssignal** — im Beispiel „Full API host" (Host-MCU, also unser Fall):
   GD **S. 73** (§4.1.1): „Module CONNECTION pins to **high-impedance digital input host GPIO**."

**Richtig nutzen (für einen Host mit MCU):**

- Als **reinen hochohmigen Eingang** am Host mitlesen — nie treiben.
- Das ist der vorgesehene Weg, um Verbindungserkennung **von** der CYSPP-Steuerung zu **trennen**.
  Wer beides über eine Leitung macht, bekommt genau unser Problem.
- Externer Pull-up ist nicht nötig (echter Push-Pull-Ausgang), definiert aber den Pegel, solange
  das Modul noch nicht gebootet hat.

**Wo steht das (alle sechs Stellen im Guide):**

- DS Tabelle 21, **S. 31**: `33 | P27 | Connection Indication GPIO`, Fußnote 20: „Connection
  Indicator pin in EZ-Serial FW"; Treiberstärke „Current: **16 mA sink**".
- GD **S. 22**: „the Host should **monitor the CONNECTION pin** to determine **when it is safe to
  begin sending data** from the Host for BLE transmission. Once the CONNECTION pin is asserted
  while the CYSPP pin is also asserted, the Host may send and receive data over CYSPP."
- GD **S. 23** (CYSPP-Ablauf, Schritt 4): „EZ-Serial ... will assert the CONNECTION pin,
  indicating that CYSPP is **ready to send and receive data**."
- GD **S. 38** (§3.2.1, Schritt 6a/6b): „**Monitor the CONNECTION pin** to detect when the remote
  peer has connected and GATT data subscription is complete. Once the CONNECTION pin goes low,
  **you can send and receive data**..."
- GD **S. 73** (§4.1.1): hochohmiger Host-Eingang (siehe oben) · (§4.2.1): LED-Variante
  (siehe oben) · außerdem: „Monitor the CONNECTION signal for a **simple indicator of BLE/BT
  connectivity** without needing to parse all possible API events ... especially helpful when
  using **CYSPP/SPP** mode."
- GD **S. 74** (§4.2.3): „If the host supports a simple **'enable' control line** for whether it
  is safe to send data, use the module's CONNECTION pin. This signal is asserted (LOW) **only
  when the CYSPP data pipe is fully established**."
- GD **S. 74** (§4.3.1): Empfehlung, CONNECTION beim Prototyping mindestens als **Testpad**
  zugänglich zu machen.

**⚠ Offener Punkt für Classic SPP:** Die präzise Zusage „asserted (LOW) only when the **CYSPP**
data pipe is fully established" (S. 74) ist CYSPP-spezifisch. S. 73 formuliert allgemeiner
„BLE/**BT** connectivity ... CYSPP/**SPP** mode", aber ein exakt spezifiziertes Verhalten für
Classic SPP steht in keinem der Dokumente. Falls CONNECTION je verdrahtet wird, muss das
Verhalten bei Classic **selbst gemessen** werden.

**Warum WIR ihn (noch) nicht nutzen:**

- **Er wäre die saubere Lösung für unser Kernproblem.** Mit CONNECTION als eigenem Statuseingang
  wäre CYSPP reiner Steuerausgang und die Doppelrolle von IC54 Bit 7 aufgelöst.
- **Zurückgestellt, weil die Firmware-Korrektur gereicht hat:** Seit wir CYSPP nicht mehr treiben,
  funktioniert die Erkennung über Bit 7 zuverlässig (12/12 Verbindungszyklen, 3 × 4,2 MB, siehe
  Testlauf 2026-08-26). Eine Hardware-Änderung wäre also Aufwand ohne aktuellen Nutzen.
- **Bleibt als Reserve:** Wenn die Verbindungserkennung je wieder klemmt — z. B. beim bekannten
  „Split-Brain" (Modul hält die Leitung, obwohl der Link tot ist) — ist das der erste Kandidat.
- Sekundärnutzen wäre eine **Verbindungs-LED** am Gerät (siehe LED-Variante oben); das ist bei uns
  aber kein Anforderungspunkt.

**Auf unserem Board:** **unverdrahtet** — im Schaltplan hat IC37 P27 überhaupt kein Netz.
Der Kandidat „P27 → IC54 IO5" (Maßnahme 2 in `HW_Massnahmen_Bewertung_2026-08.md`) ist
zurückgestellt.

---

## 3. `CP_ROLE` — P1, Pin 31 — Rolle Central oder Peripheral

**Was er macht:** **Eingang.** LOW = Central, HIGH = Peripheral. Offen/floating = die per
Software (`.CYSPPSP`) konfigurierte Rolle gilt.

**Richtig nutzen:**

- Für ein Gerät, das sich vom Handy/PC verbinden lässt (unser Fall): **offen lassen** oder HIGH.
  Der Software-Default ist ohnehin Peripheral.
- Achtung, gleiche Logik wie bei CYSPP: **wird der Pin extern getrieben, gewinnt der Pin gegen
  die Firmware-Einstellung.**

**Wo steht das:**

- DS Tabelle 21, **S. 31**: `31 | P1 | CP_ROLE | floating | I/O | VDDIO`.
- GD **S. 15**: „CP_ROLE | Input | CYSPP role control. Assert (LOW) for central mode, de-assert
  (HIGH) for peripheral mode."
- GD **S. 38** (§3.2.1): „If left floating, EZ-Serial ... will use the role configured in
  firmware ... uses the **peripheral** role with factory default settings."
- GD **S. 27** (§2.4.7.9, Zustandsdiagramm): „...if the host or hardware design drives it to a
  different state, CYSPP will operate in the **pin-defined** state and not the firmware-defined
  state."

**Warum WIR ihn nicht nutzen:** Unser Gerät ist immer die Seite, die sich **verbinden lässt**
(Handy/PC/App baut auf) — also Peripheral. Das ist der Software-Werksdefault des Moduls, wir
müssen dafür nichts tun. Ein Central-Betrieb (Gerät verbindet sich selbst zu etwas) ist in keinem
Anwendungsfall vorgesehen. **Auf unserem Board unverdrahtet** (kein Netz an IC37 P1) — das ist
korrekt so, weil „floating" genau „Software-Rolle gilt" bedeutet.

---

## 4. `LP_MODE` — P12, Pin 30 — Sleep erlauben / verhindern

**Was er macht:** **Eingang** und der eigentliche Energiespar-Steuerpin. LOW = Modul darf
schlafen, HIGH = Modul bleibt aktiv bzw. wacht auf. **Intern gepullt (Pull-up)** — offen
gelassen heißt also „kein Sleep".

**Richtig nutzen:**

- Wer **kein** Energiesparen braucht: offen lassen. Dann ist das Modul immer ansprechbar.
- Wer sparen will: LOW halten, und **vor** jeder Host→Modul-UART-Übertragung auf HIGH ziehen,
  danach wieder LOW. Zusätzlich das Sleep-Level per `SSLP` festlegen
  (0 = kein Sleep, 1 = normal, 2 = Deep Sleep).
- Der Pin **überstimmt** die Software-Einstellung: HIGH → Active Mode, egal was `SSLP` sagt.

**Wo steht das:**

- DS Tabelle 21, **S. 30**: `30 | P12 | LP_MODE | floating | I/O | VDDIO`.
- GD **S. 15**: „LP_MODE | Input | Low-power mode control. **Assert (LOW) to allow sleep,
  de-assert (HIGH) to disable sleep or exit sleep mode.** Note: The LP_MODE pin is internally
  pulled up."
- GD **S. 36** (Sleep-Logik): „If the LP_MODE pin is de-asserted to high, the module will
  **remain in Active mode**, otherwise select the lowest value (0 = no sleep, 1 = normal sleep,
  2 = deep sleep)..."
- GD **S. 37** (§3.1.5.3): „**Properly asserting and de-asserting this pin surrounding
  host-to-module UART transmissions** provides efficient power consumption while still allowing
  normal sleep at all other times."
- GD **S. 73** (§4.1.1): „Module CYSPP, and LP_MODE pins to **digital output** host GPIOs."

**Warum WIR ihn nicht nutzen:** Wir betreiben **kein** Energiesparen am BT-Modul. Das Gerät muss
jederzeit scanbar und verbindbar sein (Advertising läuft dauerhaft, `SAP ... O=0000` = unbegrenzt),
und unsere Firmware sendet **kein** `SSLP` — das Modul behält also seinen Werksdefault. Ein
Sleep-Handshake über LP_MODE würde nur zusätzliche Fehlerquellen ins UART-Timing bringen, ohne
messbaren Nutzen für unser Leistungsbudget. **Auf unserem Board unverdrahtet** (kein Netz an
IC37 P12) — durch den internen Pull-up liegt der Pin damit auf HIGH = „kein Sleep", was genau
unserem Wunsch entspricht.

---

## 5. `BT_HOST_WAKE` — Pin 29 — „ich bin wach" (Modul → Host)

**Was er macht:** **Ausgang** des Moduls. HIGH = Modul ist wach und kann Kommandos annehmen,
LOW = sonst.

**Richtig nutzen:**

- Nur relevant, wenn man Sleep überhaupt nutzt. Dann als **Eingang** am Host lesen (Polling oder
  Interrupt): nach dem Aufwecken über LP_MODE **auf HIGH warten**, statt eine feste Wartezeit
  einzubauen — dann erst senden.
- Wer kein Sleep nutzt, kann den Pin unbeschaltet lassen.
- Nicht treiben, nicht mit festen Pulls belasten (Ausgang des Moduls).

**Wo steht das:**

- DS Tabelle 21, **S. 30**: `29 | BT_HOST_WAKE | BT_HOST_WAKE19 | floating | **O** | VDDIO |
  A signal from the CYW20820 device to the host indicating that the Bluetooth device requires
  attention.`
- DS **S. 30**, Fußnote 19: „BT_HOST_WAKE is an **output** from the IF820. With EZ-Serial it is
  used for **low power operation**. When the IF820 is awake and ready to accept commands, this
  pin will be **high**, otherwise it will be **low**."

**Warum WIR ihn nicht nutzen:** Er ist nur im Zusammenspiel mit Sleep sinnvoll — und Sleep nutzen
wir nicht (Punkt 4). Zusätzlich wäre er bei uns **gar nicht isoliert nutzbar**, weil er über R138
(10 kΩ) an derselben Leitung wie CYSPP hängt: ein Host-GPIO würde dort nicht den reinen
Wake-Status, sondern auch die CYSPP-Aktivität mitlesen. Ein externer Pull auf diese Leitung ist
deshalb ausdrücklich zu vermeiden — er würde über die Kopplung auf CYSPP wirken.

**Auf unserem Board:** hängt über R138 (10 kΩ) am Netz `BLT-LINK`, also an derselben Leitung wie
CYSPP — nicht isoliert nutzbar.

---

## 6. `BT_DEV_WAKE` — Pin 28 — von EZ-Serial **nicht** benutzt

**Was er macht:** **Eingang**, laut Datenblatt „host requires attention". **Aber:** die Spalte
„EZ-Serial FW Default Function" steht auf **`NA`** — die EZ-Serial-Firmware wertet den Pin gar
nicht aus. Es ist eine reine CYW20820-Siliziumfunktion.

**Richtig nutzen:** **gar nicht.** Für Sleep/Wake ist `LP_MODE` der richtige Pin (Punkt 4). Pin
unbeschaltet lassen.

**Wo steht das:**

- DS Tabelle 21, **S. 30**: `28 | BT_DEV_WAKE | **NA** | floating | I | VDDIO | A signal from the
  host to the CYW20820 indicating that the host requires attention.`
- Kein einziger Treffer für `DEV_WAKE` im gesamten EZ-Serial-Guide — es gibt dort keine
  Funktionsbeschreibung und kein Kommando dazu.

**Warum WIR ihn nicht nutzen:** Weil es nichts zu nutzen gibt — die EZ-Serial-Firmware wertet ihn
nicht aus. Für Sleep/Wake wäre LP_MODE zuständig, und Sleep nutzen wir nicht. Er ist bei uns
trotzdem **elektrisch relevant**: über R139 (0 Ω) liegt er praktisch direkt auf der
CYSPP-Leitung, ist also nicht frei beschaltbar. Auch hier: **keinen externen Pull anbringen.**

**Auf unserem Board:** hängt über R139 (**0 Ω**, also Kurzschluss) am Netz `BLT-LINK` — praktisch
direkt an der CYSPP-Leitung. Siehe `HW_Massnahmen_Bewertung_2026-08.md`, Abschnitt 9b.

---

## 7. `RST` (RST_L) — Pin 34 — Hardware-Reset

**Was er macht:** **Eingang**, aktiv LOW, **interner Pull-up**. Startet den Chip neu. Löscht
**keine** Konfiguration — ein Werksreset ist ausschließlich Software (`/RFAC`).

**Richtig nutzen:**

- Beschaltung ist **nicht erforderlich** — Datenblatt erlaubt „floating".
- **Zwingend nötig ist er nur zum Flashen** der Modul-Firmware, und dort mit einer Zusatzregel:
  **CTS muss beim Reset LOW sein**, um in den Programmiermodus zu kommen.
- Wer ein hängendes Modul aus der Firmware heraus retten will, braucht ihn trotzdem — sonst hilft
  nur ein Power-Cycle des ganzen Geräts.
- **Kein Timing spezifiziert** (keine Mindest-Pulsbreite in irgendeinem der Dokumente). Einzige
  verwandte Zahl: „~150 ms" Boot-Dauer nach `/RFAC` (GD S. 38).

**Wo steht das:**

- DS Tabelle 21, **S. 31**: `34 | RST | RST_L | **floating** | I | VDDIO | Active-low system reset
  with **internal pull-up** resistor. NOTE - Required for flashing EZ-Serial and HCI firmware.`
- DS **§11, S. 34**: „Flashing uses BT_UART_TXD/RXD/CTS/RTS · Flow control is **mandatory** ·
  **Reset line is also required** to be able to enter programming mode · UART settings are
  115200 8N1 · **CTS needs to be low during a reset** to enter programming mode."
- DS Referenzschaltungen §9.4 / §10.4: RST kommt dort **nicht** vor.

**Warum WIR ihn nicht nutzen — und warum uns das weh tut:**

- Wir nutzen ihn **nicht**, weil er am Board an nichts hängt. Nicht aus Absicht, sondern weil das
  Datenblatt „floating" erlaubt und die Referenzschaltungen ihn nicht erwähnen.
- **Die Folge spüren wir aber:** Der LPC kann das Modul **nicht** zurücksetzen. Hängt das Modul,
  hilft nur ein Power-Cycle des **ganzen Geräts** — und ein Reflash des LPC nimmt das IF820 nicht
  mit. Das ist der Grund, warum der ungeklärte „Modul antwortet nicht mehr"-Restfall so
  unangenehm ist: es gibt keinen Software-Ausweg.
- **Zweite Folge:** Ohne RST-Zugang ist ein Onboard-Update der Modul-Firmware (z. B. auf
  1.4.18.18) unmöglich — Reset ist beim Flashen zwingend.
- **Nachrüstung wäre einfach:** Das freie R124-Pad liegt auf dem RST-Netz, kein Feinpitch am Modul
  nötig. RST von dort auf einen freien LPC-GPIO → Modul per Software resetbar.
  Geschätzte Wahrscheinlichkeit, dass das die „Modul hängt"-Fälle auflöst: **75 %**.

**Auf unserem Board:** an nichts angeschlossen (Netz `N$160` = nur `IC37.RST` + R124-Pad;
**R124 unbestückt**). Details in `HW_Massnahmen_Bewertung_2026-08.md`, Abschnitt 9a.

---

## 8. `P_UART` — Pins 14–17 — die Nutzdaten-UART

| Pin | Signal | Pflicht? |
|---|---|---|
| 15 | `P_UART_RXD` (P37) | **REQUIRED** für Command-/Datenmodus |
| 17 | `P_UART_TXD` (P32) | **REQUIRED** für Command-/Datenmodus |
| 14 | `P_UART_CTS` (P10) | optional — nur bei aktivierter Flow Control |
| 16 | `P_UART_RTS` (P11) | optional — nur bei aktivierter Flow Control |

**Richtig nutzen:**

- **Gekreuzt** verdrahten: Modul-TXD → Host-RXD, Modul-RXD → Host-TXD, Modul-CTS → Host-RTS,
  Modul-RTS → Host-CTS.
- Werksdefault ist **115200 8N1 ohne Flow Control**; Flow Control muss per `STU` erst aktiviert
  werden. Der Guide empfiehlt sie ausdrücklich, wenn der Host sie kann.
- Flow-Control-Leitungen sind **active-low** (`_N`-Suffix im Datenblatt).
- Wenn ein MUX diese Leitungen wegschaltet, floatet die Modul-RXD → das Modul reicht
  Leitungsrauschen transparent weiter. Dagegen hilft ein Pull-up (10 kΩ) auf der RXD-Leitung.

**Wo steht das:**

- DS Tabelle 21, **S. 29**, Fußnoten 11–14: „P_UART_CTS (optional) - requires enabling RTS/CTS
  in the EZ-Serial FW" / „**REQUIRED** for EZ-Serial command/data mode".
- DS **S. 13**, Tabelle 13 (UART-Timing): Signalnamen `UART_CTS_N`, `UART_RTS_N` → active-low.
- GD **S. 73** (§4.1.1): Kreuzung der vier Leitungen; §4.1.3: „factory default settings are
  115200,8/N/1 with **no flow control**. However, you **should** enable and use flow control if
  the host supports it."

**Warum WIR sie nutzen — alle vier:**

- RXD/TXD sind der **Nutzdatenweg**: Menü, PIN, Messdaten-Download. Ohne sie gibt es gar keine
  Kommunikation. Wir fahren sie mit **460800 Bd** (921600 verworfen — HW-Limit des LPC1766).
- **RTS/CTS nutzen wir bewusst**, obwohl sie „optional" sind: bei 460800 Bd und 256-Byte-Puffern
  gehen ohne Flow Control Daten verloren. Die Firmware erzwingt die Aktivierung sogar selbst —
  erkennt sie ein Werksmodul (Flow Control aus), bootstrappt sie es per `cts_bypass` und
  verifiziert danach den CTS-Handshake; bleibt CTS HIGH, gibt es `F11009`.
- **Besonderheit bei uns:** Diese vier Leitungen sind über einen Analog-MUX (IC18) mit GSM **geteilt** — Eigentümer-Verwaltung über `uart1_request()`/`uart1_release()`. Deshalb kann das
  Gerät für Sekunden „stumm" sein, obwohl die BT-Verbindung steht.
- **Offener Punkt:** Beim Wegschalten des MUX floatet die Modul-RXD und das Modul reicht
  Leitungsrauschen als 0xFF-Junk transparent weiter. Ein 10-kΩ-Pull-up auf `BLT-RXD` behebt das
  (Maßnahme 4, 90 % Wahrscheinlichkeit) — **noch nicht nachgerüstet - Testen**.

**Auf unserem Board:** `BLT-RTS`/`BLT-RXD`/`BLT-CTS`/`BLT-TXD` an Pins 14/15/16/17, 460800 Bd mit
Flow Control (`STU ... F=01`).

---

## 9. `BT_UART` (HCI) — Pins 24–27 — nur zum Firmware-Flashen

**Was sie machen:** Der zweite, separate UART des Moduls (HCI). **Nicht** für Nutzdaten — nur
zum Laden/Aktualisieren der Modul-Firmware.

| Pin | Signal | Richtung laut DS |
|---|---|---|
| 24 | `BT_UART_RTS` | O, **PU** (Ausgang mit internem Pull-up) |
| 25 | `BT_UART_TXD` | O, PU |
| 26 | `BT_UART_RXD` | I |
| 27 | `BT_UART_CTS` | I, **PU** |

**Richtig nutzen:**

- Alle vier Leitungen **plus RST** zugänglich machen (Testpads/Header), wenn man die Modul-FW je
  onboard aktualisieren will.
- Flow Control ist beim Flashen **zwingend**, 115200 8N1, und **CTS beim Reset LOW**.
- Externe Pull-ups sind hier unnötig — RTS und CTS haben laut Datenblatt schon interne
  („PU" in der I/O-Spalte).

**Wo steht das:**

- DS Tabelle 21, **S. 30**: Zeilen 24–27, jeweils „NOTE - Required for flashing EZ-Serial and HCI
  firmware", I/O-Spalte mit `O, PU` bzw. `I, PU`.
- DS **§11, S. 34**: die fünf Flash-Regeln (siehe Punkt 7).

**Warum WIR sie nicht nutzen:** Sie sind ausschließlich zum Flashen der Modul-Firmware da — im
Betrieb völlig ohne Funktion. Deshalb wurden sie am Board nicht herausgeführt.
**Die Folge:** Wir können die Modul-Firmware nicht onboard aktualisieren. Der in
`PCN #2E-2026` beschriebene Fix „lockup issue due in heavy UART loading" (1.4.18.18) ist damit
für uns nicht erreichbar — unsere Module laufen auf **1.4.12.12**. Zusammen mit dem fehlenden
RST-Zugang (Punkt 7) ist das der Grund, warum ein Modul-FW-Update aktuell Ausbau bzw. DVK
bedeuten würde.

**Auf unserem Board:** nicht herausgeführt.

---

## 10. Versorgung — `VDDIO` Pin 2, `3P0V` Pin 22, GND Pins 3/21/23

**Richtig nutzen:**

- **100 nF direkt an jeden Power-Pin**, so nah wie möglich — Herstellervorschrift, nicht optional.
- Zusätzlich Bulk (10–47 µF) an der Schiene.
- **Alle Signalpins liegen in der VDDIO-Domäne.** Maximum 3,3 V, empfohlen 3 V.
- **Betriebsregel:** immer nur **eine** Versorgungsquelle (Board **oder** DVK-USB, nie beides) —
  Back-Powering hat bereits zwei Module und ein DVK zerstört.

**Wo steht das:**

- DS **§12.1, S. 34**: „Be sure to put a 100nF capacitor on **EACH** power pin. Also, place that
  capacitor to the pin **as close as possible** to make sure the internal PMU working correctly."
- DS **S. 33**: „It is recommended to put bypass capacitors 100nF in the 3P0V (pin22) and VDDIO
  (pin2)."
- DS Tabellen 4/5, **S. 10**: Absolute Maxima / empfohlene Betriebsbedingungen; Fußnote „Maximum
  rating for signals follows the **supply domain** of the signals."

**Warum WIR sie so nutzen:** Ohne Versorgung kein Modul — hier gibt es keine Wahl. 

**Offener Punkt auf unserem Board:** **C128 und C58 (je 1 µF) sind unbestückt** — die vom
Hersteller vorgeschriebene lokale Entkopplung fehlt damit komplett.

---

## Übersicht: Funktion und unsere Nutzung

Richtungsangabe immer **aus Sicht des IF820** (`I` = Eingang am Modul, der Host treibt;
`O` = Ausgang des Moduls, der Host liest). Belegt durch DS Tabelle 21, S. 30: `BT_DEV_WAKE` = `I`
mit „a signal **from the host to** the CYW20820", `BT_HOST_WAKE` = `O` mit „**from the**
CYW20820 **to the host**".

| Pin | Nr. | Richtung | Funktion in einem Satz | Nutzen wir? | Warum |
|---|---|---|---|---|---|
| `CYSPP` | 12 | I/O | Datenmodus-Schalter, low-aktiv | **ja — zentral** | Unsere **einzige** Verbindungserkennung (`BT_LINK`); als Ausgang nur in `bt_disconnect()`. Sonst hochohmig lassen — sonst stirbt BLE. |
| `CONNECTION` | 33 | O | „Datenpipe steht, senden erlaubt" | nein | Unverdrahtet. Wäre die saubere Trennung von Steuerung und Status — zurückgestellt, weil der Firmware-Fix reicht. Reserve. |
| `CP_ROLE` | 31 | I | Central (LOW) / Peripheral (HIGH) | nein | Unverdrahtet — korrekt so: floating = Software-Rolle, und die ist Peripheral. Genau unser Fall. |
| `LP_MODE` | 30 | I | Sleep erlauben (LOW) / verhindern (HIGH) | nein | Kein Energiesparen am Modul; Advertising läuft dauerhaft. Offen = intern HIGH = kein Sleep = gewollt. |
| `BT_HOST_WAKE` | 29 | O | „ich bin wach und kommandobereit" | nein | Nur mit Sleep sinnvoll. Zudem über R138 (10 k) an der CYSPP-Leitung — nicht isoliert nutzbar. |
| `BT_DEV_WAKE` | 28 | I | von EZ-Serial **nicht** implementiert (`NA`) | nein | Es gibt nichts zu nutzen. Über R139 (0 Ω) an der CYSPP-Leitung — **keinen Pull anbringen**. |
| `RST` | 34 | I | Hardware-Reset, aktiv LOW, intern gepullt | nein (fehlt uns) | Unbeschaltet → LPC kann Modul nicht resetten, kein Onboard-FW-Update. R124-Pad wäre der Zugang. |
| `P_UART` | 14–17 | — | Nutzdaten-UART, RXD/TXD Pflicht | **ja — alle vier** | 460800 Bd **mit** Flow Control (bei 460800 zwingend). Mit GSM/GPS über MUX geteilt. |
| `BT_UART` | 24–27 | — | HCI, nur Firmware-Flashen | nein | Nicht herausgeführt → Modul-FW-Update (1.4.18.18) onboard unmöglich, wir bleiben auf 1.4.12.12. |
| `VDDIO`/`3P0V` | 2 / 22 | — | Versorgung, 100 nF an **jeden** Pin | **ja** | Schaltbarer Zweig über T10 — bei fehlendem RST der einzig denkbare Software-Reset. C128/C58 fehlen noch. |
