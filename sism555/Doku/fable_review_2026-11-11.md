# Code- und Doku-Review 2026-11-11 (Firmware 5.137, Branch `connection-pin`)

Geprüft wurde der Quellcode unter `sism555/` gegen sich selbst und gegen die Doku. Es wurde **nichts gebaut,
nichts geflasht und kein Quellcode geändert** - alle Befunde stammen aus dem Lesen des Codes und aus dem
Keil-Aufrufgraphen `Objects/sism.htm`. Jeder Befund nennt, ob er am Code nachvollzogen ("bestätigt") oder nur
plausibel ist ("plausibel", braucht einen Test oder eine Messung).

Vollständig gelesen: `btio.c`, `btio.h`, `sio.c`, `hard.c`, `measure.c`, `main.c`, `i2cm.c` (ab Zeile 60),
`libtool.c` (Schnittstellenwechsel, Modem-Terminal, Reboot), `sicom.c` (PIN, BT-Menü, Firmware-Update),
`ramcode.c` (Flash-Kommando). Abschnitt 4 (GSM/MQTT/Xmodem/Flash) und Abschnitt 5 (Doku) stammen aus zwei
getrennten Prüfläufen. Deren Befunde habe ich nur stichprobenartig am Code nachgelesen; wo das geschehen ist,
steht es dabei.

## Reihenfolge, in der ich die Punkte angehen würde

| Nr. | Befund | Schwere | Status |
|---|---|---|---|
| 1.1 | Thread-Stacks zu knapp (GSM-Thread 256 Byte bei 248 Byte Bedarf) | hoch | bestätigt (Aufrufgraph) |
| 1.2 | `flbuf` wird von I2C und Flash gemeinsam benutzt, zwei Threads | hoch | plausibel |
| 1.3 | `connect` wird von zwei Threads umgeschaltet | mittel | plausibel |
| 2.1 | `uart1_release(MUX_BT)` schaltet die BT-Ausgabe einer laufenden Sitzung stumm | mittel | bestätigt |
| 2.2 | `clear_comchange()` verschluckt BT-Verbinden/Trennen | mittel | bestätigt |
| 2.3 | UART1 treibt in ein abgeschaltetes Modul (BT_TIMEOUT) | mittel | plausibel, HW prüfen |
| 2.4 | Einmaliger Fehlschlag beim Hochlauf legt BT bis zum Stromzyklus lahm | mittel | bestätigt |
| 2.5 | Baud-Scan prüft 921600 und 230400 gar nicht | niedrig | bestätigt |
| 2.6 | Kein Wiederholversuch, wenn CTS beim Verbinden HIGH ist | niedrig | bestätigt |
| 2.7 | PIN ohne Sperre, Vergleich nach jeder Ziffer | mittel | bestätigt |
| 3.x | Puffergrenzen und Zeitvergleiche in `sio.c`/`i2cm.c` | niedrig bis mittel | bestätigt |
| 4.1 | `sendmail()` löscht Messdaten ohne Versand | hoch | bestätigt |
| 4.2 | Ohne Flash: zufällige Parameter, Sicherungskopie überschrieben | hoch | bestätigt |
| 4.3 | `protocol()`/Flash aus dem MQTT-Thread (Zeitsync) | hoch | plausibel |
| 4.4 | `connect==0` nach fehlgeschlagenem MQTT-Publish | hoch | plausibel |
| 4.5-4.9 | Spannungsausfall-ISR, Zeigerprüfung, MQTT-Längen, Xmodem | niedrig bis mittel | gemischt |

Die Befunde 1.1, 1.2, 4.3 und 4.6 gehören zusammen: alle vier können Mess- oder Parameterdaten beschädigen und
sind Kandidaten für `Analyseplan_Messdatenzeiger-Korruption.md`.

---

## 1. Nebenläufigkeit (Hauptschleife gegen GSM-/MQTT-Thread)

RTX läuft mit Round-Robin und 1 ms Zeitscheibe (`RTE/CMSIS/RTX_Conf_CM.c`: `OS_ROBIN 1`, `OS_ROBINTOUT 1`).
Hauptschleife, `GSM_Registration`, `MQTT_Connect` und `USB_HostService` haben dieselbe Priorität. Jeder dieser
Threads kann also jede Millisekunde an beliebiger Stelle unterbrochen werden. Der Code behandelt das an einigen
Stellen ausdrücklich (`osActiveThread`-Abfragen vor I2C), an anderen nicht.

### 1.1 Thread-Stacks sind zu knapp - `GSM_Registration` hat 8 Byte Reserve

**Was:** `measure.c:34-36` legt die Threads mit 256 Byte (`USB_HostService`, `GSM_Registration`) und 512 Byte
(`MQTT_Connect`) Stack an. Der Keil-Aufrufgraph (`Objects/sism.htm`) nennt:

| Thread | Stack | Größte bekannte Tiefe | + 64 Byte Kontext | Reserve |
|---|---|---|---|---|
| `GSM_Registration` | 256 | 184 (`atcpin -> modem_start -> gsm_power -> uart1_release -> uart1_request -> bt_release -> write_i2c_dev`) | 248 | 8 |
| `MQTT_Connect` | 512 | 360 (`... -> gsm_nettime -> clock_set -> protocol -> putnumber -> putc`) | 424 | 88 |
| `USB_HostService` | 256 | 144 | 208 | 48 |
| Hauptthread | 1024 | 816 (E-Mail/FAT-Pfad) | 880 | 144 |

Die 64 Byte sind der Registersatz, den Cortex-M3 und RTX bei jedem Threadwechsel auf den Stack des
unterbrochenen Threads legen. **Nicht enthalten** sind Aufrufe über den Funktionszeiger `pchar`: der Graph endet
bei `putc`, tatsächlich folgt `putb` mit 40 Byte, bei gestecktem USB mit bis zu 128 Byte
(`putb -> USBD_CDC_ACM_PutChar -> ... -> rt_psq_enq`).

**Warum schlecht:** Die Kette über `uart1_release -> uart1_request -> bt_release -> write_i2c_dev` (96 Byte) ist
erst mit der IF820-Arbeit entstanden (UART1-Owner-Zustandsmaschine, 5.51/5.127). Der GSM-Thread ist dadurch an
die Grenze gewachsen, ohne dass sein Stack angepasst wurde. Läuft er über, erkennt RTX das beim nächsten
Threadwechsel (`OS_STKCHECK 1`) und ruft `os_error()` auf. Die Funktion ist leer und kehrt nicht zurück
(`sism.htm`: `os_error` ruft sich selbst). Das Gerät steht dann bis zu 32 s und startet per Watchdog neu; im
Protokoll steht nur "Watchdog-Reset", ohne Hinweis auf die Ursache. Läuft er nur wenig über, bevor geprüft wird,
überschreibt er den Stack des Nachbar-Threads.

