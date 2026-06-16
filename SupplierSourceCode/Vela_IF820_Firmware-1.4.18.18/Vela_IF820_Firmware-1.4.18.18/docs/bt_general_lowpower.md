# BLE Low-Power Strategien für IoT-Geräte

**Kontext:** Ein IoT-Gerät mit einem BLE-Chip (z.B. IF820) soll von einem User  
(Handy-App, PC mit bleak) auslesbar sein — aber man weiß nicht *wann* der User  
vorbeikommt. Ziel: minimaler Energieverbrauch bei trotzdem akzeptabler Erreichbarkeit.

---

## Übersicht

| Strategie | Ø Strom (BT-Chip) | Verbindungszeit | HW-Aufwand | SW-Aufwand |
|---|---|---|---|---|
| 1 — Always-On Advertising | 1–5 mA | sofort | keine | minimal |
| 2 — Slow Advertising (Low Duty Cycle) | 0.01–0.1 mA | 1–10 s | keine | minimal |
| 3 — Host-Controlled Sleep | < 0.01 mA | abhängig von Weckintervall | LP_MODE + BT_HOST_WAKE | mittel |

---

## Strategie 1 — Always-On Advertising

### Was passiert

Der BLE-Chip sendet kontinuierlich Advertisement-Pakete in kurzem Abstand  
(typisch 100–500 ms). Der User kann sich jederzeit verbinden.

```
Zeit: ──[ADV]──[ADV]──[ADV]──[ADV]──[ADV]──[ADV]──▶
            ↑
         alle ~100–500 ms, Chip bleibt dauerhaft aktiv
```

### Hardware

- Keine zusätzliche HW nötig
- Stromversorgung muss dauerhaft 1–5 mA liefern (Netzteil oder große Batterie)

### Software (IF820 / EZ-Serial)

```python
CMD_GAP_SET_ADV_PARAMETERS(
    type = UNDIRECTED_HIGH_DUTY_CYCLE,   # hohe Wiederholrate
    high_interval = 0x50,               # 50 * 0.625ms = 50ms
)
CMD_GAP_START_ADV()
# fertig — läuft bis Ctrl+C oder Verbindung
```

### Wann sinnvoll

- Gerät am Netzteil / USB
- Sofortige Verbindbarkeit ist Pflicht (Produktionsmessplatz, Kiosk-Terminal)
- Batteriebetrieb mit sehr großem Akku (> 10 000 mAh)

---

## Strategie 2 — Slow Advertising (Low Duty Cycle)  ← Standard für IoT

### Was passiert

Der BLE-Chip sendet ein kurzes Paket, schläft dann mehrere Sekunden, sendet wieder.  
Das Modul verwaltet diesen Sleep **intern** — der Host-MCU muss nichts tun.  
Ein Handy/PC findet das Gerät trotzdem, weil sein BLE-Scan länger als ein Intervall läuft.

```
Zeit: ──[ADV]····················[ADV]····················[ADV]──▶
        12ms senden             2s schlafen (intern)
           ↑                       ↑
        RF aktiv               Chip im internen Low-Power
```

### Hardware

- Keine zusätzliche HW nötig
- `LP_MODE` und `BT_HOST_WAKE` Jumper werden **nicht** benötigt

### Software (IF820 / EZ-Serial)

```python
CMD_GAP_SET_ADV_PARAMETERS(
    type = UNDIRECTED_LOW_DUTY_CYCLE,    # internes Duty-Cycling
    low_interval  = 0x640,              # 0x640 * 0.625ms = 1s Intervall
    low_duration  = 0,                  # 0 = unbegrenzt
    high_interval = 0x640,
    high_duration = 0,
)
CMD_GAP_START_ADV()
# fertig — Chip steuert seinen Sleep selbst
```

**Faustregel Intervall:** 1–2 s ist ein guter Kompromiss.  
Bei 1 s Intervall wartet der User im schlimmsten Fall ~1 s auf Discovery.

