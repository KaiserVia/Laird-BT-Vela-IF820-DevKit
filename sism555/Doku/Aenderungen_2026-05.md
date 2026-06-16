# Firmware-Aenderungen sism555 (Mai 2026)

Version: 5.57 -> 5.78

---

## 1. UART1 State Machine (neu)

**Dateien:** `hard.h`, `hard.c`

**Warum:**
UART1 wird per Hardware-MUX zwischen BT, GSM und GPS geteilt. Bisher wurde der MUX
per nacktem GPIO-Makro (`MUX_BT_Select`, `MUX_GSM_Select`) umgeschaltet. Das hatte
mehrere Probleme:
- Baudrate wurde dabei nicht angepasst
- Kein Tracking wer UART1 gerade besitzt
- BT-Terminal-Ausgabe (`putb()`) konnte ins GSM-Modem laufen
- Kein automatisches Restore der BT-Verbindung nach GSM-Zugriff

**Was:**
Neue Funktionen `uart1_request(requester, baud)` und `uart1_release(requester)`:

```c
// hard.h
enum { MUX_NONE=0, MUX_BT, MUX_GSM, MUX_GPS };
extern uchar uart1_owner;
extern uchar uart1_request (uchar requester, uint baud);
extern void  uart1_release (uchar requester);
```

Verhalten:
- `uart1_request`: Schaltet MUX + Baudrate atomar, suspendiert BT (`connect &= ~UART1`)
  wenn von BT weg geschaltet wird. Thread-safe via `__disable_irq()`/`__enable_irq()`.
- `uart1_release`: Gibt UART1 frei. Wenn ein Nicht-BT-Owner freigibt und BT_LINK aktiv
  ist, wird automatisch `uart1_request(MUX_BT, 115200)` aufgerufen (Auto-Restore).
- Gleicher Owner erneut: nur Baudrate wird aktualisiert, kein MUX-Wechsel.

---

## 2. Baudraten-Konfiguration mit BT_BAUD / GSM_BAUD Makros

**Dateien:** `btio.h`, `btio.c`, `gsmio.c`, `gsmio.h`, `main.c`, `libtool.c`, `hard.c`, `sicom.c`

**Warum:**
Die Betriebsbaudraten waren vorher an ~20 Stellen hartcodiert. Aenderung der Baudrate
erforderte manuelle Anpassung jeder einzelnen Stelle (fehleranfaellig).

**Was:**
Neue Makros in `btio.h`:
```c
#define BT_BAUD  (fp.btmodem==Laird ? 460800 : 307200)
#define GSM_BAUD 460800
```

Betriebsbaudraten (unveraendert gegenueber Original):

| Modul | Baudrate | AT-Kommando |
|-------|----------|-------------|
| Laird 730-SA | 460800 | `ATS521=460800` |
| RN4678 | 307200 | `SU,10` |
| UC15/EG91 | 460800 | `AT+IPR=460800;&W` |

Alle Betriebsstellen verwenden jetzt `BT_BAUD` bzw. `GSM_BAUD`.
Baudrate kuenftig nur noch an einer Stelle (btio.h) aenderbar.

Ausnahmen (bleiben hartcodiert):
- Fallback-Erkennung: 115200 als Startrate (Modul-Default vor Konfiguration)
- `init_bluetooth()` nach Factory-Reset (`SF,1`): 115200 (Modul startet mit Default)
- `init_bluetooth()` nach `SU,10` + Reboot: 307200 (erst jetzt konfiguriert)
- `init_gsm()` Probe-Sequenz: startet mit 115200 bevor `AT+IPR` gesendet wird

---

## 3. Fallback-Erkennung erweitert

**Dateien:** `btio.c`, `gsmio.c`

**Warum:**
Module die noch mit aelterer Firmware auf hoeherer Baudrate konfiguriert sind,
muessen beim Einschalten trotzdem gefunden werden.

**Was:**
Erkennung testet jetzt 4 Stufen statt 2-3:

- **Laird**: `460800 -> 115200 -> 9600 -> 307200`
- **RN4678**: `115200 -> 9600 -> 307200 -> 460800`
- **GSM**: `115200 -> 9600 -> 307200 -> 460800`

- `btio.c` `init_bluetooth()`: Laird und RN4678 jeweils 4 Versuche
- `gsmio.c` `init_gsm()`: GSM-Modem 4 Versuche (vorher: 115200, 460800, 9600)

---

## 4. Init_BT_ch / Init_GSM_ch als State-Machine-Wrapper

**Dateien:** `btio.c`, `gsmio.c`

