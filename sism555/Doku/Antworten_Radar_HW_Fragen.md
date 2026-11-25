# Antworten auf die Fragen des HW-Kollegen zum Via-2-Radarmodul (Abschnitt 6a)

Stand 2026-11-25. Quellen: Schaltplan/Board `Mainboard-V2-A1_24_11_2026.sch/.brd` (Netze maschinell ausgewertet),
Firmware `sism555/` (Stand 5.153/5.154), `Doku/HF_Kanal_Nachfuehrung.md`. **Nichts davon ist am Gerät gemessen.**
Status: **ja** = aus Schaltplan/Code belegt, **teils** = Teil belegt, **offen** = nicht aus den Unterlagen ablesbar.

## Anschluss des Moduls: STV24

Der Radar-Stecker auf dem Mainboard ist **STV24** (Würth WE 690357281676, Micro-Match 16-polig SMD; in der Frage
"X100"). Die Pad-Nummern im Board stimmen mit den Modul-Pins der Fragen überein (7 = V_FSK, 9 = VPTAT, 16 = Teiler):

| Pin | Netz | Anschluss auf dem Mainboard |
|---|---|---|
| 1 | V-RF | Versorgung der ZF-Verstärker (LMV652): `R94` 0 Ω nach V+A, parallel `T11` (P-MOS, Gate `EN/-V+RF` = Expander IC29 P1.2) |
| 2, 4 | GND | |
| 3 | V+A' | Modulversorgung: `T13` (P-MOS, Gate = `EN/-MD-RF`) und `R104` 0 Ω nach V+A (siehe Frage 8) |
| 5 | EN/-MD-RF | Expander IC29 P1.4 über `R91` 100 Ω, LOW = ein |
| 6 | EN/-IF' | LPC P4.29 (`DIS_IF`) über `R101` 1 kΩ, `R98` 10 kΩ |
| 7 | FM-IN (V_FSK) | Puffer IC35 über `R96` 220 Ω, optional DAC A über `R73` 10 kΩ |
| 8 | EN/-F-MESS | LPC P1.21 (`DIS_MESS`) über `R76` 1 kΩ |
| 9 | V-PTAT | `R109` 2,2 kΩ auf den Eingang des Puffers IC35 |
| 10 | R-REF-HF (R_TUNE) | `R89` 22 kΩ nach GND, parallel `R95` 7,5 kΩ + Digitalpoti IC32 (MCP4561-503) |
| 11 | EN-TX | `R150` 0 Ω zu `EN/-MD-RF`; Expander IC29 P0.6 (`AMP-EN`) und P1.5 über je 1 kΩ |
| 12 / 13 | IQ / IF | je `R86`/`R85` 3,3 kΩ ins ZF-Netzwerk |
| 14, 15 | nicht belegt | |
| 16 | F-HF/8192-A | Teiler-Ausgang, siehe Frage 14 |

## Abstimmspannung (Frequenz)

