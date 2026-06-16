# Fix: BT MUX Baudrate nach GSM-Kommunikation

**Datum:** 2026-04-10  
**Betroffene Dateien:** `sicom.c`, `gsmio.c`, `btio.h`  
**Problem:** Nach GSM-Modem-Kommunikation über UART1 bleibt die Baudrate auf 460800, obwohl das RN4678 BT-Modul 307200 erwartet → Zeichensalat auf BT-Schnittstelle.

---

## ⚠️ Festgelegte Betriebsbaudraten (2026-04-27)

| Schnittstelle | Modul | Betriebsbaudrate | Hinweis |
|---------------|-------|-----------------|---------|
| LTE (UART1) | Quectel UC15 / EG91 | **460800** | Nur diese Baudrate funktioniert zuverlässig |
| BT (UART1) | Microchip RN4678 | **307200** | Nur diese Baudrate funktioniert zuverlässig |
| BT (UART1) | Laird 730-SA | **460800** | Konfigurierbar via `ATS521` |

Die Betriebsbaudrate wird beim Initialisieren ins Modem gespeichert (`AT+IPR=...;&W` bzw. `ATS521=...` bzw. `SU,...`).  
Fallback-Erkennung beim Einschalten: 307200 → 115200 → 460800 → 9600.

---

## Ursache

UART1 wird per Hardware-MUX (IC55/IC56) zwischen BT, GSM und GPS umgeschaltet.  
`MUX_BT_Select` schaltet nur die MUX-Pins (CSAF/CSBF), setzt aber **nicht** die UART1-Baudrate zurück.

| BT-Modul | UART1 Baudrate | GSM Baudrate |
|----------|---------------|-------------|
| Laird 730-SA | 307200 | 460800 → Mismatch, wird per `Init_BT_ch()` korrigiert |
| **Roving RN4678** | **307200** | **460800 → Mismatch, wird per `Init_BT_ch()` korrigiert** |

---

## Änderungen

### 1. sicom.c — `upload_file()`: Nach `modem_start()`

**Vorher:**
```c
MUX_BT_Select;                          // MUX Bluetooth Leitungsauswahl
clear_comchange ();
```

**Nachher:**
```c
Init_BT_ch(fp.btmodem==Laird ? 460800 : 307200);  // MUX BT select and restore BT baud rate
bxi=rxi;                                           // Flush receive buffer - discard residual GSM bytes
clear_comchange ();
```

### 2. sicom.c — `upload_file()`: Nach `file_to_uc15()` Erfolg

**Vorher:**
```c
if (file_to_uc15(filesize, fname)>0)
    putln(T_csucc);                      // Erfolg ausgeben (ging ans GSM-Modem!)
```

**Nachher:**
```c
if (file_to_uc15(filesize, fname)>0)
{
    Init_BT_ch(fp.btmodem==Laird ? 460800 : 307200);  // Restore BT MUX and baud rate
    bxi=rxi;                                           // Flush receive buffer
    putln(T_csucc);                                    // Erfolg ausgeben
}
```

### 3. sicom.c — `upload_file()`: Nach `file_to_uc15()` Fehler

**Vorher:** Kein else-Zweig, MUX blieb auf GSM, keine Fehlermeldung.

**Nachher:**
```c
else
{
    Init_BT_ch(fp.btmodem==Laird ? 460800 : 307200);  // Restore BT MUX and baud rate
    bxi=rxi;                                           // Flush receive buffer
    put2str(T_err,E_gsm);                              // Error: GSM modem write failed
    newline();
}
```

### 4. gsmio.c — fehlender `#include "btio.h"`

**Compiler-Fehler:**
```
gsmio.c(1311): warning: #223-D: function "Init_BT_ch" declared implicitly
gsmio.c(1311): error:   #20: identifier "Laird" is undefined
```

`gsmio.c` verwendete `Init_BT_ch()` und das Makro `Laird`, die beide in `btio.h` deklariert/definiert sind, aber die Datei fehlte in den Includes.

**Vorher:**
```c
#include "gsmio.h"
#include "i2cm.h"
```

