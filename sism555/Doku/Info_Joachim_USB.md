# Info für Joachim: USB-Kabelanschluss (Branch `usb-verbindung`, 5.u1–5.u23)

Stand 26.11.2026. Ausführlich: `Doku/USB_Nachttest_2026-11-25.md` (Befunde, Tests), `Doku/USB_Analyse_2026-11.md`
(Analyse mit Schaltplan), `project-status.md` (Kapitel 5.u1 bis 5.u23).

## Worum es geht

USB am PC war bei Kunden unzuverlässig. Wir haben diese Fehler gefunden und behoben:

| Nr. | Fehler | Folge beim Kunden | behoben in |
|---|---|---|---|
| F11 | Keil-USB-Treiber schreibt beim Schließen des COM-Ports während einer Ausgabe ein EP0-Paket nach Adresse 0 | HardFault, 32 s Stillstand, Watchdog-Reset | Treiber |
| F12 | Keil-USB-Treiber gibt in `USBD_ReadSetupPacket()` seine Sperre nicht frei | USB tot bis zum Neustart, Gerät läuft sonst weiter | Treiber |
| F8 | `USB_LINK` in `connect` (`__noinit`) überlebt jeden Reset | nach Update über USB oder Watchdog-Reset hängt das Gerät bis zum Stromzyklus | main.c |
| F9 | USB-Empfang: Index vor dem Byte sichtbar | NAKs/Fehler bei XMODEM-Update über USB | USB_tools.c |
| F1 | Tiefschlaf bei gestecktem Kabel stoppt den USB-Takt | An-/Abmeldeschleife am PC | measure.c, hard.c |

Ergebnis mit 5.u21 an 26SC9966: 600-mal Schließen während der Ausgabe ohne Absturz, Dauerlauf 129/129 Runden mit
Stromzyklen, 43 Downloads und 25 Updates über USB ohne Fehler.

## Am einfachsten: den Stand übernehmen statt nachbauen

Wenn du im gemeinsamen Repo arbeitest: nach dem Merge von `usb-verbindung` nach `main` einfach `main` holen
(bzw. `git merge main` in deinen Branch). Dann ist alles drin, auch die Projektdatei. **Auf deinem PC muss dafür
kein Keil-Pack installiert oder aktualisiert werden** – die neuen USB-Treiber liegen als Quelltext im Projekt.

Nur wenn du in einer eigenen Kopie weiterarbeitest, die Änderungen unten von Hand übertragen.

## Änderungen im Einzelnen

### 1. USB-Treiber (wichtigster Teil)

Neuer Ordner `sism555/USB_Driver_Patch/` mit vier Dateien – Kopien aus **Keil LPC1700_DFP 2.7.2** (letzte Version,
Keil pflegt das Pack nicht mehr) statt der bisherigen 2.0.0 aus dem Pack:

| Datei | Version | unsere Änderungen (im Code mit `viasis 5.u23` markiert) |
|---|---|---|
| `USBD_LPC17xx.c` | V2.14 (vorher V2.00) | Ersatz-Makros `__UNALIGNED_UINT32_READ/WRITE` (fehlen in CMSIS 4.2.0); Lesen nur so weit, wie im Zielpuffer Platz ist; Schalter `RTE_Drivers_USBD0/USBH0` |
| `USBD_LPC17xx.h` | V2.14 | keine |
| `USBH_LPC17xx.c` | V2.6 (vorher V2.00) | Capabilities ohne zweites Feld (CMSIS 4.2.0); Schalter |
| `OTG_LPC17xx.c` | V1.3 (vorher V1.00) | **Fehler im Keil-Pack behoben:** `USBH0_IRQ`/`USBD0_IRQ` → `USBH_IRQ`/`USBD_IRQ` (Host+Gerät auf LPC176x hat Keil nie gebaut); Schalter |

F11 und F12 hat Keil in V2.12/V2.14 selbst behoben; Gerät, Host und OTG gehören zusammen (gemeinsame Variablen
`usb_role`/`usb_state`), deshalb alle drei.

In µVision (`sism.uvprojx`):

