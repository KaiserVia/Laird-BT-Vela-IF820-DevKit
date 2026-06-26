# Project Status – IF820 Integration in sism555

Stand: 2026-06-16
Strategie: **Klein anfangen.** Zuerst nur prüfen, dass Verdrahtung + Toolchain laufen
(Lebenszeichen vom Modul). Alle größeren HW-/SW-Änderungen sind als Backlog notiert und
werden später Stück für Stück abgearbeitet. Detail-Hintergrund: `Doku/IF820_EZSerial_Konzept_Mapping.md`.

---

## ⚠ Merker: Spannungsversorgung instabil (2026-06-21)

**Unbedingt merken / noch offen.** Symptom: Der PC findet `VIASIS_12SC3456` nur schlecht und
**verliert die Verbindung wieder**; zeitweise erscheinen **gleichzeitig** `Fehler Bluetooth`
*und* `Fehler GSM/GPRS` (Version 5.88 läuft).

Verdacht: **Brownout auf der gemeinsamen 3,3-V-Versorgung.** Das IF820 wird über Fly-Leads
(J4/J5) aus der Hauptplatine gespeist; Sende-Stromspitzen von BT und/oder LTE lassen die Schiene
einbrechen → Modul(e) resetten → Gerät „verschwindet" und kommt wieder. Frühere Indizien:
flackeriger Loopback (erst 1/6 Byte) = marginale Kontakte/Versorgung.

To-Do, wenn wir das angehen:
- DVK testweise über **eigenes USB** versorgen (nur GND + UART teilen) → bleibt es stabil, ist
  es die 3,3-V-Speisung/Kontaktierung.
- **3,3 V unter Last messen** (Oszi/Logikanalysator), Stromreserve prüfen, Stützkondensatoren,
  solide Kontakte an J4/J5.
- Wechselwirkung mit **LTE-Modem-TX** prüfen (gemeinsame Schiene? gleichzeitiges Wegbrechen
  beider Funkmodule deutet darauf hin).
- Optional Firmware-Diagnose: `@E,…,BOOT,…`-Events mitlesen (= Reset-Nachweis); Sleep testweise
  aus (`SSLP,L=00`).

---

## Phase 0 – Bring-up / Lebenszeichen (JETZT)

**STATUS 2026-06-16: ERREICHT.** `/PING` -> `@R,001D,/PING,0000,R=0000006C,F=A6DC` bei
115200 empfangen (45 Byte, sauber), IF820 erkannt ("Bluetooth IF820: 115200"). Ursache der
anfaenglichen RX=0-Fehler: die UART-Kabel waren nicht gekreuzt (RTS<->CTS und TxD<->RxD
vertauscht). Nach korrekter Kreuzung (MCU-CTS<->BT-RTS, MCU-TxD<->BT-RxD) lief der /PING
sofort durch. CTS liegt jetzt auf LOW. TxD/RxD, Pegel, Baudrate und
Toolchain damit bestaetigt.

**Update 5.84:** Flow Control (STU F=1) + CTS/RTS-Leitungen verifiziert: CTS=LOW unter FlowCtl
(Modul treibt RTS aktiv), `/PING` ueber Normalpfad (CTS respektiert) liefert `@R`. 4-Draht-UART
komplett ok. CTS-Bypass-Hack damit nicht mehr noetig. Wurzelursache war nicht gekreuzte
Verkabelung (RTS<->CTS, TxD<->RxD).

**Update 5.85/5.86:** Erkennung auf Normalpfad aufgeraeumt (Debug-Probe + CTS-Bypass entfernt).
Generische Flow-Control-Verwaltung eingebaut: `bt_get_flowcontrol()` / `bt_set_flowcontrol()` /
`bt_ensure_flowcontrol()` mit Dispatch je Modultyp + `default`-Fangnetz (neues Modul wird nicht
still durchgewunken). IF820 voll implementiert: GTU-Abfrage -> bei Bedarf STU F=01 + /SCFG ->
Verifikation per GTU. Flash-Write nur wenn noetig (schont Flash). RN4678/Laird als TODO-Stubs.

**Update 5.87:** BT-Name generisch (Name = VIASIS_<serno>): `bt_get_name()` / `bt_set_name()` /
`bt_ensure_name()`, gleiche Dispatch-Logik + `default`-Fangnetz. IF820: GDN-Abfrage -> bei Bedarf
`SDN$` (schreibt direkt RAM+Flash) -> Verifikation per GDN. Schreibt nur, wenn Name noch nicht
VIASIS_<serno> ist; ohne gesetzte Seriennummer wird uebersprungen. RN4678/Laird als TODO-Stubs.

**Update 5.88:** Bugfix Name. `SDN`/`GDN` haben einen Typ-Parameter `T` (0=BLE, 1=BT-Classic).
Bisher nur BLE-Name (T=0 default) gesetzt -> PC sah im SPP-Scan weiter den Werks-Classic-Namen
"EZ-Serial <MAC>_BT". Jetzt werden BEIDE gesetzt (`SDN$,T=00` und `SDN$,T=01`) und beide
geprueft. Hinweis: BLE-Name ist sofort aktiv, der BT-Classic-Name wird ggf. erst nach
Modul-Power-Cycle im Inquiry sichtbar (SDN$ persistiert in Flash).

Ziel: `/PING` an den IF820 senden und `@R,...,/PING,0000` zurückbekommen. Damit sind
UART-Verdrahtung, Pegel und Toolchain bewiesen. Bewusst **ohne** Steuerpins, ohne SPP-Verbindung.

### Hardware (DVK, Chip-Antennen-Variante)

Versorgung **nur über USB** (einfachster Fall, kein externes 3,3 V):

| Jumper | Stellung | Zweck |
|--------|----------|-------|
| J6 (Ext 5V) | **gesteckt** | 5 V von USB → Onboard-LDO |
| J4 (VBAT) | **gesteckt** (wieder einsetzen) | LDO 3,3 V → Modul VBAT |
| J5 (VDDC/VDDIO) | **gesteckt** (wieder einsetzen) | LDO 3,3 V → Modul VDDIO |
| J9 (Strommessung) | gesteckt | Modulversorgung geschlossen |

PUART zum Mainboard – diese **4 Jumper ABZIEHEN** und Fly-Leads auf die **Modulseite** legen
(trennt den RP2040 von den Leitungen):

| Signal | DVK-Header | → Mainboard (LPC1766 / MUX) |
|--------|-----------|------------------------------|
| P_UART_TXD | J1.24 | RXD1 (P2.1) |
| P_UART_RXD | J1.20 | TXD1 (P2.0) |
| P_UART_RTS | J1.22 | CTS (P2.2) |
| P_UART_CTS | J1.18 | RTS (P1.15) |
| GND | J2.1 | GND (gemeinsame Masse Pflicht) |

Hinweise:
- TX/RX gekreuzt (Modul-TXD → MCU-RXD usw.).
- Für `/PING` reicht prinzipiell TXD/RXD/GND; RTS/CTS trotzdem mitverdrahten, weil
  `Init_BT_ch()` den CTS-Pegel prüft. Flow Control bleibt in SW aber AUS (Default F=00).