### Stromberechnung (Beispiel)

```
Paket senden:    12 ms  ×  10 mA  =  0.12 mAs
Schlafen:      1000 ms  ×  0.01 mA =  0.01 mAs
Gesamt pro Sekunde: ~0.13 mAs → Ø ~0.13 mA
```

→ CR2032 Knopfzelle (220 mAh) hält theoretisch **~70 Tage** (nur BT-Chip).

### Wann sinnvoll

- Standard-IoT-Szenario: Sensor, der von gelegentlichen Besuchern ausgelesen wird
- Batteriebetrieb mit mittlerer Laufzeit (Monate)
- Keine externe Trigger-Quelle vorhanden

---

## Strategie 3 — Host-Controlled Sleep (Maximum-Effizienz)

### Was passiert

Der Host-MCU (Mikrocontroller neben dem BT-Chip) steuert den BT-Chip aktiv  
über zwei GPIO-Leitungen. **Beide** schlafen zwischen den Wachfenstern.  
Der BT-Chip kann nur dann kommunizieren, wenn der MCU ihn explizit aufweckt.

```
MCU + BT-Chip schlafen tief (µA)
       │
       ├── Trigger (RTC-Timer, Sensor-IRQ, Taster, ...)
       │
       ▼
MCU wacht auf
 → LP_MODE = HIGH      (MCU → BT: "wach auf")
 → warte BT_HOST_WAKE = HIGH  (BT → MCU: "bin bereit")
 → Advertising-Fenster starten (z.B. 30 s)
       │
       ├── User verbindet sich → Daten übertragen → Verbindung trennen
       │   ODER: Timeout (kein User) → Fenster schließen
       │
       ▼
 → CMD_GAP_STOP_ADV
 → LP_MODE = LOW       (MCU → BT: "du kannst schlafen")
 → MCU schläft wieder
```

### Hardware

Zwei dedizierte GPIO-Verbindungen zwischen MCU und BT-Chip sind **Pflicht**:

| Signal | Richtung | Funktion |
|---|---|---|
| `LP_MODE` | MCU → BT-Chip | `HIGH` = bleib wach; `LOW` = darf schlafen |
| `BT_HOST_WAKE` | BT-Chip → MCU | `HIGH` = bin bereit; `LOW` = schlafe |

Beim IF820 DVK: **Jumper `LP_MODE` und `BT_HOST_WAKE` müssen bestückt sein.**

Zusätzlich für den Trigger (mindestens eine der folgenden Quellen):

| Trigger-Quelle | Typische HW | Beispiel-Usecase |
|---|---|---|
| RTC (Echtzeituhr) | im MCU integriert | alle 5 min Fenster öffnen |
| Bewegungssensor | PIR-Sensor + IRQ-Pin | Fenster öffnen wenn jemand vorbeigeht |
| Reed-Kontakt | Magnet + Schalter | Tür öffnet → Fenster öffnen |
| Taster | Button + Pull-up | User drückt Knopf → Fenster öffnen |

### Software (MCU-Seite — Pseudocode)

```c
// ── Initialisierung ────────────────────────────────────────────
gpio_output(LP_MODE);
gpio_input(BT_HOST_WAKE);
gpio_write(LP_MODE, LOW);     // BT-Chip schläft initial

// ── Haupt-Loop ────────────────────────────────────────────────
while (true) {
    sleep_deep_until_trigger();   // MCU schläft (RTC / IRQ)

    // BT-Chip aufwecken
    gpio_write(LP_MODE, HIGH);
    while (gpio_read(BT_HOST_WAKE) == LOW) {}  // warten bis bereit

    bt_start_advertising(window_seconds = 30);

    bool connected = bt_wait_for_event(EVENT_CONNECTED, timeout_ms = 30000);
    if (connected) {
        bt_wait_for_event(EVENT_DISCONNECTED, timeout_ms = 60000);
    }

    bt_stop_advertising();
    gpio_write(LP_MODE, LOW);    // BT-Chip schlafen schicken
}
```

