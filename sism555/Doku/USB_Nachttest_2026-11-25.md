# USB-Nachttest 25./26.11.2026 - Bericht

Branch `usb-verbindung` (Worktree `Projects\USB-Verbindung`), Board **26SC9966** (Mainboard V2), RS232 an COM3, Relais 2
an COM7, USB direkt am Pruef-PC (Windows 11), keine Hubs. Analyse und Massnahmenliste: `USB_Analyse_2026-11.md`.
Rohdaten: `Doku/tools/Testreports/usb/` (nicht im Git).

## Kurzfassung

1. **Hauptfund: Absturz im Keil-USB-Treiber beim Schliessen des COM-Ports (F11).** Schliesst der PC den Port, waehrend
   das Geraet ausgibt, schreibt der Treiber ein EP0-Paket nach Adresse 0 -> HardFault -> Geraet steht 32 s ->
   Watchdog-Reset. Rate im Test ca. 5 % je Schliessen waehrend einer Ausgabe. **Mit der heutigen Firmware (5.156) ist
   das Geraet danach tot (USB und RS232), bis der Strom weg war** - das passt genau zu "USB geht nicht/nicht mehr".
   Behoben in **5.u18** durch eine gepatchte lokale Kopie des Treibers. Nachweis: 600x Schliessen waehrend der Ausgabe
   mit 5.u17/5.u18 ohne einen Absturz (vorher alle 3-20 Versuche).
2. **Kein Neustart nach Firmware-Update ueber USB (F8)**, auch nach jedem Watchdog-Reset waehrend einer USB-Sitzung
   (Ursache fuer "tot" in Punkt 1). Behoben in 5.u2.
3. **Datenfehler beim USB-Empfang (F9)**: 10-28 NAK je Update ueber USB. Behoben (5.u3/5.u18): 0 NAK, Update 22-23 s
   statt 30 s.
4. **Tiefschlaf bei gestecktem Kabel (F1)**, Seriennummer, VBUS-Entprellung, Neuanmeldung: 5.u1. F1 konnte am Tisch
   nicht ausgeloest werden (GPS-Suche haelt das Geraet 60 min wach, Kabel nicht automatisch steckbar).
5. **Rest:** wird der Port innerhalb ca. 1 s nach einem Schliessen mit laufender Ausgabe wieder geoeffnet, lehnt
   Windows das Oeffnen gelegentlich einmal sofort ab (0-8 je 300, stark schwankend); das naechste Oeffnen klappt, das
   Geraet laeuft durch. Mit 3 s Pause 0/300. Kein verworfenes EP0-Paket dabei (5.u20-Diagnose) - Ursache offen,
   vermutlich Windows-seitig.

6. **Zweiter Treiberfehler (F12): USB tot ohne Absturz.** `USBD_ReadSetupPacket()` gibt in einem Fehlerzweig seine
   Sperre und den USB-Interrupt nicht frei -> USB antwortet nie mehr, das Geraet laeuft sonst weiter (RS232 ok). Mit
   5.u18 im Test nach 145 Schliessvorgaengen eingetreten. Behoben in **5.u21**.

**Firmware-Kandidat: 5.u21** (Commit `746df9a`). Versionen 5.u4-5.u17, 5.u19, 5.u20 waren Vergleichs-/Diagnosebuilds
und sind nicht im Git (Binaries unter `Testreports/usb/fw/`).

## Ablauf und Ergebnisse

### 1. Vergleichslauf 5.157 (Ausgangsstand)

10 Stromzyklen (0,5-10 s stromlos), danach je 60 s Leerlauf: 10/10 ok. USB-Port 35 s nach dem Einschalten da (nach
0,5-1 s stromlos 15 s: Elektronik nicht ganz entladen, vgl. H4). Hinweis: an diesem PC tritt der Treiber-Absturz im
normalen Menuebetrieb selten auf; er braucht das Schliessen waehrend einer Ausgabe.

### 2. Firmware-Update ueber USB haengt (F8) -> 5.u2

Update 5.157 -> 5.u1 ueber USB: kein Neustart, erst der zweite Stromzyklus half. `USB_LINK` in `connect` (`__noinit`)
ueberlebt den Reset, die erste Ausgabe geht in den nicht gestarteten USB-Stack. Seit 5.u2 kommt jedes Update nach 3 s
mit USB zurueck (ueber 50 Updates in der Nacht, alle ok).

### 3. NAKs beim Update ueber USB (F9)

Empfangsschleife im USB-Thread: Index vor dem Byte sichtbar. Fix: erst Byte, dann `rxi`. 5.u3 sperrte dabei je Byte den
UART0-Interrupt - das erhoehte die Absturzrate (siehe 4.); 5.u18 macht es ohne Sperre, voller Puffer wird nach 100 ms
verworfen (wie RS232).

