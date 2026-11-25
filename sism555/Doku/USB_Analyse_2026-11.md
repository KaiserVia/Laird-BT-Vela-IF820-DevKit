# USB-Kabelanschluss (Geraete-USB) - Analyse und Massnahmen

Stand 2026-11-25, Branch `usb-verbindung` (Worktree `Projects\USB-Verbindung`, Basis 5.156, Firmware ab 5.u1).

**Kundenbefund:** Der USB-Kabelanschluss der Hauptplatine laeuft bei vielen Kunden nicht zuverlaessig. Mit einem
aktiven Hub (eigenes Netzteil) geht es, direkt am PC oder ueber einen passiven Hub gibt es Probleme.

**Ziel:** USB laeuft an jedem PC und mit jedem normgerechten Kabel.

**Grundlagen:** Schaltplan `Doku/ECAD/Mainboard-V2-A1_24_11_2026.sch` (im Hauptordner, nicht im Git), NXP-Datenblatt
LPC1769-63 Rev. 9 (`Doku/LPC1766/`), Firmware-Quellen 5.156. Am Geraet ist noch nichts gemessen; alles Folgende kommt
aus Schaltplan, Datenblatt und Code.

## 1. Aufbau heute

- Der LPC1766 hat **einen** USB-Anschluss (P0.29 D+, P0.30 D-). Den teilen sich zwei Funktionen:
  - USB-Geraet: CDC-ACM (virtueller COM-Port) fuer Menue, Terminal und Firmware-Update; PC am Stecker `USB-B`.
  - USB-Host: USB-Stick (MSC); Stick am Stecker `USB-A`.
- Kein USB-Seriell-Wandlerchip; die Firmware (Keil MDK-Middleware USB 6.2) ist selbst das USB-Geraet.
- Beide Funktionen gehen ueber den 10-poligen Stecker **STV9** von der Platine weg (Pins 5/6 = B-D+/D-, 8/9 = A-D-/D+,
  4 = B-VBUS, 7 = A-VBUS). STV10 (Alternative) ist in beiden Bestueckungsvarianten nicht bestueckt.

| Netz | Bauteile |
|---|---|
| `USB-D+` (LPC P0.29) | R135 33 Ohm -> `USB-B-D+`, R136 33 Ohm -> `USB-A-D+`, **R137 15 k nach GND**, R120 1,5 k -> T15 (SoftConnect) |
| `USB-D-` (LPC P0.30) | R133 33 Ohm -> `USB-B-D-`, R134 33 Ohm -> `USB-A-D-`, **R132 15 k nach GND** |
| `USB-B-D+` | STV9.5, C134 22 pF, IC49 I/O1 **+ I/O3 + I/O4** |
| `USB-B-D-` | STV9.6, C133 22 pF, IC49 I/O2 |
| `USB-B-VBUS1` | STV9.4, IC49 VBUS, R128 1 k -> `USB-B-VBUS` |
| `USB-B-VBUS` (LPC P1.30 VBUS) | R128 1 k von VBUS1, R122 47 k nach GND |
| `USB-B-CONNECT` (LPC P2.9) | T15 BSS84 Gate, R116 100 k nach V+D (ohne Ansteuerung aus) |
| `VBUS-A` | IC47 TPS2041B (Enable P1.19 `USB_PPWR`, Fehler P1.27), C131 47 uF, R131 47 k |

## 2. Befunde Hardware

### H1 - Geraet und Host haengen parallel an denselben Datenleitungen (hoch)

R135/R136 und R133/R134 verbinden die LPC-Pins **gleichzeitig** mit beiden Steckern. Steckt der PC an USB-B, bleibt
die Leitung zur USB-A-Buchse (ueber STV9 und den Kabelbaum) als offenes Leitungsende dran. Bei Full-Speed (12 Mbit/s,
Anstiegszeit 4-20 ns) erzeugt das Reflexionen, je laenger der Kabelbaum, desto staerker. Ein aktiver Hub bereitet das
Signal neu auf und haelt die Strecke kurz; direkt am PC addieren sich langes Kundenkabel, Kabelbaum und offenes Ende.