**Wie beheben:**
- `measure.c:34-36`: `USB_HostService` 512, `GSM_Registration` 768, `MQTT_Connect` 1024.
- `RTX_Conf_CM.c`: `OS_PRIVSTKSIZE` ist die Summe aller dieser Stacks in Worten. Heute 768 Worte = 3072 Byte,
  belegt 1024 Byte. Für 2304 Byte reicht der Wert, ich würde ihn trotzdem auf 1024 Worte setzen (RAM prüfen).
- `os_error()` füllen: Fehlercode protokollieren (`protocol()` mit einem neuen Code) und gezielt neu starten,
  damit ein Stack-Überlauf im Gerätelog erkennbar ist.
- Nach jeder größeren Änderung `Objects/sism.htm` auf "Max Depth" der drei Thread-Funktionen prüfen.

### 1.2 `flbuf` ist gleichzeitig I2C-Puffer und Adresspuffer der Flash-Kommandos

**Was:** `write_i2c_dev()` kopiert seine Daten nach `flbuf[0..3]` (`i2cm.c:60`), `read_i2cdev()` schreibt
`flbuf[0]` und liest nach `flbuf` (`i2cm.c:81-91`). `flash_adress()` legt Seiten- und Byteadresse in
`flbuf[1..3]` ab (`ramcode.c:39-58`), `flash_command()` setzt `flbuf[0]` auf den Opcode und schiebt dann
`flbuf[0..3]` per SPI hinaus (`ramcode.c:113-135`). Kein Mutex, keine Interruptsperre.

**Warum schlecht:** Beide Seiten laufen in beiden Threads:
- I2C aus der Hauptschleife: `setswitches()`/`reset_switches()` bei jeder Anzeige, `bt_release()` jede Minute
  (`measure.c:180-181`, ohne `osActiveThread`-Abfrage), `communication_change()`.
- I2C aus dem GSM-/MQTT-Thread: `clear_comchange() -> get_com_status()` (`gsmio.c:278`, `gsmio.c:1059`),
  `uart1_release() -> bt_release()`.
- Flash aus der Hauptschleife: `store()` für jeden Messwert, `protocol()`.
- Flash aus dem Thread: `protocol()` (`gsmio.c:483`, `clock_logsrc`), Zertifikats- und Firmware-Übertragung.

Wird ein Thread zwischen `flash_adress()` und dem Hinausschieben der vier Adressbytes unterbrochen und der andere
ruft `write_i2c_dev()`, geht das Flash-Kommando mit fremder Adresse hinaus. Ein Schreibkommando trifft dann eine
falsche Seite, im ungünstigen Fall die Parameterseite. Das Fenster ist einige zehn Mikrosekunden breit, der
Fehler also selten - aber er passt zum Fehlerbild in `Analyseplan_Messdatenzeiger-Korruption.md` (dort bisher
nicht als Hypothese geführt). Zusätzlich ist der CMSIS-I2C-Treiber selbst nicht für zwei gleichzeitige Aufrufer
gebaut: ein zweiter `MasterTransmit` während eines laufenden Transfers liefert `ARM_DRIVER_ERROR_BUSY`, dessen
Rückgabewert `i2cm.c` nicht auswertet.

**Wie beheben:**
- Sofort und ohne Risiko: `i2cm.c` bekommt einen eigenen Puffer (`static uchar i2cbuf[8]`), `flbuf` bleibt dem
  Flash. Dasselbe für die zwei Direktaufrufe `I2Cdrv->MasterTransmit(IC37>>1,flbuf,3,...)` in `flash.c:39-40`
  und `libtool.c:1178-1179`.
- Danach: je ein `osMutex` um I2C-Transfer und Flash-Kommando, oder alle I2C-Zugriffe aus dem Thread heraus
  entfernen (der Thread setzt nur `thread_comchange`, die Hauptschleife liest IC54).
- `measure.c:180`: `bt_release()` im Minutentakt nur bei `!osActiveThread`.

### 1.3 `connect` wird von beiden Threads als Ausgabeweiche umgeschaltet

**Was:** Das Muster `cc=connect; connect=...; putstr(...); connect=cc;` steht in fast jeder BT-Funktion
(`bt_sm_show()` `btio.c:725-735`, `bt_event_line()` `btio.c:992-994`, `bt_command()` `btio.c:49-61`). Der
GSM-/MQTT-Thread benutzt dasselbe Muster (`gsm_nettime()` `gsmio.c:515`, `MQTT_fast_disconnect()` `mqtt.c:1747`).
`bt_command()` stellt am Ende nicht den gesicherten Wert her, sondern `concpy`.

**Warum schlecht:** Unterbricht die Zeitscheibe einen Thread, während `connect` umgestellt ist, sendet der andere
Thread seine Bytes an die falsche Schnittstelle. Beispiel: Die Hauptschleife gibt in `bt_sm_show()` eine
`[BT-Zustand]`-Zeile aus (`connect = concpy & ~UART1`), der GSM-Thread sendet in diesem Moment ein AT-Kommando -
es landet auf RS232 statt beim Modem, das Kommando läuft in den Timeout. Umgekehrt landet eine Zustandszeile im
AT-Strom. `bt_monitor_poll()` berücksichtigt das bereits ("connect während des Threads NICHT anfassen",
`btio.c:582`), `bt_sniff_poll()`/`bt_event_line()`/`bt_sm_show()` nicht; sie laufen laut `measure.c:363` auch bei
aktivem Thread. In der Praxis ist das Fenster klein, weil `rbuf1` kaum Daten bekommt, solange GSM UART1 hält.
Außerhalb des BT-Codes besteht dasselbe Problem für die Online-Messwertausgabe (`measure.c:411`).

**Wie beheben:**
- Kurzfristig: in `bt_sniff_poll()` die Ausgabe (nicht das Leeren von `rbuf1`) bei `osActiveThread` unterdrücken
  oder puffern.
- Richtig: Ausgabeziel als Parameter statt als globale Variable (`putstr_to(mask, text)`), `connect` bleibt der
  reine Verbindungszustand. Das ist ein größerer Umbau; bis dahin jede neue Stelle mit dem Muster gegen
  `osActiveThread` absichern.
- `bt_command()`: am Ende den beim Eintritt gesicherten Wert herstellen statt `concpy`. Dann entfallen die
  `concpy=connect;`-Zeilen vor jedem Aufruf (`btio.c:474`, `807`, `965`), die heute leicht vergessen werden
  (`test_BT()`, `send_bt_info()` setzen `concpy` nicht).

---

## 2. Bluetooth-Ablauf

### 2.1 `uart1_release(MUX_BT)` schaltet die Ausgabe einer laufenden BT-Sitzung stumm

**Was:** `hard.c:550-568`. Für `requester==MUX_BT` und IF820 läuft immer der Leerlauf-Zweig:
`uart1_owner=MUX_NONE; ... connect&=~UART1;` - auch wenn `connect&BT_LINK` gesetzt ist.

