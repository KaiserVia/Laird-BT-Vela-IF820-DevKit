# LTE-Update — Analyse LTE-Modul (Quectel EG91) in sism555

> Erstellt am 2026-07-27. Grundlage: `gsmio.c` / `gsmio.h`, `mqtt.c`,
> `UART1_Owner_StateMachine.md`, EG9x AT-Command-Referenz (Quectel).
> Zweck: Ist-Aufnahme der LTE-Anbindung + Bewertung gegen die EG91-Doku +
> konkrete Robustheits-Empfehlungen, besonders für **schlechten Empfang**.
>
> **Rahmenannahme:** Das Gerät steht **fest an einem Standort**. Das verändert
> die Bewertung: Funkbedingungen sind stabil, „schnell aufgeben und woanders
> probieren" bringt nichts. Die Hebel sind deshalb **einmalige Konfiguration am
> Standort** (bestes Netz/Band merken) plus **geduldige Verbindung mit langem
> Backoff bei echtem Ausfall** — nicht kürzere Timeouts. Siehe Kap. 5.7, 7, 8.

---

## 1. Wie wird das Modul angesprochen?

Das Modul (UC15 3G *oder* EG91 LTE, per `fp.gsm` unterschieden) hängt zusammen
mit dem BT-Modul und GPS an **UART1**, umgeschaltet über einen externen MUX. Der
Zugriff läuft komplett über **AT-Kommandos** als ASCII-Text.

Physische Steuerleitungen (in `hard.h`/`gsmio.c`):

- `GSMPWR` (FIO1) — Versorgungsspannung Modem an/aus.
- `PWRKEY` (FIO1) — Einschalt-/Power-Key-Impuls.
- `GSM_DTR` (FIO0) — Doppelfunktion: `LOW` = Sleep verhindern; als Flanke
  `HIGH` = Data→Command-Mode-Umschaltung (Escape ohne `+++`).
- `CTS` (FIO2) — Hardware-Flow-Control-Eingang vom Modem (RTS des Moduls).
- `CSAF` — MUX-Selectleitung GSM-Kanal.

Datenrouting: Es gibt eine globale Bitmaske `connect`. `connect|=UART1` sendet an
das Modem, `connect&=~UART1` nur an die Terminals (RS232/USB/BT). Ausgaben
werden über `putln()`/`putstr()` gesendet, Antworten über `wait_message()`,
`wait_ok()`, `wait_line()`, `uart_read()` mit Timeouts (`XMODEM_*_TIMEOUT`)
ausgewertet. Kommando-Konstanten stehen zentral als `T_*`-Defines in `gsmio.h`.

Command-/Data-Mode:

- **Data→Command**: `send3plus()` (`+++` mit 1 s Guard-Time davor/danach) für
  EG91, ODER DTR-Flanke `HIGH` (schneller, kein Guard-Delay). Der Code nutzt für
  EG91 überwiegend `send3plus()`, für UC15 die DTR-Flanke — siehe `sendmail()`
  State 34, `cert_to_uc15()`, `Download_firmware()` State 10.
- **Command→Data**: `AT+QISWTMD=1,2` (`T_ctsw`).

---

## 2. Wie wird initialisiert?

### 2.1 Einschaltsequenz — `gsm_power(mode)`

Bitmaske `mode`: Bit0 = Modem ein, Bit1 = MUX-GSM wählen + auf `+CFUN:1` warten.

- **Aus (`mode=0`)**: PWRKEY LOW, ggf. Versorgung aus (nur wenn GPS auch aus),
  DTR LOW, `uart1_release(MUX_GSM)`, Status-Flags zurücksetzen.
- **Ein (`mode&1`)**: **Bei EG91** wird zuerst hart abgeschaltet (`GSMPWR` LOW,
  1 s warten), dann PWRKEY-Impuls (HIGH → 500 ms LOW → HIGH). `power_restart=1`.
- **`mode&2`**: `Init_GSM_ch(GSM_BAUD)`, auf `CTS=LOW` warten (max. 200×50 ms =
  10 s), dann Funktionsstatus per `AT+CFUN?` bzw. Einschalt-URC `+CFUN: 1`
  abwarten. Fehlermarken: `mqtt_con_err=43/44`.

