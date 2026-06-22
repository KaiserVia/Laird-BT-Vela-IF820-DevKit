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

## Fehler-Codes (DTC) — Mechanismus & Registry  (ab Version 5.89)

Ziel: jeder Fehler bekommt einen **eindeutigen** Marker `DTC<nnnnn>`, damit man ihn im Feld/Log
zweifelsfrei zuordnen kann. **Dubletten sind strukturell unmöglich.**

**Funktionsweise:** `dtc.h` definiert Makros, die `puterror()`, `puterrstr()` und `dtcerr()`
automatisch auf Varianten mit Code `DTCBASE + __LINE__` umleiten. Jede .c-Datei setzt ihre
eigene `DTCBASE` (10000er-Schritte) vor `#include "dtc.h"`. Da alle Dateien < 10000 Zeilen
haben, kann sich kein Code mit einem anderen überschneiden.

**Ausgabe am Terminal:** `Fehler DTC<code> <Kategorie/Text>` (z. B. `Fehler DTC10204 Bluetooth`).
Bei `puterror` wird der eindeutige Code zusätzlich in den Flash protokolliert (`protocol(dtc)`).

**Datei-Basis → Quelle (so dekodiert man einen DTC):**

| DTCBASE | Datei | Beispiel |
|---------|-------|----------|
| 10000 | btio.c | DTC10204 = btio.c, Zeile 204 |
| 20000 | gsmio.c | |
| 30000 | sicom.c | |
| 40000 | mqtt.c | |
| 50000 | gpsio.c | |
| 60000 | flash.c | |
| 70000 | main.c | |
| 80000 | libtool.c | |
| 90000 | USB_tools.c | |
| 100000 | sictst.c | |

Dekodierung: führende Stelle(n) = Datei (1xxxx = btio …), Rest = Quellzeile.

**Neue Funktionen/Dateien:** `dtc.h` (neu); in `sio.c`/`sio.h` ersetzt durch
`puterror_dtc()`, `puterrstr_dtc()`, `dctext()` (Makro `dtcerr`).

**Neue Fehlerdatei hinzufügen:** nächste freie `DTCBASE` (110000, 120000, …) definieren und
`#include "dtc.h"` ergänzen — sonst nichts.

**Bekannter Kompromiss:** Da der Code zeilenbasiert ist, ändert sich der DTC einer Fehlerstelle,
wenn darüber Zeilen eingefügt/entfernt werden. Dafür sind Dubletten ausgeschlossen und der Code
zeigt direkt Datei + Quellzeile.


---

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
