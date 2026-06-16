# Project Status – IF820 Integration in sism555

Stand: 2026-06-16
Strategie: **Klein anfangen.** Zuerst nur prüfen, dass Verdrahtung + Toolchain laufen
(Lebenszeichen vom Modul). Alle größeren HW-/SW-Änderungen sind als Backlog notiert und
werden später Stück für Stück abgearbeitet. Detail-Hintergrund: `Doku/IF820_EZSerial_Konzept_Mapping.md`.

---

## Phase 0 – Bring-up / Lebenszeichen (JETZT)

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