### Software (IF820 / EZ-Serial — Python-Äquivalent)

```python
# Aufwecken
board.probe.gpio_to_output_high(board.LP_MODE)          # LP_MODE = HIGH
while not board.probe.gpio_read(board.BT_HOST_WAKE):    # warte auf bereit
    time.sleep(0.01)

# Advertising-Fenster
board.p_uart.send_and_wait(CMD_GAP_START_ADV, ...)
res = board.wait_for_ble_connection(timeout=30)         # 30s Fenster

if res:
    # Daten senden / empfangen
    board.wait_for_ble_disconnect()

board.p_uart.send_and_wait(CMD_GAP_STOP_ADV)

# Schlafen schicken
board.probe.gpio_to_output_low(board.LP_MODE)           # LP_MODE = LOW
```

### Stromberechnung (Beispiel: 5-Minuten-Intervall, 30s Fenster)

```
Wachfenster (30s):   30s  × 0.13 mA  =  3.9 mAs   (wie Strategie 2)
Deep Sleep (270s):  270s  × 0.002 mA =  0.54 mAs
Gesamt pro 5 min:   4.44 mAs  →  Ø 0.015 mA = 15 µA
```

→ CR2032 (220 mAh) hält theoretisch **~1,7 Jahre** (nur BT-Chip).

### Wann sinnvoll

- Extremes Batterie-Sparen (Coin-Cell, jahrelanger Betrieb)
- Gerät mit physischen Trigger-Quellen (Tür, Bewegung, Taster)
- Wartungs-Readout: Techniker kommt vorbei, drückt Knopf → liest aus → geht

---

## Kombinierter Ansatz (beste UX + maximale Effizienz)

Das Gerät schläft im Deep Sleep. **Zwei parallele Weckpfade:**

```
         ┌── RTC-Timer (z.B. alle 5 min) ──┐
MCU-Sleep ─┤                                 ├──▶ Advertising-Fenster (30s)
         └── Sensor-Interrupt (sofort) ────┘
```

- RTC stellt sicher, dass das Gerät auch ohne physischen Trigger periodisch  
  erreichbar ist (für automatisierte Auslesung).
- Sensor-Interrupt (Taster, PIR, Reed) sorgt für **sofortige** Reaktion  
  wenn jemand vorbeikommt.

---

## Schnell-Entscheidungsbaum

```
Gerät am Netzteil?
  └─ JA  → Strategie 1 (Always-On)
  └─ NEIN
       ↓
Gibt es einen physischen Trigger (Taster, Sensor)?
  └─ JA  → Strategie 3 (Host-Controlled Sleep) + Trigger
  └─ NEIN
       ↓
Laufzeit > 6 Monate auf Batterie nötig?
  └─ JA  → Strategie 3 (RTC-gesteuertes Fenster)
  └─ NEIN → Strategie 2 (Slow Advertising, einfachste Lösung)
```

---

## Referenz: IF820-spezifische Details

| Parameter | Wert |
|---|---|
| Deep Sleep Strom (IF820) | ~2 µA (laut Datenblatt) |
| Wachzeit bis `BT_HOST_WAKE` HIGH | ~300 ms |
| Advertising-Paket Dauer | ~12 ms pro Paket |
| Minimales Advertising-Intervall | 20 ms (BLE-Standard) |
| EZ-Serial Kommando für Sleep-Steuerung | `CMD_SET_SLEEP_PARAMS` |
| Relevante Samples | `sample_sleep.py`, `sample_custom_gatt_pc.py -l` |

---

## Entwicklungs-Learnings: IF820 + bleak (Python/PC als BLE-Central)