### 2.2 Erst-Initialisierung / Modultyp-Erkennung — `init_gsm()`

1. Modem einschalten, UART1 auf 115200.
2. **Baud-Probing**: `AT` bei 115200 → 9600 → 307200 → 460800, bis `OK` kommt.
3. `AT&F` (Werksreset) + `ATI` (Produkt-ID).
4. **Modultyp-Erkennung** aus der `ATI`-Antwort: `UC15` → `modem=15`,
   `EG91` → `modem=91`, sonst `1`.
5. Gemeinsame Konfiguration `T_cfg` = `ATQ0;E0;S0=1;+IFC=2,2` (Echo aus,
   Auto-Answer, **HW-Flow-Control ein**). Es wird geprüft, dass danach
   **CTS=LOW** ist (Flow-Control tatsächlich aktiv).
6. `T_cfuc15` = `AT+QURCCFG="URCPORT","UART1";+QCFG="NWSCANMODE",0,1`.
7. `T_cfsto` = `AT+IPR=460800;&W` — feste Baudrate + Speichern.
8. Bei Erfolg: Default-Parameter setzen, `interfaces|=GSM_LINK`, `fp.gsm=modem`.
9. Fehler → `dtcerr(DTC_GSM_NO_RESP)`, Modem aus.

### 2.3 PIN & Registrierung — `modem_start()` / `atcpin()` / `GSM_Registration()`

- `modem_start()` fährt das Modem hoch, wartet die Boot-URCs ab (`+CPIN`,
  `READY`/`SIM PIN`).
- `atcpin()` setzt `AT+CMEE=2` (verbose Fehler) und behandelt die SIM-PIN
  (`AT+CPIN?` / `AT+CPIN=`), protokolliert `SIM_ERROR` bei falscher PIN.
- `GSM_Registration()` läuft als eigener RTOS-Thread und stößt `atcpin()` an.

### 2.4 Einbuchung & Datenkontext

- `test_registration()`: pollt bis zu **16×** im **5-s-Takt** (≈ 80 s)
  `AT+CREG?;+CGREG?` (UC15) bzw. `AT+CREG?;+CGREG?;+CEREG?` (EG91). Akzeptiert
  Status `1` (Home) oder `5` (Roaming, nur wenn `fp.roaming`). Bei Erfolg
  zusätzlich `AT+CSQ` (→ `fp.csq`), `AT+QCCID` (ICCID) und `gsm_nettime()`
  (`AT+QLTS=2`, NITZ-Zeitsync).
- `set_gprs_config()`: `AT+QICSGP=1,<APN>,<user>,<pass>` und `AT+QIACT=1`
  (PDP-Kontext aktivieren, Timeout 20 Blöcke).
- Anwendungen darüber: **MQTT(S)** (`mqtt.c`, `Con_MQTT_server`), **SMTP-Email**
  (`sendmail`), **SMS** (`sendsms`), **HTTP-Firmware-Download** (`Download_firmware`
  mit `AT+QHTTP*`, blockweisem Retry × 3).

---

## 3. Betrieb zusammen mit Bluetooth (UART1-Sharing)

BT und Modem teilen sich physisch UART1. Koordiniert wird das über die
**UART1-Owner-State-Machine** (`hard.c`, dokumentiert in
`UART1_Owner_StateMachine.md`):

- Zustände `MUX_NONE / MUX_BT / MUX_GSM`, globaler `uart1_owner`.
- `uart1_request(MUX_GSM, baud)` (Wrapper `Init_GSM_ch`): schaltet MUX auf GSM,
  **suspendiert die BT-Ausgabe** (`connect &= ~UART1`), damit `putb()` nicht auf
  falschem CTS hängt.
- `uart1_release(MUX_GSM)`: gibt UART1 frei und **stellt automatisch BT wieder
  her**, falls `BT_LINK` noch aktiv ist (sonst UART1 deinit).
- Kritische Abschnitte mit `__disable_irq()/__enable_irq()` (Race MQTT-Thread ↔
  Main-Thread).

