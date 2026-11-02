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


---

## Update 5.11 - Bring-up zurueckgebaut -> normale Software

Neues Mainboard verkabelt + Bluetooth bestaetigt (/PING ok). Die temporaeren Bring-up-Aenderungen
(5.10) sind vollstaendig entfernt - zurueck auf normalen Stand:
- main.c: Auto-Init wieder aktiv (Self-Heal + automatische Konfiguration; #if-Guard entfernt).
- sicom.c: "Bluetooth (j/n)? j" ruft wieder direkt init_bluetooth() (Menue-Hook entfernt).
- btio.c: Debug-Konsole bt_bringup() entfernt.
- btio.h: Schalter BRINGUP_DEBUG + Prototyp entfernt.
Verifiziert: keine BRINGUP-Reste mehr (grep leer), Encoding ISO-8859/CRLF, kein \r\r\n.

Doku/Bringup_Plan.md bleibt als Vorlage erhalten - falls ein weiteres Board in Betrieb genommen
werden muss, einfach wieder anwenden (BRINGUP_DEBUG-Schalter + bt_bringup-Konsole).

## Update 5.12 - F11001-Rauschen im Fehlerspeicher unterdrueckt

**Beobachtung:** Im Protokoll stand `F11001 PING keine Antwort` zeitgleich mit `I1 Eingeschaltet`,
obwohl BT danach laeuft. Ursache: `test_BT()` (btio.c) macht beim Boot **einen** /PING auf 460800
und loggte bei Fehlschlag sofort `F11001` - erst danach heilt `main.c` per `init_bluetooth()`.
Der Fehler ist also ein **selbstheilender Transient** (frisches Modul noch auf Werksbaud, oder
kalter Erst-Ping).

**Aenderung (btio.c, test_BT, IF820-Zweig):**
- CR-Flush + **ein Retry** vor dem Urteil (killt den kalten Erst-Ping).
- `puterror(DTC_BT_PING)` **entfernt** - die Boot-Probe loggt nichts mehr. Echte Defekte meldet
  weiterhin `init_bluetooth` als `F11005 Kein BT-Modul gefunden` (-> `syserror++`).
- `send_bt_info()` (benutzerinitiierte Info-Ausgabe) bleibt unveraendert - dort ist ein
  Ping-Fehler ein echter, zu meldender Fehler.

Hinweis: btio.c ist ISO-8859/CRLF -> **nur byte-sicher per Skript editieren** (das Edit-Tool
interpretiert die Datei als UTF-8 und zerstoert Umlaute). Nach versehentlicher Korruption alle
20 Umlaut-Bytes aus dem Kontext wiederhergestellt; Encoding verifiziert (ISO-8859, kein \r\r\n).

## Update 5.13 - Mehrstufige automatische Zeitsynchronisation

Umgesetzt nach Doku/Zeitsync_Plan.md. Eine zentrale Funktion, vier Quellen mit Rang/Drift,
Nutzer-Steuerung. Uhr stellt sich selbst (kein "2015"/Build-Datum mehr noetig sobald eine
Quelle da ist).

**Zentrale Funktion (rtc.c):** `clock_set(dd,mm,yyyy,hh,mi,ss, utc_off, src, flags)` -
validiert (dayofweek + Bereiche), Rang-/Drift-Pruefung (RESYNC_STUNDEN=12), UTC->Lokal
(CLK_FROM_UTC via fp.utc), Offset-Update (CLK_SET_UTC), schreibt RTC nur bei >3 s Abweichung,
Info-Ereignis nur bei Quellenwechsel (clock_logsrc). CLK_FORCE umgeht Rang/Drift/Auto-Aus.
Globals g_timesrc/g_timesync_h (RAM, kein fp-Eingriff). Raenge: NONE0 / LTE1 / GPS2=MANUAL2 / MQTT3.

**Quellen:**
- Backend/MQTT (mqtt.c): Inline-RTC-Block -> clock_set(...,SRC_MQTT,CLK_SET_UTC).
- GPS-RMC (gpsio.c): bei gueltigem Fix Feld1=hhmmss, Feld9=ddmmyy -> clock_set(...,SRC_GPS,CLK_FROM_UTC).
- LTE-NITZ (gsmio.c): neue Funktion gsm_nettime(force) - AT+QLTS=2 parsen (Lokalzeit+Zeitzone in
  Viertelstunden) -> clock_set(...,SRC_LTE,CLK_SET_UTC). best-effort, in test_registration mit 0 gerufen.
- Build-Datum (rtc.c InitRTC): Fallback bei ungestellter Uhr (5.12).
- Manuell (sicom.c Datum/Zeit-Menue): nach set_date/set_time -> clock_mark_manual() (SRC_MANUAL).

**Nutzer (sicom.c Information und Zeit):**
- Anzeige "Zeitquelle: Backend/GPS/Netz/manuell/ungestellt" (time_source_text()).
- Menuepunkt 7 "Zeit jetzt synchronisieren": gsm_nettime(1) erzwungen (Code 0x0067).
- Menuepunkt 8 "Automatische Zeit ein/aus": fp.autotime_off umschalten (Code 0x0068).
- "zurueck" jetzt Item 9 (Return 0x0069); select('9',1).

**Persistenz:** fp-Reserve 'fillbytes[2]' -> 'autotime_off' (==1 = aus, sonst ein; Default ein,
robust gegen 0x00 und 0xFF). Layout unveraendert (keine gespeicherten Parameter gebrochen).

**Codes:** 4 Info-Ereignisse 17050-17053 (Typ I) in Diagnose-Codes.csv; dtc_text.h neu generiert
(114 Eintraege). EV_TIME_* in rtc.h.

Encoding aller geaenderten ISO-8859-Dateien verifiziert (kein \r\r\n, keine neuen Replacement-
Zeichen; gsmio.h/sictxt.c hatten je vorbestehende). Version -> 5.13. In Keil bauen und testen.

HINWEIS: AT+QLTS wirkt nur, wenn das Modem gerade auf dem GSM-MUX-Kanal eingebucht ist (sonst
liefert "Zeit jetzt synchronisieren" "Keine Netzzeit"); der automatische Pfad laeuft nach der
Registrierung in test_registration.

## Update 5.14/5.15 - CTS-Timeout begrenzt + Warning-Log, fruehe Seriennummer-Pruefung

**Problem 1:** Ohne/falsch verdrahtetes CTS (RTS/CTS vertauscht) blockierte putb() pro Byte fast
eine ganze Watchdog-Periode -> MCU "haengt brutal", danach F11009. Ursache: CTS geht nie LOW.

Fix (sio.c putb): CTS-Warteschleife auf `CTS_WAIT_MS` (200 ms, an EINER Stelle einstellbar)
begrenzt statt ~WDT-Periode. Bei Ablauf wird Byte verworfen, globales Flag `cts_timeout` gesetzt
(KEIN Logging in putb -> keine Rekursion/Flut). In bt_command() wird daraus EINMAL (throttled 2 s,
nicht ueber die BT-UART) eine Warnung + Protokolleintrag: neuer Code **W11015 "CTS Timeout"** (Typ W).
Zusatz: `put_dtc()` gibt jetzt den echten Typ-Praefix (F/W/I) aus statt hart 'F'.

**Problem 2:** Wenn das Geraet noch keine gueltige Seriennummer hat, kann BT-Name/PIN nicht gesetzt
werden -> Init lief bisher erst halb durch und scheiterte spaet.

Fix (btio.c init_bluetooth): FRUEHE Pruefung ganz am Anfang (vor jeder Modul-Interaktion) -
`!isdigit(serno[0]) || last4==0` -> `F11008 "Seriennummer nicht gesetzt"` (Code existierte bereits)
auf Konsole + ins Protokoll, dann sauberer Abbruch. Kein sinnloses Scannen/Umkonfigurieren mehr.

Codes: W11015 in Diagnose-Codes.csv (Typ W) + dtc_text.h neu generiert (115 Eintraege) +
DTC_BT_CTS_TIMEOUT in dtc_codes.h. F11008 unveraendert (nur frueher ausgeloest). Version -> 5.15.
Hinweis: CTS_WAIT_MS ggf. erhoehen, falls bei sehr langsamer BT-Gegenstelle im Normalbetrieb
W11015 faelschlich kommt.


---

## Update 5.16 - Selbstheilende BT-Init mit CTS-Bootstrap (Werksmodul/Modultausch)

**Problem:** Ab 5.14/5.15 wartet `putb()` vor JEDEM Byte an UART1 auf CTS-LOW. Ein Modul im
**Werkszustand** (Flow Control AUS, `F=00`) treibt seine RTS-Leitung aber nicht als "ready" ->
die MCU-CTS (P2.2) bleibt HIGH. Folge: die Firmware kann nicht einmal das `/PING` zur Erkennung
senden, geschweige denn das `STU,...,F=01`, das Flow Control ueberhaupt erst einschaltet ->
`W11015 CTS Timeout` auf allen Baudraten -> `F11009 BT UART Init fehlgeschlagen`. **Deadlock:**
Zum Einschalten von Flow Control muesste gesendet werden, aber Senden ist bis zum Einschalten
blockiert. (Das Debug-Tool 5.10 lief nur, weil dessen `putb()` die CTS-Pruefung noch nicht hatte.)

**Fix:** Neues globales Flag `cts_bypass` (sio.c/sio.h). In `putb()` ist die CTS-Warteschleife
jetzt mit `if (!cts_bypass)` geklammert -> waehrend Bring-up/Erst-Konfiguration kann ohne
CTS-Handshake gesendet werden (gefahrlos, da das Modul bei `F=00` seinerseits keine Flow Control
macht).

`init_bluetooth()` (btio.c) neu dreistufig & selbstheilend - erkennt auch **Werksreset oder
Modultausch**, weil es sich nie auf gespeicherte Werte verlaesst:
- **Phase 0 (Schnellpfad, CTS scharf):** Nur wenn CTS bereits LOW -> `/PING @ IF820_BAUD` +
  `GTU`-Pruefung `F=01`. Erfolg -> `fastpath`, Phase A/B/C entfallen (schnellster Fall, kein
  unnoetiges W11015 auf Werksmodulen, da nur bei CTS-LOW versucht).
- **Phase A (Baud-Scan, CTS AUS):** `cts_bypass=1`, `/PING` ueber
  `{460800,115200,921600,307200,230400,9600}`. Findet auch das Werks-/getauschte Modul (115200/F=00).
- **Phase B (Konfig, CTS AUS):** Wenn nicht auf Zielbaud ODER `F!=01`: `bt_set_flowcontrol()`
  (`STU B=460800,F=01` -> Host 460800 -> `/SCFG`) + `bt_reboot()` (`/RBT`, Modul kommt sauber mit
  `F=01` aus dem Flash hoch).
- **Phase C (Verifikation, CTS scharf):** `cts_bypass=0`, `/PING` mit echtem CTS-Handshake.
  Antwort -> Handshake elektrisch bewiesen -> weiter mit Name/PIN. **Kein** `/PING` trotz
  `F=01`+Reboot -> `F11009` "CTS/RTS pruefen" + sauberer Abbruch = **echter Verdrahtungsfehler**
  (unterscheidet heilbaren Werkszustand von HW-Defekt).

`cts_bypass` wird auf ALLEN Austrittspfaden wieder auf 0 gesetzt (kein Haengenbleiben). Die alte
`bt_ensure_flowcontrol()` bleibt definiert, wird hier aber nicht mehr aufgerufen (durch Phase B/C
ersetzt). Version -> 5.16 (`sprachen/sictxt.c` `T_version`).

**Noch zu tun:** In Keil kompilieren + auf Hardware verifizieren (Werksmodul-Fall: erst Scan,
dann automatische Umkonfiguration auf 460800/F=01 + Reboot, danach Menue ueber BT).


---

## Update 5.17 - Gebremste + diagnosefaehige IF820-Erkennung (Scan haengt nicht mehr)

**Symptom (5.16 auf HW):** Kein W11015 mehr (cts_bypass greift, Senden frei), aber der Baud-Scan
lieferte 6x `/PING` ohne Antwort und brauchte **~10 min**, dann `F11009`. Modul lebt (GTU im
5.10-Debugtool antwortet).

**Ursache:** `wait_message()` verlaengert bei JEDEM empfangenen Byte den Timeout
(`ltime=mstimer+timeout/2`). Bei falscher Baud kommt Muell herein -> die Warteschleife lief pro
Baud sehr lange, statt sauber abzubrechen. Ausserdem sendete der Scan die Zielbaud (460800)
zuerst -> Werksmodul (115200) bekam erst Muell, was den Modul-Parser verstopfte.

**Fix (btio.c):** Neuer Helfer `bt_scan_ping(baud)` fuer die Erkennung (Phase A):
- **Harte Zeitgrenze** 250 ms pro Baud ueber eigenen `mstimer`-Deadline-Loop - unabhaengig von
  RX-Aktivitaet (kein Verlaengern durch Muell mehr) -> Gesamt-Scan ~1,5 s statt Minuten.
- **CR-Flush + RX-Puffer leeren** vor jedem `/PING` (gegen Parser-Verstopfung, 5.98).
- **Roh-RX-Dump** am Terminal: ` scan <baud>: <empfangene Zeichen>` (nicht darstellbare Bytes als
  '.') bzw. `(keine Antwort)`. Damit sieht man pro Baud, WAS das Modul liefert - Kern der
  Ferndiagnose.
- Scan-Reihenfolge: **115200 (Werksbaud) zuerst**, dann 460800, dann die uebrigen -> ein frisch
  zurueckgesetztes/getauschtes Modul wird ohne Muell-Vorlauf sauber getroffen.

Phasen 0/B/C unveraendert (5.16). Version -> 5.17.

**Test:** Ueber RS232 `Bluetooth? j` -> die ` scan <baud>: ...`-Zeilen mitlesen. Antwortet das
Modul bei 115200 mit `@R,...,/PING,0000` -> Erkennung greift, dann Phase B (STU F=01+/SCFG+/RBT)
und Phase C (Verifikation mit CTS). Was pro Baud zurueckkommt, sagt uns, falls es noch klemmt,
genau wo.


---

## Update 5.18 - Ablaufmeldungen fuer die BT-Init (sichtbar, wo es haengt)

Diagnose-Helfer `bt_msg()` / `bt_msgnum()` (btio.c): Klartext-Zeilen NUR ans Terminal
(`connect=concpy & ~UART1`, nicht ins Modul). `init_bluetooth()` gibt jetzt jeden Schritt aus,
sodass die LETZTE Zeile im Terminal immer zeigt, wo der Ablauf steht:

```
BT 5.18 init: Command-Mode ok, Erkennung startet
 Phase 0: Schnellpfad-Ping (CTS ist LOW)          (nur falls CTS schon LOW)
 Phase A: Baud-Scan (ohne CTS-Handshake):
 scan 115200: @R,...,/PING,0000,...               (Roh-RX-Dump je Baud, aus 5.17)
 scan 460800: (keine Antwort)
 -> IF820 erkannt bei Baud 115200
 Phase B: Flow Control setzen (STU F=01 + /SCFG)...
 Phase B: Modul-Reboot (/RBT), warte auf Boot...
 Phase C: Verifikation /PING mit CTS-Handshake...
 Phase C: OK - CTS-Handshake funktioniert
 Name setzen/pruefen (VIASIS_<serno>)...
BT-Init fertig: IF820, 460800, F=01
```

Fehlerfaelle bleiben wie gehabt (`F11009` bei CTS-Fehler in Phase C, `-> kein IF820 gefunden`
wenn keine Baud antwortet). Version -> 5.18.

Damit ist bei der Ferndiagnose sofort erkennbar, welcher Schritt haengt bzw. was das Modul pro
Baud liefert.


---

## Update 5.19 - Schritt-Marker im Scan + TX-Wait begrenzt (Haenger lokalisieren)

**Symptom (5.18):** Ausgabe blieb bei ` Phase A: Baud-Scan` stehen - der Ablauf haengt INNERHALB
`bt_scan_ping`, noch vor der ersten ` scan`-Zeile. Also entweder in `Init_BT_ch` oder beim Senden
auf UART1.

**Zwei Massnahmen:**
1. `bt_scan_ping` gibt jetzt VOR jeder Aktion Marker aus: ` scan @<baud>` -> `  [ch] sende CR`
   -> `  [CR] sende /PING` -> `  [PING] warte 250ms` -> `  rx: <daten>`. Die letzte sichtbare
   Zeile lokalisiert den Haenger exakt.
2. `putb()` (sio.c): die TX-FIFO-Warteschleife (`while(!(U1->LSR&THRE))`) hat jetzt eine harte
   50-ms-Grenze (`mstimer`). Bislang konnte sie ewig haengen, wenn der Watchdog von einem anderen
   Thread gefuettert wird und THRE nie gesetzt wird (z. B. UART1-TX nicht bereit). Kein Dauerhang
   mehr beim Senden.

Version -> 5.19. Naechster Lauf zeigt an den Markern, ob es beim Kanalwechsel (`Init_BT_ch`) oder
beim Senden klemmt.


---

## Update 5.20 - TX-Steckt-Zaehler (TX-Sendeproblem vs. stummes Modul trennen)

**Erkenntnis (5.19):** Haenger weg (TX-Wait auf 50 ms begrenzt), aber `rx: (nichts)` auf ALLEN
Baudraten. Marker `sende CR/PING` beweisen NICHT, dass Bytes rausgingen - `putstr` kehrt jetzt auch
nach TX-Timeout zurueck. Verdacht: UART1-TX zum Modul geht nicht raus (der alte 10-min-Haenger war
`putb()` an der THRE-Warteschleife). Das "/PING" in alten Logs war nur das Terminal-Echo von
`bt_command`.

**Diagnose-Erweiterung:** globaler Zaehler `bt_tx_stuck` (sio.c) - wird erhoeht, wenn die
TX-FIFO-Warteschleife (`THRE`) in den 50-ms-Timeout laeuft. `bt_scan_ping` nullt ihn pro Baud und
haengt an die Ausgabe `[TXsteckt=<n>]`.

Auswertung beim naechsten Lauf:
- `[TXsteckt>0]` -> Sender kommt nicht raus (UART1-TX/MUX/PCONP/Baud-Divisor oder HW). Modul
  bekommt gar kein /PING.
- `[TXsteckt=0]` + `rx:(nichts)` -> TX ok, Modul antwortet nicht (Command-Mode? RX-Verdrahtung?
  Modul stumm?).

Version -> 5.20. Damit ist die naechste Weiche eindeutig.


---

## Update 5.21 - WURZELURSACHE: Hardware-Auto-CTS blockierte TX (Werksmodul)

**Beweis (5.10-Debugtool vs. 5.20 auf gleicher HW):** Debugtool: Loopback ok, `/PING` -> Modul
antwortet (`@E,...,ERR,E=0203`), `GTU` -> `@R,...,B=0001C200,...,F=00,...` = Modul lebt, 115200,
**F=00**. Produktiv 5.20: `[TXsteckt=6]` auf jeder Baud - der Sender kam gar nicht raus.

**Ursache:** `Init_UART1()` (hard.c) schaltete IMMER `U1->MCR |= (1<<7)|(1<<6)` = **Hardware
Auto-RTS/Auto-CTS** der LPC1766-UART1 ein. Auto-CTS haelt den Sender HARDWARESEITIG an, solange
CTS HIGH ist. Werksmodul F=00 -> RTS wird nicht als "ready" getrieben -> CTS HIGH -> TX komplett
blockiert, `THRE` nie gesetzt -> `[TXsteckt]`. Der `cts_bypass` (5.16) umging nur die SOFTWARE-
CTS-Schleife in `putb()`; die HARDWARE-Auto-CTS im MCR blieb aktiv und blockierte weiter. Deshalb
half der Bypass nicht. (Das 5.10-Debugtool aktiviert Auto-CTS nicht -> sendet immer.)

**Fix (hard.c Init_UART1):** MCR-Flusssteuerung an `cts_bypass` gekoppelt:
- `cts_bypass=1` (Bring-up/Scan): `U1->MCR &= ~((1<<7)|(1<<6))` -> Auto-CTS AUS -> TX sendet auch
  bei CTS HIGH (Werksmodul erreichbar).
- `cts_bypass=0` (Normalbetrieb/Phase C): `U1->MCR |= (1<<7)|(1<<6)` -> HW RTS/CTS an.

Damit schaltet `cts_bypass` jetzt BEIDE CTS-Ebenen (Software-Warteschleife in putb + Hardware
Auto-CTS im MCR). `extern volatile uchar cts_bypass;` in hard.c ergaenzt (sio.h ist dort
auskommentiert). Version -> 5.21.

**Erwartung:** Phase A findet das Modul jetzt bei 115200 (`rx: @R,...,/PING,0000`, TXsteckt=0) ->
Phase B stellt auf 460800/F=01 + /RBT -> Phase C verifiziert mit echtem CTS-Handshake.

**BESTAETIGT auf Hardware (5.21):** Werksmodul (115200/F=00) laeuft vollstaendig selbstheilend
durch: `scan @115200 -> @R,/PING,0000 [TXsteckt=0]` -> erkannt -> Phase B `STU F=01`+`/SCFG`+`/RBT`
-> Phase C `/PING` mit CTS-Handshake OK -> Name `VIASIS_<serno>` gesetzt+verifiziert ->
"BT-Init fertig: IF820, 460800, F=01". Der komplette Bring-up-Deadlock (5.15-5.20) ist geloest;
Wurzelursache war die HW-Auto-CTS im MCR.

Kleiner Restpunkt (kosmetisch): `bt_show_name()` gab "Geraetename = ?" aus, obwohl GDN
`N=VIASIS_26SC9993...` lieferte - vermutlich Parse-/Puffer-Timing beim Auslesen direkt nach dem
Setzen. Nicht funktional (Name ist gesetzt+gesichert). Bei Gelegenheit die Anzeige nachziehen.


---

## Update 5.22 - /SCFG-Persistenz haerten + Idempotenz sichtbar machen

**Beobachtung (2. Lauf):** Init lief beim WIEDERHOLEN erneut komplett durch (STU/SCFG/RBT),
obwohl scheinbar schon konfiguriert. Ursache: Der Scan fand das Modul beim 2. Lauf wieder bei
**115200/F=00** - es war also erneut im Werkszustand. Im Log fehlt nach `/SCFG` das
`@R,...,SCFG,0000`. Verdacht: **/SCFG persistiert nicht**, weil es DIREKT nach dem Baud-Wechsel
gesendet wird, bevor das Modul seine UART auf 460800 umgestellt hat -> /SCFG verpufft, Flash
behaelt 115200/F=00; `/RBT` behaelt nur die RAM-Werte (deshalb war Phase C ok), aber der naechste
Kaltstart ist wieder Werkszustand.

**Wichtig:** Die gewuenschte Idempotenz ("wenn alles passt, nichts tun") ist bereits vorhanden -
Phase B schreibt nur bei `Baud!=460800 || F!=01`, Name nur bei Abweichung, Phase 0 (Schnellpfad)
beendet sofort. Sie greift nur nicht, solange das Modul nach jedem Kaltstart wieder auf 115200
faellt.

**Fix (btio.c bt_set_flowcontrol):**
- nach dem STU (Antwort @ alter Baud) `osDelay(80)` -> Modul stellt eigene UART auf Zielbaud um,
- dann `Init_BT_ch(IF820_BAUD)` (Host), `osDelay(20)` + CR-Flush,
- dann erst `/SCFG` (Antwort `SCFG,0000` wird geprueft).
- Rueckgabewert wird in Phase B jetzt gemeldet: " STU + /SCFG OK -> in Flash gesichert" bzw.
  " /SCFG FEHLGESCHLAGEN -> NICHT persistiert!". Skip-Fall meldet
  " bereits 460800/F=01 -> nichts schreiben/rebooten".

Version -> 5.22. Naechster Test zeigt, ob /SCFG jetzt bestaetigt (SCFG OK) und - nach echtem
Power-Cycle - ob das Modul auf 460800/F=01 bleibt (dann greift der Schnellpfad und Init endet sofort).


---

## Update 5.23 - Auto-RTS bleibt immer an (Modul kann bei F=01 antworten) + MUX-Verdacht

**Beobachtung:** `/SCFG FEHLGESCHLAGEN`, Phase C `W11015`/`F11009`. Bei F=00 laeuft alles (Debug-
Tool, Phase A) - sobald F=01 aktiv ist, wird das Modul STUMM.

**Zwei zusammenwirkende Ursachen:**
1. **Eigener Fehler (5.21):** Beim `cts_bypass` hatte ich BEIDE MCR-Bits geloescht - Auto-CTS (7)
   UND Auto-RTS (6). Auto-RTS ist aber das Signal, mit dem die MCU dem Modul "empfangsbereit"
   meldet. Ohne assertiertes RTS sendet ein F=01-Modul seine Antwort (z. B. `/SCFG`-Ack) NICHT ->
   `/SCFG` scheitert. Fix (hard.c): **Auto-RTS immer an**, nur Auto-CTS per `cts_bypass` schalten.
2. **Hardware/MUX (Anwender-Hinweis):** CTS/RTS laufen auf dem Mainboard ueber einen MUX
   (IC36/IC39, CSAF/CSBF) - anders als auf dem DVK (direkte 4-Draht, wo Flow Control lief, s.
   Update 5.84). Ob der LPC-RTS1-Pin (P2.7) das Modul-CTS ueber den MUX wirklich erreicht, ist
   offen. Fehlerbild (Modul stumm sobald F=01) passt zu "RTS-Handshake kommt nicht durch".

**Test 5.23:** Wenn Fix 1 reicht (RTS1 erreicht Modul via MUX) -> `/SCFG` meldet OK, Modul
antwortet bei F=01, Persistenz haelt. Wenn NICHT -> MUX gibt den RTS-Handshake nicht durch ->
Entscheidung noetig: HW-seitig RTS-Routing klaeren ODER Modul auf **F=00** betreiben (im Debug-
Tool voll funktionsfaehig, bidirektional). Version -> 5.23.


---

## Update 5.24 - Ueberfluessigen /RBT aus Phase B entfernt (Phase C pingte zu frueh)

**Beobachtung (5.23):** `/SCFG` OK (persistiert!), aber Phase C danach `W11015`/`F11009`.
`/RBT.` (nur 1 Punkt = Boot-Byte kam schnell) -> Phase C pingte 300 ms spaeter, da war das Modul
noch am Booten -> RTS/CTS noch HIGH -> W11015.

**Ursache:** Der `/RBT` in Phase B war UEBERFLUESSIG. `STU` aktiviert 460800/F=01 sofort im RAM;
`/SCFG` sichert es in Flash. Das Modul LAEUFT danach bereits auf 460800/F=01 (es hat ja gerade
`@R,/SCFG,0000` geantwortet). Der Reboot nahm es nur kurz offline, und Phase C traf das noch
bootende Modul.

**Fix:** `bt_reboot()` aus Phase B entfernt. Phase C verifiziert direkt am laufenden Modul ->
CTS ist LOW (Modul empfangsbereit) -> `/PING` -> `@R` -> OK. Version -> 5.24.

Stand: Erkennung + Persistenz + Verifikation sollten jetzt ohne Reboot-Timingluecke durchlaufen.


---

## Update 5.25 - Konfiguriertes Modul wird in Phase 0 erkannt (kein Fehl-Fallback) + Legacy gekuerzt

**Symptom (5.24-Log):** Phase 0 pingte das Modul erfolgreich bei 460800 (`@R,/PING,0000`,
Persistenz greift also!), fiel aber trotzdem in Phase A -> dort `rx:(nichts)` -> "kein IF820" ->
**Laird/RN4678-Fallback** -> `F11010`. Aergerlich, weil das Modul offensichtlich da war.

**Ursache:** Ein Modul mit **F=01** kommuniziert nur mit vollem Handshake (Auto-CTS an). Phase 0
(Auto-CTS an) fand es -> aber die Detbaud-Bedingung haing zusaetzlich an `bt_get_flowcontrol()==1`;
schlug die GTU-Abfrage fehl, wurde `detbaud` NICHT gesetzt -> Bypass-Scan (Phase A, Auto-CTS AUS)
-> der kann ein F=01-Modul nicht ansprechen -> nichts -> Fallback.

**Fix (btio.c Phase 0):** `detbaud=IF820_BAUD` wird jetzt schon gesetzt, sobald `/PING@460800`
antwortet (Modul gefunden). Die F=01-Pruefung entscheidet nur noch, ob Phase B noch nachkonfiguriert
(`fastpath`). Damit landet ein bereits konfiguriertes Modul nie mehr faelschlich im Legacy-Fallback.

**Timeout-Korrektur (wichtig):** In `bt_command` ist ein Timeout **>10000** ein CRLF-Flag
(`|pause|-10000`), KEIN 10-s-Wert. `10300` = "CRLF + 300 ms". Der Laird/RN4678-Fallback dauert
real also ~5-6 s (nichts gefunden), nicht Minuten (frueher falsch dargestellt).

**Legacy wie gewuenscht ueberarbeitet:**
- Fallback jetzt mit Trace `" Fallback: kein IF820 -> kurze Laird/RN4678-Erkennung"`.
- Wartezeiten gekuerzt: Laird-Loop osDelay 300->150 ms, RN4678-Silence-Gap 500->250 ms.
- Neue Info-Codes: **I11016 "Laird-Modul nicht erkannt"**, **I11017 "RN4678-Modul nicht erkannt"**
  (Doku/Diagnose-Codes.csv + dtc_codes.h + dtc_text.h via gen_dtc.py regeneriert). Werden im
  Fallback geloggt; finaler Sammelcode bleibt F11010.

Version -> 5.25.


---

## Update 5.26 - GTU-Abfrage robust (falsches "F noch nicht 01" in Phase 0 behoben)

**Beobachtung (5.25):** Modul persistent auf 460800/F=01 (Phase-B-GTU zeigt `F=01`, "nichts
schreiben/rebooten"). ABER Phase 0 meldete faelschlich "F noch nicht 01" -> deshalb liefen /DIS,
Phase B, Phase C durch, obwohl alles passt. F=01 wird also KORREKT gespeichert - nur die ERSTE
GTU-Abfrage (direkt nach dem /PING) kam ohne Antwort zurueck (-1 = unbekannt).

**Ursache:** `bt_get_flowcontrol()` fragte GTU genau einmal ab. Direkt nach dem /PING ist die
UART-Antwort noch nicht sauber lesbar (Puffer-/Timing-Nachlauf) -> Timeout -> -1 -> Code wertete
das als "nicht F=01".

**Fix (btio.c bt_get_flowcontrol):** bis zu 3 Versuche mit RX-Flush (`bxi=rxi`), Timeout 300->400 ms
und 60 ms Settle zwischen den Versuchen. Damit liest Phase 0 F=01 zuverlaessig ->
`fastpath` greift -> "bereits konfiguriert (F=01) -> fertig" -> /DIS/Phase B/Phase C entfallen.

Version -> 5.26. Erwartung: ein konfiguriertes Modul ist ueber Phase 0 (fast) sofort durch,
ohne Nachkonfiguration.


---

## Hinweis: btio.c war abgeschnitten (Build-Fehler) - wiederhergestellt

Symptom (Keil): `btio.c(765): error #67: expected a "}"` + "last line of file ends without a
newline". Ursache: die Datei war am Ende abgeschnitten - mitten in `set_bt_pin` (Laird-Zweig),
4 schliessende Klammern fehlten (Editor/Linter-Artefakt, keine inhaltliche Ursache).

Behebung: Der hintere, NIE geaenderte Teil (`set_bt_pin` bis Dateiende) wurde aus dem Git-Stand
(`git show HEAD:sism555/btio.c`) zurueckgeholt und an den unveraenderten Kopf (alle 5.16-5.26-
Aenderungen) angehaengt. Ergebnis: 783 Zeilen, 127/127 Klammern, alle Funktionen vorhanden.
Die `concopy`-Warnung (unbenutzte Variable) stammt aus dem Originalcode und ist harmlos.


---

## Update 5.27 - init_bluetooth als klarer Prüf-dann-fixe-Ablauf (Anwender-Struktur)

Neu strukturiert nach Anwender-Vorgabe - jede Eigenschaft einzeln pruefen, nur bei Bedarf
setzen, am Ende genau EINMAL speichern/rebooten wenn etwas geaendert wurde:

- **Erkennung:** Modul + Baud finden. Erst `/PING@460800` MIT Handshake (konfiguriertes Modul,
  F=01). Kommt nichts -> Baud-Scan OHNE CTS (Werksmodul F=00 haelt CTS HIGH - deshalb zwingend
  ohne Handshake).
- **Schritt 1/2:** Baud 460800 + Flow Control F=01. `STU` setzt BEIDES gemeinsam (ein Kommando),
  `GTU` liest beides. Nur wenn `Baud!=460800 || F!=01` -> `bt_set_flowcontrol()` (STU + /SCFG),
  `changed=1`, danach Handshake-Modus.
- **Schritt 3:** Name. `bt_get_name()` prueft; nur bei Abweichung `bt_set_name()` (SDN$ schreibt
  Flash direkt), `changed=1`.
- **Schritt 4:** Nur wenn `changed` -> `/RBT` + Verifikation `/PING` MIT Handshake (scheitert ->
  F11009). Wenn nichts geaendert -> sofort fertig (kein Speichern, kein Reboot).

**Wichtige Korrektur ggue. 5.16-5.26:** Die `GTU`-Abfrage laeuft jetzt NACH `/DIS` (nicht mehr
direkt nach dem `/PING` wie im alten Phase 0) -> zuverlaessig. Damit verschwindet das falsche
"F noch nicht 01". Der `fastpath`-Zweig entfaellt; die Idempotenz kommt jetzt sauber ueber
`changed`.

Ergebnis: konfiguriertes Modul -> Erkennung(Handshake) + GTU + GDN, alles ok, `changed=0` ->
"nichts geaendert -> fertig" (kein Reboot). Werksmodul -> Scan(ohne CTS) -> setzen -> speichern ->
Reboot -> verifizieren. Version -> 5.27.


---

## Update 5.28/5.29 - Saubere cmd-Ausgabe (formatierte Echos) + Reboot-Verifikation robust

**Formatierung (Anwenderwunsch: Echos behalten, aber lesbar):** `bt_command` spiegelt Kommando/
Antwort nicht mehr roh (alles zusammengeklebt), sondern schickt das Kommando NUR ans Modul und gibt
formatiert je eine eigene Zeile am Terminal aus: `  > <gesendet>` und `  < <empfangen>`. (Das
kurzzeitige `bt_quiet`-Experiment wieder entfernt - Echos bleiben sichtbar.)

**Reboot-Bug behoben (W11015 nach Namensaenderung):** Nach `/RBT` war das Modul beim Verifikations-
`/PING` noch am Booten -> CTS HIGH -> W11015 -> F11009. Fix:
- `bt_reboot()` wartet nach dem Boot-Signal zusaetzlich 1,5 s (Modul-UART/RTS erst dann bereit).
- Schritt-4-Verifikation pingt bis zu 4x mit 400 ms Pause statt nur einmal.

**Hinweis Datei-Truncation:** btio.c wurde zwischendurch MEHRFACH am Ende abgeschnitten (mitten in
set_bt_pin, 4 fehlende Klammern -> Keil `#67 expected "}"`). Ursache extern (Editor/Linter/Sync auf
PC-Seite, nicht der Code). Jeweils via `git show HEAD:sism555/btio.c` (unveraenderter Schwanz)
wiederhergestellt. Falls es erneut auftritt: sicherstellen, dass btio.c nicht parallel in einem
anderen Editor offen ist/gespeichert wird. Version -> 5.29.


---

## Update 5.30 - WURZELURSACHE "kann nicht verbinden": CYSPP haing als Ausgang (IC54/PCA9554)

**Symptom:** Nach Flashen von 5.29 auf dem Eval-Kit (wo SPP frueher lief) war das Geraet zwar
sichtbar (VIASIS_...), aber Classic-SPP-Verbindung scheiterte. Nach vollem **Power-Cycle** ging es.

**Ursache:** CYSPP laeuft ueber IC54 (PCA9554) Bit 7. Ein frueherer `bt_cmdmode()` (bei
Konfiguration) setzt Bit 7 als **Ausgang HIGH**. Der PCA9554 behaelt seine Register ueber einen
LPC-Reflash hinweg - nur ein echter Power-Cycle setzt ihn auf "alle Eingaenge". Beim 5.29-Boot war
`test_BT()` erfolgreich -> `init_bluetooth()` (und damit `bt_release()`) lief NICHT -> CYSPP blieb
als Ausgang HIGH -> das Modul konnte die Leitung beim SPP-Connect nicht auf LOW ziehen ->
Verbindung scheitert. Power-Cycle setzt PCA9554 zurueck -> Modul kann treiben -> Verbindung geht.

**Fix (main.c, Boot):** `if (fp.btmodem==IF820) bt_release();` am Anfang des BT-Boot-Blocks (vor
test_BT). Damit ist CYSPP nach jedem Start garantiert Eingang -> Modul kann die Leitung treiben,
auch nach reinem Reflash ohne Power-Cycle. Version -> 5.30.


---

## Update 5.31 - CYSPP/BT_LINK Live-Monitor im Testmenue (HW-Diagnose Mainboard)

Symptom: SPP-Connect geht auf dem Eval-Kit, NICHT auf dem Mainboard mit Chip-Modul -> Verdacht
CYSPP-Leitung (Mainboard war fuer RN4678-Statuspin ausgelegt; IF820-CYSPP muss auf IC54 Bit 7
liegen).

Neu: `bt_cyspp_monitor()` (btio.c) - liest IC54 Bit 7 (CYSPP/BT_LINK) alle ~120 ms und gibt bei
Zustandswechsel eine Zeile aus (`CYSPP bit7 = HIGH/LOW`), Ende per Tastendruck (max ~30 s).
Erreichbar ueber: Menue -> 6 Component tests -> 8 CYSPP/BT_LINK monitor. (Ende-Taste im Component-
Menue jetzt '9'.)

Anwendung am Mainboard: Monitor starten, per BT verbinden.
- springt auf LOW -> Modul treibt CYSPP, Signal kommt am MCU/IC54 an -> Leitung ok.
- bleibt HIGH (obwohl Modul-Pin laut Oszi LOW zieht) -> Signal kommt nicht an -> CYSPP-Netz/
  falscher Modul-Pin auf dem Mainboard (RN4678-Statuspin != IF820-CYSPP-Position pruefen).

Dateien: btio.c (+bt_cyspp_monitor), btio.h (Prototyp), sictxt.h (T_comp +Punkt 8),
sicom.c (compmenu case '8', Ende '9'). Version -> 5.31.


---

## Update 5.32 - CYSPP-Monitor ins richtige Testmenue (Hauptmenue->3) verschoben

5.31 hatte den Monitor faelschlich ins "Component tests" (Konfig-/Werksmenue) gelegt. Jetzt im
normalen "Testfunktionen"-Menue (Hauptmenue -> 3) als Punkt 7 (zurueck -> 8). Umsetzung
sprachunabhaengig ohne Eingriff in die mehrsprachigen Menuetexte: in sicom.c case 0x0003 nach
putstr(T_estmenu) ein putstr("\n7. CYSPP monitor") + select('8',1); neuer case 0x0037 ->
bt_cyspp_monitor(). Navigation korrekt (unbelegter Code 0x0038 -> default menu>>=4 + Post-Switch
menu>>=4 -> Hauptmenue). compmenu wieder auf 7 Punkte/Ende '8' zurueckgebaut. Version -> 5.32.


---

## Update 5.37 - CYSPP-Steuerung per Schalter abschaltbar (Testmodus)

Neuer Schalter `#define BT_CYSPP` in btio.h: 1=normal, 0=Testmodus. Bei 0 sind `bt_cmdmode()`
(CYSPP HIGH/Command-Mode erzwingen) und `bt_release()` (Bit7 Eingang) per `#if BT_CYSPP`
ausgeklammert -> Firmware treibt IC54 Bit7 NICHT mehr. Lesen (BT_LINK/get_com_status,
bt_cyspp_monitor) bleibt aktiv. Aktuell auf 0 (Test); spaeter wieder 1.
Hinweis: mit 0 erzwingt init_bluetooth keinen Command-Mode ueber CYSPP -> beim Konfigurieren keine
offene BT-Verbindung; nach Flash einmal Power-Cycle (PCA9554 auf Eingang). Version -> 5.37.


---

## Update 5.39 - CYSPP als Pflicht bestaetigt -> BT_CYSPP zurueck auf 1

Test mit BT_CYSPP=0 (5.37): keine BT-Verbindung moeglich, wenn die CYSPP-Leitung unterbrochen ist.
Fazit: CYSPP (IC54 Bit7 <-> Modul) ist ZWINGEND fuer den SPP-Datenmodus - das Modul schaltet ohne
CYSPP nicht in den Datenmodus. Damit ist das Mainboard-Problem erklaert: dort ist die CYSPP-Leitung
unterbrochen -> HARDWARE-Fix noetig (Modul-CYSPP-Pin sauber auf IC54 Bit7 fuehren). BT_CYSPP wieder
auf 1 (normal). Version -> 5.39.


---

## Update 5.41 - System-Freeze bei BT-Verbindung behoben (CTS-Busy-Spin -> Taskwechsel)

**Symptom:** Geraet laeuft normal; SOBALD per BT verbunden wird, wird binnen kurzem ALLES traege
(auch RS232: ~1 Zeichen / 0,5 s), BT quasi tot. Nach Power-Cycle wieder ok.

**Ursache:** Bei aktiver BT-Verbindung geht die Ausgabe an UART0+UART1. In `putb()` wartet der
UART1-Zweig vor jedem Byte auf CTS=LOW - als **Busy-Spin ohne Taskwechsel** (bis CTS_WAIT_MS=200 ms).
Bei BT-Backpressure (Gegenstelle/Luftrate langsamer als 460800, oder marginale CTS-Leitung) bleibt
CTS HIGH -> jedes Byte blockiert die volle Wartezeit und der Spin frisst die CPU -> der kooperative
RTOS-Loop wird ausgehungert -> systemweit traege; RS232 haengt mit, weil es im selben putb steckt
(~0,5 s/Zeichen = genau der CTS-Timeout).

**Fix (sio.c putb):** In der CTS-Warteschleife `osDelay(1)` -> Taskwechsel zulassen (kein
Busy-Spin), CTS_WAIT_MS 200 -> 50. Damit friert das System bei Backpressure nicht mehr ein; nur
die BT-Ausgabe pacet sich ggf. langsamer. Version -> 5.41.

Offen/separat: warum CTS bei Verbindung dauerhaft HIGH wird (Peer liest nicht / BT-Luftrate <
460800 UART / CTS-Leitung marginal). Fuer sauberes Verhalten waere langfristig eine gepufferte/
ISR-getriebene UART1-Ausgabe (nicht-blockierend) die Idealloesung.


---

## Update 5.42 - KORREKTUR: CTS/RTS laufen ueber Analog-MUX (IC13), nicht "direkt"

**Befund (Schaltplan bestaetigt):** Die alte Doku (BT-Evalkit-Tasks.md) war falsch - dort stand
CTS/RTS "direkt" und MUX "IC55/IC56". Tatsaechlich:

- **IC18 (TS5A23157DGSR)** = MUX fuer Datenleitungen (RXD/TXD).
- **IC13 (TS5A23157DGSR)** = MUX fuer **Flusssteuerung RTS/CTS**.
  - MCU-CTS (P2.2, das putb() pollt) kommt ueber IC13 2Z(Pin6)<-2Y0 von BLT-CTS.
  - MCU-RTS (P1.15) geht ueber IC13 1Z(Pin10)->1Y0 nach BLT-RTS.
- Select beider Switches = **CS-A-FUNK = CSAF (P0.1)**: LOW=BT, HIGH=LTE/Funk.
- **VCC beider Switches = V+D** (gemeinsame 3,3-V-Schiene).

**Bedeutung fuer den "Board langsam nach ~1 min"-Fehler:** Beim DVK liegt CTS direkt am
USB-Bridge; beim Mainboard laeuft CTS durch den Analog-Switch IC13. Sackt V+D beim BT-Senden ab
(Brownout gemeinsame Schiene), wird IC13 hochohmig/undefiniert -> P2.2 liest HIGH -> putb()
blockiert (bis CTS_WAIT_MS) -> System traege, RS232 troepfelt. Erholt sich die Spannung / BT
trennt -> wieder normal. ~1 min ist KEIN Firmware-Timer (BT_TIMEOUT=6 min) -> elektrisch.

**HW-Test:** im langsamen Abschnitt V+D am IC13/Modul + P2.2(CTS) mit Oszi ansehen: bricht V+D ein
und geht CTS dabei HIGH -> Ursache bestaetigt. Abhilfe: eigener LDO/Stuetz-Elko fuers BT-Teil
(wie beim DVK) bzw. Versorgung des Analog-MUX absichern.

**Bezug BT-only-Sonderversion:** CSAF dauerhaft LOW zu halten (IC18/IC13 fest auf BT) beseitigt
Umschalt-Glitches auf dem CTS/RTS-MUX - aber NICHT den Brownout (V+D bleibt geteilt).


---

## Update 5.43 - KORREKTUR der Isolierung: Board-Pfad ist ok, Verdacht = Onboard-Modul (RF/Power)

**Neuer Testaufbau (Nutzer):** Das externe Eval-Kit-Modul wird MIT DEN BOARD-PINS versorgt und
angesteuert (RxD, TxD, CTS, RTS, CYSPP auf die Eval-Kit-Bruecken). In diesem Aufbau laeuft BT
einwandfrei. Das AUFGELOETETE Onboard-Modul dagegen wird nach ~1 min langsam.

**Schlussfolgerung (korrigiert):** Damit durchlaeuft das funktionierende Eval-Kit-Modul dieselbe
Board-Elektronik/Versorgung wie das defekte Onboard-Modul. Der frueher vermutete MUX (IC13) und
der UART1-Pfad sind also NICHT der Unterschied - sie treiben das Eval-Kit-Modul sauber.
Die Aussage aus 5.42 ("Eval-Kit umgeht IC13") gilt fuer diesen Aufbau NICHT.

**Neuer Hauptverdacht = Onboard-Modul + unmittelbares Umfeld:**
1. RF/Antenne: schlechte Funkstrecke -> Retransmits -> Luftrate << 460800 UART -> Modul-TX-Puffer
   voll -> Modul zieht RTS -> unser CTS HIGH -> putb() bremst -> RS232 troepfelt; nach ~1 min
   bricht Verbindung ab -> normal. (Passt exakt zum Zeitverhalten.)
2. Lokale Versorgung/Entkopplung am Onboard-Modul (Eval-Kit hat eigene LDO+Stuetz-Cs).

**Tests:** (a) Peer 5-10 cm neben Onboard-Modul -> laenger stabil? = RF. (b) INT-ANT vs EXT-ANT
Variante + Ground-Plane/Keep-out pruefen. (c) V+D direkt am Onboard-Modul beim Senden per Oszi
vs. Eval-Kit-Modul.

Hinweis: IC13/IC18 (TS5A23157DGSR) als CTS/RTS- bzw. Daten-MUX bleiben korrekt dokumentiert
(Schaltplan) - nur die Fehler-Zuordnung war falsch.

**Nachtrag 5.43:** Nutzer versorgt Eval-Kit-Modul auch mit Board-VBAT/VDDIO -> gleiche Rails,
laeuft. ABER: das Eval-Kit haengt damit auch seine grossen Stuetz-Cs an die Board-Schiene ->
stabilisiert die Versorgung selbst. Onboard-Modul hat diese lokale Pufferung evtl. nicht ->
lokaler Spannungseinbruch NUR am Modul beim TX-Peak (an der Quelle/Schiene nicht sichtbar).
Billiger Entscheidungstest: Bulk-Elko 10-47 uF + 100 nF direkt an VBAT/VDDIO des Onboard-Moduls.
Weg = Entkopplung/Power bestaetigt. Zusatz: Peer 5-10 cm ans Modul = RF/Antenne-Check;
INT-ANT-Variante? Ground-Plane/Keep-out pruefen.

**Nachtrag 5.43 (Datenblatt EZ-DS-Vela IF820 v2_2, Abschnitt 12):** INT-ANT-Variante, Peer nur
40 cm entfernt -> Ausfall ist KEIN Reichweitenproblem. Zwei konkrete Datenblatt-Vorgaben zu pruefen:
- 12.2 Antennen-Layout INT-ANT: (1) Modulkante buendig mit PCB-Kante; (2) >=15 mm GND-Flaeche
  seitlich neben dem Modul. 12.1: keine Bauteile/High-Speed-Leitungen UNTER dem Funkteil, kein
  Metall ueber der Antenne. -> Verstoss = verstimmte Antenne = schon auf 40 cm schlechte RF.
- 12.1 Entkopplung woertlich: "put a 100nF capacitor on EACH power pin, as close as possible"
  (VBAT + VDDIO) fuer korrekte interne PMU. Fehlt das/zu weit weg -> sporadischer Einbruch unter Last.
Pruefreihenfolge Board: (1) Antennenzone (Rand? 15 mm GND frei? Kupfer/Bauteil unter Antenne?),
(2) 100 nF je Power-Pin direkt am Modul + Bulk-Elko 10-47 uF. Eval-Kit erfuellt beide Regeln,
Custom-Board vermutlich nicht -> erklaert "Eval-Kit ok, Onboard nach 1 min langsam".


---

## Update 5.44 - BT-Chip: Firmware-Version + Einstellungen abfragbar (Menue Bluetooth -> Information)

**Wunsch:** Firmware des IF820 und dessen Einstellungen aus dem Geraet heraus auslesen.

**Umsetzung:** `send_bt_info()` (btio.c) im IF820-Zweig erweitert. Bisher nur /PING. Jetzt zusaetzlich:
- `/QFV` -> Firmware-/Stack-/Protokoll-/HW-Version (E=/S=/P=/H=). E byteweise hex = Version,
  z. B. E=010412 12 -> 1.4.18.18.
- `GTU`  -> UART-Einstellungen: Baud (B=), Flow Control (F=01 an / F=00 aus), 8N1 (D=08 etc.).
- `GDN,T=00` -> BLE-Name; `GDN,T=01` -> Classic-Name.
Antworten erscheinen als "> gesendet / < empfangen" am Terminal (bestehende Spiegelung in bt_command).

**Bedienung:** Menue **Bluetooth -> Information** (sicom.c case 0x0511, ruft send_bt_info()).
Kein neuer Menuepunkt noetig - semantisch die richtige Stelle. Neues Kommando-Text-Makro
`T_qfv`/`T_qfvok` in btio.h.

Hinweis: laeuft im Command-Mode (nicht waehrend aktiver CYSPP-SPP-Datenverbindung). Version -> 5.44.

**Nachtrag 5.45:** send_bt_info() zusaetzlich MAC + CYSPP-Parameter:
- `GBA` -> Bluetooth MAC-Adresse (A=<MAC>).
- `.CYSPPGP` -> CYSPP-Parameter (E=enable, G=role, C=connkey, L=local_key, R=remote_key, M=mask).
Neue Makros T_gba/T_gbaok, T_cypget/T_cypgetok in btio.h. Version -> 5.45.

**Nachtrag 5.46 - FIX: BT-Info liess RX-Bytes ins Menue laufen (Menue raste durch):**
Symptom: nach "Bluetooth -> Information" sprang das Menue wild zwischen Hauptmenues, phantom "0".
Ursache: Eingabe- und Modul-Lesefunktionen teilen denselben RX-Ring (rbuf/bxi/rxi). bt_command
matchte das Kommando-ECHO und kehrte zurueck, bevor die @R-Antwort ("...,0000,...") komplett da
war -> die "0000"-Bytes blieben im Puffer und wurden vom Menue als Tasten gelesen.
Fix: neue Helper bt_query_show() (Muster bt_scan_ping): Kommando senden, feste 400 ms warten,
KOMPLETTE Roh-Antwort ans Terminal, danach bxi=rxi (Puffer leeren). Zusaetzlich finaler Flush
(osDelay(50); bxi=rxi) vor uart1_release. Zeigt jetzt auch die vollen Werte (vorher am Match
abgeschnitten). Version -> 5.46.


---

## Update 5.47 - BEFUND: Onboard-Module haben ALTE Firmware (1.4.12.12) vs Eval-Kit (1.4.18.18)

Mit der neuen "Bluetooth -> Information"-Abfrage (5.46) beide Module verglichen. EINZIGER echter
Unterschied = Firmware-Version des Chips (/QFV E-Feld):
- Eval-Kit:  E=01041212 -> 1.4.18.18  (aktuelle GA3, = Projekt-Firmware in SupplierSourceCode)
- Onboard:   E=01040C0C -> 1.4.12.12  (alt)
Alles andere identisch: GTU B=460800, F=01 (Flow Control AN), 8N1; CYSPPGP E=02,G=00,C=0131,...;
Stack S=03010000; Protokoll P=0104; HW H=F1. (Name/MAC erwartungsgemaess unterschiedlich.)

PCN #2E (Release 1.4.18.18) behebt genau passende Bugs:
- "Fix a lockup issue due in heavy UART loading."  <-- passt zu "laeuft 1 min, dann traege".
- "Implement SBTP,F=1 to disable sniff feature. This improves SPP throughput."
- "Add using CYSPP pin to exit SPP MODE (pull CYSPP high) -> back to command mode."  <-- unser
  bt_cmdmode() setzt CYSPP HIGH; dieses Feature gibt es erst ab 1.4.18.18 -> erklaert, warum
  CYSPP-HIGH auf altem 1.4.12.12 die SPP-Verbindung nicht trennte.

=> HEISSER Kandidat fuer den "Board wird nach 1 min langsam"-Fehler, unabhaengig von Power/Antenne.
Massnahme: Onboard-Module auf 1.4.18.18 flashen. Hinweis: Flashen ueber HCI-UART (BT_UART
TXD/RXD/CTS/RTS) + Reset (Flow Control zwingend, CTS beim Reset LOW), if820_flasher_cli fuer
lose/aufgeloetete Module (GUI nur DVK/Dongle). HCI-Pins am Mainboard herausgefuehrt? -> pruefen.

**Nachtrag 5.47 - Downgrade-Test:** Um zu bestaetigen, dass die alte Modul-Firmware der Grund ist,
soll testweise 1.4.12.12 aufs Eval-Kit (INT-ANT) geflasht werden.
Download (GA1, interne Antenne):
  https://github.com/Ezurio/Vela_IF820_Firmware/releases/download/v1.4.12.12/IF820_INT_ANT_v1.4.12.12.zip
Flashen mit vorhandenem SupplierSourceCode/if820_flasher_gui-Windows-v2.1.0 (GUI erkennt DVK auto)
oder CLI: if820_flasher_cli.exe -ce -f <..._141212_..._download.hex>. -ce setzt Konfig auf Werk zurueck
-> Geraete-FW rekonfiguriert beim Boot (F=01, Name). Kontrolle via "Bluetooth -> Information": E=01040C0C.
Erwartung: bricht Eval-Kit mit 1.4.12.12 auch nach ~1 min ein -> Firmware = Ursache -> Onboard-Module
auf 1.4.18.18 flashen. Bleibt stabil -> doch Hardware (Power/Antenne).

**Nachtrag 5.47 - 32-kHz-Quarz (Datenblatt EZ-DS v2_2):** IF820 hat internen 32-kHz-LPO, laeuft
also OHNE externen Quarz. Ezurio empfiehlt aber externen 32,768-kHz-Quarz (XTALI_32K/XTALO_32K,
Pin 4/5) ausdruecklich: "lowest power and highest timing accuracy" und - wichtig - "if not, LINK
ISSUE may be occurred". => Fehlender Quarz ist ein weiterer heisser Kandidat fuer BT-Instabilitaet.
Firmware: ohne Quarz -> int-lpo-Variante noetig; mit Quarz -> normale Variante. TODO: pruefen ob
Onboard-Design 32-kHz-Quarz hat + passende Firmware-Variante. USB-Dongle hat keinen Quarz (int-lpo).


---

## Update 5.47b - BT-Timeout trennt Verbindung jetzt SAUBER per /DIS (statt nur virtuell)

**Problem:** Der Inaktivitaets-Timeout (measure.c) trennte bisher nur virtuell: bt_cmdmode()
(CYSPP-Puls) + Flags loeschen. Die SPP-Verbindung blieb am Modul oft bestehen (CYSPP blieb LOW,
Peer nicht informiert, SPP-Slot belegt) - im CYSPP-Monitor beobachtet.

**Fix:** Neue Funktion `bt_disconnect()` (btio.c), aufgerufen im Timeout-Pfad (measure.c):
1. bt_cmdmode()  -> CYSPP HIGH -> PUART in Command-Mode
2. osDelay(150)  -> Modul: SPP-Datenmodus -> Command-Mode
3. bt_command(T_dis, T_disok, -300) -> `/DIS` (gap_disconnect): Modul baut Verbindung aktiv ab
4. osDelay(50); bt_release(); bxi=rxi (RX-Reste verwerfen)
Deklariert in btio.h. Defensiv: /DIS-Quittung egal -> es wird immer freigegeben (nie schlechter
als vorher). Version -> 5.47.

Abhaengigkeit: Schritt 1 (CYSPP-HIGH -> Command-Mode aus SPP heraus) ist erst ab Modul-FW
1.4.18.18 zuverlaessig; auf 1.4.12.12 kann /DIS das Modul evtl. nicht erreichen -> weiteres
Argument fuers Firmware-Update. Geaenderte Dateien: btio.c, btio.h, measure.c, sictxt.c.


---

## Update 5.48 - BT-Terminal-Junk bei MUX-Wechsel auf GSM/GPS unterdruecken (Software, versionssicher)

**Symptom:** Auf einem per HyperTerminal verbundenen BT-Terminal erscheint 0xFF-Junk, sobald das
Modem/GSM initialisiert. RS232 (UART0) zeigt gleichzeitig sauberen Modem-Start (EG91). => Modem/GSM
sind in Ordnung; der Junk ist ein Nebeneffekt des geteilten UART1-MUX.

**Ursache:** uart1_request(MUX_GSM/GPS) schaltet den Analog-MUX (IC18) weg von BT. Damit floatet
die IF820-RXD (BLT-RXD). Das IF820 bleibt im SPP-Datenmodus und leitet das Leitungsrauschen
transparent ueber Bluetooth weiter -> 0xFF-Flut am BT-Terminal.

**Fix (hard.c, uart1_request):** Symmetrisches Command-Mode-Handling:
- BT->GSM/GPS und BT verbunden (connect&BT_LINK) und IF820: vor dem MUX-Wechsel bt_cmdmode()
  (CYSPP HIGH) -> Modul stoppt SPP-Weiterleitung.
- Rueckkehr GSM/GPS->BT (prev_owner GSM/GPS) und IF820: bt_release() -> Weiterleitung/Status wieder aktiv.
Guard ueber prev_owner, damit die init_bluetooth-Sequenz (eigenes cmdmode) NICHT gestoert wird;
fp.btmodem==IF820 nur. I2C-Aufrufe ausserhalb der Critical Section.

**Versionssicher:** CYSPP-HIGH-zum-SPP-Verlassen wirkt erst ab Modul-FW 1.4.18.18. Auf 1.4.12.12
bleibt der Junk (Feature fehlt), aber der Code macht nichts kaputt. Firmware-unabhaengige Alternative
bleibt ein HW-Pull-up auf der IF820-RXD. Version -> 5.48. Geaenderte Datei: hard.c, sictxt.c.

---

## Update 5.49 - FIX: bt_disconnect() korrumpierte connect (concpy nicht gesetzt)

**Symptom:** Nach einer BT-Verbindung + Inaktivitaets-Timeout „ging das Modul/Terminal nicht mehr".
**Ursache:** bt_disconnect() (5.47) ruft bt_command() auf; bt_command() stellt am Ende
`connect=concpy` her. bt_disconnect setzte `concpy` aber NICHT -> im Timeout-Kontext war concpy
veraltet -> `connect` wurde nach dem Timeout auf einen falschen Wert gesetzt -> Terminal/BT-Routing
kaputt bis Reboot.
**Fix:** bt_disconnect() setzt jetzt `concpy=connect` vor bt_command() und `connect=concpy` danach
-> connect bleibt definiert/unveraendert; measure.c maskiert anschliessend nur die BT-Bits.
Version -> 5.49. Geaenderte Datei: btio.c, sictxt.c.

**Nachtrag 5.49 - Modul-Defekt, SW entlastet:** Nach Power-Cycle antwortete ein IF820 auf KEINER
Baud (rx:(none), TXstuck=0 -> LPC-TX ok, Modul stumm) -> F11009. Mit einem ANDEREN Modul und
derselben Firmware 5.49 laeuft es. => defektes Modul bzw. dessen Verdrahtung/Versorgung, NICHT die
Software. Vermutliche Ursache wie beim DVK: externe Einspeisung/Brownout. Merker: nur EINE
Versorgungsquelle (DVK-USB ODER Board), VBAT/VDDIO nicht gegeneinander speisen, Modul beim Umstecken
stromlos machen.

## Update 5.50 - SIM-Karte nicht erkannt -> eigener Fehlercode F12009

**Anforderung:** Fehlende/nicht erkannte SIM soll einen eigenen Fehlercode bekommen und
in der Kommunikation angezeigt werden.

**Ursache/Luecke:** atcpin() unterschied bisher nur "READY" (PIN ok) und "incorrect"
(falsche PIN). Der Fall "keine SIM" fiel still durch (nur interne Marke mqtt_con_err=41),
ohne Diagnose-Code und ohne gezielte Meldung.

**Fix:** Neuer Code DTC_GSM_SIM_NF=12009 "SIM-Karte nicht erkannt" (Typ F, Subsystem 12).
Erkennung ueber AT+QSIMSTAT? (physischer Steckstatus, unabhaengig von PIN) via neue
statische Hilfsfunktion sim_inserted() in gsmio.c, aufgerufen in atcpin() direkt nach
modem_start() (deckt PIN- und Kein-PIN-Pfad ab). Bei insert=0 -> dtcerr(DTC_GSM_SIM_NF)
(gibt "F12009 SIM-Karte nicht erkannt" aus + protokolliert), Modem aus. QSIMSTAT ohne
eindeutige Antwort (-1, z. B. altes Modem) loest bewusst KEINEN Fehler aus (kein Fehlalarm);
insert=2 ("wird erkannt") gilt als vorhanden.

**Geaenderte Dateien:** dtc_codes.h, dtc_text.h, Doku/Diagnose-Codes.csv, gsmio.h, gsmio.c,
sprachen/sictxt.c (Version -> 5.50). Details in Doku/LTE_Fix.md.

**Hinweis:** dtc_text.h ist generiert (tools/gen_dtc.py aus der CSV); Generator liegt nicht
im Repo -> CSV und Header parallel von Hand gepflegt. Firmware hier nicht kompiliert -> in
Keil bauen; QSIMSTAT-Antwortformat und Verhalten mit/ohne SIM am Geraet verifizieren
(Checkliste in LTE_Fix.md).

---

## Update 5.51 - FIX: CYSPP blieb als Ausgang HIGH haengen -> BLE/SPP-Datenkanal unmoeglich

**Symptom (von der App-Seite gemeldet, `Doku/ble-new-chip-bringup.md`):** iOS-App verbindet
sich per BLE auf `VIASIS_<serno>_BLE` einwandfrei, Service Discovery und MTU-Negotiation (515)
laufen, Writes werden mit ATT-Success quittiert - **aber sobald der Client Notifications oder
Indications aktiviert (CCCD-Write `0x2902`), trennt das Modul die Verbindung sofort.**
Reproduzierbar 3 von 3 Laeufen, unabhaengig von der Characteristic. Zusaetzlich: Writes
vor dem Subscribe bewirken nichts (keine Antwort vom Geraet).

**Ursache:** Der von der App entdeckte Service `65333333-a115-11e2-9e9a-0800200ca100` ist der
**CYSPP-Service** des IF820. Der CCCD-Write ist der dokumentierte Trigger, der den
CYSPP-**Datenmodus** startet. Laut EZ-Serial-Doku bleibt die Datenpipe offen, *"until the
peripheral device disconnects, **or the CYSPP pin is de-asserted locally**"* - CYSPP ist
low-aktiv, ein vom Host getriebenes HIGH bedeutet also "Datenkanal beenden".

Genau das tat unsere Firmware: `uart1_request()` (hard.c) ruft bei Verdraengung von BT durch
GSM/GPS bewusst `bt_cmdmode()` (CYSPP HIGH, Junk-Unterdrueckung beim MUX-Wechsel, Update
5.48), gab die Leitung aber **nur auf dem Rueckweg GSM/GPS -> MUX_BT** wieder frei
(Guard `prev_owner`). Faellt `BT_LINK` weg, waehrend GSM/GPS UART1 haelt, laeuft der Release
in den else-Zweig von `uart1_release()` -> `uart1_owner = MUX_NONE` -> dieser Rueckweg kommt
nie mehr -> **CYSPP bleibt bis zum Reboot als Ausgang HIGH.** Das ist selbstverriegelnd: das
erzwungene HIGH trennt die BT-Verbindung, der Verbindungsverlust verhindert die Freigabe.
Ergebnis: weder Classic-SPP noch BLE-CYSPP koennen danach einen Datenkanal aufbauen. Writes
werden vom GATT-Server weiter mit ATT-Success quittiert, aber ohne Datenmodus verworfen -
daher "Schreiben geht, Lesen nie".

**Fix (3 Teile):**
1. `btio.c` / `btio.h`: neuer Zustandsmerker `bt_cyspp_out` - wird in `bt_cmdmode()` gesetzt,
   in `bt_release()` geloescht. Damit ist jederzeit bekannt, ob der Host die Leitung treibt.
2. `hard.c`, `uart1_release()` else-Zweig: fuer Nicht-BT-Requester CYSPP freigeben, wenn es
   noch getrieben wird. Kein Verlass mehr auf `prev_owner`. Guard `requester != MUX_BT`, damit
   die `init_bluetooth()`-Sequenz (haelt CYSPP bewusst ueber eigene
   `Init_BT_ch`/`uart1_release`-Paare HIGH) nicht gestoert wird.
3. `measure.c`, Minutentick: Selbstheilung - besteht kein `BT_LINK` und ist
   `uart1_owner != MUX_BT`, wird CYSPP freigegeben. **Bewusst ohne `bt_cyspp_out`-Abfrage:**
   der Merker liegt im RAM (nach Reset 0), der PCA9554 haelt sein Config-Register aber ueber
   Reset/Reflash - nur ohne Flag-Abfrage heilt auch ein aus der Vor-Reset-Phase
   stehengebliebenes Hardware-HIGH. `bt_release()` ist idempotent (1x/Minute).
   Damit ist diese ganze Bugklasse auf "heilt binnen einer Minute" reduziert, statt bis
   zum Reboot BT-tot zu sein.

**Haertung aus dem Code-Review (gleiche Version):**
- `main.c`: `bt_release()` vor `get_com_status()` - laeuft bei **jedem** Reset-Grund.
  `InitI2C_Devices()` (setzt Config 0xFF) ist bei `FIRM_UP_RES`/`SLEEP_RES` ausgeklammert
  (`main.c` Zeile ~96) und das bisherige `bt_release()` hing nur am POR-Zweig - nach einem
  **Reflash** blieb ein stehengebliebenes HIGH also bestehen (deckt sich mit dem bereits
  dokumentierten Befund "PCA9554 haelt Register ueber Reflash").
- `bt_release()`: loescht `bt_cyspp_out` **nur bei erfolgreichem I2C-Zugriff** - sonst gelten
  die Sicherungen als erledigt, obwohl die Leitung weiter getrieben wird.
- `bt_cyspp_out` ist `volatile` (wird auch aus dem GSM-/MQTT-Thread via `uart1_request()`
  gesetzt, gelesen im Main-Thread).
- `uart1_request()` `default:`-Zweig (ungueltiger Requester): gibt CYSPP frei statt getrieben
  zurueckzukehren.

**Vom Review geprueft und in Ordnung:** keine Rekursion (`uart1_release` -> `uart1_request`
maximale Tiefe 1), kein CYSPP-Flattern (neuer Guard sitzt im else-Zweig und verlangt
`!(connect&BT_LINK)`, ein neues `bt_cmdmode()` verlangt `bt_displaced` d. h. `BT_LINK` - schliesst
sich gegenseitig aus), Include-Ketten vollstaendig, bei `BT_CYSPP 0` konsistent (Flag bleibt 0,
Guards feuern nie). `init_bluetooth()`/`bt_disconnect()`/Minutentick laufen alle im Main-Thread,
der Minutentick kann sie also nicht unterbrechen. Alle vier `bt_cmdmode()`-Aufrufpfade
(btio.c init_bluetooth + bt_disconnect, sicom.c, hard.c) sind jetzt auf jedem Ausgang dicht.

**Geaenderte Dateien:** btio.c, btio.h, hard.c, measure.c, main.c, sprachen/sictxt.c
(Version -> 5.51), Doku/ble-chip-response.md (neu, Antwort an die App-Seite).

**Am Geraet zu verifizieren (nicht kompiliert/getestet):**
- Testmenue `bt_cyspp_monitor()` (IC54 Bit7) ueber RS232 laufen lassen, waehrend die App
  verbindet und subscribed: Bit7 muss beim Notify-Enable auf **LOW** gehen und dort bleiben.
- Vorher-Nachweis der Ursache: nach einer GSM/LTE-Session mit zwischenzeitlichem
  BT-Verbindungsverlust muss Bit7 in der **alten** Firmware dauerhaft HIGH stehen.
- Reihenfolge Provokation: BT verbinden -> GSM-Aktion ausloesen (UART1-Verdraengung) ->
  BT trennen lassen -> GSM-Session beenden -> erneut per App verbinden + subscriben.

**Offen / separat:** Read-Timeout auf `a47f7608-...` (Service 2, moduleigene GATT-DB) und
Manufacturer-Payload im Advertising (`00 00 00 00`) - beides niedrige Prioritaet, siehe
`Doku/ble-chip-response.md`.

---

## Update 5.52 - bt_cmdmode() modul-geguarded (kein Pegelkonflikt bei RN4678 / BT730)

**Befund (Vorbedingung, nicht durch 5.51 entstanden):** IC54 Bit 7 ist bei **allen drei**
BT-Modulen dieselbe physische Leitung (`BT_LINK`, hard.h). Nur beim IF820 ist sie
**bidirektional** (CYSPP) und darf vom Host getrieben werden. Beim **RN4678** und beim
**Laird BT730** ist sie ein reiner **Status-Ausgang des Moduls**.

`init_bluetooth()` rief `bt_cmdmode()` bisher **unbedingt** auf - also auch, wenn ein RN4678
oder BT730 steckt - und hielt die Leitung dann mindestens 600 ms als Ausgang HIGH gegen den
Modulausgang. Folge: Pegelkonflikt fuer die Dauer der Init, und `BT_LINK` liest waehrenddessen
faelschlich "nicht verbunden". In der Praxis bisher harmlos (laeuft beim Boot, wenn keine
Verbindung besteht), aber unnoetig.

**Fix:** Guard **in `bt_cmdmode()` selbst** (nicht an der Aufrufstelle), damit alle heutigen
und kuenftigen Aufrufer automatisch abgedeckt sind:
`if ((fp.btmodem==Roving) || (fp.btmodem==Laird)) return;`
-> beim RN4678/BT730 ein No-Op. `fp.btmodem==0` (Erstboot, Modultausch, Erkennung noch nicht
gelaufen) bleibt **bewusst erlaubt**: dort muss der Command-Mode erzwingbar sein, sonst
scheitert die IF820-Erkennung an einem Modul, das noch im Datenmodus steht. Die Reihenfolge in
`init_bluetooth()` passt dazu - `fp.btmodem` gilt beim Aufruf noch aus dem Flash und wird erst
danach auf 0 gesetzt.

`bt_release()` bleibt bewusst **ungeguarded**: "alle Pins Eingang" ist der Werks-/Init-Zustand
von IC54 (identisch `i2cm.c`) und fuer RN4678/BT730 genau der benoetigte Zustand.

**Konsistenz-Nebeneffekt:** Da `bt_cyspp_out` nur in `bt_cmdmode()` gesetzt wird, bleibt der
Merker bei RN4678/BT730 dauerhaft 0 - die drei Sicherungen aus 5.51 (uart1_release,
Minutentick, default-Zweig) sind dort ohnehin per `fp.btmodem==IF820` geguarded und feuern nie.

**Modul-Vertraeglichkeit von 5.51/5.52 geprueft:** IC54 wird projektweit nur an drei Stellen
beschrieben (`bt_cmdmode`, `bt_release`, `i2cm.c` Init) - es gibt also keinen weiteren Ausgang
auf diesem Expander, der durch "alle Pins Eingang" umkonfiguriert werden koennte. Alle
Neuerungen aus 5.51 sind entweder `fp.btmodem==IF820`-geguarded oder (Boot-`bt_release()` in
main.c) modul-neutral bis leicht positiv.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (Version -> 5.52).

**Nicht kompiliert, nicht am Geraet verifiziert.** RN4678 und BT730 sind auf diesem Branch
seit Monaten nicht mehr am Geraet getestet - 5.52 verschlechtert dort nichts, ein echter
Regressionstest der beiden Module steht aber weiter aus.

---

## Update 5.53 - BT-Download 3x langsamer: Diagnose eingebaut + USB-Endlosschleife begrenzt

**Symptom (Anwender):** Messdaten-Download (~400 000 Werte) ueber Classic SPP dauert **45 min**,
frueher **15 min**. Dieselben Daten ueber **RS232: 13 min**.

**Rechnung (X/Y-Modem, 1024-Byte-Bloecke, stop-and-wait, ein ACK pro Block, ~9 MB):**

| | RS232 @115200 | BT @460800 |
|---|---|---|
| Zeit | 13 min | 45 min |
| Bloecke | ~8 800 | ~8 800 |
| Drahtzeit/Block | 89 ms | 22 ms |
| Ist/Block | 89 ms | 307 ms |
| **Totzeit/Block** | ~0 ms | **~285 ms** |

RS232 ist exakt drahtgebunden - die Round-Trip-Zeit ist dort vernachlaessigbar. Ueber BT waere
der Draht 4x schneller, es dauert aber 3,5x laenger. Das Problem ist also **Latenz, nicht
Bandbreite**, und das Geraet ist entlastet (RS232 beweist, dass es die Daten viel schneller
liefern kann).

**Verdaechtiger 1 (eigener Code, `sio.c` `putb()`):** Der UART1-Zweig wartet **pro Byte** auf
CTS und schlaeft dabei `osDelay(1)`. Ein Byte braucht bei 460800 Baud ~22 us - ein `osDelay(1)`
kostet mindestens das 45-fache. Bei einem Bulk-Transfer bremst das Modul permanent (der
Luftweg ist langsamer als der Draht), also trifft es sehr viele Bytes. `git log -S` datiert
diese Zeile auf **94d175a, 2026-07-27** - genau das Zeitfenster der Verschlechterung. Der
UART0-Zweig wartet dagegen mit engem Busy-Wait ohne `osDelay` - deshalb ist RS232 schnell.
Bemerkenswert: Auto-CTS ist in Hardware aktiv (`U1->MCR |= (1<<7)`), die Softwareschleife ist
fuer die Korrektheit also gar nicht noetig, nur fuer die Erkennung eines steckenden Moduls.

**Verdaechtiger 2 (Modul, aus der Ezurio-Kommandoreferenz):** `SBTP` (bt_set_parameters, 14/10)
hat ein Flags-Feld: *"Bit 0 - Disable sniff mode ... 0: Enable sniff mode (factory default)"*.
**Sniff-Mode ist ab Werk AN.** Das ist genau die Signatur von ~300 ms Latenz pro Block.

Beide wirken zusammen: Sniff macht den Luftweg langsam/schubweise -> CTS bremst haeufig ->
`osDelay(1)` pro Byte verstaerkt es massiv.

**Diese Version aendert das Sendeverhalten NICHT** (bewusste Anwenderentscheidung: erst messen,
dann eingreifen). Eingebaut wurde:

1. **Diagnosezaehler in `putb()`** (`sio.c`/`sio.h`): `bt_tx_bytes` (an UART1 gesendet),
   `bt_cts_wait` (Bytes, die bremsen mussten), `bt_cts_sleep` (`osDelay(1)`-Durchlaeufe
   ~= verlorene Millisekunden). Nullstellung bei jedem neuen BT-Connect
   (`libtool.c`, `communication_change()`) -> Zaehler gelten je BT-Sitzung.
2. **Anzeige** `bt_show_counters()` im BT-Info-Menue: `TX bytes / ctswait / sleeps / lost=<s> /
   ctsTO / TXstuck / USBstuck`. Steht `lost` in der Groessenordnung der Downloaddauer, ist
   Verdaechtiger 1 bewiesen.
3. **Zwei neue Modulabfragen** im BT-Info-Menue: `GBTDC` (14/13, Class of Device) und `GBTP`
   (14/11, Classic-Parameter inkl. Sniff-Flag). Damit ist Verdaechtiger 2 pruefbar und die
   Windows-Fehlklassifikation (siehe unten) auslesbar.
4. **USB-Endlosschleife begrenzt** (Variante 4): `putb()` wiederholte
   `USBD_CDC_ACM_PutChar()` **unbegrenzt und ohne Taskwechsel**. Steckt ein USB-Kabel, ist CDC
   enumeriert, holt aber niemand die Daten ab, drehte das pro Byte endlos und blockierte das
   System. Jetzt: Timeout `USB_WAIT_MS`=50 ms, Zaehler `usb_tx_stuck`, und `osThreadYield()`
   statt `osDelay(1)` - CPU abgeben ohne 1-ms-Zwangspause je Byte.

**Nebenbefund Windows-Fehlklassifikation:** Der PC zeigt `VIASIS_26SC9996` als **Audiogeraet**
und legt keinen SPP-COM-Port automatisch an. Recherche in der Ezurio-Kommandoreferenz
(`Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf`, 220 S.): Die EZ-Serial-Firmware hat
**ueberhaupt keine Audio-Profile** - 0 Treffer fuer A2DP/AVRCP/HFP/Audio in Guide und
Datenblatt; ab Werk laufen genau **Classic SPP + CYSPP (BLE)**. Die Fehlklassifikation kann
also nur aus der **Class of Device** kommen -> mit `GBTDC` auslesen, ggf. per `SBTDC,C=<hex>`
+ `/SCFG` korrigieren. Ein Windows-seitiger Haken im Dienste-Reiter ist **keine** Loesung
(Kundenzumutung), sondern nur ein Messinstrument.

**Ausserdem geklaert:** Der Infineon-Guide fuer den CYW20822 ist BLE-only (keine
Classic-Kommandos). Die maßgebliche Referenz fuer den IF820 ist die **Ezurio**-Datei oben, die
bereits im Repo unter `Documentation/` liegt. Classic-Kommandos: **BT-Gruppe ID=14**, SPP-Gruppe
ID=19. BLE-Advertising stoppt nach 30 s + 60 s = **90 s** (aus den `SAP`-Werksdefaults
`D=0x1E`, `O=0x3C`); per `SAP,D=0000,O=0000` unbegrenzt stellbar.

**Geaenderte Dateien:** sio.c, sio.h, btio.c, btio.h, libtool.c, sprachen/sictxt.c (-> 5.53).

**Naechste Schritte (in dieser Reihenfolge):**
1. 5.53 bauen/flashen, BT-Download wiederholen, danach BT-Info-Menue aufrufen und
   `lost=<s>`, `GBTDC` und `GBTP` notieren.
2. Ist `lost` gross -> CTS-Wait umbauen (erst kurz eng pollen, dann erst schlafen; Freeze-Schutz
   und Timeouts behalten). Ist `F=0` in `GBTP` -> `SBTP,F=1` (Sniff aus) + `/SCFG`.
3. `GBTDC`: Major Device Class 0x04 oder Service-Bit 0x200000 -> `SBTDC` auf eine
   unkategorisierte/serielle Klasse, damit Windows das Geraet richtig einordnet.

---

## Update 5.54 - Sniff-Mode aus + Class of Device seriell (Init, selbstheilend)

**Grundlage:** Messung aus 5.53 am Geraet (Firmware-Modul 1.4.12.12, QFV E=01040C0C):

- `TX bytes=458961 ctswait=37 sleeps=661 lost=0s TXstuck=28` -> der `osDelay(1)`-CTS-Pfad in
  `putb()` ist **entlastet** (nur 0,66 s ueber 459 kB). Mein urspruenglicher Hauptverdacht war
  falsch - gut, dass gemessen wurde, statt Variante 1 blind zu bauen.
- `GBTP ... F=00` -> **Sniff-Mode ist ab Werk AN.** Da putb ausscheidet, bleibt die Zeit nur im
  Warten auf die X/Y-Modem-ACKs haengen (stop-and-wait, ein ACK je 1k-Block). Sniff blaeht den
  Round-Trip pro Block auf ~300 ms auf -> aus ~3 min werden ~45-67 min. Passt zur gemessenen
  Rate von ~1 kB/s (0,3 MB in 5 min).
- `GBTDC C=00240418` -> dekodiert Major Class 0x04 (Audio/Video), Minor 0x06 (Kopfhoerer),
  Service-Bits Audio+Rendering. **Das** ist der Grund, warum Windows das Geraet als Audiogeraet
  fuehrt und keinen SPP-COM-Port anlegt.
- `TXstuck=28`: 28 Bytes wurden nach 50 ms THRE-Timeout verworfen -> je verworfenes Byte ein
  X/Y-Modem-Blockneuversand. Zweiter, unabhaengiger Bremsfaktor (separat, siehe unten).

**Warum das intermittierend ist:** Sniff wird **ausgehandelt**, meist vom PC. `F=00` heisst nur
"Modul erlaubt Sniff"; ob es aktiv wird, entscheidet der Windows-BT-Stack/die Energieverwaltung.
Daher dasselbe Modul am einen PC schnell, am anderen zaeh (Eval-Board 18.18 ok, ein 12.12-Board
gestern ok, dieses heute langsam). Ein Firmware-Upgrade auf 18.18 ist damit **kein** verlaesslicher
Fix - die Variable liegt auf der PC-Seite.

**Robuste Loesung (5.54):** Dem Modul Sniff **verbieten** (`SBTP,F=01`) -> es lehnt jede
Sniff-Anforderung ab und bleibt voll empfangsbereit, unabhaengig vom PC. Zusaetzlich Class of
Device auf seriell/unkategorisiert (`SBTDC,C=001F00`), damit Windows das Geraet richtig einordnet
und den COM-Port anlegt. Beides selbstheilend im Init gesetzt, wie Name und Flow Control.

**WICHTIGE Korrektur aus dem Code-Review (bevor committed):** Das GBTP-Flags-Feld ist laut
Ezurio-Doku *"always set to 0 in the current release"* -> der **Sniff-Zustand ist nicht ueber
GBTP ruecklesbar** (deckt sich mit dem Geraetedump: `F=00` trotz Werksdefault). Ein
Idempotenz-Check ueber das Sniff-Flag haette daher bei JEDEM Boot fehlgeschlagen und jedes Mal
Flash-Write + Reboot erzwungen. Die Pruefung laeuft deshalb ueber die **Class of Device**
(GBTDC), die zuverlaessig lesbar ist. Da CoD und Sniff immer gemeinsam geschrieben werden, ist
"CoD == Zielwert" ein verlaesslicher Indikator, dass auch Sniff bereits aus ist.

**Umsetzung (analog `bt_ensure_flowcontrol`/`bt_ensure_name`):**
- `bt_get_cod()`: liest `GBTDC` per Raw-Tail-Read (wie `bt_show_name`, weil `C=` das LETZTE Feld
  ist -> kein Folge-Anker fuer `wait_message`) und vergleicht die 8 Hex-Ziffern mit `IF820_COD`
  ("00001F00"). -1=keine Antwort, 0=abweichend, 1=Zielwert. Beleg, dass GBTDC auf 12.12
  funktioniert: der 5.53-Menue-Dump lieferte `GBTDC,0000,C=00240418`.
- `bt_set_btcfg()`: `SBTDC,C=00001F00` (CoD, 8 Hex wie Readback) -> `SBTP,...,F=01,...`
  (Sniff aus, uebrige Felder = beobachteter Werksdefault T=7D00,D=02,C=01,S=02,V=0000,N=0000)
  -> CR-Flush -> `/SCFG`. Struktur identisch zu `bt_set_flowcontrol()`.
- `bt_ensure_btcfg()`: Pruefen ueber `bt_get_cod()`, bei Bedarf setzen, per `bt_get_cod()`
  verifizieren.
- Eingehaengt als **Schritt 3b** in `init_bluetooth()` (nach Name, vor dem /RBT), Idempotenz
  ueber `bt_get_cod()`. Setzt `changed=1` -> der bestehende Reboot+Verify aus Schritt 4 macht
  CoD/Name live.
- **Fehlschlag ist NICHT fatal:** nur Warnung per `putln`, kein `dtcerr`, Init laeuft weiter -
  Sniff/CoD sind Optimierung, keine Korrektheit. Kein neuer DTC-Code, keine Aenderung an den
  generierten DTC-Tabellen.

**Wirkungsbereich (bewusst eng):** Nur IF820. Sniff ist ein reines **Classic**-Konzept -> BLE/App
unberuehrt. Sniff gilt nur waehrend einer aktiven Verbindung -> unverbundene Batteriegeraete
(kein BT-Peer) sind im Stromverbrauch **nicht** betroffen; der Ruhe-/Advertising-/Deep-Sleep-
Verbrauch haengt an anderen Parametern, die hier nicht angefasst werden.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.54).

**Am Geraet zu verifizieren (nicht kompiliert, nicht getestet):**
1. 5.54 bauen/flashen. Beim BT-Init muss "Step 3b: disable sniff + set serial CoD" erscheinen,
   danach der Reboot. Bei erneutem Init "Step 3b: ... already ok" (kein zweiter Flash-Write).
2. BT-Info-Menue: `GBTDC` muss jetzt `C=00001F00` zeigen. (`GBTP` zeigt weiterhin `F=00` -
   das Flag ist nicht ruecklesbar; das ist erwartet und KEIN Fehler. Ob Sniff wirklich aus ist,
   zeigt sich nur am Durchsatz.)
3. Download wiederholen -> Zeit sollte einbrechen (Zielgroesse: RS232-nah, ~wenige Minuten).
   `bt_show_counters` gegenpruefen. Windows sollte das Geraet nicht mehr als Audiogeraet fuehren
   und einen SPP-COM-Port anlegen (ggf. altes gekoppeltes Geraet einmalig entfernen).

**Noch offen (separat):** `TXstuck` (50-ms-Byte-Drop in `putb()` verwirft Bytes bei laengerer
CTS-Blockade -> X/Y-Modem-Neuversand). Sitzt an einer heiklen Stelle, daher eigene Version, erst
nachdem 5.54 gemessen ist - moeglicherweise ist der Drop nach dem Sniff-Fix gar nicht mehr
relevant.

---

## Update 5.55 - BLE-Advertising dauerhaft + BESTAETIGUNG: Download-Problem geloest

**Erfolgsmeldung zuerst:** Nach 5.54 (Sniff aus) konnten **4 MB in unter 15 Minuten** ueber
Classic SPP heruntergeladen werden - vorher waeren das bei ~1 kB/s ueber eine Stunde gewesen.
Der Sniff-Mode war die Ursache, der Fix ist am Geraet bestaetigt. Geraetedump nach 5.54:
`GBTDC C=00001F00` (CoD seriell), `GBTP F=01` (Sniff aus - auf 12.12 IST das Flag ruecklesbar,
anders als der v2.2-Guide fuer "current release" behauptet), `TXstuck=0` (keine verworfenen
Bytes/Blockneuversande mehr), `lost=4s` ueber 519 kB (klein; der Anstieg von 0s auf 4s ist sogar
ein SCHNELLIGKEITSindikator: die Daten fliessen jetzt schnell genug, dass der Modulpuffer
volllaeuft und die CTS-Flusskontrolle einspringt).

**Anforderung 5.55:** BLE muss **dauerhaft** advertisen - "man weiss nie, wann ein Kunde ans
Geraet kommt". Ab Werk stoppt das Modul nach ~90 s (SAP-Default D=001E=30s High + O=003C=60s Low).

**Umsetzung (Muster wie 5.54):** Neuer Schritt 3c in `init_bluetooth()`.
- `bt_set_adv()`: `SAP,T=03,C=07,H=0030,D=001E,L=0800,O=0000,F=01` -> Low-Duty-Dauer O=0000
  (unendlich) + F=01 (Auto-Advertising bei Boot UND Disconnect). Ergebnis: erste 30 s schnelles
  High-Duty-Advertising, danach dauerhaft im stromsparenden Low-Duty-Intervall (1,28 s). Dann CR-
  Flush + `/SCFG`.
- `bt_get_adv()`: liest `GAP`, prueft das O-Feld (Anker `,F=` -> O komplett in cbuf, da O vor F).
  Idempotenz zuverlaessig, da GAP im Gegensatz zum SBTP-Sniff-Flag ruecklesbar ist.
- `bt_ensure_adv()`: pruefen/setzen/verifizieren.
- Fehlschlag NICHT fatal (nur `putln`, kein `dtcerr`).

**Bewusst fuer ALLE IF820-Geraite** (kein Netz/Batterie-Guard) - so gewuenscht. L=0800 (1,28 s
Low-Duty) ist der Strom-Kompromiss; falls Batterielaufzeit kritisch wird, L vergroessern.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.55).

**Am Geraet zu verifizieren (nicht kompiliert, nicht getestet):**
1. Beim Init erscheint "Step 3c: set permanent advertising", danach Reboot; beim naechsten Init
   "already ok".
2. BT-Info-Menue: `GAP` muss `O=0000` zeigen.
3. Geraet laeuft >90 s, dann per App/BLE-Scanner pruefen: **muss weiterhin auffindbar sein.**
4. Einmalig die reale GAP-Antwort gegenlesen und bestaetigen, dass das F-Feld hinter O steht
   (Parser-Annahme; ueber EzSerialPort.py-Enums und das GTU-Muster erschlossen, nicht am Geraet
   verifiziert).

**Fuer die App-Seite** ist das die Antwort auf "BLE dauerhaft an" - siehe aktualisiertes
`Doku/ble-chip-response.md`, Abschnitt 3b.

---

## Update 5.56 - Low-Duty-Advertising-Intervall auf 3 s

**Entscheidung:** Das dauerhafte Low-Duty-Advertising aus 5.55 laeuft jetzt mit **3 s** statt
1,28 s. Hintergrund: Es gibt firmware-seitig **kein** verlaessliches Merkmal, ob ein Geraet netz-
oder batteriebetrieben ist (nur die gemessene Versorgungsspannung, die beim Boot nicht
zwischen "geladene Batterie" und "Netz" unterscheidet). Daher EIN einheitlicher Wert fuer alle
Geraete, bewusst richtung stromsparend gewaehlt.

**Aenderung:** In `T_sap` (btio.h) `L=0800` (1,28 s) -> `L=12C0` (4800 * 0,625 ms = 3,0 s).
Sonst unveraendert: T=03, D=001E (30 s High-Duty schnell), O=0000 (unendlich), F=01 (Auto-Adv).
Erste 30 s nach Boot/Disconnect also weiterhin schnell auffindbar, danach dauerhaft im 3-s-Takt.

**Geaenderte Dateien:** btio.h, sprachen/sictxt.c (-> 5.56). Nicht kompiliert, nicht getestet.

---

## Update 5.57 - App-Config im Init: Echo aus (A1), CYSPP-Paketierung (A2), Just-Works (A5)

Umsetzung von drei Kandidaten aus `Doku/IF820_Config_Kandidaten.md`, selbstheilend im Init
(Muster wie 5.54/5.55: get -> bei Bedarf set -> /SCFG, Idempotenz ueber ein ruecklesbares Feld).

- **A1 Command-Echo aus** (`SPEM,M=0`): Der Guide empfiehlt Echo-aus fuer MCU-Hosts - der LPC
  parst dann nur noch Antworten, nicht sein eigenes Echo. Werksdefault ist Echo AN, A1 schreibt
  also beim ersten Init. Betrifft nur den UART-Command-Mode, nicht SPP/CYSPP-Daten. Gate: GPEM
  M=00 (Raw-Tail-Read mit Retry, da M das letzte Feld ist).
- **A2 CYSPP-Paketierung** (`.CYSPPSK,M=01,W=01,L=14,E=0D`): Werks-Wartewert 5 ms ist bei 460800
  zu traege ("Safe Wait" laut Guide = 1 ms). Nur W -> 1 ms, uebrige Felder = Default. Verbessert
  den BLE-Datenfluss Geraet->App. Gate: .CYSPPGK W=01 (Anker ",L=").
- **A5 Just-Works festschreiben** (`SSBP,M=00,B=00,K=10,P=00,I=03,F=01`): sichert, dass NIE ein
  OS-Pairing-Dialog erzwungen wird (No Input/No Output, kein MITM/Bond, Auto-Accept). Ist der
  Werksdefault, wird nur explizit festgeschrieben. Gate: GSBP M=00 (Anker ",B=").

Alle drei: Fehlschlag NICHT fatal (nur `putln`, kein `dtcerr`), Init laeuft weiter. Eingehaengt
als Schritte 3d/3e/3f (nach 3c Advertising, vor Schritt 4 Reboot). `changed=1` -> bestehender
Reboot+Verify macht sie live.

**Code-Review (Subagent): commit-faehig.** Parsing/Anker/Feldreihenfolge gegen die Doku
(ezserial.txt) bestaetigt (GPEM M letztes Feld; .CYSPPGK M,W,L,E; GSBP M,B,K,P,I,F). Graceful-
Fail bestaetigt: fehlt ein Kommando auf 1.4.12.12, matcht der Antwort-Token nicht -> bt_command
FALSE -> non-fatal. Nachbesserungen eingebaut: Retry in bt_get_echo (vermeidet unnoetigen
Reboot), Kommentar zu A5 entschaerft (M-Werksdefault nicht dokumentiert).

**NICHT umgesetzt - A4 (Seriennummer in Manufacturer-Data): bewusst zurueckgestellt.**
Sicherheitsfund: die Zugangs-PIN IST die numerische Seriennummer (`fp.btpin =
atoi(serno)*10000 + atoi(serno+4)`, geprueft an sicom.c). A4 wuerde diese Zahl roh in die
Manufacturer-Data broadcasten. Sie steckt zwar schon (ableitbar) im Advertising-Namen, aber die
Entscheidung, sie zusaetzlich roh zu senden, liegt beim Nutzer. Separater eigentlicher Befund:
die App-Layer-PIN ist aus der oeffentlichen Seriennummer trivial ableitbar - schwaches Schema,
unabhaengig von A4 zu ueberdenken.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.57).

**Am Geraet zu verifizieren (nicht kompiliert, nicht getestet):**
1. Erst-Init zeigt "Step 3d: disable command echo" und "Step 3e: set CYSPP packetization",
   Schritt 3f je nach Werksdefault. Danach Reboot; Re-Init alle drei "already ok".
2. BT-Info-Menue: `GPEM M=00`, `.CYSPPGK W=01`, `GSBP M=00`.
3. Existenz der Kommandos auf 1.4.12.12 pruefen - falls eine Warnung "could not set ..."
   erscheint, kennt die alte Modul-FW das Kommando nicht (dann per Modul-FW-Update 1.4.18.18
   nachziehen oder Schritt weglassen).

## Update 5.57b - Entscheidungen A4 / PIN-Schema (nur Doku)

- **A4 (Seriennummer in Manufacturer-Data): verworfen.** Wird nicht umgesetzt. Die App liest die
  Seriennummer aus dem Advertising-Namen; die PIN-gleiche Zahl wird nicht zusaetzlich gesendet.
- **PIN-Schema (PIN == numerische Seriennummer): wird separat behandelt** (eigener Arbeitsstrang,
  nicht Teil der IF820-Config-Liste).

Manufacturer-Data bleiben damit bewusst 00000000. Doku aktualisiert:
IF820_Config_Kandidaten.md und ble-chip-response.md.

## Update 5.57 - VERIFIKATION am Geraet (Modul-FW 1.4.12.12)

Init-Log 5.57 bestaetigt alle drei neuen Schritte + zwei bemerkenswerte Befunde:

- **Step 3d Echo aus:** SPEM,M=0 ok. Ab da zeigt der Log `< @R,...` OHNE vorangestelltes
  Echo -> Echo ist wirklich aus, bt_command laeuft weiter (Token-Match echo-unabhaengig). Wie erwartet.
- **Step 3e Paketierung:** `.CYSPPGK` lieferte **W=0A (10 ms)** als Ist-Wert - der Werksdefault
  auf 1.4.12.12 ist also 10 ms, nicht die dokumentierten 5 ms (v2.2-Guide). Umso wichtiger der Fix:
  auf W=01 (1 ms) gesetzt. Gate erkennt das kuenftig als "already ok".
- **Step 3f Security - substanzieller Befund:** `GSBP` lieferte **M=41**, NICHT M=00. Dekodiert:
  High-Nibble 4 (Classic: MITM nicht noetig, general bonding, auto-accept), **Low-Nibble 1 = BLE
  "Required - General Bond"**. Das Modul verlangte also auf der BLE-Seite **Bonding** - potenzielle
  Quelle fuer iOS-Pairing-Dialoge. A5 hat das auf M=00 (Just Works, kein Bond) gestellt. A5 war
  hier also KEINE Kosmetik, sondern eine echte Aenderung.
- **Step 4:** /RBT + Verify nach 3 Anlaeufen ok.

Alle Gates lesen jetzt den Zielwert zurueck (Adv O=0000, Echo M=00, Paket W=01, Security M=00)
-> beim naechsten Init muessen 3c-3f "already ok" liefern (kein zweiter Reflash/Reboot). DAS ist
noch zu bestaetigen: einmal erneut booten und pruefen, dass keiner der Schritte nochmal schreibt.

## Update 5.58 - BT-Info-Menue zeigt die neuen Config-Werte

send_bt_info() (Menue Bluetooth->Information) zeigt jetzt zusaetzlich GAP (Adv-Dauer O=),
GPEM (Echo M=), .CYSPPGK (Paketierung W=) und GSBP (Security M=). Damit lassen sich die
ab 5.55/5.57 gesetzten Werte direkt ablesen, ohne den Boot-Log abzuwarten:
- GAP    -> O=0000  = dauerhaftes Advertising
- GPEM   -> M=00    = Echo aus
- .CYSPPGK -> W=01  = 1 ms Paketierung
- GSBP   -> M=00    = Just Works
Nur Anzeige, keine Logikaenderung.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.58).

## Update 5.58 - VERIFIKATION: Idempotenz am Geraet bestaetigt

Zweiter Boot mit 5.58: alle Config-Schritte melden "already ok", kein Reflash, kein Reboot
("Step 4: nothing changed -> done"). Die Selbstheilungs-/Idempotenz-Logik ueber ALLE
Config-Bereiche ist damit am Geraet bestaetigt. Rueckgelesene Zielwerte im Info-Menue:
- GAP L=12C0, O=0000  -> Advertising dauerhaft, 3 s
- GPEM (Echo) aus     -> Log zeigt durchgehend `< @R,...` ohne Kommando-Echo
- .CYSPPGK W=01       -> 1 ms Paketierung
- GSBP M=00           -> Just Works

Damit sind 5.51-5.58 (CYSPP-Fix, Sniff aus, CoD seriell, dauerhaftes Advertising, Echo,
Paketierung, Just-Works) am Geraet verifiziert und stabil. Config-Runde abgeschlossen.

## Update 5.59 - Download wieder langsam: aggressives Advertising (High-Duty) war die Ursache

**Befund:** Nach 5.58 war der Classic-SPP-Download wieder extrem langsam. Zaehler: lost=0s
(putb sauber, NICHT die Bremse), aber TXstuck=11 (verworfene Bytes -> Blockneuversand). Die Zeit
ging ins Warten auf die X/Y-Modem-ACKs = Funkstrecke.

**Ursache = eigene Init-Aenderung (kein externer Faktor):** Das dauerhafte Advertising aus
5.55/5.56 nutzte T=03 (Undirected HIGH duty) mit H=0030 = 30 ms Intervall fuer D=30 s, und F=01
startet das bei JEDEM Disconnect neu. Der IF820 ist Dual-Mode mit EINEM Funkteil -> das
High-Duty-Advertising (alle 30 ms) funkte gegen den Classic-Download an. Bei kurzen Link-Abrissen
waehrend der Uebertragung (marginales RF / CYSPP-Float) feuerte es via F=01 immer wieder neu.
Vor 5.55 (schneller 5.54-Lauf) stoppte Advertising nach 90 s -> keine Contention.

**Hinweis:** Advertising mitten in einer Classic-Sitzung per /AX zu pausieren geht NICHT - das
Modul ist dann im transparenten Datenmodus, ein Kommando ginge als Nutzdaten an den PC. Loesung
daher rein ueber die Adv-Parameter.

**Fix (5.59):**
- **T=03 -> T=04** (Undirected LOW duty only, KEINE 30-ms-High-Duty-Phase). Advertising bleibt
  dauerhaft (O=0000) und connectable im 3-s-Takt (L=12C0), aber kaum Funkzeit -> keine Contention.
  Gate bt_get_adv prueft jetzt T=04 UND O=0000 (damit 5.55-5.58-Geraete mit T=03 sauber upgraden).
- **TXstuck-Byte-Drop behoben** (sio.c putb): THRE-Wait-Timeout 50 ms -> BT_TXWAIT_MS 1000 ms +
  osThreadYield. Bei echtem CTS-Rueckstau wird das Byte jetzt NICHT mehr verworfen (das zerstoerte
  X/Y-Modem-Bloecke -> Neuversand). Der Dead-/Werksmodul-Fall wird weiter von der CTS-Schleife
  (CTS_WAIT_MS=50) abgefangen, bevor die THRE-Schleife greift.

**Geaenderte Dateien:** sio.c, btio.c, btio.h, sprachen/sictxt.c (-> 5.59).

**Am Geraet zu verifizieren (nicht kompiliert/getestet):**
1. Init: Step 3c schreibt einmal neu (T=03 alt -> T=04), Reboot; danach "already ok".
2. Info-Menue: GAP zeigt T=04, O=0000.
3. Download wiederholen -> sollte wieder schnell sein (Richtung 5.54: 4 MB < 15 min). TXstuck
   sollte klein/0 bleiben. Falls WEITER langsam -> Advertising war doch nicht die Ursache, dann
   PC-seitiges Sniff/Energieverwaltung oder RF pruefen.

---

## Update 5.60 - BT-Diagnose im Testmenue: Event-Sniff + CYSPP-Contention-Test

**Hintergrund:** Logic-Analyzer-Messung (Anwender, 2026-08-10) an der CYSPP-Leitung
(Modulseite, IF820 Pin 12) zeigt einen klaren Unterschied zwischen Classic und BLE:
- **Classic:** CYSPP faellt beim Connect sauber auf LOW und bleibt dort (Referenz, funktioniert).
- **BLE:** CYSPP bleibt ueber die gesamte Aufzeichnung (~20 s, inkl. eines TxD/RTS-Ausschlags
  vermutlich beim Subscribe) durchgehend HIGH - faellt kein einziges Mal ab.

Das bestaetigt empirisch den Kernverdacht aus `Doku/HW_Massnahmen_Bewertung_2026-08.md`
(R77 fehlt -> nur der interne PCA9554-Pull-up haelt die Leitung HIGH). Offen bleibt aber, ob
das HIGH selbst den GATT-Disconnect ausloest: laut EZ-Serial-Guide (S. 25-26) fuehrt ein
dauerhaft HIGH stehendes CYSPP bei aktivem Autostart-Profil zu "keine Daten fliessen", nicht
zwingend zu einem sofortigen Verbindungsabbruch. Um zwischen "Pin-Ursache" (Modul trennt lokal,
gap_disconnected reason=0x0916) und "RF/Brownout" (Supervision-Timeout, reason=0x0908) zu
unterscheiden, braucht es den rohen EZ-Serial-Event-Stream waehrend eines BLE-Subscribe-Versuchs
- den gab es bisher nicht (kein UART1-Event-Parser im Projekt).

**Umsetzung - zwei rein diagnostische, reversible Testmenue-Funktionen (btio.c/btio.h):**
- `bt_event_sniff()`: liest bis zu 60 s lang roh vom UART1-RX-Ringpuffer mit (Muster wie
  `bt_query_show()`/`bt_cyspp_monitor()`: eigene Terminal-Ausgabe ueber `connect=concpy&~UART1`,
  nichts geht zurueck ans Modul), druckt alles druckbare 1:1 durch, filtert nur CR. Ziel:
  waehrend App-Subscribe live `gap_connected` (C), `gatts_data_written` (W) und vor allem
  `gap_disconnected` (DIS, mit reason=) sichtbar machen. Guard: nur `fp.btmodem==IF820` und
  `uart1_owner==MUX_BT` (sonst wuerde bei aktiver GSM/GPS-Nutzung von UART1 deren Datenstrom
  mitgelesen). Bricht bei Tastendruck/Comchange ab (Endlosschleifenschutz wie bei
  `bt_cyspp_monitor`).
- `bt_cyspp_contention_test()`: IC54 Bit7 (dieselbe Leitung wie CYSPP) fuer 500 ms aktiv als
  Ausgang LOW getrieben (Gegenstueck zu `bt_cmdmode()`, das HIGH treibt), danach sofort
  `bt_release()`. Fuer Messprotokoll Schritt 6 (`Doku/Messprotokoll_CYSPP_Diagnose.md`):
  Delta-U ueber R78 (100 Ohm) waehrenddessen messen, um eine Bus-Contention mit einem evtl.
  gleichzeitig treibenden Modul-Ausgang zu erkennen, bevor endgueltig zwischen Weg B (ohne R77)
  und Weg C (mit R77) entschieden wird.
- `bt_diag_menu()`: gemeinsamer Einstieg (ein Ziffernplatz im Testmenue war frei), fragt
  1=Event-Sniff / 2=Contention-Test ab.

**Menue:** Testmenue (`sicom.c` case 0x0003) um Punkt 8 "BT-Diagnose (Event-Sniff/Contention)"
erweitert (`case 0x0038`, ruft `bt_diag_menu()`). Die bisherige Testmenue-Return-Case-Nummer
verschiebt sich von 0x0038 auf 0x0039 (rein technisch, gleiches Verhalten wie zuvor - faellt in
den bestehenden `default`-Zweig, `menu>>=4`). CYSPP-Monitor (Punkt 7, `case 0x0037`) unveraendert.

**Bewusst NICHT invasiv:** Keine Aenderung an `init_bluetooth()`, `bt_cmdmode()`/`bt_release()`
oder sonst am produktiven BT-Ablauf - beide neuen Funktionen sind reine Diagnose, nur ueber das
Testmenue erreichbar, greifen nur waehrend ihres eigenen Aufrufs ein.

**Geaenderte Dateien:** btio.c, btio.h, sicom.c, sprachen/sictxt.c (-> 5.60).

**Am Geraet als Naechstes zu tun (nicht kompiliert/getestet):**
1. Testmenue -> 8 -> 1 (Event-Sniff) starten, dann per App BLE verbinden + auf `...ca102`
   subscriben. Erwartung: `gap_connected`/`gatts_data_written`-Events sichtbar, dann entweder
   Daten oder `gap_disconnected,...,reason=0x0916` (Pin-Theorie bestaetigt) bzw. `reason=0x0908`
   (eher RF/Brownout, siehe Massnahme 3/8 in `HW_Massnahmen_Bewertung_2026-08.md`).
2. Danach Testmenue -> 8 -> 2 (Contention-Test) fuer Messprotokoll Schritt 6: Oszi auf CH1/CH2
   (R77-Pad/R78-Pad) legen, Delta-U waehrend der 500 ms notieren.
3. Ergebnis in `Doku/Messprotokoll_CYSPP_Diagnose.md` Schritt 6 eintragen bzw. den
   Disconnect-Reason-Code hier als neues Update dokumentieren.

---

## Update 5.61 - FIX: bt_event_sniff() brach sofort ab (UART1 gehoerte GSM/GPS)

**Symptom (erster Geraetetest von 5.60):** Testmenue -> 8 -> 1 brach sofort mit "UART1 gehoert
aktuell nicht dem BT-Modul (GSM/GPS aktiv) - abgebrochen." ab, bevor ueberhaupt etwas mitgelesen
werden konnte.

**Ursache:** Denkfehler im Guard. `uart1_owner!=MUX_BT` ist der NORMALFALL im Ruhebetrieb (UART1
haengt meist an GSM), kein Fehlerzustand - die urspruengliche 5.60-Fassung wertete das faelschlich
als Abbruchgrund, statt sich die Leitung selbst zu holen.

**Fix (btio.c, `bt_event_sniff()`):** Wie `send_bt_info()` es bereits vormacht, fordert die
Funktion UART1 jetzt aktiv per `Init_BT_ch(BT_BAUD)` an (verdraengt GSM/GPS noetigenfalls ueber
den MUX, exakt das bestehende, akzeptierte Verhalten anderer BT-Diagnosefunktionen) und gibt es
am Ende per `uart1_release(MUX_BT)` wieder frei. Der vorherige Owner-Guard entfaellt komplett.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.61).

**Am Geraet zu verifizieren:** Testmenue -> 8 -> 1 muss jetzt bis zu 60 s laufen (Abbruch nur per
Taste), auch wenn direkt zuvor GSM aktiv war.

---

## Update 5.62 - FIX: bt_event_sniff() brach bei jedem Modul-Byte ab + verschluckte Menue-Eingaben

**Symptom (zweiter Geraetetest von 5.61):** Event-Sniff brach weiterhin fast sofort ab
("Event-Sniff beendet." kam quasi ohne Wartezeit). Zusaetzlich beobachtet: **waehrend** des
BT-Tests bekam das Hauptmenue von sich aus Eingaben ("0", "0", "3", "5", ...) und navigierte
selbststaendig, ohne dass jemand etwas eingegeben hatte.

**Ursache - zwei zusammenhaengende Effekte:**
1. `rbuf`/`bxi`/`rxi` ist EIN gemeinsamer Ringpuffer fuer UART0 (RS232-Terminal) UND UART1
   (BT/GSM-Modul) - beide Interrupt-Handler (`hard.c` `UART0_IRQHandler`/`UART1_IRQHandler`)
   schreiben hinein. `getkey()` (`sio.c`) bricht bei JEDEM neuen Byte in diesem Puffer ab, ohne
   zwischen Terminal-Tastendruck und BT-Modul-Traffic zu unterscheiden.
2. `bt_event_sniff()` (5.60/5.61) nutzte `getkey(100)` als Abbruchbedingung - bei laufendem
   BT-Verkehr (also genau dem, was beobachtet werden soll) kommt praktisch sofort neuer Byte an,
   `getkey()` bricht ab, und das dabei noch UNGELESENE Byte bleibt im Puffer stehen. Der normale
   Menue-Zeichenleser (`getc()`/`getbyte()`, dieselben `bxi`/`rxi`) liest dieses Byte beim
   naechsten Durchlauf und interpretiert es als Tastatureingabe - da EZ-Serial-Eventzeilen
   (`@E,001A,...`) haeufig Ziffern enthalten, fuehrte das zu genau der beobachteten
   selbststaendigen Menuenavigation.

**Fix (btio.c, `bt_event_sniff()`):** `getkey()` komplett entfernt. Die Funktion laeuft jetzt
FEST 30 Sekunden (300 * 100 ms `osDelay`, kein Fruehausstieg bei Traffic oder Taste), drainiert
`bxi` weiterhin fortlaufend selbst, und verwirft am Ende explizit alle Restbytes (`bxi=rxi`)
bevor sie zurueckkehrt und `connect` wiederherstellt - damit kann nichts mehr an den Menue-
Zeichenleser durchsickern.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.62).

**Am Geraet zu verifizieren:** Testmenue -> 8 -> 1 muss jetzt durchgehend 30 s lang Modul-Traffic
zeigen (kein vorzeitiger Abbruch), und nach Ablauf darf das Hauptmenue NICHT von selbst
weiternavigieren, auch wenn waehrenddessen viel BT-Verkehr lief.

---

## Update 5.63 - Event-Sniff mit Zeitstempel + erste echte Event-Auswertung (BLE-CYSPP)

**Erster erfolgreicher Mitschnitt (5.62, Anwender):** Waehrend `ble_cyspp_probe.py` lief, zeigte
der Event-Sniff:
```
@E,0035,C,C=01,A=9C971BE9FA3D,T=00,I=0006,L=0000,O=0064,B=00   <- gap_connected
@E,000E,ASC,S=00,R=03
@E,001A,W,C=01,H=0012,T=00,D=0100                               <- gatts_data_written = CCCD-Subscribe
@E,000C,.CYSPP,S=01                                             <- CYSPP-Datenmodus GESTARTET
@E,0010,DIS,C=01,R=0913                                         <- Disconnect
@E,000E,ASC,S=04,R=01
@E,000C,.CYSPP,S=00
```
**Wichtigster Befund:** `.CYSPP,S=01` erscheint direkt nach dem Subscribe (`W`) - CYSPP startet
also tatsaechlich. Der Disconnect (`DIS,...,R=0913` = "Remote user terminated connection")
kam laut Python-Log erst 8056 ms spaeter, exakt am Ende des Skript-eigenen 8-Sekunden-
Lauschfensters - d. h. **das Skript selbst hat getrennt**, nicht das Modul. Dieser Durchlauf war
also ein ECHTER Erfolg: CYSPP hielt die volle Beobachtungszeit.

**Neuer Datenpunkt aus dem `gap_connected`-Event:** `O=0064` = Supervision-Timeout = 0x64 * 10 ms
= **nur 1000 ms**. Ungewoehnlich kurz (ueblich sind 4-6 s) - ein einziger 1-Sekunden-Funkaussetzer
(RF/Brownout) reicht, um die Verbindung zu killen. Passt zur Beobachtung, dass es in anderen
Durchlaeufen NICHT hielt (Abbruch nach 5,1 s bzw. 5,0 s in fruaheren/spaeteren Laeufen) - eher
Symptom einer instabilen HF-/Spannungslage als eines harten CYSPP-Pin-Logikfehlers.

**Ungeklaerte Anomalie (Anwenderbeobachtung):** Beim naechsten Durchlauf erschien im RS232-Log
nur `C -> ASC -> DIS -> ASC` (OHNE `W`/`.CYSPP`), obwohl das Python-Skript zeitgleich einen
erfolgreichen Subscribe + einen ECHTEN 5,07-s-Abbruch meldete. Zeitliche Zuordnung zwischen
RS232-Log und Python-Zeitstempeln war NICHT moeglich, weil dem Sniff bisher jeglicher Zeitstempel
fehlte - die Ereignisse koennten aus einem ueberlappenden/vorherigen Verbindungszyklus stammen
(die `client.connect()`-Aufrufe brauchen im Test 13-30 s, was Ueberlappungen zwischen
Sniff-Fenster und Python-Durchlauf wahrscheinlich macht).

**Fix/Erweiterung (btio.c, `bt_event_sniff()`):** Jede Zeile bekommt jetzt einen Zeitstempel
`[NNNNms]` relativ zum Start des Sniffs (`mstimer`-Differenz), damit sich das Modul-Log kuenftig
exakt mit den Millisekunden-Zeitstempeln von `ble_cyspp_probe.py` abgleichen laesst.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.63).

**Am Geraet als Naechstes:** Sniff + Python-Skript nochmal parallel laufen lassen, jetzt mit
Zeitstempeln auf beiden Seiten; pruefen ob `.CYSPP,S=01` / `DIS,...,reason=` bei den
"echten" Fruehabbruechen (~5 s) ebenfalls auftaucht und mit welchem reason-Code. Danach:
Recherche, mit welchem EZ-Serial-Kommando sich der Supervision-Timeout (`O=0064`, 1000 ms) auf
einen groesseren, saferen Wert setzen laesst (vermutlich ueber die GAP-Verbindungsparameter-
Praeferenz, die die 0x2A04-Characteristic fuellt).

---

## Update 5.64 - Event-Sniff-Laufzeit 30s -> 180s

**Anmerkung Anwender:** `ble_cyspp_probe.py --runs 3` (Standard) braucht bei diesem Setup deutlich
laenger als 30 s - allein der Verbindungsaufbau dauert 13-30 s PRO Durchlauf, dazu 8 s Lauschzeit
und 6 s Pause je Durchlauf, macht in Summe leicht 1,5-2+ Minuten fuer alle drei Durchlaeufe. Die
bisherigen 30 s (5.62/5.63) deckten also nur einen Bruchteil eines kompletten Testlaufs ab.

**Fix (btio.c, `bt_event_sniff()`):** Laufzeit auf 1800*100 ms = 180 s (3 min) verlaengert - deckt
einen kompletten "--runs 3"-Durchlauf komfortabel ab. Sonst keine Verhaltensaenderung (weiterhin
feste Laufzeit ohne Fruehausstieg, siehe 5.62-Fix; Zeitstempel je Zeile, siehe 5.63).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.64).

---

## ⚠ KURSWECHSEL - DVK-Nachweis: CYSPP-Ursache ist Firmware, nicht Hardware (Doku-Korrektur, kein Codechange)

**Quelle:** `Doku/ble-Dvk-request.md` - Ergebnis unabhaengiger, ausfuehrlicher Tests auf einem
IF820-DVK (andere KI-Session, Debug-Probe auf GPIO 19 = If820Board.CYSPP). Reproduzierbar bei
identischer CYSPP-Konfiguration:

| Zustand der Host-CYSPP-Leitung | Ergebnis |
|---|---|
| aktiv LOW | Fehler - kein nutzbarer Datenpfad |
| aktiv HIGH | Fehler - BLE-Schreiben erscheinen als EZ-Serial-GATT-Events, nicht als UART-Nutzdaten |
| **Eingang / hochohmig** | **Pass** - CCCD 0100, UART<->BLE transparent |

**Das kippt die bisherige Kernannahme:** Ein gemessenes HIGH ist KEIN Beweis fuer den
richtigen Zustand - aktiv HIGH und hochohmig sind beide als HIGH messbar, verhalten sich am
IF820 aber nachweislich unterschiedlich. Die bisherige Verdaechtigung "R77 (Pull-down) fehlt,
daher liegt CYSPP nur ueber den PCA9554-Pull-up auf HIGH" (`HW_Massnahmen_Bewertung_2026-08.md`)
ist damit NICHT der Kernfehler. Der tatsaechliche Fehler: unsere Firmware treibt die Leitung
ueber `bt_cmdmode()` (IC54 Bit 7 als Ausgang HIGH) an mehreren Stellen AKTIV, was laut DVK-Test
explizit verboten ist - auch wenn der Pegel dabei "richtig" (HIGH) aussieht.

**Betroffene Stellen (noch NICHT geaendert, nur identifiziert):**
- `bt_disconnect()` (btio.c:449) - SPP-Trennung erzwingen
- `init_bluetooth()` (btio.c:617) - Reconfig-Command-Mode erzwingen
- `uart1_request()` (hard.c:483) - beim Verdraengen von BT durch GSM/GPS (`bt_displaced`)
- Bluetooth-Deinstallieren (sicom.c:376, case -1)

Alle vier widersprechen der DVK-bestaetigten Regel "Leitung nie aktiv treiben - auch nicht bei
UART1-Muxwechsel oder Fehler-/Timeout-Pfaden". Die DVK-Doku fordert zusaetzlich: Bit 7 muss
ab Boot/Reset sofort auf Eingang stehen (PCA9554 haelt seinen Zustand ueber einen reinen
MCU-Reflash - deckt sich mit unserem 5.51-Fund "PCA9554 haelt Register ueber Reflash"), und es
braucht eine Recovery ohne Reboot, falls Bit 7 doch mal als Output haengen bleibt.

**Wichtig - auch unser eigener `bt_cyspp_contention_test()` (5.60) bestaetigt implizit den
DVK-Befund:** aktives LOW-Treiben ist laut DVK ebenfalls ein Fehlerzustand, nicht nur HIGH.

**Konsequenz fuer die bisherige Doku:** `HW_Massnahmen_Bewertung_2026-08.md` und
`Messprotokoll_CYSPP_Diagnose.md` (Wege A/B/C, R77/P27-Umverdrahtung) sind als Loesungsweg
ueberholt - mit Hinweis-Box am Dateianfang markiert, aber nicht geloescht (Messhistorie).
**Voraussichtlich ist KEINE Hardware-Aenderung noetig**, nur eine Firmware-Ueberarbeitung von
`bt_cmdmode()`/den vier Aufrufstellen. `CLAUDE.md` entsprechend aktualisiert.

**Naechster Schritt (Entscheidung mit Anwender, noch nicht umgesetzt):** Ersatz-Mechanismus fuer
"Command-Mode erzwingen"/"SPP trennen" finden, der CYSPP NICHT aktiv treibt - z. B. ueber
`/DIS` direkt (falls im Datenmodus sendbar) oder ueber die im DVK-Dokument vorgeschlagene
Zustandsregel (Boot/Reset/Advertising/Connect/Disconnect/Muxwechsel -> immer Eingang) plus
Wartezeit statt aktivem Treiben. Betrifft moeglicherweise auch die Zuverlaessigkeit der
Classic-SPP-Trennung (Timeout-Disconnect, Reconfig-waehrend-BT-Verbindung), die bisher auf
`bt_cmdmode()` beruht - hier vorsichtig vorgehen, da Classic aktuell funktioniert.

**Umsetzungsplan (freigegeben):** 4 Aufrufstellen von `bt_cmdmode()`, ein Schritt pro Version,
jeweils mit eigenem Test am Geraet, bevor der naechste Schritt beginnt (Plan-Details siehe
Session-Verlauf): 1) `hard.c uart1_request()` GSM-Verdraengung, 2) `bt_disconnect()`
(BT-Timeout), 3) `init_bluetooth()` (Reconfig-Anfang), 4) `sicom.c` BT-Deinstallieren.
Danach 5) Recovery-Fenster in `measure.c` von 60 s auf wenige Sekunden verkuerzen, 6) Abschlussdoku.

---

## Update 5.65 - Schritt 1: kein bt_cmdmode() mehr beim GSM/GPS-Verdraengen (hard.c)

**Aenderung (hard.c, `uart1_request()`):** Der Block, der bei Verdraengung des IF820 von UART1
durch GSM/GPS waehrend einer aktiven BT-Verbindung (`bt_displaced`) `bt_cmdmode()` (CYSPP HIGH)
aufrief, ist ersatzlos entfernt (samt der jetzt ungenutzten `bt_displaced`-Variable). Dieser Pfad
feuerte bisher AUTOMATISCH und UNSICHTBAR jedes Mal, wenn GSM/GPS UART1 brauchte, waehrend BT
verbunden war - der wahrscheinlichste Erklaerungsansatz fuer die bisher beobachtete Zufaelligkeit
der BLE-Testergebnisse (mal haelt die Verbindung 8 s, mal bricht sie nach 5 s ab).

**Bewusste Nebenwirkung:** Der urspruengliche Zweck dieses Codes (Update 5.48: Terminal-Junk
unterdruecken, wenn die IF820-RXD beim Mux-Wegschalten floatet) faellt weg. Auf einem per
Classic-SPP verbundenen Terminal kann waehrend eines GSM/GPS-Mux-Wechsels wieder kurz
Zeichenmuell auftreten - rein kosmetisch, kein Verbindungsabbruch. Ein sauberer Ersatz (Host-
seitiges Verwerfen statt Treiben von CYSPP) ist separat vorgesehen, aber nicht Teil dieses
Schritts.

Der Rueckweg (GSM/GPS -> BT: `bt_release()` bei echter Rueckkehr) bleibt unveraendert - er tat
bereits das Richtige (freigeben statt treiben).

**Geaenderte Dateien:** hard.c, sprachen/sictxt.c (-> 5.65).

**Am Geraet zu testen (Reihenfolge siehe Plan):**
1. `ble_cyspp_probe.py --runs 5` parallel mit Event-Sniff (Testmenue 8->1, 180 s) - Ziel:
   `.CYSPP,S=01` haelt konsistent ueber die volle Lauschzeit, kein spontaner Abbruch mehr nach
   ~5 s.
2. Konkurrenztest: BT verbinden (Classic oder BLE), dann eine GSM-Aktion ausloesen, die UART1
   braucht, waehrenddessen `bt_cyspp_monitor` (Testmenue 7) beobachten - Bit7 darf nicht auf
   "HIGH (Ausgang)" wechseln, BT-Verbindung darf nicht abreissen.
3. Classic-SPP-Regression: normale Menuebedienung ueber Bluetooth, auch waehrend eines
   GSM-Mux-Wechsels - kurzer Zeichenmuell ist akzeptiert, ein echter Abbruch nicht.

---

## Update 5.66 - BLE robuster: kuerzeres Advertising + Verbindungsparameter-Praeferenz (PPCP)

**Anlass:** Tests zeigten sehr lange (13-47 s) und teils fehlschlagende Verbindungsaufbauten,
sowie in den Event-Logs unterschiedliche, vom Central (Windows) frei gewaehlte Supervision-
Timeouts (einmal 1 s, einmal 9,6 s) - wir hatten dafuer bisher ueberhaupt keine eigene
Konfiguration gesetzt. Anwenderwunsch: Robustheit erhoehen, Stromsparen ist aktuell zweitrangig.
Zwei unabhaengige, kleine Config-Schritte nach bekanntem A1/A2/A5-Muster (get -> set -> /SCFG
bzw. RAM -> verifizieren, Fehlschlag nicht fatal):

**1) Advertising-Intervall verkuerzt (btio.h `T_sap`):** Low-Duty-Intervall `L=` von 3 s
(0x12C0, Update 5.56) auf **200 ms** (0x0140) gesenkt - schnellerer Verbindungsaufbau. Bleibt
weit unter der 5.59-Grenze (30 ms High-Duty, die die Classic-SPP-Kontention verursacht hatte),
also keine erneute Funk-Kontention zu erwarten. `bt_get_adv()` prueft jetzt zusaetzlich das
L-Feld (vorher nur T und O) - sonst haetten bereits konfigurierte Geraete (5.55-5.59, L=12C0)
den neuen Wert nie automatisch uebernommen, weil ihr T/O bereits als "ok" gegolten haette.

**2) Verbindungsparameter-Praeferenz neu gesetzt (btio.c `bt_ensure_connparams()`):** Bisher
wurde die BLE-Standard-Characteristic "Peripheral Preferred Connection Parameters" (0x2A04,
GAP-Service) NIE beschrieben - der Central bestimmt Intervall/Latency/Timeout bisher komplett
selbst. Guide (Kap. 3.8.1.5): "select a longer supervision timeout, shorter connection
interval, and/or lower slave latency value to achieve required connection stability." Neuer
Zielwert (8 Byte, SIG-Format, little-endian): Intervall 15-30 ms, Slave-Latency 0 (nie
aussetzen), Supervision-Timeout **6 s** (vs. den beobachteten 1 s in einer BLE-Sitzung).
Geschrieben ueber die generische `/WLH` (gatts_write_handle) API auf Handle 0x0007 - laut
EZ-Serial-Guide-Referenzbeispiel (Kap. 3.8) der Value-Handle von 0x2A04 im Standard-GATT-Layout
(GAP-Service wird beim Stack-Init immer zuerst angelegt, unsere CYSPP-/App-Services bekommen
spaeter hoehere Handles). Sicherheitsnetz: `bt_get_connparams()` verifiziert vor dem Schreiben
die Antwortlaenge (PPCP ist IMMER genau 8 Byte/16 Hex-Zeichen - keine andere Standard-
Characteristic hat zufaellig dieselbe Laenge); bei unplausibler Laenge wird NICHT geschrieben
(Warnung statt moeglicher Beschaedigung einer falschen Characteristic). Kein `/SCFG`, da reiner
RAM-Wert des lokalen GATT-Servers - wird deshalb bei jedem Init neu gesetzt (kein Flash-Write,
daher unkritisch). Trotzdem nicht garantiert wirksam: der Central KANN diesen Hint ignorieren
(Windows/iOS teils eigene Defaults, Android meist zuverlaessig).

Beide Werte jetzt auch im BT-Info-Menue sichtbar (`GAP` zeigt L=, neue Zeile fuer `/RLH,H=0007`
zeigt D=).

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.66).

**Am Geraet zu verifizieren:**
1. BT-Info-Menue: `GAP` muss `L=0140` zeigen, neue `/RLH`-Zeile muss `D=0C00180000005802` zeigen.
2. `ble_cyspp_probe.py` mehrfach: Verbindungsaufbau sollte spuerbar schneller sein als vorher
   (13-47 s). Im Event-Sniff sollte das naechste `gap_connected`-Event (`C,...,O=`) idealerweise
   `O=0258` (6 s) zeigen, falls Windows die Praeferenz uebernimmt - falls nicht, ist das ein Hin-
   weis, dass Windows sie ignoriert (kein Fehler unsererseits, siehe Einschraenkung oben).
3. Classic-SPP-Regression weiterhin unauffaellig.

---

## Update 5.67 - Eigener BLE-Transfer-Test (Firmware-Gegenstelle + PC-Skript)

**Anlass:** Der Zulieferer-DVK-Test (`SupplierSourceCode/.../tests/Bluetooth-Simulation/ble/
cyspp_dvk_hil.py`) hat einen robusten 40-kB-CRC/ACK-Integritaetstest, laeuft aber nur gegen das
IF820-DVK (braucht dessen zweite USB-Debug-Verbindung zum Modul, `If820Board`/`common_lib`).
Gegen unser Mainboard nicht direkt lauffaehig. Anwenderwunsch: einen eigenen, gleichwertigen
Test (1. Smoke-Test mit Debug-Infos, 2. 40-kB-Datentransfer) OHNE DVK, direkt gegen das
Mainboard.

**Firmware-Gegenstelle (btio.c, `bt_transfer_echo()`, neuer Testmenue-Punkt 8->3):**
Empfaengt Frames ueber UART1 (vom PC per BLE-Write transparent durch CYSPP durchgereicht),
Wire-Format bewusst IDENTISCH zum Zulieferer-Referenztest, damit Ergebnisse vergleichbar sind:
- Frame: `"CYS1" + Sequenz(4B LE) + Groesse(2B LE) + CRC32(4B LE) + Payload`
- ACK: `"ACK1" + Sequenz(4B LE)`, geht transparent per CYSPP/BLE-Notify zurueck an den PC.

Neu implementiert: `crc32_calc()` (bitweise IEEE-802.3/zlib-kompatible CRC-32, gab es im Projekt
noch nicht) und ein kleiner Byte-Parser mit 3 Zustaenden (Sync/Header/Payload) ueber die
bekannte `bxi`/`rxi`-Ringpuffer-Schleife. **Wichtig:** ACK-Bytes werden ueber `pchar()` gesendet,
NICHT `putc()` - `putc()` wuerde jedes 0x0A-Byte in Binaerdaten faelschlich zu `\r\n` machen
(LF->CRLF-Uebersetzung, `sio.c`). Feste Laufzeit 120 s, kein Fruehausstieg (gleicher Grund wie
`bt_event_sniff`, 5.62-Fix). `bt_diag_menu()` um Option 3 erweitert (kein neuer Testmenue-Platz
noetig, war schon der Sammel-Einstieg fuer Diagnosefunktionen).

**PC-Skript (neu, `Doku/tools/ble_mainboard_test.py`):**
- `--mode smoke`: Verbindungstest mit Phasen-Timings (Scan/Connect/GATT-Discovery/Subscribe je
  in ms), volle GATT-Tabelle, haelt die Verbindung danach `--hold` Sekunden (Default 8s) und
  prueft auf Spontan-Disconnect. Braucht KEINE Firmware-Gegenstelle. JSON+Markdown-Report unter
  `Doku/tools/Testreports/`.
- `--mode transfer`: sendet `--bytes` (Default 40 kB, wie DVK-Referenz) CRC/Sequenz-Frames per
  Write-without-Response, Sliding-Window-ACK (Default Fenster 4, 3 Retries, 8000 B/s Zielrate,
  `--rate 0`/`--window 0` fuer unpaced Burst), automatische Recovery (Fenster->1, Rate halbiert)
  bei Timeout wie im DVK-Vorbild. Verifikation laeuft rein ueber die vom Mainboard per CRC
  bestaetigten ACKs - kein zusaetzlicher Kanal noetig (einfacher als beim DVK, der dafuer eine
  zweite UART-Direktverbindung brauchte).
- Referenzwert zum Vergleichen: DVK-Referenzlauf 40 kB, Fenster 4, 0 Verlust/Korruption,
  ~7900 B/s.

**Geaenderte/neue Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.67),
Doku/tools/ble_mainboard_test.py (neu).

**Am Geraet zu testen:**
1. `python Doku/tools/ble_mainboard_test.py --mode smoke` - sollte "passed" mit stabilen
   Phasen-Zeiten liefern (nach 5.66 idealerweise deutlich unter den bisherigen 13-47 s Connect).
2. Firmware Testmenue -> 8 -> 3 starten, dann `python Doku/tools/ble_mainboard_test.py --mode
   transfer` - Ziel: 40 kB ohne Retransmissions/Timeouts, Durchsatz in Richtung der DVK-Referenz
   (~7900 B/s). Fehlende ACKs/viele Retransmits deuten auf denselben Instabilitaets-Kandidaten
   (RF/Brownout/Supervision-Timeout) wie in den bisherigen Tests.

**⚠ Bewusst TEMPORAER (Anwenderentscheidung 2026-08-18):** `bt_transfer_echo()` ist reiner
Diagnose-Code fuer die BLE-CYSPP-Untersuchung, kein produktives Feature. Wird wieder entfernt,
sobald die CYSPP-Diagnose abgeschlossen ist (siehe Aufraeum-Punkt weiter unten im Dokument /
CLAUDE.md-Backlog). Betrifft: `bt_transfer_echo()`, `crc32_calc()`, `bt_send_ack()` (btio.c),
die zugehoerige `extern`-Deklaration (btio.h) und Menuepunkt 8->3 in `bt_diag_menu()`.

---

## Update 5.68 - FIX: bt_diag_menu() gab bei Stoerbytes kommentarlos auf

**Symptom (Anwender):** Testmenue -> 8 (BT-Diagnose) mehrfach gewaehlt, aber die Auswahl
1/2/3 kam nie zur Wirkung - direkt nach der Anzeige "BT-Diagnose: 1 = ... 3 = ..." sprang das
Menue kommentarlos zurueck zum Testmenue, ohne dass eine Ziffer sichtbar eingegeben wurde. Erst
beim vierten Versuch klappte "1" (Event-Sniff).

**Ursache:** `bt_diag_menu()` nutzte `getc()` fuer die Ziffernabfrage - `getc()`/`getbyte()`
lesen vom selben gemeinsamen UART0/UART1-Ringpuffer (`rbuf`/`bxi`/`rxi`) wie GSM-/BT-Modul-
Traffic (identische Ursache wie der 5.62-Fix bei `bt_event_sniff()`). Ein einzelnes Stoerbyte
(z. B. GSM-Chatter), das genau in dem Moment ankam, in dem der Anwender "1"/"2"/"3" eingeben
wollte, wurde als "ungueltige Eingabe" gewertet - die Funktion gab sofort auf und kehrte zum
Testmenue zurueck, ohne den Anwender ueberhaupt zum Zug kommen zu lassen.

**Fix (btio.c, `bt_diag_menu()`):** Bis zu 20 Versuche: ungueltige Zeichen werden jetzt
ignoriert und die Eingabe wiederholt, statt beim ersten Stoerbyte aufzugeben. Nur ein echter
Timeout/Comchange (kein Zeichen ueberhaupt, `getc()` liefert < 0) beendet die Schleife vorzeitig.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.68).

**Am Geraet zu verifizieren:** Testmenue -> 8 -> Ziffer eingeben muss jetzt zuverlaessig beim
ersten Versuch funktionieren, auch wenn GSM im Hintergrund aktiv ist.

---

## Update 5.69/5.70 - FIX comchange blockierte bt_diag_menu() vollstaendig + Diagnose-Zaehler

**Symptom (Anwender, nach 5.68):** Testmenue -> 8 -> Ziffer eingeben sprang WEITERHIN sofort
zurueck, obwohl der 5.68-Fix (20 Wiederholungen) bereits eingebaut war.

**Ursache (tiefer als 5.68 angenommen):** `getc()`/`getbyte()` (sio.c) brechen SOFORT mit -1 ab,
wenn das globale `comchange`-Flag (Schnittstellenwechsel, von `EINT1_IRQHandler` bei JEDER
IC54-Pegelaenderung gesetzt - u. a. die CYSPP/BT_LINK-Leitung) beim Aufruf bereits gesetzt ist -
NOCH BEVOR ueberhaupt einmal gewartet/yieldet wird. Der 5.68-Fix half deshalb nicht: ohne jeden
Yield dazwischen bekam nichts anderes im System die Chance, `comchange` zu loeschen - alle 20
Versuche schlugen in Mikrosekunden fehl. Etabliertes Muster aus `gpsio.c` ("Einschaltung von
GSM/GPS Versorgung kann scheinbaren Schnittstellenwechsel verursachen"): `comchange` VOR dem
Warten explizit loeschen.

**Fix (btio.c, `bt_diag_menu()`):** `comchange` wird jetzt vor jedem Versuch per
`clear_comchange()` verworfen, bevor `getc()` es sehen kann; bei Fehlschlag `osDelay(50)` statt
Busy-Spin. Nach den 20 Versuchen erscheint jetzt eine klare Meldung statt stillem Abbruch.

**Zusaetzlich (Anwenderwunsch): TEMPORAERER Diagnose-Zaehler `comchange_count`** (hard.c/.h,
volatile, in `EINT1_IRQHandler` hochgezaehlt) - zeigt, WIE OFT das IC54/CYSPP-Interrupt seit Boot
gefeuert hat. Sichtbar in `bt_diag_menu()` (Stand vor/nach der Warteschleife + Anzahl Versuche)
und in `bt_cyspp_monitor()` (Testmenue 7, Start-/Endwert + Delta). Ziel: klaeren, ob die
floatende CYSPP-Leitung (DVK-Nachweis: muss hochohmig bleiben) durch elektrisches Rauschen
tatsaechlich bouncet (viele Interrupts ohne echte Verbindungsaenderung) - das waere zugleich eine
Erklaerung fuer das bt_diag_menu()-Symptom UND ein Hinweis auf einen moeglichen Nebeneffekt von
Schritt 1 des CYSPP-Plans (5.65: CYSPP wird jetzt oefter/laenger als reiner Eingang belassen als
vorher, floatet also mehr).

**Geaenderte Dateien:** btio.c, hard.c, hard.h, sprachen/sictxt.c (-> 5.70).

**Am Geraet zu verifizieren:**
1. Testmenue -> 8 -> Ziffer muss jetzt beim ersten Versuch reagieren.
2. `comchange_count`-Werte notieren: springt er zwischen zwei Menueaufrufen (Sekunden
   auseinander) um viele hundert/tausend, ist die CYSPP-Leitung vermutlich elektrisch instabil/
   bouncet - dann Testmenue 7 (CYSPP monitor) laufen lassen und das Delta ueber ~10-30 s
   beobachten. Ein Delta nahe 0 bei ruhigem, unverbundenem Geraet waere unauffaellig.
3. Nach Abschluss der CYSPP-Diagnose: `comchange_count` ist TEMPORAERER Diagnose-Code (wie
   `bt_transfer_echo()`) und gehoert zum spaeteren Aufraeumen dazu.

**Zum Smoke-Test-Ergebnis des Anwenders (2 von 3 Laeufen "device_not_found", einer mit
manuellem Abbruch beim Verbinden):** passt weiterhin zum bekannten Bild instabiler/langsamer
Verbindungsaufbauten (siehe 5.60-5.66) - vermutlich RF/Brownout, nicht durch dieses Menue-Problem
verursacht. Bitte nach 5.68 nochmal neu testen, jetzt ohne die Menue-Fehlbedienung als Stoerfaktor.

---

## Doku-Korrektur (2026-08-18) - drei Annahmen widerlegt/korrigiert

Nach den `comchange_count`-Messungen (5x Delta=0 im Ruhezustand - CYSPP-Leitung elektrisch
sauber, kein Bounce) und drei erneut fehlgeschlagenen BLE-Connects hat der Anwender drei
Denkfehler in meiner Analyse aufgedeckt:

1. **32-kHz-Quarz Q3 ist bestueckt** (Anwender hat es am Board geprueft) - der bisher in
   CLAUDE.md gelistete Punkt "Modul laeuft auf internem LPO, Timing ungenau" entfaellt.
2. **Modul-FW-Unterschied entkraeftet:** Das Eval-Kit aus `ble-Dvk-request.md` (erfolgreicher
   BLE-Test) lief ebenfalls auf 1.4.12.12, nicht 1.4.18.18 - das war ein *anderes* Eval-Kit als
   das in Update 5.47 verglichene. Referenzboard und Mainboard laufen also auf identischer
   Modul-Firmware.
3. **Reine HW/RF-Erklaerung insgesamt unwahrscheinlich:** Classic Bluetooth laeuft auf demselben
   Board (gleicher Funkteil/Antenne/Stromversorgung) sauber - eine reine Antennen-/Entkopplungs-/
   Stromversorgungs-Erklaerung muesste Classic genauso treffen. Zusaetzlich war GSM/LTE waehrend
   der fehlgeschlagenen BLE-Tests durchgehend AUS - GSM-Interferenz scheidet fuer diese
   Fehlschlaege damit aus.

**Neue Arbeitshypothese:** Der Unterschied liegt vermutlich in BLE-spezifischen Protokoll-/
Timing-Eigenschaften, nicht in gemeinsam genutzter Hardware. BLE hat einen deutlich kuerzeren,
beobachteten Supervision-Timeout (teils nur 1 s, siehe Update 5.63) und einen zeitkritischeren
Verbindungsaufbau als Classic (reifere, fehlertolerantere Baseband-Prozedur mit laengerem
Timeout ~20 s). Fokus bleibt auf den bereits angegangenen BLE-Verbindungsparametern
(PPCP/Supervision-Timeout, Update 5.66) statt auf Antenne/Stromversorgung/GSM.

**CLAUDE.md** entsprechend aktualisiert (Abschnitt "Offene / zu beobachtende Punkte").

**Naechster Schritt:** Pruefen, ob der PPCP-Wert aus 5.66 tatsaechlich wirkt - BT-Info-Menue
(`/RLH,H=0007` sollte `D=0C00180000005802` zeigen) und beim naechsten Connect-Versuch im
Event-Sniff den `O=`-Wert im `gap_connected`-Event ablesen (Ziel: 0258 = 6 s statt der
beobachteten 1 s).

**Ergebnis Init-Log (Anwender, Modul nach langem Power-Cycle wieder online):** Advertising-
Intervall wurde erfolgreich von `L=12C0` (3 s) auf `L=0140` (200 ms) umgestellt (Update 5.66
wirkt). Der PPCP-Schreibversuch (`/WLH,H=0007,...`) schlug fehl ("Setzen fehlgeschlagen") -
Handle 0007 ist auf diesem Geraet offenbar nicht die 0x2A04-Characteristic (war eine Annahme
aus dem Guide-Beispiel, keine gepruefte Tatsache). **Entscheidung (Anwender):** PPCP/GATT-
Discovery NICHT weiterverfolgen - Aufwand (echte GATT-Discovery fuer den realen Handle) steht
in keinem Verhaltnis zum Nutzen, da unser Testaufbau (Windows-PC) PPCP-Hints oft ohnehin
ignoriert und PPCP erst NACH Verbindungsaufbau wirkt (das eigentliche Problem - Verbindungsaufbau
selbst dauert 13-47 s oder scheitert komplett - bleibt davon unberuehrt). Der fehlgeschlagene
Schreibversuch ist harmlos (schlaegt sauber fehl, aendert nichts) und bleibt vorerst im Code.

**Wichtige Zwischen-Erkenntnis (Anwender-Einwand, korrigiert mehrere Annahmen):**
Bluetooth Classic laeuft auf demselben Board sauber - eine reine Hardware-/RF-Erklaerung
(Antenne/Entkopplung/GSM-Interferenz) fuer die BLE-Verbindungsprobleme ist damit unwahrscheinlich
(gleicher Funkteil/Antenne/Versorgung wie BLE). GSM/LTE war waehrend der fehlgeschlagenen BLE-
Tests durchgehend AUS. Ausserdem: 32-kHz-Quarz Q3 ist bestueckt (frueherer CLAUDE.md-Punkt war
falsch), und das erfolgreiche Referenz-Eval-Kit (`ble-Dvk-request.md`) lief ebenfalls auf
Modul-FW 1.4.12.12 (kein FW-Unterschied zum Mainboard). Neue Arbeitshypothese: BLE-spezifische
Protokoll-/Timing-Eigenschaften (kurzer Supervision-Timeout, zeitkritischerer Verbindungsaufbau
als Classic), nicht gemeinsam genutzte Hardware. CLAUDE.md entsprechend korrigiert.

**Separat notiert (nicht Teil dieser Untersuchung):** Anwender berichtet gelegentlichen
kompletten Modul-Ausfall (`/PING` scheitert komplett), nur durch Power-Cycle mit laengerer
Stromlos-Zeit behebbar - als eigener offener Punkt in CLAUDE.md aufgenommen, moeglicher
Zusammenhang mit dem fehlenden RST_L-Zugang (R124-Pad), nicht bestaetigt.

---

## Update 5.71 - Schritt 2: kein bt_cmdmode() mehr in bt_disconnect()

**Aenderung (btio.c, `bt_disconnect()`):** Der `bt_cmdmode()`-Aufruf (CYSPP HIGH treiben, um vor
`/DIS` den Command-Mode zu erzwingen) ist entfernt. `/DIS` wird weiterhin versucht - falls das
Modul noch im Datenmodus ist, geht der Befehl einfach als Nutzdaten durch und bewirkt nichts
(kein Fehler, nur kein aktives Trennen in diesem Fall). `bt_release()` bleibt als Sicherheitsnetz
(idempotent). Genutzt vom BT-Inaktivitaets-Timeout (`measure.c`).

**Bewusste Nebenwirkung:** Ohne erzwungenen Command-Mode kann `/DIS` in einigen Faellen wirkungs-
los verhallen (Modul noch transparent) - dann bleibt nur die virtuelle Trennung (Flags loeschen),
wie vor Update 5.47b. Das ist ein Rueckbau auf ein frueher akzeptiertes Verhalten, kein neues
Risiko.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.71).

**Am Geraet zu testen (Schritt 2 aus dem Plan):**
1. BT-Verbindung (Classic) aufbauen, `BT_TIMEOUT` abwarten, pruefen dass die Verbindung sauber
   endet (Terminal zeigt "Bluetooth getrennt", `bt_cyspp_monitor` zeigt Bit7 zurueck auf Eingang,
   danach ist ein Neuverbinden sofort moeglich).
2. Gleiches mit BLE: Client verbinden+subscriben (KEIN eigenes Disconnect im Client), Geraet-
   seitigen Timeout abwarten lassen, pruefen ob der Client sauber getrennt wird und ein erneuter
   Connect danach klappt.
3. Falls das eben beobachtete "Verbindung trennen haengt sich auf" (Anwenderbericht vor 5.65)
   reproduzierbar war: jetzt nochmal genau denselben Ablauf testen - haelt es jetzt, war es
   dieser Bug.

---

## Update 5.72 - FIX: bt_diag_menu() (5.70) verschluckte echte BT-Disconnects dauerhaft

**Symptom (Anwender, nach 5.71):** Windows-seitiger BT-Classic-Disconnect (HyperTerminal ueber
BT-COM-Port "Trennen") hing sehr lange, endete mit Windows-Fehlern ("Kein Prozess am anderen Ende
der Pipe", "Zeitlimit fuer die Semaphore erreicht"). Danach lief das Geraet auf RS232 extrem
langsam (5 s fuer ein paar Menuezeilen). `bt_cyspp_monitor()` zeigte `CYSPP bit7 = LOW` - die
Firmware dachte weiter, sie sei verbunden, obwohl der Windows-Link laengst tot war.

**Der Anwender hat zurecht widersprochen, dass das Zufall/Ueberlastung sei - es lag tatsaechlich
an unserem eigenen Code (5.70-Fix), nicht am Modul oder an RF.**

**Ursache - schwerer Folgefehler aus dem 5.70-Fix:** `bt_diag_menu()` rief bei gesetztem
`comchange` blind `clear_comchange()` auf, um den Merker zu verwerfen (Ziel damals: `getc()`
nicht durch ein stehendes Stoerbyte sofort abbrechen lassen). Das ist aber grundlegend falsch,
wenn `comchange` durch eine ECHTE `BT_LINK`-Zustandsaenderung (z. B. einen echten Disconnect)
ausgeloest wurde: Die EINZIGE Stelle, die `comchange` tatsaechlich AUSWERTET und `connect`/
`BT_LINK` korrekt nachfuehrt, ist `communication_change()` (libtool.c), normalerweise aus
`measure.c` aufgerufen, sobald `comchange` gesetzt ist. `clear_comchange()` loescht den Merker
aber OHNE `communication_change()` je aufzurufen - die Firmware hat den echten Disconnect
dadurch NIE verarbeitet. `connect&BT_LINK` blieb fuer immer auf "verbunden" haengen (Split-
Brain-Zustand), und jede Terminalausgabe wurde weiter sinnlos Richtung UART1 gespiegelt und
wartete dort auf CTS/THRE-Timeouts (bis zu ~1 s pro Byte) - das erklaert exakt die beobachtete
Verlangsamung auf RS232.

**Fix (btio.c, `bt_diag_menu()`):** Bei gesetztem `comchange` wird jetzt `communication_change()`
selbst aufgerufen (macht die vollstaendige, korrekte Verarbeitung UND loescht `comchange`
danach) - mit derselben Schutzbedingung wie in `measure.c` (`!osActiveThread`, um einen
laufenden GSM/MQTT-Verbindungsthread nicht zu stoeren; ist ein Thread aktiv, wird ausnahmsweise
weiterhin nur `clear_comchange()` verwendet, wie zuvor).

**Wichtig fuer die laufende CYSPP-Untersuchung:** Dieser Bug war eine Nebenwirkung unserer
EIGENEN Diagnose-Tooling-Erweiterung (5.70), NICHT des DVK-Umbauplans (Schritte 1+2, 5.65/5.71)
selbst. Die zuvor vermutete "GSM-Masking"-Erklaerung fuer das fruehere Hang-Symptom war falsch
und wurde zurueckgenommen (siehe Konversation) - der tatsaechliche Fehler liegt in `bt_diag_menu()`.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.72).

**Am Geraet zu verifizieren:** BT-Classic verbinden, ueber Windows/HyperTerminal trennen -
`bt_cyspp_monitor()` sollte danach zuverlaessig `HIGH` zeigen (nicht mehr dauerhaft haengend
LOW), und RS232 sollte nicht mehr traege werden. Falls doch nochmal reproduzierbar: pruefen, ob
zum Zeitpunkt des Disconnects zufaellig Testmenue -> 8 offen war (das ist die einzige Stelle,
die je den fehlerhaften Pfad ausgeloest haben kann).

**Am Geraet bestaetigt:** Anwender hat BT-Classic-Connect/Disconnect via Windows/HyperTerminal
erneut getestet - funktioniert jetzt wieder sofort, kein Haengen mehr.

---

## Update 5.73 - Schritt 3 CYSPP-Plan: kein bt_cmdmode() mehr in init_bluetooth()

**Aenderung (btio.c, `init_bluetooth()`):** Der unbedingte `bt_cmdmode()`-Aufruf am Anfang
(CYSPP HIGH treiben, um vor der Modul-Erkennung/Rekonfiguration den Command-Mode zu erzwingen)
ist entfernt, ebenso das zugehoerige `osDelay(600)`.

**Bewusste Nebenwirkung (Rueckbau auf den Stand vor Update 5.05):** Besteht beim Aufruf von
`init_bluetooth()` eine aktive BT/BLE-Verbindung, wird das Modul nicht mehr zwangsweise in den
Command-Mode gebracht - die Erkennung darunter scheitert dann einfach (Modul bleibt transparent,
`/PING` kommt auf keiner Baud durch), wie vor Update 5.05 dokumentiert. Reconfig waehrend einer
aktiven Verbindung braucht dann wieder manuelles Trennen oder Konfiguration ueber RS232 (Update
5.04 hatte das bereits als akzeptierte Einschraenkung dokumentiert). Der weitaus haeufigere Fall
- Boot-Selbstheilung bzw. Reconfig OHNE aktive Verbindung - ist von dieser Aenderung nicht
betroffen: `init_bluetooth()` laeuft in dem Fall unveraendert durch.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.73).

**Am Geraet zu testen (Schritt 3 aus dem Plan):**
1. Normaler Boot mit bereits korrekt konfiguriertem Modul: `test_BT()` muss weiterhin sofort
   erfolgreich sein, `init_bluetooth()` wird gar nicht erst aufgerufen (unveraendertes Verhalten).
2. Erzwungene Neuerkennung (z. B. Modul auf Werksbaud zuruecksetzen oder `fp.btmodem=0` im
   Konfigmenue, OHNE aktive BT-Verbindung): `init_bluetooth()` muss weiterhin sauber durchlaufen
   (Schritte 1-4, 3a-3g).
3. Reconfig-Versuch WAEHREND aktiver BT-Verbindung (Menue "Bluetooth?y"): akzeptiertes neues
   Verhalten ist ein sauberer Fehlschlag/Hinweis statt automatischem Kill - pruefen, dass das
   nicht z. B. haengt, sondern sauber abbricht.

---

## Update 5.74 - KORREKTUR Schritt 2: bt_cmdmode() in bt_disconnect() wieder eingebaut

**Anlass:** Am Geraet reproduziert - nach Schritt 2 (5.71, kein bt_cmdmode() mehr) feuerte der
6-Minuten-Inaktivitaets-Timeout, `/DIS` erreichte das Modul nicht (Modul im Datenmodus, Befehl
verhallt wirkungslos), das Modul blieb CYSPP LOW haengen ("Bluetooth getrennt" im Log, danach
`W11015 CTS Timeout`, RS232-Terminal komplett blockiert - jede Ausgabe wartete auf CTS Richtung
eines nicht mehr antwortenden Moduls).

**Korrigierte Erkenntnis:** Der DVK-Befund ("CYSPP nie aktiv treiben") bezieht sich auf
UNGEWOLLTES Stoeren laufenden Betriebs (Schritt 1: GSM-Verdraengung; Schritt 3: Reconfig-
Erzwingen). `bt_disconnect()` ist aber eine BEWUSST GEWOLLTE, sofortige Terminierung - in diesem
Moment spielt der Transport (Classic/BLE) keine Rolle, weil die Verbindung ohnehin sofort endet.
Kurzes CYSPP-HIGH-Treiben genau in diesem Moment widerspricht dem DVK-Befund nicht.

**Aenderung (btio.c, `bt_disconnect()`):** `bt_cmdmode()` + `osDelay(150)` vor `/DIS` wieder
eingebaut (Stand wie vor 5.71). Damit erreicht `/DIS` das Modul wieder zuverlaessig, auch wenn
es sich gerade im Datenmodus befindet.

**Schritt 4 (sicom.c "Bluetooth deinstallieren") neu bewertet - KEINE Aenderung noetig:** Diese
Stelle (`bt_cmdmode(); osDelay(300); bt_release();`) ist bereits derselbe Fall wie
`bt_disconnect()` - eine bewusste, zeitlich begrenzte Trennung durch Anwenderaktion. Der
urspruengliche Plan haette das faelschlich auch entfernt (derselbe Fehler wie Schritt 2). Bleibt
unveraendert.

**Backlog-Idee dokumentiert (kein Code):** Classic/BLE-Transport-Erkennung wurde als moegliche
"sauberere" Alternative diskutiert (transport-spezifisches Verhalten statt einer pauschalen
"bei bewusstem Trennen ist Treiben ok"-Regel) - Aufwand/Risiko dafuer sind erheblich (siehe
CLAUDE.md), deckt sich mit der bereits zurueckgestellten BLE-Durchsatz-Option B. Bewusst nicht
jetzt umgesetzt.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.74).

**Am Geraet zu testen:**
1. BT-Verbindung aufbauen, `BT_TIMEOUT` abwarten (oder testweise verkuerzen) - Terminal darf
   danach NICHT blockieren, `bt_cyspp_monitor()` muss `HIGH` zeigen.
2. Windows-seitiges Trennen (HyperTerminal ueber BT-COM-Port) - muss weiterhin sofort klappen
   (5.72-Fix bleibt unveraendert wirksam).
3. "Bluetooth deinstallieren" waehrend aktiver Verbindung - muss weiterhin sauber trennen.

**Naechster Schritt (Schritt 5 aus dem Plan, noch offen):** Wachhund fuer den Fall, dass
`BT_LINK` trotz allem unplausibel lange aktiv bleibt (z. B. wenn auch der jetzt korrigierte
`bt_disconnect()` das Modul nicht erreicht) - Design noch abzustimmen.

**Test 1-3 am Geraet bestaetigt (Anwender):** 6-Minuten-Timeout jetzt sauber (kein CTS-Timeout,
`bit7` geht auf HIGH zurueck, RS232 bleibt schnell). Windows-Trennen weiterhin sofort. "Bluetooth
deinstallieren" ueber die BT-Verbindung selbst funktioniert; die anschliessend beobachtete
Fortsetzung des Konfigurations-Wizards (USB/Modem/Turnswitch/viaspeedcam-Abfragen) auf RS232 ist
erwartetes Verhalten (RS232 spiegelt einfach die laufende Menuefuehrung mit, kein Bug).

---

## Update 5.75 - PPCP-Setzen entfernt (schlug immer fehl, nur Anzeige bleibt)

**Anlass:** Der `/WLH`-Schreibversuch aus 5.66 schlug bei jedem Init fehl ("BT-ConnParams:
WARNUNG - Setzen fehlgeschlagen") - Handle `0007` war nur eine Annahme aus dem EZ-Serial-Guide-
Referenzbeispiel, auf unserem Geraet nicht die tatsaechliche 0x2A04-Characteristic. Da (Update
5.66-Diskussion) ohnehin entschieden wurde, keine echte GATT-Discovery dafuer zu bauen, ist der
wiederholte Fehlschlag nur noch stoerende Log-Ausgabe ohne Nutzen.

**Aenderung (btio.c/.h):** `bt_get_connparams()`, `bt_set_connparams()`, `bt_ensure_connparams()`
sowie die zugehoerigen Makros `T_wlh_ppcp`/`T_wlhok`/`IF820_PPCP_VALUE` und der Aufruf in
`init_bluetooth()` (Schritt 3g) sind entfernt. Die reine Anzeige im BT-Info-Menue
(`bt_query_show(T_rlh_ppcp)`, liest per `/RLH` nur den aktuellen Wert aus, schreibt nichts)
bleibt erhalten - harmlos, keine Fehlermeldung moeglich.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.75).

**Am Geraet zu verifizieren:** Init-Log zeigt keine "BT-ConnParams"-Zeile mehr; BT-Info-Menue
zeigt weiterhin die `/RLH`-Zeile mit dem aktuellen (unveraenderten) Wert.

---

## Update 5.76 - Schritt 5 CYSPP-Plan: Recovery-Fenster 60s -> 5s

**Aenderung (measure.c):** Die bestehende Selbstheilung (5.51: "CYSPP darf im verbindungslosen
Ruhezustand NIE als Ausgang haengen" - `if ((fp.btmodem==IF820) && !(connect&BT_LINK) &&
(uart1_owner!=MUX_BT)) bt_release();`) lief bisher nur einmal pro RTC-Minutentick (`if
(minute)`). Jetzt laeuft dieselbe Pruefung unabhaengig vom Minutentick alle 5 Sekunden (eigener
`mstimer`-basierter Zaehler, wie an anderen Stellen im Code ueblich). Bewusst schlank gehalten:
keine neue Logik, nur ein kuerzeres Intervall fuer die bereits bewaehrte, idempotente Pruefung -
kein Retry-Mechanismus, keine Eskalation (das haette fuer ein Problem, das mit Bordmitteln
ohnehin nicht loesbar ist - ein wirklich haengendes Modul braucht Power-Cycle/RST, siehe
CLAUDE.md-Punkt "Modul faellt gelegentlich komplett aus" - keinen Mehrwert, nur zusaetzliches
Risiko).

**Warum das jetzt reicht:** Mit dem 5.74-Fix (bt_cmdmode() wieder in bt_disconnect()) sollte der
6-Minuten-Timeout das Modul jetzt zuverlaessig zum echten Trennen bringen. Dieser Wachhund ist
das zusaetzliche Sicherheitsnetz fuer den selteneren Fall, dass CYSPP trotzdem als Host-Ausgang
haengen bleibt (z. B. durch einen noch unbekannten Randfall) - Erholzeit jetzt 5s statt bis zu 60s.

**Damit sind alle 5 Schritte des DVK-Umbauplans umgesetzt** (Schritt 4 brauchte keine Aenderung,
siehe Update 5.74).

**Geaenderte Dateien:** measure.c, sprachen/sictxt.c (-> 5.76).

**Am Geraet zu verifizieren:** Normalbetrieb unauffaellig (Selbstheilung ist idempotent, laeuft
jetzt nur oefter). Funktional nicht einfach isoliert testbar (der Fall "Host treibt HIGH und
haengt" ist selten) - vor allem beobachten, ob Regressionen bei GSM/BT-Umschaltung auftreten.

---

## Update 5.77 - Schritt 5 TEMPORAER ZURUECKGENOMMEN (Verdacht: 5.76 verursachte erneutes Haengen)

**Symptom (Anwender, sauber reproduziert):** Geraet aus -> RS232 verbinden -> Geraet an -> BT-
Hyperterminal verbinden -> 40 s warten -> BT-Hyperterminal trennen -> haengt lange, endet mit
"Kein Prozess ist am anderen Ende der Pipe". `bt_cyspp_monitor()` zeigte danach wieder
`bit7 = LOW`, RS232 wieder traege - exakt dasselbe Symptombild wie vor dem 5.74-Fix, obwohl
5.74 das Problem bestaetigt behoben hatte (Test 2 vom Anwender explizit bestaetigt).

**Zeitliche Eingrenzung:** Zwischen dem bestaetigt funktionierenden 5.74 und dem erneuten
Auftreten liegen nur 5.75 (PPCP-Aufraeumung, inhaltlich unabhaengig von BT-Disconnect) und 5.76
(Schritt-5-Wachhund: `bt_release()`-Selbstheilung von 1x/Minute auf 1x/5s verkuerzt). Verdacht:
5.76 interferiert auf noch unbekannte Weise mit dem Disconnect-Ablauf.

**Nebenbefund geklaert (kein Bug):** Die zwischenzeitlich beobachtete zweite "Viasis 3003M..."-
Banner-Ausgabe auf RS232 war KEIN Geraete-Neustart, sondern die normale Begruessungsnachricht,
die `communication_change()` bei JEDER neuen BT-Verbindung sendet (`startmessage=1`) - wird auf
RS232 mitgespiegelt, weil beide Schnittstellen (`connect`-Bits UART0+UART1) gleichzeitig aktiv
waren. Keine Aenderung noetig.

**Aenderung (measure.c):** Schritt 5 (5.76) komplett zurueckgenommen - `bt_release()`-Selbst-
heilung wieder wie vor 5.76, nur 1x pro RTC-Minutentick. Reiner Isolationsschritt: noch NICHT
bewiesen, dass 5.76 tatsaechlich die Ursache ist, nur der naheliegendste Verdaechtige (einzige
Aenderung im relevanten Zeitraum). Ziel: pruefen, ob das Symptom mit dem alten Takt verschwindet.

**Geaenderte Dateien:** measure.c, sprachen/sictxt.c (-> 5.77).

**Am Geraet zu testen:** Exakt denselben Ablauf wie oben (RS232 + BT verbinden, 40 s warten,
BT trennen) mehrfach wiederholen.
- **Haengt es NICHT mehr:** 5.76 war tatsaechlich die Ursache - Schritt 5 muss anders (seltener
  oder mit einer Schutzbedingung) umgesetzt werden.
- **Haengt es TROTZDEM noch:** 5.76 war nicht die Ursache - dann liegt es an etwas anderem, das
  wir noch nicht gefunden haben (ggf. ein rein zeitabhaengiges/RF-Problem, das mit unserem Code
  gar nichts zu tun hat).

**Ergebnis (Anwender):** 5.76 war NICHT die Ursache - derselbe Fehler trat auf 5.77 (mit
zurueckgenommenem Schritt 5) erneut auf, in einem minimalen Reproduktionsfall (Boot -> RS232 ->
BT verbinden -> kurz warten -> trennen), OHNE jede Testmenue-Funktion. Keine unserer Aenderungen
dieser Woche liegt in diesem Codepfad (weder Schritt 1-5 noch die Diagnose-Tools) - der
Verdacht verschiebt sich Richtung "schon immer vorhandenes Verhalten in `communication_change()`/
I2C-Statuslesen" oder echtes Modul-/RF-Problem. Zusaetzlicher Beleg: beim naechsten regulaeren
6-Minuten-Timeout kam trotz 5.74-Fix (bt_cmdmode() wieder in bt_disconnect()) erneut ein
`W11015 CTS Timeout` - selbst der erzwungene Command-Mode erreichte das Modul nicht. Spricht
fuer ein echtes Modul-Haenger-Problem (siehe CLAUDE.md "Modul faellt gelegentlich komplett aus"),
nicht fuer einen Pin-Logik-Fehler unsererseits.

---

## Update 5.78 - FIX: Diagnose-Tools (Event-Sniff/Transfer-Test) verschluckten comchange

**Bestaetigter, unabhaengiger Fund (bei der Fehlersuche zum obigen Update entdeckt):**
`mainmenu()` (sicom.c, komplette Menueverarbeitung) wird SYNCHRON aus der Hauptschleife in
`measure.c` (`runcycle()`, Zeile 338) aufgerufen - im selben Single-Thread-Kontext wie die
Pruefung `if (comchange) communication_change();`, die einen ECHTEN BT-Disconnect verarbeitet.
Jede lang blockierende Testmenue-Funktion (`bt_event_sniff()` 180s, `bt_transfer_echo()` 120s)
friert dadurch die GESAMTE Hauptschleife ein, inklusive der Disconnect-Erkennung - beide
Funktionen pruefen `comchange` waehrend ihrer Laufzeit bisher gar nicht. Ein echter Disconnect
waehrend eines laufenden Tests wuerde also komplett verschluckt, bis die Funktion fertig ist -
dieselbe Bugklasse wie der bereits behobene 5.72-Fix in `bt_diag_menu()`.

**Fix (btio.c):** In beiden Funktionen wird jetzt bei jedem Schleifendurchlauf (alle 100 ms)
geprueft: `if (comchange) { if (!osActiveThread) communication_change(); else
clear_comchange(); }` - identisches Muster wie in `measure.c`/`bt_diag_menu()`.

**Wichtig - erklaert NICHT den aktuellen Haenger:** Der oben beschriebene Fehlerfall trat OHNE
jede Testmenue-Funktion auf, dieser Fix behebt also ein echtes, aber ANDERES Problem (verhindert
zukuenftig, dass unsere eigenen Diagnose-Tools das CYSPP-Bild waehrend eines Tests verfaelschen).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.78).

**Am Geraet zu verifizieren:** Event-Sniff/Transfer-Test starten, waehrenddessen eine BT-
Verbindung trennen - `connect&BT_LINK` sollte jetzt prompt (binnen ~100ms) korrekt aktualisiert
werden, statt erst nach Ablauf der 180s/120s.

## Update 5.79: FIX PIN-Abfrage - Endlosschleife durch Nicht-Ziffer-Bytes als Fehlversuch gezaehlt

**Symptom (Anwenderbeobachtung, RS232-Log ueber COM3, BT-Verbindung ueber COM10):** Waehrend
einer aktiven BT-Verbindung erscheint am Terminal wiederholt und automatisch, ohne jede
Tastatureingabe, `PIN = ******` (sechs Sternchen) im Sekundentakt neu - nicht Zeichen fuer
Zeichen aufgebaut, sondern komplett fertig und sofort wieder neu ausgegeben. Reproduzierbar
unabhaengig vom urspruenglichen CYSPP-Thema; trat u. a. direkt im Anschluss an einen 180s
Event-Sniff-Lauf auf, sobald `mainmenu()` nach der Sniff-Pause wieder Bytes verarbeitete.

**Ursache (`sicom.c`, `mainmenu()`, PIN-Zweig `bt_pininit>0`):** Der Fehlversuchszaehler
`bt_pininit` wurde bisher fuer **jedes** empfangene Byte erhoeht, unabhaengig davon, ob es
tatsaechlich eine Ziffer war. Waehrend einer bestehenden BT-Verbindung treffen ueber den
gemeinsam genutzten UART0/UART1-Puffer laufend irgendwelche Nicht-Ziffer-Bytes ein (Modul-/
Protokoll-Rauschen, Steuerzeichen etc.) - jedes davon zaehlte als Fehlversuch. Nach 6 solcher
Bytes: `reset_bt_pin()` (PIN-Abfrage-Reset + Neuausgabe der PIN-Aufforderung), sofort erneut
gefuettert von den naechsten 6 eintreffenden Bytes - eine Endlosschleife, die sich selbst
staendig neu antreibt, solange die Verbindung aktiv ist und Bytes eintreffen. Voellig
unabhaengig von CYSPP/Pin-Treiben; ein reiner Zaehl-Bug im PIN-Handler.

**Fix (`sicom.c`):** Die gesamte Verarbeitung (Echo, Aufbau von `menu`, Zaehler-Erhoehung,
Erfolgs-/Fehlversuchspruefung) ist jetzt komplett hinter `isdigit(c)` gestellt. Nicht-Ziffer-
Bytes (ausser `'H'` fuer den expliziten Reset) werden still verworfen und zaehlen nicht mehr
als Fehlversuch.

**Geaenderte Dateien:** sicom.c, sprachen/sictxt.c (-> 5.79).

**Am Geraet zu verifizieren:** BT verbinden, PIN-Abfrage abwarten, PIN wie gewohnt eingeben -
sollte jetzt sauber funktionieren, auch waehrend im Hintergrund andere BT-Aktivitaet
(Event-Sniff, normale Verbindungsnutzung) laeuft. Insbesondere: kein automatisches, unaufge-
fordertes Neu-Anzeigen der PIN-Abfrage mehr ohne eigene Tastatureingabe.

**Nachtrag (2026-08-18) - 5.79 allein war NICHT ausreichend, echte Ursache war woanders:**
Nach Flashen von 5.79 trat die Endlosschleife ("PIN = ******" wiederholt, ohne Tastatur-
eingabe) weiterhin auf, reproduziert direkt im Anschluss an einen Event-Sniff-Lauf (Testmenue
8->1). Direkter Vergleich mit dem normalen, ungestoerten PIN-Ablauf (RS232-Log ohne Sniffer:
PIN-Prompt erscheint genau einmal, 6 Ziffern werden als `******` echot, danach sofort das
Hauptmenue - kein Wiederholen) zeigte: das Problem liegt nicht im Ziffern-Zaehler (5.79-Fix war
zwar sachlich richtig, aber nicht die Hauptursache), sondern in `bt_event_sniff()` selbst.

## Update 5.80: FIX Event-Sniff/Transfer-Test verschlucken eine waehrenddessen ueber BT
eingegebene PIN

**Ursache:** `bt_event_sniff()` liest waehrend der festen 180s-Laufzeit ALLE eingehenden
UART1-Bytes roh mit (reine Diagnose-Ausgabe) und verwirft am Ende alles Restliche im Puffer
(`bxi=rxi;` - "nichts darf ans Menue durchsickern"). Wird waehrend dieser 180s ueber BT die
echte PIN eingegeben (bt_pininit>0, PIN-Abfrage aktiv), werden genau diese Ziffern-Bytes vom
Sniffer als Rohdaten verschluckt statt an `mainmenu()`/den PIN-Handler weitergereicht - die
Eingabe geht spurlos verloren, `bt_pininit` bleibt auf 1 haengen. Alles was danach eintrifft,
landet in einem PIN-Handler ohne je eine vollstaendige, gueltige Eingabe zu bekommen - das
erzeugte die beobachtete Dauerschleife. Dieselbe Schwachstelle besteht in `bt_transfer_echo()`.

**Fix (btio.c):** Beide Funktionen pruefen jetzt beim Start `if (bt_pininit)` und brechen mit
einer Meldung ab ("PIN-Abfrage noch offen - erst PIN ueber BT eingeben, dann erneut starten."),
statt die laufende PIN-Eingabe zu zerstoeren.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.80).

**Am Geraet zu verifizieren:** BT verbinden, PIN-Abfrage bewusst NICHT sofort beantworten,
stattdessen ueber RS232 Testmenue -> 8 -> 1 (Event-Sniff) versuchen zu starten - sollte jetzt
sofort mit der neuen Meldung abbrechen statt 180s zu blockieren. Danach PIN normal eingeben -
sollte wie im ungestoerten Fall sauber funktionieren (kein Wiederholen mehr).

**Nachtrag (2026-08-18) - 5.80 behob nur den Start-Fall, nicht das eigentliche Problem:**
Anwendereinwand zurecht: der Start-Guard (`if (bt_pininit) return;`) greift nur, wenn die PIN-
Abfrage schon VOR dem Sniff-Start offen ist. Im echten Reproduktionsfall war das nie so -
mainmenu() haette bei offener PIN-Abfrage RS232-Eingaben (Navigation zu Testmenue 8->1) gar
nicht zugelassen (jedes Byte wird bei bt_pininit>0 als PIN-Ziffer interpretiert, unabhaengig
von der Schnittstelle). Die PIN-Abfrage entstand immer ERST waehrend des laufenden Sniffs
(neue BT-Verbindung mitten in den 180s), der Start-Guard lief also nie. Anwendervorschlag:
Sniff soll parallel/im Hintergrund mitlesen und aufs Terminal schreiben, ohne mainmenu() zu
stoeren - das ist der richtige Ansatz und behebt die Bugklasse an der Wurzel statt sie nur
abzufangen.

## Update 5.81: Event-Sniff komplett neu gebaut - nicht-blockierend, eigener Leseindex

**Alter Aufbau (5.62-5.80):** `bt_event_sniff()` blockierte 180s lang die komplette
Hauptschleife und las dabei ueber `bxi` denselben Index, den auch `mainmenu()`/die PIN-Abfrage
zum Verarbeiten von Eingaben benutzen. Der Sniffer war damit faktisch der alleinige "Besitzer"
des gemeinsamen Ringpuffers waehrend seiner Laufzeit - jede echte Eingabe (allen voran eine
gerade laufende BT-PIN-Abfrage) wurde als Rohdaten mitgelesen und am Ende verworfen (`bxi=rxi`),
statt bei `mainmenu()` anzukommen. 5.78/5.80/5.81 (verworfen) haben das nur nachtraeglich
abgefangen (comchange durchreichen, PIN-Start-Guard, Abbruch bei bt_pininit in der Schleife) -
alles Pflaster auf derselben Architekturschwaeche, keines davon hat den Fall "PIN-Abfrage
entsteht waehrend des Sniffs" wirklich sauber geloest.

**Neuer Aufbau:** Der Sniffer bekommt einen EIGENEN, von `bxi` unabhaengigen Leseindex
(`sniff_i`), der `rxi` genauso verfolgt wie `bxi` es tut, aber `bxi` selbst nie anfasst.
`mainmenu()` bekommt dadurch exakt dieselben Bytes wie ganz ohne Sniffer - der Sniffer ist ein
reiner Mitleser, kein Verbraucher. `bt_event_sniff()` (Testmenue 8->1) setzt nur noch den
Startzustand (`sniff_i=rxi`, Zeitstempel-Referenz) und kehrt SOFORT zurueck - das Testmenue
bleibt normal bedienbar. `bt_sniff_poll()` (neu, `measure.c`'s Hauptschleife, einmal je
Durchlauf, direkt nach der `mainmenu()`-Verarbeitung) liest non-blocking nach, was seit dem
letzten Aufruf neu in `rbuf` steht, und gibt es mit Zeitstempel am RS232-Terminal aus - ganz
ohne `osDelay()` oder Blockieren. Nach 180s (`mstimer`-basiert, kein Schleifenzaehler mehr)
schaltet sich der Sniffer selbst ab und gibt UART1 wieder frei.

**Vorteil:** Eine BT-PIN-Abfrage, die waehrend des Sniffs neu entsteht (der urspruengliche
Reproduktionsfall), wird jetzt ganz normal von `mainmenu()` verarbeitet - der Sniffer bekommt
dieselben Bytes nur zusaetzlich zum Mitlesen, nimmt sie `mainmenu()` aber nie weg. Die
komchange-Durchreich-Logik (5.78) und der PIN-Start-Guard (5.80) sind damit fuer diese Funktion
hinfaellig und wurden entfernt (weniger Spezialfaelle statt mehr).

**Geaenderte Dateien:** btio.c (Neubau `bt_event_sniff()`, neue Funktion `bt_sniff_poll()`),
btio.h (Deklaration `bt_sniff_poll`), measure.c (Aufruf `bt_sniff_poll()` in `runcycle()`),
sprachen/sictxt.c (-> 5.81).

**Am Geraet zu verifizieren:** Testmenue 8->1 (Event-Sniff) starten - Menue muss SOFORT wieder
bedienbar sein (kein 180s-Warten). Waehrend der 180s ueber BT verbinden, PIN eingeben - sollte
wie im ungestoerten Fall einmalig funktionieren, waehrend gleichzeitig am RS232-Terminal die
rohen `@E,...`-Eventzeilen mit Zeitstempel erscheinen. Nach 180s: "Event-Sniff (Hintergrund)
beendet" am Terminal, UART1 wieder frei fuer GSM/GPS.

**Noch offen (bewusst zurueckgestellt):** `bt_transfer_echo()` (Transfer-Test, Testmenue 8->3)
hat dieselbe blockierende Bauweise und denselben PIN-Start-Guard (5.80) wie die alte
Event-Sniff-Fassung, wurde aber NICHT auf das neue Poll-Modell umgebaut (komplexerer
Frame-Parser mit Zustandsmaschine, aufwendiger als reines Byte-Durchreichen). Da dieser Test
ohnehin als temporaerer Diagnose-Code zur Entfernung vorgesehen ist (siehe CLAUDE.md), nur bei
konkretem Bedarf nachziehen.

## Update 5.82: FIX Hauptmenue navigiert selbststaendig beim (fremdinitiierten) BT-Trennen

**Symptom (Anwenderbeobachtung):** Klickt der verbundene Client (HyperTerminal/viagraph) auf
"Trennen", navigiert das RS232-Hauptmenue direkt danach von selbst, ohne jede Tastatureingabe -
beobachtet als Zeichenfolge `0`, `0`, `1` (aus dem Zeitmenue zurueck ins Hauptmenue, dann in
"1. Datenausgabe").

**Ursache:** Dieselbe Bugklasse wie die PIN-Endlosschleife (5.79/5.80/5.81) - der gemeinsame
UART0/UART1-Ringpuffer (`rbuf`/`bxi`/`rxi`) unterscheidet nicht zwischen echten Tastatur-
eingaben und rohem Modul-Protokolltext. Beim (fremdinitiierten) Trennen sendet das Modul noch
Event-/Statustext ueber UART1 (Ziffern darin, z. B. Teil einer Disconnect-Meldung), der beim
`communication_change()`-Uebergang "Bluetooth Verbindung beendet" bisher NICHT verworfen wurde -
im Gegensatz zu `bt_disconnect()` (unser eigenes, gezieltes Trennen ueber `/DIS`), das genau
deshalb schon immer `bxi=rxi;` am Ende macht. Der fehlende Flush im automatisch erkannten
Trennen-Zweig liess die Rohbytes unveraendert bei `mainmenu()` als Tastatureingaben ankommen.

**Fix (`libtool.c`, `communication_change()`):** Im Zweig "Bluetooth Verbindung beendet" wird
jetzt ebenfalls `bxi=rxi;` gesetzt - Rest im Puffer wird verworfen, analog zu `bt_disconnect()`.

**Geaenderte Dateien:** libtool.c, sprachen/sictxt.c (-> 5.82).

**Am Geraet zu verifizieren:** BT verbinden, PIN eingeben, dann ueber den Client (HyperTerminal/
viagraph) trennen - das RS232-Hauptmenue sollte danach unveraendert stehen bleiben, keine
selbststaendige Navigation mehr.

**Verifiziert (2026-08-18):** Beide Fixes (5.81 Hintergrund-Sniff, 5.82 Puffer-Flush beim
Trennen) am Geraet bestaetigt - PIN-Eingabe waehrend laufendem Event-Sniff funktioniert sauber,
keine Endlosschleife mehr; Trennen vom Client aus navigiert das RS232-Menue nicht mehr von
selbst.

**Nachtrag (2026-08-18) - der eigentliche 40s-Idle-Haenger ist ein SEPARATES, bereits bekanntes
Problem, kein Software-Bug von heute:** Reproduziert (BT verbinden, PIN eingeben, 40s nichts
tun, dann vom Client trennen -> Windows-Fehler "Kein Prozess ist am anderen Ende der Pipe").
RS232 wird danach extrem langsam (jede Ausgabe wartet auf CTS/THRE-Timeout Richtung UART1, weil
`connect` noch `UART1|BT_LINK` gesetzt hat - Split-Brain, siehe bereits dokumentierter
Nachtrag oben unter "Offene/zu beobachtende Punkte"). CYSPP-Monitor (Testmenue 3->7) zeigt
waehrenddessen `bit7 = LOW` (Modul haelt Datenmodus fest) mit `comchange_count` unveraendert
(Delta=0) ueber volle 2 Minuten Beobachtung - keine einzige Pegelaenderung, kein Prellen. Das
Modul reagiert in dieser Zeit also gar nicht auf den laengst toten Host-Link. Deutet auf
echten Modul-internen Lockup hin (nicht auf einen normalen, laengeren Classic-Supervision-
Timeout - der waere kuerzer als 2 Minuten). Deckt sich mit dem bereits separat gefuehrten
CLAUDE.md-Punkt "BT-Modul faellt gelegentlich komplett aus" (`/PING` scheitert komplett, nur
Power-Cycle mit laengerer Stromlos-Zeit hilft, moeglicher Zusammenhang mit fehlendem RST_L/
R124-Pad). Keine weitere Live-Diagnose heute fortgesetzt (Diminishing Returns bei manueller
Menuenavigation waehrend der Traegheit). Fuer eine kuenftige Session sinnvoll: automatisches
Mitprotokollieren von CYSPP-Zustand+Zeitstempel statt manuellem Nachschauen alle paar Minuten.

## Update 5.83: FIX gemeinsamer UART-Ringpuffer konnte volllaufen und blieb dann DAUERHAFT tot

**Der eigentliche Kernfund der heutigen Session (Anwender-Hartnaeckigkeit hat das aufgedeckt -
"das liegt am Mikrocontroller, nicht am Modul" war richtig):**

`UART1_IRQHandler()` (hard.c) deaktiviert bei vollem `rbuf` (gemeinsamer 256-Byte-Ringpuffer
fuer RS232 UND BT/GSM) den kompletten UART1-Empfangsinterrupt (`NVIC_DisableIRQ`) und setzt
`rbuf_full=1` - das ist grundsaetzlich gewollte Hardware-Flusskontrolle (Auto-RTS haelt das
Modul dann automatisch an). Codeweite Suche zeigte aber: `rbuf_full` wird nur an ZWEI Stellen
je wieder zurueckgesetzt und der Interrupt reaktiviert (`NVIC_EnableIRQ`) - `modem_terminal()`
(libtool.c, unbeteiligtes "Matrix-Controller"-Feature) und `mqtt.c`'s MQTT-Nachrichtenschleife.
Im NORMALEN BT-Betrieb (measure.c Hauptschleife, mainmenu(), communication_change()) gab es
KEINE Stelle, die das prueft. Laeuft der Puffer also waehrend einer normalen BT-Sitzung mal
voll, bleibt UART1 fuer den Rest der laufenden Betriebszeit (bis zum naechsten Power-Cycle)
komplett taub - das Modul kann nichts mehr senden (kein Event, kein `/PING`, keine Disconnect-
Meldung). Erklaert vermutlich die ganze Verwirrung der heutigen Session: nach dem ersten
Ueberlauf (reproduziert beim 40s-Idle-dann-Trennen-Test) wirkten alle nachfolgenden Tests
in derselben laufenden Sitzung wie sich verschlimmernde/neue Bugs (Datenausgabe stockt "wie
sau", BT-Menue "geht nach 10s nicht mehr") - waren aber vermutlich alle Folge desselben, seit
dem ersten Ueberlauf nie wieder reparierten toten UART1-Interrupts.

**Warum der 40kb-Transfertest (`bt_transfer_echo()`) davon nie betroffen war:** Der liest den
Puffer in einer ENGEN Schleife komplett leer (`while (bxi!=rxi)`, alle 100ms), waehrend die
normale Menue-/Verbindungsverarbeitung nur 1 Byte pro komplettem Hauptschleifendurchlauf
konsumiert (measure.c, geteilt mit Messzyklen/GPS/MQTT etc.) - reicht fuer einzelne
Tastendruecke, aber nicht fuer einen Schwall mehrerer Modul-Event-Zeilen auf einmal (z. B. beim
Trennen).

**Fix (measure.c):** Zwei Massnahmen kombiniert:
1. Bis zu 16 Bytes pro Hauptschleifendurchlauf verarbeiten statt nur 1 (senkt die
   Wahrscheinlichkeit, ueberhaupt in den Vollzustand zu geraten, deutlich).
2. `if (rbuf_full && bxi==rxi) { rbuf_full=0; NVIC_EnableIRQ(UART1_IRQn); }` nach jedem
   Durchlauf - falls es doch passiert, wird der Interrupt automatisch wieder freigegeben, sobald
   der Puffer leergelaufen ist, statt dauerhaft tot zu bleiben.

**Geaenderte Dateien:** measure.c, sprachen/sictxt.c (-> 5.83).

**Am Geraet zu verifizieren:** Nach Flashen + Power-Cycle den 40s-Idle-dann-Trennen-Test
mehrfach hintereinander wiederholen, OHNE zwischendurch neu zu starten - sollte jetzt auch bei
wiederholten Tests in derselben Sitzung nicht mehr kumulativ schlechter werden. Datenausgabe
sollte nach einem BT-Vorfall nicht mehr dauerhaft "stocken".

**Noch offen:** Der genaue Ausloeser (was am Trennen nach 40s Leerlauf konkret einen Byte-
Schwall verursacht, den 5-10s-Verbindungen nicht zeigen) ist weiterhin nicht mit einem rohen
Mitschnitt bewiesen - nur die Fix-Wirkung (Wiederfreigabe + schnelleres Draining) ist unabhaengig
vom genauen Ausloeser wirksam. Bei Interesse: Event-Sniff (jetzt nicht-blockierend, 5.81) waehrend
eines 40s-Idle-Tests mitlaufen lassen, um den tatsaechlichen Byte-Schwall zu erfassen.

## Update 5.84: TEMPORAER - Factory-Reset-Diagnose (Testmenue 8->4) fuer Alt-FW-Vergleichstest

**Anlass:** Anwender wollte pruefen, ob der 40s-Idle-Haenger auch mit einer sehr alten
Firmware (5.50, letzter Stand vor Beginn der BLE-CYSPP-Untersuchung) auftritt - dazu wurde
5.50 in einem separaten Worktree-Ordner (`../branch-550`) ausgecheckt und gebaut. Verbindungs-
versuch scheiterte aber: das Modul behaelt seine EZ-Serial-Konfiguration (Name, Advertising,
Pairing etc.) dauerhaft im eigenen Speicher, unabhaengig von der geflashten MCU-Firmware - nach
allen Aenderungen von 5.51 bis 5.83 passt der Modul-Zustand nicht mehr zu dem, was 5.50 beim
Start erwartet (RS232 lief normal, aber am Modul/BT tat sich nichts).

**Fix/Ergaenzung (btio.c/btio.h):** Neue TEMPORAERE Diagnosefunktion `bt_factory_reset()`
(Testmenue 8->4), sendet `/RFAC` (system_factory_reset) - setzt das Modul auf Werkszustand
zurueck (auch Baud -> 115200). Nach Power-Cycle sollte JEDE Firmware-Version (auch alte) das
Modul ueber die ohnehin vorhandene Werksmodul-Erkennung (F=00) wieder frisch und passend
konfigurieren.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.84).

**Wieder entfernen** sobald der Alt-FW-Vergleichstest abgeschlossen ist (gleiche Kategorie wie
die anderen temporaeren Testmenue-Diagnosen, siehe CLAUDE.md-Aufraeum-Punkt).

## Update 5.85: Reboot-Verify nach Werksreset schlug fehl - Diagnose erweitert (F11018/I11019)

**Beobachtung (direkt nach dem 5.84-Factory-Reset-Test):** Die automatische Werksmodul-
Reprovisionierung (Baud, Name, Sniff aus, Advertising, CYSPP, Security) lief komplett
fehlerfrei durch bis Schritt 4 (`/RBT` - Reboot, um die neuen Einstellungen zu uebernehmen).
Danach antwortete das Modul auf kein einziges `/PING` mehr -> `F11009`. Das ist vermutlich das
erste Mal, dass wir das seit Langem vermutete "Modul faellt komplett aus"-Verhalten (CLAUDE.md)
direkt beim Auftreten erwischt haben, statt nur die Nachwirkungen zu sehen.

**Problem bei der Analyse:** `bt_reboot()` (btio.c) wartete bisher bis zu 6s auf irgendeine
Boot-Ausgabe (brach frueher ab, sobald etwas ankam), wartete pauschal weitere 1,5s und verwarf
danach den kompletten Empfangspuffer ungelesen (`bxi=rxi`) - was das Modul in dieser Zeit
tatsaechlich sagte, war nirgends sichtbar. Dieselbe Bugklasse (Modul-Ausgabe wird still
verworfen statt angezeigt) wie an mehreren anderen Stellen dieser Session.

**Fix/Ergaenzung (btio.c):**
1. `bt_reboot()` liest jetzt durchgehend ~8s lang mit und gibt alles mit Zeitstempel am
   Terminal aus (TEMPORAER, wie Event-Sniff), statt es zu verwerfen - beim naechsten
   Reboot-Verify-Fehlschlag sehen wir, was das Modul (falls ueberhaupt etwas) sendet.
2. Neuer, spezifischerer Fehlercode **F11018** "Reboot-Verifikation fehlgeschlagen" ersetzt an
   dieser Stelle das generische `DTC_BT_UART_INIT` (F11009) - landet wie gewohnt sowohl am
   Terminal als auch im persistenten Protokoll (`dctext()`/`protocol()`), auch wenn RS232 nicht
   live mitgelesen wird.
3. Neuer Info-Code **I11019** "Reboot-Verifikation nach Retries ok" wird geloggt (mit Anzahl der
   benoetigten Versuche als Wert), wenn die Verifikation zwar Wiederholungen brauchte, aber am
   Ende erfolgreich war - damit sind auch "knapp geschafft"-Faelle im Protokoll sichtbar, nicht
   nur harte Fehlschlaege.

**Geaenderte Dateien:** btio.c, dtc_codes.h, Doku/Diagnose-Codes.csv, dtc_text.h (generiert via
`tools/gen_dtc.py`), sprachen/sictxt.c (-> 5.85).

**Am Geraet zu verifizieren:** Werksreset (Testmenue 8->4) + Power-Cycle wiederholen, bis der
Reboot-Verify-Fehlschlag erneut auftritt (falls reproduzierbar) - diesmal sollte am Terminal
sichtbar sein, was das Modul nach `/RBT` tatsaechlich sendet (Boot-Banner? nichts? Fehler-Event?),
und im Protokoll sollte F11018 mit Zeitstempel auftauchen.

## Update 5.86: KORREKTUR - kein Power-Cycle zwischen Factory-Reset und Alt-FW-Flash

**Anwender-Einwand (berechtigt):** Die 5.84/5.85-Anleitung riet nach `/RFAC` zu einem Power-Cycle
VOR dem Flashen der Zielfirmware. Das ist falsch: solange danach noch die AKTUELLE Firmware
laeuft, wuerde deren eigene Werksmodul-Erkennung (`init_bluetooth()`) das frisch zurueckgesetzte
Modul beim naechsten Boot sofort wieder erkennen und automatisch mit den AKTUELLEN Einstellungen
neu konfigurieren - der Factory-Reset waere hinfaellig, bevor die gewuenschte alte Firmware
ueberhaupt zum Einsatz kommt.

**Fix (btio.c, `bt_factory_reset()`):** Meldung korrigiert - direkt nach `/RFAC` soll OHNE
Zwischen-Power-Cycle sofort die Zielfirmware geflasht werden (der Flash-Vorgang bootet direkt in
die neue Firmware, kein Zwischenlauf der aktuellen).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.86).

**Nachtrag (2026-08-18) - Ursache des Reboot-Verify-Fehlschlags (F11009 bei 5.84/5.50) GEKLAERT,
5.85-Fix bestaetigt:** Mit der neuen Rohausgabe (5.85) zeigt sich am Geraet:
`/RBT` wird vom Modul bei ~1s bestaetigt (`@R,...,/RBT,0000`), der eigentliche `BOOT`-Event
(`@E,...,BOOT,...,F=EZ-Serial-VELA_IF820_INT V1.4.12.12...`) kommt aber erst bei ~3s, gefolgt
von einem `ASC`-Event kurz danach. Die ALTE `bt_reboot()`-Schleife brach ihre Wartezeit ab,
sobald IRGENDEIN Byte ankam - und das war nur die `/RBT`-Bestaetigung selbst (~1s), nicht der
echte Boot. Nach nur ~2,5s (1s Fruehausstieg + pauschale 1,5s) ging es weiter zur Verify-
Schleife, obwohl der Boot erst bei ~3s abgeschlossen ist - je nach genauem Timing reichte das
Restbudget (~2,8s) mal knapp, mal nicht (genau das beobachtete unzuverlaessige Verhalten bei
5.84 und 5.50). **Kein Modul-Hardware-Problem, sondern ein Timing-/Abbruchbedingungs-Bug in
unserer eigenen Wartelogik** - durch den 5.85-Fix (kein Fruehausstieg mehr, durchgehend ~8s)
behoben. Am Geraet bestaetigt: Verifikation lief danach zuverlaessig durch (`I11019`, 1 Retry).

## Update: 40s-Idle-Haenger AUFGELOEST - Board 269996 ist defekt (Hardware, kein Firmware-Bug)

**Test-Historie:** Der 40s-Idle-dann-Trennen-Haenger wurde ausschliesslich auf **Board 269996**
reproduziert. Systematischer Vergleich auf **Board 123456**:
- 5.50 (Alt-Firmware) + 5.50-eigene Konfiguration (nach Factory-Reset): 5x sauber, kein Haenger.
- 5.50 (Alt-Firmware) + AKTUELLE Modul-Konfiguration (5.86 konfiguriert, dann 5.50 direkt
  zurueckgeflasht, ohne Zwischen-Reboot): 3x sauber, kein Haenger.
- 5.86 (aktuelle Firmware) + aktuelle Konfiguration: 3x sauber, kein Haenger.

Auf Board 123456 also unter KEINER Firmware/Konfig-Kombination reproduzierbar. Damit war die
geplante Versions-Bisektion (5.51-5.86) hinfaellig, bevor sie begonnen wurde.

**Aufloesung (Anwenderbefund):** Board 269996 ist **hardwaredefekt und nicht reparabel ohne
Bauteil-Nacharbeit** (Neuloeten erforderlich). Der 40s-Idle-Haenger war also die ganze Zeit ein
**Hardware-Defekt auf diesem einen Board** - kein allgemeiner, reproduzierbarer Firmware-Bug.
Das erklaert rueckblickend auch die widerspruechlich wirkenden Beobachtungen der letzten Stunden
(scheinbar wechselnde Reproduzierbarkeit, CYSPP bit7 dauerhaft LOW ueber 2 Minuten ohne jede
Pegelaenderung, Windows-Pipe-Fehler) - passt zu einem echten Hardware-Defekt, nicht zu einem
Timing-/Software-Problem.

**Die im Rahmen dieser Untersuchung gefundenen und behobenen Firmware-Bugs bleiben trotzdem
gueltig und wertvoll** (unabhaengig vom 269996-Defekt gefunden, auf Board 123456 nachvollzogen):
PIN-Endlosschleife (5.79), Event-Sniff verschluckte PIN (5.80, ueberholt), nicht-blockierender
Sniffer-Umbau (5.81), Selbstnavigation beim Trennen (5.82), UART-Puffer-Recovery nach Ueberlauf
(5.83), Reboot-Verify-Timing-Bug (5.85). Diese haben nur nicht den 40s-Haenger selbst verursacht
oder behoben - der sitzt auf defekter Hardware.

**Noch offen (auf Board 123456, unabhaengig vom 269996-Defekt):** Beim Trennen kommen weiterhin
vereinzelt echte Bytes (Wert '0'/0x30) ueber UART1 an, die als Menue-Zifferneingabe interpretiert
werden und kurzzeitig eine Selbstnavigation/Neuanzeige des Hauptmenues verursachen (kein Schaden,
nur optisch stoerend). Herkunft nicht abschliessend geklaert (vermutlich fortlaufende Modul-
Events/Advertising-Nachlauf nach dem Trennen, kein Beweis per Rohmitschnitt). Anwender hat einen
Mitigationsvorschlag (kurzes "Settle-Fenster" nach Trennung, eingehende Bytes kurz ignorieren)
ABGELEHNT (Menue soll nicht kurzzeitig unresponsive werden). Bleibt vorerst unbehoben, geringe
Prioritaet (rein kosmetisch).

## Rueckkehr zur urspruenglichen BLE-Frage: PC-Test (ble_mainboard_test.py) gegen 5.86/5.87

**Smoke-Test (Board 123456, 5.86):** Subscribe (CCCD-Write) in 24-28ms quittiert, Verbindung
hielt die vollen 8s inkl. Datenempfang (Boot-Banner+PIN-Prompt kommt transparent ueber CYSPP
an) - **der urspruengliche BLE-CYSPP-Bug (Subscribe -> sofortiger Disconnect) ist damit als
behoben bestaetigt.** Gefundener Nebeneffekt: das Test-Skript rief bisher vor dem Trennen
`client.stop_notify()` auf (CCCD explizit auf 0x0000 zurueckschreiben) - das loeste zuverlaessig
einen Disconnect aus (~5s Verzoegerung). Kein Firmware-Bug, sondern ein Test-Skript-Artefakt -
`stop_notify()`-Aufruf entfernt (ein echter Client wuerde beim gewollten Trennen ohnehin nicht
extra abbestellen wollen).

**PIN-Handhabung:** `ble_mainboard_test.py` um `--pin`/automatische Ableitung aus dem
Geraetenamen (`VIASIS_<serno>` -> erste 2 + letzte 4 Ziffern) erweitert - sendet die BT-PIN nach
dem Subscribe ueber den CYSPP-Kanal, damit `bt_pininit` nicht offen bleibt (das blockierte sonst
auch RS232-Navigation zum Testmenue).

## Update 5.87: FIX derselbe Puffer-Ueberlauf-Bug (5.83) jetzt auch in bt_transfer_echo()

**Symptom (Transfer-Test, PIN korrekt gesendet):** Erste 1-2 Frames wurden bestaetigt, danach
~20s komplette Stille, dann scheiterten auch alle 3 Recovery-Versuche (Fenster/Rate reduziert)
vollstaendig - "Frame 5 ueberschritt 3 Retries nach 3 Recoveries", Disconnect.

**Ursache:** `bt_transfer_echo()` (btio.c) hat exakt dieselbe Schwachstelle, die in 5.83 fuer
`measure.c`'s Hauptschleife behoben wurde, aber NICHT fuer diese Funktion: Bei vollem `rbuf`
deaktiviert `UART1_IRQHandler()` (hard.c) den Empfangsinterrupt (`rbuf_full=1`) und nirgends in
dieser Funktion wird das zurueckgesetzt. Bei der Test-Zielrate 8000 B/s (= 800 B pro 100ms-
Polling-Intervall, weit ueber den ~255 nutzbaren Bytes von `rbuf`) lief das zuverlaessig hinein -
erklaert, warum nach den ersten 1-2 ACKs GAR NICHTS mehr ankam, auch nicht bei per Recovery
reduzierter Rate (der Empfang war schon dauerhaft abgeschaltet, nicht nur langsam).

**Fix (btio.c):** Gleiche Wiederfreigabe wie in 5.83 ergaenzt:
`if (rbuf_full && bxi==rxi) { rbuf_full=0; NVIC_EnableIRQ(UART1_IRQn); }` nach jedem
Schleifendurchlauf. Die innere Drain-Schleife von `bt_transfer_echo()` liest ohnehin schon ALLES
Verfuegbare pro Durchlauf (kein "1 Byte/Durchlauf"-Problem wie bei `mainmenu()`), es fehlte nur
die Wiederfreigabe selbst.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.87).

**Am Geraet zu verifizieren:** Transfer-Test mit PIN-Vorabsendung wiederholen - sollte jetzt auch
nach einem kurzzeitigen Puffer-Ueberlauf weiterlaufen statt komplett stehenzubleiben. Falls der
Durchsatz bei voller Zielrate (8000 B/s) weiterhin instabil ist (Puffer laeuft schneller voll als
100ms-Polling ihn leeren kann), waere eine niedrigere `--rate` oder ein kuerzeres Polling-
Intervall der naechste Schritt - das ist aber ein Performance-/Tuning-Thema, kein Korrektheits-Bug
mehr.

## ✅ BLE-CYSPP-Bug ENDGUELTIG BEWIESEN BEHOBEN (Board 269993, Event-Sniff + BLE gleichzeitig)

**Zwischenzeitliches Ratsel:** Auf Board 123456 kamen nach mehreren erfolgreichen Tests
irgendwann gar keine BLE-Notify-Daten mehr an, auch scheinbare Schreibzugriffe (PIN, 'H') zeigten
keinerlei Reaktion mehr auf RS232 - trotz Bluetooth-Neustart, Geraet-Neustart und komplettem
Entkoppeln/Neukoppeln in Windows. Vermutung: ein Windows-seitiger BLE-Stack-/Pairing-Zustand fuer
genau dieses Geraet, verursacht durch sehr viele Reconnects waehrend der Tests (das Entkoppeln
hat zusaetzlich auch die Classic-BT-Kopplung mit zerstoert - COM-Port liess sich danach nicht
mehr oeffnen, auch nicht ueber viagraph).

**Endgueltiger Beweis auf einem zweiten, "sauberen" Board (269993):** Event-Sniff (Testmenue
8->1, nicht-blockierend seit 5.81) am RS232-Terminal gestartet, waehrend gleichzeitig per
`ble_mainboard_test.py --mode smoke` per BLE verbunden wurde. Der RS232-Mitschnitt zeigt exakt
die von der DVK-Anforderung (`ble-Dvk-request.md`) geforderte Ereigniskette:

```
@E,001A,W,C=01,H=0012,T=00,D=0100   <- CCCD-Write (Notify aktiviert) vom Modul gemeldet
@E,000C,.CYSPP,S=01                  <- CYSPP-Bridge wird AKTIV
PIN = 269993                         <- die per BLE-Write gesendete PIN kommt tatsaechlich an!
...
@E,000C,.CYSPP,S=00                  <- Bridge wird beim Trennen wieder inaktiv
```

Gleichzeitig zeigte das Python-Skript: Banner + PIN-Prompt kommen sauber per BLE-Notify an,
Verbindung haelt die vollen 8s, kein Absturz. **Damit ist bidirektionaler Datenfluss (nicht nur
"Subscribe bricht nicht mehr sofort ab") ueber BLE nachgewiesen - der urspruengliche BLE-CYSPP-
Bug (Verbindung bricht beim Subscribe ab, Gegenstand der gesamten CYSPP-Untersuchung dieser
Session) ist behoben.**

**Einordnung Board 123456:** Sehr wahrscheinlich ein Windows-/PC-seitiges Problem (kaputter
BLE-Pairing-/GATT-Zustand nach exzessivem Test-Reconnect-Verkehr), kein Firmware-Bug - auf einem
Board ohne diese Test-Vorgeschichte funktioniert der komplette Ablauf auf Anhieb.

**Lehre fuer kuenftige Tests:** Nicht zu viele schnelle BLE-Reconnects hintereinander vom
gleichen PC aus, und Windows-Geraetekopplung nicht leichtfertig entfernen (zerstoert ggf. auch
die Classic-BT-Kopplung mit).

## Update 5.88: FIX gleicher Puffer-Ueberlauf-Bug jetzt auch in bt_transfer_echo()

**Symptom (nach erfolgreichem Transfer-Test mit 178 ok/41 fehlerhaft):** Direkt nach dem
manuellen Navigieren zu Testmenue 8->1 (Event-Sniff) liefen Hauptmenue-Anzeigen von selbst
durch (Selbstnavigation `0`, `7`), gefolgt von einer automatisch erscheinenden Zeit-/Datums-
Zeile (`17:00:35;20.08.26;4`, Format aus `date_time()`, rtc.c). Ausserdem funktionierte `H`
(Reset ins Hauptmenue) danach nicht mehr zuverlaessig.

**Ursache:** Der 40kb-BLE-Transfertest sendet Nutzdaten mit ALLEN Byte-Werten 0-255 zyklisch
(`(offset+i) % 256`). Blieb nach Ende des Transfer-Tests bzw. waehrend einer noch aktiven
BLE-Verbindung Rohdatenmuell im gemeinsamen Ringpuffer stehen, wurde der beim naechsten
`mainmenu()`-Durchlauf als Tastatureingabe interpretiert - zufaellige Byte-Werte trafen dabei
auch `'0'`/`'7'` (Selbstnavigation) UND `'T'` (loest `date_time()` aus, unabhaengig vom
Menuezustand, siehe `mainmenu()`s `if (c=='T') {...}` VOR der normalen Menue-Logik). `date_time()`
kann bei aktivem viaspeedcam-Modus `online=1` setzen - und der gesamte normale Menue-Dispatch
(inkl. der `H`-Behandlung) steht hinter `if (!online && !simulation)`, was `H` dann unwirksam
machen kann.

**Fix (btio.c, `bt_diag_menu()`):** Nach dem Dispatch an Event-Sniff/Contention-Test/Transfer-
Test/Factory-Reset wird der Rest im Ringpuffer verworfen (`bxi=rxi;`), BEVOR `mainmenu()` ihn
normal weiterverarbeitet. Bei Event-Sniff (nicht-blockierend, hat zu diesem Zeitpunkt bereits
seinen eigenen Leseindex `sniff_i=rxi` gesetzt) werden dadurch nur die VOR dem Start bereits
vorhandenen Altbytes verworfen - echte Nutzereingaben danach bleiben unangetastet.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.88).

**Am Geraet zu verifizieren:** Transfer-Test laufen lassen, danach Testmenue 8->1 (Event-Sniff)
oder eine andere BT-Diagnose-Option starten - keine automatische Selbstnavigation mehr, `H`
funktioniert weiterhin normal.

## Update 5.89: WURZEL-FIX - UART1-Protokoll/Status strukturell von Menue-Eingabe getrennt

**Anlass:** 5.88 (und davor 5.79/5.82) haben die Selbstnavigations-Bugklasse jeweils nur an
EINEM festen Zeitpunkt geflickt (Puffer leeren beim Trennen bzw. beim Verlassen von
BT-Diagnose). Direkt danach zeigte sich erneut ein Fall, bei dem ein einfaches Trennen (ganz
ohne Testmenue-Bezug) wieder Selbstnavigation + einen `H`-Ausfall ausloeste - das Modul schickt
Protokoll-/Statusereignisse (`@E,...DIS...`, `@E,...ASC...`, `@E,...CYSPP,S=00`) teils ueber
mehrere Sekunden verteilt, ein einmaliges Leeren zu einem festen Zeitpunkt kann das grundsaetzlich
nicht zuverlaessig abfangen. Anwender hat zurecht auf einer strukturellen Loesung bestanden statt
weiterer Einzelfall-Flicken.

**Design:** Der IC54/CYSPP-Pin-Status (`connect&BT_LINK`, bereits ueber `communication_change()`
gepflegt) sagt bereits zuverlaessig, ob die transparente Datenbruecke gerade aktiv ist oder
nicht - eine einzelne Protokollzeile muss dafuer nicht inhaltlich verstanden werden. Der
UART1-Interrupt (`UART1_IRQHandler()`, hard.c) route jetzt an der Quelle in zwei getrennte
Puffer, nur fuer IF820 (`fp.btmodem==IF820`; Laird/RN4678 unveraendert, da dort BT_LINK ein
reiner Status-Ausgang ist, kein bidirektionaler CYSPP-Pin):

- **`rbuf`/`bxi`/`rxi`** (bestehend, unveraendert): RS232 (UART0) **plus** UART1, wenn die
  CYSPP-Bruecke aktiv ist (`(connect&BT_LINK) && !bt_cyspp_out`) - also echte Transparent-Daten
  (Tastatureingabe ueber Classic/BLE, oder die CRC/ACK-Frames des Transfer-Tests). Genau das
  liest `mainmenu()` weiterhin unveraendert.
- **`rbuf1`/`bxi1`/`rxi1`** (neu, gleiches Muster wie `rbuf`): UART1, wenn die Bruecke NICHT
  aktiv ist - Command-Mode: Boot-Meldungen, `/PING`-Antworten, Connect-/Disconnect-Events. Landet
  NIE mehr bei `mainmenu()`, unabhaengig vom Timing.

`bt_cyspp_out` (bereits vorhandenes Flag) deckt den Sonderfall ab, dass `bt_disconnect()` bewusst
Command-Mode erzwingt (`bt_cmdmode()`), obwohl `connect&BT_LINK` zu dem Zeitpunkt noch nicht
zurueckgesetzt ist - die `/DIS`-Antwort landet dank dieser Bedingung trotzdem in `rbuf1`.

**Wer liest was:**
- Bleiben auf `rbuf`/`bxi`/`rxi` (unveraendert): `mainmenu()`; `bt_transfer_echo()` (echte
  Transparent-Nutzdaten); `bt_diag_menu()`s eigene Ziffernabfrage per `getc()`.
- Umgezogen auf `rbuf1`/`bxi1`/`rxi1`: `bt_command()` (dafuer neue Variante `wait_message1()` in
  sio.c - `wait_message()` selbst bleibt unveraendert, wird auch von GSM/MQTT/Matrix-Controller-
  Code genutzt), `bt_get_flowcontrol()`/`bt_get_cod()`/`bt_get_echo()`, `bt_show_name()`,
  `bt_reboot()`s Boot-Wartschleife, `bt_scan_ping()`, `bt_query_show()`, sowie `bt_event_sniff()`s
  Leseindex `sniff_i` (verfolgt jetzt `rxi1` - die `@E,...`-Ereignisse, die Event-Sniff zeigen
  soll, treten per Definition nur im Command-Mode auf). Der Laird-BT730-Fallback-Pfad in
  `init_bluetooth()` bleibt komplett unangetastet (nicht IF820, siehe oben).
- `bt_diag_menu()`s bisheriger 5.88-Flush (`bxi=rxi` vor Rueckkehr) ist jetzt ueberfluessig und
  wurde entfernt - der Wurzel-Fix wirkt an der Quelle, unabhaengig davon, wie lange ein
  Diagnose-Ausflug dauert.
- Ueberlauf-Recovery fuer `rbuf1` (eigenes `rbuf1_full`-Flag) analog zum 5.83/5.87-Fix ergaenzt
  (measure.c, neben der bestehenden `rbuf_full`-Pruefung).

**Geaenderte Dateien:** hard.c, hard.h (neue Puffer/Indizes, ISR-Weiche), sio.c, sio.h (neue
`wait_message1()`), btio.c (Konsumenten umgezogen, siehe oben), measure.c (rbuf1-Ueberlauf-
Recovery), sprachen/sictxt.c (-> 5.89).

**Am Geraet zu verifizieren (beide Boards, 123456 und 269993):**
- Boot/Init unveraendert: normaler Start, `init_bluetooth()` erkennt/konfiguriert wie bisher.
- Der eigentliche Bug weg: BT verbinden (Classic und BLE), PIN eingeben, trennen - RS232 bleibt
  danach unveraendert stehen, keine Selbstnavigation mehr, `H` funktioniert weiterhin.
- Event-Sniff unveraendert nuetzlich: zeigt weiterhin die rohen `@E,...`-Events wie bisher.
- Transfer-Test unveraendert funktionsfaehig: CRC/ACK-Frames werden weiterhin korrekt
  empfangen/bestaetigt.
- Nach einem Transfer-Test direkt zu Testmenue 8->1 oder normale Navigation - keine automatische
  Selbstnavigation/`date_time()`-Ausloesung mehr.
- GSM/MQTT unbeeinflusst (MUX_GSM-Pfad in der ISR wurde nicht angefasst).

## Update 5.90: FIX Bedingung in 5.89 - fp.btmodem==IF820 erst NACH Erkennung gesetzt

**Symptom (Anwender-Log direkt nach 5.89):** Waehrend der Werksinit-Assistent ("Bluetooth
(j/n)?y") das Modul konfigurierte, erschien wirres Bild aus erkennbaren Text-Fragmenten
("/PING", "Bluetooth Laird: 460800", "AT&F1", "$$$") und rohen Binaerbytes untereinander
gemischt - sah nach kaputtem/inkonsistentem Zustand aus.

**Ursache (eigener Bug in 5.89):** Die ISR-Weiche (hard.c) hat auf `fp.btmodem==IF820` geprueft,
um Laird/RN4678 unangetastet zu lassen. Problem: `fp.btmodem` ist waehrend der ALLERERSTEN
Erkennung (bt_scan_ping() usw., ganz am Anfang von init_bluetooth()) noch `0` (unbekannt) - IF820
wird als Wert erst NACH erfolgreicher Erkennung gesetzt. Die migrierten Konsumenten
(bt_scan_ping(), bt_query_show(), bt_command() waehrend der Erkennung) lasen also bereits aus
rbuf1, waehrend die ISR die eingehenden Bytes zu diesem Zeitpunkt (fp.btmodem noch 0, nicht
IF820) weiterhin nach rbuf schrieb - Erkennung/Konfiguration lief dadurch komplett an den
tatsaechlichen Modul-Antworten vorbei, mit den beobachteten Folgeerscheinungen.

**Fix (hard.c):** Bedingung von `fp.btmodem==IF820` auf `fp.btmodem!=Laird &&
fp.btmodem!=Roving` geaendert - deckt sowohl den Erkennungszustand (0, unbekannt) als auch
bestaetigtes IF820 (82) korrekt ab. Nur bei bestaetigtem Laird/RN4678 bleibt alles beim alten
Verhalten (rbuf), wie eigentlich beabsichtigt.

**Geaenderte Dateien:** hard.c, sprachen/sictxt.c (-> 5.90).

**Wichtig fuer den Anwender:** Die im Log sichtbare wirre Ausgabe war hoechstwahrscheinlich
dieser Software-Bug (Erkennung/Konfiguration lesen am falschen Puffer vorbei), keine dauerhafte
Hardware-Beschaedigung des Moduls - vergleichbar mit den bereits mehrfach beobachteten
"Modul reagiert nicht mehr"-Faellen dieser Session, die durchgehend per Power-Cycle behoben
werden konnten. Am Geraet zu verifizieren: 5.90 flashen, Power-Cycle, Werksinit/Bluetooth-Setup
erneut durchlaufen - sollte jetzt sauber ablaufen wie vor 5.89.

## Update 5.91: FIX Puffer-Reste aus Transfer-Test/BLE-Verkehr loesten Selbstnavigation aus

**Symptom (Anwender-Log, nach 5.90):** Beim Auslesen von Messdaten ueber Bluetooth
(Hauptmenue -> 1 Datenausgabe -> XMODEM) wurde das RS232-Menue mit Datenmuell "vollgeballert"
und stuerzte ab - trat vor der 5.89/5.90-Umstellung nicht auf.

**Ursache (Luecke im 5.89-Umbau, nicht der 5.90-Bug selbst):** Der neue Command-Mode-Puffer
`rbuf1` (fuer UART1-Protokolltext, siehe 5.89) wird nur dann geleert, wenn entweder
`bt_command()` gerade eine Antwort erwartet ODER Event-Sniff aktiv laeuft. Im normalen
Betrieb (laengerer BT-Datentransfer, kein Sniff aktiv) liest **niemand** `rbuf1` - er kann
volllaufen und ueber `rbuf1_full`/`NVIC_DisableIRQ` den UART1-Empfangsinterrupt dauerhaft
abschalten (die vorhandene Wiederfreigabe in `measure.c` wartet auf vollstaendiges Leerlesen,
das aber ohne aktiven Verbraucher nie eintritt) - das killt dann auch den eigentlich ueber
`rbuf` laufenden XMODEM-Datenaustausch, weil derselbe physische UART1-Interrupt betroffen ist.

**Fix (btio.c, `bt_sniff_poll()`):** `rbuf1` wird jetzt IMMER geleert (jeder Hauptschleifen-
durchlauf, unabhaengig davon ob Event-Sniff aktiv ist) - nur die Anzeige mit Zeitstempel bleibt
an `sniff_active` gebunden. Damit kann `rbuf1` im Normalbetrieb praktisch nie mehr ueberlaufen.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.91).

**Am Geraet zu verifizieren:** Laengeren Datentransfer ueber BT (Datenausgabe/XMODEM oder der
40kb-BLE-Transfer-Test) durchlaufen lassen - RS232 bleibt danach normal bedienbar, kein
Datenmuell/Absturz mehr.

## Update 5.92: FIX 5.91-Ergaenzung leerte den FALSCHEN Index - Modul wirkte "komplett tot"

**Symptom (Anwender-Log, nach 5.91):** Nach 3-5 aufeinanderfolgenden BT-Classic-Verbindungs-
zyklen (verbinden, PIN eingeben, trennen, wiederholen) blieb irgendwann die PIN-Abfrage leer
(keine Sterne mehr), und ein anschliessender Werksinit-Bluetooth-Schritt fand das Modul auf
KEINER Baudrate mehr (`/PING` scheiterte komplett, alle 6 getesteten Baudraten). Sah aus wie
der bereits dokumentierte "Modul faellt komplett aus"-Hardwarefall - trat aber jetzt mehrfach
reproduzierbar nach wenigen Verbindungszyklen auf, nicht mehr zufaellig.

**Ursache (eigener Bug, Fortsetzung von 5.89/5.91):** Die 5.91-Ergaenzung liess `bt_sniff_poll()`
zwar staendig einen Index (`sniff_i`) auf dem Laufenden halten - aber das ist NICHT derselbe
Index, an dem die Ueberlauf-/Wiederfreigabe-Logik (`rbuf1_full`/`NVIC_DisableIRQ`, siehe
`UART1_IRQHandler()` und die Wiederfreigabe in `measure.c`) haengt. Diese haengt an `bxi1` - dem
Index, den `bt_command()` benutzt. `sniff_i` und `bxi1` liefen also komplett unabhaengig
nebeneinander her: `sniff_i` wurde brav aktuell gehalten, `bxi1` aber nur, wenn gerade explizit
ein `bt_command()`-Aufruf lief. Zwischen zwei BT-Verbindungszyklen (kein Kommando unterwegs,
Event-Sniff meist auch nicht aktiv) blieb `bxi1` also liegen, waehrend `rxi1` weiter durch
Boot-/Connect-/Disconnect-Ereignisse anwuchs - der Puffer lief aus `bxi1`s Sicht nach ein paar
Zyklen voll, schaltete den UART1-Empfangsinterrupt dauerhaft ab: **das Modul war nicht wirklich
tot, unsere eigene Firmware hatte sich selbst taub gemacht.**

**Fix (btio.c):** `sniff_i` als eigenstaendiger Index komplett entfernt. Event-Sniff/
`bt_sniff_poll()` benutzt jetzt denselben Index (`bxi1`) wie `bt_command()` - nur EIN
Verbraucher-Index fuer `rbuf1`, konsistent mit der Ueberlauflogik. Kein Konflikt zwischen beiden
Nutzern, da sie nie gleichzeitig laufen (einzelner Ausfuehrungsthread).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.92).

**Wichtig fuer die Einordnung:** Die vorherige Einschaetzung, das beobachtete "Modul-Aussetzen"
sei ein allgemeines, nicht mit den heutigen Puffer-Aenderungen zusammenhaengendes Hardware-/
Modul-Thema (siehe CLAUDE.md-Nachtrag vom 20.08.), war zu voreilig - zumindest ein erheblicher
Teil der reproduzierten Faelle heute war tatsaechlich dieser eigene Bug, nicht das Modul selbst.
Der aeltere, seit laengerem dokumentierte "Modul faellt gelegentlich aus"-Punkt (vor dem
CYSPP-Investigations-Zeitraum) bleibt als eigenstaendiges, moeglicherweise verwandtes Thema
bestehen, ist damit aber nicht abschliessend geklaert.

**Am Geraet zu verifizieren:** Mehrfach (5-10x) hintereinander BT verbinden/trennen (Classic UND
BLE) - kein Ausfall/keine "Modul tot"-Situation mehr, unabhaengig davon ob Event-Sniff dabei
aktiv war oder nicht.

## Update 5.93: Code-Review der 5.89-5.92-Umstellung - vier weitere Puffer-Weichen-Luecken behoben

Nach Abschluss von 5.92 wurde der gesamte 5.89-5.92-Diff mit dem `code-review`-Skill geprueft
(mehrere unabhaengige Review-Agenten + Verifikationspass). Ergebnis: 2 bestaetigte, 1
plausibler und 1 architektureller (zurueckgestellter) Befund. Die ersten drei sind hier behoben,
konsequent nach der in [[feedback_shared_state_review]] festgehaltenen Lehre aus 5.89-5.92:
vor dem Bauen JEDEN Verbraucher der betroffenen Variable durchgehen, nicht nur die eine gerade
bearbeitete Stelle.

**Befund 1 (bestaetigt) - Rennen in `bt_disconnect()` (btio.c):** `bt_release()` setzt
`bt_cyspp_out` zurueck auf 0, aber `connect&BT_LINK` bleibt bis zum Aufrufer (measure.c, NACH
`bt_disconnect()`) gesetzt. In diesem Fenster liest die ISR-Weiche (`UART1_IRQHandler()`,
hard.c) "Bruecke aktiv" (`connect&BT_LINK` gesetzt, `bt_cyspp_out` schon 0) und haette
Nachlauf-Bytes (Rest der `/DIS`-Antwort, spaete `@E,...`-Disconnect-Events) faelschlich nach
`rbuf` statt `rbuf1` geroutet - dort haette `mainmenu()` sie wieder als Eingabe
fehlinterpretiert (dieselbe Selbstnavigations-Bugklasse wie vor 5.89, nur ueber einen neuen Weg).
**Fix:** `bt_disconnect()` verwirft jetzt zusaetzlich `rbuf` (`bxi=rxi`, analog zum bereits
vorhandenen `bxi1=rxi1`), bevor `connect` wiederhergestellt wird - gerechtfertigt, weil eine
bewusste, sofortige Trennung ohnehin macht, dass in diesem Moment weder echte Tastatureingaben
noch echte Protokollantworten relevant sind.

**Befund 2/3 (bestaetigt) - Laird/RN4678-Erkennungspfad nicht konsistent mit der ISR-Weiche:**
`bt_command()` benutzte seit 5.89 unbedingt `rbuf1`/`bxi1`/`wait_message1()` - die ISR routet
UART1 aber nur dann nach `rbuf1`, wenn `fp.btmodem!=Laird && fp.btmodem!=Roving`. Solange
`fp.btmodem` noch 0 ist (gesamte Erkennungsphase, auch fuer Laird/RN4678) passt das noch
zusammen - sobald ein Laird- oder RN4678-Modul aber erkannt UND `fp.btmodem` dauerhaft gesetzt
ist, haetten alle spaeteren `bt_command()`-Aufrufe (z. B. `set_bt_name()`) am falschen, nie
befuellten Puffer gewartet und waeren immer per Timeout gescheitert. Zusaetzlich benutzte die
RN4678-Boot-Wartschleife in `init_bluetooth()` sowie mehrere manuelle Flush-Stellen im
Laird/RN4678-Erkennungsblock weiterhin das alte `wait_message()`/`bxi=rxi` (rbuf), obwohl
`fp.btmodem` dort noch 0 ist und die ISR bereits nach `rbuf1` routet - ein Rest der 5.89-
Migration, der in diesem (bisher nie am echten Laird/RN4678-Modul getesteten) Codepfad
uebersehen wurde. **Fix:** `bt_command()` waehlt den Puffer jetzt anhand von `fp.btmodem` -
`rbuf`/`wait_message()` nur noch fuer bereits bestaetigtes Laird/Roving, sonst (inkl. `0`/
unbekannt, also waehrend der gesamten Erkennung) `rbuf1`/`wait_message1()`, exakt wie die ISR.
Alle manuellen Flushes (`bxi=rxi`) und der direkte `wait_message()`-Aufruf im Laird/RN4678-
Erkennungsblock (btio.c, `init_bluetooth()`) wurden auf `bxi1`/`rxi1`/`wait_message1()`
umgestellt, da `fp.btmodem` dort durchgehend `0` ist, bis die Erkennung erfolgreich abschliesst.

**Befund 4 (plausibel) - Wiederfreigabe fuer `rbuf1` haette bei aktivem GSM/MQTT-Thread
blockiert bleiben koennen (measure.c):** Die `rbuf1_full`/`NVIC_EnableIRQ`-Wiederfreigabe (5.91)
stand innerhalb von `if (!osActiveThread) {...}` - das Draining von `rbuf1` ueber `bt_sniff_poll()`
(seit 5.91/5.92 unbedingt, siehe Update 5.91) laeuft aber ausserhalb dieser Bedingung. Waere
`rbuf1` waehrend eines aktiven GSM-/MQTT-Verbindungsthreads (`osActiveThread` gesetzt)
uebergelaufen, haette `bxi1` durch `bt_sniff_poll()` trotzdem `rxi1` erreicht - aber die
Wiederfreigabe-Pruefung waere nie gelaufen, der UART1-Interrupt waere dauerhaft aus geblieben
(derselbe Fehlerklasse wie 5.92, nur unter einer anderen Randbedingung ausgeloest). **Fix:** die
`rbuf1_full`-Wiederfreigabe steht jetzt direkt nach dem unbedingten `bt_sniff_poll()`-Aufruf,
also ebenfalls unbedingt - konsistent mit dem Draining, das sie ueberhaupt erst noetig macht.

**Bewusst NICHT behoben (zurueckgestellt, kein Teil dieses Updates):** der architektonische
Befund, dass die gesamte Weiche letztlich von `connect&BT_LINK` abhaengt - einem Flag, das
andernorts bereits als potenziell veraltet dokumentiert ist ("Split-Brain", siehe CLAUDE.md).
Befund 1 behebt einen konkreten, bekannten Fall dieser Klasse (bewusstes Trennen), aendert aber
nichts an der grundsaetzlichen Abhaengigkeit. Als bekanntes Risiko dokumentiert, keine akute
Aenderung noetig.

**Geaenderte Dateien:** btio.c, measure.c, sprachen/sictxt.c (-> 5.93).

**Am Geraet zu verifizieren:** normales Verbinden/Trennen (Classic + BLE) weiterhin ohne
Selbstnavigation; Laird/RN4678-Pfad ist auf diesem Board nicht real testbar (kein solches Modul
verbaut) - Aenderung ist eine reine Konsistenzkorrektur anhand der ISR-Logik, kein Verhaltens-
wechsel fuer den aktuell verwendeten IF820.

## Update 5.94: FIX F11018 auf brandneuem Board - Reboot-Verify-Budget nach Werksinit verdoppelt

**Symptom (Anwenderlog, erstes Flashen von 5.93 auf ein NEUES, unkonfiguriertes Board):**
`init_bluetooth()` erkannte das IF820 korrekt als Werksmodul ("Detection: baud scan without CTS
(factory module)"), konfigurierte es vollstaendig neu (Name, Class-of-Device, Advertising,
CYSPP-Paketierung, Just-Works-Security - ALLE Schritte 3/3b/3c/3d/3e/3f liefen, da beim
allerersten Init naturgemaess nichts schon passt), sendete `/RBT` - und danach antwortete
`/PING` trotz 4 Versuchen (mit 400 ms Wartezeit dazwischen, siehe unten) nicht mehr:
`F11018 Reboot-Verifikation fehlgeschlagen`.

**Ursache:** kein Software-Fehler in 5.93 (die dortigen Aenderungen betreffen ausschliesslich
Puffer-Routing, nicht diese Stelle) - vermutlich schlicht ein zu knappes Zeitbudget fuer DIESEN
Sonderfall. Die Retry-Schleife in `init_bluetooth()` (btio.c, Schritt 4 nach `/RBT`) ist fuer den
Normalfall ausgelegt (nur wenige Einstellungen geaendert, kurzer Reboot) - bei einem
brandneuen Modul, das beim allerersten Reboot ALLE genannten Einstellungen zum ersten Mal
dauerhaft ins Modul-Flash schreibt/uebernimmt, kann der eigentliche Neustart laenger dauern als
die bisherigen ca. 4 x (300 ms Ping-Timeout + 400 ms Pause) ≈ 2,8 s Gesamtbudget.

**Fix (btio.c):** Retry-Anzahl in der Post-`/RBT`-Verifikationsschleife von 4 auf 8 verdoppelt
(Zeilenkommentar bei der Schleife) - verdoppelt das Gesamtzeitbudget auf ca. 5,6 s, ohne die
Abfragefrequenz (400 ms) selbst zu aendern. Reine Wartezeit-Anpassung, keine Logikaenderung.

**Noch offen:** ob ein einziges Mal (dieses eine neue Board) reicht, um die Ursache sicher als
"knappes Zeitbudget" statt eines anderen, selteneren Problems zu bestaetigen, ist mit einer
einzigen Beobachtung nicht endgueltig zu sagen - falls `F11018` auf einem NEUEN Board mit dieser
Aenderung erneut auftritt, ist das ein Hinweis auf eine andere/zusaetzliche Ursache statt reiner
Zeitknappheit.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.94).

**Am Geraet zu verifizieren:** ein weiteres neues/werksfrisches Board erstmalig flashen und
booten lassen - Werksinit sollte jetzt auch bei einem besonders langen ersten Reboot ohne
`F11018` durchlaufen.

## Update: Modul nicht beim Pairing sichtbar - per Hardware-Massnahme behoben (2026-08-26)

**Symptom (dasselbe neue Board wie oben, nach erfolgreichem 5.94-Boot ohne F11018):** Modul
liess sich weiterhin ganz normal per RS232/UART konfigurieren und ansprechen (Werksinit,
`/PING`, `/QFV` usw. liefen einwandfrei) - erschien aber beim Bluetooth-Pairing (Classic wie
BLE) auf keinem Endgeraet. Da UART-Kommandobetrieb komplett unabhaengig vom HF-/Antennenpfad
ist, deutete das von Anfang an auf ein Problem NACH der digitalen Konfiguration (Advertising-
Ebene, Rollen-Pin oder HF-Pfad), nicht auf einen Software-/Konfigurationsfehler.

**Untersuchte Kandidaten (Diagnose per Schaltplan, ohne Platinenzugriff von hier aus):**
- `CP_ROLE` (LOW=Central, HIGH=Peripheral, laut `IF820_EZSerial_Konzept_Mapping.md` am Mainboard
  *"nicht verdrahtet"*, kein bestaetigter interner Pull-up wie bei `LP_MODE`) - potenziell
  board-abhaengig schwankender Logikpegel, der das Modul faelschlich in die Central-Rolle
  (Scannen statt Advertising) haette kippen lassen koennen. Ueber einen vom Anwender geteilten
  Schaltplanausschnitt geprueft; die zunaechst vermutete Pin-Zuordnung (`P32`) stellte sich als
  reine manuelle Markierung des Anwenders heraus, nicht als tatsaechliche Schaltungsbesonderheit
  - die Pin-Zuordnung von `CP_ROLE` auf dieser Modulvariante (`453-00171R`) bleibt offen, da die
  entsprechende Datenblatt-Pin-Funktionstabelle hier nicht renderbar war (kein PDF-Tooling in
  dieser Session).
- `VDDIO`/`3P0V`-Entkopplung (`C128`/`C58`, siehe HW_Massnahmen_Bewertung_2026-08.md) - als
  moeglicher Kandidat besprochen, aber nicht der schnellste/naheliegendste erste Check (RF-Pfad-
  Instabilitaet unter TX-Bursten waere die Wirkkette).

**Fix:** vom Anwender eigenstaendig durch eine Hardware-Massnahme direkt am Board behoben - die
genaue Massnahme wurde in dieser Session nicht im Detail mitgeteilt (nur "mit Hardware-
Aenderungen geloest, Modul laeuft jetzt"). Sollte spaeter dokumentiert werden, welche
Aenderung es konkret war (Pull-up auf `CP_ROLE`? Entkopplung ergaenzt? Antennen-/Loetstelle
korrigiert?), damit diese Board-Serie kuenftig direkt richtig bestueckt werden kann.

## Update: Umfassender End-to-End-Testlauf nach dem HW-Fix (2026-08-26) - Modul vollstaendig bestaetigt funktionsfaehig

Nach der Hardware-Korrektur (siehe oben) hat der Anwender einen erweiterten, automatisierten
Testlauf (`Doku/tools/ble_mainboard_test.py`, Modus `module-suite` - die Skriptdatei wurde
zwischen den Sessions erheblich ausgebaut: native Windows-Classic-BT-Inquiry per `ctypes`/
`BluetoothAPIs.dll`, PIN-Prompt-bewusster Smoke-Test, HTML/JSON-Suite-Report - Erweiterung liegt
noch uncommitted im Arbeitsverzeichnis) auf dem reparierten Board gefahren:

| Phase | Transport | Ergebnis | Dauer | Details |
| --- | --- | --- | ---: | --- |
| Classic Smoke | Classic | passed | 51.9 s | |
| Classic Verbindung (12 Zyklen, Haltezeiten 0-300 s) | Classic | passed | 1016.1 s | 12/12 Zyklen, PIN/Menue jedes Mal ok |
| BLE Smoke | BLE | passed | 11.7 s | |
| BLE Verbindung (12 Zyklen, Haltezeiten 0-300 s) | BLE | passed | 1063.6 s | 12/12 Zyklen, PIN/Menue jedes Mal ok |
| Flash schreiben (419430 Test-Messwerte) | - | passed | 163.1 s | |
| Classic Download 1/3 | Classic | passed | 603.6 s | 4.199.424 Byte / 4101 Bloecke, 7024 B/s, 17 Retries |
| Classic Download 2/3 | Classic | **failed** | 8.3 s | "PIN-Prompt nicht gesehen" - einmaliger Ausreisser, siehe unten |
| Classic Download 3/3 | Classic | passed | 578.4 s | 4.199.424 Byte / 4101 Bloecke, 7289 B/s, 8 Retries |
| BLE Download 1/3 | BLE | passed | 638.1 s | 4.199.424 Byte / 4101 Bloecke, 6639 B/s, 0 Retries |
| BLE Download 2/3 | BLE | passed | 687.2 s | 4.199.424 Byte / 4101 Bloecke, 6184 B/s, 0 Retries |
| BLE Download 3/3 | BLE | passed | 649.2 s | 4.199.424 Byte / 4101 Bloecke, 6537 B/s, 0 Retries |

**Gesamtergebnis der Suite:** `failed` (wegen des einen Ausreissers Classic Download 2/3), aber
10 von 11 Phasen liefen einwandfrei durch - inklusive drei vollstaendiger, echter Ymodem-
Downloads von je 4,2 MB je Transport (nicht nur der kleine 40-kB-Diagnose-Transfer-Test).

**Einordnung des einen Fehlschlags:** "Classic Download 2/3" brach nach nur 8,3 s mit "PIN-Prompt
nicht gesehen" ab - direkt zwischen zwei erfolgreichen Classic-Downloads (1/3 und 3/3 liefen
beide einwandfrei durch). Sieht nach einem einmaligen COM-Port-/Windows-SPP-Reconnect-Timing-
Problem aus (aehnlich den bereits dokumentierten Windows-Classic-Eigenheiten, siehe "Split-Brain"-
Absatz weiter oben), nicht nach einem reproduzierbaren Firmware- oder Navigationsfehler - wird
aber nicht ohne Wiederholung als endgueltig geklaert betrachtet.

**Bedeutung fuer 5.89-5.94:**
- Bestaetigt die gesamte dual-Puffer-UART1-Architektur (5.89-5.93) und das verdoppelte Reboot-
  Verify-Budget (5.94) als stabil unter echtem, wiederholtem Dauerbetrieb: 24 Connect/Disconnect-
  Zyklen (12x Classic + 12x BLE) mit Haltezeiten bis 5 Minuten, ohne einen einzigen Fall von
  Selbstnavigation, PIN-Endlosschleife oder haengengebliebenem UART1-Interrupt.
- Bestaetigt erstmals den ECHTEN Produktions-Downloadpfad (`send_vtf_file()`/Ymodem, nicht nur
  den Diagnose-Transfer-Test) Ende-zu-Ende ueber beide Transporte, inklusive einer sehr grossen
  Datenmenge (419.430 Messwerte, 4,2 MB) - die im ersten Entwurf des Download-Testmodus (siehe
  vorheriger Eintrag "ble_mainboard_test.py um BT-Classic + Cycle/Download-Modi erweitert")
  ausdrueckliche Unsicherheit ("nicht an echter Hardware verifiziert") ist damit ausgeraeumt.
- Durchsatz-Größenordnung: Classic ~7000-7300 B/s (mit vereinzelten Retries, wie unter realem
  Bluetooth-Verkehr zu erwarten), BLE ~6200-6640 B/s (durchgehend 0 Retries) - beide deutlich
  hoeher als der fruehere kleine 40-kB-Referenztransfer, da hier eine echte, große Datei
  uebertragen wurde.

**Noch offen:** die genaue Hardware-Massnahme, die das Pairing-Problem behoben hat, ist noch
nicht dokumentiert (siehe Update direkt oberhalb); der eine Classic-Download-Ausreisser sollte
bei Gelegenheit wiederholt werden, um "einmaliger Reconnect-Glitch" von "seltener aber echter
Bug" zu unterscheiden.

## Untersuchung YMODEM-Abbruch: kein Flashzeigerfehler belegt (2026-08-27)

**Korrektur eines frueheren Verdachts:** Der lange Download ist im aktuellen Modul-Suite-Lauf
erwartet. Die vor dem YMODEM-Header ausgegebene Firmwarediagnose lautet
`md_start_page=5137 md_page=5028 md_adr=500 anzmw=419430`; der Zaehlwert entspricht damit exakt
den 419430 erzeugten Test-Messwerten. Die vom Python-Empfaenger verworfenen Bytes sind dieser
Diagnosetext, nicht Flashdaten und kein YMODEM-Protokollfehler.

**Tatsaechlicher Abbruch:** Der BLE-Download lief anschliessend mit stabilen rund 6750 B/s bis
zu 2620 Bloecken beziehungsweise 2 682 880 Byte. Danach kam ein unvollstaendiger Block und die
BLE-Verbindung war bereits geschlossen (`Not connected`), sodass weder NAK noch CAN an das
Geraet gesendet werden konnten. Die folgenden BLE-Verbindungsversuche fanden die Adresse nicht
mehr. Das belegt einen Verbindungsabbruch waehrend des Transfers, aber keinen Fehler in der
Flashbereichsberechnung oder der YMODEM-Exportschleife.

**Empfaengerkorrektur:** Fuehrende Nicht-YMODEM-Bytes werden im Python-Testtool seitdem bis zum
naechsten `SOH`/`STX` ohne NAK und ohne Verbrauch des Block-Retrybudgets verworfen. Echte
Blockfehler behalten das bisherige NAK-/Retryverhalten; bei einem lokalen Abbruch sendet das Tool
`CAN CAN CAN`, sofern die Verbindung noch besteht.

**Noch zu untersuchen:** Ursache des BLE-Disconnects nach etwa sieben Minuten unter laufendem
Transfer. Die Firmware empfängt pro Block ein ACK; `UART1_IRQHandler()` setzt dabei `bt_time`
zurueck. Der 6-Minuten-Inaktivitaetstimeout erklaert den Abbruch daher nicht unmittelbar.

## Testlauf 2026-08-26: BLE-Datenkanal per Testsuite belegt + Antwort an die App-Seite

**Kein Versionssprung** - reiner Test-/Doku-Eintrag.

**Testlauf:** `ble_mainboard_test.py --mode module-suite`, Board `26SC9993`, Firmware 5.93.
Report: `Doku/tools/Testreports/mainboard-module-suite-2026-08-26_11-57-14.json` (+ `.html`).

| Phase | Transport | Ergebnis | Messwerte |
|---|---|---|---|
| Classic Smoke | classic | passed | Connect 47,2 s (COM-Port-Aufbau), Banner + PIN + Menue ok |
| Classic Verbindung | classic | passed | 12/12 Zyklen, Haltezeiten 0...300 s, Connect 45-117 ms |
| BLE Smoke | ble | passed | Connect 1383 ms, GATT 2 ms, **Subscribe 39 ms**, PIN-Prompt + Menue ok, 8 s gehalten ohne Disconnect |
| BLE Verbindung | ble | passed | **12/12 Zyklen**, Haltezeiten 0...**300 s**, Connect 3,1-6,1 s, Disconnect 126-133 ms |
| Flash schreiben | ble | passed | 419430 Messwerte, 163 s |
| Classic Download 1/3 | classic | passed | 4 199 424 B, 598 s, **7024 B/s**, 17 Retries |
| Classic Download 2/3 | classic | **failed** | Abbruch nach 8,3 s: "PIN-Prompt nicht gesehen" - siehe unten |
| Classic Download 3/3 | classic | passed | 4 199 424 B, 576 s, **7289 B/s**, 8 Retries |
| BLE Download 1-3 | ble | passed | je 4 199 424 B, 632/679/642 s, **6639/6184/6537 B/s**, **Retries 0/0/0** |

**Kernaussage:** Der BLE-CYSPP-Datenkanal ist damit nicht nur "einmal beobachtet funktionierend"
(Update 5.87 ff.), sondern ueber 12 Verbindungszyklen und 3 x 4,2 MB Nutzdaten belegt - ueber BLE
sogar fehlerfreier als ueber Classic SPP (0 statt 8-17 Ymodem-Retries). Der von der App-Seite in
`ble-retest-1-negative.md` gemeldete Fehler ("subscribe ...ca102 ok -> sofort Disconnect") ist
damit abschliessend widerlegt.

**Der einzige Fehlschlag (`Classic Download 2/3`) ist Classic-only und noch offen:** der Versuch
startete 9 s nach dem Ende eines 10-minuetigen 4,2-MB-Downloads und sah keinen PIN-Prompt; der
Wiederholungsversuch 9 s spaeter lief vollstaendig durch. Verdacht: Wiederverbinden zu kurz nach
einer langen Session (auf Classic-Seite zusaetzlich der Windows-COM-Port-Neuaufbau). Nicht
weiter untersucht - als Punkt notiert, nicht als Firmware-Bug bestaetigt.

**Nebenbefund im Testskript (kein Firmware-Problem):** In der BLE-Smoke-Phase steht
`unexpected_disconnect: true`, obwohl `disconnect_reason: hold_time_elapsed_still_connected` und
`result: passed` gemeldet werden. Ursache: der Bleak-`disconnected_callback` feuert auch bei
unserem eigenen, gewollten `client.disconnect()` im `finally`-Block, also NACH dem Setzen von
`result`. Reines Reporting-Artefakt in `ble_mainboard_test.py`; das Feld ist in dieser Phase
nicht aussagekraeftig. Sollte irgendwann bereinigt werden (Flag erst nach dem gewollten Trennen
ignorieren), aendert aber nichts am Ergebnis.

**Neues Dokument: `Doku/ble-app-integration-freigabe.md`** - Antwort/Freigabe an die App-Seite.
Enthaelt: (a) den Nachweis anhand der Zahlen oben, (b) die Richtigstellung zu deren Frage 4
(der CYSPP-Pin muss hochohmig sein, nicht aktiv LOW - ein gemessenes HIGH beweist nichts),
(c) eine vollstaendige Integrationsliste mit acht Punkten, die die App einhalten muss:
nur `...ca102` (notify + writeWithoutResponse), erst nach `PIN = ` senden (Puffer-Flush),
PIN = 2 erste + 4 letzte Ziffern der Seriennummer, nur Ziffern ohne CR/LF, Textkodierung
ISO-8859-1 statt UTF-8, Notify-Reassemblierung (20-Byte-Paketierung), Bursts unter 250 B und
Fluss ueber Geraeteantworten takten, keine Binaerdaten (globale Hotkeys), Connect-Timeout >= 15 s,
Supervision-Timeout >= 5 s selbst anfordern (Firmware setzt PPCP nicht), kein `stop_notify()`
vor dem Trennen, Reconnect-Pause, 6-Minuten-Inaktivitaets-Trennung, stille Antwortpausen bei
GSM/GPS-UART-Uebernahme. Zusaetzlich die Ymodem-Parameter fuer den Datenexport.

## Update 5.95: Diagnose + Sicherheitsnetz fuer "leere Flashseiten"-Export (Root Cause weiter offen)

**Ausgangspunkt:** siehe "Offener Firmwarefehler: Messdatenexport kann leere Flashseiten bis zum
Ringpufferende senden (2026-08-27)" weiter oben. Board 269992 reproduzierte den Fehler mehrfach
(3x Loeschen+Schreiben+Export, davon 1x mit Power-Cycle), Board 269990 nicht.

**Root-Cause-Jagd (chronologisch, alle Theorien am Ende WIDERLEGT bzw. nicht abschliessend
bestaetigt):**
1. *Fehlende Batterie auf 269992* (269990 hat eine, 269992 hatte urspruenglich keine) - passt
   zur `md_pg_adr`/GPREG4-Mechanik aus dem `vario245`-Incident (`flash.c` Zeile ~423: bei
   fehlgeschlagenem Kennung-Read wird `fp.md_start_page` aus der battery-backed GPREG-Kopie
   rekonstruiert - ohne Batterie ueberlebt das keinen Stromausfall). **Widerlegt:** Batterie
   nachgeruestet, Fehler blieb identisch reproduzierbar (3x Loeschen+Schreiben+Export, 1x mit
   Power-Cycle).
2. *Reiner Code-Bug unabhaengig vom Zustand* (Header-Laenge `anzwerte` vs. tatsaechliche
   Schleifenbegrenzung `fp.md_start_page`/`fp.md_page` laufen auseinander) - passte zur
   deterministischen Reproduktion aus (1). **Nicht bestaetigt:** `sicom.c`/`xmodem.c`/`flash.c`/
   `libtool.c` (inkl. `send_vtf_file()`, `messdaten()`, `md_simulation()`, CRC-Routinen) sind seit
   Firmware 5.55 (Commit `caab3d2`) funktional UNVERAENDERT - der grosse `libtool.c`-Diff bestand
   nachweislich nur aus Umlaut-Encoding-Rauschen in Kommentaren, keine Logikaenderung. Ein reiner
   Code-Bug haette dann auch auf 5.55 auftreten muessen.
3. **Tatsaechlicher Befund:** Zurueckflashen auf 5.55 behob den Fehler auf 269992 - ABER erneutes
   Zurueckflashen auf 5.94 behob ihn ebenfalls. Der Fehler ist also weder an eine Firmware-Version
   noch an eine deterministische Code-Logik gebunden, sondern an einen **persistenten Zustand**
   (vermutlich `fp.md_start_page`/`md_pg_adr` bzw. der Parameterblock-Kennung-Abgleich in
   `InitParameter()`), der durch den Flash-Vorgang selbst (Neustart mit vermutlicher Kennung-/
   Versions-Neuinitialisierung) zuruckgesetzt wird - unabhaengig davon, welche Firmware-Version
   geflasht wird. **Die eigentliche Ursache, WARUM `fp.md_start_page` waehrend des normalen
   Betriebs (ohne Neuflashen) von `fp.md_page` abweichen kann, ist damit weiterhin nicht
   gefunden** - jeder bisherige Reproduktionsversuch wurde durch das anschliessende Neuflashen
   (zum Testen einer Theorie) versehentlich "repariert", bevor die Zeigerwerte abgelesen werden
   konnten.

**Was diese Version aendert (zwei unabhaengige, bewusst kleine Aenderungen in `send_vtf_file()`/
`messdaten()`, `sicom.c`):**

1. **Diagnose-Ausgabe vor jedem Export:** `md_start_page`, `md_page`, `md_adr` und `anzmw` werden
   jetzt direkt vor dem Start eines Ymodem-Exports auf das Terminal geschrieben. Reine Anzeige,
   keine Verhaltensaenderung. Zweck: sollte der Fehler erneut auftreten, OHNE dass zwischendurch
   neu geflasht wird, zeigt diese Zeile die tatsaechlichen (mutmasslich auseinanderlaufenden)
   Zeigerwerte - die bisher fehlende Evidenz fuer die eigentliche Root Cause.
2. **Sicherheits-Obergrenze in der Export-Schleife:** Die Seitenschleife in `send_vtf_file()` lief
   bisher ausschliesslich, bis `seite==fp.md_page` erreicht wird - bei einem falschen/veralteten
   `fp.md_start_page` konnte das den gesamten Ringpuffer umlaufen und dabei geloeschte (`0xFF`)
   Flashseiten senden. Jetzt bricht die Schleife zusaetzlich hart ab, sobald mehr als
   `anzwerte/MD_PER_PG + 2` Seiten durchlaufen wurden (Sicherheitsmarge fuer Rundungseffekte),
   unabhaengig davon, ob `seite` jemals `fp.md_page` erreicht. Der Export schliesst dann trotzdem
   sauber mit `ymodem_close()` ab, statt bis zu einem Empfaenger-Timeout/-Abbruch (`F13010`)
   weiterzulaufen.

**Bewusst NICHT geaendert:** die eigentliche Ursache der Zeiger-Divergenz wird mit dieser Version
nicht behoben, weil sie noch nicht bekannt ist - beide Aenderungen sind Diagnose- bzw.
Schutzmassnahmen, kein Fix der Wurzelursache.

**Geaenderte Dateien:** sicom.c, sprachen/sictxt.c (-> 5.95).

**Naechster Schritt, sobald der Fehler wieder auftritt:** VOR jedem weiteren Neuflashen die
Diagnosezeile (`md_start_page=... md_page=... md_adr=... anzmw=...`) direkt vor dem fehlerhaften
Export ablesen und hier dokumentieren - das ist die fehlende Evidenz, um die eigentliche
Root Cause endlich einzugrenzen.

## Update 5.96: BEWEIS gefunden (Board 269990) - md_start_page ausserhalb des Flash-Bereichs + Bounds-Check

**Board 269990 (Serial 26SC9992), Firmware 5.94** zeigte dasselbe Symptom ("Scheisse beim
Messdaten ausgeben"). Diesmal gelang der Beweis OHNE Neuflashen, ueber bereits vorhandene,
rein lesende Diagnosewege in 5.94 (kein Risiko, den Fehler durch die Pruefung selbst zu
verdecken):

- Hauptmenue -> 1: `Anzahl Messwerte = 3507` - unauffaellig, plausibel.
- Funkmodems -> 2 -> Ctrl-G -> 5 (`get_mqtt_ind()`, alle Felder nur mit Enter durchgeklickt,
  NICHTS veraendert): `MD_start: 30913` - reproduzierbar identisch bei zweimaliger Abfrage.
- Testmenue -> 2 (Flash Speicher, Abfrage NICHT bestaetigt - waere Datenloeschung!): Ausgabe
  "4 MB Flash Speicher installiert".

**Der Beweis:** Fuer einen 4-MB-Flash-Chip gilt laut `flash.c` (`InitSPIFlash()`, Case `0x27`):
`maxpage=8191` - also sind NUR die Seiten 0...8191 real vorhanden. `fp.md_start_page=30913`
liegt fast 4x ausserhalb dieses Bereichs. Das ist keine leichte Abweichung, sondern eindeutig
und beweisbar eine ungueltige, korrupte Zeigerposition.

**Warum das nicht sofort als Absturz auffaellt:** Der vorhandene Ringpuffer-Ueberlauf-Schutz
(`if (seite==PROTOCOLPAGE) seite=MEASUREPAGE;` in `send_vtf_file()`/`put_ascii_data()`) prueft
nur auf EXAKTEN Treffer beim Hochzaehlen - ein Zeiger, der bereits weit VOR dem ersten Zaehl-
schritt ausserhalb liegt, trifft diese Grenze nie exakt und laeuft stattdessen sehr lange durch
ungueltige Seitennummern (in `put_ascii_data()` beobachtet als scheinbares "Einfrieren" - tatsaech-
lich vermutlich kein Hardware-Hang, siehe unten, sondern ein sehr langer, aber endlicher Marsch
durch ungueltige Adressen).

**Nachtrag zum vermuteten "Einfrieren":** Ein Reconnect des RS232-Terminals (ohne Reflash, ohne
Neustart - kein Boot-Banner beim Reconnect sichtbar) stellte die Verbindung wieder her, und
`MD_start` las sich danach identisch (`30913`) aus. Das Geraet lief also vermutlich die ganze
Zeit weiter - das "Einfrieren" war wahrscheinlich ein Terminal-/COM-Port-seitiges Problem
(bereits an anderer Stelle in diesem Dokument als bekannte Windows-COM-Eigenart vermerkt),
kein echter Firmware-Hang. Bleibt als Nebenbefund, nicht abschliessend geklaert.

**Root Cause der Korruption selbst: weiterhin NICHT gefunden.** Wir wissen jetzt zweifelsfrei
DASS der Zeiger korrupt wird und WIE das sich auswirkt - aber nicht WARUM/WODURCH `md_start_page`
(und wahrscheinlich `md_page`) auf einen Wert jenseits des gueltigen Flash-Bereichs gelangt.

**Fix (flash.c, `InitParameter()`):** Nach dem Laden des Parameterblocks (egal ob aus externem
Flash oder aus der LPC-internen Kopie via GPREG-Fallback) wird jetzt geprueft, ob `fp.md_page`,
`fp.md_start_page` oder `fp.md_adr` ausserhalb des durch `maxpage`/`PROTOCOLPAGE` tatsaechlich
gueltigen Bereichs liegen. Falls ja: neuer Diagnose-Code **F16003** ("Messdatenzeiger ausserhalb
des installierten Flash-Bereichs") wird protokolliert, und die Zeiger werden auf einen sicheren
Startwert (`MEASUREPAGE`, leerer Puffer) zurueckgesetzt - inklusive der GPREG-Kopien
(`MQTT_send`, `MQTT_index`, `md_pg_adr`), damit der Reset konsistent bleibt und nicht durch den
naechsten Kennung-Lesefehler wieder rueckgaengig gemacht wird.

Das ist eine Absicherung GEGEN die Auswirkung der Korruption (verhindert, dass ein bereits
kaputter Zeiger je wieder benutzt wird), aber noch nicht der Fix FUER die Korruption selbst -
die Ursache dafuer bleibt offen. Ergaenzt (nicht ersetzt) den in 5.95 eingebauten Sicherheitsnetz
in der Export-Schleife.

**Geaenderte Dateien:** flash.c, dtc_codes.h, dtc_text.h, Doku/Diagnose-Codes.csv,
sprachen/sictxt.c (-> 5.96).

**Am Geraet zu verifizieren:** 5.96 auf Board 269990 flashen (Achtung: der Reflash selbst
setzt den kaputten Zustand vermutlich zurueck, siehe fruehere Beobachtung auf Board 269992 -
das ist also KEIN Beweis, dass der Bounds-Check gegriffen hat). Der eigentliche Test des
Bounds-Checks braucht ein Board, auf dem die Korruption NACH dem Flashen von 5.96 erneut
auftritt (ohne zwischenzeitliches Neuflashen) - dann sollte F16003 im Protokoll erscheinen
und `MD_start`/`Anzahl Messwerte` nach dem naechsten Boot wieder einen sauberen, kleinen
Wert zeigen, statt die Korruption unveraendert durchzureichen.

## Update 5.97: Fix ueberarbeitet (delete_data() statt Handarbeit) + Root-Cause-Hypothese

**Ueberarbeitung des 5.96-Fixes:** Auf Nachfrage geprueft, ob noch andere Zaehler/Zeiger beim
Reset mit beruecksichtigt werden muessen. Ergebnis:
- `MQTT_index` darf NICHT mit zurueckgesetzt werden - das bestehende Reset-Muster im Code
  (`fp.md_page=MEASUREPAGE; delete_data();`, siehe mqtt.c vor einer Firmware-Programmierung)
  laesst `MQTT_index` bewusst unangetastet, da es ein monoton steigender Cloud-Sendezaehler
  zur Deduplizierung ist, kein Ringpuffer-Zeiger. Der 5.96-Fix hatte ihn faelschlich auf 0
  gesetzt - jetzt entfernt, stattdessen `delete_data()` direkt aufgerufen (gleiches, bereits
  etabliertes Reset-Muster wie im Rest des Codes).
- Physische Flash-Seite wird jetzt zusaetzlich explizit geleert (`flash_erase(fp.md_page,1)`)
  direkt im Fix - `InitParameter()` liest unmittelbar danach ungefragt den Seiteninhalt nach
  `vbuf` (Zeile ~464); ohne expliziten Erase haette dort sonst alter Seiteninhalt (von der
  vorherigen Nutzung dieser Seitennummer) geladen werden koennen. Der bestehende Boot-Erase in
  main.c (Zeile ~121) deckt den Normalfall zwar meistens ab, wird aber bei SLEEP_RES/FIRM_UP_RES
  uebersprungen - der Fix ist jetzt eigenstaendig korrekt, unabhaengig vom Reset-Grund.

**Root-Cause-Hypothese (NEU, noch nicht verifiziert):** `flash_wbuf1()` (flash.c) schreibt den
kompletten `fp`-Parameterblock beim Notfall-Speichern (Power-Loss-ISR, `EINT0_IRQHandler`) NICHT
atomar, sondern seitenweise in einer Schleife (`maxbyte`=528 Byte je Seite, `sizeof(fp)` ist
deutlich groesser -> mehrere Erase+Programm-Zyklen von je mehreren ms). Verliert das Geraet
waehrend dieser laufenden Mehrfach-Schreibsequenz tatsaechlich die Spannung (das Notfall-Save
laeuft ja bereits als Reaktion auf einen erkannten Spannungsausfall - ein Wettlauf gegen die
Restenergie), kann das genau die Seite treffen, in der `md_start_page`/`md_page`/`md_adr` liegen -
mit undefiniertem Ergebnis (bei einem unterbrochenen Flash-Erase/Programm-Zyklus sind die
betroffenen Zellen typischerweise weder zuverlaessig alt noch neu, sondern effektiv zufaellig -
passt zu einem Wert wie `30913`, der weder ein sinnvoller alter noch neuer Zeigerstand ist).
Selbe Fehlerklasse wie der vario245-Incident (Zustand haengt davon ab, WIE das Geraet abgeschaltet
wird), aber ein anderer konkreter Mechanismus (Torn Write waehrend Notfall-Save, nicht die
GPREG-Fallback-Divergenz).

**Analyseplan fuer diese Hypothese:** siehe `Doku/Analyseplan_Messdatenzeiger-Korruption.md`.

**Geaenderte Dateien:** flash.c, sprachen/sictxt.c (-> 5.97).

## Update 5.98: FIX Diagnose-Zeile (5.95) stoerte den echten Ymodem-Export - entfernt

**Verifikation am Geraet (Board 269990, 5.97):** Die 5.96/5.97-Zeigerkorrektur wurde live bestaetigt
- nach Boot erschien `F16003 ... 30913` im Protokoll, und `Anzahl Messwerte`/`MD_start` zeigten
danach sauber `0`. Ein anschliessender ECHTER Exportversuch mit sinnvollen Werten
(`md_start_page=0 md_page=23 md_adr=380 anzmw=1234`, Ymodem-Header korrekt `VTF17460` =
`1024+2048+2048+10*1234`) zeigte aber ein NEUES Problem: Viagraph (das echte Produktionswerkzeug,
per RS232/COM3) blieb beim Datenexport dauerhaft bei "0 kB" haengen - keinerlei Fortschritt,
obwohl dieselbe Aktion frueher zuverlaessig funktioniert hat.

**Ursache gefunden (Anwender-Beobachtung):** Die in 5.95 eingebaute Diagnose-Zeile
(`md_start_page=... md_page=... md_adr=... anzmw=...`) wurde bisher IMMER unmittelbar VOR dem
Start des echten Ymodem-Transfers auf die Schnittstelle geschrieben - also genau an der Stelle,
an der ein strenger Ymodem-Client (wie Viagraph) das erste Byte bereits als `SOH` (Blockstart)
erwartet. Die eingestreute Klartextzeile hat den Protokollparser vermutlich verwirrt/blockiert -
ein einfaches Terminal oder das tolerante Python-Testtool haben das ueberlebt bzw. teilweise
kompensiert, ein strikter Produktionsclient offenbar nicht. Damit war die 5.95-Diagnosehilfe
selbst die Ursache des in dieser Sitzung neu beobachteten Viagraph-Haengers - kein weiterer
Firmware-Bug im eigentlichen Exportpfad.

**Fix (sicom.c):** Diagnose-Zeile aus dem `c=='C'`-Zweig in `messdaten()` wieder entfernt.
`send_vtf_file()` startet jetzt wieder unmittelbar, ohne vorgeschalteten Klartext.

**Geaenderte Dateien:** sicom.c, sprachen/sictxt.c (-> 5.98).

**Am Geraet zu verifizieren:** Viagraph-Download auf Board 269990 erneut versuchen - sollte jetzt
wieder normal Fortschritt zeigen statt bei 0 kB zu haengen.

## Update 5.99: FIX zwei weitere ungeschuetzte Feldzugriffe - Korruption ist NICHT auf md_page begrenzt

**Neuer Befund (Board 269990):** Hauptmenue -> 4 (Einstellungen) fuehrte zu endloser Garbage-
Ausgabe (Flut von `ÿ`/0xFF-Zeichen ohne Ende). Ursache: die Menueanzeige selbst liest
`T_brsel[fp.hoff][0]` ([sicom.c:131](sicom.c)) OHNE jede Bereichspruefung - ein korrupter,
zu grosser `fp.hoff`-Wert (Array hat nur 3 Eintraege) liest weit ausserhalb des Arrays und
gibt den dort gefundenen Speicherinhalt so lange aus, bis zufaellig eine Null-Byte gefunden
wird (kann sehr lange dauern -> "endlose" Ausgabe).

**Damit ist bewiesen:** die Korruption auf diesem Board betrifft NICHT nur `md_page`/
`md_start_page`/`md_adr` - mindestens auch `fp.hoff` (Anzeigeoptimierung) und vermutlich
`fp.sprache` (siehe vorheriger Verdacht, noch nicht mit dem defekten Board direkt bestaetigt,
da der unklammerte Lesezugriff selbst schon dieses Menue unbenutzbar machte). Alle drei Felder
liegen im selben Bereich des `FlashSavedParameter`-Structs, nahe beieinander (`md_start_page`/
`md_page`/`md_adr` bei Byte-Offset ~kurz vor `sprache`, `hoff` etwas weiter dahinter) - staerkt
die Hypothese einer REGIONALEN Korruption (torn write ODER Pufferueberlauf aus den davor
liegenden Schaltergruppen-Arrays `swno`/`swexp`/`swport`/`nosw`, noch nicht abschliessend
untersucht) statt eines auf `md_page` beschraenkten Einzelfalls.

**Praktisches Problem:** Der unklammerte `fp.hoff`-Zugriff blockierte den Zugang zum
"Parameter Xmodem up-/download" (`Parameter_transfer()`, erreichbar nur ueber genau dieses
Einstellungen-Menue) - ohne Fix war also nicht einmal mehr moeglich, den rohen Parameterblock
zwecks weiterer Analyse herunterzuladen.

**Fix (sicom.c, sio.c):** dieselbe Klammerung wie bereits in `sprachauswahl()` (sio.c) jetzt
auch an den beiden anderen bekannten ungeschuetzten Stellen ergaenzt:
- `sicom.c` Zeile ~131 (`parametermenue()`): `fp.hoff` auf `0` geklammert, wenn `>2`
  (T_brsel hat 3 Eintraege), bevor `T_brsel[fp.hoff]` gelesen wird.
- `sicom.c` Zeile ~259 (`werkeinstellung()`): `fp.sprache` auf `0` geklammert, wenn
  `>ANZSPRACHEN-1`, bevor `T_spritm[fp.sprache]` gelesen wird.
- `sio.c` `ja()`: dieselbe Klammerung fuer `fp.sprache` vor dem Vergleich mit
  `T_ja[fp.sprache]`/`T_nein[fp.sprache]` ergaenzt (erklaert das fruehere "j funktioniert
  nicht"-Symptom).

**Bewusst NICHT geaendert:** dies sind weiterhin gezielte Einzel-Patches an bekannt
unsicheren Lesestellen, kein systematischer Schutz. Es koennen noch weitere, bisher nicht
gefundene ungeschuetzte Zugriffe auf andere Felder in diesem Struct-Bereich existieren - siehe
Analyseplan (`Doku/Analyseplan_Messdatenzeiger-Korruption.md`) fuer die geplante umfassendere
Loesung (CRC/Bereichspruefung ueber den gesamten Parameterblock).

**Geaenderte Dateien:** sicom.c, sio.c, sprachen/sictxt.c (-> 5.99).

**Am Geraet zu verifizieren:** Hauptmenue -> 4 sollte jetzt ohne Garbage-Flut die
"Einstellungen:"-Liste zeigen; danach `D` druecken und einen Parameterblock-Download
per XMODEM-1K/CRC versuchen (siehe Analyseplan), um die Korruption erstmals direkt als
Hex-Dump zu untersuchen.

## Update (kein Versionssprung): Rohen Parameterblock analysiert - Korruption per Hex-Dump bewiesen

**Board 269990:** Nach dem 5.99-Fix war das Einstellungen-Menue wieder benutzbar. Ueber
`Parameter_transfer()` (Hauptmenue -> 4 -> `D`, XMODEM-1K/CRC-Empfang am PC) wurde der rohe
Parameterblock zweimal heruntergeladen: einmal VOR und einmal NACH einer vollstaendigen
Werksinitialisierung (Testmenue -> Ctrl-W -> 5 -> `y`, `InitParameter(1)`).

**Vorher (Hex-Dump, Byte-Offsets manuell aus `FlashSavedParameter` in `hard.h` berechnet):**
- `md_start_page`/`md_page`/`md_adr` (Offset ~710-715): `49/49/0` - zu diesem Zeitpunkt bereits
  saniert durch den 5.96/5.97-Bounds-Check, also nicht mehr korrupt.
- `dmode` (Offset ~720): `0x10` (16) - unplausibel, Feld ist dokumentiert als 0/1.
- **`fp.sprache` (Offset 721): `0x64` (100)** - eindeutig korrupt (gueltiger Bereich nur 0-2).
  Erster direkter Byte-Beweis, nicht nur aus Symptomen abgeleitet.
- Ab Offset ~1740 ein langer Lauf `0xFF` (unbeschriebener/geloeschter Flash-Zustand) genau im
  Bereich der E-Mail/APN/Server-Konfigurationsfelder.

**Nachher (nach `InitParameter(1)`/`werkparameter()`), an denselben Byte-Offsets:**
- `dmode` = `0x00` - passt exakt zu `fp.dmode=0;` in `werkparameter()`.
- `fp.sprache` = `0x00` - passt exakt zu `fp.sprache=Def_sprache;` (=0, Deutsch).
- `fp.hoff` (Offset ~1738, aus der Struct-Reihenfolge hergeleitet) = `0x01` - passt EXAKT zu
  `fp.hoff=1;` in `werkparameter()` - bestaetigt zugleich, dass die manuell berechnete
  Offset-Kette bis dorthin korrekt war.
- `hell[0..3]` = `100,100,100,100` (einheitlich, passt zu `Def_hell`), `farben`=2 (`Def_farben`).

**Schlussfolgerung:** Die volle Werksinitialisierung hat die Korruption in allen bisher
gefundenen Feldern (`md_page`-Familie, `dmode`, `sprache`, `hoff`) nachweislich beseitigt.
Das stuetzt (ohne es endgueltig zu beweisen, da ein Werksinit jede der bisherigen Hypothesen
gleichermassen "uebertuenchen" wuerde) die neue, in der Diskussion aufgekommene dritte
Hypothese: das `FlashSavedParameter`-Struct ist ueber viele Firmwareversionen gewachsen
(BT/GSM/MQTT/GPS/viaspeedcam-Felder kamen sukzessive dazu) - ein Parameterblock, der von
einer AELTEREN Firmware-Version mit kleinerem/anders sortiertem Struct zuletzt vollstaendig
initialisiert wurde und seither nur per Firmware-Update (nie per vollem Werksinit) weiter-
gepflegt wurde, wuerde genau so ein Bild ergeben: manche Felder korrekt, manche korrupt/
zufaellig, manche schlicht nie beschrieben (`0xFF`).

**Naechster sinnvoller Schritt (Konsens aus der Diskussion):** kein weiteres Einzelfeld-Patchen
mehr, sondern eine generelle Parameterblock-Integritaetspruefung (CRC-16 ueber den gesamten
`fp`-Block, gespeichert und bei jedem Laden verifiziert, Fehlerfall gleichrangig zum
bestehenden `Kennung`/`pKennung`-Mismatch behandelt -> loest automatisch denselben
`werkparameter()`/`default_parameter()`-Neuaufbau aus). Neuer Diagnose-Code vorgeschlagen:
`F16004 Parameterblock-Pruefsummenfehler` (breiter als das bestehende `F16003`, das nur den
Einzelfall "Messdatenzeiger ausserhalb des Bereichs" abdeckt). Noch NICHT umgesetzt - siehe
`Doku/Analyseplan_Messdatenzeiger-Korruption.md` (ueberarbeitet) fuer Umfang und geplantes
Vorgehen; bewusst als groesserer, sorgfaeltig zu planender Schritt zurueckgestellt, nicht als
schnelle Einzelaenderung.

## Update (kein Versionssprung): Speicherplatz-Pruefung fuer A/B-Doppelpufferung des Parameterblocks

**Frage:** Reicht der verfuegbare Flash-Speicherplatz fuer eine A/B-Doppelpufferung (Ping-Pong
mit Sequenznummer) des Parameterblocks, als robusteste Loesung gegen Torn Writes (siehe
Analyseplan, Abschnitt 3)? Ressourcen sind begrenzt, daher vor einer Design-Entscheidung
gegengeprueft.

**Ergebnis: Ja, ohne jede Einschraenkung der Messdaten-Kapazitaet.** Das bestehende Speicher-
layout (`hard.h`) reserviert bereits einen ungenutzten 5-kB-Puffer explizit fuer genau diesen
Zweck:

```c
#define PARAMBLOCKS   2   // Parameterblock = 2x1k = 2048 Byte (entspricht dem gezogenen Hex-Dump)
#define FREEBLOCKS    5   // "5k zwischen Protokoll und Parametern frei ... z. B. fuer Parameter Upload"
#define PROTBLOCKS    2   // Protokoll = 2x1k
```

Speicherlayout (von niedriger zu hoher Adresse): `[Messdaten: 0..PROTOCOLPAGE] [Protokoll: 2k]
[frei/reserviert: 5k] [Parameter: 2k] [Chip-Ende]`. Eine zweite Parameterblock-Kopie braucht nur
2 weitere 1k-Bloecke - passt bequem in die bereits reservierten 5k (3 Bloecke blieben noch frei),
OHNE `PROTOCOLPAGE` zu verschieben - die Messdaten-Ringpuffer-Kapazitaet ist davon vollstaendig
unberuehrt.

**Groessenordnung gegenueber der Gesamtkapazitaet:** selbst auf dem kleinsten unterstuetzten
Flash-Chip (512 kB, `maxpage=2047`) macht Protokoll+frei+Parameter zusammen nur ca. 9 kB aus
(<2 % der Gesamtkapazitaet) - auf dem in den aktuellen Boards verbauten 4-MB-Chip ist es
verschwindend gering. Eine Verdopplung des Parameterblocks ist auf JEDER unterstuetzten
Chip-Variante ohne spuerbaren Kapazitaetsverlust moeglich.

**Konsequenz fuer den Analyseplan:** die A/B-Doppelpufferung (bisher als "aufwendigste Option"
eingestuft) ist ressourcenseitig uneingeschraenkt machbar - der Aufwand liegt ausschliesslich
im Software-Design (Sequenznummer, atomarer "welcher Slot ist aktuell"-Marker, Anpassung aller
drei Schreibstellen), nicht im verfuegbaren Speicherplatz.

## Update 5.101 (Branch `connection-pin`): CONNECTION-Pin verdrahtet + Diagnose-Monitor erweitert

**⚠ WICHTIG - 5.101 ist HARDWARE-ABHAENGIG, kein normaler Mainline-Stand:** Diese Version (und
alle folgenden Versionen auf dem Branch `connection-pin`) setzen die in diesem Eintrag
beschriebene Umverdrahtung voraus (IF820 Pin 33 auf IC54/IC36-Pad IO5, GND-Bahn dafuer
aufgetrennt). Auf einem UNVERAENDERTEN Board (IO5 noch auf GND) liefert `bt_cyspp_monitor()`s
neue `CONNECTION bit5`-Zeile lediglich einen bedeutungslosen, konstanten Wert (Pin liegt weiter
fest auf GND) - kein Fehler, aber keine verwertbare Information. `communication_change()`/
`BT_LINK` sind in 5.101 noch unveraendert (siehe Stufe A/B unten), daher ist 5.101 auf einem
unveraenderten Board funktional identisch zu 5.99 - **nicht** verwechseln mit dem Hauptbranch
`LTE-Update`, der bei 5.99 bleibt, bis dieser Branch (falls erfolgreich) zurueckgemerged wird.

**Anlass:** Die automatisierten Connect/Disconnect-Tests (`ble_mainboard_test.py`) sind
gelegentlich fehlerhaft, ohne bekannten Grund. Verdacht: die Doppelrolle von IC54 Bit7
(CYSPP-Steuerausgang UND einzige Verbindungserkennung) macht die Erkennung angreifbar - siehe
`Doku/IF820_Pin_Referenz.md`, Abschnitt 2, und `HW_Massnahmen_Bewertung_2026-08.md`, Maßnahme 2.
Plan (bereits laenger dokumentiert, bisher zurueckgestellt): den bislang unverdrahteten
`CONNECTION`-Pin (IF820 Pin 33, P27/CONN_IND) als eigenen, dedizierten Statuseingang nutzen und
damit Steuerung (CYSPP) und Statuserkennung entkoppeln.

**Hardware (durch Anwender umgesetzt):** IF820 Pin 33 (`CONNECTION`) auf das bisher fest auf GND
liegende Pad `IO5` (Pad 10) des PCA9554-I2C-Expanders (`IC54`/`IC36`) verdrahtet - die GND-
Verbindung zu diesem Pad wurde dafuer aufgetrennt. Bit 5 des Expander-Eingaberegisters ist damit
frei fuer diesen neuen Zweck; im Code war das bereits vorab bestaetigt (`MQTT_LINK (1<<5)` ist ein
reines Software-Flag, wird nie aus dem echten IC54-Eingaberegister gelesen - kein Konflikt).

**Firmware (Stufe A - reine Diagnose, noch OHNE Aenderung an der Produktionslogik):**
`bt_cyspp_monitor()` (btio.c, Testmenue -> 8 -> ... CYSPP-Monitor) liest jetzt zusaetzlich Bit 5
und zeigt beide Signale unabhaengig voneinander live an, sobald sich eines aendert:
```
CYSPP bit7      = HIGH (idle / not connected) / LOW (connected / data mode)
CONNECTION bit5 = HIGH (idle / not ready)     / LOW (data pipe fully established)
```
`communication_change()`/`BT_LINK`-Erkennung selbst ist UNVERAENDERT - dieser Schritt liefert
nur Beobachtungsdaten, aendert nichts am tatsaechlichen Geraeteverhalten. Bewusste Entscheidung:
zuerst per Diagnose bestaetigen, dass `CONNECTION` sich wie im Datenblatt beschrieben verhaelt
(insb. ob es fuer Classic SPP genauso funktioniert wie fuer BLE - im Datenblatt nicht eindeutig
zugesagt, siehe Pin-Referenz), bevor die eigentliche Erkennungslogik angefasst wird.

**Geplanter naechster Schritt (Stufe B, noch NICHT umgesetzt):** sobald die Diagnose bestaetigt,
dass `CONNECTION` zuverlaessig sowohl bei BLE als auch bei Classic SPP anspringt, `BT_LINK` in
`communication_change()` auf Bit 5 umstellen (oder Bit 5 UND Bit 7 gemeinsam auswerten) und damit
die Doppelrolle von Bit 7 tatsaechlich aufloesen. Das ist die eingentliche Verbesserung fuer die
"manchmal fehlerhaften" Tests - wird aber bewusst erst nach Stufe A angegangen (gemeinsam
genutzter Erkennungscode, gleiche Vorsicht wie bei der 5.89-5.92-Lehre).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.101).

**Am Geraet zu verifizieren:** Testmenue -> 8 -> CYSPP-Monitor waehrend eines BLE- UND eines
Classic-Connect/Disconnect-Zyklus beobachten - zeigt `CONNECTION bit5` in beiden Faellen sauber
LOW waehrend der Datenkanal steht und HIGH sonst? Insbesondere interessant: Verhaelt es sich
identisch zu Bit 7, oder gibt es Faelle, in denen sie auseinanderlaufen (genau das waere die
Erklaerung fuer die "manchmal fehlerhaften" automatisierten Tests).

## Update 5.102 (Branch `connection-pin`): Stufe A ausgewertet (negativ fuer Split-Brain) + Stufe B umgesetzt (Lese-/Schreibrolle entkoppelt)

**Stufe A - Messergebnis (elektrisches Rauschen zuerst behoben):** Die neu verdrahtete
`CONNECTION`-Leitung brauchte zunaechst noch einen Pull-up (Pad war zuvor fest auf GND, jetzt
hochohmig) - ohne Pull-up floatete sie und loeste pro Sekunde hunderte `EINT1`-Interrupts aus
(`comchange_count`-Deltas von 800-1300 in wenigen Sekunden), was ueber `communication_change()`s
I2C-Read bei jedem Interrupt das gesamte Geraet spuerbar verlangsamte. Anwender hat einen 18 kOhm
Pull-up nach VDDIO ergaenzt (nicht die urspruenglich vorgeschlagenen 10 kOhm, elektrisch aber
unproblematisch) - danach durchgehend `Delta=0` im Ruhezustand, Rauschen vollstaendig behoben.

**Mit sauberen (rauschfreien) Daten wurde die eigentliche Kernfrage getestet:** verhaelt sich
`CONNECTION` (Bit5) beim unsauberen/nicht quittierten Trennen anders als `CYSPP` (Bit7)? Dreimal
reproduziert, immer dasselbe Ergebnis:
- Normaler Connect (Classic SPP, PC-HyperTerminal): beide Bits gemeinsam LOW, korrekt.
- Sauberes Trennen (HyperTerminal-Fenster regulaer geschlossen): beide Bits gemeinsam wieder HIGH,
  korrekt.
- **Unsauberes Trennen (Windows-seitiges COM-Port-"Trennen" in HyperTerminal, informiert die
  Bluetooth-Verbindung selbst nicht zuverlaessig - bereits laenger bekannter Effekt, siehe
  Abschnitt "Offene Punkte" in `CLAUDE.md`):** beide Bits bleiben gemeinsam auf LOW haengen,
  `comchange_count` bewegt sich danach gar nicht mehr (Modul bekommt vom Abbruch nichts mit).
  Reproduziert ueber die eigentliche Windows/HyperTerminal-Bluetooth-Verbindung selbst (nicht nur
  ueber RS232-Beobachtung) - **CONNECTION haengt exakt zusammen mit CYSPP fest, keine Divergenz.**

**Ergebnis/Schlussfolgerung:** `CONNECTION` ist **kein unabhaengigeres Signal** als `CYSPP` fuer
den Split-Brain-Fall - das IF820-Modul selbst merkt in diesem Fehlerfall nicht, dass der Link weg
ist, und haelt beide abgeleiteten Statusausgaenge gemeinsam aktiv. Eine Umstellung der
Erkennungslogik auf `CONNECTION` loest das urspruenglich vermutete Robustheitsproblem
("automatisierte Tests manchmal fehlerhaft ohne bekannten Grund") **nicht** - dafuer braeuchte es
einen host-seitigen Watchdog (periodisches `/PING` o.ae.), unabhaengig vom Modul-eigenen Status.
Divergenztest mit absichtlich verzoegerter BLE-GATT-Subscription (laut Datenblatt der einzige
dokumentierte Unterschied zwischen den beiden Pins) steht noch aus - bisher nur Classic SPP
getestet, wo beide Ereignisse ohnehin zeitgleich ausgeloest werden.

**Stufe B - trotzdem umgesetzt, aber mit anderer Begruendung:** Unabhaengig vom Split-Brain-
Ergebnis bleibt ein eigenstaendiger Vorteil: `CYSPP` (Bit7) ist beim IF820 gleichzeitig
Schreibleitung (`bt_cmdmode()`/`bt_release()` schalten sie zur Laufzeit auf Ausgang) UND wurde
bisher als Statusquelle gelesen (`communication_change()`) - diese Doppelrolle war die Grundursache
des urspruenglichen BLE-CYSPP-Bugs und traegt noch das im 5.93-Fix dokumentierte Race-Potenzial
(`bt_release()` vs. Aufrufer). `CONNECTION` (Bit5) wird vom Host nie getrieben, ist also als
alleinige Statusquelle strukturell race-frei. Umgesetzt in `get_com_status()` (i2cm.c):
```c
if (fp.btmodem==IF820)
 state = (state & ~BT_LINK) | (((state>>5)&1)<<7);
```
Nur fuer `fp.btmodem==IF820` (Laird/RN4678 unveraendert, kein CONNECTION-Pin/keine
Umverdrahtung bei diesen Modulen). `bt_cyspp_monitor()` selbst liest weiterhin per
`read_i2cdev()` direkt (unveraendert, umgeht `get_com_status()` bewusst) - liefert also immer die
rohen Hardware-Pegel, unabhaengig von dieser Umstellung.

**⚠ Wichtige Korrektur des 5.101-Hinweises:** Ab 5.102 ist dieser Branch NICHT mehr "auf einem
unveraenderten Board funktional identisch zur Hauptlinie" - `BT_LINK` haengt jetzt fuer den IF820
produktiv von der Umverdrahtung ab. Auf einem unveraenderten Board (Pad IO5 noch auf GND fest)
wuerde `get_com_status()` dauerhaft Bit5=0 lesen -> `BT_LINK` bliebe permanent "verbunden"
haengen, obwohl gar keine Verbindung besteht. **Dieser Branch darf ab 5.102 unter keinen
Umstaenden auf ein Board ohne die CONNECTION-Umverdrahtung geflasht werden** (CLAUDE.md
entsprechend aktualisiert).

**Am Geraet noch zu verifizieren:** BLE-Connect/Disconnect-Zyklen mit 5.102 (bisher nur Classic
SPP getestet), und der oben beschriebene Divergenztest (BLE-Connect ohne sofortige
GATT-Subscription).

**Geaenderte Dateien:** i2cm.c, sprachen/sictxt.c (-> 5.102).

## Update 5.103 (Branch `connection-pin`): Stufe B WIEDER ENTFERNT - reproduzierbare Haenger am Geraet

**Symptom (Live-Test 5.102, mehrfach reproduziert):** Normaler Ablauf (Boot, per BT verbunden,
per BT getrennt) - danach:
1. Erst RS232-Terminal in Zeitlupe (einzelne Zeichen, mehrere Sekunden Verzoegerung).
2. In einem weiteren Durchlauf: RS232-Terminal "beschmiert" (Datenmuell, weil zu viele/falsche
   Daten eingelaufen sind) - passt zur alten, eigentlich schon behobenen Bugklasse "Modul sendet
   noch rohen Event-/Protokolltext ueber UART1, der bei mainmenu() als Tastatureingabe ankommt"
   (siehe 5.82/5.89-Fixes).
3. In einem weiteren Durchlauf: Terminal komplett tot, auch nach Power-Cycle zunaechst nicht
   sicher wiederhergestellt ("nicht rettbar" - Anwenderaussage, Ausgang zum Zeitpunkt dieses
   Eintrags noch nicht abschliessend geklaert).
Ausserdem einmal reproduziert: der Menuepunkt "Bluetooth deinstallieren" (`sicom.c:378`, treibt
CYSPP kurz HIGH per `bt_cmdmode()`, sendet aber KEIN `/DIS`) hat das Geraet haengen lassen -
"Verbindung bleibt aktiv, aber es geht nichts mehr".

**Verdacht (nicht abschliessend bewiesen):** Die bestehenden Puffer-Flush-Fixes 5.82/5.89/5.93
(`bxi=rxi`/`bxi1=rxi1` in `bt_disconnect()`, `communication_change()`) sind exakt auf den
Uebergangszeitpunkt von CYSPP (Bit7) abgestimmt - der Moment, in dem das Modul typischerweise noch
letzte Event-/Protokolldaten ueber UART1 sendet. Wenn CONNECTION (Bit5) zu einem leicht anderen
Zeitpunkt umschaltet als Bit7 (frueher oder spaeter), feuert `communication_change()` (die jetzt
auf Bit5 wartet) moeglicherweise zu einem Zeitpunkt, an dem der Flush nicht mehr zum tatsaechlichen
Datenstrom passt - alte Bugklasse mit neuer Ursache. **Nicht verifiziert** - dafuer haette es einen
Logic-Analyzer-Mitschnitt beider Bits UND des UART1-Datenstroms gebraucht, den wir nicht gemacht
haben, bevor die Symptome auftraten.

**Entscheidung:** Stufe B sofort zurueckgebaut, ohne die genaue Ursache erst zu beweisen - die
Haeufung reproduzierbarer Haenger direkt nach der Umstellung ist Grund genug, nicht auf einer
Vermutung weiterzuarbeiten. `get_com_status()` liest jetzt wieder ausschliesslich das rohe
Register (reines Bit7-Verhalten, wie vor 5.102). Stufe A (`bt_cyspp_monitor()`, reine Diagnose-
Anzeige beider Bits) bleibt unveraendert bestehen - die dort gewonnenen Messergebnisse (Update
5.102, Split-Brain-Befund) bleiben gueltig, nur die Produktivumstellung ist zurueckgenommen.

**Lehre:** Bevor eine "eigentlich aequivalente" Statusquelle produktiv getauscht wird, muss nicht
nur der ENDZUSTAND uebereinstimmen (das hatten wir in Stufe A per Diagnose bestaetigt), sondern
auch das TIMING des Uebergangs - genau die Art Shared-State-Annahme, die vor dem Flashen am Code
zu pruefen gewesen waere, nicht erst am haengenden Geraet.

**Offen:** Ob/wie sich CONNECTION (Bit5) noch sinnvoll nutzen laesst (z. B. nur als zusaetzliche
PLAUSIBILITAETSPRUEFUNG neben Bit7, nicht als alleinige Quelle), ist nicht weiter verfolgt.
Naechster Schritt waere ein Logic-Analyzer-Mitschnitt (Bit7, Bit5, UART1-RX) genau beim Trennen,
um das vermutete Timing-Problem tatsaechlich zu belegen oder zu widerlegen, bevor ein neuer Versuch
unternommen wird.

**Geaenderte Dateien:** i2cm.c, sprachen/sictxt.c (-> 5.103).

## Update 5.104: FIX BT_LINK-Erkennung von Flanken- auf Pegelvergleich umgestellt (neues Board 12SC1234)

**Symptom (neues, unmodifiziertes Board 12SC1234, Firmware 5.99, PC/Viagraph mit bestehender
Windows-Kopplung):** Nach jedem Boot steht `CYSPP bit7` (CYSPP-Monitor) sofort auf `LOW`
("verbunden"), obwohl noch niemand bewusst verbunden hat - `comchange_count` bleibt dabei auf 0.
Viagraph zeigt "verbunden", aber es kommt nie ein PIN-Prompt; `bt_pininit`/`startmessage` werden
nie ausgeloest.

**Root Cause gefunden ([libtool.c:1013](sism555/libtool.c:1013), `communication_change()`):**
Die BT_LINK-Erkennung war ein reiner **Flankenvergleich** (`(comstate_neu^comstate)&BT_LINK`),
mit einer beim Boot einmalig eingelesenen Baseline ([main.c:133](sism555/main.c:133)). Windows
verbindet sich bei bestehender Kopplung so schnell automatisch wieder, dass die Verbindung oft
schon steht, BEVOR diese Baseline gelesen wird - die Baseline "sieht" die Verbindung dann von
Anfang an als gegeben, ein Vergleich kann folglich nie eine Aenderung feststellen, und der ganze
Connect-Ablauf (`Init_BT_ch()`, `startmessage`, `bt_pininit`) wird schlicht nie angestossen. Fuer
`RS232_LINK` existierte fuer genau dieses Problem bereits ein Bit-Invertierungstrick
([main.c:208](sism555/main.c:208)) - fuer `BT_LINK` fehlte das Gegenstueck.

**Erste Loesung (Flanken-Trick analog RS232, verworfen):** Baseline beim Boot gezielt invertieren,
wenn BT schon als verbunden erkannt wird. Funktioniert nur fuer den Boot-Fall, ist aber ein
Einmal-Hack ohne Selbstheilung bei jedem aehnlichen zukuenftigen Timing-Problem. Auf Anwenderwunsch
("gefaellt die Flankengeschichte nicht, geht das nicht robuster mit high und low?") verworfen.

**Umgesetzte, robustere Loesung:** BT_LINK-Erkennung von Flanken- auf **Pegelvergleich** umgestellt
- statt "hat sich das Bit seit dem letzten Aufruf geaendert?" wird jetzt direkt gefragt "stimmt der
aktuelle Hardware-Pegel mit dem ueberein, was die Software (`connect&BT_LINK`) gerade glaubt?":
```c
if ((interfaces&BT_LINK) && !bt_cyspp_out)   // BT registriert, Leitung nicht selbst getrieben
{
 if (((comstate_neu&BT_LINK)==0) && !(connect&BT_LINK))      // HW verbunden, SW weiss es noch nicht
 { ... unveraendert: Init_BT_ch(), startmessage, bt_pininit, connect|=BT_LINK ... }
 else if (((comstate_neu&BT_LINK)!=0) && (connect&BT_LINK))  // HW getrennt, SW denkt noch verbunden
 { ... unveraendert: connect&=~(UART1|BT_LINK), bt_pininit=0, bxi=rxi-Flush ... }
}
```
Selbstheilend bei JEDEM Aufruf (nicht nur beim Boot), unabhaengig davon, ob/wie eine Flanke verpasst
wurde. Die `!bt_cyspp_out`-Bedingung schuetzt bewusst das Zeitfenster, in dem `bt_cmdmode()`/
`bt_disconnect()` die Leitung selbst als Ausgang treiben (dort weicht der Pegel bewusst von der
Software-Wahrheit ab - kein Fehler, keine Aktion noetig) - sonst wuerde der Pegelvergleich
faelschlich mitten in einer bewussten Trennung eingreifen (dieselbe Race-Klasse wie der 5.93-Fix).

**⚠ Neuer, NOCH NICHT geklaerter Befund beim Live-Test (5.104 auf 12SC1234):** Nach dem Flashen
zeigte Viagraph den PIN-Prompt zwar jetzt - aber in einer **Endlosschleife**, "PIN = ******" flutet
das Terminal im Sekundenbruchteil-Takt (Screenshot Anwender). Arbeitshypothese: die BT_LINK-
Erkennung funktioniert jetzt zum ERSTEN MAL ueberhaupt auf diesem Board - falls `CYSPP` (Bit7) auf
diesem konkreten Board elektrisch unruhig ist (schnelles Bouncing HIGH/LOW, z. B. durch die in
`CLAUDE.md` seit laengerem dokumentierte fehlende Entkopplung `C128`/`C58`), wuerde JEDE erkannte
Verbindungsaenderung (alte Flankenlogik genauso wie die neue Pegellogik) `startmessage=1` und
`bt_pininit=1` erneut setzen -> Banner+PIN-Prompt wird bei jedem Bounce neu gesendet. **Nicht
verifiziert** - Anwender wurde gebeten, nach Power-Cycle sofort `comchange_count` im CYSPP-Monitor
zu beobachten (rasanter Eigenanstieg ohne Nutzeraktion wuerde die Bouncing-Theorie bestaetigen).
Bis zur Klaerung: Root-Cause-Fix (Flanke -> Pegel) bleibt wie er ist, da er unabhaengig von der
PIN-Schleife korrekt und sinnvoll ist; die PIN-Schleife selbst ist ein separat zu untersuchender
Befund.

**Geaenderte Dateien:** libtool.c, sprachen/sictxt.c (-> 5.104).

## Update 5.105: FIX PIN-Endlosschleife aufgeklaert - Contention-Test fehlte bt_cyspp_out-Schutz (KEIN Hardwaredefekt)

**Aufklaerung des 5.104-Befunds:** Die "PIN = ******"-Endlosschleife (beobachtet auf 12SC1234
UND, unabhaengig von jedem 5.104-Code, auf 26SC9983 mit unveraendertem 5.99) trat reproduzierbar
NUR auf, wenn vorher/waehrend Testmenue -> 8 (BT-Diagnose) genutzt wurde - speziell Option 2
(Contention-Test). Ohne dieses Diagnose-Menue zu betreten, verbindet sich BT normal und sauber
(vom Anwender bestaetigt). Die zuvor vermutete Hardwaredefekt-Erklaerung fuer 12SC1234 war damit
hoechstwahrscheinlich eine Fehldiagnose - beide Boards liefen vermutlich in dieselbe, rein
software-seitige Falle.

**Root Cause ([btio.c:748](sism555/btio.c:748), `bt_cyspp_contention_test()`):** Der Contention-
Test treibt IC54 Bit7 (CYSPP) bewusst 500 ms aktiv LOW, um am R78 eine Spannungsdifferenz zu
messen (Hardware-Diagnose aus der urspruenglichen CYSPP-Untersuchung, Messprotokoll Schritt 6).
Im Gegensatz zu `bt_cmdmode()` (die dieselbe Leitung als Ausgang treibt, aber VORHER
`bt_cyspp_out=1` setzt) fehlte dieser Schutz hier komplett. Ohne `bt_cyspp_out` haelt
`communication_change()` das testbedingte LOW fuer einen echten neuen Bluetooth-Connect und loest
`Init_BT_ch()`, `startmessage` und `bt_pininit` aus - eine komplett vom Test selbst erzeugte,
falsche Verbindungserkennung. Trifft das mit einer zeitgleich laufenden echten Verbindung
zusammen (wie im Testszenario: BT-Diagnose-Menue UND gleichzeitiger echter BT-Connect), kollidieren
beide Zustaende und erzeugen wiederholte Connect/Disconnect-Zyklen mit staendig neu eingespeisten
Zeichen - genau das beobachtete Bild.

**Fix:** `bt_cyspp_out=1` vor dem Treiben gesetzt (analog `bt_cmdmode()`), `bt_release()` am Ende
setzt es wie ueberall sonst ueblich wieder zurueck.

**Einordnung:** Dieser Bug existiert unabhaengig vom 5.104-Fix (Flanke->Pegel) bereits seit
Einfuehrung des Contention-Tests (Update 5.6x, temporaerer CYSPP-Diagnose-Code) und betrifft nur
diesen einen manuell aufgerufenen Diagnose-Menuepunkt - keine Auswirkung auf normalen Feldbetrieb
ohne Testmenue-Nutzung. Damit steht der 5.104-Fix (Pegelvergleich statt Flankenvergleich fuer
BT_LINK) nicht mehr unter Verdacht, selbst fehlerhaft zu sein.

**Weiterhin offen:** `bt_event_sniff()` (Testmenue 8->1) sollte auf denselben Fehlertyp geprueft
werden (manipuliert ebenfalls UART1/Puffer waehrend Verbindungsaufbau) - noch nicht untersucht.
Sobald die CYSPP-Diagnose insgesamt abgeschlossen ist, sollten Contention-Test, Event-Sniff und
Transfer-Test ohnehin wie in `CLAUDE.md` seit laengerem vorgesehen wieder entfernt werden.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.105).

## Update 5.106: Split-Brain live bestaetigt (Hardware treibt LOW, nicht wir) + neue Diagnose-/Testwerkzeuge

**Live-Test (5.105, Board 26SC9983):** Nach BT-Verbindungsaufbau via Viagraph/COM5 wieder ausgestiegen
(Dropout) - RS232 (COM3) blieb danach langsam. `bt_cyspp_monitor()` zeigte `bit7=LOW` UND
`bit5=LOW`, `comchange_count` unveraendert (Delta=0) ueber den gesamten Beobachtungszeitraum
(zwei parallele Terminal-Fenster, RS232 + BT, synchron mitgeloggt). Kein Interrupt-Sturm (widerlegt
die Vermutung "irgendein Interrupt feuert wild") - das Modul haelt die Leitung einfach konstant auf
LOW, ohne dass sich am I2C-Register ueberhaupt etwas aendert.

**Anwenderfrage: koennte UNSER Board/MCU das selbst auf LOW ziehen?** Um das direkt am Geraet
auszuschliessen, zeigt `bt_cyspp_monitor()` jetzt zusaetzlich bei jeder Bit7-Aenderung das IC54-
Richtungsregister (PCA9554 Register 0x03: Bit=1 -> Eingang, Bit=0 -> Ausgang) UND den Software-Merker
`bt_cyspp_out` mit an. Steht die Richtung auf "Eingang" und `bt_cyspp_out=0`, kann das beobachtete
LOW nachweislich nicht von unserer Seite kommen (der PCA9554 ist dann rein passiv, das Level kommt
extern vom Modul). `bt_release()` setzt zu Monitor-Beginn ohnehin auf Eingang - die Anzeige macht
das jetzt aber laufend sichtbar statt nur einmalig anzunehmen.

**Neues Testwerkzeug (Anwenderwunsch):** Testmenue -> 8 -> **5 = Force-Disconnect** ruft
`bt_disconnect()` (Command-Mode + `/DIS`) manuell auf und zeigt Bit7/Bit5/Richtung/`bt_cyspp_out`
jeweils VOR und NACH dem erzwungenen Trennen - damit laesst sich direkt pruefen, ob ein erzwungenes
`/DIS` den haengenden Split-Brain-Zustand aufloesen kann (Alternative zum Power-Cycle). Noch nicht
am Geraet getestet.

**Einordnung:** Damit ist die urspruengliche "vielleicht ziehen WIR das runter"-Sorge mit einem
direkten Messwerkzeug beantwortbar, nicht nur durch Code-Lesen. Der Split-Brain-Fall selbst bleibt
das schon lange bekannte, ungeloeste Problem (Modul erkennt einen bestimmten Trennungsweg nicht) -
siehe `CLAUDE.md`, Abschnitt "BT-Modul faellt gelegentlich komplett aus"/Split-Brain.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.106).

## Update (kein Versionssprung): Neuer Testmodus `idle-latency` in ble_mainboard_test.py

**Anlass:** Anwenderbeobachtung "das Problem kommt auch, wenn man zu lange in einem Menue ist und
nichts macht" (unabhaengig vom Testmenue) - passt zum im Code gefundenen Minutentick (`measure.c`,
`bt_release()` bei jeder RTC-Minute, siehe Update 5.106-Kontext). Manuelles Stoppen mit der Uhr war
umstaendlich und stoerungsanfaellig (jede Pruefung erfordert eine Aktion, die selbst das Timing
beeinflusst).

**Neuer Modus (aufbauend auf der bestehenden `ClassicTransport`/Marker-Navigation-Infrastruktur,
wie vom Anwender gewuenscht):** `--mode idle-latency` - verbindet frisch (--transport classic),
erreicht ueber `authenticate_or_sync_main_menu()` das Hauptmenue, wartet dann **exakt** eine von
`--idle-times` vorgegebene Leerlaufzeit (Default `20,55,65,90,125`s - bewusst um die vermutete
60s-Grenze verteilt), und misst danach per einziger minimaler Navigation (Hauptmenue -> Testmenue
-> zurueck, ueber die bestehenden Marker `MARK_TESTMENU`/`MARK_HAUPTMENU`) die Reaktionszeit in ms.
Jede Leerlaufzeit bekommt eine frische Verbindung (wie `--mode cycle`) - vermeidet Ordnungseffekte
zwischen den Messungen. Schreibt JSON+Markdown-Report wie die anderen Modi.

**Erwartetes Ergebnis, falls der Minutentick-Verdacht stimmt:** Laeufe mit Idle-Zeit knapp UNTER
einer Minutengrenze (20s, 55s) sollten normal schnell reagieren, Laeufe knapp DARUEBER (65s, 90s,
125s) auffaellig langsamer oder mit Timeout - noch nicht am Geraet verifiziert.

**Aufruf:** `python ble_mainboard_test.py --transport classic --port COM5 --mode idle-latency`

**Geaenderte Dateien:** Doku/tools/ble_mainboard_test.py.

## Update (kein Versionssprung): Manueller Test - Minutentick-Theorie widerlegt, Modul vermutlich nur "angeschlagen"

**Manueller Test (Board 26SC9983, 5.105, ohne Testmenue 8):** Verbunden, im Kopf bis 20 gezaehlt
(keine Aktion), dann einen Menuepunkt angeklickt - **war da schon nicht mehr sauber/langsam.**
Das widerspricht der Minutentick-Theorie (`measure.c`, `bt_release()` bei jeder RTC-Minute) klar -
20s ist deutlich kuerzer als die vermutete 60s-Grenze.

**Neue Arbeitshypothese:** Nach dem sehr intensiven heutigen Testtag (viele Connect/Disconnect-
Zyklen, mehrere Reflashs 5.101-5.106, Factory-Resets, Contention-Tests) ist das Modul auf diesem
Board vermutlich schlicht in einem angeschlagenen/degradierten Zustand - nicht, dass ein neues,
kurzfristiges Zeitproblem existiert. "Leerlaufzeit" waere dann gar nicht die relevante Variable.

**Naechster Schritt (noch nicht durchgefuehrt):** Kompletter Power-Cycle (laenger stromlos), danach
**sofort** nach dem Verbinden (ohne jede Wartezeit) einen Menuepunkt anklicken. Klappt das schon
beim allerersten Versuch nicht sauber, bestaetigt das die "Modul angeschlagen"-Hypothese - dann
waere ein Factory-Reset (Testmenue->8->4) der naheliegende naechste Schritt, nicht die weitere
Suche nach einem Zeit-Trigger. Der neue `idle-latency`-Testmodus (siehe oben) bleibt fuer eine
spaetere, sauberere/automatisierte Wiederholung dieser Frage nuetzlich, sobald das Modul wieder in
einem bekannt guten Ausgangszustand ist.

**Pause vereinbart (2026-09-30):** Test an dieser Stelle fuer ca. 20 Minuten unterbrochen.

## Update (kein Versionssprung): Nach Pause - ISP-Bootloader-Haenger + neue Hypothese BT/LTE-Mux-Verdraengung

**Zwischenfall nach dem Power-Cycle:** Geraet liess sich danach gar nicht mehr per RS232
ansprechen (LED blinkte, Geraet lebte, aber kein Terminalzugriff) - vermutlich im LPC-ISP-
Bootloader haengengeblieben (Ursache nicht geklaert, evtl. Nebenwirkung der heutigen Loetarbeiten
am ISP-/Reset-Pin-Bereich). Behoben durch Neuflashen mit Flash Magic - danach normaler Start mit
5.106.

**Neue Hypothese (Anwenderfrage "liegt es an USB/LTE/GPS?"):** Board 26SC9983 hat laut
Info-Menue Bluetooth, USB, Modem/Email-LTE UND GPS gleichzeitig aktiv. `uart1_request()`/
`uart1_release()` (hard.c) zeigen eine MUX-Ressourcen-Verwaltung fuer UART1, bei der ein
GSM/LTE-Request BT zwangsweise verdraengt (`connect&=~UART1`, MUX umgeschaltet) und beim
Rueckweg wiederherstellt. Arbeitshypothese: Wenn das IF820-Modul waehrend einer solchen
Verdraengung (z. B. geplante Email/SMS/MQTT-Aufgabe) die BT-Verbindung nicht sauber
weiterverarbeiten kann, koennte das beim Rueckweg zu genau dem beobachteten CTS-Wartehaenger
fuehren (`putb()`, sio.c:63ff., wartet auf CTS, das nach einer gestoerten Session u. U. nie mehr
kommt).

**⚠ KORREKTUR (Anwender, Hardware-Architektur):** GPS haengt NICHT am selben UART1-Mux wie
BT/LTE - GPS hat eine **eigene, dedizierte RS232-Verbindung** (vermutlich UART2, siehe
`Init_UART2()` in hard.c) und ist an dieser Verdraengungs-Problematik unbeteiligt. Der im Code
sichtbare `MUX_GPS`-Fall in `uart1_request()`/`case MUX_GPS: MUX_GPS_Select` bezieht sich auf
etwas anderes (vermutlich eine aeltere/optionale Variante) und ist fuer die aktuelle Analyse
nicht relevant - **nur BT und LTE/GSM teilen sich den UART1-Mux.** Die Hypothese verengt sich
entsprechend auf BT-vs-LTE-Verdraengung, GPS scheidet als Erklaerung aus.

**Naechster Schritt (noch nicht durchgefuehrt):** LTE-Modem (Email/SMS/MQTT-Zeitplanung)
voruebergehend deaktivieren (Einstellungen->Funkmodems bzw. Werksmenue) und pruefen, ob das
Haengenbleiben dann auch bei laengerem BT-Leerlauf ausbleibt.

## Update 5.107: Neues Testwerkzeug - CYSPP manuell HIGH treiben oder freigeben (floatend)

**Anlass (Anwenderwunsch):** "Wie kann ich CYSPP von Hand auf HIGH setzen? Oder floaten lassen?" -
zum gezielten Testen, ob ein manuell erzwungener Zustand einen haengenden Split-Brain-Fall
aufloest. Der bestehende Force-Disconnect-Test (5.106) laeuft automatisch in <1s durch (HIGH ->
/DIS -> Eingang) und laesst sich nicht anhalten/manuell steuern.

**Neu:** Testmenue -> 8 -> **6 = CYSPP manuell HIGH/Freigeben**. Zeigt Bit7-Pegel, Richtung und
`bt_cyspp_out` an, dann Auswahl: `1` = HIGH treiben (bleibt aktiv HIGH stehen, bis der Anwender
`2` waehlt), `2` = Freigeben (Eingang/floatend), `9` = zurueck. Der Zustand bleibt jeweils
bestehen, bis der Anwender ihn aktiv aendert - kein automatisches Timeout. Sicherheitsnetz:
Verlassen mit `9` gibt die Leitung IMMER zuerst frei (`bt_release()`), falls sie gerade HIGH
getrieben wird - CYSPP bleibt dadurch nie versehentlich ueber das Testmenue hinaus HIGH haengen.

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.107).

## Update (kein Versionssprung): Vergleichstest 26SC9983 vs. 26SC9994 - Konfiguration ausgeschlossen, Board-Ursache bestaetigt

**Vergleichstest "20s-Huerde":** Board 26SC9994 (unveraendertes Referenzboard, 151 Betriebsstunden,
Firmware 5.99, heute NICHT angefasst) zeigt das ~20s-Haenge-Problem **nicht** - Verbindung bleibt
laut Anwendertest normal. Board 26SC9983 (heute x-fach neu geflasht/zurueckgesetzt,
Firmware 5.106) zeigt es weiterhin. Bestaetigt: **kein allgemeiner Firmware-Bug**, sondern etwas
board-/modulspezifisches.

**IF820-Parametervergleich (`Hauptmenue->5->Bluetooth->1 Information`, `send_bt_info()`):**
Vollstaendiger Parameter-Dump auf beiden Boards gezogen und Zeile fuer Zeile verglichen - `/QFV`,
`GTU`, `.CYSPPGP`, `GBTDC`, `GBTP` (inkl. des zuvor verdaechtigten `T=7D00`-Feldes), `GAP`, `GPEM`,
`.CYSPPGK`, `GSBP`, `/RLH` sind **byte-identisch** auf beiden Boards - einzige Unterschiede sind
die erwarteten geraeteindividuellen Werte (Seriennummer im Namen, MAC-Adresse `GBA`) und der
Sitzungs-Traffic-Zaehler (kein Konfigurationswert). **Die IF820-Konfiguration ist damit als
Ursache ausgeschlossen** - beide Module sind identisch eingestellt.

**Einordnung:** Da weder Firmware-Version (5.104-5.107 vs. 5.99) noch Modul-Konfiguration den
Unterschied erklaeren, bleibt als wahrscheinlichste Erklaerung entweder (a) der physische
Board-/Modulzustand (Verschleiss, Entkopplung - alte C128/C58-Vermutung) oder (b) ein durch den
heutigen intensiven Testtag (viele Factory-Resets/Reflashs/Reboots) angeschlagener Modulzustand.
Naechster Schritt zur Unterscheidung: Factory-Reset (Testmenue->8->4) + Power-Cycle auf 26SC9983,
danach erneuter 20s-Test (siehe vorheriger Eintrag) - noch nicht durchgefuehrt/Ergebnis ausstehend.

## Update 5.108: FIX Layout - Factory-Reset-Meldung lief auf derselben Zeile wie die Modul-Antwort

**Nebenbefund (Anwender):** Nach `/RFAC` im BT-Diagnose-Menue erschien die Bestaetigungsmeldung
direkt hinter der rohen Modul-Antwort auf derselben Zeile ("< @R,000B,/RFAC,0000 /RFAC
bestaetigt (@R,...,/RFAC,0000)."), statt wie sonst ueberall in eigener Zeile.

**Ursache:** `bt_command()` (btio.c) haengt nach der `"  < <Antwort>"`-Ausgabe nur dann ein
abschliessendes `newline()` an, wenn der RN4678-spezifische CRLF-Flag-Pause-Wert genutzt wird
(`|pause|>10000`) - beim normalen positiven Pause-Wert (wie in `bt_factory_reset()`,
`bt_command(T_rfac,T_rfacok,1000)`) fehlt das Newline. Jede direkt folgende eigene Ausgabe lief
dadurch auf derselben Zeile weiter.

**Fix:** `bt_factory_reset()` ruft jetzt vor der eigenen Meldung explizit `newline()` auf.
**Layout-Konvention festgehalten** (Gedaechtnis): bei jedem `bt_command(...)`-Aufruf mit
positivem Pause-Wert, dem direkt eigener Text folgt, zuerst pruefen, ob ein `newline()` noetig ist.

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.108).

## Update (kein Versionssprung): Board 26SC9983 - vorlaeufiges Fazit "vermutlich Hardwaredefekt"

**Factory-Reset + Power-Cycle auf 26SC9983 durchgefuehrt (siehe vorheriger Eintrag) - Ergebnis:
Fehler (Verbindung haelt keine ~20s, siehe fruehere Eintraege) besteht unveraendert weiter.**

**Damit ergibt sich in der Summe der heutigen Tests ein eindeutiges Bild:**
- IF820-Konfiguration: byte-identisch zum unauffaelligen Referenzboard 26SC9994 (siehe voriger
  Eintrag) - Konfiguration als Ursache ausgeschlossen.
- Factory-Reset + Power-Cycle (voelliger Neuaufbau des Modul-Zustands): aendert nichts - "durch
  den heutigen Testtag angeschlagener Software-/Modulzustand" als Erklaerung damit ebenfalls
  ausgeschlossen.
- Referenzboard 26SC9994 (gleiche Firmware-Codebasis, andere Hardware-Einheit): zeigt das Problem
  nicht.
- Firmware-Versionen 5.99 bis 5.108 (edge- und pegelbasierte BT_LINK-Erkennung, mit und ohne die
  heutigen Fixes): Problem tritt in allen getesteten Versionen gleichermassen auf.

**Vorlaeufiges Fazit:** Alles, was sich per Software/Konfiguration/Reset beeinflussen liesse, wurde
ausgeschlossen. Es spricht viel fuer einen **echten, board-spezifischen Hardwaredefekt auf
26SC9983** - dasselbe Muster wie beim frueher bereits bestaetigten Fall Board 269996
(Firmware/Konfiguration unauffaellig, ein einzelnes Board reproduzierbar fehlerhaft, am Ende als
nicht-reparabler Hardwaredefekt identifiziert).

**Konkreter naechster Verdacht (physische Pruefung noetig, nicht per Software feststellbar):**
Die alte, nie abschliessend geklaerte Beobachtung aus `CLAUDE.md` - fehlende Entkopplungs-
kondensatoren `C128`/`C58` (100 nF an den IF820-Power-Pins, in beiden Bestueckungsvarianten als
nicht bestueckt eingetragen) - passt symptomatisch gut ("laeuft kurz, dann traege/instabil").
Empfehlung: pruefen, ob C128/C58 auf 26SC9983 bestueckt sind (Vergleich mit 26SC9994), sofern
Platinenzugriff besteht. Bis dahin: 26SC9983 fuer weitere BT-Stabilitaetstests als **vermutlich
hardwaredefekt** einstufen, nicht mehr als repraesentatives Testboard fuer Firmware-Aenderungen
verwenden.

## ✅ AUFGELOEST: Board 26SC9983 - Ursache gefunden, defekter Quarz (IF820)

**Anwenderbefund:** Der mit dem IF820-Modul verbundene Quarz auf Board 26SC9983 war defekt.
Behoben durch: neuer Quarz **+ 10 MOhm Widerstand parallel zum Quarz** (wie laut Anleitung/
Datenblatt fuer diese Oszillatorbeschaltung gefordert). Danach laeuft das Board einwandfrei -
die ~20s-Verbindungsinstabilitaet ist damit behoben. Bestaetigt auf 5.104, 5.108 (jeweils
sauberer Boot + Connect ohne Testmenue->8, kein Haengen mehr).

**Einordnung:** Bestaetigt die heutige Elimination in voller Reihenfolge: IF820-Konfiguration
(byte-identisch zu Referenzboard 26SC9994) und Software-/Modulzustand (Factory-Reset+Power-Cycle
ohne Wirkung) waren zu Recht ausgeschlossen - es war die ganze Zeit ein echter, physischer
Hardwaredefekt auf diesem einen Board, kein Firmware- oder Konfigurationsproblem. Passt zum
bereits bekannten Muster (Board 269996, frueher bestaetigt defekt). Der Verdacht "fehlende
Entkopplung C128/C58" hat sich damit NICHT bestaetigt - die tatsaechliche Ursache war der Quarz.

**Board-Inventar aktualisieren:** 26SC9983 Status von "vermutlich hardwaredefekt" auf "behoben
(Quarztausch), wieder normal nutzbar" setzen.

## Doku 2026-10-01: Recherche Modul-FW 1.4.12.12 vs. 1.4.18.18 + interner LPO vs. Quarz

**Kein Versionssprung** - reiner Doku-Eintrag. Volle Ausarbeitung mit Fundstelle je Aussage:
`Doku/HW_Massnahmen_Bewertung_2026-08.md`, Abschnitte **9.1** und **9.2**.

Kernpunkte:
- 1.4.18.18 behebt u. a. "lockup issue due in heavy UART loading" (PCN #2E-2026 S. 1 Abschn. 3).
- **Zwei Befunde fuer unseren Code auf 12.12:** `SBTP ... F=01` (btio.h:114, Sniff aus) und
  "CYSPP HIGH beendet SPP" sind laut PCN #2E-2026 S. 1 Abschn. 3 (New Features 3+4) erst in
  18.18 implementiert -> auf 12.12 vermutlich wirkungslos. Sniff-Zustand auf 12.12 noch nicht am
  Geraet geprueft.
- 18.18 gibt es als Quarz-Image (`int-ant`) und LPO-Image (`int-ant_int-lpo`); Herstellerregel:
  LPO-Image nur OHNE Quarz (GitHub-Release v1.4.18.18, "EZ-Serial binaries"). Datenblatt empfiehlt
  den Quarz ("if not, link issue may be occurred", DS S. 27 Fussn. 8/9). Keine ppm-Angabe fuer den
  LPO. -> Q3 bestueckt lassen, Quarz-Image verwenden.
- Neue Module ab Date Code 0926102 kommen mit 18.18, vermutlich LPO-Variante (Werksstandard seit
  16.16, PCN #7C-2024) - Folgerung, nicht belegt; beim ersten neuen Modul pruefen.

## Update 5.109 (Branch `connection-pin`): CONNECTION jetzt direkt an LPC-Pin P0.19 - Diagnose liest GPIO statt I2C

**Hardware (Anwender):** Die `CONNECTION`-Leitung (IF820 Pin 33/P27/CONN_IND) wurde direkt an den
Mikrocontroller **P0[19]** gefuehrt - nicht mehr ueber IC54/Pad IO5. Folge fuer den Code: der Pegel
kommt per einfachem GPIO-Read (`FIO0PIN & CONN_PIN`), ohne I2C-Zugriff und ohne gemeinsamen
IC54-Interrupt.

**Pin-Pruefung vorab (keine Kollision):** P0.19 ist in `hard.h` unbelegt (`USB_PPWR` hat zwar auch
Bit 19, ist aber Port 1). `PINSEL1[7:6]` bleibt 00 = GPIO (Init_Ports setzt dort nur AD02/AD03/
CAP30/CAP31), `FIO0DIR` enthaelt Bit 19 nicht -> Eingang, `PINMODE1[7:6]` bleibt 00 = **interner
Pull-up aktiv** (`PULLUP=0`). Der I2C1-Treiber (dessen SDA1-Alternativpin P0.19 waere) ist im
Projekt abgeschaltet (`RTE_I2C1 0`), SDA-Pin dort ohnehin P0.0. Kein weiterer Init-Code noetig.

**Aenderungen:**
- `hard.h`: `#define CONN_PIN (1<<19)` (Port 0, low aktiv, nur lesen).
- `btio.c` `bt_cyspp_monitor()`: CONNECTION wird direkt von P0.19 gelesen. **Vergleichsanzeige
  CYSPP vs. CONNECTION:** bei jeder Aenderung eines der beiden Pegel eine Zeile mit beiden Werten
  plus Bewertung (`bt_pair_verdict()`: beide LOW = verbunden/konsistent, beide HIGH = idle/
  konsistent, CYSPP LOW + CONNECTION HIGH = Datenmodus ohne vollstaendige Pipe, CYSPP HIGH +
  CONNECTION LOW = Abweichung). Darunter wie bisher Bit7-Richtung und `bt_cyspp_out`.
  Vorher kamen zwei getrennte Zeilen mit je eigenem Aenderungstrigger - ein direkter Vergleich war
  nur durch Zeilen-Zuordnung im Kopf moeglich.
- `btio.c` Force-Disconnect (8->5) VOR/NACH und manuelle Steuerung (8->6): zeigen
  `CONNECTION(P0.19)` statt des alten IC54-Bit5.

**Bewusst NICHT geaendert:** Die produktive `BT_LINK`-Erkennung (`get_com_status()`/
`communication_change()`) liest weiterhin CYSPP (IC54 Bit7). Der Umstieg der Statusquelle auf
CONNECTION (frueher "Stufe B", in 5.103 zurueckgebaut) ist ein eigener, groesserer Schritt - jetzt
mit GPIO statt I2C und mit den inzwischen gefixten Bugs (5.104/5.105) neu zu bewerten, aber erst
nach Auswertung der neuen Vergleichsanzeige am Geraet.

**Am Geraet zu pruefen:** Testmenue 7 (Monitor) bei Verbinden/Trennen: bleiben CYSPP und CONNECTION
im Normalfall konsistent? Gibt es Abweichungsfaelle (v. a. BLE: CONNECTION erst nach CCCD-Write)?
Ohne angeschlossenes CONNECTION-Kabel liest P0.19 wegen des internen Pull-ups dauerhaft HIGH.

**Geaenderte Dateien:** hard.h, btio.c, sprachen/sictxt.c (-> 5.109), CLAUDE.md (Branch-Hinweis).

**Live-Test 5.109 (Geraet 99SC9923, RS232 = COM3, BT-Classic = COM11):** Verbinden/Trennen/Verbinden
funktioniert, die Vergleichsanzeige zeigt idle = beide HIGH, verbunden = beide LOW (konsistent).
**Fehler:** Bei parallel offenem RS232 kamen nach dem Trennen "komische Zeichen", die das RS232-Menue
weiterfuehrten (im Log: "Ihre Auswahl 1...9?0", danach Hauptmenue -> "1" -> Datenausgabe-Abfragen).
Ausserdem beendete jeder Verbinden/Trennen-Moment (comchange) den Monitor sofort (Delta=2) - der
Uebergang selbst war damit nie zu sehen.

## Update 5.110 (Branch `connection-pin`): Datenbruecke am echten CONNECTION-Pegel + Monitor bricht bei comchange nicht mehr ab

**Ursache der Zeichen:** `UART1_IRQHandler` (hard.c) entscheidet per `connect&BT_LINK` (Software-
Zustand), ob ein Byte Benutzereingabe (`rbuf`) oder Modul-/Event-Text (`rbuf1`) ist. Dieser Zustand
wird erst nachgefuehrt, wenn EINT1 -> `comchange` -> Messschleife -> I2C-Read -> `communication_change()`
durchgelaufen sind. Das Modul sendet das `@E,...,BTDIS,...,R=0000`-Event aber SOFORT beim Trennen: die
Bytes landen in dieser Luecke in `rbuf`, `mainmenu()` liest sie unmittelbar als Tastendruck (Ziffern
`0`/`1` aus `...R=0000`/`...0012`). Der Flush `bxi=rxi` in `communication_change()` (5.82) kommt
dann zu spaet - die Bytes sind schon verbraucht. (Selbe Bugklasse wie 5.82/5.89, nur mit dem
Software-Zustand als Schwachstelle.)

**Fix (der eigentliche Nutzen von CONNECTION direkt am GPIO):** Die Datenbruecke gilt im ISR jetzt nur,
wenn `connect&BT_LINK` UND `!bt_cyspp_out` UND der echte Hardware-Pegel `CONNECTION (P0.19) == LOW`
(`BT_CONN_ACTIVE`, btio.h). Sobald das Modul die Datenpipe schliesst, geht CONNECTION HIGH und alle
weiteren Bytes laufen sofort nach `rbuf1` - ohne Software-Verzoegerung, ohne I2C. Per `BT_CONN_GPIO`
(btio.h, 1/0) abschaltbar; auf einem Board OHNE P0.19-Verdrahtung muss er auf 0, sonst liest der Pin
durch den Pull-up dauerhaft HIGH und BT-Eingaben wuerden nach `rbuf1` umgeleitet.

**Shared-State-Pruefung:** Betroffen ist nur der Fall "Software sagt verbunden, CONNECTION HIGH".
Beim Verbinden ist `BT_LINK` ohnehin noch nicht gesetzt (Bytes gehen schon bisher nach `rbuf1`), danach
liegt CONNECTION LOW (im Monitor bestaetigt). `rbuf1` wird laufend durch `bt_sniff_poll()` geleert
(`bxi1` zieht nach, measure.c) und per `rbuf1_full`-Wiederfreigabe (measure.c:368) abgesichert -
kein Deadlock; im ungünstigen Fall werden Bytes verworfen statt falsch als Taste gelesen.

**Offen/zu pruefen:** (a) Sendet das Modul das BTDIS-Event erst NACH dem Loeschen der Datenpipe
(CONNECTION HIGH)? Wenn es das Event davor sendet, bleibt ein Rest der Luecke. (b) BLE: CONNECTION
kommt laut Datenblatt erst nach dem CCCD-Write - Eingaben davor wuerden verworfen (Nutzdaten kommen
ohnehin erst nach dem Subscribe).

**Monitor:** `bt_cyspp_monitor()` beendet sich nur noch bei einer Taste (`getkey()==1`), nicht mehr bei
`comchange` - sonst endete er genau im interessanten Verbinden/Trennen-Moment. Bei `comchange` kurzer
`osDelay(50)`, weil `getkey()` dann sofort zurueckkehrt (kein Spin).

**Geaenderte Dateien:** hard.c, btio.h, btio.c, sprachen/sictxt.c (-> 5.110).

## Update 5.111: Ringpuffer-Review - Vollpruefung korrigiert, UART0-Ueberlaufschutz

**Anlass:** Nach 5.110 (Pufferproblem "Zeichen nach dem Trennen") Anwenderwunsch, alle Ringpuffer zu
reviewen. Bestand: `rbuf` (UART0/UART1), `rbuf1` (UART1 Command-Mode), `pbuf` (UART0 Plus / UART1
MQTT), `gbuf` (UART2 GPS) - alle 256 Byte mit `uchar`-Indizes (natuerlicher Wrap), Verbraucher-Index
`bxi`/`bxi1`/`bpi`/`bx2`, Erzeuger-Index nur im ISR.

**Befund 1 - Vollpruefung greift an genau einer Position nicht (alle vier Ringe):** `bxi!=(rxi+1)`
vergleicht `uchar` mit `int`: bei `rxi==255` ist `rxi+1 == 256` und kann nie gleich `bxi` sein. Steht
`bxi` dann auf 0, gilt der Puffer faelschlich als "nicht voll", `rxi` springt auf 0 und gleicht `bxi`
-> alle 255 ungelesenen Bytes wirken wie "leer", ohne Ueberlaufmeldung/Interrupt-Abschaltung. Nur bei
echtem Ueberlauf relevant (1 von 256 Positionen). **Fix:** `(uchar)(rxi+1)` an allen fuenf Stellen
(hard.c: MQTT-`pbuf`, `rbuf1`, `rbuf`, `gbuf`; plus die neuen UART0-Pruefungen).

**Befund 2 - UART0-ISR ohne Ueberlaufschutz** (`rbuf` und Plus-`pbuf`): bei Verbraucher >255 Byte im
Rueckstand (ca. 22 ms bei 115200 Baud; `mainmenu` verarbeitet max. 16 Byte je Hauptschleife) wurden
ungelesene Bytes ueberschrieben bzw. der Puffer wirkte wieder leer. **Fix:** ist der Puffer voll, wird
das neue Byte per Dummy-Read verworfen und `u0_ovf` hochgezaehlt (wie bei `gbuf`). Bewusst KEIN
Interrupt-Abschalten wie bei UART1, weil UART0 keine Hardware-Flusskontrolle hat (RX-FIFO liefe sonst
in Overrun). Die aeltesten Bytes bleiben dadurch unversehrt erhalten. `u0_ovf` wird am Ende des
CYSPP-Monitors (Testmenue 7) angezeigt.

**Befund 3 - Ausreisser `gsmio.c:410` `rxi=bxi;`:** schrieb den Erzeuger-Index aus einem Thread (alle
anderen Flush-Stellen machen `bxi=rxi`). Wirkung dieselbe (Puffer leer), aber Kollisionsrisiko mit dem
ISR. **Angeglichen** auf `bxi=rxi;`.

**Weitere Beobachtungen (nicht geaendert):** (a) Flushes zu festen Zeitpunkten (5.82/5.83/5.88/5.93)
koennen strukturell nicht greifen, weil das Menue die Bytes live liest - wirksam waren nur die Routing-
Fixes (5.89, 5.110). (b) Modul-Events in `rbuf1` setzen `bt_time` zurueck (`hard.c`, rbuf1-Zweig): ein
geschwaetziges Modul haelt den Sitzungs-Timeout am Leben. (c) `rbuf1` ist ueber `bt_sniff_poll()`
(measure.c) immer geleert, die Wiederfreigabe `measure.c:368` haengt am selben Index `bxi1` - kein
Deadlock. (d) Der `rbuf_full`-/`rbuf1_full`-Interrupt-Abschaltweg bleibt fuer UART1 unveraendert.

**Shared-State-Pruefung:** Nur ISR-Pruefausdruecke (ein `(uchar)`-Cast je Stelle) und der neue
UART0-Verwerfzweig wurden angefasst - keine Index-/Flag-Semantik der Verbraucher. Der UART0-Zweig
aendert das Verhalten nur im bisherigen Ueberlauffall (vorher: Datenverlust/Verwuerfelung).

**Nicht belegt:** Die Fehler wurden nicht am Geraet provoziert (Probe war angeboten, vom Anwender
verworfen: "bau den Fix einfach ein"). Am Geraet pruefbar: Testmenue 7 beenden und `u0_ovf` ansehen;
bei normaler Nutzung muss er 0 bleiben (>0 nur bei Bursts, z. B. grosse Texteingabe ins RS232-Terminal).

**Geaenderte Dateien:** hard.c, hard.h, gsmio.c, btio.c, sprachen/sictxt.c (-> 5.111).

## Update 5.112: SIM-Pruefung beim Hochfahren (+CPIN: READY), sonst Fehlerprotokoll

**Anlass (Anwender, 2026-10-02):** Es gibt Geraete, bei denen die SIM-Karte nicht erkannt wird, das fiel
bisher beim Hochfahren nicht auf. Wunsch: Ist LTE in den Parametern aktiviert, muss beim Hochfahren ein
`+CPIN: READY` kommen, sonst Eintrag ins Fehlerprotokoll.

**Bestand:** `atcpin()` prueft zwar per `AT+QSIMSTAT?` auf F12009 (5.50), laeuft aber nur bei der
geplanten Registrierung bzw. manuell (Menue) - nicht beim Boot. Beim Power-On wurde bisher nur
`test_gsm(1)` (AT/OK) gemacht, das Modem danach gleich wieder ausgeschaltet.

**Neu:** `gsm_sim_check()` (gsmio.c), aufgerufen in `main.c` im Power-On-Zweig direkt nach erfolgreichem
`test_gsm(1)` (Modem ist dann noch an, vor dem GPS-Test und vor `gsm_power(0)`):
- **Nur bei aktiviertem LTE:** der Aufruf liegt im Zweig `if (fp.gsm)` und nur nach erfolgreichem
  AT-Test; zusaetzlich bricht die Funktion bei `fp.gsm==0` sofort ab (Rueckgabe -1).
- Schaltet per `gsm_power(2)` UART1/MUX auf das Modem und fragt bis zu 8x `AT+CPIN?` ab (je ~0,5 s
  Wartezeit + 1 s Pause - die SIM-Initialisierung braucht nach dem Einschalten einige Sekunden und
  antwortet zunaechst "SIM busy").
- `+CPIN: READY` (und nicht "NOT READY") -> OK, kein Eintrag.
- `+CPIN: SIM PIN` -> SIM ist erkannt, PIN noch nicht eingegeben. **Bei konfigurierter PIN
  (`fp.simpin!=0`) ist das beim Boot der Normalfall** (`atcpin()` setzt die PIN erst bei der ersten
  Registrierung) und gilt als OK. Ohne konfigurierte PIN -> **F12002** (SIM-PIN-Problem).
- Alles andere (nie READY, "NOT READY", `+CME ERROR`, keine Antwort) -> **F12009** "SIM-Karte nicht
  erkannt" per `puterror(...,-1)` (Terminalausgabe + `protocol()` ins Flash-Fehlerprotokoll, wie bereits
  `F12001` beim Einschalten).
- Stellt danach den MUX-Zustand wie `test_gsm(1)` her (`FIO0CLR=CSAF`).

**Abweichung zur Formulierung "muss READY geben":** "SIM PIN" bei konfigurierter PIN zaehlt bewusst
NICHT als Fehler, sonst gaebe jedes Geraet mit PIN-geschuetzter SIM bei jedem Boot einen falschen
Fehlereintrag. Muss auch der PIN-geschuetzte Fall als Fehler gelten, ist das eine Zeile.

**Nicht geaendert:** Die bestehende `QSIMSTAT`-Pruefung in `atcpin()` (F12009 bei der Registrierung).
Der Boot-Test nutzt bewusst `CPIN` (Anwenderwunsch) - `+CPIN: SIM PUK` wird wie "nicht bereit" als
F12009 gemeldet (nicht als eigener Code).

**Nebenwirkungen/zu beachten:** (a) Ohne SIM verlaengert sich der Boot im Fehlerfall um bis zu etwa 12 s
(8 Versuche); mit SIM kommt READY normalerweise im ersten oder zweiten Versuch. (b) Bei jedem
Power-On ohne SIM entsteht ein Protokolleintrag (keine Entdoppelung - gewuenscht: "Eintrag ins
Fehlerprotokoll"). (c) Nicht am Geraet getestet - zu pruefen: mit SIM (READY, kein Eintrag), ohne SIM
(F12009 + Boot-Dauer), mit PIN-geschuetzter SIM und konfigurierter bzw. fehlender PIN.

**Geaenderte Dateien:** gsmio.c, gsmio.h, main.c, sprachen/sictxt.c (-> 5.112).

## Update 5.113: BT-Diagnose-Menue im Standardformat (eine Zeile je Punkt, normaler Prompt)

**Anlass (Anwender, Screenshot):** Die BT-Diagnose sah "hasslich" aus. In 5.108 war nur die verrutschte
Meldung nach `/RFAC` behoben worden - das Menue selbst nicht. Ausserdem erschien "Layout" dort versprochen.

**Befund (Ist-Zustand):** drei langgezogene Textzeilen mit Spalten ("1 = ...   2 = ...   3 = ..."),
dazu zwei `comchange_count`-Debugzeilen (5.69) vor und nach der Eingabe, **kein** normaler Prompt
(Eingabe-Echo stand allein auf einer Zeile) und kein Standardkopf. Die manuelle CYSPP-Steuerung (8->6)
hatte dasselbe Muster plus Meldungen direkt hinter dem Eingabe-Echo.

**Neu (Format wie Testmenue/Bluetooth-Menue):** Kopf `BT-Diagnose:`, jeder Punkt in eigener Zeile
ueber `put_menuitem()`, Abschluss `select('9',1)` -> "9. zurueck" und "Ihre Auswahl 1...9?". Das
Eingabe-Echo steht direkt hinter dem Prompt, Folgemeldungen beginnen in neuer Zeile. Die
`comchange_count`-Diagnose (TEMPORAER, 5.69) erscheint nur noch bei Auffaelligkeit (`Versuche>0` oder
`Delta>0`) als eine Zeile `[comchange_count=..., Delta=..., N Versuche]`; im Normalfall ist das Menue
frei davon (der CYSPP-Monitor zeigt die Zaehler weiterhin). Gleiches Format fuer 8->6.

**Bewusst unveraendert:** Menueziffern (1-6, 9) und der Kopftext `BT-Diagnose` - `ble_mainboard_test.py`
navigiert ueber den Marker "BT-Diagnose" und sendet `3` (Transfer-Test).

**Nicht am Geraet geprueft** (Kompilieren ohne Fehler/Warnungen; Sichtkontrolle am Terminal steht aus).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.113).

## Update 5.114: Einheitliche Einrueckung/Zeilenumbrueche in ALLEN BT-Diagnose-Ausgaben

**Anlass (Anwender):** "Mir geht es um die Einrueckungen und Zeilenumbrueche. Das soll konsistent
aussehen. Jeder Punkt bekommt eine neue Zeile." 5.113 hatte nur die Menueliste angeglichen; die Ausgaben
der Werkzeuge selbst waren uneinheitlich (Titel mal buendig, mal mit 1 Leerzeichen, Details mit 1 oder
3 Leerzeichen, mehrere Werte kommagetrennt in einer Zeile, Meldungen ohne Zeilenende).

**Festgelegtes Format (gilt fuer alle Diagnose-Werkzeuge in btio.c):**
- **Titel** buendig links, endet mit `:` (z. B. `Force-Disconnect:`, `Vorher:`, `Zustand:`).
- **Detailzeilen** genau **2 Leerzeichen** eingerueckt - dasselbe Mass wie `bt_command()` (`  > cmd` /
  `  < resp`), damit sich alles in einer Sitzung einheitlich liest.
- **Ein Wert je Zeile**, Werte-Spalte ausgerichtet (`  CYSPP bit7:         HIGH`).
- **Ergebnis-/Hinweiszeilen** als `  -> ...`.
- **Jede Meldung endet mit Zeilenumbruch**; Folgeausgaben beginnen in neuer Zeile.
Umgesetzt ueber kleine Hilfsfunktionen `bt_key()`, `bt_kv()`, `bt_kvn()`, `bt_lvl()`, `bt_dir7()` und
`bt_pin_state()` (btio.c, vor `bt_cyspp_monitor()`); Eingabeaufforderung und "9. zurueck" weiterhin
generisch ueber `select()` (`T_ausw`, sio.c) wie in allen Menues.

**Angepasst:** CYSPP/CONNECTION-Monitor (Kopf, Zustandsblock je Pegelwechsel mit je einem Wert pro
Zeile, Abschlussblock), Force-Disconnect (`Vorher:`/`Nachher:` je vier Zeilen), manuelle CYSPP-
Steuerung (`Status:`-Block), Contention-Test, Transfer-Test, Event-Sniff (Kopf und Ende), Factory-Reset
(inkl. `WICHTIG:`-Block), alle "nur IF820"-Hinweise, die `comchange_count`-Auffaelligkeitszeile.

**Bewusst nicht angefasst:** Die BT-Init-Meldungen im Boot-Log (` Step ...`, ` BT-Cfg: ...`, ` BT-Name: ...`,
1 Leerzeichen Einrueckung) - eigenes Format des Initialisierungslogs, nicht Teil der Diagnose-Menues.
Menueziffern und der Kopftext `BT-Diagnose` bleiben (`ble_mainboard_test.py` navigiert darueber).

**Nicht am Geraet geprueft** (Kompilieren ohne Fehler/Warnungen; Sichtkontrolle am Terminal steht aus).

**Geaenderte Dateien:** btio.c, sprachen/sictxt.c (-> 5.114).

## Update 5.115 (Branch `connection-pin`): CYSPP/CONNECTION-Monitor als Hintergrundtask - TEMPORAER

**Anlass (Anwender, Test von 5.114):** Befindet sich das Menue nicht im Hauptmenue (z. B. im Monitor),
wird bei einem BT-Connect keine PIN abgefragt, und es geht nicht weiter. Verbinden vor dem Monitor
(Hauptmenue, BT, PIN, dann Testmenue 7) zeigte korrekt **beide LOW, konsistent** - P0.19 reagiert also.

**Ursache (mein Fehler aus 5.110):** `bt_cyspp_monitor()` lief blockierend innerhalb von `mainmenu()`
und hielt damit die Hauptschleife an, in der `communication_change()` laeuft (`measure.c:307`). Ein BT-
Connect wurde waehrend des Monitors nicht verarbeitet (kein Banner, keine PIN-Abfrage). In 5.109 ging
es nur, weil der Monitor bei `comchange` abbrach; das hatte 5.110 entfernt. Zusaetzlich waeren die
Uebergaenge selbst (Verbinden, Trennen bei RS232 im Hauptmenue = Test des 5.110-Fixes) mit einem
blockierenden Monitor nicht beobachtbar.

**Fix:** Hintergrund-Monitor nach dem Muster von `bt_sniff_poll()`: `Testmenue 7` startet ihn und kehrt
sofort zurueck (nochmal `7` = beenden, sonst Selbstende nach 5 min); `bt_monitor_poll()` wird einmal je
Hauptschleifendurchlauf aus `measure.c` aufgerufen, fragt alle 100 ms IC54 Bit7 und P0.19 ab und gibt nur
bei Pegelaenderung einen `Zustand [+ms]:`-Block aus - **nur ans RS232-Terminal** (`connect&=~UART1`,
`BT_LINK` bleibt gesetzt, weil die UART1-ISR-Weiche es liest). Kein I2C-Zugriff bei aktivem GSM-/MQTT-
Thread (`osActiveThread`, wie `communication_change()`).

**⚠ TEMPORAER (Anwenderentscheidung 2026-10-14) - muss zurueckgebaut werden:** Nur fuer den CONNECTION-
Test (`Doku/Testplan_CONNECTION_P0-19.md`). **Rueckbau:** `bt_monitor_poll()` + `mon_*`-Variablen
(btio.c), Aufruf (measure.c), Deklaration (btio.h) und "(ein/aus)" im Testmenue-Text (sicom.c)
entfernen; `bt_cyspp_monitor()` auf die blockierende Fassung aus Commit `d6f8bb8` (5.114) zuruecksetzen
oder - mit dem uebrigen CYSPP-Diagnosecode - ganz entfernen. Vermerk steht im Code (btio.c, measure.c)
und in `CLAUDE.md` ("Aufraeumen"). Nicht vergessen: ein spaeter wieder eingefuehrter blockierender
Monitor darf nicht erneut den `comchange`-Abbruch weglassen (Ursache dieses Updates).

**Nicht am Geraet geprueft:** Das Verhalten des Hintergrundmonitors (Verbinden/Trennen waehrend der
Beobachtung) - Gegenstand von Testplan T1-T6.

**Geaenderte Dateien:** btio.c, btio.h, measure.c, sicom.c, sprachen/sictxt.c (-> 5.115),
Doku/Testplan_CONNECTION_P0-19.md (neu), CLAUDE.md.

**Testergebnisse CONNECTION an P0.19 (Board 99SC9923, Firmware 5.115, 2026-10-14; Details im Testplan):**
- **T1 Idle:** beide HIGH, kein Flackern, `comchange_count` stabil - kein externer Pull-up noetig.
- **T2/T3 Classic verbinden, PIN, Betrieb:** beide LOW gleichzeitig, PIN-Prompt einmal, PIN wird von der
  5.110-Weiche akzeptiert, Menues reagieren, keine weiteren Bloecke.
- **T4/T5 sauber trennen/neu verbinden (2 Laeufe + Zusatzzyklen):** beide HIGH gleichzeitig, **keine
  Fehlzeichen** am RS232 (5.110-Fix bestaetigt), PIN-Prompt je Verbindung einmal.
- **T6 hart trennen:** (a) Terminalfenster schliessen und (b) Bluetooth am PC aus: beide Pegel gehen jeweils
  gleichzeitig auf HIGH (ca. 1,2 s bzw. < 18 s), kein Split-Brain, kein Haengen. Zusaetzlich ein
  **echter Funkabriss** (nur Zufallstreffer, erster Connect nach BT-Reset am PC, COM11 blieb leer): beide
  Pegel blieben 19,8 s LOW (= Link-Supervision-Timeout `0x7D00` = 20 s) und gingen **gemeinsam** auf HIGH,
  das Geraet erholte sich selbst. **CONNECTION weicht in keinem der beobachteten Faelle von CYSPP ab.**
- **Einordnung:** Die "20 s"-Beobachtungen auf 26SC9983 (Quarz-Defekt) passen zu diesem Timeout - ein
  instabiler Funklink faellt nach 20 s auf - und haben mit Leerlauf nichts zu tun (plausibel, nicht bewiesen).
- **T7 (60 s Leerlauf):** noch offen - der erste Versuch war ungueltig (Monitor mit zweitem `7` wieder
  abgeschaltet, Leerlaufzeit nicht erkennbar). **T8 (BLE), T9, T10:** noch offen.

## Update 5.116: Testmenue-Zeile 7 zeigt den Monitor-Zustand (TEMPORAER)

**Anlass:** `7` schaltete den Hintergrund-Monitor mehrfach ungewollt wieder aus (statt mit `9` das Menue zu
verlassen), weil die Beschriftung "(ein/aus)" den Zustand nicht zeigte.
**Fix:** `bt_monitor_active()` (btio.c/.h); die Testmenue-Zeile lautet jetzt `7. CYSPP monitor (7 = starten)`
bzw. `7. CYSPP monitor (laeuft - 7 = stoppen)` (sicom.c). **TEMPORAER wie der Monitor selbst** - Rueckbau
zusammen mit ihm (Vermerke in btio.c und CLAUDE.md erweitert).
**Geaenderte Dateien:** btio.c, btio.h, sicom.c, sprachen/sictxt.c (-> 5.116), CLAUDE.md.

## Update (kein Versionssprung): CONNECTION-Test T7/T8 - BLE zeigt, was CONNECTION wirklich bedeutet

**T7 (Classic, 60 s Leerlauf):** bestanden - Antwort sofort, kein Pegelwechsel waehrend der Sitzung.

**T8 (BLE, `ble_mainboard_test.py --mode smoke --name VIASIS_99SC9923 --no-rs232`, Firmware 5.116):**
Script-Ergebnis `passed` (Subscribe nach 35 ms quittiert, Banner und `PIN = ` per BLE empfangen, PIN
akzeptiert, Hauptmenue erreicht, Verbindung hielt 8 s). Der Hintergrund-Monitor zeigte dabei die
**erste echte Abweichung** zwischen den beiden Signalen:
- `+110880 ms`: `CYSPP bit7: HIGH`, `CONNECTION P0.19: LOW` -> "ABWEICHUNG (unerwartet)"
- `+113851 ms` (ca. 3 s spaeter): beide LOW, danach Banner/PIN-Prompt (CYSPP geht mit dem Subscribe)
- `+127045 ms` (Trennen): beide gemeinsam HIGH.

**Deutung:** `CONNECTION` ist "Verbindung steht" und geht bereits beim Verbindungsaufbau auf LOW (BLE-Link),
nicht erst bei vollstaendiger Datenpipe (so nur sinngemaess im Datenblatt gelesen - gemessen ist: Link).
`CYSPP` ist "Datenpipe steht" und geht erst nach dem Subscribe (CCCD-Write) auf LOW. Bei Classic SPP fallen
beide zusammen (T2-T6: immer gleichzeitig). Die ~3 s Differenz entsteht beim Windows-BLE-Aufbau (GATT-
Discovery vor dem Subscribe), bei anderen Zentralen kann sie abweichen.

**Entscheidung/Folgerung:**
- **Stufe B ("`BT_LINK` aus CONNECTION") ist damit sachlich widerlegt:** die Verbindungsbehandlung
  (`Init_BT_ch`, `startmessage`, PIN-Prompt) wuerde bei BLE ca. 3 s VOR dem Subscribe laufen - das Banner
  ginge verloren (aehnlich dem urspruenglichen BLE-Fehler). `BT_LINK` bleibt bei CYSPP.
- **Die 5.110-Weiche ist bei BLE korrekt:** Bruecke gilt bei `BT_LINK` (CYSPP-basiert, erst nach Subscribe)
  UND CONNECTION LOW - ab dem Subscribe sind beide LOW, die PIN kam per BLE an.
- **Nutzen von CONNECTION bleibt:** als reiner "Link steht"-Indikator und fuer die Diagnose (Abweichung
  CYSPP/CONNECTION sichtbar). Fuer die Split-Brain-Erkennung gab es bei Classic keinen Unterschied.
- Noch nicht beobachtet: Fall "CYSPP LOW bei CONNECTION HIGH" (haengende Datenpipe ohne Link).

**Nebenbefund (Script):** Nach dem eigenen Trennen meldet `ble_mainboard_test.py` kurz "DISCONNECT (vom
Geraet ausgeloest)" und gleich danach "Sauber getrennt (durch uns)" - wirkt wie eine kosmetische Eigenheit
des Disconnect-Callbacks (Monitor zeigt einen sauberen Wechsel), nicht geprueft.

**T9/T10 (abgeschlossen):** Classic-Cycle-Test 10/10 bestanden, am RS232 pro Zyklus Banner/PIN/Hauptmenue ohne
Fehlzeichen; `UART0-RX verworfen: 0`; Protokoll ohne Fehlereintraege. **Testplan `Doku/Testplan_CONNECTION_P0-19.md`
damit komplett durchlaufen (T0-T10 ok).** Nebenbefund: Protokollzeitstempel bei ungestellter Uhr unplausibel
(`I32 ... 17:03:13` liegt nach der Geraetezeit 16:52:21).

**Noch zu entscheiden:** (1) Rueckbau des temporaeren Hintergrund-Monitors (siehe 5.115/5.116, Vermerke in
btio.c/CLAUDE.md). (2) Ob `BT_CONN_GPIO 1` (5.110-Weiche) produktiv bleibt - hardwareabhaengig, auf Boards ohne
P0.19-Verdrahtung muss er auf 0. (3) Welche Aenderungen vom Branch `connection-pin` nach `LTE-Update`
uebernommen werden: allgemein gueltig sind 5.104 (BT_LINK-Pegelvergleich), 5.105 (Contention-Test-Schutz),
5.111 (Ringpuffer), 5.112 (SIM-Pruefung), 5.113/5.114 (Menueformat); hardwareabhaengig sind 5.109/5.110
(P0.19, Weiche) und der temporaere Monitor 5.115/5.116.


## Update 5.117: Ymodem-Download scheiterte am 1. Datenblock - CTS-Timeout in putb() verwarf Bytes

**Symptom (Board 99SC9923, 5.116, 2026-10-14):** Nach dem Flash-Schreiben (419430 Messwerte) schlugen alle
Downloads (Classic und BLE) fehl: Ymodem-Header und ACK ok, der 1. Datenblock kam am PC mit ~1023-1025 statt
1029 Byte an, der PC sendete nach ~1,9 s NAK, das Gerät wiederholte den Block - ebenfalls zu kurz - bis zum
Abbruch (F13010 Kommunikations-Protokollfehler). Im Fast-Lauf (100 Messwerte) trat derselbe Fehler beim 1. Versuch
ebenfalls auf, dort half erst der Retry. Auch der 5.108-Lauf vom 2026-10-01 zeigte "Retries 3" - der Fehler ist
also nicht neu und nicht durch 5.109-5.116 entstanden.

**Beleg (Zaehler im Info-Menue nach dem fehlgeschlagenen Lauf):** `W11015 CTS Timeout` direkt nach dem ersten
`/PING`, `ctswait=30` (= 6 Versuche x 5 Bytes), `TXstuck=0`. RS232 (UART0-Spiegel) zeigte alle 6 Versuche
vollstaendig. Das Modul bremst am Ende des ersten Blocks laenger als 50 ms per CTS.

**Ursache:** `putb()` (sio.c) wartete je Byte hoechstens `CTS_WAIT_MS` = 50 ms auf CTS-LOW und verwarf das
Byte dann mit `return(-1)`. `sendblock()` (xmodem.c) wertet den Rueckgabewert nicht aus. Die verworfenen Bytes
fehlten im Block; der Retry traf dieselbe Bremse wieder. Die 50 ms waren als Totmodul-Erkennung gedacht (5.59
hatte schon den FIFO-Stau auf 1 s gesetzt, die CTS-Schleife aber nicht angepasst).

**Fix (5.117, sio.c):** Bei aktiver BT-Datenverbindung (`connect&BT_LINK`) wartet `putb()` bis zu
`CTS_WAIT_LINK_MS` = 1000 ms auf CTS-LOW. Laeuft das ab (Link tot), schaltet `cts_stalled` auf die kurzen 50 ms
zurueck, bis CTS wieder LOW ist - das begrenzt die Blockadezeit bei Funkabriss. GSM/Kommandomodus
(ohne BT_LINK) unveraendert 50 ms. Neuer Diagnosezaehler `bt_cts_drop` (verworfene Bytes) im Info-Menue als
`ctsDrop=` neben `ctsTO=`.

**Verifikation:** Build 0 Error(s), 0 Warning(s). **Am Geraet noch nicht getestet.** Test: `ble_mainboard_test.py
--mode receive --transport classic` (und BLE) mit 419430 Messwerten; erfolgreich, wenn der Download
durchlaeuft und `ctsDrop=0`.


## Update 5.118: Sitzungszaehler beim BT-Trennen als eine Zeile (TEMPORAER, nur Debug)

Beim Trennen einer BT-Verbindung gibt die Firmware eine Zeile `[BT-Sitzung beendet] TX=.. ctswait=.. ctsDrop=..
TXstuck=.. lost_ms=..` nur ans Terminal aus (`bt_session_summary()`, btio.c; Aufruf in libtool.c). Der
RS232-Sniffer des Testskripts schreibt sie in den Trace - die Zaehler muessen nicht mehr von Hand im Info-Menue
abgerufen werden. Reine Anzeige, kein Eingriff in die Uebertragung. **TEMPORAER:** nach Abschluss der
CTS-/Ymodem-Untersuchung (5.117) wieder entfernen, Hinweis in CLAUDE.md ("Aufraeumen").
Build 0 Error(s), 0 Warning(s); am Geraet nicht getestet.


## Verifikation 5.117/5.118 am Geraet (Board 99SC9923, 2026-10-14): Ymodem-Download laeuft durch

- **Fast-Suite (100 Messwerte)** `2026-10-14_16-32-36`: bestanden (104 s). In allen BT-Sitzungen `ctsDrop=0`,
  `TXstuck=0`; Classic-Download `ctswait=0`, BLE-Downloads `ctswait` 1-2. Kein Retry beim 1. Datenblock (mit 5.116
  war im Fast-Lauf ein NAK-Retry noetig).
- **Grosser Fall (419430 Messwerte, Classic)** `2026-10-14_16-35-44`: **bestanden** - 4.199.424 Byte in 4101
  Bloecken, 561,9 s, 7474 B/s, **Retries 0**. Sitzungszeile: `TX=4273086 ctswait=1270 ctsDrop=0 TXstuck=0
  lost_ms=14679`. Das Modul bremst also 1270 Mal per CTS (zusammen ~14,7 s von 562 s), es geht aber kein Byte
  mehr verloren. Mit 5.116 scheiterte derselbe Test am 1. Datenblock (F13010).
- **Noch offen:** gleicher grosser Test ueber BLE; Wiederholung zur Absicherung (Fehler trat vorher nicht bei jedem
  Lauf auf); Vergleich mit 5.99 (`sism555/fw_5.99/`, nicht committet) nur noch noetig, wenn der Fehler wieder
  auftritt; Rueckbau von `bt_session_summary()` (5.118, TEMPORAER) nach Abschluss.


## Dauertest 5.118 am Geraet (Board 99SC9923, 2026-10-15): zwei komplette Modul-Suiten bestanden

**Laeufe:** `Testreports/2026-10-15_08-16-38_VIASIS_99SC9923_module-suite` (08:16-09:56) und
`.../2026-10-15_10-00-52_...` (10:00-11:40), je ~99 min, Firmware 5.118 (enthaelt 5.117), RS232-Sniffer auf COM3 aktiv.
Beide Laeufe **passed**, alle 11 Phasen je Lauf: Classic Smoke, Classic 12 Zyklen (Haltezeiten 0-300 s), BLE Smoke,
BLE 12 Zyklen, Flash schreiben (419430 Messwerte, je 161,2 s), 3x Classic- und 3x BLE-Ymodem-Download.

| Messgroesse | Lauf 08:16 | Lauf 10:00 |
|---|---|---|
| Classic-Zyklen 12/12, PIN+Menue ok | ja (Connect 50-1751 ms) | ja (48-1477 ms) |
| BLE-Zyklen 12/12, PIN+Menue ok | ja (Connect 3,1-9,2 s) | ja (3,1-5,5 s) |
| Downloads je 4.199.424 Byte / 4101 Bloecke | 6/6, Retries 0 | 6/6, Retries 0 |
| Download Classic | 7303 B/s (575 s) | 7480 B/s (561 s) |
| Download BLE | 6559 B/s (640 s) | ca. 6,6 KB/s (625-640 s) |
| `ctsDrop` / `TXstuck` in allen 33 BT-Sitzungen | 0 / 0 | 0 / 0 |
| F-/W-Codes, Watchdog, Reset auf RS232 | keine | keine |

**Pruefungen / Querbezuege (alle stimmig):**
- `TX` je Download-Sitzung ist in **allen 12 Downloads identisch: 4.220.641 Byte** - ein einziger Retry (+1029) oder
  verworfenes Byte wuerde sich sofort zeigen. Das ist der staerkste Beleg dafuer, dass 5.117 den Ymodem-Fehler
  (Update 5.117) behoben hat.
- **CTS-Bremse nur bei Classic:** `ctswait` je 4,2-MB-Download Classic 1426-1598 (Zeitverlust 15-18 s von ~570 s,
  also ~3 %), BLE nur 2-9 (< 0,1 s). Das Modul bremst per CTS nur auf dem Classic-Pfad; dort greift die
  CTS-Wartezeit-Aenderung von 5.117 (vorher: 50-ms-Limit verwarf Bytes).
- Exportdateien durchnummeriert `_019`...`_024` (Lauf 1) und `_025`...`_030` (Lauf 2) - lueckenlos, es gab keine
  versteckten oder wiederholten Exporte.
- UART0-Spiegel: Sniffer zaehlte 25.384.820 bzw. 25.384.897 Byte (= 6 Downloads x ~4,22 MB) in beiden Laeufen -
  praktisch identisch.
- 5.116 scheiterte bei gleichem Test am 1. Datenblock (F13010); der Fast-Lauf desselben Tages brauchte dort einen
  NAK-Retry. Diese Retries sind mit 5.117/5.118 verschwunden (Retries 0 in 12/12 Downloads).

**Unstimmigkeiten im Testskript/Bericht (kein Firmware- oder Testergebnisfehler, `ble_mainboard_test.py`):**
1. **Berichte werden ueberschrieben:** `write_report()` schreibt je Art immer dieselbe Datei (`download.json`,
   `cycle.json`). `phase()` in `run_module_suite()` erkennt neue Berichte per Dateimengen-Differenz; nur der erste
   Lauf je Art wird gefunden. Folge: In `module-suite.json` und im HTML (Tabelle "Ymodem-Downloads", Zyklen) stehen nur
   Classic Download 1/3 und die Classic-Zyklen; fuer die anderen 5 Downloads und die BLE-Zyklen ist `report: null`,
   die Ordner-Dateien `download.json`/`cycle.json` enthalten nur den letzten Lauf (BLE). Das Ergebnis `passed`
   stimmt trotzdem (Rueckgabecode); die Einzelwerte sind im `trace.ndjson` und in den `BT-Sitzung`-Zeilen.
2. **`unexpected_disconnect: True` bei jedem erfolgreichen BLE-Smoke** (`disconnect_reason:
   hold_time_elapsed_still_connected`): der Disconnect-Callback setzt das Flag auch beim eigenen, gewollten Trennen
   (`ble_mainboard_test.py` ~Z. 813). Das Wort "vom Geraet ausgeloest" im Log ist dadurch irrefuehrend.
3. **Ymodem-Dateigroesse wird nicht geparst:** Log "Groesse=None", `filesize: null` - der Test kann die gemeldete
   Groesse (4.199.420) nicht gegen die empfangenen 4.199.424 Byte (Blockauffuellung) pruefen. Es gibt ausserdem keine
   Inhaltspruefung ueber die Block-CRC hinaus (Dateiinhalt der 12 Downloads wird nicht verglichen).
4. **Sitzungszeile eine Phase verschoben:** Die Zeile `[BT-Sitzung beendet]` (5.118) erscheint erst, wenn die
   Firmware das Trennen erkennt, und wird daher im Trace der **folgenden** Phase zugeordnet (z. B. steht der
   Flash-Schreiben-Sitzung `TX=52733` unter "Classic Download 1/3"). Beim Auswerten um eine Sitzung versetzt lesen
   (33 Zeilen fuer 33 Sitzungen, Reihenfolge stimmt).
5. **BLE-Smoke schreibt keine Verbindungs-Events in den Trace** (nur `marker_found`), die Smoke-Zeiten stehen nur in
   `smoke.json`.

**Weitere Auffaelligkeit (Firmware, bekannt):** Die Exportdateinamen tragen das Geraetedatum `141026`, am Testtag war
aber der 15.10.2026 - die Geraeteuhr ist nicht gestellt/driftet (siehe T10 im Testplan CONNECTION: Protokollzeiten
bei ungestellter Uhr unplausibel).

**Fazit:** 5.117/5.118 bestehen den kompletten Dauertest auf Classic und BLE zweimal hintereinander fehlerfrei.
Offen: Vergleichslauf mit 5.99 (`sism555/fw_5.99/`, nur noetig, falls das Problem wiederkehrt); Rueckbau von
`bt_session_summary()` (5.118) und des CYSPP/CONNECTION-Monitors (5.115/5.116); Entscheidung `BT_CONN_GPIO`; Uebernahme
der allgemeinen Aenderungen (5.104/5.105/5.111/5.112/5.113/5.114/5.117) nach `LTE-Update`; Behebung der
Skript-Unstimmigkeiten 1-3.

**Nachtrag 2026-10-15: Skript-Unstimmigkeiten 1-3 behoben** (`ble_mainboard_test.py`, offline mit Beispieldaten
getestet, noch nicht in einem echten Lauf):
1. `collect_phase_report()` ersetzt die Mengendifferenz: je Phase wird der neueste seit Phasenstart geaenderte Bericht
   gelesen und zusaetzlich als `phase_NN_<Phase>.json` gesichert - Gesamtbericht/HTML enthalten damit alle Downloads,
   Zyklen und BLE-Phasen.
2. Der Disconnect-Callback des BLE-Smoke ignoriert das eigene, gewollte Trennen (`unexpected_disconnect` bleibt bei
   erfolgreichem Test `False`).
3. Die Ymodem-Dateigroesse wird nach dem ersten Nullbyte des Header-Blocks gelesen (Beispiel `4199420`); am Ende wird
   `filesize <= bytes < filesize+1024` geprueft (`size_ok`), ein abgeschnittener Empfang schlaegt fehl.
Punkte 4 (Sitzungszeile eine Phase verschoben) und 5 (BLE-Smoke ohne Trace-Events) bleiben offen.


## Beobachtung 2026-10-19: BLE-Download nach 4 Tagen Laufzeit nur noch halb so schnell (Board 99SC9923, 5.118)

**Lauf:** `Testreports/2026-10-19_08-23-08_VIASIS_99SC9923_module-suite` (Gerät zuvor 4 Tage am Stück eingeschaltet,
5.118). Der Test lief nur teilweise: **Classic Smoke fehlgeschlagen** (Verbindung steht nach 0,5 s, aber kein
Banner/`PIN = ` innerhalb 8 s, obwohl die Firmware die 70 Byte Banner an UART1 gesendet hat), dadurch Classic-Phasen
übersprungen. BLE-Smoke, 12 BLE-Zyklen, Flash schreiben und 3 BLE-Downloads bestanden (Retries 0, `ctsDrop=0`,
Größenprüfung `4199420` ok).

| Messgröße | 2026-10-15 (frisch eingeschaltet) | 2026-10-19 (nach 4 Tagen) |
|---|---|---|
| BLE-Download Durchsatz | ca. 6,6 KB/s (625-640 s) | **3,12-3,15 KB/s (1335-1346 s)** |
| `ctswait` je 4,2-MB-BLE-Download | 2-9 | **13.300-13.400** |
| `lost_ms` (Zeit in der CTS-Wartschleife) | < 100 ms | **~457-459 s (ca. 34 % der Dauer)** |
| BLE-Connect je Zyklus | 3,1-5,5 s (einzeln bis 9,2 s) | 5,1-7,6 s |

**Befund:** Die Rate ist über die drei Downloads konstant (kein schleichender Abfall innerhalb des Laufs), aber
halbiert gegenüber dem Frischstart. Die Firmware verbringt etwa ein Drittel der Zeit damit, auf CTS-LOW des Moduls zu
warten: das **IF820 bremst bei BLE jetzt ~13.400-mal** (vorher kaum), sein Sendepuffer läuft über die Funkstrecke
nicht schnell genug leer. Kein Byte geht verloren (`ctsDrop=0`, einzelne Wartezeiten < 1 s; mit dem alten
50-ms-Limit vor 5.117 wäre das riskant gewesen). Gleichzeitig ist der BLE-Verbindungsaufbau langsamer und der
Classic-Datenpfad fiel beim ersten Connect aus. Das spricht für einen schlechteren BLE-/Funkzustand (Modulzustand nach
langer Laufzeit, Verbindungsparameter oder Umgebung); die Firmware-Seite (Flash-Lesen, `putb()`) ist nicht die
Bremse. Ursache **noch nicht geklärt**.

**Nächste Schritte (zur Trennung der Ursachen):** gleicher BLE-Download (a) nach PC-Bluetooth aus/an, (b) über Classic,
(c) nach Aus-/Einschalten des Geräts. Ist (c) wieder bei ~6,5 KB/s mit `ctswait` < 10, hängt es an der Laufzeit von
Gerät/Modul; sonst an Umgebung/PC.

**Testskript:** Beim Auswerten fiel auf, dass `collect_phase_report()` (Nachtrag 2026-10-15) mit 1 s Spielraum den
`cycle.json`-Bericht der Vorphase der Phase "Flash schreiben" zuordnete (doppelte Zyklenzeilen im HTML). Spielraum
entfernt. Die neue Dateigrößenprüfung und die Phasenberichte `phase_NN_*.json` haben im echten Lauf funktioniert.

**Nachtrag 2026-10-19: Langsam-Erkennung im Testskript** (`ble_mainboard_test.py`; offline mit simuliertem
Ymodem-Transport und gepatchtem Wiederholungslauf getestet, noch nicht in einem echten Lauf):
- Je Download wird die Rate gegen eine Schwelle geprueft (Standard BLE 4500 B/s, Classic 5500 B/s; `--min-bps`,
  `0` = aus). Liegt sie darunter, steht `slow: true` im Bericht, die HTML-Tabelle "Ymodem-Downloads" zeigt in der neuen
  Spalte "Link" `langsam`, und ein Trace-Eintrag `slow_link` (Warnung, mit RSSI) erscheint unter "Warnungen und Fehler".
  Das Testergebnis bleibt `passed`.
- Bei grossen Downloads (>= 300 KB) wird nach `--slow-check-s` (60 s) frueh abgebrochen, wenn der Schnitt unter der
  Schwelle liegt; nach `--slow-wait-s` (120 s) folgt ein neuer Versuch (`--slow-retries`, Standard 1). Der letzte
  Versuch laeuft ohne Fruehabbruch zu Ende und wird nur markiert. Nebenwirkung: Der Abbruch erzeugt am Geraet einen
  Protokolleintrag F13010 und verbraucht eine Dateinummer.
- Gilt fuer den Modus `receive` (Downloads der Suites); der Modus `download` (Schreiben + Export in einem Lauf)
  prueft nur die Markierung, bricht aber nicht frueh ab.


## Update 5.119: Modul-Events "C" und "DIS" immer als Zeile auf RS232 (TEMPORAER, nur Diagnose)

**Anlass:** BLE-Download 2/3 im Lauf `2026-10-19_10-04-59` brach bei ~96 % mit `Not connected` ab, danach fand der PC das
Geraet 30 s lang nicht (kein Advertising), und die Firmware merkte vom Abbruch erst 25 s spaeter (`F13010 ...
67290`, wartet danach am Prompt `Daten loeschen (j/n)?`). Warum die Verbindung endete, steht in keiner Datei: das
Modul meldet es als `@E,...,DIS,C=01,R=....` (R=0913 Gegenstelle beendet, 0916 lokal beendet, 0908 Supervision-
Timeout), diese Zeile wurde aber nur im manuellen 180-s-Event-Sniff angezeigt. Dasselbe fuer den Langsamlauf vom
2026-10-19: das `C`-Event nennt die Verbindungsparameter (`I=` Intervall, `L=` Latenz, `O=` Timeout; Beispiel
`@E,0035,C,C=01,A=...,T=00,I=0006,L=0000,O=0064,B=00`).

**Aenderung:** `bt_sniff_poll()` (btio.c) liest rbuf1 weiter ausschliesslich ueber `bxi1` (unveraendert einziger
Verbraucher, Momentaufnahme `end=rxi1`), setzt die Bytes aber zu Zeilen zusammen (`bt_event_line()`) und gibt
Events mit `,C,` oder `,DIS,` als `[BT-Event] @E,...` ans Terminal aus (nie ans Modul). Alles andere wird wie bisher
verworfen; bei laufendem Event-Sniff aendert sich nichts. Build 0 Error(s), 0 Warning(s). **Am Geraet nicht getestet.**

**Grenzen:** Das Event erreicht rbuf1 nur, wenn die Bruecke zum Zeitpunkt nicht aktiv ist (ISR-Routing nach
`BT_LINK`/`BT_CONN_ACTIVE`); ein `DIS` waehrend die Firmware noch `BT_LINK` haelt kann im Bruecken-Puffer landen und
fehlt dann. **TEMPORAER:** nach Abschluss der Untersuchung entfernen (Hinweis in CLAUDE.md "Aufraeumen").

**Nachtrag 2026-10-19: PC-Seite im Testskript mitprotokolliert** (`ble_mainboard_test.py`; auf diesem PC offline gegen die
echten Windows-Ereignisse der letzten 6 h getestet - es findet genau die drei Standby-Phasen des Tages und die
Stromquellenwechsel -, noch nicht in einem kompletten Testlauf):
- **Wachhalten:** `SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED)` waehrend des Laufs. Ob das den modernen
  Standby tatsaechlich verhindert, ist noch offen (die Anzeige darf ausgehen); im naechsten Lauf an den Kernel-Power-
  Ereignissen 506/507 pruefen.
- **Trace `pc_info`** zu Beginn: PC-Laufzeit, Stromquelle, Akku, Wachhalten ja/nein; Warnung bei < 15 min seit PC-Start.
- **Am Ende (vor dem Bericht)** werden die Windows-Systemereignisse des Testzeitraums (Kernel-Power, Bluetooth/USB, PnP) in
  `pc_events.json` im Report-Ordner abgelegt; Standby-Phasen (`pc_standby`), Ruhezustand, Stromquellenwechsel und
  Bluetooth-/USB-Fehler erscheinen als Trace-Warnungen und damit im HTML unter "Warnungen und Fehler".
- Hintergrund: Auf diesem PC sind keine Bluetooth-Betriebslogs aktiviert; die Systemereignisse sind die einzige verfuegbare
  PC-Quelle. Am 2026-10-19 ging der PC waehrend der Laeufe dreimal in den modernen Standby (08:36-08:44, 11:14-11:14,
  11:19-11:30) und wurde um 07:59 neu gestartet (Testlauf ab 08:23).

**Nachtrag 2026-10-19: neue Testmodi `interleave` und `soak`** (`ble_mainboard_test.py`; Ablauf offline mit simulierten
Zyklen/Downloads getestet - Reihenfolge, Haltezeit-Rotation, Fehlerzaehlung je Uebergang, Abbruch, CSV -, noch nicht am
Geraet). Mit dem vorhandenen Aufbau (PC, ein Geraet, RS232-Mitschnitt, Classic + BLE) sind die folgenden Tests moeglich:
- **Schneller Zyklentest (Verbinden/Trennen im Sekundentakt)** - gab es schon: `--mode cycle --count 500 --hold 1
  --cooldown 0 --transport ble` (bzw. `classic`).
- **`--mode interleave`:** strikter Wechsel Classic/BLE; je Runde ein Classic- und ein BLE-Zyklus mit rotierenden Haltezeiten
  (Standard 1/2/5/10/20 s, `--cycle-holds`), `--rounds` (20), `--max-failures` (10). Ausgabe: Erfolg je Uebergang
  ("ble nach classic" usw.) und je Schritt `interleave_step` im Trace. Ziel: Fehler wie "BLE ohne Daten nach der
  Classic-Phase" einem Transportwechsel zuordnen.
- **`--mode soak`:** schreibt einmal `--soak-records` (50000 = ~500 KB) Messwerte und laedt sie dann `--soak-hours` (8)
  lang im Wechsel BLE/Classic herunter (`--soak-pause-s` 60). Je Download eine Zeile in `soak.csv`: Rate, Retries,
  Langsam-Markierung, RSSI, PC-Laufzeit und die Firmware-Sitzungszaehler (`ctswait`, `lost_ms`, `ctsDrop`, `TXstuck`).
  Zeigt Verlangsamung ueber die Geraetelaufzeit und Pufferlecks; Gegenstueck zur Uptime-Reihe (Test 12/20) ohne
  Neustart des Geraets. Fuer eine Reihe ueber mehrere Tage das Geraet einfach laufen lassen und `--soak-skip-write`
  verwenden.

**Nachtrag 2026-10-19: Kontrollmessung mit zwei Geräten (`soak --devices`)** (`ble_mainboard_test.py`; Ablauf offline mit zwei
simulierten Geräten getestet, noch nicht am Gerät): `--mode soak --devices VIASIS_<sernoA>,VIASIS_<sernoB>
--rs232-port COM3 --rs232-port-b <USB-COM des zweiten Boards>` schreibt auf beiden Geräten Testdaten und lädt dann
abwechselnd A-BLE, A-Classic, B-BLE, B-Classic herunter, alles unter denselben PC-Bedingungen. Je Gerät wird der RS232-/USB-
Mitschnitt gelesen (Sitzungszählerzeile `ctswait`/`lost_ms`/`ctsDrop`); `soak.csv` hat die Spalte `geraet`; am Ende steht
eine Auswertung je Gerät/Transport (n, Fehler, Rate mittel/min/max). Lesart: wird nur ein Gerät langsam oder fehlerhaft,
liegt es an Gerät/Modul; werden beide gleichzeitig schlecht, an PC/Funk. Die Zieldaten (BLE-Adresse, Classic-Name und -Port)
werden je Gerät beim ersten Durchlauf gemerkt. Voraussetzung: beide Boards sind am PC gekoppelt (Classic-COM-Port), beide
laufen mit Firmware >= 5.118/5.119.

**Nachtrag 2026-10-19: automatische Port-Erkennung für das zweite Board** (`soak --devices`, `ble_mainboard_test.py`; Erkennung
auf diesem PC real geprüft, Zuordnungslogik mit simulierten Bannern, noch nicht mit zwei echten Boards):
- `--rs232-port-b auto` sucht den USB-Port des Mainboards über VID/PID `1FC9:80F7` (LPC1766-CDC; auf diesem PC genau ein
  Treffer: COM13 = Board 26SC9983 am USB). Die USB-Beschreibung enthält keine echte Seriennummer (Windows vergibt
  `7&...`-IDs je Port), der Port verrät also nur "ein Mainboard", nicht welches.
- Welches Board an welchem Port hängt, ermittelt `verify_device_ports()`: je Gerät kurz per BLE verbinden und abonnieren,
  die Firmware gibt dann das Banner (`Viasis 3003M ... PIN =`) auf RS232 bzw. USB aus; der Port mit dem Banner gehört zum
  Gerät. Bei vertauschter Reihenfolge wird die Geräteliste getauscht, bei Mehrdeutigkeit bleibt sie (mit Warnung).
  Abschaltbar mit `--no-verify-ports`.

**Erster Lauf `soak --devices` (2026-10-19 15:22, 99SC9923 + 26SC9983):** Ergebnis: Board A (99SC9923) lief, **Board B (26SC9983) ging
nicht**: BLE-Verbindung und Subscribe ok (`connect_succeeded`, `notify_subscribed`), aber kein Banner/`PIN = ` und kein
Hauptmenue (`buffered_bytes: 0`); sein USB-Mitschnitt (COM13) lieferte in der ganzen Zeit **0 Zeilen/0 Byte**. Die Reihe brach
beim Flash-Schreiben von B ab. Erstes Live-Ergebnis von **5.119 auf A**: `[BT-Event] @E,0035,C,C=01,A=...,T=00,I=0006,L=0000,
O=0064,B=00` (Verbindungsintervall 6 x 1,25 = **7,5 ms**, Latenz 0, Supervision-Timeout 0x64 x 10 ms = **1 s**) und beim Trennen
`@E,0010,DIS,C=01,R=0913` (Gegenstelle hat getrennt) sowie `[BT-Sitzung beendet] TX=8392 ctswait=1 ctsDrop=0`.
Skript danach angepasst: Portzuordnung wartet bis 10 s auf Banner **oder** Verbindungs-Event und loggt die Characteristic;
faellt ein Geraet beim Flash-Schreiben aus, laeuft die Reihe mit den uebrigen weiter (`ausgefallen` im Bericht).
Offen: warum B kein Banner sendet (Firmware-Version, USB-Terminal, Bluetooth-Einstellung) - noch ungeklaert.


## Update 5.120: BT-Eingabe-Weiche `BT_CONN_GPIO` aus - CONNECTION wird nicht mehr für die Funktion gebraucht

**Anlass (2026-10-19):** Auf Board 26SC9983 (ohne P0.19-Draht) liess sich die PIN per Bluetooth nicht tippen. Ursache im Code:
Seit 5.110 geht der UART1-Empfang nur dann in den Menüpuffer `rbuf`, wenn `BT_LINK` gesetzt ist **und** P0.19 (CONNECTION)
LOW ist (`hard.c`, `BT_CONN_ACTIVE`). Ohne Draht liest P0.19 durch den Pull-up dauerhaft HIGH: jede BT-Taste landete in
`rbuf1` und wurde verworfen. Die Aussage "auf einem unveränderten Board funktional identisch zur Hauptlinie" stimmte seit 5.110 nicht.

**Entscheidung (Anwender):** CONNECTION wird nicht mehr gebraucht. `BT_LINK` bleibt allein an `CYSPP` (Stufe B war widerlegt, siehe
Testplan CONNECTION), `CONNECTION` taugt nur noch als Diagnose.
**Änderung:** `BT_CONN_GPIO` von 1 auf 0 (`btio.h`), Version 5.120. Build 0 Error(s), 0 Warning(s). **Am Gerät nicht getestet.**

**Offene Prüfung:** Die Weiche war die Gegenmaßnahme für "Stray-Zeichen nach dem Trennen" (Event-Geschwätz des Moduls landete in der
Lücke zwischen Pegelwechsel und Softwareerkennung im Menüpuffer). Ob das ohne Weiche zurückkehrt, ist noch nicht gemessen. Prüfung am
verdrahteten Board 99SC9923 mit RS232-Mitschnitt: `--mode interleave` (oder `cycle`) laufen lassen und das Trace mit
`Doku/tools/trace_stray_check.py` prüfen (meldet Menütitel ohne Navigation und Steuerzeichen auf dem Terminal in Phasen, in denen
der Test nicht navigiert). **Vergleichswert mit Weiche (5.116-5.119):** die Zyklen-Phasen der Suite-Läufe `2026-10-15_10-00-52`,
`2026-10-19_08-23-08` und `2026-10-19_10-04-59` (zusammen 733.922 RS232-Zeilen) ergeben **0 Auffälligkeiten**. Kehren die Zeichen
zurück, braucht es eine Weiche ohne P0.19 (z. B. Verwerfen der Bytes in einem kurzen Fenster nach dem Trennen).
Werkzeug neu: `Doku/tools/trace_stray_check.py` (nur lesend).


## Update 5.121: USB-Ausgabe blockiert RS232/Bluetooth nicht mehr, wenn der Host den USB-Port nicht liest

**Symptom/Ursache:** Steckt ein USB-Kabel und liest der PC den USB-COM-Port nicht (kein Terminal offen), wartete `putb()` (sio.c) bei
**jedem** Byte bis zu `USB_WAIT_MS` = 50 ms auf Platz im CDC-Puffer. `putb()` sendet jedes Byte nacheinander an RS232, UART1 (Bluetooth)
und USB, die gesamte Ausgabe lief dadurch auf ~20 Byte/s (Menü ~1 Zeile/s, BLE-Banner 70 Byte >= 3,5 s, Ymodem praktisch unmöglich).
Zusätzlich hängt mit USB am Gerät die GSM/LTE-Aufgabenplanung (`measure.c:203`) und beim Anstecken wird MQTT geschlossen (`measure.c:289`)
- beides gewollt, nur dokumentiert.

**Fix (5.121, sio.c):** Nach dem ersten Timeout merkt sich `usb_stalled`, dass der Host nicht liest; dann gibt es je Byte nur noch **einen**
Versuch ohne Warten (Byte wird bei vollem Puffer verworfen). Gelingt `USBD_CDC_ACM_PutChar` wieder, ist der Stau aufgehoben. Wirkt nur auf
die USB-Ausgabe; RS232 und Bluetooth bleiben unberührt. Build 0 Error(s), 0 Warning(s). **Am Gerät nicht getestet** (Test: USB stecken,
kein Terminal öffnen, per BLE verbinden - Banner und PIN-Prompt müssen sofort erscheinen).


## Update 5.122: USB-Empfangsdienst pollt nicht mehr im Dauer-Yield

**Befund (Codeanalyse, nicht gemessen):** `USB_HostService()` (USB_tools.c) fragt solange VBUS anliegt in einer Schleife nach Daten und rief bei
leerem Puffer nur `osThreadYield()` auf - der Thread wird dadurch nie blockiert, sondern ist immer "ready". Mit Round-Robin (1 ms Zeitscheibe,
`OS_ROBIN`/`OS_ROBINTOUT` in `RTX_Conf_CM.c`) bekommt er in jeder Runde einen Kontextwechsel, solange USB steckt (kleine, aber dauernde
Rechenzeit-Abgabe, relevant vor allem bei langen BT-Downloads). Strom spart ein Wechsel auf `osDelay` **nicht**: der Idle-Thread
(`os_idle_demon`, RTX_Conf_CM.c) ist eine leere Schleife ohne `__WFI()`, die CPU schläft nie, auch ohne USB.
**Fix:** Bei leerem USB-Empfangspuffer `osDelay(1)` statt `osThreadYield()` (Empfangslatenz <= 1 ms; der Puffer-voll-Zweig ist unverändert).
Build 0 Error(s), 0 Warning(s). **Am Gerät nicht getestet** (zu prüfen: USB-Terminaleingabe fühlt sich unverändert an; BT-Download mit
gestecktem USB nicht langsamer).
**Offen / optional:** `__WFI()` im Idle-Thread würde das Gerät insgesamt sparsamer machen (Wecken über SysTick im 1-ms-Takt), ändert aber
das Zeitverhalten des ganzen Systems und gehört getrennt bewertet und getestet.

**Prüfung am Gerät 2026-10-20 (Anwender, 5.121/5.122):** (1) USB gesteckt ohne geöffnetes Terminal, Bluetooth-Verbindung: Banner und PIN-Aufforderung
erscheinen ohne Verzögerung - **bestanden**. (2) USB-Terminal: Eingabe fühlt sich unverändert an - **bestanden**. (3) BT-Download bei gestecktem USB
im Vergleich zu ohne USB: noch nicht geprüft (später).


## Update 5.123: USB ist in der Werkseinstellung aktiv

**Anlass:** Nach einer Werkinitialisierung war USB aus (`fp.usb=0` in `werkparameter()`), man musste in der Konfiguration jedes Mal "USB (J/N)?" mit `J`
beantworten, um per USB verbinden zu können.
**Änderung:** `werkparameter()` (flash.c) setzt `fp.usb=1`. Die Abfrage in der Konfiguration und das Abschalten (Menü, MQTT-Befehl 199) bleiben unverändert.
**Wo die Voreinstellungen liegen:** `werkparameter()` (flash.c:266) setzt den Werkszustand inkl. installierter Schnittstellen (BT/GSM/GPS/USB); sie läuft bei falscher
Parameterkennung (`!retry`, z. B. neuer Flash oder geänderter Parameterblock) oder über das Menü (`InitParameter(1)`, sicom.c:807). `default_parameter()`
(flash.c:335, "Reset auf Standardparameter", `InitParameter(2)` über `X` im Menü) setzt nur überschreibbare Parameter (Schaltschwellen, Zeitpläne usw.) und
lässt die Schnittstellen unberührt. Konstanten `Def_*` stehen in `hard.h`. **Gilt nur für neu initialisierte Geräte:** Bei bereits konfigurierten Geräten bleibt der
gespeicherte Wert von `fp.usb` bestehen (einmal in der Konfiguration auf `J` stellen). Build 0 Error(s), 0 Warning(s); am Gerät nicht getestet.


## Update 5.124: USB_LINK erst setzen, wenn der Host das Gerät eingerichtet hat (Ladegerät/Powerbank sperrt LTE nicht mehr)

**Befund (Codeanalyse, nicht am Gerät nachgestellt):** `USB_HostService()` (USB_tools.c) setzte `connect|=USB_LINK` unmittelbar nach `USBD_Connect()` und wartete dann
mit `while (USBD_Configured(0)==0) osDelay(10);` **ohne Ausstieg** auf die Konfiguration durch den Host. Wurde das Kabel vor Abschluss der Enumeration gezogen oder
war nur ein Ladegerät/eine Powerbank (VBUS ohne Datenhost) angeschlossen, blieb der Thread dort hängen und `USB_LINK` dauerhaft gesetzt: Die geplanten GSM/LTE-Aufgaben
(`measure.c:203` verlangt "keine Schnittstelle aktiv") starteten bis zum Neustart nicht mehr, MQTT war beim Anstecken bereits geschlossen worden (`measure.c:289`).
**Fix:** Die Wartezeit endet jetzt auch, wenn `VBUS` wegfällt, und `USB_LINK` wird erst gesetzt, wenn `USBD_Configured()` wahr ist (mit der Meldung `T_usbcf`);
ohne Host bleibt `USB_LINK` aus, der Dienst räumt nach `VBUS`-Verlust sauber auf (`USBD_Disconnect`/`Uninitialize`). `online=1` (speedcam) beim Trennen nur noch, wenn
USB wirklich verbunden war (`usb_linked`). Eine Zeitgrenze (z. B. 10 s) habe ich bewusst nicht eingebaut: Sie würde bei angeschlossenem Ladegerät den USB-Treiber
dauernd neu starten (Init/Connect/Timeout/Uninit im Takt), ohne dass jemand etwas davon hat; das Warten ohne Link kostet nichts.
Build 0 Error(s), 0 Warning(s). **Am Gerät nicht getestet.** Zu prüfen: (1) PC-USB-Verbindung funktioniert wie vorher (Terminal, Banner, Menü); (2) Ladegerät/Powerbank
ohne Daten anstecken - `USB` darf `connect` nicht setzen, LTE-Aufgaben laufen weiter; (3) Kabel während der Enumeration ziehen und neu stecken.


## Prüfrunde mit Firmware 5.124 (2026-10-20): beide Boards, Zwei-Geräte-Vergleich

**Schritt 1 - Board A (99SC9923), `--mode interleave --rounds 15`:** 30 Zyklen (15x Classic, 15x BLE im Wechsel) alle bestanden, Übergänge "BLE nach Classic" 15/15 und
"Classic nach BLE" 14/14; `trace_stray_check.py` auf 391 RS232-Zeilen: **0 Auffälligkeiten** (BT_CONN_GPIO ist seit 5.120 aus). Alle Sitzungszeilen `ctsDrop=0`.
Vorbehalt: nur 30 Trennvorgänge; mit abgeschalteter Weiche fehlen die `DIS`-Zeilen im Trace (die Event-Bytes landen beim Trennen im Brückenpuffer und werden von der
Firmware verworfen) - der Trenngrund ist dadurch nicht mehr sichtbar, das `C`-Event schon. In diesem Lauf wählte Windows bei 13 von 15 BLE-Verbindungen 7,5 ms Intervall
(Timeout 1 s), bei 2 von 15 52,5 ms (Timeout 9,6 s).
**Schritt 2 - Board B (26SC9983):** Der Fehler vom 19.10. (BLE-Verbindung steht, aber kein Banner) ist **weg**: BLE-Smoke bestanden (Banner, PIN akzeptiert, 8 s gehalten);
PIN von Hand per Classic-Terminal (Viagraph, COM8) getippt - Sternchen und Hauptmenü erscheinen, die Eingabe erreicht also die PIN-Abfrage. Ursache des früheren Fehlers
nicht sicher geklärt (unbekannte Vorversion auf B und/oder abgeschaltete Weiche).
**Schritt 3 - `soak --devices VIASIS_99SC9923,VIASIS_26SC9983 --rs232-port-b auto`, 9 Downloads, alle bestanden, USB-Mitschnitt von B (COM13) lieferte Zeilen:**

| Gerät | Transport | Rate | ctswait je ~500 KB | Verbindungsparameter (C-Event) |
|---|---|---|---|---|
| 99SC9923 | BLE | 6,6-6,7 KB/s | 0 | Intervall 7,5 ms, Timeout 1 s |
| 99SC9923 | Classic | 7,2-7,4 KB/s | 168-178 | - |
| 26SC9983 | **BLE** | **3,06-3,13 KB/s** (Langsam-Erkennung brach den 1. Versuch nach 60 s ab, 2. Versuch lief durch) | **1427-1445** (lost_ms ~48 s) | **Intervall 60 ms, Timeout 9,6 s** |
| 26SC9983 | Classic | 6,9-7,5 KB/s | 114-178 | - |

**Befund:** Gleicher PC, gleiche Zeit, im Wechsel gemessen: nur **B über BLE** ist halb so schnell. Classic ist auf B normal. Das BLE-Verbindungsintervall war bei B 60 ms
gegenüber 7,5 ms bei A (gemessen für die jeweils erste Verbindung; bei den späteren Sitzungen fehlten die `C`-Zeilen im Trace, Grund nicht geklärt). Das erklärt die
Kette: langes Intervall -> weniger Pakete je Sekunde -> das Modul-Sendepuffer läuft voll -> Modul bremst per CTS (1427x) -> halbe Rate. Die Firmware setzt keine bevorzugten
Verbindungsparameter (`/RLH,H=0007` liefert auf beiden Boards Nullen); das Intervall wählt der PC. **Vermutung (nicht geprüft):** Windows verwendet für B einen
zwischengespeicherten oder anders ausgehandelten Parameter (B ist das Board der PPCP-Versuche 5.6x). **Gegenprobe:** Gerät `VIASIS_26SC9983_BLE` in den Windows-
Bluetooth-Einstellungen entfernen, neu koppeln, B-BLE-Download wiederholen. **Hinweis zum Skript:** Die Spalte `rssi` in `soak.csv` ist nach der ersten Suche je Gerät
veraltet (beide Boards zeigen -57); die Langsam-Erkennung (5.-Update im Skript) hat im echten Lauf funktioniert.


## Testskript: Geräteidentifikation als erster Schritt jedes Tests (2026-10-20)

**Ziel:** Bei jedem Lauf stehen Firmware, Seriennummer und weitere Gerätedaten im Bericht, ohne dass man danach fragen muss; wichtig für Vergleiche zwischen Boards und
Firmware-Ständen (z. B. B lief am 19.10. mit unbekannter Version).
**Ablauf (`read_device_info()` in `ble_mainboard_test.py`):** `H` (Hauptmenü) -> warten auf `1...6?` -> `6` (Information und Zeit) -> warten auf `1...9?` -> Text sichern -> `9`
(zurück) -> warten auf `1...6?`. Die Menüs werden sprachunabhängig an den Auswahlbereichen `1...6?`/`1...9?` erkannt (die Anzeigetexte gibt es in mehreren Sprachen).
**Kanäle:** Alles, was einen Eingangsstrom (`.rx`) und `write()` hat: Bluetooth-Transporte (BLE/Classic, mit PIN über `authenticate_or_sync_main_menu`) und **RS232/USB**
über den ohnehin laufenden Mitschnitt-Thread (`Rs232Sniffer.attach/write/detach`, `SnifferChannel`), also ohne den Port ein zweites Mal zu öffnen.
**Auswertung (`parse_info_screen()`):** Modell, Firmware, Hardware-Revision, Seriennummer (`\d{2}[A-Z]{2}\d{4}`), Betriebsstunden, aktiver Parametersatz, Schnittstellen, Datum,
Uhrzeit, Zeitzone, Zeitquelle, "Automatische Zeit"; alles optional, der **Rohtext bleibt im Bericht** (`device_info`).
**Prüfungen/Warnungen (Trace + Log):** Firmware gegen `--expect-fw`; Seriennummer gegen den Zielnamen (`--name VIASIS_<serno>`); Geräteuhr weicht mehr als 10 min von der PC-Uhr
ab (Hinweis auf unzuverlässige Zeitstempel); Info-Seite nicht lesbar.
**Einbau:** `main()` ruft `preflight_identify()` vor jedem Modus auf (`--via auto|rs232|bt`; auto = RS232/USB, wenn der Mitschnitt-Port aktiv ist, sonst Bluetooth;
`--no-identify` schaltet es ab). Das Ergebnis steht in `DEVICE_INFO`, im Trace (`device_info`), in den JSON-Gesamtberichten (`device_info`) und in der Kopfzeile des HTML-Berichts
("Gerät A: Seriennr. ..., Firmware ..., Betriebsstunden, Geräteuhr ..."). Neuer Modus `--mode info` gibt die Seite aus und schreibt `device-info.json`.
**Zwei Geräte (`soak --devices`):** Board A wird über RS232, Board B über USB identifiziert; die Seriennummern ordnen Port und Gerätename **eindeutig und ohne Bluetooth** zu (Reihenfolge
wird bei Bedarf getauscht); nur wenn das nicht eindeutig ist, greift das alte Banner-Verfahren.
**Stand:** Parser und Ablauf mit nachgebildeten Info-Seiten (Deutsch/Englisch), stummem Gerät und simulierten Zwei-Geräte-Läufen getestet. **Der echte Text der Info-Seite ist noch nicht
mit dem Gerät abgeglichen** (Format aus dem Quelltext `infomenu()` abgeleitet): einmal `--mode info` laufen lassen und die Ausgabe prüfen/schicken, dann werden die Muster nachgezogen.

**Abgleich mit dem echten Gerät (2026-10-20, Board 99SC9923 über RS232 COM3, 5.124):** `--mode info` lief fehlerfrei (ca. 1 s). Die Info-Seite sieht so aus (Fixture:
`Doku/tools/testdata/info_screen_5.124_99SC9923.txt`): "Viasis 3003M - Version 5.124, **J2**" (Hardware-Revision hinter einem **Komma**, nicht Doppelpunkt - Muster angepasst), "Seriennummer: 99SC9923",
"Betriebsstunden: 108 h", "Aktiver Parametersatz: 1", "Schnittstellen: Bluetooth, USB", "3. Datum: 14.10.2026, Mittwoch", "4. Zeit: 17:35:25", "5. Zeitzone: 1", "Zeitquelle: ungestellt",
"8. Automatische Zeit: ein". Der Parser liefert damit alle Felder fehlerfrei. **Befund:** Die Uhr dieses Boards ist ungestellt (Zeitquelle "ungestellt", Anzeige 14.10.2026 17:35:25,
-136,5 h gegenüber dem PC) - die Warnung "Geräteuhr weicht ab" greift. Noch nicht am Gerät geprüft: Identifikation über USB (`--rs232-port COM13`) und über Bluetooth (`--via bt`).

**Nachtrag 2026-10-20: Schnittstellen/Bluetooth-Modul in der Geräteidentifikation** (`ble_mainboard_test.py`). Die Schnittstellenliste der Info-Seite (z. B. "Bluetooth, USB") war schon enthalten.
Neu: Ist Bluetooth installiert, liest die Identifikation über RS232/USB zusätzlich **Hauptmenü -> 5 Funkmodems -> 1 Bluetooth Modem -> 1 Information** (ca. 10 s, EZ-Serial-Befehle an das IF820)
und wertet sie aus (`parse_bt_info()`): Modul, **Modul-Firmware** (`/QFV E=01040C0C` -> 1.4.12.12), Stack (3.1.0.0), Protokoll, Hardware (F1), Bluetooth-Adresse, BLE-/Classic-Name, UART-Baudrate,
CYSPP-Modus, Classic-Supervision-Timeout (`GBTP T=7D00` = 20 s), Sniff-Modus. Ergebnis im Trace (`bt_module_info`), im JSON-Bericht (`device_info[...].bt_modul`, mit Rohtext) und in der HTML-Kopfzeile
("BT-Modul A: IF820 FW 1.4.12.12 ..."). Optionen: `--expect-module-fw 1.4.12.12` (Warnung bei Abweichung), `--no-bt-module-info` (überspringen). **Grenzen:** Nur über RS232/USB möglich, denn das Bluetooth-Menü ist
bei aktiver BT-Verbindung gesperrt (`sicom.c` Fall 0x0051, `T_noacc`) - über Bluetooth steht dann "nicht zugänglich". Das Mobilfunk-Modem hat im Gerät **kein** Informationsmenü (nur Zeitplan, PIN, Server,
APN/SMS/SMTP), seine Version ist daher nicht lesbar. Fixture: `Doku/tools/testdata/bt_info_5.124_26SC9983.txt` (echte Ausgabe von Board B). Parser und Menüführung mit dieser echten Ausgabe und einer
simulierten Menüführung getestet (inkl. gesperrtem BT-Menü); die Navigation selbst am Gerät noch nicht gelaufen.

**Lauf am Gerät (2026-10-20 10:15, Board 99SC9923, `--mode info`, 8 s):** Info-Seite und Bluetooth-Modul vollständig gelesen: **IF820, Modul-Firmware 1.4.12.12, Stack 3.1.0.0, Protokoll 1.4, Hardware F1, Adresse
CD:49:1B:93:00:5F** (stimmt mit der Windows-Adresse des Boards überein), Namen `VIASIS_99SC9923_BLE`/`VIASIS_99SC9923`, 460800 Baud, CYSPP-Modus 02, Classic-Supervision 20 s, Sniff aus. Die Geräteuhr
läuft (12:25 min Anzeige gegen 12:25 min Realzeit seit dem ersten Lauf), ist aber ungestellt ("Zeitquelle: ungestellt", Datum 14.10.2026).
**Ziel-Abgleich (neu):** `check_target_address()` vergleicht die Bluetooth-Adresse jedes BLE-/Classic-Verbindungsziels mit den Adressen der über RS232/USB identifizierten Module und warnt, wenn das Ziel zu keinem davon
gehört (Test und Mitschnitt würden sich dann auf verschiedene Boards beziehen, z. B. bei mehreren VIASIS-Geräten in Reichweite). Trace: `target_address_ok` / `target_address_unknown`.

**Nachtrag 2026-10-20: COM-Port-Zuordnung und PC-Bluetooth im Bericht** (`ble_mainboard_test.py`; auf diesem PC real geprüft):
- **COM-Port-Übersicht** (`build_com_overview()`, Log + Trace `com_ports` + JSON `com_ports` + HTML-Kopfzeile "COM-Ports"): Quelle sind pyserial (USB-Kennung, Bluetooth-Adresse im Hardware-Pfad der SPP-Ports) und die identifizierten
  Geräte (Terminal-Port, Modul-Adresse); Namen nicht identifizierter Bluetooth-Geräte kommen aus den Windows-Geräten (PowerShell, nur lesend). Beispiel dieses PCs: COM3 = USB-RS232-Adapter (Prolific) -> RS232-Terminal Board 99SC9923;
  COM13 = USB-Anschluss Mainboard (LPC1766, 1FC9:80F7) -> USB-Terminal Board 26SC9983; COM11 = Classic Board 99SC9923 (`CD491B93005F`); COM8 = Classic Board 26SC9983 (`FF4F7F9EEEBA`); COM6 = VIASIS_26SC9994; COM10 = VIASIS_12SC1234;
  COM5/7/9/12 = ausgehende Bluetooth-Ports, ungenutzt.
- **Auf welchem Bluetooth läuft der Test (PC-Seite):** `pc_bluetooth_adapter()` liest Adapter, Hersteller, Treiberversion/-datum, USB-Kennung, Windows-Stack und Dienststatus (Trace `pc_bluetooth`, JSON `pc_bluetooth`, HTML "PC-Bluetooth").
  Dieser PC: **Intel(R) Wireless Bluetooth(R), Intel Corporation, Treiber 24.50.0.4 vom 2026-08-05, USB 8087:0033, Windows-Stack 10.0.26100.9711, Dienst bthserv läuft**.

**Korrektur 2026-10-20 (Anwenderwunsch): Testbericht zeigt nur, welche Ports der TEST benutzt hat.** Die vollständige COM-Port-Übersicht des PCs und das PC-Bluetooth sind "nice to know" und stehen nicht mehr im Testlog/-bericht
(das PC-Bluetooth nur noch als Trace-Ereignis `pc_bluetooth`, die Übersicht nur in `--mode info`). Neu: `register_port()` merkt sich beim Benutzen eines Ports, wofür er diente - **RS232-/USB-Terminal** (Mitschnitt der
Firmware-Ausgabe, nach der Identifikation mit Seriennummer und Firmware: "COM3: ... Board 99SC9923 (Firmware 5.124, Gerät A)"), **Bluetooth-Classic-Verbindung** (COM-Port und Zielgerät), **Bluetooth-LE-Verbindung** (Adresse und Name).
Ausgabe: Log am Laufende ("Vom Test benutzte Ports"), Trace `ports_used`, JSON-Gesamtbericht `ports_used`, HTML-Kopfzeile "Verwendete Ports".
**Kleines Hilfsprogramm `Doku/tools/comports.bat`** (Doppelklick, nur lesend, ohne Python): listet alle COM-Ports mit Art und Verbindung (Bluetooth-Ports mit Gerätename und Adresse, USB-Mainboard, RS232-Adapter,
ungenutzte ausgehende Ports).

**Rückschritt 2026-10-20 10:30 (Zwei-Geräte-Lauf `soak`):** Direkt nach der Geräteidentifikation inkl. **Bluetooth-Modul-Abfrage** (`Funkmodems -> Bluetooth -> Information`, 5.124 auf beiden Boards) kam bei **beiden** Boards über BLE
kein Banner mehr (Verbindung und Subscribe ok, `buffered_bytes: 0`), das Flash-Schreiben schlug auf beiden fehl (Phase 1 A -> aus der Reihe genommen, Phase 2 B -> Abbruch). Identifikation, Seriennummern, Port-Zuordnung (COM3/COM13) und
Ziel-Abgleich (`target_address_ok`) liefen dabei fehlerfrei. **Ursache nicht gefunden:** `send_bt_info()` (btio.c) lässt CYSPP laut Code unberührt (`bt_command` ruft `bt_cmdmode()` nicht auf, `bt_cyspp_out` bleibt 0) und gibt UART1
am Ende frei; trotzdem ist das der einzige Unterschied zu den erfolgreichen Läufen davor. **Maßnahme:** Die Modul-Abfrage ist in Tests jetzt **aus** (`--bt-module-info` schaltet sie ein; im Modus `info` bleibt sie an,
`--no-bt-module-info` schaltet sie dort ab). **Zu klären (A/B-Test):** (1) `soak --devices ...` ohne Modul-Abfrage -> läuft BLE wieder? (2) `--mode info` (mit Modul-Abfrage), danach `--mode smoke --no-identify` -> fehlt das Banner?

**Lauf 2026-10-20 10:58 (`soak --devices ...`, Modul-Abfrage aus):** Gleiches Fehlerbild auf **beiden** Boards (BLE verbunden und abonniert, kein Banner, A: "kein PIN-Prompt", B: "Not connected"); auch die `[BT-Event] C`-Zeilen fehlen,
die Firmware hat die Verbindung also gar nicht erkannt. Identifikation (Info-Seite über RS232/USB, Seriennummern, Port-Zuordnung) lief wieder fehlerfrei. **Damit ist die Modul-Abfrage im Lauf selbst nicht der Auslöser - aber die Boards waren
seit den Abfragen (A: 10:15 und 10:30, B: 10:30) nicht neu gestartet:** ein dauerhaft hängender Zustand nach `send_bt_info()` bleibt möglich. **Zu prüfen (Boards vorher aus-/einschalten):** (1) `--mode smoke --no-identify`: läuft BLE?
(2) `--mode smoke` mit Identifikation (nur Info-Seite): läuft BLE? (3) `--mode info` (mit Modul-Abfrage), danach `--mode smoke --no-identify`: fehlt das Banner?

**Lauf 2026-10-20 12:42 (`--mode smoke --name VIASIS_99SC9923`, mit Identifikation, Board A nach Aus-/Einschalten; die Uhr stand danach auf 14.10.2026 16:23:39, war also neu gestartet):** BLE verbindet (RSSI -54 dBm), Subscribe
ok, **kein Banner, nach 5,5 s trennt das Gerät** ("DISCONNECT vom Gerät ausgelöst", Ergebnis unstable). Das ist **nach einem Neustart** und nur mit der Info-Seite (ohne Modul-Abfrage) passiert - die Kontrolle ohne Identifikation (`--no-identify`)
fehlt noch (der erste Versuch war ein Tippfehler im Befehl). **Skript:** Die RS232-Vorprüfung meldete "COM3 ist offen, aber keine Daten", weil eine ruhige Firmware nichts von selbst sendet; sie sendet jetzt nach 2 s Stille
einmal `H` (Hauptmenü, harmlos) und wartet auf die Antwort. Das "offen" in der Meldung heißt "geöffnet", nicht "belegt"; der Prozess wartete lediglich auf die Eingabe [R]etry/[S]/[W]/[A]bbrechen und hielt den Port dabei.

**Läufe 2026-10-20 12:45 und 12:46 (Board A, beide `--mode smoke --no-identify`, also ohne jede Identifikation):** 12:45:46 **bestanden** (`[BT-Event] C ... I=0006`, Banner, PIN, Hauptmenü, 8 s gehalten), 12:46:54
(nur 1 Minute später, gleicher Befehl) **unstable**: verbunden, Subscribe ok, kein Banner, kein `C`-Event, nach 5,5 s trennt das Gerät. Damit sind **Identifikation und Modul-Abfrage als Auslöser widerlegt**: gleicher Aufruf, einmal gut, einmal schlecht.
Chronik: bis 09:40 alles bestanden (Interleave 30/30, Zwei-Geräte-Messreihe, B-Smoke); danach bei beiden Boards überwiegend kein Banner (10:29 A und B, 10:58 A und B, 12:42 A, 12:46 A), eine Ausnahme (12:45 A).
Offen: **intermittierend, beide Boards betroffen -> Verdacht PC-/Windows-Bluetooth-Zustand** (am 19.10. morgens ähnlich), nicht Firmware (unverändert 5.124) und nicht die Boards (Classic und BLE liefen zuvor stundenlang fehlerfrei).
Trennversuche: BLE auf B, Classic auf A, danach Bluetooth am PC aus/ein.

## Diskriminierungstest BLE-Intermittenz (2026-10-20, 12:51-12:56, 5.124, ohne Identifikation)

Ziel: klaeren, ob der intermittierende BLE-Fehler ("verbunden + Subscribe ok, kein Banner, Geraet trennt nach ca. 5,5 s") am PC, an den Boards oder an der Firmware liegt.

| Lauf | Board | Modus | Ergebnis |
|---|---|---|---|
| 12:51 | B 26SC9983 | BLE smoke | unstable (kein PIN-Prompt, Trennung nach ca. 5,5 s) |
| 12:52 | A 99SC9923 | Classic smoke | bestanden |
| 12:52-12:54 | A 99SC9923 | BLE cycle, 6 Zyklen (hold 3 s, cooldown 5 s) | 6/6 bestanden |
| 12:54 | A 99SC9923 | BLE smoke | unstable (kein PIN-Prompt, Trennung nach ca. 5,5 s) |
| 12:55 | A 99SC9923 | BLE smoke | bestanden (Banner, PIN, Hauptmenue) |

Befund: Classic stabil; BLE fehlerhaft nur im smoke-Modus, und zwar auf beiden Boards intermittierend (A: 1 von 2). Der cycle-Modus lief direkt dazwischen 6/6. Fehler haengt damit weder an einem Board noch an der Identifikation noch an der Firmware-Version. Verdacht: PC-BLE-Stack / Verbindungsaufbau im smoke-Modus (jeweils neuer 15-s-Scan und neuer BleakClient, gegenueber cycle mit wiederverwendeter Adresse). Offen: Bluetooth-Funk am PC aus/ein (Systemeinstellung, muss der Anwender machen); smoke mehrfach wiederholen und mit cycle vergleichen.

### Nachtrag: Bluetooth-Funk aus/ein und Smoke-Wiederholung (2026-10-20, 14:14-14:32)

- Bluetooth-Funk am PC aus/ein (durch den Anwender) behebt den Fehler nicht: danach smoke an Board A 1x unstable (Verbindungsaufbau 2261 ms), 2x bestanden (1179/1430 ms).
- `ble_mainboard_test.py --mode smoke` wiederholt jetzt einmal, wenn der Erstversuch ohne PIN-Prompt scheitert und der Verbindungsaufbau > 2 s dauerte (`SMOKE_SLOW_CONNECT_MS`, Meldung "Bekanntes PC-BLE-Verhalten"). Scheitert auch die Wiederholung, gilt es als echter Fehler.
- Kontrolllaeufe mit Wiederholung (4 Laeufe): Lauf 1 Erstversuch 3583 ms unstable, Wiederholung (1711 ms) ebenfalls unstable; Lauf 2 Erstversuch 2315 ms unstable, Wiederholung (4172 ms) bestanden; Lauf 3 und 4 sofort bestanden (2136/1591 ms).
- **Korrektur der Hypothese:** Der Zusammenhang "langsamer Aufbau = Fehlschlag" ist nur eine Tendenz, kein Beweis (Wiederholung scheiterte bei 1711 ms, ein Lauf mit 4172 ms bestand). Die Fehlerrate im smoke-Modus liegt bei ca. 40-50 %, im cycle-Modus bei 0/6. Ursache weiter offen; die Wiederholung ist ein Workaround im Test, keine Erklaerung. Naechster Schritt: Windows-Eintrag des Geraets entfernen und neu koppeln; ggf. smoke mit wiederverwendeter Adresse (`--address`) gegen smoke mit Scan vergleichen.

### Lauf 2026-10-21 (module-suite, 26SC9983): BLE Download 2/3 und 3/3 durch Standby/Dock gescheitert

- Classic: 12 Zyklen + 3 Downloads bestanden (7,0-7,3 KB/s); BLE: 12 Zyklen + Flash + Download 1/3 bestanden (6,7 KB/s), Retries 0.
- BLE Download 2/3 brach 10:52:30 ab: `rs232_error` (USB-RS232-Adapter an der Docking-Station verloren), 10:52:33-10:52:58 moderner Standby nach Win+L; Download 3/3 startete mitten im Standby (29 s, Timeout). Ursache Testumgebung (Dock-USB/Standby), kein Befund fuer Firmware oder Modul. Das bestehende Wachhalten (`ES_SYSTEM_REQUIRED`) verhinderte den modernen Standby nach dem Sperren nicht.
- Testskript: Bildschirm-Anforderung zusaetzlich (`ES_DISPLAY_REQUIRED`), Hintergrundwaechter fuer Zeitspruenge > 8 s (PC stand), verlorener RS232-/USB-Port meldet `env_disturbance`; Phasen, die waehrend einer Stoerung scheitern, erhalten den Status `environment` (orange, Gesamtergebnis `environment` statt `failed`, wenn nur solche Phasen fehlen).
- Anleitung fuer die PC-Einstellungen (Win11): `Doku/Win11_PC_Einstellungen_Dauertest.md`.
- Offen: Lauf wiederholen nach Umstellung; die smoke-Fehler vom 2026-10-20 (Tageszeit, PC aktiv) sind damit nicht erklaert.

### Lauf 2026-10-21 12:01-14:25 (module-suite, 26SC9983, Wachhalten aktiv): komplett bestanden, BLE langsam

- Gesamtergebnis passed (alle Phasen, 8685 s). `powercfg /requests` zeigte waehrend des Tests `python.exe` unter DISPLAY und SYSTEM (Wachhalten wirkt). Keine Standby-Phasen, kein verlorener Port, 0 Umgebungsstoerungen. (Der Lauf 11:29 ohne `--name` fand kein Classic-Geraet, weil mehrere VIASIS-Geraete gekoppelt sind: `--name` immer angeben.)
- Classic-Downloads 3x ohne Retries (ca. 550-560 s je 4,2 MB, ca. 7,5 KB/s).
- BLE-Downloads 3x bestanden, aber langsam: ca. 3,0-3,1 KB/s, je ~1550 s. Die Langsam-Erkennung brach jeweils nach 61 s ab und wiederholte (3100 B/s < Schwelle 4500).
- Ursache wie am 2026-10-20: In allen 14 `[BT-Event]` C-Ereignissen steht `I=48` (60 ms Verbindungsintervall), `L=0`, `O=960` (9,6 s). Windows waehlt fuer Board B immer 60 ms; Board A bekam 7,5 ms (6,6 KB/s). Das Modul bremst dann per CTS.
- Naechster Schritt: Windows-Eintrag `VIASIS_26SC9983_BLE` entfernen und neu koppeln (Gegenprobe); falls es bei 60 ms bleibt, Modul/Firmware sollte die Verbindungsparameter selbst anfordern (EZ-Serial-Guide: Befehl zum Aktualisieren der Verbindungsparameter pruefen).

### Gegenprobe Board B 2026-10-21 (14:37-14:40) und Loesung per /UCP (EZ-Serial-Guide)

- Classic-Kopplung von 26SC9983 entfernt und neu gekoppelt (BLE ist bei uns nie gekoppelt, im Geraete-Manager steht kein BLE-Eintrag). Danach BLE-smoke 2x bestanden, aber `--mode receive` ble weiterhin 3,1-3,2 KB/s (Langsam-Erkennung nach 61 s). Das lange Intervall (60 ms) liegt also nicht an der Windows-Kopplung von B.
- EZ-Serial-Guide (v2.2, Abschnitt BLE-Durchsatz, Beispiel 16): Als Peripheral bestimmt der Central das Anfangsintervall; der Peripheral kann nach dem Verbinden `/UCP,C=<handle>,I=6,L=0,O=64` (gap_update_conn_parameters) senden, der Central darf annehmen oder ablehnen; Erfolg zeigt das Event `CU` (`@E,...,CU,C=..,I=..`). Alternativ `SCP` (gap_set_conn_parameters) fuer Voreinstellung.
- Naechster Schritt (Firmware, nicht umgesetzt): nach dem `@E,...,C,C=01...`-Event ein `/UCP` mit 7,5-15 ms senden und das CU-Event auswerten. Vorher klaeren: UART1 steht dann im Command-Mode-Zeitfenster (vor dem CCCD-Subscribe), das UCP darf den CYSPP-Pin nicht treiben (nur Kommando ueber UART).

## Update 5.125 (2026-10-21): BLE-Verbindungsintervall per /UCP anfordern

**Symptom:** BLE-Durchsatz haengt am vom PC gewaehlten Verbindungsintervall: Board 26SC9983 immer 60 ms (3,1 KB/s, CTS-Bremse), Board 99SC9923 7,5 ms (6,6 KB/s). Neu-Koppeln aendert nichts (BLE ist nie gekoppelt).

**Aenderung:** `btio.c`/`btio.h`: Im `C`-Event (gap_connected, Command-Mode, vor dem CCCD-Subscribe) wertet `ucp_check()` das Intervall `I=` aus; ist es laenger als `BT_UCP_I` (6 = 7,5 ms), merkt sie `/UCP` vor, und `bt_sniff_poll()` sendet nach der Drain-Schleife einmal `/UCP,C=<handle>,I=6,L=0,O=190` (4 s Supervision-Timeout, nicht 1 s wie im Guide-Beispiel, wegen der frueheren 1-s-Abbrueche). Schutz: nicht senden, wenn `bt_cyspp_out` oder `BT_LINK` gesetzt (Bruecke aktiv, der Befehl kaeme sonst als PIN-Eingabe an). CYSPP-Pin wird nicht angefasst. Weiche `BT_UCP_ENABLE` in `btio.h`. Diagnose: `[BT-Event] /UCP gesendet` und das `CU`-Event (neues Intervall) erscheinen zusaetzlich auf RS232 (temporaer, gehoert zum 5.119-Diagnosecode).

**Pruefen (am Geraet, 26SC9983):** (1) RS232-Zeile `[BT-Event] @E,...,CU,C=..,I=0006` nach dem Verbinden = Windows hat angenommen; kommt kein `CU`, hat Windows abgelehnt. (2) `--mode receive --transport ble`: Durchsatz deutlich ueber 3,1 KB/s. (3) PIN-Eingabe ueber BLE unveraendert (kein `/UCP`-Text im Terminal). (4) Classic unveraendert. Build 0 Fehler, 0 Warnungen; noch nicht geflasht.

## Update 5.126 (2026-10-21): /UCP wieder abgeschaltet (Rollback der Funktion aus 5.125)

**Symptom:** Mit 5.125 auf Board 26SC9983 scheiterten alle 8 BLE-Versuche (14:50-15:02: smoke und cycle): Verbindung steht, kein PIN-Prompt, Geraet trennt nach ca. 5,5 s; im RS232-Mitschnitt keine Zeile `[BT-Event]` (kein C-Event, kein /UCP, kein CU). Classic-smoke bestanden (Modul und UART ok). Mit 5.124 liefen BLE-smoke (14:37/14:38) und module-suite (12:01-14:25) auf demselben Board. Board A war zur Pruefung nicht verfuegbar.

**Aenderung:** `BT_UCP_ENABLE` in `btio.h` auf 0 (der /UCP-Code ist damit ausgeblendet; zusaetzlich bleibt nur die Anzeige des `CU`-Events im 5.119-Diagnosecode). Version 5.126, Build 0 Fehler/0 Warnungen.

**Pruefen:** BLE-smoke/cycle auf B mit 5.126. Laeuft BLE: 5.125 war die Ursache (Code oder Timing durch /UCP) - dann gezielt weiter. Scheitert BLE weiter: Zustand von Board B (nach langem Ausschalten) oder PC, nicht 5.125.

### Vergleichslaeufe Board A (99SC9923), 2026-10-21 16:34-16:44: 5.118 und 5.126

- Firmware 5.118 (`fw_5.118/`, aus Commit 4b213f1 neu gebaut): BLE-smoke 4/4 bestanden (Verbindung 1,1-2,3 s).
- Firmware 5.126 (= 5.124-Verhalten, /UCP aus): BLE-smoke 3/3 bestanden (1,4-2,0 s). Im RS232-Mitschnitt jetzt sichtbar: `C`-Event mit `I=0006` (7,5 ms), kurz danach `CU` mit `I=000C` (15 ms, Windows aktualisiert selbst), Banner mit `Version 5.126`.
- Damit 7/7 auf Board A, waehrend derselbe smoke-Test mit 5.124 am Vormittag/Mittag etwa jeden zweiten Lauf scheiterte. Firmware ist als Ursache des intermittierenden BLE-Fehlers entlastet; geaendert hat sich dazwischen die PC-Seite (Energieeinstellungen, USB-Suspend, Bluetooth-Funk aus/ein, Classic-Neukopplung von 26SC9983; Anleitung `Doku/Win11_PC_Einstellungen_Dauertest.md`). Welche dieser Massnahmen es war, ist nicht getrennt geprueft.
- Board B (26SC9983) mit 5.125: 8/8 BLE-Versuche gescheitert, nie ein `C`-Event auf RS232 (Lauf 14:50-15:02) - mit 5.126 am 16:24 ebenfalls (Versionszeile im Mitschnitt fehlte, Version unbestaetigt). Ursache offen: Board-/Modulzustand oder Windows-Kopplung von B (Classic-Neukopplung 14:39); Gegenprobe Classic-Kopplung entfernen steht aus.
- Naechster Schritt: Board B mit 5.126 flashen, Versionszeile pruefen, smoke 3x; dann ggf. Classic-Kopplung von B entfernen und wiederholen.

### Lauf 2026-10-22 09:27-10:40 (module-suite, Board A 99SC9923, Firmware 5.126)

- Bestanden: Classic Smoke/12 Zyklen/3 Downloads (7,3-7,4 KB/s, Retries 0), BLE Smoke, 12 BLE-Zyklen, Flash schreiben.
- **BLE Download 1/3 gescheitert** nach 307 s: `Not connected` (BLE-Link brach etwa 5 min nach Beginn des Downloads, letzte Daten 10:39:30). Kein PC-Standby, kein verlorener Port, keine Umgebungsstoerung im Trace (`pc_events.json` leer). Trennungsgrund unbekannt (waehrend der Datenbruecke sieht die Firmware kein DIS-Event).
- **BLE Download 2/3 und 3/3:** `Device ... was not found` (je 30 s Verbindungs-Timeout): das Geraet war nicht auffindbar. Auf RS232 lief danach noch die Binaerausgabe der Ymodem-Sendung (rund 185000 Zeilen/14,7 MB im Mitschnitt) und endete 10:40:00 mit `F13010 Kommunikations-Protokollfehler`; erst danach wieder Hauptmenue. Die Firmware bricht die Ymodem-Ausgabe also nach Linkverlust nicht sofort ab und ist dann fuer BLE nicht auffindbar.
- Vergleich: Dauertest 5.118 auf demselben Board am 2026-10-15: 6/6 BLE-Downloads bestanden. Offen, ob 5.126 (5.119-5.124-Aenderungen) oder Zufall/Funk; Gegenprobe: 5.118 auf A flashen und dieselbe module-suite laufen lassen.
- Moegliche Verbesserung (nicht umgesetzt): Ymodem-Senden bei Linkverlust sofort abbrechen, damit das Geraet nicht noch minutenlang ins Leere sendet und BLE wieder auffindbar wird.

## Auswertung aller Testreports (2026-10-23): Belege zur BLE-Ursache

Alle 104 Testreports (01.09.-22.10., 5,7 GB) maschinell ausgewertet (`Doku/tools/trace_events_extract.py`, `Doku/tools/trace_ble_attempts.py`): 387 BLE-Verbindungsversuche mit Board, Firmware, Modus, Ergebnis und Modul-Events (`C`/`CU`/`DIS`, ab 5.119), dazu das Windows-Systemprotokoll seit 01.09. Details, Tabellen und angepasster Plan: `Doku/BLE_Ursachensuche_Plan.md`.

- **"Kein PIN-Prompt" trifft fast nur den ersten Versuch eines Testprozesses (nach dem 15-s-Scan):** Board A erster Versuch 16 von 46 gescheitert (35 %), Folgeversuche direkt auf die Adresse 3 von 190 (2 %, alle 14.10. mit 5.116). Board B 50 % gegen 20 %. Ursache liegt im PC-seitigen Ablauf (neuer Prozess, Scan, erster Connect), nicht an Board oder Firmware-Version (Fehler mit 5.118, 5.124 und 5.126).
- **Bei diesem Fehler meldet das IF820 keine Verbindung:** Board A, smoke ab 5.119: in 10/10 Erfolgen kommt das `C`-Event 0,4-5,4 s nach der Geraeteauswahl und vor dem PIN-Prompt; in 6 von 7 Fehlschlaegen kommt es gar nicht, einmal (20.10. 14:14) erst nach 16,2 s, 6 s nachdem das Skript aufgegeben hatte. Windows meldete jeweils trotzdem "verbunden" und "Subscribe ok". Firmware und Python-Logik nach dem Connect sind damit als Ursache ausgeschlossen; PC gegen Modul trennt nur ein BLE-Sniffer.
- **Download-Abbruch auch mit 5.118:** 19.10. 10:04 (5.118) BLE-Download 2/3 riss bei ca. 96 %, danach "nicht gefunden" und `F13010` - dasselbe Bild wie am 22.10. 09:27 (5.126); der PC war dort 11:19-11:30 im modernen Standby. Ausserdem lief 5.126 am 22.10. 07:51 3/3 BLE-Downloads fehlerfrei (oben nicht erwaehnt). Kein Rueckschritt durch 5.119-5.126. Die geplante Gegenprobe (eine module-suite mit 5.118) kann bei dieser Abbruchrate nichts entscheiden und ist zurueckgestellt.
- **PC-Adapter zeitweise gestoert:** `BTHUSB` Ereignis 3 ("Adapter hat nicht geantwortet") 975-mal, nur am 02.09., 21.-23.09., 14.10. und 22.10. (14-16 Uhr, 67-mal). Erklaert die Laeufe 22.10. 14:40/14:57 ("Geraet nicht gefunden"), nicht die smoke-Fehler vom 20./21.10. und nicht den Abbruch 22.10. 10:39.
- **Neuer Firmware-Befund (offen):** Nach der Phase "Flash schreiben" gibt die Firmware bis zum Neustart keine Modul-Events (`[BT-Event]`) mehr aus (19.10. 12:41-12:44, 20.10. 09:07, 22.10. 07:51-09:27; im Lauf 09:27 kein Event bei 30 erfolgreichen Verbindungen). Die Verbindungen selbst funktionieren. Verdacht: UART1-Command-Mode-Empfang (`uart1_owner`, Routing in `UART1_IRQHandler`, `rbuf1`) - dieselbe Stelle wie der 5.92-Fehler.
- **Testskript:** `smoke.json` meldet bei erfolgreicher Wiederholung `passed`, der gescheiterte Erstversuch steht nur im Trace ("7/7" vom 21.10. waren 7 von 9). Bei Board-B-Laeufen lief der Mitschnitt teils auf COM3 (Board A); `C`-Event-Aussagen gelten daher nur fuer A.
- **Naechste Schritte:** smoke mit `--address` gegen smoke mit Scan (je 20x); Handy (nRF Connect) 20x; BLE-Sniffer (nRF52840) beschaffen; Firmware-Befund nach "Flash schreiben" im Testmenue pruefen; 5.127 (Ymodem bei Linkverlust abbrechen, Trenngrund auch bei aktiver Bruecke erfassen).

## Testskript: automatischer BLE-Funk-Mitschnitt (--sniff), 2026-10-29
- **Neu in `ble_mainboard_test.py` (Option `--sniff`, ohne Option unveraendert):** startet den nRF52840-Sniffer (`nrfutil-ble-sniffer sniff --follow <Zieladresse>`, Port automatisch ueber `nrfutil device list`, hier COM15), schneidet je Einzeltest bzw. je BLE-Phase der Suiten mit und wertet mit `tshark` aus. Ergebnis im Report-Ordner: `sniffer_<label>.pcapng` (nur Zielgeraet) und `.json` (Verbindungsparameter, Parameter-Updates, CCCD-Writes mit Antwortzeit, Notifications, `LL_TERMINATE_IND` mit Sender und Grund, CRC-Fehler, RSSI); dazu Trace-Ereignis `sniffer_summary`, Eintrag `sniffer` im Phasenbericht, Befundzeile im Log. Optionen `--sniff-port`, `--sniff-follow`, `--sniff-keep-raw`. Beschreibung in `Doku/Plan_BLE_Sniffer_nRF52840.md`.
- **Erste Laeufe (smoke gegen Board A, 5.126, 10:12-10:21):** Der Fehler "Verbunden, Subscribe ok, kein PIN-Prompt, Geraet trennt nach ca. 5,5 s" trat bei allen vier Laeufen auf, auch in den Wiederholungen (jeweils 'unstable'); Hinweis: ohne `--name`/`--pin` lief der erste Lauf ohne PIN.
- **Befunde des Sniffers:** `CONNECT_IND` mit Intervall 7,5 ms, Latenz 0, Timeout 1000 ms (Initiator `9c:97:1b:e9:fa:3d`); PC sendet nach 0,18 s `LL_CONNECTION_PARAM_REQ` (7,5-15 ms, Timeout 9600 ms), Modul antwortet `LL_CONNECTION_PARAM_RSP`, danach `LL_CONNECTION_UPDATE_IND` auf 15 ms; Service-Discovery; erster CCCD-Write ist Handle 0x000b mit 0200 (Service Changed, Indicate), sofort beantwortet. Bei ca. 1,9 s sendet der PC erneut `LL_CONNECTION_PARAM_REQ` mit 7,5-7,5 ms und Timeout 1000 ms.
- **Grenzen:** Der Sniffer verliert nach 0,2 bis 2 s den Sync (nach Parameter-Update) und sieht das Trennen nach 5,5 s nicht; der Befund meldet das als "Sync verloren". Er muss mindestens 4 s vor dem Verbinden laufen (`SNIFF_WARMUP_S`), sonst fehlt das `CONNECT_IND` (zwei der ersten Laeufe).
- **Offen:** Sync-Verlust beheben (Dongle naeher/Position, Intervall-Update einzeln beobachten); den Fehler mit vollstaendigem Mitschnitt bis zum Trennen erfassen.

### Sniffer-Befund Board A, 2026-10-29 10:30-10:33 (noch ohne Firmware-Bezug)
- **Funkbild eines fehlgeschlagenen BLE-Laufs (Dauer-Mitschnitt, Verbindung 10:31:05):** Intervall 30 ms; PC fuehrt Service-Discovery durch und schreibt um 06,6 s auf das CCCD Handle 0x0012; das Modul sendet bis ca. 10:31:10,5 (4,9 s nach `CONNECT_IND`) noch leere Pakete, **danach antwortet es nicht mehr**. Der PC sendet weiter bis zum Supervision-Timeout (9,6 s, Ende 10:31:20). Also: Modul-Funk bricht ohne `LL_TERMINATE_IND` ab (Modul haengt/resettet/stoppt), nicht der PC.
- **Der Sniffer verliert in `--sniff`-Laeufen den Sync nicht wegen 2M-PHY** (Dauer-Mitschnitte ohne `--sniff` im Testskript lieferten ganze Verbindungen). Ursache des Sync-Verlusts im Skript-Lauf offen.
- **Wichtig:** In denselben Minuten ist **Board A auf RS232 (COM3, 115200) stumm** (kein Banner, keine Antwort auf 'H'), und **auch Classic (COM12) liefert kein Banner/PIN-Prompt**. Das Modul wirbt und verbindet, aber die Firmware dahinter meldet sich weder ueber BLE noch Classic noch RS232. Verdacht: Board-MCU laeuft nicht (Reset-Schleife/haengt/RS232 nicht angeschlossen). Die Smoke-Fehler von heute sind damit nicht aussagekraeftig, solange das nicht geklaert ist.
- Hinweis: Classic-Adresse von Board A = BLE-Adresse `CD:49:1B:93:00:5F`; `9C:97:1B:E9:FA:3D` ist der PC-Adapter (Plan-Zeile entsprechend zu korrigieren).

## Befund Board A, module-suite mit Sniffer, 2026-10-29 (Firmware 5.126)

**Ergebnis: module-suite komplett `passed`** (11:19 bis 13:00, ca. 100 min): Classic- und BLE-Smoke, Verbindungszyklen, 419430-Messwerte-Flash, je 3 Ymodem-Downloads (4,2 MB) per Classic und BLE, 0 Retries. BLE-Durchsatz 6602 B/s bei Intervall 7,5 ms. Sniffer je BLE-Phase: Verbindung bis Mitschnittende aktiv, keine Trennung durch das Modul.

**Korrekturen zu den Zwischenergebnissen vom selben Tag:**
- Die Serien "8/8 und 10/10 fehlgeschlagen" waren ein Messfehler: mit `--address` leitet das Skript keine PIN ab, `--pin` ist Pflicht (26SC9983 gibt 269983, 99SC9923 gibt 999923).
- Der Ausfall um 10:31 (Modul verstummt nach ca. 4,9 s ohne LL_TERMINATE_IND) trat im "komischen Zustand" des Moduls auf. Der Kippschalter (Power-Cycle) behob ihn. Auslöser weiter unbekannt.
- Adressen Board A: BLE und Classic CD:49:1B:93:00:5F; 9C:97:1B:E9:FA:3D ist der PC-Adapter (Zeile in `Plan_BLE_Sniffer_nRF52840.md` noch zu korrigieren).

**Schluss:** Mit gesundem Modul läuft 5.126 auf Board A durchgehend stabil. Der 22.10.-Abbruch (BLE-Download 1/3) ist nicht reproduziert; der Fehler hängt am Modulzustand, nicht an einer Firmware-Eigenschaft, die sich im normalen Ablauf zeigt. Nächster Schritt wäre, den Zustand zu provozieren (viele Zyklen nach Flash-Schreiben) oder Board B mit Sniffer zu prüfen.

## Befund Board B (26SC9983), 2026-10-29 13:23-14:40 (Firmware 5.126)

- BLE-Smoke mit `--pin 269983` und Sniffer: 5 Läufe (je 2 Versuche), 10 von 10 "unstable" (Gerät trennt nach ca. 5 s, kein PIN-Prompt). Das gilt auch nach Neuprogrammierung, Werksreset und Parameter-Neuprogrammierung durch den Anwender (danach meldet das Gerät HW `V2` mit Schnittstellen "Bluetooth, USB, Modem/Email-LTE, GPS" statt vorher `J2` "Bluetooth, USB").
- **Auf B kommt nie ein `[BT-Event]` (C/CU) auf RS232**, obwohl der PC verbunden ist und GATT auflöst. Auf Board A erscheinen die Events bei jeder Verbindung.
- **Classic-Smoke auf B scheitert ebenfalls** (kein PIN-Prompt). Der Fehler ist also nicht BLE- oder Windows-spezifisch: die Firmware empfängt nach dem Verbindungsaufbau nichts vom Modul.
- Modulkonfiguration (Kommandoabfragen) von B und A sind bis auf Name und Adresse identisch; Kommandos im Command-Mode funktionieren auf B.
- Offene Hypothesen: (1) UART1-Mux steht auf B auf LTE statt BT (HW V2 mit LTE/GPS), (2) Hardwarefehler im Pfad Modul-TX zu LPC-RX, (3) Modulzustand.

## Befund Board 26SC9994 (unveraendert, HW V2), Firmware-Bisektion, 2026-10-29 14:48-15:54

- Neues Werkzeug `Doku/tools/flash_xmodem.py`: flasht per RS232/USB (Hauptmenue, 3, Strg+U, XMODEM-1K, y); Strom-Zyklus mit `relay_power.py zyklus 10` (Relais COM7). Beide funktionieren; die Version wurde nach Flash und Stromzyklus kontrolliert (5.126 und 5.99).
- 26SC9994 scheitert **mit 5.126 und mit 5.99** (BLE und Classic: kein PIN-Prompt, Trennung nach ca. 5 s, kein `[BT-Event]`). Damit ist eine Firmware-Regression nach 5.99 ausgeschlossen. Stromzyklus (10 s) aendert nichts.
- Sniffer bei 5.99/26SC9994: PC schreibt Service-Changed-CCCD (Antwort ok) und danach den CYSPP-CCCD 0x0012 (0100): **das Modul antwortet auf diesen Write nicht** (Antwort FEHLT), danach keine Daten, Trennung. Auf Board A (umverdrahtet, IO5 vom GND getrennt) laeuft BLE mit 5.126 stabil.
- Offene Hypothesen: (1) Hardwareunterschied unverdrahtet/umverdrahtet (IO5 auf GND, CYSPP-Pegel), (2) veralteter Windows-Bond (noch nicht geprueft, Entfernen der Geraete in Windows steht aus). 26SC9994 laeuft nach dem Test auf 5.99.

## Update 5.127 (TEST, 2026-10-29): UART1 im Leerlauf nicht mehr abschalten - Board 26SC9994 laeuft wieder

**Symptom:** Boards B (26SC9983) und C (26SC9994) scheitern per BLE und Classic: kein PIN-Prompt, Trennung nach ca. 5 s, nie ein `[BT-Event]`; Sniffer: Modul beantwortet den CYSPP-CCCD-Write (0x0012) nicht. Die Firmware merkt die Verbindung gar nicht (bleibt im Hauptmenue), weil das Modul den Datenkanal nie oeffnet und CYSPP nie auf LOW zieht.

**Ursache (Code-Review, am Geraet bestaetigt):** Nach `init_bluetooth()` (und nach jeder GSM-Sitzung) rief `uart1_release()` (hard.c) ohne bestehende BT-Verbindung `Init_UART1(0)` auf: UART1 aus, RXD1/TXD1/RTS1/CTS1 wieder GPIO-Eingaenge mit Pull-up. Damit sah das IF820 (Flow Control `F=01`) seinen CTS dauerhaft HIGH = "Host nicht bereit" und durfte nichts senden (EZ-Serial-Guide 3.1.5.4). Beim Verbinden/CCCD-Write gibt das Modul aber Events aus ("alle GATT-Events bis der CYSPP-Kanal offen ist", Guide 3.2.1.1); die stauen sich. Das Modul (FW 1.4.12.12; "lockup in heavy UART loading" erst in 1.4.18.18 behoben, PCN #2E-2026) haengt dann offenbar und beantwortet den CCCD-Write nicht. Warum Board A trotzdem lief, ist offen (vermutlich elektrischer Unterschied an CTS/RTS1 bzw. MUX IC55/IC56, nicht gemessen). Erklaert vermutlich auch B6 (nach "Flash schreiben" keine Events mehr) und den "komischen Modulzustand" auf A.

**Fix:** `uart1_release()`: beim IF820 bleibt UART1 im Leerlauf auf BT aktiv (MUX BT, 460800, Auto-RTS LOW), Owner trotzdem `MUX_NONE`, `connect&UART1` geloescht. `UART1_IRQHandler`: die rbuf1-Weiche gilt jetzt auch fuer `MUX_NONE`, damit Leerlauf-Events nie im Menue-Puffer `rbuf` landen. Laird/RN4678 unveraendert (`Init_UART1(0)`). Der Minutentick (`bt_release()` bei `uart1_owner!=MUX_BT`) wirkt unveraendert, weil der Owner `MUX_NONE` bleibt.

**Test am Geraet (Board 26SC9994, COM3, Flash per `flash_xmodem.py`, Stromzyklus 10 s):**
- 5.127: BLE-Smoke 4/4 `passed` (jeder Lauf ein neuer Prozess, also Erstversuch), je Lauf `[BT-Event] C` und `CU` auf RS232 (vorher nie). Classic-Smoke 2/2 `passed` (Banner, PIN, Hauptmenue).
- Gegenprobe 5.126 (zurueckgeflasht, Stromzyklus): BLE-Smoke 2/2 `unstable`, also wieder das alte Fehlerbild.
- Danach 5.127 wieder geflasht: BLE-Smoke `passed`. **26SC9994 laeuft jetzt auf 5.127.**

**Test Board B (26SC9983, HW V2, Schnittstellen Bluetooth, USB, LTE, GPS; RS232 an COM3), 2026-10-29 16:27-16:33:**
- Ausgangslage 5.126: BLE-Smoke `unstable` (2 Versuche, altes Fehlerbild).
- 5.127 (Flash + Stromzyklus 10 s): BLE-Smoke 4/4 `passed`, jeder Lauf mit `[BT-Event] C` und `CU` auf RS232 (auf B bisher nie). Intervall vom PC beim Verbinden meist 7,5 ms (`I=0006`), einmal 60 ms (`I=0030`), jeweils per `CU` angepasst.
- Classic auf B nicht getestet: In Windows existiert kein Bluetooth-COM-Port mehr fuer B (FF:4F:7F:9E:EE:BA, frueher COM8), das Geraet muss neu gekoppelt werden. **26SC9983 laeuft jetzt auf 5.127.**

**Offen:** Board B und Board A mit 5.127 pruefen (A: keine Verschlechterung, module-suite inkl. Download); Wirkung bei LTE-Boards (GSM-Sitzung -> Rueckkehr in den aktiven Leerlauf); Doku `Doku/UART1_Owner_StateMachine.md` (Zustand NONE) nachgezogen. Status TEST: erst nach den Pruefungen als regulaere Version betrachten.

**Geaenderte Dateien:** hard.c, sprachen/sictxt.c (-> 5.127).

## Testlauf am Tisch 2026-11-02 (Board B 26SC9983, Firmware 5.127): zwei neue Fehler mit derselben Ursache

Neue automatische Tests (`Doku/tools/ble_mainboard_test.py`, Modi `nightly`, `reboot-check`, `power-sweep`, ...;
Gesamtablauf `Doku/tools/bt_testlauf.py`, Anleitung `Doku/tools/bt_testlauf_Anleitung.md`). Board B an RS232 (COM3),
Versorgung ueber Relais 2 (COM7), nur BLE (B ist in Windows nicht fuer Classic gekoppelt).

**Fehler 1 - BT tot nach dem naechtlichen Neustart um 3:15 (BEWIESEN):** `nightly` stellt die Uhr auf 03:13:30, die
Firmware startet um 03:15 neu (`BOOTTASK`, rtc.c, Protokolleintrag `I7 Reboot;..;03:15:00`). Danach 3 von 3 Mal BLE
tot: verbunden, kein PIN-Prompt, Trennung, **kein einziges `[BT-Event]`** (das Modul kann nichts an den LPC senden).
Gegenprobe nach Stromzyklus 3 von 3 bestanden. Ursache: Der Zweig `i_reset==BOOT_RES` in main.c ist leer; weder
`test_BT()` noch `init_bluetooth()` laufen, also auch kein `uart1_release()`, das seit 5.127 den BT-UART im
Leerlauf aktiv laesst. UART1 bleibt aus, das IF820 sieht CTS dauerhaft HIGH (wie vor 5.127 auf B und C).
**Folge im Feld: jedes Geraet verliert jede Nacht um 3:15 Bluetooth bis zum naechsten Stromzyklus** (LTE-Geraete
eventuell bis zur naechsten GSM-Sitzung, die UART1 wieder freigibt). Nach dem Reboot kommt auf RS232 kein Banner.

**Fehler 2 - BT tot nach 1 s Stromunterbrechung:** `power-sweep` (je 2 Mal): 0,05 / 0,1 / 0,2 / 0,5 s bestanden
(Geraet ueberbrueckt die Luecke, kein Neustart sichtbar), **1 s 0 von 2** (kein Banner, kein `[BT-Event]`),
2 / 5 / 10 / 30 s bestanden (sauberer Neustart mit Banner nach ca. 29 s). Bei 1 s startet der LPC neu (RS232
antwortet nach 20 s wieder), aber ohne Einschalt-Banner. Wahrscheinlich ein Brown-out-Reset: In `RSID` ist weder
POR noch WDTR gesetzt, main.c behandelt diesen Fall nicht -> wie Fehler 1 kein BT-Start. (Brown-out nicht direkt
nachgewiesen; das Geraeteprotokoll ist nicht eindeutig, weil die Uhr ohne Strom stehenbleibt.)

**Fehler 3 - Verbinden waehrend des Hochlaufs legt BT bis zum naechsten Stromzyklus lahm (BEWIESEN, kundenrelevant):**
`boot-race` (Stromzyklus 10 s, dann ein BLE-Versuch 1/2/3/5/8/12 s nach dem Einschalten, bei Fehlschlag Folgeversuch
10 s spaeter; je 2 Mal): Erstversuch 0 von 12 (erwartbar, das Banner kommt erst nach ca. 29 s), **Folgeversuch 7 von
12 gescheitert**. RS232-Mitschnitt (z. B. Schritt 5 s): Das IF820 wirbt sofort nach dem Einschalten, der PC verbindet,
das Modul geht in den Datenmodus. Der Hochlauf (`main.c`, POR-Zweig) ruft `test_BT()` -> `/PING` bleibt im Datenmodus
unbeantwortet -> `fp.btmodem=0; init_bluetooth()` -> Baud-Scan ohne Antwort ("no IF820 found") -> Fallback-Erkennung
Laird (`AT&F1`) und RN4678 (`$$$`), **diese Befehle gehen als Nutzdaten an die verbundene Gegenstelle** -> `I11016`,
`I11017`, **`F11010 BT Modul nicht konfiguriert`**. Danach meldet die Firmware keine Verbindung mehr (kein PIN-Prompt,
kein `[BT-Event]`). Erst der naechste normale Hochlauf erkennt das Modul wieder (`fp.btmodem==0` -> `init_bluetooth()`).
Folge im Feld: Wer die App sofort nach dem Einschalten verbindet (Techniker beim Aufbau), hat bis zum naechsten
Stromzyklus kein Bluetooth. Gelang der Folgeversuch, lag der Erstversuch offenbar nach `test_BT()`.
**Vorschlag:** Beim Hochlauf mit bekanntem IF820 (`fp.btmodem==IF820` aus dem Flash) bei aktiver Verbindung (CYSPP/
`BT_LINK` LOW, `comstate`) KEINE Neuerkennung, `fp.btmodem` behalten; Laird/RN4678-Fallback nie bei aktiver Verbindung.

**`gsm-mux` auf B nicht aussagekraeftig:** Das LTE-Modem antwortet nicht (`F18001 GSM-Fehler in libtool` nach 18 s),
das Modem-Terminal oeffnet sich nie. Ob Modem, Antenne oder SIM fehlen, ist offen. Der Test meldet den Fall jetzt
als "uebersprungen".

**Bestanden:** `reboot-check` mit 0,1 s Unterbrechung und 10 s Stromzyklus je 3 Mal, danach je 2 BLE-Smokes.
`bt-timeout` (BLE): Das Geraet trennt nach 415,8 s ohne Eingabe (erwartet 360-420 s). **Danach ist es bis zu 1 min
nicht auffindbar:** measure.c schaltet das Modul nach dem Zeitlimit ab (`PINMODE3` Pull-up) und erst beim naechsten
Minutentakt wieder ein. 35 s nach der Trennung "nicht gefunden", nach ca. 1,5 min wieder Smoke bestanden. Gewolltes
Verhalten, fuer Kunden aber spuerbar (App nach 6 min Leerlauf getrennt, Wiederverbinden erst nach bis zu 1 min).
Der Test wartet jetzt 90 s (`--timeout-recover`) vor der Pruefung.

**Vorschlag 5.128:** Am Ende des Hochlaufs in main.c fuer alle Reset-Arten bei `fp.btmodem==IF820` den BT-UART in den
Leerlauf-Zustand bringen (`uart1_release()` mit Owner `MUX_NONE` bzw. `test_BT()`). Behebt beide Fehler.

**Nebenbefund Testwerkzeug (behoben):** Nach der Protokollausgabe fragt die Firmware "Daten loeschen (j/n)?"; das
Skript antwortet jetzt ausdruecklich mit `n` (vorher nicht beantwortet, geloescht wurde nichts).

## Update 5.128 (TEST, 2026-11-02): BT nach jedem Reset starten, keine BT-Neuerkennung bei aktiver Verbindung

**Symptom:** Fehler 1 bis 3 aus "Testlauf am Tisch 2026-11-02" (BT tot nach 3:15-Reboot, nach 1 s Stromunterbrechung,
nach Verbinden waehrend des Hochlaufs).

**Fix 1+2 (main.c, Ende des Hochlaufs):** `if ((fp.btmodem==IF820) && (uart1_owner==MUX_NONE)) uart1_release(MUX_NONE);`
fuer jede Reset-Art. `uart1_release()` laesst seit 5.127 den BT-UART im Leerlauf aktiv (RTS LOW). Bisher erreichten
nur der POR-Zweig (`test_BT()`) und ein Watchdog-Reset mit GSM diesen Zustand; Reboot (`BOOTTASK`), Brown-out (in
main.c nur protokolliert) und Watchdog ohne GSM liessen UART1 aus. Greift nur, wenn UART1 niemandem gehoert (nach
Firmware-Update gehoert er BT, GSM wird nicht verdraengt).

**Fix 3 (btio.c, main.c):** Neu `bt_link_busy()` (CYSPP/IC54 Bit7 LOW und nicht selbst getrieben = Modul im
Datenmodus) und `bt_link_busy_wait(ms)`. Hochlauf (POR-Zweig): antwortet ein gespeichertes IF820 nicht auf `/PING`,
wird bis zu 3 s auf eine aktive Verbindung geprueft; ist sie da, bleibt `fp.btmodem=IF820`, keine Neuerkennung, die
Verbindung wird ueber `comchange` normal angenommen (PIN-Prompt). `init_bluetooth()` bricht bei aktiver Verbindung
sofort ab ("BT init skipped: connection active") - nie mehr Laird/RN4678-Befehle in eine laufende Verbindung.

**Test (Board B, `bt_testlauf.py --profil kurz`, 2026-11-02 07:51-08:04, nur BLE):** `nightly` 1/1 (vorher 0/3),
`reboot-check` dropout 1/1 und power 1/1, `power-sweep` 0,1 / 1 / 10 s je 1/1 (1 s vorher 0/2), `boot-race` 2 s und
8 s je Folgeversuch bestanden (vorher F11010). Der Erstversuch in den ersten ca. 30 s scheitert weiterhin (die
Firmware ist dann noch im Hochlauf); entscheidend ist, dass BT danach nicht mehr dauerhaft ausfaellt.
`gsm-mux` uebersprungen (Modem antwortet nicht). Bericht: `Doku/tools/Testreports/2026-11-02_07-51-21_26SC9983_testlauf/`.
**Bestaetigungsrunde (08:04-08:28, je 3 Wiederholungen, nur BLE):** `nightly` 3/3, `power-sweep` 0,1 / 1 / 10 s je
3/3, `boot-race` 2 s und 8 s je 3/3 (Folgeversuch; Erstversuch 0/6, Banner jeweils nach 30-32 s). Bericht:
`Doku/tools/Testreports/2026-11-02_08-04-58_26SC9983_testlauf/`. Offen: Classic, `bt-timeout` Classic und `bt-update`
auf einem gekoppelten Board (A oder C); Board ohne LTE (Watchdog-Weg ohne GSM).
**Nicht behoben, Beobachtung:** Das Geraet braucht nach dem Einschalten ca. 30 s bis zum Banner; das IF820 wirbt aber
sofort. Ein Kunde, der in dieser Zeit verbindet, bekommt keinen PIN-Prompt und muss neu verbinden.

**Geaenderte Dateien:** main.c, btio.c, btio.h, sprachen/sictxt.c (-> 5.128).

## Update 5.129 (TEST, 2026-11-02): Phase 0 der CYSPP-Zustandsmaschine - Testmenuepunkt "Modulbefehl senden"

**Zweck:** Werkzeug fuer Phase 0 aus `Doku/Konzept_CYSPP_Zustandsmaschine.md` (Abschnitt 6: wirbt das Modul mit
`.CYSPPSP E=1` im RAM noch, wird der Kanal nach `.CYSPPSTART` transparent, welche Ereignisse kommen vorher, ...).
Keine Aenderung am normalen BT-Ablauf.

**Neu (TEMPORAER, nach Abschluss der Zustandsmaschine wieder entfernen):** Testmenue -> 8 (BT-Diagnose) -> 7
"Modulbefehl senden", `bt_module_cmd()` in btio.c. Eingabe ueber RS232: Zeile + Enter = EZ-Serial-Befehl (CR wird
angehaengt), `"text` = roh ohne CR (im transparenten Modus Nutzdaten an die Gegenstelle), `!` = Pinzustand, leere Zeile
= Ende. Angezeigt werden jede Modulzeile aus `rbuf1` als `  < [+ms] ...` (unvollstaendige Zeilen nach 200 ms mit
"(ohne LF)") und jeder Pegelwechsel des CYSPP-Pins als `  [CYSPP] LOW/HIGH [+ms]` (IC54 Bit7, alle 100 ms, nicht neben
einem GSM-Thread). Waehrend der Schleife bewusst kein `communication_change()`: BT_LINK bleibt aus, transparente
Daten vom PC landen in `rbuf1` und werden sichtbar (sonst als Menueeingabe in `rbuf`); `comchange` bleibt gesetzt und
wird nach dem Verlassen normal verarbeitet. Der CYSPP-Pin wird nie getrieben. Ende nach 10 min ohne Eingabe.

**Gegenstueck PC:** `Doku/tools/cyspp_phase0.py` arbeitet die Fragen 1-6 automatisch ab (RS232 COM3 fuer die
Befehle, BLE per Bleak und Classic per SPP-COM-Port vom PC). Alle Modul-Aenderungen nur im RAM (Set-Befehle aus den
Get-Antworten, nur das betroffene Feld geaendert), am Ende `/RBT` (Flash-Stand = Autostart) und ein BLE-Smoke mit PIN
als Gegenprobe. Bericht `phase0.md`/`phase0.json` im Report-Ordner unter `Doku/tools/Testreports/`.

Build 0 Fehler, 0 Warnungen. Am Geraet noch nicht gelaufen (Hardware belegt).

**Geaenderte Dateien:** btio.c, btio.h, sprachen/sictxt.c (-> 5.129); neu `Doku/tools/cyspp_phase0.py`.