**Warum schlecht:** Mehrere Diagnosefunktionen fordern UART1 mit `Init_BT_ch()` an und geben es am Ende mit
`uart1_release(MUX_BT)` frei: Event-Sniff nach 180 s (`btio.c:1032`), Transfer-Test (`btio.c:1154`), Modulbefehl
(`btio.c:1373`), Factory-Reset (`btio.c:1230`). Verbindet sich währenddessen ein Kunde - beim Event-Sniff ist
genau das der Zweck -, setzt `communication_change()` `connect|=UART1|BT_LINK`. Läuft der Sniff danach ab, wird
`UART1` aus `connect` gelöscht: Die BT-Sitzung empfängt weiter Eingaben (die ISR-Weiche hängt an `BT_LINK`),
sendet aber nichts mehr. Der Kunde sieht ein eingefrorenes Menü, bis er trennt.

**Wie beheben:** In `uart1_release()` den Leerlauf-Zweig nur ohne Verbindung nehmen:

```c
else if (fp.btmodem==IF820)
{
 if (connect & BT_LINK) return;      // Sitzung laeuft: Owner MUX_BT und UART1-Ausgabe behalten
 ...
```

`measure.c:188-189` (BT_TIMEOUT) muss dann `connect&=~(UART1|BT_LINK)` **vor** `uart1_release(MUX_BT)` setzen.

### 2.2 `clear_comchange()` verschluckt echte BT-Wechsel

**Was:** `libtool.c:541-554`. Läuft kein Thread, setzt die Funktion `comstate=state; comchange=0;` ohne
`communication_change()` aufzurufen. Aufrufer außerhalb von Threads: `gpsio.c:197/252/263/313`, `sicom.c:430/476`,
`libtool.c:673/869/922`, `btio.c:1630/1639/1808`, `measure.c:300`.

**Warum schlecht:** Der Zweck ist, die Störflanke beim Ein-/Ausschalten von GSM/GPS zu verwerfen. Fällt in
dasselbe Fenster aber ein BT-Verbinden oder -Trennen, ist das Ereignis weg; EINT1 kommt nur bei der nächsten
Pegeländerung wieder.
- Trennen verschluckt: `connect&BT_LINK` bleibt gesetzt. Jede Ausgabe wartet in `putb()` auf CTS, RS232 wird
  träge, bis BT_TIMEOUT nach 6 Minuten das Modul abschaltet. Das ist die Fehlerklasse, die in `bt_diag_menu()`
  (Kommentar `btio.c:1194-1207`, 5.72) schon einmal gefunden und nur dort behoben wurde.
- Verbinden verschluckt: kein Banner, kein PIN-Prompt, und kein Timeout räumt auf, weil `bt_time` nur bei
  gesetztem `BT_LINK` zählt.

**Wie beheben:** In `clear_comchange()` die Bits, die nicht von der Versorgungsschaltung kommen, nicht verwerfen:

```c
if (state>=0)
{
 uchar bt_differs = (interfaces&BT_LINK) && !bt_cyspp_out &&
                    (((state&BT_LINK)==0) != ((connect&BT_LINK)!=0));
 if (osActiveThread || bt_differs || ((state^comstate)&RS232_LINK)) thread_comchange=1;
 else comstate=state;
}
```

`measure.c:308` wertet `thread_comchange` bereits aus. Zusätzlich als Sicherheitsnetz im Minutentakt: Pin und
`connect&BT_LINK` vergleichen und bei Abweichung `thread_comchange=1` setzen.

### 2.3 Nach BT_TIMEOUT treibt UART1 in ein abgeschaltetes Modul

**Was:** `measure.c:184-192`. Nach 6 Minuten ohne Empfang: `uart1_release(MUX_BT)`, dann
`PINMODE3&=~PULLDOWN` (Modulversorgung aus). Seit 5.127 lässt `uart1_release()` UART1 beim IF820 aktiv
(`hard.c:563-564`: MUX auf BT, `Init_UART1(IF820_BAUD)`). TXD1 liegt im Ruhezustand auf 3,3 V, RTS1 wird
getrieben. Das Modul bleibt bis zum nächsten Minutentakt stromlos (`measure.c:171`).

**Warum schlecht:** Bis zu 60 s lang liegt ein getriebener HIGH-Pegel an RXD (und der RTS-Pegel an CTS) eines
unversorgten Moduls. Das ist die Konstellation, vor der `HW_Massnahmen_IF820.md` Abschnitt 3 warnt
(Rückspeisung über die Eingangsschutzdioden, "tot bis lange stromlos"). Vor 5.127 waren die Pins in dieser Zeit
Eingänge mit Pull-up. Ob wirklich Strom fließt, hängt vom MUX (IC18/IC13) und von Serienwiderständen ab - das
habe ich nicht am Schaltplan geprüft. Ein unvollständig entladenes Modul wäre auch eine Erklärung für
"Modul stumm nach kurzer Stromunterbrechung".

**Wie beheben:** Im BT_TIMEOUT-Zweig vor dem Abschalten `Init_UART1(0)` aufrufen (Pins werden Eingänge) und
UART1 erst beim Wiedereinschalten in `measure.c:171` wieder aktivieren (`uart1_release(MUX_NONE)` wie in
`main.c:256`, nur wenn `uart1_owner==MUX_NONE`). Vorher mit dem Oszilloskop an 3P0V des Moduls messen, ob die
Spannung im abgeschalteten Zustand tatsächlich nicht auf 0 V fällt.

### 2.4 Ein einziger Fehlschlag beim Hochlauf legt BT bis zum nächsten Stromzyklus lahm

**Was:** `main.c:167-176`. Antwortet das IF820 beim Einschalten nicht auf zwei `/PING` und ist nicht verbunden,
wird `fp.btmodem=0` gesetzt und `init_bluetooth()` gerufen. Findet auch die nichts, läuft der Laird/RN4678-Zweig
(`AT&F1`, `$$$`, `SF,1` an ein IF820), am Ende F11010, `interfaces` ohne `BT_LINK`. Zur Laufzeit gibt es keinen
zweiten Versuch: der Minutentakt schaltet zwar die Modulversorgung wieder ein, ruft aber nie `test_BT()`.

**Warum schlecht:** Der Testlauf vom 2026-11-02 zeigte "1x Modul stumm nach 0,5 s stromlos". Ein Modul, das
einmal zu spät startet, ist danach bis zum nächsten Power-on-Reset unerreichbar, obwohl es wenige Sekunden
später normal arbeitet. Der nächtliche Neustart hilft nicht (der `BOOT_RES`-Zweig in `main.c:215-217` ist leer).
Außerdem geht mit `fp.btmodem=0` die Information "hier war ein IF820" verloren; wird der Parameterblock in dem
Zustand gesichert, steht 0 im Flash.