Wichtig: `connect|=UART1` innerhalb einer GSM-Session ist ein **exklusiver
Modus** — solange das Modem UART1 besitzt, geht keine BT-Terminalausgabe raus.
Die Menü-/Config-Funktionen prüfen daher `if (!(connect&UART1))`, bevor sie eine
Modemaktion (z. B. `set_gprs_config`) direkt auslösen, um Kollisionen mit einer
laufenden BT- oder GSM-Session zu vermeiden.

**Bewertung:** Diese Trennung ist sauber gelöst und war bereits Quelle mehrerer
gefixter Bugs (Endlosschleife bei BT-Disconnect, falscher Owner im Debug-Terminal,
Race Condition). Für den LTE-Betrieb selbst ist hier nichts offen.

---

## 4. Prüfung gegen die EG91-Doku — was ist korrekt?

Grundsätzlich ist die AT-Sequenz für den EG91 **korrekt und modultauglich**:

- `+IFC=2,2` (HW-Flow-Control) + CTS-Verifikation — richtig und wichtig bei
  460800 Baud, damit der Empfangspuffer nicht überläuft.
- `AT+IPR=460800;&W` — feste, gespeicherte Baudrate, kein Autobauding-Risiko.
- EG91 nutzt korrekt **`+CEREG`** zusätzlich zu `+CREG`/`+CGREG` (LTE-EPS-Status).
- `AT+QCFG="NWSCANMODE",<0|3>` — `0`=automatisch, `3`=LTE-only. Das
  `netcode`-Mapping in `change_radionet()` (net 2 → 3 bei EG91) passt zur EG9x-Doku.
- `AT+QLTS=2` (NITZ-Netzzeit), `AT+QCCID` (ICCID), `AT+QICSGP`/`AT+QIACT`,
  `AT+QSSLCFG`/`AT+QSSLOPEN`, `AT+QHTTP*` — allesamt gültige EG9x-Kommandos.
- EG91-spezifisches Data→Command per `+++`/`No Carrier` statt DTR ist korrekt
  behandelt.

**Fazit:** Die Grundanbindung „macht es richtig". Die Schwächen liegen nicht in
falschen Kommandos, sondern in **fehlender Adaptivität bei schlechtem Empfang**.

---

## 5. Robustheit bei schlechtem Empfang — Empfehlungen

Reihenfolge grob nach Nutzen/Aufwand. Alle Kommandos sind EG9x-gültig.

### 5.1 Signalqualität *messen und nutzen* (höchster Nutzen)

**Ist:** `AT+CSQ` wird abgefragt und in `fp.csq` gespeichert — aber **nirgends
ausgewertet**. `+CSQ` liefert bei LTE nur RSSI (0–31 / 99), was wenig über die
tatsächliche LTE-Verbindungsgüte aussagt.

**Empfehlung:**

- Zusätzlich **`AT+QCSQ`** abfragen. Bei LTE liefert es
  `+QCSQ: "eMTC/…",<rssi>,<rsrp>,<sinr>,<rsrq>` — **RSRP, SINR und RSRQ** sind
  die aussagekräftigen Kennwerte für schwachen Empfang. Alternativ das
  Standardkommando `AT+CESQ` (RSRP/RSRQ).
- Optional `AT+QNWINFO` (RAT/Band/Kanal) und `AT+QENG="servingcell"`
  (Zellinfo, Nachbarzellen) zur Diagnose ins Fehler-/Statusprotokoll aufnehmen.
- **Gate nur „kein Netz", nicht „schwach"**: `csq==99` / kein Fix → Versuch
  verschieben. Ein **harter RSRP-Schwellwert als Gate ist bei festem Standort
  gefährlich** — ein dauerhaft marginaler, aber funktionierender Standort würde
  sich dann nie verbinden. Maßstab für „gut genug" ist, dass die **Verbindung
  klappt**, nicht der Signalpegel. Primärnutzen der Signalwerte ortsfest:
  einmalige **Antennen-Optimierung** bei Installation + Langzeit-Logging
  (erkennt Verschlechterung durch Bewuchs/Bau) + Eingangsgröße für den Netzscan
  (Kap. 7).
