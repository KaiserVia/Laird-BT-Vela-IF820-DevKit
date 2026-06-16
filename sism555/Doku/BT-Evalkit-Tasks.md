# BT Eval-Kit Aufgaben und Tests

Ziel: Neues BT-Modul evaluieren und pruefen ob es als Ersatz fuer RN4678 / Laird 730-SA
in der viasis3004 Firmware (sism555) funktionieren kann.

---

## Anforderungen aus dem Projekt

Das BT-Modul muss folgendes koennen:

| Anforderung | Detail |
|-------------|--------|
| Profil | SPP (Serial Port Profile), Classic Bluetooth |
| Baudrate UART | 307200 bps (aktuell RN4678), idealerweise bis 460800 |
| Flow Control | RTS/CTS Hardware-Handshake (zwingend!) |
| Betriebsart | Transparent serial (kein Protokoll-Overhead) |
| Command Mode | AT-Kommandos oder aehnlich fuer Konfiguration |
| Status-Pin | GPIO das bei SPP-Verbindung auf LOW geht |
| Power Control | Ein/Aus ueber externen GPIO (Enable/PULLUP) |
| Pairing | PIN-basiert oder SSP Just Works |
| NVM | Name, PIN, Baudrate muessen im Modul gespeichert bleiben |
| Dual Mode | BLE + Classic waere Vorteil (Smartphone-Erkennung) |

---

## Use Cases im Geraet

### UC1: Terminal-Verbindung (Hauptanwendung)

Benutzer verbindet sich per BT-Terminal-App, sieht ein Textmenue,
gibt Ziffern/Buchstaben ein, bekommt Antworten.

- Datenrate: gering (< 1 KB/s)
- Richtung: bidirektional
- Dauer: Minuten bis Stunden (6 Min Timeout bei Inaktivitaet)

### UC2: Datenausgabe (Bulk Download)

Messdaten werden vom Geraet gesendet. Kann mehrere Minuten dauern.

- Datenrate: maximal (bei 307200: ~29 KB/s)
- Richtung: primaer Geraet -> PC/App
- Flow Control: kritisch! Ohne CTS gehen Daten verloren
- Timeout: Empfaenger muss ggf. Keep-Alive senden (NUL-Byte)

### UC3: Firmware-Update ueber BT (Xmodem-1K)

App sendet neue Firmware per Xmodem-1K Protokoll.

- Datenrate: maximal (sustained)
- Richtung: primaer App -> Geraet, mit ACK/NAK Handshake
- Dateigroesse: ~150 KB (ca. 5s bei 307200)
- Zuverlaessigkeit: 0 Fehler erlaubt (CRC-Pruefung pro Block)

### UC4: Zertifikats-Upload

Datei wird per Xmodem ueber BT empfangen, dann intern ans GSM-Modem
weitergeleitet. Waehrenddessen ist BT-Terminal stumm (UART1 gehoert GSM).

- Ablauf: BT empfangen -> BT pausiert -> GSM sendet -> BT wiederhergestellt
- Kritisch: Nach GSM-Transfer muss BT sofort wieder auf korrekter Baudrate laufen

### UC5: Netzumschaltung waehrend BT-Verbindung

User loest ueber BT-Terminal eine GSM-Netzumschaltung aus (AT+QCFG).
UART1 wechselt kurz zu GSM und kommt zurueck.

- Wie UC4, aber ohne Dateitransfer
- BT-Modul muss CTS-Pause tolerieren (einige Sekunden kein TX)

---

## Eval-Kit Tests

### Test 1: Grundkommunikation

**Ziel:** Transparente serielle Verbindung pruefen.

1. Eval-Kit mit PC verbinden (USB-UART Adapter, 307200 8N1, RTS/CTS)
2. Vom PC aus Text senden, pruefen ob es am UART-Ausgang ankommt
3. Vom UART-Eingang Text senden, pruefen ob es am PC ankommt
4. Bidirektional gleichzeitig testen

**Erfolgskriterium:** Keine verlorenen Bytes bei 307200 bps ueber 60 Sekunden Dauertest.

---

### Test 2: Hardware Flow Control (CTS/RTS)