**Wie beheben:**
- `fp.btmodem` erst überschreiben, wenn eine Erkennung **erfolgreich** war (lokale Variable in
  `init_bluetooth()`); bei Fehlschlag alten Wert behalten und nur `interfaces&=~BT_LINK`.
- Den Laird/RN4678-Zweig überspringen, wenn vorher ein IF820 eingetragen war.
- Im Minutentakt: `fp.btmodem==IF820 && !(interfaces&BT_LINK) && !osActiveThread` -> `test_BT()`, bei Erfolg
  `interfaces|=BT_LINK` (auf z. B. 10 Versuche begrenzen).
- Kleiner Fehler im selben Pfad: `btio.c:1649-1650` kehrt bei CTS HIGH ohne `uart1_release(MUX_BT)` und ohne
  `connect=concpy` zurück; `uart1_owner` bleibt auf `MUX_BT` stehen.

### 2.5 Der Baud-Scan prüft 921600 und 230400 nicht

**Was:** `btio.c:1524` scannt `{115200, 460800, 921600, 307200, 230400, 9600}`. `Init_UART1()` (`hard.c:460-479`)
kennt nur 9600, 115200, 460800 und 307200; für jeden anderen Wert bleiben `DLL`/`FDR` unverändert.

**Warum schlecht:** "scan @921600" läuft in Wahrheit ein zweites Mal mit 460800, "scan @230400" mit 307200. Die
Diagnoseausgabe behauptet etwas, das nicht stattfindet; ein Modul auf 921600 oder 230400 wird nie gefunden.
Dasselbe gilt für `GSM_BAUD` (`gsmbaud`): ein anderer Wert als die vier wird stillschweigend ignoriert.

**Wie beheben:** Die beiden Einträge aus `bdscan[]` streichen oder die Teiler ergänzen (230400: `DLL=4`,
`FDR=5/8`; 921600 ist laut CLAUDE.md wegen des LPC1766 verworfen). `Init_UART1()` sollte bei unbekannter Baudrate
`FALSE` zurückgeben oder einen Diagnosecode setzen.

### 2.6 Kein Wiederholversuch, wenn CTS beim Verbinden HIGH ist

**Was:** `libtool.c:1041-1056`. Liefert `Init_BT_ch()` `FALSE` (CTS HIGH), wird die Verbindung nicht
eingetragen, `comchange` aber gelöscht.

**Warum schlecht:** Der Pin bleibt LOW, es kommt keine weitere Flanke, der Kunde bleibt ohne Banner verbunden.
Gleiche Folge wie 2.2 (Verbinden verschluckt).

**Wie beheben:** In diesem Fall `thread_comchange=1` stehen lassen (mit Zähler, z. B. 20 Versuche im Abstand von
250 ms) oder das Sicherheitsnetz aus 2.2 im Minutentakt nutzen.

### 2.7 PIN-Abfrage: keine Sperre, Vergleich nach jeder Ziffer

**Was:** `sicom.c:2167-2188`. Nach jeder Ziffer wird `fp.btpin==menu` geprüft; nach sechs falschen Ziffern
beginnt die Abfrage sofort neu. Es gibt weder Wartezeit noch Zähler noch Trennung.

**Warum schlecht:** Dass die PIN aus der Seriennummer im Gerätenamen ableitbar ist, steht bereits in
`BT_Zugriffsschutz_Konzepte.md`. Unabhängig davon macht die fehlende Sperre jede spätere, echte PIN wertlos: eine
Million Kombinationen sind über BLE in weniger als einer Stunde durchprobiert. Der Vergleich nach jeder Ziffer
akzeptiert außerdem eine PIN mit führender Null schon nach fünf Ziffern.

**Wie beheben:** Erst nach genau sechs Ziffern vergleichen. Nach drei Fehlversuchen `bt_disconnect()` und eine
Sperrzeit (z. B. 30 s, verdoppelnd), Fehlversuche mit einem W-Code protokollieren.

### 2.8 Kleinere Punkte im BT-Code

- **`bt_get_name()` prüft nur den Anfang** (`btio.c:340`): `wait_message1()` sucht `N=VIASIS_<serno>` als
  Teilstring. Ein Classic-Name `VIASIS_<serno>_BT` (Firmware 5.88-5.92) oder `..._BLE` gilt als korrekt und wird
  nie umbenannt. Abhilfe: bis zum Zeilenende lesen und exakt vergleichen.
- **`set_bt_name()`** (`btio.c:2002-2009`): `btnamebuf[40]`, für Laird `maxlen=40`. `getstr()` schreibt `n+1`
  Bytes (`sio.c:449`), also 41 - ein Byte hinter den Puffer. Nur Laird. Abhilfe: `char btnamebuf[41]`.
  Der IF820-Zweig der Funktion ist tot (`sicom.c:1881` ruft sie nur für Laird) und würde den BLE-Namen ohne
  `_BLE` setzen, was `bt_get_name()` beim nächsten Start wieder umschreibt. Entfernen.
- **`bt_module_cmd()`** (`btio.c:1317-1321`): Ist die Zeile voll, wird das Zeichen, das den Überlauf auslöst,
  verworfen (der Kommentar verspricht "Rest folgt als neue Zeile"). Abhilfe: nach der Ausgabe `rx[n_rx++]=c`.
- **Bluetooth deinstallieren** (`sicom.c:381-382`): treibt CYSPP 300 ms HIGH, sendet aber kein `/DIS`. Laut
  Kommentar zu `bt_disconnect()` (5.74) reicht das nicht zuverlässig. Das Modul bleibt außerdem sichtbar und
  verbindbar, obwohl die Firmware es nicht mehr bedient. Abhilfe: `bt_disconnect()` und danach `/AX` + `SBTP C=0`.
- **`identify()`** (`sicom.c:146-149`, `284-288`): gibt für Roving und Laird den Modultyp aus, für IF820 nichts.
- **`bt_conn_update()`** (`btio.c:959-968`): `bt_command()` setzt beim Start `bxi1=rxi1` und verwirft damit
  Ereignisse, die zwischen dem Leeren in `bt_sniff_poll()` und dem `/UCP` eingetroffen sind (z. B. `.CYSPP`).
  Folge nur für die Zustandsanzeige. Abhilfe: vor dem Senden `rbuf1` noch einmal über `bt_event_line()` leeren.
- **`bt_sm_minute()`**: Ist das Modul verborgen und per BT_TIMEOUT abgeschaltet, sendet der Minutentakt Befehle
  an ein stromloses Modul (CTS-Timeout, W11015). Abhilfe: `BT_MODULE_ON` in `bt_sm_cmd()` abfragen.

---

## 3. Serielle Ein-/Ausgabe, I2C, Hardware-nahe Routinen

### 3.1 `wait_message()`, `wait_message1()`, `wait_message_2()`, `wait_line()`: `cbuf` ohne Grenze