### 4. Port laesst sich gelegentlich nicht oeffnen -> Treiber-Absturz (F11)

Dauerlauf 5.u3 (100 Runden): 9 Runden mit Oeffnungsfehler. Oeffnungstest `usb_open_stress.py` (oeffnen, `H`, Menue
lesen, schliessen, 1 s Pause):

| Stand | Oeffnungen | Fehler | Bemerkung |
|---|---|---|---|
| 5.156 | 444 | 1 | danach Geraet tot (F8), Lauf abgebrochen |
| 5.u1 / 5.u2 | 300 / 1500 | 0 / 1 | |
| 5.u3-5.u5 (UART0-Sperre) | 900 | 13 | |
| 5.u6 / 5.u8 / 5.u9 / 5.u10 | 1500 / 473 / 900 / 500 | 8 / 4 / 7 / 2 | Empfangsvarianten |
| 5.u18 | 292 | 2 | nur noch sofortige Windows-Fehler, kein Neustart |

Das Geraeteprotokoll zeigte zu jedem Fehler `I31 Watchdog reset`. Gezielter Ausloeser `usb_close_provoke.py` (oeffnen,
`H`, sofort schliessen): ca. 5 % Abstuerze. Diagnose-Builds:

- 5.u11: Ringpuffer laeuft nie voll -> nicht die Ursache.
- 5.u12/5.u14: Watchdog-Waechter in der 1-ms-ISR gibt nichts aus -> keine ISR laeuft mehr, auch nicht mit USB-IRQ unter
  Timer-Prioritaet -> kein USB-ISR-Sturm.
- 5.u15/5.u16: HardFault-Handler mit Ausgabe ueber UART0:
  `pc=0001BC0A psr=61000028 (IRQ 24 = USB) cfsr=00000400 (IMPRECISERR) r4=00000004 r5=00000008 r6=00000000`
  -> `USBD_ReadEP()` im Keil-Treiber, EP0 OUT, 8-Byte-Paket, Zielpuffer NULL.
- 5.u17: gepatchter Treiber -> 300x Ausloeser: 0 Abstuerze. 5.u18 (sauber, ohne Diagnose): 300x: 0 Abstuerze.

### 5. Restfehler beim Oeffnen

5.u18: 3/300 beim Ausloeser, 2/292 beim Oeffnungstest - alle sofort (0,01 s), ohne Neustart, naechster Versuch ok.
5.u19 (Ausgabe nach DTR-Fall gesperrt) brachte keine Besserung (2/300, 3/74) und ist verworfen.

### 6. Treiberfehler 2: USB tot (F12) -> 5.u21

A/B-Vergleich 5.u21/5.u18 (je 2x300 Ausloeser, abwechselnd geflasht): 5.u21 0 Abstuerze, 5 bzw. 0 sofortige
Oeffnungsfehler; 5.u18 1 Fehler im ersten Lauf, im zweiten Lauf ab Versuch 145 **USB tot** (jedes Oeffnen 3,5 min
"Zeitlimit fuer die Semaphore", RS232 bedienbar). Ursache `USBD_ReadSetupPacket()` (siehe Kurzfassung 6). 5.u21 danach:
600x Ausloeser, 1 sofortiger Oeffnungsfehler, kein Absturz, kein Ausfall.

### 7. Dauerlaeufe

| Stand | Runden | ok | Downloads | Updates (NAK) | Bemerkung |
|---|---|---|---|---|---|
| 5.u3 (17:07-19:10) | 100 | 91 | 29/31 | 18/20 (0) | Abstuerze (F11) |
| 5.u18 (23:36-01:26) | 90 | 86 | 29/30 | 18/18 (0) | 4x sofortiger Oeffnungsfehler |
| **5.u21 (02:50-05:30)** | **129** | **129** | **43/43** | **25/25 (0)** | Stromzyklen 0,5-30 s, Tiefschlaf 0 |

Runde = Stromzyklus (0,5/1/2/5/10/30 s stromlos), Port muss stabil kommen, Menue/Info ueber USB vor und nach 10 s
Leerlauf; jede 3. Runde Ymodem-Download (5000 Messwerte), jede 5. Runde Firmware-Update ueber USB. USB-Port 14-36 s
nach dem Einschalten (Median 35 s).

Phase B 5.u21 (05:30-06:30, Geraet laeuft ohne Stromzyklus, Menue/Info ueber USB alle 2 min, Port jedes Mal
oeffnen/schliessen): **30/30 ok, 0 Port-Wechsel, Tiefschlaf-Zaehler 0.**

## Weitere Befunde

- **RTX-Stackpool zu klein bei vollem Betrieb:** `OS_PRIVSTKSIZE` 768 Worte fuer `OS_PRIVCNT` 6 Threads mit eigenem
  Stack; angefordert USB-Thread 512 + GSM 512 + MQTT 768 + USB-Kern 512 + 2x CDC 512 (+ USB-Host-Kern 512) Byte. Laufen
  LTE und USB gleichzeitig, reicht der Pool nicht (Thread-Start scheitert). Nicht ausgeloest, aber zu pruefen.