- Kennwerte periodisch loggen → Feld-Diagnose „warum bucht Gerät X schlecht ein".

### 5.2 Registrierungs-Status differenziert behandeln

**Ist:** `test_registration()` bricht nur bei Status `1`/`5` positiv ab und
pollt sonst stur 16×.

**Empfehlung:** Die CEREG/CREG-Statuscodes gezielt auswerten:

- `2` = *sucht Netz* → weiter warten (ggf. Poll-Fenster verlängern).
- `3` = *Registrierung abgelehnt* → **früh abbrechen**, protokollieren
  (SIM/Netz-/Roaming-Problem), nicht 80 s blind weiterpollen.
- `0` = nicht registriert, sucht nicht → einmal `AT+COPS=0` (auto) nachtriggern.

Das verkürzt bei echten Problemen die Blockadezeit und erhöht bei „nur schwach"
die Erfolgschance.

### 5.3 PDP-Kontext-Aktivierung absichern

**Ist:** `AT+QIACT=1` wird einmal gesendet; bei Fehler bricht die ganze Kette ab.
Bei schlechtem Empfang scheitert die Aktivierung häufiger transient.

**Empfehlung:**

- Vor `QIACT` den Status per **`AT+QIACT?`** prüfen; ist der Kontext schon aktiv,
  nicht erneut aktivieren (sonst Fehler).
- **1–2 Retries** mit kurzer Pause bei transientem Fehler, bevor abgebrochen wird.
- Bei wiederholtem Fehlschlag Kontext sauber schließen (`AT+QIDEACT=1`) und mit
  Backoff neu versuchen (siehe 5.6).

### 5.4 TCP-Keepalive aktivieren

**Ist:** `T_cfgalive` (`AT+QICFG="tcp/keepalive",1,3,60,3`) ist **auskommentiert**
(mqtt.c State 10).

**Empfehlung:** Für stehende MQTT-Verbindungen aktivieren, damit „stille" tote
Links (typisch bei wackeligem Empfang / NAT-Timeout im Mobilfunk) erkannt und
neu aufgebaut werden, statt bis zum nächsten Sendeversuch unbemerkt zu bleiben.
Ergänzend MQTT-eigenes Keepalive/PINGREQ prüfen.

### 5.5 RAT-/Band-Strategie für feste Standorte

> **Wichtigster Hebel bei festem Standort.** Da Standort und Zelle konstant sind,
> lohnt eine einmalige Standort-Vermessung mit dauerhaftem Lock. Ausführlich in
> Kap. 7 (Netzscan) und Kap. 8 (GPS-Trigger + Menüpunkt).

Bei ortsfesten Geräten in Randlage hilft es, den Suchraum einzuschränken:

- **`AT+QCFG="nwscanseq",...`** — Suchreihenfolge (z. B. LTE zuerst).
- **`AT+QCFG="band",...`** — auf die real verfügbaren Bänder beschränken →
  schnellere (Re-)Registrierung, weniger Zappeln zwischen RATs.
- Für reine LTE-Standorte ist `NWSCANMODE=3` (bereits vorhanden) sinnvoll; für
  Standorte ohne LTE hingegen `0` (Fallback 2G/3G) — konfigurierbar halten.

Achtung: Fest verdrahtete Band-/RAT-Locks nur, wenn die Netzsituation am Standort
bekannt ist — sonst verschlechtert man die Robustheit.

### 5.6 Verbindungs-Backoff & Fehlerklassifizierung

**Ist:** Retry-Verzögerung ist fix (3 min; nach 20 Fehlversuchen 60 min;
`wait_min`/`wait_mail`/`wait_sms`).

**Empfehlung:** Leichter **exponentieller Backoff** (z. B. 1→2→5→15 min, Deckel
60 min) reduziert Netz-/Energielast bei längeren Ausfällen. Fehlerursache dabei
grob klassifizieren (kein Netz vs. Server nicht erreichbar vs. TLS-Fehler) und in
`mqtt_con_err`/DTC differenzieren — die Marken existieren schon (40–44), sind aber
lückenhaft belegt.

