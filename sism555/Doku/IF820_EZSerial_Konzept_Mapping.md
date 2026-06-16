# Vela IF820 (EZ-Serial) – Konzept & Befehls-Mapping für sism555

Stand: 2026-06-16
Ziel: Den **Ezurio/Laird Vela IF820** (Infineon CYW20820, Firmware **EZ-Serial**) als dritten
BT-Modultyp neben **Laird 730-SA** und **Microchip RN4678** in die viasis3004-Firmware
(`sism555`, LPC1766) einbauen. **Dieses Dokument ist reines Konzept/Mapping – noch kein Code.**

Quellen (lokal im Repo):
- `Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf` (Befehlsreferenz, Abschn. 2.4, 4, 7, 10.1)
- `Documentation/EZ-QSG-Vela IF820 Cable Replacement with SPP_v2_0.pdf` (SPP-Transparentmodus)
- `Documentation/EZ-GUIDE-DVK-IF820_v2_0.pdf` (Pin-/Header-Mapping)
- `Documentation/EZ-DS-Vela IF820_v2_2.pdf` (Modul-Pinout)
- Projektkontext: `Doku/BT-Evalkit-Tasks.md`, `btio.c`, `Doku/UART1_Owner_StateMachine.md`

---

## 1. Kernunterschied (bitte zuerst lesen)

RN4678 und Laird steuern den Wechsel zwischen **Kommando-** und **Datenmodus _in-band_** über
die UART (`$$$` / `---` bzw. AT-Kommandos). Die gesamte `btio.c`-Logik baut darauf auf.

**Der IF820 macht das anders – über Hardware-Steuerpins:**

| Pin | Richtung | Bedeutung |
|-----|----------|-----------|
| **CYSPP** | In/Out | Modus-Steuerung. Extern HIGH = Kommandomodus, extern LOW = Daten-/SPP-Modus. Bei aufgebauter SPP-Verbindung setzt das **Modul** den Pin selbst auf LOW. Treibt der **Host** ihn auf HIGH, wird die SPP-Verbindung getrennt und das Modul kehrt in den Kommandomodus zurück. |
| **CONNECTION** | Out | Verbindungs-Statusanzeige, **aktiv LOW** = Verbindung steht (BLE oder SPP). Ideal als Ersatz für den bisherigen Status-GPIO. |
| **CP_ROLE** | In | CYSPP-Rolle: LOW = Central, HIGH = Peripheral. Für unseren Anwendungsfall (Gerät wird gefunden) = Peripheral / floating. |
| **LP_MODE** | In | Sleep-Steuerung. HIGH = kein Sleep. Intern hochgezogen. |

Konsequenz für die Integration: Der reine UART-Tausch (TXD/RXD/RTS/CTS über den MUX) **reicht
nicht**. Für vollwertigen Betrieb müssen mindestens **CYSPP** und **CONNECTION** an freie
MCU-GPIOs. Siehe Abschnitt 6 (Integrationslücke) – das ist der größte Posten.