**Was:** `sio.c:919`, `958`, `992`, `1024`: `cbuf[cind++]=...` ohne Vergleich mit `CBUFSIZE` (3328 Byte laut
`hard.h:158`; der Kommentar "1312 Byte" in `hard.c:44` ist veraltet). Jedes empfangene Zeichen verlängert
zusätzlich den Timeout um `timeout/2`.

**Warum schlecht:** Kommt ein Datenstrom ohne den erwarteten Text (falsche Baudrate, 0xFF-Rauschen auf BLT-RXD,
eine lange Modemantwort), läuft `cind` über das Pufferende und überschreibt die folgenden Variablen (`probuf`,
`vbuf` - Protokoll- und Messdatenpuffer). Bei 460800 Baud dauern 3328 Byte 72 ms. Der Kommentar bei
`bt_scan_ping()` kennt die Timeout-Verlängerung, die fehlende Grenze nicht. Passt zu Hypothese 0c in
`Analyseplan_Messdatenzeiger-Korruption.md`.

**Wie beheben:** In allen vier Funktionen `if (cind>=CBUFSIZE-1) return(-1);` vor dem Schreiben, und eine harte
Gesamtzeit (z. B. `4*timeout`) zusätzlich zum Zeichen-Timeout.

### 3.2 `uart_read()`: Timeout per Gleichheit

**Was:** `sio.c:854`: `if (stoptime==mstimer) return(-1);`

**Warum schlecht:** Läuft der Thread in genau dieser Millisekunde nicht (Zeitscheibe eines anderen Threads, lange
ISR), wird der Zeitpunkt verpasst. Die Schleife endet dann erst, wenn `mstimer` nach 49 Tagen wieder vorbeikommt;
da sie den Watchdog nicht bedient, folgt nach 32 s ein Watchdog-Reset statt eines sauberen Timeouts. Betrifft
Xmodem-/Ymodem-Empfang (`xmodem.c:104`, `478`) und `gsmio.c:999`, `1847`.

**Wie beheben:** `if ((int)(mstimer-stoptime)>=0) return(-1);`

### 3.3 `mstimer` wird von Wartefunktionen auf 0 gesetzt

**Was:** `sio.c:908`, `948`, `1013`: `if (ltime<mstimer) { mstimer=0; ltime=timeout; }`, danach
`while (ltime>mstimer)`.

**Warum schlecht:** Die Funktionen schreiben in den globalen Millisekundenzähler, den die Timer-ISR hochzählt.
Alle anderen Zeitmessungen (`(uint)(mstimer-t0)`) springen in dem Moment. Der Fall tritt erst nach 49,7 Tagen
Laufzeit ein und wird durch den nächtlichen Neustart verdeckt - sobald der Neustart entfällt, wird er wirksam.

**Wie beheben:** Überlaufsicher rechnen: `uint t0=mstimer; ... while ((uint)(mstimer-t0) < limit)`, `mstimer`
nirgends schreiben.

### 3.4 I2C-Timeouts sind nicht überlaufsicher

**Was:** `i2cm.c:62-64`, `84-87`, `92-95`: `rdelay=T1TC+200; ... if (T1TC > rdelay)`. `T1TC` läuft mit 1 MHz frei
und läuft alle 71,6 Minuten über.

**Warum schlecht:** Liegt `rdelay` knapp hinter dem Überlauf, ist `T1TC > rdelay` sofort wahr: der Transfer gilt
als gescheitert, `get_com_status()` schreibt einen I2C-Fehler ins Protokoll. Liegt `rdelay` knapp vor dem
Überlauf und der Bus hängt, wird die Bedingung 71 Minuten lang nicht wahr. Beides sehr selten.

**Wie beheben:** Wie in `sendspi()`: `uint t0=T1TC; ... if ((uint)(T1TC-t0) > 200)`.

### 3.5 Weitere Punkte

- **`getstr()`** (`sio.c:449`) schreibt `n+1` Bytes. Jeder Aufrufer braucht einen Puffer von `n+1`; siehe 2.8.
- **`waitcms()`** (`sio.c:215-230`) und **`wait_return()`** (`sio.c:794-798`): erhöhen `bxi`, vergleichen aber
  `rbuf[rxi]` (zuletzt empfangenes Zeichen) statt `rbuf[bxi]`. Kommen zwei Zeichen kurz hintereinander, wird das
  gesuchte übersprungen. `waitcms()` wartet außerdem ohne `osThreadYield()` und ohne Watchdog-Bedienung.
- **`modem_terminal()`** (`libtool.c:599`, `603`): `gbuf[bx2++]` und `pbuf[bpi++]` lesen mit Post-Inkrement, die
  ISRs schreiben mit Prä-Inkrement (`gbuf[++rx2]`). Die Anzeige hängt ein Zeichen hinterher und gibt zuerst ein
  altes Byte aus. Richtig: `gbuf[++bx2]`, `pbuf[++bpi]`. Dort auch `cbuf[cind++]` ohne Grenze (`libtool.c:573`).
- **`flash_command()`** (`ramcode.c:131`): `SSP1->SR&!RNE` ist immer 0 (`!RNE` ist 0). Gemeint ist
  `!(SSP1->SR&RNE)`. Wirkt sich nicht aus, weil `BSY` dieselbe Information trägt, sollte aber korrigiert werden.
- **`EINT1_IRQHandler`/`EINT2_IRQHandler`** (`hard.c:264`, `270`): `EXTINT|=(1<<n)` schreibt alle gerade
  gesetzten Flags zurück und löscht damit auch fremde. Richtig: `EXTINT=(1<<n);`.
- **`main.c:101-102`**: `!(fp.i2cdev&IC58_b&&(fp.ex12>=2))` ist für jedes Gerät ohne Plus/FT wahr. Scheitert
  `InitI2C_Devices()` aus einem anderen Grund (DPP, LED-Spot), wird zusätzlich "I2C-Baustein fehlt" protokolliert.
  Gemeint ist `(!(fp.i2cdev&IC58_b) && (fp.ex12>=2))`.
- **`main.c:249-250`**: `if (fp.vmm<5) t_amp_en=20; else t_amp_en=20;` - beide Zweige gleich.
- **`Init_UART1()`** (`hard.c:457`) setzt bei jedem Aufruf die FIFOs zurück. `Init_BT_ch()` wird vor fast jedem
  Modulbefehl gerufen; ein gerade eintreffendes Ereignis des Moduls geht dabei verloren.
- **`hard.c:64`**: `concpy` liegt per `__at(0x2007e1d8)` auf einer festen Adresse - vermutlich ein Rest aus einer
  Fehlersuche. Entfernen, sonst kollidiert es bei wachsendem RAM-Bedarf mit dem Linker.

---

## 4. GSM, MQTT, Xmodem, Flash, Dateisystem