### 5.7 Timeouts NICHT kürzen (bei festem Standort)

**Klarstellung nach Rahmenannahme „fester Standort":** Timeouts zu *reduzieren*
ist hier **nicht** hilfreich, eher schädlich. Der Sinn kurzer Timeouts ist
„schnell aufgeben, um etwas Besseres zu finden" — das gibt es ortsfest nicht,
weil sich die Funklage nicht ändert. An einem dauerhaft schwachen Standort ist
die Verbindung, *wenn* sie steht, langsam; ein zu kurzer QIOPEN-/QIACT-/HTTP-GET-
Timeout bricht dann genau den langsamen-aber-funktionierenden Transfer ab.

Richtig ist die **Trennung zweier Fragen**: „Gibt es überhaupt Netz?" schnell
beantworten (über Registrierung/Signal, Kap. 5.1/5.2) und nur bei *nein* zügig in
langen Backoff gehen (Kap. 5.6, spart Strom — relevant wegen Batteriebetrieb).
Läuft dagegen bereits ein Transfer, dann **großzügige, aber begrenzte** Timeouts
mit Watchdog-Reset (`LONG_WD_32` vorhanden) lassen. Die bestehenden Timeouts
sind daher eher beizubehalten/zu verlängern, nicht zu kürzen.

### 5.8 Kleinere Punkte

- `AT+CMEE=2` ist gesetzt — gut; die verbose `+CME ERROR`-Texte sollten im
  Fehlerfall auch ins Protokoll wandern (aktuell nur teils im Debug-Mode sichtbar).
- Nach Modemtausch/Werksmodul greift `init_gsm()` bereits selbstheilend
  (analog zur BT-Init) — die Signal-Robustheit oben ist davon unabhängig.

---

## 6. Priorisierte To-do-Liste (neu gewichtet für festen Standort)

| Prio | Maßnahme | Aufwand |
|------|----------|---------|
| 1 | Standort-adaptive Netzwahl: `gsm_netscan()` + bestes Netz merken/locken (Kap. 7) | groß |
| 2 | GPS-gegateter Scan-Trigger, empfangs-gegatet (Kap. 8) | mittel |
| 3 | Menüpunkt „Besten LTE Empfang ermitteln" für manuellen Trigger (Kap. 8.3) | mittel |
| 4 | `AT+QCSQ` (RSRP/SINR/RSRQ) messen + loggen (5.1) | mittel |
| 5 | CEREG-Status 2/3/0 differenziert behandeln (5.2) | klein |
| 6 | `QIACT?`-Prüfung + Retry der Kontext-Aktivierung (5.3) | klein |
| 7 | TCP-Keepalive aktivieren (5.4) | klein |
| 8 | Exponentieller Backoff + Fehlerklassifizierung (5.6) | mittel |

Nicht mehr empfohlen: Timeouts kürzen (siehe 5.7) — bei festem Standort kontraproduktiv.

---

## 7. Standort-adaptive Netzwahl (`gsm_netscan()`)

**Ziel:** einmalig / selten das *tatsächlich beste* Netz am Standort ermitteln und
dauerhaft nutzen — statt sich auf `AT+COPS=0` (automatisch) zu verlassen.

**Warum nicht einfach `COPS=0`?** Der Automatikmodus wählt nach der
Prioritätsliste der SIM (EF_PLMNsel), **nicht nach Signal**. Genau deshalb bringt
ein eigener Mess-und-Merk-Lauf Mehrwert.

### 7.1 Ablauf des Scans