**Massnahme:** Host- und Geraetezweig trennen, z. B. mit einem USB-2.0-Analogumschalter (TS3USB221, FSUSB42 o. ae.,
Auswahl per GPIO; Firmware schaltet auf B, sobald VBUS am B-Stecker anliegt, sonst auf A). Alternative ohne neues IC:
Bestueckungsoption, die nur einen der beiden 33-Ohm-Paare bestueckt (dann eine Variante ohne Stick-Funktion).

### H2 - 15 k nach GND an D+ und D- fest bestueckt (hoch)

R137 (D+) und R132 (D-) sind Host-Pulldowns. Im Geraetebetrieb haengen sie zusaetzlich zu den 15 k im PC an der
Leitung. Der Ruhepegel an D+ (1,5 k nach 3,3 V) faellt dadurch von etwa 3,0 V auf etwa 2,75 V, der Abstand zur
Schaltschwelle (max. 2,0 V, Datenblatt Vth(rs)se) wird kleiner; die Treiber werden staerker belastet. Die USB-Norm
sieht fuer ein Geraet keine Pulldowns vor. Die Firmware braucht sie nur zur Stick-Erkennung (`FIO0PIN&DPLUS`,
measure.c:310).

**Massnahme:** Pulldowns nur auf der Host-Seite (hinter dem Umschalter aus H1, Seite A), nicht am LPC-Pin.

### H3 - ESD-Baustein IC49 ungleich beschaltet, zusaetzlich 22 pF (mittel, erst pruefen)

Laut Schaltplan liegen drei Kanaele des USBLC6-4SC6Y (I/O1, I/O3, I/O4) auf D+ und nur einer (I/O2) auf D-. D+
bekommt damit etwa die dreifache Schutzkapazitaet, D- nur die einfache. Dazu kommen C134/C133 (22 pF) an beiden
Leitungen. Ungleiche Kapazitaeten verschieben den Kreuzungspunkt der Differenzsignale; die Summe aus LPC-Pin (bis
20 pF, Datenblatt Ctrans), 22 pF und Schutzdiode liegt deutlich ueber dem, was ein Full-Speed-Geraet an der Leitung
haben sollte. NXP zeigt in der Referenzschaltung (Datenblatt Abschnitt 14.1) nur 33 Ohm und 1,5 k, keine
Kondensatoren.

**Massnahme:** Pinbelegung im Bauteilsymbol (SamacSys) gegen das ST-Datenblatt und am Layout pruefen; korrekt ist je
ein Kanal fuer D+ und D- (oder USBLC6-2SC6 mit Durchgang). C133/C134 nicht bestuecken (Bestueckungsoption behalten).

### H4 - VBUS-Erkennung speist ein ausgeschaltetes Geraet (mittel)

`USB-B-VBUS1` geht ueber R128 1 k und R122 47 k nach GND: am Pin P1.30 liegen etwa 4,9 V. Der Pin ist 5-V-tolerant,
laut Datenblatt (Grenzwerte, VI) aber **nur, solange VDD(3V3) anliegt**. Ist das Geraet aus und das Kabel steckt,
fliessen bis etwa 1,5 mA ueber die Schutzstruktur in die 3,3-V-Versorgung: undefinierte Zustaende beim Einschalten,
moeglicher Hochlauf-Haenger, Langzeitschaden.

**Massnahme:** Spannungsteiler, z. B. 22 k / 33 k (ergibt 3,0 V, sicher ueber der TTL-Schwelle 2,0 V; bei
ausgeschaltetem Geraet unter 0,1 mA).

### H5 - USB-Daten ueber den Kabelbaum an STV9 (mittel, Information fehlt)