- **USB-Durchsatz 7,6 KB/s** beim Download: am Pruefplatz bremst vermutlich die parallele RS232-Ausgabe (115200 Bd).
- **Ladegeraet am USB:** mit 5.u1+ schlaeft das Geraet nicht, solange VBUS anliegt.

## Fragen fuer morgen

1. **Treiber-Patch** (`USB_Driver_Patch/USBD_LPC17xx.c`, Pack-Komponente abgewaehlt, zwei Korrekturen): so in Ordnung? Alternative waere
   ein neueres LPC1700_DFP, falls Keil den Fehler behoben hat (nicht geprueft, kein Netzzugriff auf Pack-Server genutzt).
2. **HardFault-Handler dauerhaft?** Heute haengt das Geraet bei einem HardFault 32 s bis zum Watchdog. Vorschlag: eigener
   Handler, der PC/Fault-Register ins Protokoll schreibt und sofort neu startet.
3. **Restfehler** (sofortige Windows-Ablehnung nur beim Wiederoeffnen innerhalb ~1 s): akzeptieren oder mit
   USB-Analysator weiter verfolgen?
4. **F1 (Tiefschlaf) am Kunden-Szenario pruefen:** Geraet > 60 min laufen lassen (GPS gibt auf), dann USB-Kabel stecken -
   mit 5.156 und 5.u18 vergleichen. Braucht Hand am Kabel oder einen schaltbaren USB-Hub.
5. **Hardware H1-H6** (Analyse Abschnitt 4) mit dem HW-Kollegen: vor allem USB-Umschalter Host/Geraet, Pulldowns,
   IC49-Beschaltung, VBUS-Teiler, Kabelbaum STV9.
6. **USB frueher starten (F10, heute 35-68 s nach dem Einschalten)**: gewuenscht? Braucht vorher F6 (eigener USB-Zustand).
7. **Eigene USB-PID** beantragen? Strings: entschieden 2026-11-26, "via traffic"/"viasis" (5.u22).
8. Merge nach `main`: wann, und Test auf weiteren Boards (A, B) und an anderen PCs (Matrix in der Analyse)?

## Nachtrag 26.11.: Vergleich mit LPC1700_DFP 2.7.2

Neuestes Pack von keil.com: **2.7.2 (2024-02-09), als "no longer maintained" markiert**, es kommt nichts mehr nach.
Installiert und im Projekt: 2.0.0 (2014), darin `USBD_LPC17xx.c` **V2.00**; in 2.7.2 **V2.14**.

| Unser Fehler | V2.14 |
|---|---|
| F11 EP0-OUT nach Adresse 0 (HardFault) | behoben (V2.12 "ReadEP/WriteEP mit data = NULL"): Daten werden gelesen und verworfen |
| F12 Sperre in `USBD_ReadSetupPacket` | behoben: prueft `setup_received` vor der Sperre |
| Paket laenger als Restpuffer (in 5.u18 mit begrenzt) | nicht begrenzt, im Test nie aufgetreten |

Weitere Korrekturen seit V2.00, die uns betreffen koennen: V2.4 Lesen von gestalltem Endpunkt -> HardFault, **V2.8 SETUP
gefolgt von OUT-Paket** (passt zum Oeffnen des Ports: SET_LINE_CODING = SETUP + Datenpaket; moeglicher Zusammenhang mit
dem Restfehler, ungeprueft), V2.6/V2.7 Endpunkt-Konfiguration, Power-Control.

Abhaengigkeiten: keine Pflicht auf CMSIS 5 im Pack eingetragen; API USB Device 2.1 = unsere 2.01. Der Treiber nutzt aber
`__UNALIGNED_UINT32_READ/WRITE` aus CMSIS 5 (in CMSIS 4.2.0 nicht vorhanden) -> zwei Ersatz-Makros noetig.
Rest des Packs: `startup_LPC17xx.s` gleich (V1.10), `system_LPC17xx.c` V1.13 -> V1.14 (Projektkopie in `RTE/Device` bleibt,
solange man sie nicht aktualisiert), USB-Host V2.00 -> V2.6, I2C V2.01 -> V2.8, PIN/GPIO/GPDMA mit kleinen Aenderungen.

Vorschlag: nur den USB-Device-Treiber V2.14 als lokale Kopie in `USB_Driver_Patch/` uebernehmen (statt unseres gepatchten
V2.00), Ersatz-Makros dazu, eigene Laengenbegrenzung optional behalten; dann 300x Schliess-Ausloeser, Update/Download.
Ganzes Pack erst, wenn auch USB-Host/I2C getestet werden sollen.