```
gsm_netscan()  (nur ausführen, wenn Batteriespannung ok):
  1. Modem an, eingebucht, Command-Mode.
  2. AT+COPS=? (bzw. AT+QCOPS) -> Liste sichtbarer Netze:
        je Eintrag: <stat>, Name, <PLMN numerisch>, <AcT>
        stat: 1=verfügbar 2=aktuell 3=gesperrt ; AcT: 0=GSM 2=UTRAN 7=E-UTRAN(LTE)
  3. Für jedes verfügbare (stat 1/2, nicht 3) Netz:
        AT+COPS=1,2,"<PLMN>",<AcT>   // manuell einbuchen
        auf CEREG=1 warten (Timeout, z. B. 30–60 s)
        AT+QCSQ / AT+CESQ            // RSRP/SINR/RSRQ messen
        Ergebnis (PLMN, AcT, RSRP, SINR) merken
  4. Bestes Netz nach RSRP/SINR wählen.
  5. In fp speichern: bestes PLMN + AcT (+ Band), Top-2 optional,
     Scan-Referenzposition (lat/lon), Scan-Zeitstempel.
  6. Bestes Netz locken: AT+COPS=1,2,"<PLMN>",<AcT>
     (oder =4,... = manuell mit Auto-Fallback) + optional AT+QCFG="band",...
  7. Fallback-Sicherheit: findet der Scan nichts Verwertbares oder scheitert er,
     bisheriges Bestes behalten bzw. AT+COPS=0 — nie schlechter als vorher.
```

### 7.2 Neue Parameterblock-Felder (`fp`)

Analog zu `fp.radnet`/`fp.csq`:

- `fp.plmn` — bestes Netz (numerische PLMN, MCC+MNC).
- `fp.act` — RAT des Gewinners (0/2/7).
- optional `fp.band` — gelocktes Band.
- `fp.plmn2`/`fp.act2` — zweitbestes Netz (Fallback vor Rescan).
- `fp.scanlat` / `fp.scanlon` — Position beim letzten Scan (Referenz).
- `fp.scantime` — Zeitstempel letzter Scan.

> Hinweis CRC: neue `fp`-Felder in den passenden CRC-Bereich der
> Parameterblock-Sicherung aufnehmen (vgl. `mqtt.c` `crcgen`-Bereiche um `fp.csq`).

### 7.3 Voraussetzungen / Grenzen

- **Multi-Netz-/Roaming-SIM** nötig — eine reine Ein-Netz-SIM zeigt bei `COPS=?`
  nur das Heimnetz, dann ist der Scan wirkungslos.
- `COPS=?` ist **störend** (trennt Registrierung, ~2–3 min). Daher nur gezielt
  auslösen (Kap. 8), nicht im Normalbetrieb.
- Scan **batterieabhängig** gaten (`LOWBAT` → verschieben).

---

## 8. Wann scannen? — GPS-gegateter, empfangs-gegateter Trigger

**Leitprinzip:** Nur scannen, wenn nötig. **Guter Empfang ⇒ nie ein Scan** —
und Maßstab für „gut" ist, dass die Verbindungen *tatsächlich klappen*, nicht ein
Signalpegel. Der Monats-Timer ist **kein** eigenständiger periodischer Scan; er
wird erst *innerhalb* des Problem-Zweigs abgefragt.

### 8.1 Entscheidungslogik

```
periodische Prüfung / nach Verbindungsversuch:

1. Verbindungen klappen?                 -> JA:  fertig, KEIN Scan
                                                 (egal wie alt der letzte Scan)
2. NEIN, N-mal in Folge fehlgeschlagen:
     a) GPS-Fix gültig UND Distanz zur Scan-Referenz >= Schwelle (z. B. 300–500 m)
                                          -> SCAN (neuer Standort)
     b) sonst (nicht bewegt ODER kein Fix) und letzter Scan >= 1 Monat her
                                          -> SCAN (Umgebung neu bewerten)
     c) sonst                            -> gemerktes Netz behalten + Backoff
```

Details:

- **„Netzprobleme" = N aufeinanderfolgende Fehlversuche** (Registrierung / QIACT /
  MQTT-Connect), nicht ein einzelner Hänger. Zähler existiert sinngemäß bereits
  (`mailfail`/`smsfail`/`mqtt_con_err`); einen gemeinsamen Fehlerzähler nutzen.
- **GPS-Ausfall ≠ Umzug**: nur bei *gültigem* Fix die Distanz prüfen; „kein Fix"
  fällt in die Zeit-Regel (2b), löst nie fälschlich einen Umzugs-Scan aus.