- **Während des Tests kein Handy/PC per Bluetooth verbinden** → Modul bleibt im Kommandomodus
  (CYSPP-Pin floating = Command Mode), sonst antwortet `/PING` nicht.
- CYSPP-/CONNECTION-/CP_ROLE-/LP_MODE-Pins bleiben **unverdrahtet** (floating).

### Software (sism555)

- [ ] `btio.h`: Modultyp-Konstante `#define IF820 82` (oder freier Wert), siehe Frage 6 unten.
- [ ] `btio.h`: für Testphase **Baudrate 115200** für BT **und** LTE.
      `BT_BAUD` → 115200, `GSM_BAUD` → 115200 (temporär, später zurück auf 307200/460800).
- [ ] `btio.h`: EZ-Serial-Textmakros: `/PING`, Antwort-Token `@R` / `,0000` / `/PING`.
- [ ] Modulerkennung erweitern (siehe Frage 5): im Auto-Detect bei 115200 `/PING` senden,
      auf `@R,...,/PING,0000` prüfen, bei Erfolg `fp.btmodem=IF820`.
- [ ] Security: **nur SSP „Just Works"** (Factory-Default-SSP belassen), Legacy-PIN
      (`SBTPIN` / `AT+BTK`) **auskommentieren**.
- [ ] Power: Modul **dauerhaft an** lassen – keine Enable-/Power-Cycle-Logik (siehe Frage 4).

### Erfolgskriterium Phase 0
Firmware flashen → über RS232-Terminal die BT-Init/Detect anstoßen → `/PING` geht raus,
`@R,...,/PING,0000` kommt zurück → „IF820 erkannt". Mehr nicht.

---

## Backlog – später, Stück für Stück

### HW-Erweiterungen (verschoben, weil zu viele Änderungen auf einmal)
- [ ] **CONNECTION-Pin** (aktiv LOW) an MCU-Eingang / I2C-Expander → Verbindungserkennung
      (ersetzt RN4678-Status-GPIO). *Voraussetzung für sauberes Connect/Disconnect.*
- [ ] **CYSPP-Pin** an MCU-GPIO (Output) → Daten/Kommandomodus umschalten + SPP-Trennen.
      *Voraussetzung für Reconfig zur Laufzeit und für den 6-Min-Timeout-Disconnect.*
- [ ] **CP_ROLE-Pin** fest HIGH (Peripheral) oder floating.
- [ ] **LP_MODE-Pin** (nur falls Stromsparen gewünscht).
- [ ] **Power/Enable-Konzept**: Modulversorgung (VBAT/VDDIO) schaltbar **oder** Deep Sleep
      (`SSLP` + LP_MODE) statt komplettem Power-Cycle.

### SW-Erweiterungen
- [ ] **Zielbaudrate** 307200 bzw. 460800 mit Flow Control: `STU,B=<hex>,F=01` → Host-Baud
      nachziehen → `/SCFG`. **Vorher mit Oszi prüfen** (nicht-Standard-Baud, Taktfehler <3 %).
- [ ] **Legacy-PIN** wieder aktivieren (`SBTPIN`, 6-stellig aus Seriennummer) – falls SSP
      Just Works nicht reicht.
- [ ] **Persistenz**: vollständige Konfig per `/SCFG` in Flash sichern.
- [ ] **SPP-Datenmodus-Handling**: transparenter Betrieb, Verbindungs-/Trenn-Logik über
      CYSPP/CONNECTION (ersetzt `$$$`/`---`-Logik in `btio.c`).
- [ ] **Gerätename** `SDN,N=VIASIS_<serno>` setzen (BT-Classic-Anzeige bekommt `_BT`).
- [ ] **Sendeleistung** `STXP` auf Max.
- [ ] `btio.c`-Funktionen anpassen: `test_BT`, `send_bt_info`, `set_bt_name`, `set_bt_pin`,
      Timeout-Disconnect in `measure.c`.

### Eval-Tests (aus `Doku/BT-Evalkit-Tasks.md`) – nach Phase 0
- [ ] Test 1 Grundkommunikation transparent
- [ ] Test 2 Flow Control CTS/RTS
- [ ] Test 3 Baudratenwechsel + persistent
- [ ] Test 4 Command-/Datenmodus (CYSPP-Pin)
- [ ] Test 5 Factory Reset + Rekonfig
- [ ] Test 6 Verbindungserkennung (CONNECTION-Pin)
- [ ] Test 7 Xmodem-1K Durchsatz
- [ ] Test 8 Power Cycling
- [ ] Test 9 CTS-Pause (UART1-Sharing)
- [ ] Test 10 BLE Discovery + Classic SPP
- [ ] Test 11 Reichweite / Leistung
- [ ] Test 12 Timing nach Reboot


---

## Fehler-Codes (DTC) — Mechanismus & Registry  (ab Version 5.09)

> **Altes Schema (DTCBASE + `__LINE__`) abgelöst ab Version 5.09.**
> Siehe `Doku/Diagnose-Codes_Konzept.md` und den vollständigen Katalog `Doku/Diagnose-Codes.csv`.

Jeder Fehler hat einen **festen, lesbaren, stabilen** 5-stelligen Code (11001–99999), der sich
bei Quellcode-Änderungen nicht verschiebt. Der Code identifiziert Subsystem und Fehlerart.

**Ausgabe am Terminal:** `Fehler <Code> <Subsystem>` (z. B. `Fehler 11001 Bluetooth`).
Der Code wird zusätzlich in den Flash protokolliert (`protocol(code)`).

**Subsystem-Bereiche:**

| Bereich | Subsystem | Datei |
|---------|-----------|-------|
| 11xxx | Bluetooth | btio.c |
| 12xxx | GSM/LTE | gsmio.c |
| 13xxx | Menue/Kommunikation | sicom.c |
| 14xxx | MQTT | mqtt.c |
| 15xxx | GPS | gpsio.c |
| 16xxx | Flash/Speicher | flash.c |
| 17xxx | System | main.c |
| 18xxx | Hardware/libtool | libtool.c |
| 19xxx | USB | USB_tools.c |
| 20xxx | Systemtest | sictst.c |

**Neuen Code anlegen:** Nächste freie laufende Nummer im Subsystem-Bereich wählen →
Zeile in `Doku/Diagnose-Codes.csv` eintragen → `dtc_codes.h` per `tools/gen_dtc.py`
neu generieren (oder Header manuell ergänzen) → Konstante an der Fehlerstelle verwenden.

**Technisch:** `dtc_codes.h` definiert die Konstanten (`DTC_BT_PING = 11001` usw.) und
die Makros `puterror(code,v)` / `dtcerr(code)`. `sio.c` leitet den Subsystem-Text aus
`code/1000` ab. `dtc.h` ist leer (Tombstone).


## Update 5.90 — IF820-Zweige + GSM-Baudrate variabel