Diese Befunde stammen aus einem getrennten Prüflauf über `gsmio.c`, `mqtt.c`, `xmodem.c`, `flash.c`, `rtc.c`,
`i2cm.c`, `fio.c` (alle vollständig gelesen). 4.1, 4.2, 4.6 und 4.9 habe ich selbst am Code nachgelesen
("bestätigt"), die übrigen sind aus dem Prüflauf übernommen und mit dessen Einstufung gekennzeichnet. Sie liegen
außerhalb der Bluetooth-Arbeit, einige sind aber schwerer als alles in Abschnitt 2.

### 4.1 `sendmail()` löscht Messdaten, die nie verschickt wurden (hoch, bestätigt)

**Was:** `gsmio.c:1018-1034` behandelt `state==33` als "Server hat die Nachricht angenommen" und ruft bei
anstehender Datenmail `delete_data()` (`gsmio.c:1027`). `state` wird aber auch an zwei anderen Stellen auf 33
gesetzt: `gsmio.c:848` (`mode<4`, reiner Verbindungstest aus dem Konfigurationsmenü) und `gsmio.c:914`
(`isalpha(fp.emto[0])` falsch).

**Warum schlecht:** (a) Scheitern Datenmails wegen falscher SMTP-Einstellungen und der Anwender testet die
korrigierte Konfiguration, löscht der erfolgreiche Test alle gespeicherten Messdaten, ohne dass eine Mail
hinausging. (b) Beginnt die Empfängeradresse mit einer Ziffer (`1234@firma.de`), sendet jede geplante Datenmail
nichts und löscht trotzdem die Daten.

**Wie beheben:** Eigenes Merkflag, das nur im Zweig `gsmio.c:962` nach der 250-Antwort auf das Mail-Ende gesetzt
wird; die Löschung daran binden. Adressen auf "nicht leer" prüfen statt mit `isalpha()`.

### 4.2 Ohne externes Flash laufen zufällige Parameter und die Sicherungskopie wird überschrieben (hoch, bestätigt)

**Was:** `flash.c:414-464`. Die Ladeschleife läuft nur bei `flashsize!=0`. Meldet sich das Flash beim Einschalten
nicht, bleibt `retry` auf 4: weder die LPC-Kopie (`flash.c:421`) noch `werkparameter()` (`flash.c:456-458`) wird
benutzt, `fp` enthält den zufälligen RAM-Inhalt. `flash.c:463` schreibt diesen Inhalt anschließend ohne Bedingung
in den LPC-Programmspeicher (`parameter_to_progmem()`).

**Warum schlecht:** Ein einziger Kaltstart, bei dem das Flash nicht antwortet, zerstört genau die Sicherung, die
für diesen Fall gedacht ist.

**Wie beheben:** Bei `flashsize==0` aus `fp_cp_area` laden, beide Kennungen prüfen, sonst `werkparameter()`;
`parameter_to_progmem()` nur mit geprüftem `fp` aufrufen.

### 4.3 `protocol()` und Uhr-Stellen laufen im MQTT-Thread (hoch, Aufrufkette bestätigt, Folge plausibel)

**Was:** `Con_MQTT_server()` (Thread) -> `set_gprs_config` -> `test_registration` -> `gsm_nettime(0)`
(`gsmio.c:609`) -> `clock_set()` -> `clock_logsrc()` -> `protocol()`. `protocol()` ändert `probuf`/`fp.pro_adr`
und schreibt bei voller Seite ins Flash (`P_fpar`, `flbuf`, SSP1). Die Hauptschleife speichert währenddessen
Messwerte. `atcpin()` vermeidet das ausdrücklich (`!osActiveThread`, `gsmio.c:482`), der neuere Zeitsync-Pfad
nicht. Die Kette steht auch im Aufrufgraphen (Abschnitt 1.1, `MQTT_Connect`).

**Warum schlecht:** Gleiche Fehlerklasse wie 1.2, aber mit zwei Flash-Schreibern statt Flash gegen I2C: ein
Threadwechsel mitten in `flash_command()` ergibt eine beschädigte Mess- oder Protokollseite.

**Wie beheben:** Im Thread nur die gelesene Zeit ablegen (Flag + Werte), `clock_set()` in der Hauptschleife
aufrufen. Zusammen mit 1.2 lösen (Mutex um Flash-Zugriffe).

### 4.4 Fehlgeschlagenes `putb()` beim MQTT-Publish hinterlässt `connect==0` (hoch, Pfad bestätigt, Auslöser plausibel)

**Was:** Die Publish-Funktionen (`mqtt.c:236-238`, `416-422`, `452-459`, `790-823`) setzen `connect=UART1` und
kehren bei `putb()<0` ohne Wiederherstellung zurück. `MQTT_fast_disconnect()` (`mqtt.c:1738-1752`) sichert dann
den schon falschen Wert und löscht `UART1` - `connect` ist 0.

**Warum schlecht:** Hält das Modem CTS während eines großen Publish länger als das Limit in `putb()`, bleibt das
RS232-/USB-Terminal stumm, bis das Kabel neu gesteckt wird oder der nächtliche Neustart kommt.

**Wie beheben:** Ein gemeinsamer Ausgang je Publish-Funktion, der `connect` wiederherstellt. Hängt mit 1.3
zusammen.

### 4.5 Spannungsausfall-ISR (mittel, plausibel)

`flash.c:33-45` startet einen I2C-Transfer aus `flbuf` und ruft sofort `flash_wbuf1()`, das `flbuf[0..3]`
überschreibt, während der I2C-Treiber noch sendet. IC37 kann dadurch ein falsches Register erhalten, die
Analogversorgung bleibt an und verkürzt die Haltezeit für die Seitenschreibvorgänge. Trifft die ISR zwischen
`fp.md_page++` und `flash_erase()` in `mdata_to_flash()` (`flash.c:554-564`), wird ohne Löschen auf eine belegte
Seite geschrieben. Abhilfe: eigener Puffer für den I2C-Rahmen; nächste Seite löschen, bevor der Zeiger
weitergeschaltet wird.

### 4.6 Zeigerprüfung aus 5.96 ist zu weit und unvollständig (mittel, bestätigt)

`flash.c:440` akzeptiert `fp.md_adr` bis 2047, `vbuf` hat 1056 Byte; `store()` schreibt dann hinter den Puffer.
`pro_page`, `pro_start_page` und `pro_adr` werden gar nicht geprüft; ein `pro_page` außerhalb des Protokollrings
lässt die Schleifen `while (seite!=fp.pro_page)` in `mqtt.c:573-581` und `xmodem.c:346-360` nicht enden.
Abhilfe: `md_adr` gegen die Seitengröße und auf Vielfache der Satzlänge prüfen, die Protokollzeiger ebenso, beide
Schleifen durch die Ringgröße begrenzen. Gehört zu `Analyseplan_Messdatenzeiger-Korruption.md`.

### 4.7 MQTT-PUBLISH vertraut den Längen vom Server (mittel, bestätigt im Code; braucht eine unerwartete Nachricht)