USB braucht ein verdrilltes, 90-Ohm-Leitungspaar. Wenn STV9 ueber einen unverdrillten Kabelbaum oder ein Flachkabel zur
Gehaeusebuchse geht, kommen Laenge und Uebersprechen (D+/D- liegen neben VBUS-A und dem anderen Paar) zum Kundenkabel
hinzu. Gesamtlaenge Kundenkabel + intern sollte unter 5 m bleiben.

**Massnahme:** Klaeren, wie STV9 zur Buchse geht (Laenge, verdrillt, Schirm). D+/D- als verdrilltes Paar mit eigener
Masse fuehren; Buchse mit Schirm an Gehaeuse (RC, z. B. 1 MOhm || 4,7 nF).

### H6 - Masse PC gegen Geraet (Option fuer "1a ueberall")

Das Geraet haengt an 12 V (Batterie/Netzteil), der PC an einem anderen Netz. Unterschiedliche Massepotentiale und
Gleichtaktstoerungen liegen direkt auf D+/D-. Ein aktiver Hub mit eigenem Netzteil bricht diese Strecke zum Teil auf.
**Robusteste Loesung:** USB-Isolator (z. B. ADuM3160/ADuM4160, Full Speed) am Geraeteeingang. Er trennt die Massen und
bringt eigene, starke USB-Treiber zum PC mit, wirkt also wie der aktive Hub, der heute hilft.

### Geprueft und in Ordnung

- SoftConnect (T15 BSS84 + R120 1,5 k, R116 haelt ohne Ansteuerung aus) nach NXP-Referenz.
- 33-Ohm-Serienwiderstaende vorhanden.
- Das Geraet lebt nicht von VBUS (nur Erkennung). Deskriptor "selbstversorgt" (0xC0) ist richtig. Der Strom am
  PC-Anschluss ist also **nicht** das Problem.
- Quarz Q1 12 MHz, USB-Takt 48 MHz aus PLL0 (FCCO 288 MHz / 6) exakt.

## 3. Befunde Firmware

### F1 - Tiefschlaf waehrend der Anmeldung am PC (hoch, wahrscheinlich Hauptursache)

Ablauf ohne RS232-Kabel, ohne Anzeige und ohne Messergebnis (typisch beim Kunden, der das Geraet zum Auslesen
anschliesst):

1. `runcycle()` erkennt VBUS, startet den USB-Thread (`USBD_Initialize`, `USBD_Connect`) und wartet 250 ms
   (measure.c:297-305).
2. `USB_LINK` steht erst nach `USBD_Configured` in `connect` (USB_tools.c:55-59). Braucht der PC laenger (Treiber
   laden, Hub-Kaskade, erstes Einstecken an diesem Port), ist `connect` noch 0.
3. measure.c:491-500: kein `refresh|acycle|connect|simpinset|gps_pending` -> `InitWatchdog` + `DeepSleep()`.
4. `DeepSleep()` (hard.c:864) schaltet **PLL0 ab, damit auch den 48-MHz-USB-Takt**, mitten in der Anmeldung. Danach
   weckt der Watchdog per Reset: USB-Block zurueckgesetzt, Pullup weg, der PC sieht ein Abziehen. Nach dem Hochlauf
   beginnt alles von vorn - Endlosschleife aus An- und Abmelden.
5. Wird das Geraet ohne Reset geweckt, schreibt `SystemInit()` PCONP = 0x00C086CC (system_LPC17xx.c:306/558); darin
   ist **PCUSB (Bit 31) aus**. USB bleibt tot, bis das Kabel gezogen wird.

Ob es klappt, haengt also daran, wie schnell der PC das Geraet einrichtet - genau das unterscheidet PCs, Ports und
Hubs. Am Pruefplatz faellt es nicht auf, weil dort das RS232-Kabel steckt (`connect` enthaelt UART0, kein Schlaf).

**Massnahme:** Kein Schlaf, solange VBUS anliegt oder der USB-Dienst laeuft (VBUS in die Bedingung measure.c:491
aufnehmen). PCUSB nach dem Aufwachen nicht abschalten.

### F2 - VBUS ohne Entprellung (mittel)