**Vier BT-Funktionen IF820-fähig gemacht** (`btio.c`): `test_BT`, `send_bt_info`, `set_bt_name`,
`set_bt_pin`. Ursache der DTC10384 war, dass `fp.btmodem>Laird` für IF820 (=82) wahr ist und in
den RN4678-Pfad (`$$$`) lief. Jetzt hat jede Funktion einen eigenen `if (fp.btmodem==IF820)`-Zweig:
- `test_BT`: Lebenstest per `/PING`.
- `send_bt_info`: `/PING` + Ausgabe „Bluetooth IF820".
- `set_bt_name`: setzt eingegebenen Namen per `SDN$,T=00` und `SDN$,T=01` (BLE + Classic).
- `set_bt_pin`: PIN ungenutzt (Just Works) → Hinweis, kein Fehler.

**GSM_BAUD jetzt einstellbar:** war hart auf 115200 (Testphase), ist jetzt die Variable
`gsmbaud` (in `hard.c`, `extern` in `hard.h`), Default **460800** (= ursprünglicher Wert, passt
zu `AT+IPR=460800`). Damit könnte auch DTC20311 zusammenhängen, falls es ein Baud-Mismatch war.
`gsmbaud` ist eine RAM-Variable; für persistente Einstellung müsste sie ins `fp`-Parameterblock
(Flash-Layout-Änderung – separat zu machen).


---

## Update 5.91 — /RBT-Automatik nach Namens-Schreiben

`bt_set_name()` (IF820) sendet nach erfolgreichem `SDN$,T=00`+`T=01` ein `/RBT` und wartet per
Dot-Schleife auf das Boot-Signal. Damit geht der **BT-Classic-Inquiry-Name sofort live** (er wird
beim Modul-Boot aus dem Flash gelesen) — kein manuelles Power-Cycle mehr noetig. Reboot nur, wenn
der Name tatsaechlich (neu) geschrieben wird (einmalig bei Konfiguration).

Offen: **SPP-Datenmodus / "verbinden"** — Geraet wird gefunden/gepairt (Name VIASIS_<serno>), aber
der transparente SPP-Datenkanal braucht die CYSPP/CONNECTION-Steuerpins (HW-Backlog). Das ist der
naechste grosse Schritt.


---

## ⚠ HW-Check offen: Pull-up auf CYSPP-Leitung (IC54 Bit7)

CYSPP (DVK J1.14) ist auf die bestehende BT-Status-Leitung **IC54 Bit7** (= Software-Bit
`BT_LINK`, low-aktiv) gelegt, damit die vorhandene Connect-Logik (`communication_change()` in
libtool.c) die SPP-Verbindung erkennt.

**Zu prüfen / ggf. nachrüsten:** CYSPP **floatet**, wenn keine Verbindung besteht – das Modul
treibt den Pin **nur bei SPP-Connect auf LOW**. Damit „nicht verbunden" zuverlässig als **HIGH**
gelesen wird, braucht die Leitung einen **Pull-up (~10 kΩ gegen 3,3 V)**.

Wichtig: Der alte RN4678-Status-Pin trieb HIGH **aktiv** (push-pull) – die Leitung hat daher
evtl. **keinen** Pull-up. Ohne Pull-up → erratische/falsche Verbindungserkennung (BT_LINK
flackert). Also: Pegel der CYSPP/IC54-Bit7-Leitung im **unverbundenen** Zustand messen; ist er
nicht sauber HIGH, **10 kΩ gegen 3,3 V** ergänzen.

(Sauberer waere der dedizierte CONNECTION-Pin des IF820 – der ist aber am DVK nicht als
Header-Pin herausgefuehrt.)


---

## MEILENSTEIN (5.92): SPP-Datenmodus + Menue ueber Bluetooth funktioniert

Mit **CYSPP (DVK J1.14) -> IC54 Bit7** (ohne harten Pull-up) laeuft die volle Menuebedienung
ueber Bluetooth Classic (SPP):
- Bei SPP-Connect treibt das **Modul CYSPP selbst auf LOW** -> Datenmodus aktiv (transparenter
  UART), und IC54 Bit7 liest LOW -> `BT_LINK` aktiv -> `communication_change()` routet das
  Hauptmenue an UART1. Eingaben funktionieren problemlos.
- Geraetenamen: BLE = `VIASIS_<serno>`, Classic = `VIASIS_<serno>_BT`.

Wichtig: **Keinen harten Pull-up** auf die CYSPP/IC54-Leitung legen — ein erzwungenes HIGH sagt
dem Modul "Kommandomodus" und kann den SPP-Connect verhindern. Das Modul treibt die Leitung
selbst.

**Residual / noch zu beobachten:** Im **unverbundenen** Zustand floatet CYSPP -> IC54 Bit7 kann
erratisch lesen -> evtl. sporadisch falsches `BT_LINK` / Fremdzeichen / Spontan-DTC (z. B. das
fruehere DTC31405). Falls das stoert: in SW abfangen (BT_LINK entprellen / nur stabile Flanke
werten, bzw. EZ-Serial-Events filtern) — NICHT per Pull-up (Konflikt mit SPP-Connect).


---

## Update 5.93 — Geraetename vereinheitlicht (kein _BT)

Auf Wunsch einheitliches Verhalten fuer BEIDE Chips: BLE **und** BT-Classic heissen jetzt gleich
`VIASIS_<serno>` (das `_BT`-Suffix aus 5.88/5.92 wieder entfernt). Hintergrund: der RN4678 kann
gar keine getrennten BLE/Classic-Namen (`SN` setzt EINEN Namen fuer beide, lt.
RN4678-Command-Reference). Damit IF820 und RN4678 gleich aussehen, beim IF820 ebenfalls ein Name
fuer beide (`SDN$,T=00` und `SDN$,T=01` -> identisch). Beim naechsten BT-Init schreibt
`bt_set_name` den Classic-Namen einmal um (+/RBT), danach zeigen beide Transporte `VIASIS_<serno>`.


---

## Update 5.95 - Sicheres Pairing = bestehende App-Layer-PIN (fuer IF820 aktiviert)

Entscheidung Sicherheit: KEIN BT-Pairing-Passkey (beim IF820 headless nur ueber den als
"obsolete" markierten Fixed-Passkey moeglich; Classic braeuchte einen Laufzeit-BTPIN-Handler).
Stattdessen die **bereits vorhandene, erprobte App-Layer-PIN** nutzen - sie sitzt auf der
Menue-/Datenebene und ist damit transportunabhaengig (wirkt identisch ueber Classic SPP, BLE,
RS232 -> gleich auf Windows, Android, iOS).

Mechanik (war fuer RN4678 schon aktiv):
- communication_change() setzt bei BT-Verbindung bt_pininit=1 -> statt Menue wird T_pin-Prompt
  gesendet (libtool.c).