Diese Sektion dokumentiert Fallstricke und Best-Practices die beim Entwickeln  
von PC-Samples (bleak als BLE-Central, IF820 als Peripheral) aufgetreten sind.

---

### 1 — MAC-Adresse: EZ-Serial liefert eine Liste, bleak erwartet einen String

**Problem:**  
`CMD_GET_BT_ADDR` (und alle anderen Adressfelder wie `payload.address` nach  
`CMD_GAP_CONNECT` etc.) liefern die BT-Adresse als Python-Liste von Integers  
in LSB-First-Reihenfolge, z.B.:

```python
[0x29, 0x9F, 0x24, 0xF4, 0xBB, 0xFA]   # = FA:BB:F4:24:9F:29
```

bleak erwartet aber einen String `"FA:BB:F4:24:9F:29"`.  
Ohne Konvertierung crasht `d.address.upper() == if820_mac.upper()` mit  
`AttributeError: 'list' object has no attribute 'upper'`.

**Ausnahme:** `open_and_init_board()` → `boot_info.payload.address`  
liefert bereits einen formatierten String — **keine Konvertierung nötig**.

**Fix — Hilfsfunktion:**

```python
def _addr_to_mac_str(addr) -> str:
    """EZ-Serial Adresse (List, LSB-first) → 'AA:BB:CC:DD:EE:FF'."""
    if isinstance(addr, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(addr))
    return str(addr).upper()   # bereits ein String → nur normalisieren

# Verwendung:
bt_addr = ez_rsp[1].payload.address   # list
if820_mac = _addr_to_mac_str(bt_addr) # → "FA:BB:F4:24:9F:29"
```

---

### 2 — CYSPP-Status 0x05 gilt NUR für IF820↔IF820-Verbindungen

**Problem:**  
Die existierenden Samples (`sample_cyspp_IF820-IF820.py`) warten nach der  
BLE-Verbindung auf `EVENT_P_CYSPP_STATUS` mit Status `0x05` (= CYSPP-Datenmodus  
aktiv). Das funktioniert dort, weil **beide** Seiten explizit CYSPP konfigurieren  
(`CMD_P_CYSPP_SET_PARAMETERS(enable=2, role=...)`).

Wenn der Central **kein** CYSPP-Gerät ist (bleak, BT900, Handy-App), kommt  
Status `0x05` **niemals**. Eine Schleife die darauf wartet, blockiert ewig.

**Richtiges Event-Pattern für generische GATT-Centrals (folgt dem BT900-Sample):**

```
Peripheral (IF820):                      Central (bleak / BT900):
──────────────────────────────────────── ──────────────────────────────────────
wait_event(EVENT_GAP_CONNECTED)          BleakClient.connect()
wait_event(EVENT_GAP_CONNECTION_UPDATED) [BLE-Parameterverhandlung, optional]
                                         client.start_notify(cyspp_char)
wait_event(EVENT_GATTS_DATA_WRITTEN)  ←  bleak schreibt 0x0100 an CCC-Deskriptor
# → CYSPP-UART-Datenmodus jetzt aktiv
cyspp_ready.set()
                                         cyspp_ready.wait()  ←  wartet auf Signal
pc_connected.wait()  ←─────────────────  pc_connected.set()
p_uart.send(CYSPP_DATA)              →   notification_handler empfängt Daten
clear_rx_queue()
time.sleep(OTA_LATENCY)
rx = p_uart.read()                   ←   write_gatt_char(CYSPP_DATA)
```

Der Schlüssel: **`EVENT_GATTS_DATA_WRITTEN`** (das CCC-Schreiben von `start_notify()`)  
ist das zuverlässige Signal dafür, dass der CYSPP-Datenmodus aktiv ist.

---

### 3 — `CMD_P_CYSPP_START` ist destruktiv bei bestehender BLE-Verbindung