**Nachher:**
```c
#include "gsmio.h"
#include "btio.h"
#include "i2cm.h"
```

---

### 5. gsmio.c — `change_radionet()`: Nach AT+QCFG Kommando

**Vorher:**
```c
FIO0SET=GSM_DTR;
MUX_BT_Select;                          // MUX Bluetooth Leitungsauswahl
```

**Nachher:**
```c
FIO0SET=GSM_DTR;
Init_BT_ch(fp.btmodem==Laird ? 460800 : 307200);  // Restore BT MUX and baud rate
bxi=rxi;                                           // Flush receive buffer
```

---

## Warum `Init_BT_ch()` statt `MUX_BT_Select`

`MUX_BT_Select` ist nur ein GPIO-Makro:
```c
#define MUX_BT_Select  FIO0CLR=CSAF|CSBF   // Nur MUX-Pins
```

`Init_BT_ch()` macht beides:
```c
bool Init_BT_ch(uint baudrate) {
    osDelay(3);              // Warte auf letztes Zeichen
    MUX_BT_Select;           // MUX auf BT schalten
    Init_UART1(baudrate);    // UART1 Baudrate korrekt setzen
    osDelay(3);              // Warte auf Umschaltung
    ...
}
```

## Zusätzlich: `bxi=rxi` (Puffer-Flush)

Setzt den Lese-Index auf den Schreib-Index des UART1-Ringpuffers.  
Verwirft Restbytes die vom GSM-Modem noch im Puffer liegen, bevor BT-Kommunikation fortgesetzt wird.

---

## Geprüfte Stellen ohne Handlungsbedarf

| Datei | Zeile | Kontext | Warum OK |
|-------|-------|---------|----------|
| gsmio.c:78 | `gsm_power(0)` | UART1 wird deinitialisiert (`Init_UART1(0)`) | Nächster BT-Connect initialisiert neu |
| gsmio.c:147 | `gsm_power()` else | Nur bei `gsm_power(1)`, UART1 nicht aktiv | Kein BT-Traffic erwartet |
| libtool.c:681 | `modem_com()` | `Init_UART1(0)` direkt davor | UART1 ist aus |
| main.c:171 | Startup GPS-Test | BT noch nicht verbunden | Zeile 210-211 initialisiert korrekt |
| mqtt.c:1600 | MQTT-Thread Fehler | GSM wird abgeschaltet | UART1 deinitialisiert |
| btio.c:41 | `bt_command()` | BT bereits aktiv, Baudrate passt | Nur MUX-Sicherstellung |

---

## Geraetespezifischer Ablauf

### Laird 730-SA

- Startbaudrate im Projekt: 460800
- Erkennungsversuch: zuerst direkt mit 460800, bei Bedarf ein zweiter Versuch mit 9600
- Zielbaudrate nach Konfiguration: 460800
- Relevante Stellen: `Init_BT_ch(460800)`, `Init_UART1(9600)`, `ATS521=460800`

### Microchip RN4678APL

- Startbaudrate im Projekt: 115200
- Erkennungsversuch: zuerst mit 115200, bei Bedarf ein zweiter Versuch mit 307200
- Zielbaudrate nach Konfiguration: 307200
- Relevante Stellen: `Init_BT_ch(115200)`, `Init_UART1(307200)`, `SU,10`

### Quectel EG91

- Startbaudrate im Projekt: 115200
- Modeminitialisierung: AT-Dialog laeuft zunaechst mit 115200, dann wird die Modemkonfiguration gesendet
- Zielbaudrate nach Konfiguration: 460800
- Relevante Stellen: `Init_GSM_ch(115200)`, `AT+IPR=460800;&W`
- Funktyp im Projekt: LTE

### Quectel UC15

- Startbaudrate im Projekt: 115200
- Modeminitialisierung: AT-Dialog laeuft zunaechst mit 115200, dann wird die Modemkonfiguration gesendet
- Zielbaudrate nach Konfiguration: 460800
- Relevante Stellen: `Init_GSM_ch(115200)`, `AT+IPR=460800;&W`
- Funktyp im Projekt: UMTS

