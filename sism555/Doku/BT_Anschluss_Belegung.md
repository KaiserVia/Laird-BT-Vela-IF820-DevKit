# Bluetooth IF820 – Anschlussbelegung (aktuell genutzte Leitungen)

Wofür die aktuell genutzten Signale dienen (fünf auf jedem Board, CONNECTION nur auf umgebauten Boards) und womit sie am Controller (LPC1766)
verbunden sind. Grundlage: Firmware `btio.c` / `hard.h` / `hard.c`.

## Übersicht

| IF820-Pin | Signal | Richtung | Zweck | Controller-Anschluss (LPC1766) |
|-----------|--------|----------|-------|-------------------------------|
| **P_UART_TXD** (P32) | TxD | Modul → Host | Datenausgang des Moduls (Antworten, Menü über SPP) | über MUX → **RXD1 = P2.1** |
| **P_UART_RXD** (P37) | RxD | Host → Modul | Dateneingang des Moduls (Kommandos, Menü-Rückkanal) | über MUX ← **TXD1 = P2.0** |
| **P_UART_RTS** (P11) | RTS (Modul) | Modul → Host | Flusskontrolle: „Modul empfangsbereit" (LOW = darf senden) | → **CTS1 = P2.2** (auch als GPIO `FIO2PIN & (1<<2)` gelesen) |
| **P_UART_CTS** (P10) | CTS (Modul) | Host → Modul | Flusskontrolle: „Host empfangsbereit" (LOW = Modul darf senden) | ← **RTS1 = P2.7** |
| **CYSPP** (P13) | Modus/Status | bidirektional | Daten- ↔ Command-Modus umschalten + Verbindungsstatus (BT_LINK) | **nicht am LPC** – I²C-Expander **IC54 (PCA9554 @0x46), Bit 7** |
| **CONNECTION** (P27, Pin 33) | Status | Modul → Host | „Verbindung steht" (LOW aktiv); **nur auf umgebauten Boards** verdrahtet | **P0.19** (GPIO-Eingang `CONN_PIN`, nur lesen; seit 5.137 Auslöser `BT_CONN_TRIGGER`, sonst Diagnose) |

## Wichtig: Kreuzung (Nullmodem-Prinzip)

Die vier UART-Leitungen werden **gekreuzt** verdrahtet (Ausgang der einen Seite → Eingang der anderen):

- Host **TXD1 (P2.0)** → Modul **RXD** (P37)
- Host **RXD1 (P2.1)** ← Modul **TXD** (P32)
- Host **RTS1 (P2.7)** → Modul **CTS** (P10)
- Host **CTS1 (P2.2)** ← Modul **RTS** (P11)

Häufigster Verdrahtungsfehler: RTS/CTS „straight" statt gekreuzt → host-CTS wird nicht getrieben →
bleibt HIGH → Host sendet nie (IF820: W11015 CTS-Timeout, bei der Erstkonfiguration F11018; F11009 nur im Laird/RN4678-Zweig). Prüfen: host-CTS (P2.2) muss denselben Pegel
zeigen wie Modul-RTS (P11).

## Details je Signal

**TxD / RxD (Daten):** Die eigentliche Nutzdatenverbindung. Laufen am LPC über **UART1**
(P2.0/P2.1), das per **MUX** zwischen Bluetooth und GSM/GPS umgeschaltet wird. MUX-Auswahl über
**CSAF = P0.1** und **CSBF = P0.0**; **BT-Kanal = beide LOW** (`MUX_BT_Select`). Ohne korrektes
TxD/RxD kommt gar keine Antwort (auch kein `/PING`).

**RTS / CTS (Flusskontrolle, active-LOW):** Verhindern Byte-Verlust unter Last.
- **CTS1 (P2.2)** ist der Host-Eingang; die Firmware wartet in `putb()` auf **CTS = LOW**, bevor
  sie ein Byte sendet (begrenzt durch `CTS_WAIT_MS` = 50 ms, bei aktiver BT-Datenverbindung `CTS_WAIT_LINK_MS` = 1000 ms, `sio.c`).
- **RTS1 (P2.7)** ist der Host-Ausgang → sagt dem Modul, ob der Host aufnahmebereit ist.
- Hardware-Flow-Control wird in `Init_UART1()` per `U1->MCR |= (1<<7)|(1<<6)` aktiviert.
- Hinweis: EZ-Serial hat Flow-Control **ab Werk aus** (`F=00`) → das Modul treibt RTS dann nicht →
  host-CTS bleibt HIGH. Einmalig per `STU,...,F=01` + `/SCFG` einschalten und sichern.

**CYSPP (Modus/Status):** Kein direkter LPC-Pin, sondern über den I²C-Expander **IC54
(PCA9554, Adresse 0x46), Bit 7**.
- Als **Eingang** gelesen = Verbindungsstatus **BT_LINK** (`(1<<7)`): Modul zieht LOW, wenn eine
  SPP-Verbindung aktiv ist.
- Als **Ausgang** getrieben (`bt_cmdmode()`, Bit 7 = HIGH) = laufende Verbindung gezielt trennen. Das
  geschieht nur in `bt_disconnect()` (Inaktivitätstimeout) und beim Deinstallieren des Moduls, nie im
  Normalbetrieb (sonst kommt kein BLE-Datenkanal zustande). `bt_release()` setzt Bit 7 wieder auf Eingang.
- Zum Auslesen/Steuern läuft das über den I²C-Bus (kein dedizierter UART-Pin).

## Kurzfassung (Steckerseite → LPC)

```
IF820 TXD  (P32) ──MUX──►  P2.1  (RXD1)
IF820 RXD  (P37) ◄─MUX──   P2.0  (TXD1)
IF820 RTS  (P11) ───────►  P2.2  (CTS1)   (Host-CTS-Eingang, auch GPIO FIO2.2)
IF820 CTS  (P10) ◄───────  P2.7  (RTS1)   (Host-RTS-Ausgang)
IF820 CYSPP(P13) ───────   IC54 (PCA9554 @0x46) Bit7  (über I²C, nicht am LPC)
MUX-Auswahl:               CSAF=P0.1, CSBF=P0.0  → BT = beide LOW
```