Ein einziger LOW-Wert an P1.30 beendet die Sitzung (USB_tools.c:55/65). Mit dem Teiler aus H4 und einem schwachen
Port bzw. Stecker-Wackler reicht ein kurzer Einbruch.

**Massnahme:** Trennen erst nach z. B. 300 ms durchgehend LOW.

### F3 - USB-Start haengt an der Hauptschleife (mittel)

USB wird nur in `runcycle()` gestartet, also erst nach BT- und GSM-Erkennung im Hochlauf (viele Sekunden) und gar
nicht, solange ein RS232- oder BT-Menue laeuft. Kein Neustart, wenn der PC nie konfiguriert.

**Massnahme:** Eigene VBUS-Ueberwachung im USB-Thread, Start gleich nach dem Hochlauf der Grundfunktionen; wenn nach
z. B. 5 s kein `Configured` kommt: `USBD_Disconnect`, 500 ms Pause, neu verbinden (Windows versucht es dann erneut).

### F4 - Keine Seriennummer, NXP-Beispielkennung (mittel)

RTE/USB/USBD_Config_0.c: VID/PID 0x1FC9/0x80F7 (NXP-Beispiel), Strings "NXP"/"LPC USB VCom Port", Seriennummer aus
(`USBD0_STR_DESC_SER_EN 0`). Windows behandelt das Geraet an jedem Port als neues Geraet: neue COM-Nummer, neue
Treibereinrichtung, laengere Anmeldung (verschaerft F1). Kunden finden "ihren" COM-Port nicht wieder.

**Massnahme:** Seriennummer = Geraete-Seriennummer, eigene Strings ("viasis ..."), eigene PID (bei NXP kostenlos
beantragbar, oder eigene VID). EP0 auf 64 Byte (heute 16).

### F5 - DTR wird ignoriert, Bytes werden still verworfen (mittel)

`USBD_CDC0_ACM_SetControlLineState` ist leer (USB_tools.c:350). Ohne geoeffnetes Terminal holt der PC nichts ab,
`putb()` verwirft nach 50 ms jedes Byte (sio.c:109-134, `usb_stalled`). Beim Oeffnen des Ports wird `usb_stalled`
nicht zurueckgesetzt und kein Prompt ausgegeben; der Kunde sieht ein "totes" Terminal, bis er eine Taste drueckt.
Bei Ymodem-Senden zum PC fuehrt eine Host-Pause ueber 50 ms zu verfaelschten Bloecken.

**Massnahme:** DTR auswerten: Flanke -> `usb_stalled=0`, Banner/Prompt; Ausgabe nur bei DTR; bei DTR gesetzt laenger
warten statt verwerfen.

### F6 - `USB_LINK` in der gemeinsamen Variable `connect` (mittel)

Der USB-Thread setzt/loescht `USB_LINK` asynchron, waehrend viele Stellen `connect` sichern und zuruecksetzen
(btio.c, gsmio.c:1135 loescht `USB_LINK`, mqtt.c). Gleiche Prioritaet, Round-Robin: USB-Ausgabe kann verloren gehen
oder `USB_LINK` nach dem Abziehen wieder auftauchen (GSM gesperrt). Bekannt als Befund 4.4 in
`fable_review_2026-11-11.md`.

**Massnahme:** USB-Zustand in eine eigene Variable, `putb()` liest sie; `USB_LINK` in `connect` nur noch daraus
ableiten.

### F7 - Kleineres

- Empfang: `rbuf[++rxi]` aus USB-Thread und UART0-ISR ohne Schutz (Wettlauf nur bei gleichzeitigem RS232+USB).
- IAP mit gesperrten Interrupts bei der Parametersicherung (flash.c:388): USB-Steuertransfers koennen ins Timeout
  laufen.
- Vor dem Firmware-Update kein `USBD_Disconnect`; der PC merkt das Abmelden erst am Reset.
- `USB_RPWR` (P1.26) ist auf Via-2-Boards Messeingang; betrifft nur die Stick-Versorgung (5V13 -> D8 -> IC47), nicht
  den Geraete-USB.