- PIN-Eingabe-Handler in sicom.c (~2128-2150): Ziffern als '*' quittiert, Vergleich gegen
  fp.btpin (6-stellig aus Seriennummer). Treffer -> Hauptmenue. 6 Fehlversuche -> Reset.
  Trennen -> bt_pininit=0.

Aenderungen 5.95:
- libtool.c:1044: Bedingung erweitert -> `fp.btmodem==Roving || fp.btmodem==IF820`.
- btio.c IF820-Zweig: fp.btpin (=atoi(serno)*10000 + letzte4Digits) jetzt auch fuer IF820
  berechnet (vorher erst nach dem IF820-Early-Return, also uebersprungen).

Hinweis: PIN schuetzt nur BT-Verbindungen, nicht den lokalen RS232-Zugang (wired = trusted).
Verschluesselung des Funklinks (LE Secure Connections + Bonding) kann spaeter zusaetzlich ueber
SSBP konfiguriert werden, ist aber von der Zugangs-PIN unabhaengig.


---

## Update 5.96 - BT_BAUD IF820 = 460800, zentral an EINER Stelle

Einzige Stellschraube: `#define IF820_BAUD 460800` in btio.h. Alles leitet sich ab:
- `BT_BAUD` nutzt IF820_BAUD.
- STU-Kommando wird zur Laufzeit aus IF820_BAUD gebaut (8 Hex-Ziffern), kein hartcodierter
  Baud-String mehr (T_stuf1 entfernt -> T_stu_post = nur noch Suffix A/C/F/D/P/S).
- Erkennung/Erstkonfiguration nutzen IF820_BAUD bzw. IF820_FACTORY_BAUD (115200, fix).

Baudwechsel-Mechanik (laut Doku Example 1 / Abschnitt 2.5.3 "protected settings"):
STU ist protected. Ablauf: STU (RAM) auf alter Baud senden -> Antwort kommt noch auf alter Baud
-> Host auf Zielbaud umstellen (Init_BT_ch(IF820_BAUD)) -> /SCFG auf neuer Baud -> Flash +
verifiziert. So kann kein Lockout entstehen (Flash-Write nur wenn Kommunikation auf neuer Baud
klappt). Umgesetzt in bt_set_flowcontrol() (STU setzt Baud UND F=01 zusammen).

Erkennung (init_bluetooth): zuerst /PING @ IF820_BAUD (konfiguriertes Modul), sonst @ 115200
(Werksmodul). Bei Werks-Baud wird per bt_set_flowcontrol() auf Zielbaud umgestellt+gesichert.
Erstinbetriebnahme laeuft automatisch: main.c:160 ruft bei fehlendem test_BT() init_bluetooth().

Aendern auf z. B. 921600: nur IF820_BAUD anpassen. Hinweis Doku: nicht-Standard-Baud <3 %
Taktfehler, mit Oszi/Logikanalysator pruefen; LPC1766-UART1-Teiler muss die Rate sauber treffen.


---

## Update 5.97 - Bugfix: Baud-Selbstheilung (Modul hing auf alter Baud)

Symptom (5.96): nach Umstellung auf 460800 kam ueber die BT-Verbindung nur Muell an.
Ursache: fp.btmodem=IF820 war aus alter Version im Flash; das Modul stand physisch noch auf
115200. init_bluetooth() bricht am Guard (fp.btmodem!=0) ab, und der Start rief bei gesetztem
btmodem gar kein init_bluetooth() auf (nur syserror++). Folge: Firmware sendet 460800, Modul-PUART
liest auf 115200 -> Muell, der transparent ueber SPP an den PC weitergereicht wird.

Wichtig: Bei echtem Bluetooth-SPP ist die Baud des virtuellen COM-Ports am PC (viagraph 115200)
KOSMETISCH - die Strecke ist byte-transparent. Der Mismatch lag auf der WIRED PUART (LPC1766 <-> IF820).

Fix (main.c): schlaegt test_BT() fehl, wird fp.btmodem=0 gesetzt und init_bluetooth() erneut
gerufen -> Zwei-Baud-Erkennung findet das Modul auf 115200 und stellt per STU+/SCFG auf
IF820_BAUD (460800) um. Selbstheilung beim naechsten Boot, danach laeuft alles auf 460800.

Manuelle Alternative (ohne Reflash, mit 5.96): im Konfig-Menue BT deinstallieren (BT? Nein ->
fp.btmodem=0), dann Konfig erneut, BT? Ja -> init_bluetooth() laeuft und stellt um.


---

## Update 5.98 - IF820-Erkennung scannt mehrere Baudraten (+ Parser-Flush)

Symptom (5.97): beim Konfigurieren schlug /PING auf beiden Baudraten fehl (/PING/PING im Log),
danach lief die Laird/RN4678-Kaskade und endete mit DTC10395.

Zwei Ursachen behoben:
1. Modul-Parser verstopft: der /PING-Versuch auf der falschen Baud schickt dem Modul Muell, der
   bis zum naechsten CR im Eingabepuffer bleibt -> der folgende /PING wird angehaengt und nicht
   erkannt. Fix: vor jedem /PING ein CR senden (bt_command(T_CR,...,-60)) -> Modul schliesst die
   (Muell-)Zeile ab, Puffer sauber.
2. Erkennung war zu eng (nur 2 Baudraten). Jetzt scannt die IF820-Erkennung - analog zu den
   Laird/RN4678-Kaskaden - eine Liste: { IF820_BAUD, 115200, 921600, 307200, 230400, 9600 }.
   Erster Treffer gewinnt (detbaud), danach Umstellung auf IF820_BAUD via bt_set_flowcontrol(),
   falls noetig. Zielbaud steht zuerst -> schnellster Treffer bei bereits konfiguriertem Modul.

Wird auf Werks- oder Zwischen-Baud gefunden, stellt init_bluetooth automatisch auf IF820_BAUD um
(STU + Host-Umschaltung + /SCFG).


---

## HW-Aenderungswunsch: CYSPP geraeteseitig treibbar (Timeout-Disconnect + Reconfig im Betrieb)

**Ziel:** Die Hauptplatine soll eine bestehende BT-Verbindung aktiv trennen koennen
(Inaktivitaets-Timeout wie beim RN4678) und den Command-Mode erzwingen koennen, um das Modul
auch waehrend des Betriebs zu (re)konfigurieren.

**Hintergrund (aus EZ-Serial Guide + DVK-Guide verifiziert):**
- CYSPP ist bidirektional. CYSPP HIGH = Command-Mode; CYSPP LOW = Daten-/SPP-Mode.
- Bei SPP-Verbindung treibt das Modul CYSPP selbst auf LOW. Treibt ein externer Master CYSPP auf
  HIGH, wird die SPP-Verbindung beendet und das System geht in den Command-Mode zurueck.
- DVK-Guide, Note 1 zu CYSPP (J1.14, Chip-Antennen-Variante): "Required for dropping an SPP
  connection in EZ-Serial transparent parse mode." -> exakt unser Pin.
- Wichtig: CYSPP muss beim BOOT HIGH oder floating sein, sonst ist der Command-Parser inaktiv.