- **Distanz mit Hysterese**: GPS streut → Schwelle ≥ 300–500 m, verglichen gegen
  `fp.scanlat/scanlon` (Position beim letzten Scan). Positionsdaten liefert
  `get_geopos()` (bereits vorhanden).
- **Erstinbetriebnahme** (keine Scan-Referenz) → einmal initial scannen.
- **Rate-Limit**: auch der „Umzugs"-Scan mit Mindestabstand (z. B. max. 1×/Stunde),
  gegen Thrashing.

### 8.2 Rolle des Monats-Timers

Fängt Verschlechterungen **ohne** Umzug ab (neuer/abgeschalteter Mast, Betreiber
schaltet Band ab, Bewuchs). Greift aber nur, wenn ohnehin schon Probleme bestehen
— bei gutem Empfang verstreicht der Monat folgenlos.

### 8.3 Menüpunkt „Besten LTE Empfang ermitteln" (manueller Trigger) — Spec

Manuelles Auslösen des Scans, z. B. bei Inbetriebnahme/Serviceeinsatz. Passende
Stelle: **Funknetzmenü** `radionet_menu()` in `sicom.c` (Zeile ~1523), direkt
neben der bestehenden Option 4 „Signalstärke".

Konkrete Änderungen (reine Spec, noch nicht im Code umgesetzt):

1. **Neuer Textkonstante** in allen `sprachen/sictxt_*.c` (analog `T_sstr`), z. B.
   `T_bestnet` = `"\fBesten LTE Empfang ermitteln\f…FR…\f…IT…"`. Deklaration in
   `sictxt.h`.

2. **Menüzeile** in `radionet_menu()` nach der Signalstärke-Zeile ergänzen und die
   Auswahlziffer erweitern:

   ```c
   putparameter (T_sstr, fp.csq,(0x01<<16)|('4'<<8), T_nil);   // bestehend: Signalstärke
   putparameter (T_bestnet, 0, (0x02<<16)|('5'<<8), T_nil);    // NEU: Option 5
   select ('6',1);   // war '5' -> jetzt bis Ziffer 6 zulassen
   ```

3. **Dispatch-Case** im Menü-Switch (`sicom.c`, bei den `0x527x`/`0x528x`-Cases,
   Nähe Zeile ~2017):

   ```c
   case 0x5275:
   case 0x5285:
     if (!(connect&(UART1|MQTT_LINK)))     // nur ohne aktive GSM/BT/MQTT-Verbindung
     {
       put2str(T_LF, T_bestnet); putln(T_dot3);   // "Besten LTE Empfang ermitteln ..."
       gsm_netscan();                             // Scan + merken + locken (Kap. 7)
       radionet_menu();                           // Menü mit neuem Ergebnis neu ausgeben
       mensel=0;
     }
     else put2str(T_LF, T_noacc);                 // kein Zugriff (Leitung belegt)
     break;
   ```

4. **Prototyp** `extern int gsm_netscan (void);` in `gsmio.h`, Implementierung in
   `gsmio.c` gemäß Kap. 7.1. Rückgabe: Anzahl gefundener Netze bzw. < 0 bei Fehler.

**Wichtig:** Wie die anderen Modemaktionen im Menü nur ausführen, wenn UART1 frei
ist (`!(connect&(UART1|MQTT_LINK))`), sonst `T_noacc`. Watchdog im Scan regelmäßig
mit `ResetWDT()` bedienen (langer Lauf) und `LONG_WD_32` setzen.

---

## 9. Referenzen

- Firmware: `sism555/gsmio.c`, `sism555/gsmio.h`, `sism555/mqtt.c`,
  `sism555/gpsio.c` (`get_geopos`), `sism555/sicom.c` (`radionet_menu`, Menü-Switch)
- `sism555/Doku/UART1_Owner_StateMachine.md`
- Quectel EG9x AT Commands Manual / QuecCell AT Commands (EG91/EG95)
- `AT+QCSQ` / `AT+COPS` Beschreibung (Quectel-Forum-Doku, M2MSupport, Eseye)

