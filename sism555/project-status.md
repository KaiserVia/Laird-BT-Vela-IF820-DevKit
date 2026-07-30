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