**Warum:**
Alle bestehenden Aufrufer sollen ohne grosse Umbauten von der State Machine profitieren.

**Was:**
- `Init_GSM_ch(baud)` ruft jetzt intern `uart1_request(MUX_GSM, baud)` auf
  (vorher: `MUX_GSM_Select` + `osDelay` + `Init_UART1`)
- `Init_BT_ch(baud)` ruft jetzt intern `uart1_request(MUX_BT, baud)` auf + CTS-Check

---

## 5. Alle `Init_UART1(0)` durch `uart1_release()` ersetzt

**Dateien:** `btio.c`, `gsmio.c`, `libtool.c`, `measure.c`, `mqtt.c`, `sicom.c`

**Warum:**
Nacktes `Init_UART1(0)` deinitialisierte UART1, aber:
- Setzte `uart1_owner` nicht zurueck
- Kein automatisches BT-Restore
- Nachfolgendes `MUX_BT_Select` war oft vergessen oder an falscher Stelle

**Was:**

| Datei | Stelle | Vorher | Nachher |
|-------|--------|--------|---------|
| btio.c | Ende von `init_bluetooth`, `test_BT`, `send_bt_info`, `set_bt_name`, `set_bt_pin` | `Init_UART1(0)` | `uart1_release(MUX_BT)` |
| gsmio.c | `gsm_power(0)` | `Init_UART1(0); MUX_BT_Select` | `uart1_release(MUX_GSM)` |
| gsmio.c | Ende `init_gsm()` | `Init_UART1(0)` | `uart1_release(MUX_GSM)` |
| libtool.c | Ende `modem_com()` | `Init_UART1(0); MUX_BT_Select` | `uart1_release(uart1_owner)` |
| measure.c | BT Timeout | `Init_UART1(0)` | `uart1_release(MUX_BT)` |
| mqtt.c | MQTT Fehler-Handler | `Init_UART1(0); MUX_BT_Select` | `uart1_release(MUX_GSM)` |

---

## 6. Zertifikats-Upload (sicom.c `upload_file()`)

**Warum:**
- Nacktes `MUX_GSM_Select` vor `file_to_uc15` umging die State Machine
- Nach Upload fehlte `uart1_release` -> BT-Terminal blieb stumm
- `putln(T_csucc)` ging ins GSM-Modem statt an den BT-User
- Kein Fehler-Output bei fehlgeschlagenem Upload

**Vorher:**
```c
readline(fname, sizeof(fname), 'a');
MUX_GSM_Select;
if (file_to_uc15(filesize, fname) > 0)
    putln(T_csucc);
```

**Nachher:**
```c
readline(fname, sizeof(fname), 'a');
putln(T_wait);                          // User-Feedback vor Stille
uart1_request(MUX_GSM, 115200);         // State Machine: GSM uebernimmt
fehler = file_to_uc15(filesize, fname);
uart1_release(MUX_GSM);                 // Auto-Restore BT
if (fehler > 0)
    putln(T_csucc);                     // Erfolg sichtbar auf BT
else
    puterror(GSM_ERROR, -1);            // Fehler sichtbar auf BT
```

Ebenso nach `modem_start()`:
- Vorher: `MUX_BT_Select` (nur GPIO)
- Nachher: `uart1_release(MUX_GSM)` (kompletter State-Machine-Uebergang)

---

## 7. RN4678 Reboot-Fix (btio.c `init_bluetooth()`)

**Warum:**
`putstr(T_sr1)` benutzt `putb()`, welches `connect & UART1` prueft. Waehrend
`init_bluetooth()` ist `BT_LINK` noch nicht gesetzt -> `connect & UART1 = 0` ->
Reboot-Kommando "R,1\r" wurde nie gesendet. Modul blieb im Command-Mode, 
nachfolgendes `$$$` ergab `ERR1`.

**Vorher:**
```c
putstr(T_sr1);                      // BROKEN: putb() sendet nicht
Init_BT_ch(307200);
osDelay(1000);
result=bt_command(T_$, T_cmdp, 10300);  // ERR1 - bereits in CMD mode
```

**Nachher:**
```c
result=bt_command(T_sr1, T_reb, 10500); // bt_command schreibt direkt an UART1 TX
Init_BT_ch(115200);
putc('.'); osDelay(1000); ResetWDT();   // 2s warten mit visueller Rueckmeldung
putc('.'); osDelay(1000); ResetWDT();
newline();
bxi=rxi;                               // RX-Puffer leeren vor $$$
result=bt_command(T_$, T_cmdp, 10300);  // Funktioniert jetzt korrekt
```