*Hinweis: Alle Empfehlungen sind additiv — die bestehende, funktionierende
AT-Sequenz muss dafür nicht umgebaut werden.*

---

## 10. Umsetzungsplan (Reihenfolge)

Prinzip: **einzeln umsetzen**, jede Stufe eigener Commit (`T_version`-Bump gemäß
`CLAUDE.md`), dazwischen in Keil bauen und am Gerät testen. Reihenfolge nach
Abhängigkeit und aufsteigendem Risiko — Kleines/Unabhängiges zuerst, die großen
Brocken (Netzscan, Trigger) zuletzt, weil alles darauf aufbaut.

### Stufe 0 — erledigt
- **F12009 „SIM-Karte nicht erkannt"** (`AT+QSIMSTAT?` in `atcpin()`).
  Umgesetzt in Commit `ea067ca` (BT 5.50). Doku: `LTE_Fix.md`.

### Reihenfolge der offenen Punkte

| # | Maßnahme | Kap. | Abhängig von | Aufwand/Risiko | Warum an dieser Stelle |
|---|----------|------|--------------|----------------|------------------------|
| 1 | `AT+QCSQ` messen + loggen (RSRP/SINR/RSRQ) | 5.1 | – | klein | Fundament: liefert die Messgröße, die der Netzscan (#6) zum Vergleichen braucht. Für sich allein schon nützlich (Diagnose). |
| 2 | CEREG-Status 2/3/0 differenziert behandeln | 5.2 | – | klein | Lokale Änderung in `test_registration()`, unabhängig, sofort robustheitswirksam. |
| 3 | `QIACT?`-Prüfung + Retry der Kontext-Aktivierung | 5.3 | – | klein | In sich geschlossen in `set_gprs_config()`. |
| 4 | TCP-Keepalive aktivieren | 5.4 | – | klein | Eine Zeile in `mqtt.c` (State 10) aktivieren + testen. |
| 5 | Exponentieller Backoff + Fehlerklassifizierung | 5.6 | (2) hilfreich | mittel | Nutzt die klareren Fehlerursachen aus #2; betrifft `wait_min`/`wait_mail`/`wait_sms`. |
| 6 | `gsm_netscan()` — Netze scannen, bestes merken/locken | 7 | #1 (QCSQ) | groß | Kernstück. Braucht die Messgröße aus #1. Neue `fp`-Felder + CRC-Bereich. Einzeln, mit HW-Test. |
| 7 | Menüpunkt „Besten LTE Empfang ermitteln" (manueller Trigger) | 8.3 | #6 | mittel | Ruft `gsm_netscan()` auf — muss also nach #6 kommen. Gut zum manuellen Verifizieren von #6. |
| 8 | GPS-/empfangsgegatete Trigger-Automatik | 8.1/8.2 | #6, #7 | mittel | Automatischer Aufruf von `gsm_netscan()`; sinnvoll erst, wenn Scan + manueller Trigger stehen. |
| — | Band-/RAT-Lock fest verdrahten | 5.5 | #6 | mittel | **Optional/später.** Ergebnis von #6 kann das Band automatisch setzen; harte Locks nur bei bekannter Netzlage. |
| — | Timeouts | 5.7 | – | – | **Keine Code-Änderung.** Bei festem Standort bewusst NICHT kürzen (siehe 5.7). |

### Empfohlene Commit-/Versionsschritte
- **BT 5.51:** #1 QCSQ messen/loggen.
- **BT 5.52:** #2 + #3 + #4 (kleine Robustheit, ggf. ein gebündelter Commit).
- **BT 5.53:** #5 Backoff.
- **BT 5.54:** #6 `gsm_netscan()` (großer Einzel-Commit).
- **BT 5.55:** #7 Menüpunkt.
- **BT 5.56:** #8 Trigger-Automatik.
- (später) Band-Lock #5.5, wenn gewünscht.

Nach jeder Stufe: Encoding der ISO-8859-Dateien prüfen (kein `\r\r\n`), in Keil
bauen, am Gerät testen, `project-status.md` als `## Update <Version>` fortschreiben,
Commit `BT <Version>: <Kurzbeschreibung>`.