| Nr | Status | Antwort |
|---|---|---|
| 1 | teils | **Es gibt zwei Wege, die Frequenz zu beeinflussen.** (a) **R_TUNE (Pin 10):** Die Firmware stellt die Frequenz über das Digitalpoti IC32 (MCP4561-503, 50 kΩ, 257 Stufen) ein; es bildet zusammen mit `R95` 7,5 kΩ und `R89` 22 kΩ den Widerstand an Pin 10. Kennlinie auf Board B: Stufe 0 = 24157, 128 = 24171, 224 = 24214, 256 = 24270 MHz, unten ca. 0,1 MHz/Stufe, oben bis 2,5 MHz/Stufe. (b) **V_FSK (Pin 7):** hier hängt der Puffer IC35 (MCP6006, Verstärkung 1) über 220 Ω. Zusätzlich ist ein 12-Bit-DAC **IC30 MCP4922** vorgesehen (VOUTA direkt über 10 kΩ an Pin 7, VOUTB über 10 kΩ auf den Puffereingang; je 4,7 µF am Ausgang, Referenz über `R90` 1 kΩ/`C93` 1 µF aus V+A). **Die Firmware spricht den DAC nicht an:** `DA_SS` (P1.20) wird nur als Eingang geschaltet, es gibt keinen SPI-Zugriff auf IC30. Rauschen/Welligkeit: nicht gemessen. Die Bauteile IC30, `R73`, `R81`, `R159` sind in den beiden Kompatibilitätsvarianten ("ohne LTE", "mit LTE") **nicht bestückt**, nur in der Variante ohne Flag. |
| 2 | teils | **Abgleichwert:** Poti-Stellung in IC32 (nichtflüchtig im Poti selbst) und `fp.TxF` (gemessene Sendefrequenz) im Parameterblock im Flash. **Fertigung:** Werksmenü (Strg+W im Testmenü) -> 4 -> 5 "Adjust To Channel Frequency": Bisektion über den Wiper, danach Kontrolle, speichert nur bei 24150...24200 MHz; Dauer ca. 10-15 s. Das Zielband (10 Kanäle à 5 MHz, 24155...24200) wird automatisch aus der letzten Ziffer der Seriennummer gewählt. Ein CSV-Import ist nicht vorhanden. |
| 3 | ja | **Nein.** Auf dem Board gibt es keinen Temperaturfühler. Die Firmware regelt direkt auf die gemessene Frequenz (2 min nach dem Einschalten, dann alle 15 min, Totband ±1,5 MHz, höchstens 16 Stufen je Schritt, 3 Schritte je Durchgang). VPTAT (Pin 9) wird nicht ausgewertet; die ADC-Kanäle (P0.25/P0.26) sind für andere Messungen belegt. |
| 4 | teils | Die Firmware begrenzt auf **Frequenz**, nicht auf Spannung: zulässig 24150...24200 MHz (`Min_TXF_VIA2`/`Max_TXF_VIA2`), die Nachführung nimmt jeden Schritt zurück, der das Band verlässt; der Werksabgleich speichert nichts außerhalb. Offen: ob die handgeführte "Set Module Frequency" (Menüpunkt 3) den Wiper auf ganz 0...256 fahren lässt (über Stufe ca. 205 liegt die Frequenz auf Board B über 24200 MHz); nicht im Code geprüft. |
| 5 | teils | **Reiner Dopplerbetrieb, keine Modulation durch die Firmware.** Da der DAC nie beschrieben wird (Frage 1), gibt es keine FSK-/FMCW-Ansteuerung. An Pin 7 liegt der Ausgang des Puffers IC35, dessen Eingang aus VPTAT (Pin 9) kommt. Welche Spannung dort tatsächlich steht und ob der DAC beim Einschalten 0 V liefert: nicht gemessen. |
| 6 | ja | **Pin 9 ist beschaltet, nicht offen:** `R109` 2,2 kΩ auf den Knoten `V-PTAT*` (`C113` 100 nF + `C122` 47 µF nach GND, Zeitkonstante ca. 100 ms), der den Puffer IC35 speist; der Puffer treibt über `R96` 220 Ω Pin 7 (zusätzlich `C105` 4,7 µF und `C95` 100 nF an Pin 7). Pin 7 wird damit auf eine gefilterte Kopie von Pin 9 gezogen, also nicht auf einen anderen Pegel. Die Mitkopplung (Pin 9 und Pin 7 sind im Modul verbunden) ist nicht simuliert. |

## Versorgung und Steuerpins

| Nr | Status | Antwort |
|---|---|---|
| 7 | teils | 3,3 V analog (`V+A`) kommt aus **IC40 AP2125K-3.3** (Linearregler), Entkopplung `C121` 47 µF, `C110` 4,7 µF, `C61` 1 µF, `C70` 100 nF, am Modul `C96` 1 µF an V+A'. Genauigkeit, Strombelastbarkeit (der Regler selbst) und Welligkeit: Datenblatt nicht im Repo, nichts gemessen. Der Strom des Moduls (50/70 mA) ist nicht gemessen. |
| 8 | teils | Treiber: **EN/-MD-RF (Pin 5)** Expander IC29 P1.4 (`HF_DIS`, LOW = Transceiver ein), **EN/-IF (Pin 6)** LPC P4.29 (LOW = Verstärker ein), **EN/-F-MESS (Pin 8)** LPC P1.21 (LOW = Teiler ein), **EN-TX (Pin 11)** über `R150` 0 Ω mit EN/-MD-RF verbunden (nur bestückt in der Variante ohne Flag), zusätzlich Expander P0.6/P1.5 (`AMP-EN`) über 1 kΩ, **R_TUNE (Pin 10)** Poti IC32, **VDD (Pin 3)** über `T13` (Gate = EN/-MD-RF) **und** `R104` 0 Ω nach V+A; `R104` ist in den Kompatibilitätsvarianten nicht bestückt, in der Variante ohne Flag **bestückt und überbrückt dann `T13`** (Versorgung dauernd an). Dasselbe gilt für Pin 1: `R94` ist immer bestückt und überbrückt `T11`. **Bitte prüfen**, ob das so gewollt ist. Pins 14 und 15 sind nicht belegt. Hinweis zur Polarität: "nTX_ON, High = Sender ein" der Frage entspricht nicht eindeutig dem Netz `EN-TX`; die Polarität ist aus dem Schaltplan allein nicht abzulesen. |
| 9 | ja | **Ja, getaktet** (Stand 5.153): Das Modul wird nur während der Messung eingeschaltet. Ablauf Einschalten: RE/ LOW, nach dem Vorlauf (Standard 1 ms) EN/-MD-RF LOW; volle Verstärkung nach der **RE/-Zeit ab dem Einschalten: 15 ms** (bei kleinster Messgeschwindigkeit < 5 km/h: 25 ms; einstellbar 5...100 ms). Mindestmesszeit nach RE/: 8 Perioden der kleinsten Messgeschwindigkeit (bei 5 km/h 36 ms) oder fest einstellbar 10...250 ms; maximale Messzeit auf Board B 131 ms. Ausschalten: EN/-MD-RF HIGH, RE/ bleibt für den Nachlauf (Standard 10 ms) HIGH, dann LOW. Der erlaubte Sleep-Strom ist nirgends festgelegt (offen). Hinweis: Ein Commit nach 5.153 vermerkt, dass der RE/-Vorlauf am Pin nicht sichtbar war; die Zeiten sind am Oszilloskop noch nicht bestätigt. |