**Verdrahtung - KEINE Umverdrahtung noetig:**
- CYSPP (J1.14) ist bereits an IC54 = PCA9554PW, Bit 7 angeschlossen (bisher nur als Eingang
  gelesen = Verbindungsstatus / BT_LINK).
- Der PCA9554PW hat ECHTE Push-Pull-Ausgaenge (anders als PCF8574) und kann den Pin per I2C
  zwischen Eingang und Ausgang umschalten und aktiv HIGH treiben. Die vorhandene Leitung reicht.

**Optionale HW-Massnahme (Schutz, nicht zwingend):**
- Serienwiderstand ~470 Ohm in die CYSPP-Leitung (PCA9554 Bit7 <-> J1.14) gegen den kurzen
  Treiber-Konflikt, bis das Modul SPP beendet und CYSPP loslaesst. Das DVK-Referenzdesign treibt
  direkt (ohne Widerstand), also ist Direktbetrieb vorgesehen; der Widerstand ist reine Vorsicht.
- KEIN harter Pull-up auf CYSPP (frueheres Problem: falsches BT_LINK / Modul kann LOW nicht
  durchsetzen). Boot-Zustand: PCA9554 startet mit allen Pins als Eingang -> CYSPP floatet ->
  Command-Mode. Firmware darf Bit 7 beim Start nicht auf Ausgang-LOW setzen.

**Firmware-Teil (eigentliche Arbeit, noch offen):**
1. Helfer bt_cmdmode() = PCA9554 Bit 7 als Ausgang HIGH (CYSPP HIGH -> SPP trennen / Command-Mode);
   bt_release() = Bit 7 zurueck auf Eingang (Status lesen, bereit fuer naechste Verbindung).
   I2C-Zugriff analog get_com_status() auf IC54 (Direction- + Output-Register des PCA9554).
2. Inaktivitaets-Timeout-Disconnect: an bestehende bt_time/BT_TIMEOUT-Logik (measure.c) haengen ->
   bei Timeout CYSPP HIGH pulsen -> SPP getrennt -> wieder loslassen.
3. Reconfig im Betrieb: in init_bluetooth vor dem Senden ggf. bt_cmdmode() (Command-Mode erzwingen),
   danach bt_release(). Loest zugleich das Problem "Konfiguration scheitert, weil PC verbunden ist"
   (Befehle gingen sonst transparent ueber SPP an den PC statt ans Modul).

**Status:** Wunsch dokumentiert. Firmware-Umsetzung noch offen (auf Freigabe wartend).


---

## Update 5.99 - CYSPP-Steuerung: Timeout-Disconnect + Reconfig-im-Betrieb (Firmware)

Setzt den HW-Aenderungswunsch in SW um - OHNE Umverdrahtung (PCA9554 Bit 7 ist Push-Pull).

Neu (btio.c, Prototypen in btio.h):
- bt_cmdmode(): PCA9554 IC54 Bit 7 als Ausgang HIGH -> CYSPP HIGH -> laufende SPP-Verbindung
  trennen + Command-Mode erzwingen. (Output-Reg 0x01 Bit7=1, dann Config-Reg 0x03 = 0x7F.)
- bt_release(): Config-Reg 0x03 = 0xFF -> Bit 7 wieder Eingang -> Status lesbar, bereit fuer
  naechste Verbindung. (= Init-/Werkszustand.)

Eingebaut:
1. Inaktivitaets-Timeout-Disconnect (measure.c): wenn bt_time/BT_TIMEOUT abgelaufen, bei
   fp.btmodem==IF820 zusaetzlich bt_cmdmode()->osDelay(100)->bt_release() -> SPP physisch getrennt
   (nicht nur virtuell wie bisher). Parity zum RN4678.
2. Reconfig im Betrieb (init_bluetooth): ist fp.btmodem bereits IF820, wird am Anfang
   bt_cmdmode() ausgefuehrt (CYSPP HIGH -> SPP trennen + Command-Mode), die virtuelle Verbindung
   geloest und fp.btmodem=0 gesetzt -> die Baud-Scan-Erkennung laeuft erneut und kann
   konfigurieren, auch wenn vorher ein PC verbunden war. Loest zugleich das Problem
   "Konfiguration scheitert, weil PC verbunden ist" (Befehle gingen sonst transparent ueber SPP).
3. bt_release() am Ende des IF820-Blocks und als Fallback vor der Laird/RN4678-Kaskade, damit
   Bit 7 nie als Ausgang haengenbleibt (auch wenn nach erzwungenem Command-Mode kein IF820
   erkannt wird).

Boot bleibt sicher: PCA9554 startet mit allen Pins als Eingang -> CYSPP floatet -> Command-Mode.

Offen/optional (HW): Serienwiderstand ~470 Ohm in der CYSPP-Leitung als Schutz gegen den kurzen
Treiber-Konflikt beim HIGH-Treiben (DVK-Referenz treibt direkt, daher nicht zwingend).


---

## Update 6.00 - Verzoegerter BT-Reconfig ueber die BT-Schnittstelle (+ Hinweis)

Problem: Reconfig ueber BT kappt die eigene SPP-Verbindung (Command-Mode noetig). Loesung:
- Neues Flag bt_reconfig_pending (hard.c/hard.h).
- Trigger (sicom.c, "Bluetooth (J/N)?"): ist man UEBER BT verbunden
  ((connect&(UART1|BT_LINK))==(UART1|BT_LINK)), wird NICHT sofort konfiguriert (das wuerde den
  Link mitten in der Menuesequenz kappen). Stattdessen: Flag setzen + kurze Hinweis-Nachricht
  "BT-Modul wird beim Trennen neu konfiguriert - kurz keine Verbindung". Ueber RS232/USB laeuft
  init_bluetooth() wie bisher direkt.
- Ausfuehrung (measure.c, nach communication_change): sobald die BT-Verbindung getrennt ist
  (manuell oder per 6-Min-Timeout), feuert die verzoegerte Konfiguration:
  if (bt_reconfig_pending && !(connect&(UART1|BT_LINK))) { bt_reconfig_pending=0; init_bluetooth(); }
  init_bluetooth erzwingt selbst den Command-Mode (bt_cmdmode) und konfiguriert das Modul.

Ablauf fuer den Anwender: ueber BT "Bluetooth konfigurieren" waehlen -> Hinweis erscheint ->
Menue normal zuende -> BT-Verbindung trennen -> Modul wird automatisch neu konfiguriert ->
wieder verbinden.


---

## Update 5.02 - Durchsatz: SPP-Baud erhoeht; BLE-Analyse

Classic SPP (Android/Windows): PUART war mit 460800 (~368 kbit/s) der Engpass gegenueber dem
BR/EDR-Luftdurchsatz. IF820_BAUD -> 921600 (eine Stelle in btio.h; STU/Erkennung/Self-Heal leiten
sich ab; 921600 ist im Baud-Scan enthalten -> Recovery moeglich). Hinweis Doku: nicht-Standard
sauber, aber bei 921600 Bit-Timing der LPC1766-UART1 mit Oszi pruefen (<3% Fehler). Zurueck auf
460800 = nur IF820_BAUD aendern.