**Ziel:** Pruefen ob Flow Control korrekt funktioniert.

1. CTS-Leitung am Eval-Kit auf HIGH ziehen (Modul soll stoppen)
2. Vom PC Daten senden -> Modul darf NICHTS an UART-TX ausgeben
3. CTS auf LOW -> gepufferte Daten muessen komplett ankommen
4. UART-RX Puffer voll simulieren (RTS HIGH setzen) -> PC darf nichts mehr senden

**Erfolgskriterium:** Kein Datenverlust bei CTS-Toggle unter Last.

---

### Test 3: Baudratenwechsel

**Ziel:** Modul auf Zielbaudrate konfigurieren und verifizieren.

1. Verbindung bei Default-Baudrate (115200) aufbauen
2. Baudrate per AT-Kommando auf 307200 setzen (bzw. 460800)
3. UART-Adapter umstellen, Kommunikation pruefen
4. Modul stromlos machen und wieder einschalten -> Baudrate muss gespeichert sein

**Erfolgskriterium:** Nach Power-Cycle weiterhin auf konfigurierter Baudrate erreichbar.

---

### Test 4: Command Mode Ein/Austritt

**Ziel:** Pruefen ob der Wechsel zwischen Daten- und Kommandomodus zuverlaessig ist.

Beim RN4678:
1. `$$$` senden (ohne CR!) -> `CMD>` muss kommen
2. `---\r` senden -> `END` muss kommen, Modul in Datenmodus
3. Waehrend aktiver SPP-Verbindung: `$$$` darf NICHT versehentlich getriggert werden
   (bei normalen Daten die "$$$" enthalten)

**Erfolgskriterium:** Zuverlaessiger Wechsel, kein versehentlicher Trigger.

---

### Test 5: Factory Reset + Rekonfiguration

**Ziel:** Kompletten Init-Zyklus wie in der Firmware durchspielen.

Sequenz (RN4678):
```
$$$         -> CMD>
SF,1        -> AOK       (Factory Reset)
R,1         -> Reboot    (Modul startet neu bei 115200)
--- 2s warten ---
$$$         -> CMD>
SN,VIASIS_TEST  -> AOK
SA,2        -> AOK       (Auth: Just Works)
SS,SPP      -> AOK
SY,4        -> AOK       (Max Power)
SP,123456   -> AOK       (PIN)
SG,0        -> AOK       (Dual Mode)
SU,10       -> AOK       (Baudrate 307200)
SO,,        -> AOK       (Status-Meldungen aus)
R,1         -> Reboot
--- 2s warten, UART auf 307200 umstellen ---
$$$         -> CMD>
---         -> END
```

**Erfolgskriterium:** Gesamte Sequenz in < 20 Sekunden, kein Fehler.

---

### Test 6: Verbindungserkennung (Status-GPIO)

**Ziel:** Pruefen ob ein GPIO-Pin den SPP-Verbindungsstatus anzeigt.

1. Status-Pin messen (Multimeter/Logic Analyzer)
2. Keine Verbindung: Pin muss HIGH sein
3. SPP-Verbindung aufbauen: Pin muss auf LOW gehen
4. Verbindung trennen: Pin muss zurueck auf HIGH

**Erfolgskriterium:** Zuverlaessige Flanke bei Connect/Disconnect, < 100 ms Reaktionszeit.

---

### Test 7: Xmodem-1K Dateitransfer

**Ziel:** Sustained Throughput und Zuverlaessigkeit pruefen.

1. SPP-Verbindung aufbauen (307200 bps)
2. Datei (~150 KB) per Xmodem-1K vom PC zum Eval-Kit senden
3. Am UART-Ausgang mitlesen und mit Original vergleichen
4. Geschwindigkeit messen

**Erfolgskriterium:** 0 Fehler, >= 25 KB/s Durchsatz, kein Timeout.

---

### Test 8: Power Cycling (Modulabschaltung)

**Ziel:** Simuliert den 6-Minuten-Timeout Ablauf im Geraet.