**Problem:**  
`CMD_P_CYSPP_START` aufgerufen **nach** einer BLE-Verbindung trennt diese  
sofort und startet CYSPP-Advertising neu (Status `0x01 → 0x00`).  
Es ist kein Befehl um "CYSPP-Datenmodus starten", sondern "CYSPP-Advertising  
von vorne beginnen".

```
# FALSCH — nach BLE connect aufgerufen:
board.p_uart.wait_event(EVENT_GAP_CONNECTED)
board.p_uart.send_and_wait(CMD_P_CYSPP_START)  # ← trennt BLE-Verbindung!
# → CYSPP status 0x01 (advertising), dann 0x00 (disconnected)
```

**Richtig:** Nur aufrufen, wenn noch keine BLE-Verbindung besteht  
(z.B. um CYSPP-Advertising explizit zu starten, falls Autostart deaktiviert).

---

### 4 — `clear_rx_queue()` vor dem Lesen von CYSPP-Daten ist Pflicht

**Problem:**  
Während der BLE-Verbindungsaufbauphase akkumuliert der IF820-UART-Empfangspuffer  
EZ-Serial-Event-Strings. Nach `EVENT_GATTS_DATA_WRITTEN` liegen typischerweise  
im Puffer:

```
@E,001D,CU,C=01,I=000C,L=0000,O=03C0\r\n   ← EVENT_GAP_CONNECTION_UPDATED
@E,001A,W,C=01,H=0012,T=00,D=0100\r\n       ← EVENT_GATTS_DATA_WRITTEN (CCC)
@E,000C,.CYSPP,S=01\r\n                     ← EVENT_P_CYSPP_STATUS = 0x01
```

Ein `p_uart.read()` danach liefert diese Event-Strings, nicht die echten  
CYSPP-Nutzdaten vom Central!

**Fix — BT900-Sample-Pattern:**

```python
# IF820 sendet zuerst seine Daten via CYSPP-Notify:
board.p_uart.send(CYSPP_DATA)
time.sleep(OTA_LATENCY)

# Gepufferte Events wegwerfen, DANN auf eingehende CYSPP-Daten warten:
board.p_uart.clear_rx_queue()          # ← PFLICHT
time.sleep(OTA_LATENCY)
rx = board.p_uart.read()              # ← jetzt echte PC-Daten
```

---

### 5 — `wait_event()` Timeout-Handling

`wait_event(event, rxtimeout=1)` hat einen Standard-Timeout von 1 Sekunde.  
Bei Timeout gibt die Funktion `(-1, None)` zurück.  
Ein direkter Zugriff `ez_rsp[1].payload.status` crasht dann mit  
`AttributeError: 'NoneType' object has no attribute 'payload'`.

**Immer absichern:**

```python
ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_P_CYSPP_STATUS, rxtimeout=3)
if ez_rsp[0] != 0 or ez_rsp[1] is None:
    # Timeout oder Fehler — kein Event erhalten
    break
status = ez_rsp[1].payload.status
```

Für kritische Events (z.B. `EVENT_GAP_CONNECTED`) einen großen Timeout wählen:

```python
board.p_uart.wait_event(board.p_uart.EVENT_GAP_CONNECTED, rxtimeout=60)
```

---

### 6 — Synchronisation zwischen asyncio (bleak) und IF820-Thread

bleak läuft in einem `asyncio`-Event-Loop auf dem Main-Thread (Windows-Pflicht).  
Der IF820-EZ-Serial-Code läuft in einem Daemon-Thread.  
Synchronisation via `threading.Event`:

```python
cyspp_ready = threading.Event()   # IF820-Thread → PC: "CYSPP aktiv"
pc_connected = threading.Event()  # PC-Thread → IF820: "PC bereit"

# Im IF820-Thread (nach EVENT_GATTS_DATA_WRITTEN):
cyspp_ready.set()
pc_connected.wait(timeout=15)
board.p_uart.send(CYSPP_DATA)

# Im PC-Thread (asyncio, nach start_notify):
await asyncio.to_thread(cyspp_ready.wait, 15)  # wartet ohne den Event-Loop zu blockieren
pc_connected.set()
await asyncio.sleep(OTA_LATENCY + 0.5)
await client.write_gatt_char(cyspp_char, CYSPP_DATA, response=False)
```