BLE (iOS/Android) - Recherche-Ergebnis:
- MTU, Data-Length-Extension, 2M-PHY werden vom Stack beim Connect automatisch ausgehandelt -
  kein Config-Befehl beim IF820 (kein set_mtu/set_phy im API).
- Dominanter Faktor = Connection Interval, bestimmt vom Handy (iOS deckelt bei 30 ms; Android
  startet 48,75 ms, ab 4.4.3 bis 7,5 ms). Als Peripheral kann das Modul nur per /UCP zur Laufzeit
  ein kuerzeres Intervall anfordern - kollidiert mit dem transparenten CYSPP-Datenmodus.
- Einziger sauberer Config-Hebel: Sleep im Connection abschalten (.CYSPPSP P=0) - modest, da der
  Engpass das Handy/Intervall bleibt. Power egal (Geraet ist versorgt).
- Voller BLE-Durchsatz (kurzes Intervall erzwingen) braucht die aufgeschobene Event-Parsing-
  Architektur: CYSPP NICHT auto-starten -> Connect-Event im Command-Mode parsen -> /UCP senden ->
  CYSPP-Datenpipe starten (.CYSPPSTART). iOS bleibt trotzdem bei 30 ms.

Status: SPP umgesetzt (5.02). BLE-Config-/Architektur-Schritte offen (auf Anwender-Entscheidung).


---

## BLE-Durchsatz: moegliche Schritte (Backlog, noch NICHT umgesetzt)

Ausgangslage / Fakten (verifiziert aus EZ-Serial Guide, Abschnitt 3.8.1):
- MTU, Data-Length-Extension (DLE) und 2M-PHY werden beim Connect vom Stack AUTOMATISCH
  ausgehandelt. Der IF820 hat dafuer KEINEN API-Befehl -> hier ist nichts zu tun/optimieren.
- Dominanter Durchsatz-Faktor = Connection Interval. Das bestimmt der Central (das Handy):
  iOS deckelt hart bei 30 ms; Android startet bei 48,75 ms, ab 4.4.3 bis 7,5 ms anforderbar.
- Geraet ist Peripheral -> kann ein kuerzeres Intervall nur per gap_update_conn_parameters
  (/UCP, ID=4/3) zur LAUFZEIT anfordern; Central darf ablehnen (ohne Rueckmeldung -> nur
  ausbleibendes CU-Event zeigt Ablehnung).
- Problem: Im transparenten CYSPP-Datenmodus kann die Firmware keine /UCP-Kommandos senden
  (alles geht als Daten ueber die Luft).

### Option A - No-Sleep waehrend der Verbindung (klein, config-only)
Nutzen: modest (haelt das Modul wach -> mehr Pakete pro Intervall moeglich). Engpass bleibt
Handy/Intervall. Power egal (Geraet ist versorgt).
To-Do:
1. .CYSPPSP (p_cyspp_set_parameters, ID=10/3) mit allen Werks-Defaults, aber P=00 senden:
   `.CYSPPSP,E=02,G=00,C=0131,L=00000000,R=00000000,M=00000000,P=00,S=00,F=02`
   (Werks-Defaults: E=2 enable+autostart, G=0 peripheral, C=0x0131, L/R/M=0, P=1 sleep,
   S=0, F=0x02 RX-flow.)
2. Mit /SCFG persistieren; Antwort-Token "CYSPPSP,0000" pruefen.
3. Sauber als ensure-Muster (erst Get .CYSPPGP/GCYSPPSP, nur schreiben wenn P!=0) -> schont Flash.
4. In init_bluetooth (IF820-Block) einhaengen, byte-sicher (ISO-8859), Versionsbump.
Risiko: gering, aber .CYSPPSP setzt die GANZE CYSPP-Config -> Default-Werte exakt treffen,
sonst koennte CYSPP brechen. Vorher .CYSPPGP auslesen und Ist-Werte uebernehmen.

### Option B - Kurzes Intervall erzwingen via /UCP (gross, Architektur)
Nutzen: hoch auf Android (bis 7,5 ms). iOS bleibt bei 30 ms (Hard-Cap) -> dort kein Gewinn.
Voraussetzung: die aufgeschobene EZ-Serial-Event-Parsing-Architektur auf UART1.
To-Do:
1. CYSPP NICHT auto-starten: .CYSPPSP E=01 (enable, ohne autostart) statt E=02.
2. Firmware muss im Command-Mode die GAP-Events auf UART1 mitlesen und parsen:
   - gap_connected (C/CONNECTED-Event) -> Verbindung erkannt.
3. Direkt nach Connect /UCP senden, z. B. Android: `/UCP,C=<conn>,I=6,L=0,O=64` (6*1,25=7,5 ms),
   iOS: I=18 (30 ms). Conn-Handle aus dem Connect-Event.
4. Auf gap_connection_updated (CU)-Event warten (bestaetigt; Ausbleiben = abgelehnt).
5. Erst danach CYSPP-Datenpipe starten: .CYSPPSTART (p_cyspp_start, ID=10/2).
6. Beim Disconnect zurueck in den Wart-/Advertise-Zustand.
Aufwand: deutlich. Bedingt einen UART1-Event-Parser (loest zugleich das alte Problem
"Modul-Events plappern in die Menue-Eingabe"). Gut mit der CYSPP-Steuerung (bt_cmdmode/
bt_release) kombinierbar.

### Empfehlung / Reihenfolge
- iOS: kein lohnender BLE-Durchsatz-Hebel (30-ms-Cap) -> so lassen.
- Android-BLE: wenn wirklich noetig, Option B; sonst Option A als kleiner Schritt.
- Classic SPP (Android): bereits per PUART 921600 optimiert (Update 5.02).


---

## Update 5.03 - 921600 zurueckgenommen (Hardware-Limit), zurueck auf 460800

Symptom nach Flash von 5.02 (921600): wieder Datenmuell. Ursache NICHT der Baud-Mismatch
(den loest die Selbstheilung), sondern dass 921600 auf dem LPC1766 offenbar nicht mit
ausreichend kleinem Taktfehler erzeugt wird -> physikalisch unsauberer Link, unabhaengig davon
worauf sich beide Seiten einigen. (Genau der zuvor markierte Oszi-Vorbehalt.)

Fix: IF820_BAUD zurueck auf 460800 (bewaehrt, ~368 kbit/s, lief stabil). 5.02-SPP-Optimierung
damit verworfen. Empfehlung: SPP bei 460800 belassen - in der Praxis ausreichend. Hoehere Baud
NUR nach Verifikation mit Oszi/Logikanalysator (LPC1766-UART1-Bit-Timing <3% Fehler);
Fractional-Divider-Wert pruefen. Single-Point-Design: spaeter ggf. wieder testbar ueber IF820_BAUD.