`mqtt.c:1018-1263`: Offsets in `uchar` (`i`, ab Themenlänge 250 läuft er über), `compare()` ohne Grenze gegen
`mqtt_len`, Schleifen `uchar` gegen `ushort` (`mqtt.c:1074`, `1187`: Kommando über 255 Byte endet nie),
`memcpy` von 80 Byte nach `url[80]` ohne Abschluss (`mqtt.c:1122`), `certname[16]` ebenso (`mqtt.c:1158`),
`certsize` kann unterlaufen (`mqtt.c:1244`), Restlänge über 65535 läuft in `mqtt_len` über (`mqtt.c:1600-1606`).
Abhilfe: einmal am Anfang des PUBLISH-Zweigs `mqtt_head+2+toplen+2 <= mqtt_len <= MQBFSIZE` prüfen, Offsets als
`ushort`, Mindestlänge je Kommando, Zeichenketten abschließen. Der Server ist der eigene; das Risiko ist ein
Fehler auf Serverseite oder ein kompromittierter Zugang.

### 4.8 Weitere Befunde aus dem Prüflauf (übernommen)

| Stelle | Befund | Einstufung |
|---|---|---|
| `xmodem.c:428-447` | `plus_upload()` kopiert ab festem Offset 43; stimmt nur bei 8-stelliger Seriennummer (CRC wird an der richtigen Stelle gerechnet und besteht trotzdem) | mittel, bestätigt |
| `xmodem.c:453-511` | `plus_download()` prüft die Blocknummer nicht, `offset` wächst unbegrenzt -> `cbuf`-Überlauf bei wiederholtem Block | mittel, plausibel |
| `gsmio.c:1837-1874` | Firmware-Kopie aus dem Modem: nur 5 ms Warten auf `OK`, kein Leeren vor dem nächsten `AT+QFREAD`; CRC 0 vom Server überspringt den Vergleich (`gsmio.c:1913`) | mittel, plausibel |
| `gsmio.c:1502-1503` | Zertifikatsliste: bei drei belegten Plätzen wird alles gelöscht und das neue nicht eingetragen, Rückgabe trotzdem Erfolg | mittel, bestätigt |
| `rtc.c:476-480` | `get_turnswitch()` wird zweimal gelesen; liefert der zweite Aufruf 0, wird `pset=255` | mittel, plausibel |
| `mqtt.c:1614-1618` | Wiederfreigabe des UART1-Interrupts steht hinter zwei `break`; die Hauptschleife fängt das ab (`measure.c:350`), solange kein Thread läuft | niedrig |
| `xmodem.c:58`, `110` | `uchar blkno` gegen `blkno+1`: Block 256 wird nie angenommen, Dateien über 255 KB nicht empfangbar (`sism.bin` hat 172 KB) | niedrig, bestätigt |
| `xmodem.c:293-301` | `modem_eot()`: `repeat` läuft auf 255 über, `CAN` wird bei Erschöpfung nie gesendet | niedrig, bestätigt |
| `i2cm.c:234-239` | `setswitches()`: `if (retry--) break;` bricht nach dem ersten Fehlversuch ab und meldet Erfolg (richtig in `reset_switches()`: `!retry--`) | niedrig, bestätigt |
| `fio.c:31-44` | `mkdir_viasis()`: nach fünf Fehlversuchen ist `retry` -1, gilt als Erfolg; Erfolg im fünften Versuch gilt als Fehler | niedrig, bestätigt |
| `fio.c:64-72` | `close_files()` setzt die Dateizeiger nicht auf NULL; `USB_tools.c` schließt Dateien doppelt | niedrig, bestätigt |
| `fio.c:107-123` | `read_usb_file()` bricht bei `maxblock` still ab; abgeschnittene Datei nicht erkennbar | niedrig, bestätigt |
| `gsmio.c:228` | `if (i)` nach erschöpfter Baudraten-Schleife (`i==-1`) ist wahr; gemeint `i>0` | niedrig, bestätigt |
| `gsmio.c:1093`, `1119` | `!isalpha(fp.smssc[0])` ist für jede Telefonnummer wahr: die Service-Center-Nummer wird nie gesetzt | niedrig, bestätigt |
| `gsmio.c:603-605` | `clean_cpy()` ohne Quellgrenze: bei eingeschaltetem Modem-Echo läuft `iccid[23]` über | niedrig, plausibel |
| `gsmio.c:1719-1725` | `filesize` wird bei HTTP-Status ungleich 200 uninitialisiert gelesen | niedrig, bestätigt |
| `gsmio.c:445-446`, `516`, `589-597` | `putln(cmd); bxi=rxi;` leert nach dem Senden; ein Threadwechsel dazwischen verwirft die Antwort (3 s Timeout) | niedrig, plausibel |
| `flash.c:216`, `221` | `SSP1->SR&!TFE`, `&!TNF` immer 0 (wie `ramcode.c:131`) | niedrig, bestätigt |

### 4.9 `sendblock()` meldet einen Erfolg im letzten Versuch als Fehler (niedrig, bestätigt)

`xmodem.c:210-214`: Wird der Block im letzten Versuch quittiert, endet die Schleife mit `retry==0` und `error==0`;
`if (retry) return(1)` greift nicht, zurück kommt `XMODEM_ERROR_RETRYEXCEED`. Abhilfe: zuerst `if (!error)
return(1);`.

---

## 5. Doku gegen Code

### 5.1 Korrigiert (Doku an den Code angepasst)