### Kurzfassung

| Geraet | Startbaudrate | Endbaudrate |
|--------|---------------|-------------|
| Laird 730-SA | 460800 | 460800 |
| RN4678APL | 115200 | 307200 |
| EG91 | 115200 | 460800 |
| UC15 | 115200 | 460800 |

---

## Alle Funktionsaufrufe: Kommunikation MCU <-> BT / LTE

### Bluetooth

| Aufrufstelle | Funktion | Zweck |
|---|---|---|
| main.c:156 | `test_BT()` | Power-on: Prüft ob BT-Modul antwortet |
| main.c:207-208 | `Init_BT_ch()` | Nach Firmware-Update: UART1+MUX auf BT öffnen |
| sicom.c:371 | `init_bluetooth()` | Menü: BT-Modul erstmalig konfigurieren (AT-Dialog) |
| sicom.c:452 / 457 | `Init_BT_ch(BT_BAUD)` | Nach Zertifikat-Upload: MUX+Baud auf BT zurückstellen |
| sicom.c:1850 | `send_bt_info()` | Menü: BT-Informationen, MAC, Firmware-Version abfragen |
| sicom.c:1855 | `set_bt_pin()` | Menü: PIN-Nummer im BT-Modul setzen |
| sicom.c:1861 | `set_bt_name()` | Menü: Friendly-Name im BT-Modul setzen |
| gsmio.c:1310 | `Init_BT_ch(BT_BAUD)` | Nach Netzumschaltung: MUX+Baud auf BT zurückstellen |
| libtool.c:635 / 1037 | `Init_BT_ch()` | Interne Hilfsfunktionen: MUX auf BT schalten |

`init_bluetooth()` enthält den vollständigen AT-Konfigurationsdialog je nach Modul:

- **Laird 730-SA:** `AT&F1` → `ATE0` → `ATS521=460800` → `ATS502=1` → `ATS504=1` → `ATS507=2` → `ATS508=1000` → `AT+BTK` → `AT+BTN` → `ATS514=60` → `ATS512=4` → `ATS538=1` → `AT&W` → `ATZ`
- **RN4678APL:** `$$$` → `SF,1` → `R,1` → `SN,…` → `SA,2` → `SS,SPP` → `SY,4` → `SP,…` → `SG,0` → `SU,10` → `SO,,` → `R,1` → `$$$` → `---`

### LTE / GSM

| Aufrufstelle | Funktion | Zweck |
|---|---|---|
| main.c:138 | `Init_GSM_ch(460800)` | Watchdog-Reset: GSM-Kanal sichern / trennen |
| main.c:161 | `test_gsm(1)` | Power-on: Prüft ob GSM-Modul antwortet |
| main.c:215 | `gsm_power(2)` | Nach Firmware-Update: MUX+UART auf GSM initialisieren |
| sicom.c:377 | `init_gsm()` | Menü: GSM/LTE-Modul erstmalig konfigurieren |
| sicom.c:417 | `modem_start()` | Zertifikat-Upload: Modem einschalten und warten |
| sicom.c:449-450 | `Init_GSM_ch()` → `file_to_uc15()` | Zertifikat-Upload: Datei ins Modem schreiben |
| sicom.c:1497 | `test_registration()` | Menü: Netzregistrierung prüfen |
| sicom.c:2014 | `change_radionet()` | Menü: GSM / UMTS / LTE Netz umschalten |
| measure.c:199 | `sendsms(0)` | Messintervall: SMS senden |
| measure.c:218 | `sendmail(6)` | Messintervall: Email senden |
| measure.c:229 | `GSM_Registration` (Thread) | Messintervall: Im Mobilfunknetz einbuchen |
| mqtt.c:1357 | `set_gprs_config(0)` | MQTT-Thread: GPRS-Verbindung aufbauen |
| mqtt.c:1135 | `Download_firmware()` | MQTT-Befehl: Firmware über LTE laden |
| mqtt.c:1175 | `activate_cert()` | MQTT-Befehl: TLS-Zertifikate im Modem aktivieren |
| mqtt.c:1257 | `cert_to_uc15()` | MQTT-Befehl: Zertifikat ins Modem schreiben |