## 4. Was wir machen muessen - Reihenfolge

| # | Massnahme | Art | Aufwand | Wirkung auf "nur mit aktivem Hub" |
|---|---|---|---|---|
| 1 | F1 kein Schlaf bei VBUS, PCUSB erhalten | Firmware | klein | hoch |
| 2 | F3 Start/Neustart im USB-Thread, F2 Entprellung | Firmware | mittel | mittel |
| 3 | F4 Seriennummer, Strings, PID, EP0 64 | Firmware | klein (PID: Antrag) | mittel |
| 4 | F5 DTR, F6 eigener USB-Zustand | Firmware | mittel | gering-mittel (Bedienung) |
| 5 | H3 C133/C134 weg, IC49 pruefen | Bestueckung | klein | mittel |
| 6 | H2 R137/R132 nicht am LPC-Pin (Sofortmassnahme: auslassen, Stick-Erkennung dann aus) | Bestueckung/Layout | klein | mittel |
| 7 | H4 VBUS-Teiler 22 k / 33 k | Bestueckung | klein | gering (Robustheit) |
| 8 | H1 USB-Umschalter Host/Geraet | Layout | mittel | hoch |
| 9 | H5 Kabelbaum verdrillt/geschirmt, Buchse | Mechanik | mittel | mittel-hoch |
| 10 | H6 USB-Isolator | Layout | mittel | hoch (Masse, Treiber) |

Firmware 1-4 laufen auch auf vorhandenen Geraeten (Kunden-Update). 5-7 gehen als Nacharbeit an bestehenden Platinen.
8-10 sind fuer die naechste Platinenrevision.

## 5. Nachweis

**Schnelltest fuer F1 (vor jeder Aenderung):** Geraet ohne RS232-Kabel, Anzeige aus, kein Fahrzeug. USB direkt an den
PC. Im Geraete-Manager beobachten: erscheint/verschwindet der COM-Port im Takt von ca. 1,5 s oder kommt "Unbekanntes
Geraet (Fehler 43)"? Gegenprobe: dasselbe mit gestecktem RS232-Kabel (kein Schlaf) - geht es dann, ist F1 bestaetigt.

**Abnahme nach den Massnahmen (Matrix):**

- PCs: mindestens 2 Desktop + 2 Laptops (Intel und AMD), Windows 10 und 11; Laptop am Akku und am Netzteil.
- Anschluss: USB 2.0-Port, USB 3.x-Port, USB-C mit Adapter, passiver Hub, aktiver Hub.
- Kabel: 0,5 m, 1,8 m, 3 m, 5 m, ein billiges Kabel.
- Ablauf je Kombination: Einstecken bei laufendem Geraet (im Leerlauf/Schlaf), Einschalten bei gestecktem Kabel,
  Terminal oeffnen, Menue, Ymodem-Download 4 MB, Firmware-Update ueber USB, Abziehen/Anstecken 10x.
- Oszilloskop an D+/D- am LPC: Ruhepegel D+ (Ziel ca. 3,0 V), Flanken und Ueberschwinger, mit und ohne USB-A-Zweig.
- Windows: Ereignisanzeige (Kernel-PnP), USBView fuer Deskriptor und Anmeldezeiten.

## 6. Offene Fragen an Anwender / HW-Kollegen

1. Wie geht STV9 zur Gehaeusebuchse (Kabel, Laenge, verdrillt, Schirm, Buchsentyp)?
2. Welche Platinen haben die Kunden mit Problemen (Mainboard V1 oder V2)? Gleiche USB-Schaltung?
3. Fehlerbild beim Kunden: nicht erkannt, COM-Port kommt und geht, Abbruch beim Update, Zeichensalat?
4. Wird die Stick-Funktion (USB-A) beim Kunden benutzt? Wenn nicht, ist H1/H2 mit Bestueckung loesbar.
5. Eigene USB-PID beantragen (NXP-Programm) oder eigene VID?