| Datei | Was stand dort | Jetzt |
|---|---|---|
| `CLAUDE.md` | "Gerätename BLE + Classic einheitlich `VIASIS_<serno>`" | Classic `VIASIS_<serno>`, BLE `VIASIS_<serno>_BLE` (`btio.c:380-383`) |
| `CLAUDE.md` | Branch "HARDWARE-ABHAENGIG, nicht auf normale Boards flashen", Stray-Zeichen "OFFEN" | gilt nur für 5.101-5.119; ab 5.120 alle Boards; Stray-Prüfung mit 5.124 bestanden; `BT_CONN_TRIGGER` ergänzt |
| `CLAUDE.md` | "Anwender baut R125 auf B um, danach `online-bt` wiederholen" | erledigt 2026-11-10 mit Ergebnissen |
| `CLAUDE.md` | bleibt CTS HIGH -> `F11009` | `F11018`; `F11009` nur im Laird/RN4678-Zweig (`btio.c:1628`, `1650`) |
| `CLAUDE.md` | vier Stellen treiben CYSPP, "müssen überarbeitet werden" | Stand 5.137: zwei treiben nicht mehr, zwei bewusst geblieben |
| `CLAUDE.md` | Transport-Erkennung "NICHT umgesetzt" | erledigt 5.130-5.132, ursprüngliche Notiz als solche markiert |
| `CLAUDE.md` | `ev_line`/`bt_event_line()` "wieder entfernen" | nur die `[BT-Event]`-Ausgabe ist temporär; die Funktion speist `/UCP` und die Zustandsmaschine - **ein Rückbau nach alter Anweisung hätte beides stillgelegt** |
| `CLAUDE.md` | Aufräumliste, PPCP-Fokus, "Durchsatz-Optionen A/B nicht umgesetzt" | `static`-Funktionen, 5.69, vier fehlende Testfunktionen ergänzt; PPCP in 5.75 entfernt; Option B (`/UCP`) aktiv |
| `Doku/Uebersicht.md` | "R125 entfernen (Versuch läuft)", Konzept mit `E=1`/`.CYSPPSTART`, Ymodem-Abbruch "spätere Version" | R125 nach GND, `/AX`//`/A`/`SBTP`, umgesetzt in 5.130 |
| `Doku/Konzept_CYSPP_Zustandsmaschine.md` | Titel "Entwurf"; Phase 3 "für Update und GSM-Sitzung" | Hinweis auf Ist-Stand; GSM-Sperre in 5.134 entfernt |
| `Doku/IF820_Pin_Referenz.md` | CONNECTION "unverdrahtet", Events "nicht geparst", CYSPP an "genau einer Stelle" getrieben, `F11009` | P0.19 auf umgebauten Boards, Event-Parser seit 5.130, zwei Stellen, `F11018` |
| `Doku/BT_CONNECTION_vs_CYSPP.md` | "`BT_CONN_GPIO` muss dort auf 0 stehen", Gegentest offen | seit 5.120 generell 0; Gegentest bestanden; 5.137-Auslöser |
| `Doku/IF820_Config_Kandidaten.md` | "Nichts davon ist umgesetzt"; A3 "Intervall 3 s", "nur netzbetriebene Geräte" | Verweis auf Umsetzungsstand; 200 ms, alle Geräte (`T_sap`); `/UCP` ergänzt |
| `Doku/BT_Anschluss_Belegung.md` | fünf Signale; `CTS_WAIT_MS`; `bt_cmdmode()` = Command-Modus erzwingen; `F11009` | CONNECTION ergänzt; 50 ms / 1000 ms; gezieltes Trennen; `W11015`/`F11018` |
| `Doku/IF820_EZSerial_Konzept_Mapping.md` | liest sich wie Ist-Stand | als historisches Konzept markiert, Abweichungen aufgezählt |
| `Doku/Board-Inventar.md` | 26SC9966 "zuletzt getestet 2026-11-09" | 2026-11-10 mit `online-bt`-Ergebnis |

### 5.2 Nicht korrigiert - braucht eine Entscheidung oder einen Build

- **`Doku/UART1_Owner_StateMachine.md`** ist nicht in UTF-8 gespeichert (wie auch die C-Dateien); ich habe die
  Datei deshalb nicht angefasst. Falsch sind dort: überall `115200` (tatsächlich `BT_BAUD`, beim IF820 460800,
  bzw. `GSM_BAUD`); die Diagramme zeigen beim Freigeben immer `Init_UART1(0)`, beim IF820 bleibt UART1 seit 5.127
  aktiv; die Bedingung ist `requester != MUX_BT && connect & BT_LINK` (`hard.c:550`). Die Tabellen weiter unten
  sind schon richtig. Außerdem stimmt "Returns TRUE if granted" nicht: `uart1_request()` lehnt nie ab, jeder
  Aufrufer verdrängt den bisherigen Besitzer.
- **`Doku/Diagnose-Codes.csv`** ist die Quelle für `dtc_text.h`; Änderungen gehören mit einem Generatorlauf und
  Build zusammen, deshalb hier nur gelistet:
  - Zeile 87 ff.: Die "veralteten" Alt-Codes 204, 300, 521, 550, 650, 701, 800, 1000 werden weiterhin erzeugt
    (`main.c:259`, `i2cm.c:161`, `measure.c:288`, `gsmio.c:483`, `gpsio.c:495`, `gsmio.c:1723`, `flash.c:474`).
  - Die Codes 840-845 (`MQTT_ERROR + mqtt_con_err`, `measure.c:335`, Werte aus `gsmio.c:132/139/412/456/492/614`)
    fehlen in der Tabelle.
  - 11005 "Kein BT-Modul gefunden" steht als aktiv, wird aber nirgends ausgelöst (nur `dtc_codes.h:20`).
  - 13010: Ursache "Protokollkommando ungültig" trifft nicht zu; der Code meldet einen abgebrochenen
    Xmodem-/Ymodem-Download (`sicom.c:1176`, `1287`).
  - 11019 siehe 5.3.
- **`Doku/Diagnose-Codes_Konzept.md`**: Spaltenliste ohne `Typ`; "Generator erzeugt `dtc_codes.h`" (er erzeugt
  nur `dtc_text.h`, `dtc_codes.h` ist Handarbeit); Ausgabeformat "Fehler 11001 Bluetooth" (tatsächlich
  `F11001 PING keine Antwort`, `sio.c:630-635`); "00000-10999 frei" (dort liegen Ereignisse und Alt-Codes).
- **Nicht geprüft:** die übrigen Dateien unter `Doku/` (Testpläne, HW-Bewertungen, `ble-*.md`,
  `Zeitsync_Plan.md`, `parameterspeicher.md`, Werkzeug-Anleitungen bis auf die Menünummern) und der größte Teil
  von `project-status.md` (Tagebuch, ältere Kapitel sind als Historie zulässig).

### 5.3 Kommentare im Code, die nicht mehr stimmen

- `btio.c:1633`: "I11019 - nur geloggt, wenn Retries noetig waren (v>0)". Die Schleife zählt `v` auch beim
  ersten Erfolg auf 1 hoch, I11019 wird also bei **jeder** Reboot-Verifikation protokolliert. Entweder
  `if (v>1)` oder den Text in der CSV ändern ("Wert = Anzahl Versuche, 1 = sofort").
- `btio.c:1826`: "echten Fehler meldet init_bluetooth (F11005)" - 11005 wird nie ausgelöst.
- `btio.h:194-196`, `btio.c:331`, `374`: "Name = VIASIS_<serno>" ohne den BLE-Zusatz.
- `btio.h:262-265`: "OFFEN: ob die Stray-Zeichen ... zurückkehren" - mit 5.124 geprüft.
- `hard.c:44`: "cbuf (1056+256=1312 Byte)" - `CBUFSIZE` ist 3328.
- `hard.c:577-584`: Kommentar beschreibt `bt_displaced`/`bt_cmdmode()` in `uart1_request()`; beides gibt es
  dort nicht mehr.
- `hard.c:443`: "baud - 9600 oder sonst immer 460800" - vier Raten werden unterstützt, andere still ignoriert.
- `measure.c:361-362`: "liest ueber einen eigenen, von bxi unabhaengigen Index" - seit 5.92 über `bxi1`.