---

## Sequenzdiagramm Kommunikation MCU <-> BT / LTE

```
LPC1766          BT-Modul               GSM/LTE-Modul        MQTT-Server
   |                  |                        |                    |
   |=== Power-on ===================================================|
   |---test_BT()----->| AT / $$$               |                    |
   |<--OK/CMD>--------|                        |                    |
   |---test_gsm(1)----|--------Init_GSM_ch---->|                    |
   |                  |        AT / ATI        |                    |
   |<-----------------|--------OK + ID---------|                    |
   |                  |                        |                    |
   |=== Erstinitialisierung (Menue) =================================|
   |---init_bluetooth()                        |                    |
   |---bt_command(AT&F1 / SF,1)-->|            |                    |
   |<---OK/AOK--------------------|            |                    |
   |---bt_command(ATS521 / SU,10)>|            |                    |
   |<---OK/AOK--------------------|            | (Baudrate wechselt)|
   |---bt_command(ATZ / R,1)----->|            |                    |
   |<---OK/REBOOT-----------------|            |                    |
   |                              |            |                    |
   |---init_gsm()                 |            |                    |
   |---Init_GSM_ch(115200)--------|----------->|                    |
   |---AT / ATI-------------------|----------->|                    |
   |<----------------------OK + UC15/EG91------|                    |
   |---AT+IPR=460800;&W-----------|----------->|                    |
   |<---OK------------------------|            | (Baudrate wechselt)|
   |                              |            |                    |
   |=== Normalbetrieb: Messdaten ====================================|
   |---GSM_Registration (Thread)  |            |                    |
   |---modem_start()--------------|----------->|                    |
   |<---+CFUN: 1------------------|            |                    |
   |---test_registration()--------|----------->|                    |
   |<---+CREG/+CGREG/+CEREG-------|            |                    |
   |---set_gprs_config()----------|----------->|                    |
   |<---IP-Adresse----------------|            |                    |
   |                              |            |---TCP/TLS--------->|
   |<=====================MQTT-Verbindung aktiv===================>|
   |---Messdaten MQTT Publish-----|------------|------------------>|
   |<---MQTT-Commands (OTA / Cert / Netz)------|<-----------------|
   |                              |            |                    |
   |=== MQTT-gesteuerte Aktionen ====================================|
   |<---Download_firmware Befehl--|------------|<------------------|
   |---Download_firmware()--------|----------->|                    |
   |   GPRS + HTTP GET------------|----------->|---> HTTP-Server    |
   |<---Firmware-Bytes------------|            |                    |
   |                              |            |                    |
   |<---cert_to_uc15 Befehl-------|------------|<------------------|
   |---file_to_uc15()-------------|----------->|                    |
   |---AT+QFOPEN / QFWRITE--------|----------->|                    |
   |<---OK------------------------|            |                    |
   |---activate_cert()------------|----------->|                    |
   |<---OK------------------------|            |                    |
   |                              |            |                    |
   |=== Zertifikat-Upload manuell (Menue) ===========================|
   |---upload_file()              |            |                    |
   |---modem_start()--------------|----------->|                    |
   |---Init_GSM_ch(460800)--------|----------->|                    |
   |---file_to_uc15()-------------|----------->|                    |
   |---Init_BT_ch(BT_BAUD)------->|            | (MUX zurueck BT)  |
   |                              |            |                    |
   |=== Netzumschaltung =============================================|
   |---change_radionet()----------|----------->|                    |
   |---AT+QCFG="NWSCANMODE",x----|----------->|                    |
   |<---OK------------------------|            |                    |
   |---Init_BT_ch(BT_BAUD)------->|            | (MUX zurueck BT)  |
```

**Zentrales Muster:** MUX auf Zielgerät umschalten → UART1-Baudrate setzen → AT-Dialog → MUX zurück auf BT.
Bluetooth ist der Dauerpfad für die Nutzerschnittstelle; LTE wird bei Bedarf per MUX dazugeschaltet, kommuniziert und gibt die Leitung danach wieder frei.