1. *Manage Run-Time Environment* → *CMSIS Driver*: **USB Device** und **USB Host** abwählen
   (sonst doppelte Symbole). µVision schreibt `RTE/_sism/RTE_Components.h` dabei selbst neu.
2. Neue Gruppe `USB_Driver_Patch` mit `USBD_LPC17xx.c`, `OTG_LPC17xx.c`, `USBH_LPC17xx.c`.
3. Die USB-Middleware (MDK-Middleware 6.2.0: USB Core/Device/CDC/Host/MSC, File System) bleibt unverändert.
4. `RTE/Device/LPC1766/RTE_Device.h` bleibt, wie er ist.

### 2. `RTE/USB/USBD_Config_0.c`

- `USBD0_MAX_PACKET0` 16 → 64
- Hersteller `L"via traffic"`, Produkt `L"viasis"` (vorher NXP / LPC USB VCom Port)
- `USBD0_STR_DESC_SER_EN` 1, Platzhalter `L"0000000000000000"` (16 Zeichen) – wird zur Laufzeit durch die
  Geräte-Seriennummer ersetzt (siehe 3.). Folge: jedes Gerät behält am PC seine eigene, feste COM-Nummer.

### 3. `USB_tools.c`

- `usb_serial_patch()`: legt die String-Deskriptoren als RAM-Kopie an, trägt `fp.serno` ein, Aufruf vor `USBD_Initialize()`.
- `usb_vbus_on()`: VBUS entprellt (300 ms).
- Warteschleife auf `USBD_Configured()`: konfiguriert der PC nach 20 s nicht (`USB_ENUM_RETRY_MS`), ab- und wieder anmelden.
- Empfangsschleife: erst `rbuf[...]=ch`, dann `__DMB()`, dann `rxi++`; voller Ringpuffer wartet bis 100 ms
  (`USB_RX_WAIT_MS`), danach wird verworfen (Zähler `usb_rx_ovf`). **Keine** UART0-Interrupt-Sperre je Byte (machte es schlechter).

### 4. `main.c`

Ganz am Anfang von `main()` (vor der fp-Prüfung): `connect&=~USB_LINK; concpy&=~USB_LINK;`

### 5. `measure.c` (`runcycle()`)

Schlafbedingung um `||((interfaces&USB_LINK)&&(FIO1PIN&VBUS))` erweitert: kein Tiefschlaf, solange das USB-Kabel steckt.

### 6. `hard.c` (`DeepSleep()`)

`PCONP` vor dem Schlaf merken und nach `SystemInit()` zurückschreiben (SystemInit schaltet u. a. PCUSB ab).
**Keine** neuen `__noinit`-Variablen – das Speicherlayout (`non_init`, `fp`) ist unverändert, geprüft gegen 5.156.

## Prüfen nach dem Übernehmen

1. Build ohne Fehler/Warnungen; in der Map `USB_IRQHandler` aus `otg_lpc17xx.o`, `USBD_IRQ` und `USBH_IRQ` vorhanden.
2. USB-Kabel am PC: Port erscheint mit Seriennummer des Geräts (Geräte-Manager → Details → Geräteinstanzpfad endet auf die Seriennummer).
3. Menü über USB, Ymodem-Download über USB, Firmware-Update über USB (Gerät muss danach von selbst mit USB zurückkommen).
4. Terminal mehrfach schließen, während das Gerät ausgibt (z. B. Infoseite) – Gerät darf nicht neu starten.
5. **USB-Stick** (Host): Update vom Stick und Daten auf den Stick – der Host-Treiber ist ebenfalls neu.

Prüfskripte dazu liegen in `Doku/tools/` (`usb_close_provoke.py`, `usb_open_stress.py`, `usb_download.py`, `usb_test.py`).

## Hinweise

- Treiber-Lizenz: Apache 2.0 (Keil). Geänderte Dateien sind im Kopf als geändert gekennzeichnet – bitte so lassen.
- Die Pack-Komponenten USB Device/Host nicht wieder anwählen, sonst laufen wieder die alten Treiber.
- Offen und nicht Teil dieser Änderung: Hardware-Punkte H1–H6 (Analyse Abschnitt 4), späterer Umstieg auf aktuelle
  Werkzeuge (`Doku/Werkzeug_Umstieg_Aufwand.md`).