---

## 8. CTS-Pruefung in putb() (sio.c)

**Warum:**
Ohne CTS-Check konnte `putb()` in UART1 schreiben waehrend das BT-Modul noch nicht
bereit war (CTS HIGH). Das fuehrte zu verlorenen Bytes und Zeichensalat.

**Was:**
Vor dem Warten auf THRE (Transmitter Hold Register Empty) wird jetzt auf CTS LOW
gewartet:
```c
while (FIO2PIN & CTS)               // Warte auf CTS LOW (Modem empfangsbereit)
{
    if (WDTV < 1000) { ResetWDT(); return(-1); }
}
while (!(U1->LSR & THRE)) ...      // Dann warte auf TX-FIFO frei
```

---

## 9. GSM-Init Guard (gsmio.c `init_gsm()`)

**Vorher:** `if (connect & UART1) return;` -- Abbruch wenn irgendwer UART1 nutzt
**Nachher:** `if (connect & GSM_LINK) return;` -- Abbruch nur wenn GSM bereits aktiv

**Warum:** Die alte Pruefung verhinderte GSM-Init wenn BT verbunden war. Die neue
erlaubt GSM-Init auch bei aktiver BT-Verbindung (State Machine verwaltet den Wechsel).

---

## 10. EMGOFF-Pin entfernt (hard.h, hard.c, mqtt.c, libtool.c)

**Was:**
- `#define EMGOFF (1<<20)` auskommentiert
- Alle `FIO0CLR = EMGOFF` / `FIO0SET = EMGOFF` Zugriffe entfernt
- `FIO0DIR` Initialisierung ohne EMGOFF

**Warum:** Pin wird in aktueller Hardware nicht mehr verwendet (UC15/EG91 haben kein
Emergency-Off, Abschaltung erfolgt ueber PWRKEY).

---

## 11. Reboot-Funktion (libtool.c `reboot()`)

**Neu:** `if (connect & MQTT_LINK) MQTT_fast_disconnect(1);` vor Hardware-Abschaltung.

**Warum:** MQTT-Verbindung sauber schliessen bevor der Watchdog ausgeloest wird.

---

## 12. USB Host Thread Schutz (measure.c)

**Vorher:** `osSignalSet(usbh_thread_id, 1)` ohne Pruefung ob Thread existiert
**Nachher:** `else osSignalSet(usbh_thread_id, 1)` -- nur wenn Thread-Erzeugung erfolgreich

**Warum:** Nullpointer-Zugriff wenn `osThreadCreate` fehlschlaegt.

---

## 13. Reboot-Ereignis (sprachen/sictxt.c, sictxt.h)

**Was:**
- Neuer Event-Text `T_reb[] = "Reboot"` in der Ereignisliste
- `REBOOT` als Ereignisnummer in `evno[]` und `evtxt[]` hinzugefuegt

**Warum:** Reboot-Ereignisse werden jetzt im Protokoll sichtbar protokolliert.

---

## 14. Automatische BT-Konfiguration nach Werksreset (main.c)

**Datei:** `main.c`

**Was:**
Neue Zeile im Startup nach der `fp.btmodem`-Pruefung:
```c
else if (fp.serno[0]) init_bluetooth();
```

**Warum:**
Nach einem Werksreset ist `fp.btmodem=0`, aber die Seriennummer (`fp.serno`) bleibt
erhalten. Bisher musste BT manuell ueber die Standard-Konfiguration (J/N-Abfrage)
reaktiviert werden. Jetzt wird `init_bluetooth()` beim Einschalten automatisch
aufgerufen, wenn ein BT-Modul nicht konfiguriert aber eine Seriennummer vorhanden ist.

**Schutz:** `init_bluetooth()` bricht intern ab bei:
- UART1-Konflikt (`connect & UART1`)
- CTS high (kein Modem angeschlossen)
- Seriennummer ungueltig (nicht-numerisch oder letzte 4 Digits = 0)

---

## 15. Laird init_bluetooth() startet mit 460800 Baud (btio.c)

**Datei:** `btio.c`

**Vorher:** `Init_BT_ch(115200)` - Startbaudrate 115200 fuer Laird-Erkennung
**Nachher:** `Init_BT_ch(460800)` - Startbaudrate 460800

**Warum:**
Laird-Module die bereits auf 460800 konfiguriert sind (Normalbetrieb), werden jetzt
beim ersten Versuch erkannt. Retry-Reihenfolge: 460800, 115200, 9600, 307200.

---

## 16. Laird PIN-Setting dynamisch in init_bluetooth() (btio.c)