**Wichtig:** Niemals `threading.Event.wait()` direkt in einer `async`-Funktion  
aufrufen — das blockiert den gesamten asyncio-Event-Loop.  
Stattdessen `await asyncio.to_thread(event.wait, timeout)` verwenden (Python ≥ 3.9).

---

### 7 — bytearray-Werte aus EZ-Serial sind vorzeichenlos (uint8)

**Problem:**  
EZ-Serial gibt Felder wie TX-Power-Tabellen als Python-`bytearray` zurück.  
Negative dBm-Werte werden als 2er-Komplement gespeichert:  
`0xFE` = 254 (unsigned) = **−2 dBm** (signed int8).  
Ohne Konvertierung erscheint Level 1 als „254 dBm" und der Path-Loss-Wert  
wird völlig falsch berechnet.

**Fix — beim Lesen in signed int8 konvertieren:**

```python
# bytearray → signed int8 Liste
power_array = res[1].payload.power_array
ble_tx_power_table = [b if b < 128 else b - 256 for b in power_array[-8:]]
# → [-2, 0, 2, 4, 6, 8, 10, 10]   statt  [254, 0, 2, 4, 6, 8, 10, 10]
```

**Fix — beim Schreiben zurück in unsigned umwandeln:**  
EZ-Serial und Python-Listen erwarten beim Senden unsigned Bytes (0–255).  
Ein negativer dBm-Wert muss mit `& 0xFF` re-kodiert werden:

```python
ADV_DATA[-1] = claimed_dbm & 0xFF   # -2 & 0xFF → 0xFE = 254 (korrekt für Byteliste)
```

---

### 8 — IF820 TX-Power: 3 reale Leistungsgruppen statt 8 gleichmäßiger Stufen

**Beobachtung (gemessen mit `sample_advertiser_tx_power_pc.py -s 10`, ~60 Samples/Stufe):**

```
Firmware-Tabelle: [-2, 0, 2, 4, 6, 8, 10, 10] dBm

Level 1–3  (-2, 0, +2 dBm)  → RSSI ≈ -61 dBm  (Gruppen-Ø)
Level 4–5  (+4, +6 dBm)     → RSSI ≈ -54 dBm  → +7 dBm realer Sprung
Level 6–8  (+8, +10, +10 dBm) → RSSI ≈ -49 dBm → +5 dBm realer Sprung
```

**Path Loss:** 56–63 dB (±3 dB Messstreuung durch Multipath/Rauschen) — normal für  
statische Indoor-BLE-Messungen.

**Erklärung:**  
Die PA (Power Amplifier) des IF820 ist im unteren Leistungsbereich nicht-linear.  
Die firmware-seitig definierten Stufen 1–3 liegen auf DAC-Einstellungen, die  
messtechnisch nicht trennbar sind. Der **Gesamtbereich** von ~12 dBm  
(−2 bis +10 dBm) ist aber korrekt.

**Praktische Empfehlung:**

| Ziel | Empfohlene Stufe |
|---|---|
| Maximale Reichweite | Level 6–8 (+8/+10 dBm) |
| Mittlere Reichweite / Energiesparen | Level 4–5 (+4/+6 dBm) |
| Minimale Sendeleistung / Proximity | Level 1–3 (−2 bis +2 dBm) |

---

## Entwicklungs-Learnings: IF820 SPP + Python RFCOMM-Socket (PC als BT-Classic-Central)

Diese Sektion dokumentiert Fallstricke und Best-Practices beim Entwickeln von
PC-Samples mit Bluetooth Classic SPP (socket.BTPROTO_RFCOMM, Python stdlib).
Grundlage: `sample_spp_pc.py`.