1. Modul einschalten (Enable-Pin / VCC)
2. Warten bis discoverable (~2s)
3. SPP-Verbindung aufbauen, Daten senden
4. Modul abschalten (Enable-Pin LOW)
5. 1 Sekunde warten
6. Modul wieder einschalten
7. Erneut verbinden -> muss funktionieren

**Erfolgskriterium:** Nach Power-Cycle ist Modul innerhalb 3s wieder verbindbar.
Konfiguration (Name, PIN, Baudrate) bleibt erhalten.

---

### Test 9: CTS-Pause (UART1-Sharing Simulation)

**Ziel:** Simuliert den Moment wenn UART1 kurz ans GSM-Modem geht.

1. SPP-Verbindung aufbauen, laufend Daten senden (PC -> Modul -> UART)
2. CTS am UART auf HIGH ziehen (= Pause, Modul darf nicht senden)
3. 5 Sekunden warten
4. CTS wieder LOW
5. Restliche Daten muessen korrekt ankommen

**Erfolgskriterium:** Modul puffert Daten waehrend CTS HIGH, kein Verlust.

---

### Test 10: BLE Discovery + Classic SPP

**Ziel:** Dual-Mode pruefen (Smartphone findet Geraet per BLE, Daten laufen ueber SPP).

1. Modul im Dual-Mode konfigurieren (`SG,0`)
2. Mit Smartphone BLE-Scan: Geraet muss sichtbar sein
3. Gleichzeitig mit PC Classic SPP verbinden: muss funktionieren
4. Daten ueber SPP senden waehrend BLE-Advertising aktiv

**Erfolgskriterium:** BLE und Classic gleichzeitig moeglich.

---

### Test 11: Reichweite / Sendeleistung

**Ziel:** Praktische Reichweite im Anwendungsfall pruefen.

1. Modul auf maximale Sendeleistung konfigurieren
2. SPP-Verbindung aufbauen
3. Abstand schrittweise erhoehen (5m, 10m, 15m, 20m)
4. Bei jedem Schritt Datentransfer pruefen (kein Paketverlust)

**Erfolgskriterium:** Stabile Verbindung bei >= 10m (typischer Einsatz: Techniker steht neben Geraet).

---

### Test 12: Timing nach Reboot

**Ziel:** Wie schnell ist das Modul nach einem Reboot wieder ansprechbar?

1. `R,1` senden (Reboot-Kommando)
2. Zeit messen bis erstes Zeichen empfangen wird (`%REBOOT` oder aehnlich)
3. Zeit messen bis `$$$` -> `CMD>` wieder funktioniert

**Erfolgskriterium:** < 2 Sekunden bis Command Mode bereit.
(Firmware wartet aktuell 2s mit Dot-Feedback, muss also reichen.)

---

## Hardware-Schnittstelle im Geraet

```
LPC1766 (MCU)          MUX (IC55/IC56)         BT-Modul
-----------            -----------              ----------
P2.0 TXD1  ------>    MUX Input    ------>     RXD
P2.1 RXD1  <------    MUX Output   <------     TXD
P2.2 CTS   <------    (direkt)     <------     CTS (Modul -> MCU)
P1.15 RTS  ------>    (direkt)     ------>     RTS (MCU -> Modul)

MUX-Steuerung:
P0.0 CSBF  ------>    Select B
P0.1 CSAF  ------>    Select A
                       CSAF=0, CSBF=0 = BT
                       CSAF=1, CSBF=0 = GSM
                       CSAF=0, CSBF=1 = GPS

Verbindungsstatus:
BT Status-GPIO ------> IC54 (I2C Expander) Bit 7 -----> MCU liest per I2C

Power:
PINMODE3 PULLDOWN -----> BT-Modul VCC Enable
```

---

## Zusammenfassung: Minimale Anforderungen ans neue Modul

1. Bluetooth Classic SPP (zwingend)
2. UART 307200+ bps mit RTS/CTS
3. AT/Command-Mode fuer Konfiguration
4. Status-GPIO fuer Verbindungserkennung
5. NVM fuer persistente Konfiguration
6. Bootzeit < 3 Sekunden
7. Abschaltbar per GPIO (oder VCC-Steuerung)
8. Interner Puffer fuer CTS-Pausen (mind. 256 Byte)