Recovery: Modul stand vermutlich noch auf 460800 (der 921600-Reconfig duerfte am /SCFG bei 921600
gescheitert sein). Nach Flash von 5.03 findet der Baud-Scan das Modul auf 460800 -> stabil.


---

## Update 5.04 - Verzoegerten BT-Reconfig (5.00) wieder entfernt (war SW-Bug)

Symptom: Bei Konfiguration ueber RS232 (mit parallel offener BT-Verbindung) wurde BT NICHT
initialisiert - statt init_bluetooth kam nur "BT module will be reconfigured after disconnect".
Spaeter feuerte der verzoegerte Reconfig mitten im Testmenue, flutete die RS232 mit dem
/PING-Scan + Laird/Micro-Kaskade und scheiterte (LTE war da schon aktiv).

Ursache: Trigger-Bedingung (connect & BT_LINK) feuert, sobald IRGENDEINE BT-Verbindung offen ist
- auch wenn der Anwender tatsaechlich ueber RS232 konfiguriert. "Konfiguration laeuft ueber BT"
ist mit dem connect-Status nicht zuverlaessig unterscheidbar. Der verzoegerte Reconfig richtete
mehr Schaden an als Nutzen.

Fix: Deferred-Reconfig (5.00) komplett zurueckgebaut:
- sicom.c: case 1 wieder `init_bluetooth();` direkt.
- measure.c: verzoegerten Check entfernt.
- hard.c/hard.h: Flag bt_reconfig_pending entfernt.
Damit laeuft init_bluetooth direkt beim "Bluetooth?y" - also VOR der GSM-Aktivierung -> saubere
Erkennung, kein zufaelliges Spaeter-Feuern.

Konsequenz/Einschraenkung: BT-Modul-Reconfig ueber eine reine BT-Verbindung kappt weiterhin die
eigene Verbindung mitten in der Sequenz (init_bluetooth -> bt_cmdmode). BT-Konfiguration daher
ueber RS232 durchfuehren. (Akzeptierte Einschraenkung statt des fehleranfaelligen Deferrals.)


---

## Vorgehen: BT-Modul (re)konfigurieren  (bestaetigte Prozedur, Stand 5.04)

Das IF820 kann nur im COMMAND-Mode konfiguriert werden. Solange eine BT/SPP-Verbindung offen ist,
ist das Modul im DATENMODUS (CYSPP LOW) und reicht /PING & Co. transparent durch -> die Erkennung
scheitert auf ALLEN Baudraten (kein echtes Antworten).

Zuverlaessige Prozedur:
1. BT-Verbindung TRENNEN (z. B. viagraph/Handy disconnect).
2. Dann konfigurieren (Bluetooth?y / init_bluetooth) -> Modul im Command-Mode, /PING-Scan greift.
3. Danach wieder verbinden.

Alternativ: Konfiguration komplett ueber RS232 (vom BT-Status unabhaengig).

Hinweis: Reflashen des LPC1766 (SWD) setzt das IF820 NICHT zurueck. Haengt das Modul in einem
seltsamen Zustand, hilft ein echter Power-Cycle (ganzes Geraet inkl. IF820-Versorgung stromlos).

(Der frueher versuchte automatische "verzoegerte Reconfig" wurde in 5.04 entfernt, weil sein
Trigger ueber connect&BT_LINK nicht zuverlaessig "Konfiguration laeuft ueber BT" erkennen konnte.)


---

## Update 5.05 - init_bluetooth kappt jetzt IMMER aktive BT-Verbindung (vorher Luecke)

Problem: bt_cmdmode() (SPP ueber CYSPP/IC54 Bit7 kappen + Command-Mode erzwingen) wurde NUR im
Zweig `if (fp.btmodem==IF820)` aufgerufen. Nach einer fehlgeschlagenen Erkennung ist fp.btmodem=0
-> Block uebersprungen -> nicht gekappt -> Modul blieb im Datenmodus -> /PING scheiterte auf allen
Baudraten. Deshalb half nur manuelles Trennen.

Fix (btio.c init_bluetooth):
- Alten Guard `if ((connect&UART1)|(fp.btmodem!=0)) return;` ersetzt durch `if (uart1_owner==MUX_GSM)
  return;` (nur noch Konfliktschutz gegen aktive GSM-Nutzung von UART1).
- Danach IMMER: bt_cmdmode() -> osDelay(600) (statt 200) -> connect&=~(UART1|BT_LINK) -> fp.btmodem=0
  -> Scan. So wird vor jeder Erkennung der Command-Mode erzwungen, egal ueber welchen Pfad
  (Menue, Self-Heal, nach Fehlversuch).
- bt_release() am Ende / Fallback bleibt (CYSPP zurueck auf Eingang).

Status-Frage: zuverlaessiger "Command-Mode erreicht"-Status ist die /PING-Antwort selbst (der Scan).
IC54 Bit7 zuruecklesen taugt nicht, solange wir ihn selbst HIGH treiben.

OFFEN/zu testen: ob das PCA9554-HIGH das modul-seitige CYSPP-LOW (bei aktiver SPP) elektrisch
UEBERSTEUERT. Wenn der Auto-Drop bei verbundener BT-Verbindung jetzt funktioniert -> Luecke war
rein SW. Wenn NICHT -> elektrisches Override-Problem -> Serienwiderstand (~470 Ohm) in CYSPP noetig
bzw. manuelles Trennen beibehalten.


---

## Update 5.06 - SDN$ (Namensetzen) gegen Flash-Write-Timing abgehaertet

Bestaetigt: Auto-Kill (5.05) funktioniert - Erkennung lief bei NICHT getrennter BT-Verbindung
sauber durch (BT-COM still statt Muell). Verbliebenes intermittierendes Problem: SDN$,T=01
(Classic-Name) -> DTC10185, nach Neuverbinden weg.

Ursache: bt_set_name sendet zwei SDN$ direkt hintereinander; jedes schreibt Flash. Waehrend des
Flash-Writes blockt das Modul (bei Flow Control) den Eingang. Wenn T=01 + Flash-Write den 500-ms-
Timeout ueberschritt, schlug bt_command fehl.

Fix (btio.c bt_set_name, IF820): Timeout 500 -> 1500 ms, 300 ms Settle-Pause zwischen T=00 und T=01,
und 1 Retry fuer T=01. Damit robust gegen Flash-Write-Timing.

Gesamtstand BT-(Re)Init: Auto-Kill der aktiven SPP-Verbindung beim Initialisieren funktioniert
(5.05), Namensetzen robust (5.06). Manuelles Trennen ist nicht mehr noetig.


---

## Update 5.07 - Bluetooth-Deinstallation trennt jetzt auch die Verbindung

Beobachtung: "config -> Bluetooth -> n" (deinstallieren) liess die aktive BT/SPP-Verbindung
bestehen - nur die Firmware-Flags (fp.btmodem=0, BT_LINK) wurden zurueckgesetzt. Das Handy/COM
blieb "verbunden", waehrend das Geraet BT ignorierte. Inkonsistent.