**Datei:** `btio.c`

**Vorher:**
```c
if (bt_command(T_BTK,T_OKNZ,300))      // T_BTK = "AT+BTK=\"" — Kommando unvollstaendig!
```
Das Kommando `AT+BTK="` wurde ohne PIN-Wert und ohne schliessendes Anfuehrungszeichen
gesendet. Das Laird-Modul ignorierte das fehlerhafte Kommando und behielt seine
Factory-Default-PIN (`1234`). Alle Laird-Geraete hatten dadurch die gleiche PIN.

**Nachher:**
```c
connect|=UART1;
putstr(T_BTK);                          // "AT+BTK=\""
putnumber(fp.btpin,0xC4);              // 4-stellige PIN (letzte 4 Digits von fp.btpin)
putc('"');                              // Schliessendes Anfuehrungszeichen
bxi=rxi;
putc(CR);                               // Kommandoabschluss
if (wait_ok_time(300)>0)               // OK erhalten?
```

`fp.btpin` wird berechnet als `atoi(fp.serno)*10000+pin` (6-stellig aus Seriennummer).
`0xC4` gibt davon die letzten 4 Stellen aus (Laird unterstuetzt nur 4-stellige PINs).

**Auswirkung fuer Nutzer:**
- **Neu initialisierte Laird-Geraete** (Werksreset, init_bluetooth) haben jetzt eine
  individuelle PIN statt "1234". Beim Bluetooth-Pairing muss die neue PIN eingegeben werden.
- **Bereits gepaarte Geraete** sind nicht betroffen, solange kein Re-Init stattfindet.
- **RN4678**: Keine Aenderung, dort war die PIN schon immer aus der Seriennummer (6-stellig).

**KLAERUNGSBEDARF:** Ist die Aenderung von einheitlicher PIN "1234" auf individuelle
Seriennummer-PIN bei Laird gewuenscht? Betrifft alle Geraete nach erneutem init_bluetooth().

---

## 17. AT Parser Reset in Baudrate-Retry-Loop (btio.c)

**Datei:** `btio.c`

**Was:**
Nach jedem fehlgeschlagenen Baudraten-Versuch bei Laird-Erkennung wird jetzt ein
AT-Reset gesendet:
```c
connect=UART1;
putstr(T_AT);           // Sende AT\r um Modul AT Parser zurueckzusetzen
connect=concpy;
osDelay(300);           // Warte auf Modul-Antwort
bxi=rxi;                // Empfangspuffer leeren
```

**Warum:**
Das Laird-Modul kann nach einem fehlgeschlagenen Kommando (z. B. AT&F1 mit falscher
Baudrate) in einem undefinierten Parser-Zustand sein. Das einfache `AT\r` setzt den
Parser zurueck und das `bxi=rxi` leert den Empfangspuffer vor dem naechsten Versuch.

---

## 18. Modultyp- und Baudraten-Ausgabe bei init_bluetooth() (btio.c)

**Datei:** `btio.c`

**Was:**
Beide Erkennungspfade (Laird und RN4678) geben jetzt zu Beginn den Modultyp und die
Startbaudrate auf dem Terminal aus:

Laird:
```c
put2str(T_bt,T_laird);                         // "Bluetooth Laird"
putstr(T_dpkt); putnumber(460800,0); newline(); // ": 460800"
```

RN4678:
```c
put2str(T_bt,T_rn4678);                        // "Bluetooth Micro"
putstr(T_dpkt); putnumber(115200,0); newline(); // ": 115200"
```

Bei jedem Retry wird zusaetzlich die aktuell getestete Baudrate ausgegeben.

**Warum:**
Sichtbares Feedback waehrend der BT-Initialisierung. Der Benutzer sieht welches Modul
erkannt wird und welche Baudraten getestet werden.

---

## 19. RN4678 Silence Gap in Retry-Loop (btio.c)

**Datei:** `btio.c`

**Was:**
```c
osDelay(500);    // Silence gap fuer $$$ Kommandomodus Erkennung
bxi=rxi;         // Empfangspuffer leeren
```

**Warum:**
Der RN4678 benoetigt eine Ruhepause (keine Daten auf UART) vor dem `$$$`-Kommando,
um in den Command-Mode zu wechseln. Ohne den Silence Gap wurde `$$$` nach einem
Baudratenwechsel zu frueh gesendet und ignoriert.

---

## 20. Versionsnummer 5.68 -> 5.78 (sprachen/sictxt.c)

**Datei:** `sprachen/sictxt.c`

**Was:** `T_version[] = "5.78"` (vorher "5.68")