## ZF-Signal und Auswertung

| Nr | Status | Antwort |
|---|---|---|
| 10 | teils | **Nicht fertig ausgewertet.** Aufbau je Kanal (IF und IQ gleich): `R85`/`R86` 3,3 kΩ am Modulausgang, umschaltbares RC-Netzwerk (Bandbreite über IC25, Verstärkung über IC14, beide 74HC4052, gesteuert von Expander IC29 P0.0...P0.5), erste Stufe IC20 (LMV652, Rückkopplung u. a. 150 kΩ/470 kΩ/1 MΩ/2,2 MΩ über Analogschalter IC15, 12 pF), zweite Stufe IC17 (LMV652) mit Digitalpoti IC21 (MCP4461, 100 kΩ) für die Verstärkung und Koppelkondensator 220 nF + 5,6 kΩ davor; Ausgang `IF1-A`/`IQ1-A` (an STV22 messbar), danach Tiefpass (1 kΩ + 10 nF) und Komparator IC19 (TLV9032), dessen Schwelle aus IC9 (MCP6006) kommt. Referenz für die Eingänge ist `VCC/2` (R62/R67 10 kΩ), also **AC-gekoppelt, einseitig 3,3 V**. Verstärkung, Eckfrequenzen und Filterordnung je Stellung sind **nicht berechnet** und müssen aus den Stellungen der Mux und Potis abgeleitet werden. Das ist die wichtigste offene Frage der Liste. |
| 11 | ja | **Kein ADC in der Radarkette.** Die ZF wird verstärkt, an IC19 in ein Rechtecksignal (`IF1-D`/`IQ1-D`) gewandelt und per Timer-Capture ausgewertet (Zeitabstände der Nulldurchgänge, LPC P0.4 CAP2.0 bzw. P0.23/P0.24 CAP3). Die Richtung liefert ein Flip-Flop (IC16/IC27, Signal an `DIR`). Der interne LPC-ADC (12 Bit, 1 MHz Takt, 0...3,3 V, `get_ad()`) wird für andere Zwecke gelesen (z. B. Helligkeit). Der DC-Offset des Moduls von 1,3...2,2 V wird analog abgefangen; die Eingangsbereiche der Stufen sind dazu nicht geprüft. |
| 12 | teils | **Geschwindigkeitsbereich:** Doppler ca. 45 Hz je km/h, Messbereich bis ca. 11 kHz (ca. 245 km/h); anzeigbar 1...199 km/h (`Defminspeed`/`Defmaxspeed`); kleinste gemessene Geschwindigkeit ist der Parameter `fp.vmm`. **Ein Mindest-SNR ist nicht definiert:** Die Auswertung prüft Plausibilität (Phasenlage IF zu IP zwischen 30° und 150°, Streuung der Periodendauern höchstens `MAXFEHLER`), nicht einen SNR-Wert. Ein belastbares Mindest-SNR müsste mit einem Doppler-Signal am Gerät bestimmt werden. |
| 13 | teils | Richtung wird in Hardware (Flip-Flop, Pin `DIR`) erkannt und in `measure()` zur Hälfte des Messpuffers gelesen (`kommt`/`geht`). Eine Zahl für die Trennung in dB gibt es in der Software nicht. **Ob I und Q auf dem Modul vertauscht berücksichtigt werden:** im Code nicht ausdrücklich dokumentiert, offen (Test mit herannahendem und abfahrendem Fahrzeug nötig). |

## Fertigung und Mechanik

| Nr | Status | Antwort |
|---|---|---|
| 14 | teils | Der Teiler-Ausgang (Pin 16, `F-HF/8192-A`) liegt auf STV24 und geht an `IC34`, der zusätzlich um 1024 teilt (`F-HF/8388608`, Gesamtteilung 2^23); dieses Signal liegt am LPC (P1.26, CAP0.0) und am Pfostenstecker **STV21**. Die Firmware misst darüber die Frequenz selbst (Werksmenü, Strg+T). Ein eigener Testpunkt ist im Plan nicht bezeichnet; STV21 ist in den Kompatibilitätsvarianten nicht bestückt. Belegung von STV21 nicht ausgewertet. |
| 15 | offen | Der Gegenstecker zu STV24 (Micro-Match 16-polig, passende Buchsenleiste/Kabelkonfektion) und die Kabellänge sind in den Unterlagen nicht festgelegt. |