Fix (sicom.c, case -1): vor dem Zuruecksetzen der Flags bei IF820 die SPP-Verbindung kappen
(bt_cmdmode -> osDelay(300) -> bt_release) und connect&=~(UART1|BT_LINK). Deinstallieren =
Verbindung wird jetzt auch physisch getrennt.


---

## Update 5.08 - Verbindung aktiv per /DIS trennen + Verifikation vor Reboot

Wichtiger Hinweis vom Anwender: die BT-Verbindung war waehrend der Konfiguration NOCH AKTIV.
Erkenntnis: bt_cmdmode (CYSPP HIGH) schaltet nur die PUART in den Command-Mode (Modul antwortet
auf Kommandos, BT-COM wird still = nichts mehr durchgepiped) - terminiert aber die SPP-Verbindung
NICHT. Die aktive Classic-Verbindung stoerte das Setzen/Lesen des Classic-Namens
(SDN$,T=01 brauchte Retry, GDN,T=01-Verifikation schlug fehl, DTC10189).

Fixes:
1. Nach der Erkennung wird die offene Verbindung jetzt AKTIV per Kommando geschlossen:
   gap_disconnect /DIS (T_dis/T_disok) -> @R,...,/DIS,0000. Das beendet die SPP wirklich
   (was CYSPP-HIGH bei unserer Beschaltung nicht tut). Best-effort (kein Connect -> Timeout egal).
2. Namens-Verifikation laeuft jetzt VOR dem Reboot: bt_set_name setzt nur noch (SDN$ T=00/T=01,
   kein /RBT mehr). bt_ensure_name: set -> verify (Modul stabil, keine Verbindung mehr) -> erst
   DANN bt_reboot() (neuer Helfer) um den Classic-Namen live zu machen. Vorher lief die
   Verifikation direkt nach /RBT (Modul am Booten) -> GDN,T=01 unzuverlaessig.

Ablauf bei Config mit aktiver Verbindung jetzt: bt_cmdmode (Command-Mode) -> Scan/Erkennung ->
/DIS (Verbindung schliessen) -> Flow Control -> Name setzen+verifizieren -> /RBT (Classic live).


---

## Update 5.09 - Diagnose-Codes vereinheitlicht (alt+neu) mit Typ-Praefix F/W/I

Umgesetzt (Vorschlag des Anwenders: alte Eintraege in CSV migrieren, alte Tabellen abl: oesen):

Katalog (Doku/Diagnose-Codes.csv) - EINE Quelle fuer alles:
- Neue Spalte Typ (F=Fault, W=Warning, I=Info/Ereignis, englische Konvention).
- 68 neue Fehlercodes (11xxx-20xxx) = Typ F.
- 14 Ereignisse (alte evtxt, Nummern 1-50) = Typ I, Status aktiv (werden weiter geschrieben).
- 28 Alt-Fehlercodes (alte errtxt, Nummern 201-1001) = Typ F, Status veraltet (nur fuer
  Bestands-Logs). Alte und neue Nummern ueberschneiden sich nicht (alt <=1001, neu >=11001).

Generator (tools/gen_dtc.py): liest die CSV -> erzeugt dtc_text.h (Code -> Typ + Kurztext,
110 Eintraege). Einzige Quelle = CSV, kein Drift. dtc_text.h wird NUR in sio.c per #include
eingebunden (kein Keil-Projekt-Eintrag noetig).

Firmware:
- sio.c: dtc_text(code,&typ) Lookup ueber die generierte Tabelle; put_dtc(code) gibt einheitlich
  "F<code> <Kurztext>" aus (Kurztext aus Tabelle, sonst Subsystem). Die drei Fehler-Ausgaben
  (puterrstr_dtc/dctext/puterror_dtc) nutzen jetzt put_dtc. Ausgabe also z. B. "F11001 PING keine Antwort".
- sicom.c: Protokoll-Anzeige nutzt dtc_text -> "<Typ><Code> <Kurztext>" (F/W/I) bzw. nur Nummer
  bei unbekanntem Code. errtxt/evtxt/number_exists werden hier nicht mehr verwendet.
- sio.h: Prototypen dtc_text/put_dtc.

OFFEN (Cleanup, Build-sicher spaeter): die alten Tabellen errtxt/errno/evtxt/evno (+ ggf.
number_exists) sind jetzt UNBENUTZT, aber noch definiert (sprachen/*.c, sictxt.h). Koennen
spaeter entfernt werden - bewusst NICHT jetzt, um den Build nicht blind ueber mehrere
Sprachdateien zu brechen.

WICHTIG: Firmware-Aenderungen konnten hier nicht kompiliert werden -> in Keil bauen und testen.
Neue Diagnose hinzufuegen: Zeile in CSV -> tools/gen_dtc.py laufen lassen -> dtc_text.h aktuell;
Konstante in dtc_codes.h ergaenzen und an der Fehlerstelle verwenden.


---

## Update 5.10 - TEMP Bring-up neues Mainboard (eigener Git-Branch, spaeter zuruecknehmen)

Kontext: neues Mainboard, Verkabelung TXD/RXD/CTS/RTS unklar, Modul-Firmware unklar.
Alle Aenderungen TEMPORAER, ueber EINEN Schalter rueckbaubar. Plan: Doku/Bringup_Plan.md.

Schalter: #define BRINGUP_DEBUG 1 in btio.h. (0 = normales Verhalten.)

Aenderungen:
- main.c: Auto-Init (BT-nicht-gefunden -> init_bluetooth + automatische Konfiguration) unter
  #if BRINGUP_DEBUG DEAKTIVIERT (else syserror++). Bei unklarer Verkabelung nichts auto-konfigurieren.
- btio.c: neue CTS-UNABHAENGIGE Debug-Konsole bt_bringup() (#if BRINGUP_DEBUG): sendet direkt
  ueber U1->THR (Auto-RTS/CTS aus, kein CTS-Warten); Antworten roh als hex+ascii. Funktionen:
  Loopback (TXD<->RXD-Jumper), /PING, Baud-Scan /PING (9600..921600), eigenes Kommando, Baud+.
- btio.h: BRINGUP_DEBUG-Schalter + Prototyp bt_bringup.
- sicom.c: Menue-Hook - "Bluetooth (j/n)? j" startet bei BRINGUP_DEBUG bt_bringup() statt init_bluetooth().

Bedienung: RS232/HyperTerminal -> Konfig -> Bluetooth? j -> Debug-Konsole.
Test-Reihenfolge: Loopback (Jumper) -> /PING 115200 -> Baud-Scan -> eigene Kommandos (AT/$$$/PING).

RUECKBAU vor Merge: BRINGUP_DEBUG auf 0; bt_bringup() (btio.c), Prototyp (btio.h),
Menue-Hook (sicom.c) und main.c-#if-Guard entfernen. Alle Stellen mit BRINGUP markiert.
ACHTUNG: Firmware hier nicht kompiliert -> in Keil bauen.