Quelle: EZ-Serial Guide Abschn. 2.3.2.1 / 2.4 ("The active communication mode depends on the
state of the CYSPP pin").

---

## 2. EZ-Serial Protokollgrundlagen (Text-Mode)

EZ-Serial läuft ab Werk im **Text-Mode** – das passt gut zu unserem zeilen-/substring-basierten
Parser (`wait_message`). Empfehlung: **Text-Mode beibehalten** (kein Binär-Mode).

- Kommandos enden mit **CR (0x0D)**, LF oder beidem.
- Präfixe: `/` = ACTION, `S` = SET, `G` = GET, `.` = PROFILE.
- Antwort auf Kommando: **`@R,<len>,<name>,<errcode>,...`** – Erfolg = `errcode=0000`.
- Asynchrone Ereignisse: **`@E,<len>,<name>,...`** (z. B. `BOOT`, `BTCON`, `DIS`).
- **Echo ist ab Werk AN** → jedes gesendete Kommando wird zurückgespiegelt. Vor dem Parsen
  per `SPEM,M=0` (protocol_set_echo_mode) abschalten, sonst Doppel-Empfang.
- Argumente hex, ohne `0x`, Reihenfolge egal, fehlende SET-Argumente bleiben unverändert.

Werte ab Werk (EZ-Serial Guide Abschn. 2.2 / 10.1):
- UART **115200 8N1, Flow Control AUS** (`STU,B=0001C200,A=00,C=00,F=00,D=08,P=00,S=01`).
- Device-Name-Vorlage `SDN,N=EZ-Serial %M4:%M5:%M6` (für BT-Classic hängt das Modul `_BT` an →
  Anzeige z. B. `EZ-Serial 15:65:B1_BT`).
- **SPP-Service und CYSPP sind ab Werk aktiv** (Dual-Mode); das Modul advertised sofort.

Boot-Meldung (nach Reset/Power-On):
```
@E,0076,BOOT,E=01040C0C,S=03010000,P=0104,H=F1,C=00,A=E48AC81565B1,F=EZ-Serial-VELA_IF820_INT V1.4.x.x ...
```
`A=` BT-Adresse, `F=` Firmware-Version.

Ping (Lebenszeichen / Modulerkennung):
```
TX:  /PING<CR>
RX:  @R,001D,/PING,0000,R=...,F=...
```

---

## 3. Befehls-Mapping RN4678 / Laird → EZ-Serial

> Hinweis: Beim RN4678/Laird sind das textuelle Makros aus `btio.h`. Beim IF820 ist die
> Spalte „EZ-Serial" der **Text-Mode-Befehl** plus der API-Name (Gruppe/ID).

| Funktion | RN4678 (`$$$`-Mode) | Laird 730-SA (AT) | **IF820 EZ-Serial** | API (ID) |
|----------|---------------------|-------------------|---------------------|----------|
| Modul erkennen / Lebenszeichen | `$$$` → `CMD>` | `AT` → `OK` | `/PING` → `@R,...,/PING,0000` | system_ping (2/1) |
| Echo aus | (Statusmeld. via `SO`) | `ATE0` | `SPEM,M=0` | protocol_set_echo_mode (1/3) |
| Werksreset | `SF,1` → `AOK` | `AT&F1` | `/RFAC` | system_factory_reset (2/5) |
| Reboot | `R,1` → `%REBOOT` | `ATZ` | `/RBT` → `@E,...,BOOT,...` | system_reboot (2/2) |
| In NVM/Flash sichern | (auto bei SET) | `AT&W` | `/SCFG` | system_store_config (2/4) |
| Gerätename | `SN,<name>` | `AT+BTN`/`AT+BTF` | `SDN,N=<name>` | gap_set_device_name (4/15) |
| PIN (Legacy-Pairing) | `SP,<pin>` | `AT+BTK="<pin>"` | `SBTPIN,...` | smp_set_pin_code (7/15) |
| Pairing-Methode / Security | `SA,2` (Just Works) | `ATS502/504` | `SSBP,M=..,I=..,F=..` | smp_set_security_parameters (7/11) |
| Fester Passkey (statt PIN) | – | – | `SFPK,P=<hex>` + `SSBP` | smp_set_fixed_passkey (7/13) |
| Profil = SPP (Classic) | `SS,SPP` | (Laird Default) | SPP ab Werk aktiv; ggf. `SBTP` | bt_set_parameters (14/10) |
| Sendeleistung max | `SY,4` | `ATS...` | `STXP,P=<hex>` | system_set_tx_power (2/21) |
| Dual-Mode (BLE+Classic) | `SG,0` | – | ab Werk Dual; CYSPP-Param `.CYSPPSP` | p_cyspp_set_parameters (10/3) |
| UART-Baudrate setzen | `SU,10` (307200) | `ATS521=460800` | `STU,B=<hex>,F=<0/1>` | system_set_uart_parameters (2/25) |
| Statusmeldungen aus | `SO,,` | div. ATS | Echo aus (`SPEM`) + Events nicht abonnieren | – |
| In Kommandomodus | `$$$` | (immer) | **CYSPP-Pin HIGH** (Hardware) | – |
| Zurück in Datenmodus | `---` | `CONNECT` | **CYSPP-Pin LOW** / Auto bei SPP-Connect | – |
| Verbindungsstatus | Status-GPIO → I2C-Expander Bit7 | – | **CONNECTION-Pin (aktiv LOW)** | – |
| Verbindung trennen (Gerät) | Disconnect-Kommando | – | CYSPP-Pin HIGH, **oder** im Cmd-Mode `/BTDIS` | bt_disconnect (14/6) |

### Baudraten als EZ-Serial-Hex (`STU,B=`)
| Baud | Hex (8-stellig) |
|------|-----------------|
| 9600 | `00002580` |
| 115200 (Default) | `0001C200` |
| 307200 (Ziel RN4678) | `0004B000` |
| 460800 (Ziel Laird) | `00070800` |

Flow Control: `F=01` = RTS/CTS an (zwingend fürs Projekt), `F=00` = aus.

**Wichtig (protected setting):** `STU` ist geschützt – Werte werden erst in RAM angewendet,
dann mit `/SCFG` in Flash geschrieben. Ablauf: `STU,...` senden → Host-Baudrate sofort
nachziehen → Kommunikation prüfen → `/SCFG`. Falsche Reihenfolge = Kommunikationsverlust.

---

## 4. Vorgeschlagener Init-Ablauf für IF820 (analog `init_bluetooth`)

Voraussetzung: CYSPP-Pin im **Kommandomodus** (HIGH oder floating), keine aktive Verbindung.

1. `Init_BT_ch(115200)` – IF820 startet immer bei 115200 (Default). CTS-Check wie gehabt.
2. **Erkennung:** `/PING` → auf `@R,...,/PING,0000` prüfen. Schlägt fehl → ggf. weitere
   Baudraten (falls schon mal umkonfiguriert) durchprobieren, analog der bestehenden Retry-Schleife.
3. `SPEM,M=0` – Echo aus.
4. (Optional sauberer Startpunkt) `/RFAC` → Werksreset → `/RBT` → auf `BOOT`-Event warten,
   danach erneut bei 115200, Echo erneut aus.
5. `SDN,N=VIASIS_<serno>` – Gerätename setzen (BT-Classic-Anzeige bekommt `_BT` angehängt).
6. **Security/Pairing:**
   - Variante PIN (wie bisher 6-stellig aus Seriennummer): `SBTPIN,...` mit der PIN.
   - Variante "Just Works"/SSP: `SSBP,M=..,I=..,F=..` passend setzen.
   - (Genaues Argument-Set in Test-Phase festlegen, siehe offene Punkte.)
7. `STXP,P=<hex>` – Sendeleistung (Max-Stufe).
8. **Baudrate:** `STU,B=0004B000,A=00,C=00,F=01,D=08,P=00,S=01` (307200, Flow Control an)
   → Host-UART1 auf 307200 umstellen → mit `/PING` verifizieren.
9. `/SCFG` – alles dauerhaft in Flash sichern.
10. Status setzen: `fp.btmodem = <IF820>`, `interfaces |= BT_LINK`.
11. `uart1_release(MUX_BT)` (State Machine wie bei RN4678/Laird).

Betrieb danach: Modul advertised SPP. Verbindet sich ein Peer, geht **CONNECTION** auf LOW und
das Modul schaltet selbst in den SPP-Datenmodus (CYSPP LOW) → ab da fließen die Terminal-/
Menüdaten transparent. Trennen: CYSPP-Pin HIGH treiben.

---

## 5. Betriebsmodell im Gerät (UART1-Sharing)

Die bestehende UART1-State-Machine (`UART1_Owner_StateMachine.md`) bleibt im Prinzip gültig –
der IF820 hängt am selben MUX-Kanal BT. Unterschied:

- **Kein `$$$`-Escape** nötig/möglich, um „kurz" in den Kommandomodus zu wechseln, während eine
  SPP-Verbindung läuft. Re-Konfiguration zur Laufzeit erfordert, die SPP-Verbindung über den
  CYSPP-Pin zu beenden (→ Kommandomodus), oder Konfiguration nur im verbindungslosen Zustand.
- **Verbindungserkennung** läuft über den CONNECTION-Pin statt über ein in-band Event. Das ist
  sogar robuster und entkoppelt von der UART (funktioniert auch während Datenmodus).
- UC4/UC5 (UART1 geht kurz an GSM): unkritisch, da SPP transparent ist und der CTS-Handshake
  die Daten im Modulpuffer hält. Nach Rückgabe von UART1 an BT läuft die SPP-Verbindung weiter.

---

## 6. Integrationslücke Hardware (größtes Risiko / To-Do)

Aktuelle Verdrahtung Gerät ↔ Modul (laut `BT-Evalkit-Tasks.md`): nur **TXD/RXD/CTS/RTS** über
den MUX. Für den IF820 fehlen die Steuer-/Statuspins:

| IF820-Signal | Zweck | Status im sism555-Design | Vorschlag |
|--------------|-------|--------------------------|-----------|
| **CONNECTION** | Verbindungsstatus (aktiv LOW) | nicht verdrahtet | An den bestehenden Status-Eingang legen (bisher RN4678-Status → I2C-Expander IC54 Bit7). Am Eval-Kit: CONNECTION-Pin abgreifen. |
| **CYSPP** | Daten-/Kommandomodus + SPP-Trennen | nicht verdrahtet | Freien MCU-GPIO als Output. Zum Trennen/Reconfig HIGH treiben. Sonst floating lassen (Auto-SPP). |
| CP_ROLE | Central/Peripheral | nicht verdrahtet | Floating oder fest HIGH (Peripheral) – passt zum Anwendungsfall. |
| LP_MODE | Sleep | nicht verdrahtet | Für Always-On fest HIGH; nur relevant bei Stromsparbetrieb. |

**Power/Enable:** Das Projekt schaltet bisher BT-VCC per GPIO (6-Min-Timeout). Der IF820 hat
**keinen** simplen Enable-Pin – Optionen: (a) Modul-Versorgung (VBAT/VDDIO) extern schalten,
oder (b) Deep Sleep via `SSLP` + LP_MODE-Pin statt komplettem Power-Cycle. Designentscheidung
nötig.

### Mapping am DVK (Chip-Antennen-Variante), für die Eval-Verdrahtung
Aus `EZ-GUIDE-DVK-IF820_v2_0.pdf` (Antennen-Variante):
- PUART (für UART1): `P_UART_TXD=J1.24`, `P_UART_RXD=J1.20`, `P_UART_RTS=J1.22`, `P_UART_CTS=J1.18`, `GND=J2.1`.
- CYSPP: Header J1.14 (Modul-Pin 12, „CYSPP"). CONNECTION/CP_ROLE: siehe DVK-Tabelle 4 /
  Modul-Pinout in `EZ-DS-Vela IF820_v2_2.pdf` – in der Test-Phase final festklopfen.

---

## 7. Abgleich mit den 12 Eval-Tasks (`BT-Evalkit-Tasks.md`)

| Test | IF820-Bewertung |
|------|-----------------|
| 1 Grundkommunikation transparent | ✅ über SPP-Datenmodus (QSG „Cable Replacement with SPP"). |
| 2 Flow Control CTS/RTS | ✅ konfigurierbar `STU ...,F=01`. Verhalten messen. |
| 3 Baudratenwechsel + persistent | ⚠️ `STU` + `/SCFG`. **307200/460800 sind nicht-Standard** – EZ-Serial unterstützt das (fraktionaler Teiler), warnt aber: Taktfehler <3 %, **mit Oszi/Logikanalysator verifizieren**. |
| 4 Command-Mode Ein/Austritt | ⚠️ **anders als RN4678** – kein `$$$`, sondern CYSPP-Pin. Logik in `btio.c` muss umgebaut werden. |
| 5 Factory Reset + Rekonfig | ✅ `/RFAC` → `/RBT` → SET-Sequenz → `/SCFG`. |
| 6 Verbindungserkennung Status-GPIO | ✅ **CONNECTION-Pin (aktiv LOW)** – sauberer als beim RN4678. Muss verdrahtet werden. |
| 7 Xmodem-1K Durchsatz | ✅ transparent; abhängig von Baudrate/Flow Control. Mit Test 3 koppeln. |
| 8 Power Cycling | ⚠️ kein Enable-Pin → Versorgung schalten oder Deep Sleep (siehe Abschn. 6). NVM bleibt erhalten. |
| 9 CTS-Pause (UART1-Sharing) | ✅ Modul puffert bei CTS HIGH. Puffergröße messen. |
| 10 BLE Discovery + Classic SPP | ✅ Dual-Mode ab Werk (CYSPP/BLE + SPP/Classic gleichzeitig). |
| 11 Reichweite/Leistung | ✅ `STXP` Max; praktisch messen. |
| 12 Timing nach Reboot (<3s bis Cmd) | ⚠️ Boot-Event-Timing messen (Test). Bisher RN4678-Annahme 2s. |

---

## 8. Offene Punkte / vor dem Coden zu klären

1. **Steuerpins verdrahten** (CYSPP, CONNECTION) – ohne sie ist nur „dummer" Always-SPP-Betrieb
   möglich (CYSPP fest an GND), dann aber keine API-Konfiguration zur Laufzeit. Empfehlung:
   „Smart MCU host"-Design (EZ-Serial Guide Abschn. 4.1) nachbauen.
2. **Security-Modell festlegen:** Legacy-PIN (`SBTPIN`, kompatibel zur bisherigen 6-stelligen
   Seriennummer-PIN) vs. SSP „Just Works"/Passkey (`SSBP`/`SFPK`). Exakte Argumentwerte erst
   im Test verifizieren.
3. **Zielbaudrate 307200 vs. 460800** – am IF820 messen (Taktfehler). Ggf. auf eine
   Standardbaudrate (z. B. 115200/230400) ausweichen, wenn Fehlerrate zu hoch.
4. **Power-Konzept** (Versorgung schalten vs. Deep Sleep) für den 6-Min-Timeout.
5. **Modul-Erkennung** in der bestehenden Auto-Detect-Schleife ergänzen: IF820 reagiert weder
   auf `AT` noch `$$$`, sondern auf `/PING`. Reihenfolge der Erkennung definieren.
6. **`btmodem`-Konstante** für IF820 in `btio.h` definieren (z. B. `#define IF820 82`),
   `BT_BAUD`-Makro erweitern.

---

## 9. Integrationsplan in `btio.c` (Schritte, kein Code)

1. `btio.h`: Modultyp-Konstante `IF820`, EZ-Serial-Textmakros (`/PING`, `SPEM,M=0`, `/RFAC`,
   `/RBT`, `SDN,N=`, `SBTPIN`/`SSBP`, `STXP`, `STU,B=`, `/SCFG`), Antwort-Token (`@R`, `0000`,
   `BOOT`), Baud-Makros.
2. Neue Funktion `init_if820()` nach dem Init-Ablauf aus Abschnitt 4; oder Erweiterung der
   `init_bluetooth()`-Kaskade um einen dritten Zweig (nach Laird/RN4678-Fehlschlag).
3. `bt_command()` ist wiederverwendbar (sendet String, wartet auf Antwort-Substring) – als
   Erfolgskriterium `@R,...,0000` statt `OK`/`AOK`/`CMD>` parsen. Ggf. kleiner Wrapper
   `ezs_command(cmd, "0000", pause)`.
4. Mode-Handling: an die Stellen, die bisher `$$$`/`---` nutzen (`test_BT`, `send_bt_info`,
   `set_bt_name`, `set_bt_pin`, Timeout-Disconnect in `measure.c`), einen **CYSPP-Pin-Treiber**
   einsetzen (GPIO HIGH = Kommandomodus, LOW/floating = Daten/SPP).
5. Verbindungserkennung: CONNECTION-Pin lesen (statt RN4678-Status). Falls über I2C-Expander
   geführt – dort Bit anpassen.
6. State Machine (`hard.c`) bleibt; nur die BT-spezifischen Hilfsfunktionen ändern sich.

> Reihenfolge der Umsetzung später: erst **Kommunikationstest** (`/PING` bei 115200), dann
> Security/Baud/SPP, dann Steuerpin-Logik, dann die Eval-Tests 1–12 abarbeiten.