---

### 9 — `CMD_FACTORY_RESET scope=ram` randomisiert die BT-Adresse nur im RAM — NVM bleibt stabil

**Kernproblem (ursprünglich):**  
`CMD_FACTORY_RESET` (ohne expliziten Scope, entspricht `scope=ram`) generiert bei
jedem Aufruf eine neue zufällige BT-Classic-Adresse im RAM. Windows musste daher
nach jedem Script-Start neu gekoppelt werden (alte Kopplung entfernen → neu koppeln).

**Ursache:**  
`scope=ram` schreibt Factory-Defaults nur in den RAM — der NVM (Flash) wird **nicht
verändert**. Nach einem Hardware-Reset (`open_and_init_board()`) lädt der Chip die
Adresse wieder aus dem NVM, d. h. die Adresse vor dem Factory-Reset ist die
**NVM-stabile** Adresse und bleibt zwischen Script-Läufen identisch.

**Lösung:**
```python
# Vor factory_reset: stabile NVM-Adresse sichern
ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_GET_BT_ADDR)
stable_addr = ez_rsp[1].payload.address

# Factory reset + Reboot abwarten ...

# Danach: original Adresse im RAM wiederherstellen
ez_rsp = board.p_uart.send_and_wait(
    board.p_uart.CMD_SET_BT_ADDR, address=stable_addr)
```

**Ergebnis:**  
Windows-Kopplung ist nur **einmalig** (Erstinbetriebnahme) nötig.
Jeder weitere Script-Lauf findet dieselbe MAC-Adresse vor.

---

### 10 — Windows RFCOMM-Socket erfordert vorherige Kopplung — kein COM-Port

**Problem:**  
`socket.connect((mac, scn))` mit `AF_BLUETOOTH / BTPROTO_RFCOMM` schlägt auf Windows
mit `OSError` fehl, wenn das Gerät nicht vorab in Windows gekoppelt ist.
Symptom auf dem IF820: `@E,000E,ASC,S=03,R=03` (Security Challenge abgelehnt),
gefolgt von sofortigem Disconnect `@E,0012,BTDIS`.

**Zusätzliche Falle — COM-Port:**  
Wird bei der Windows-Kopplung ein „ausgehender COM-Port" konfiguriert, verbindet
Windows den RFCOMM-Kanal automatisch sobald das Gerät in Reichweite kommt — bevor
das Python-Script `socket.connect()` aufruft. Der Socket schlägt dann fehl.

**Lösung:**  
Gerät einmalig in Windows koppeln (Just Works, kein PIN), **keinen COM-Port anlegen**.

---

### 11 — `socket.recv()` auf RFCOMM ist ein Stream — immer akkumulieren

**Problem:**  
RFCOMM ist ein Stream-Protokoll. `sock.recv(256)` gibt zurück, was gerade im
Empfangspuffer liegt — auch nur 1 Byte.

**Lösung:**
```python
data = b''
sock.settimeout(2.0)
while len(data) < len(SPP_DATA):
    chunk = sock.recv(256)
    if not chunk:
        break
    data += chunk
```

---

### 12 — Probe-Socket triggert `EVENT_BT_CONNECTED` vorzeitig

**Problem:**  
Ein Ansatz zur SCN-Erkennung öffnete und schloss Sockets auf SCN 1–8 nacheinander
(Probe). Das erste `socket.connect()` — auch wenn es gleich wieder geschlossen wird —
triggert auf dem IF820 `EVENT_BT_CONNECTED`. Der IF820-Thread konsumiert das Event
für die Probe-Verbindung, nicht für die eigentliche Datenverbindung.

**Lösung:**  
Probe-Scan entfernt. `socket.connect()` direkt als echte Datenverbindung verwenden.
Der EZ-Serial SPP-Kanal nach Factory-Reset ist immer SCN 2.
