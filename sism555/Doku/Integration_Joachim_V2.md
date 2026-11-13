# Integration Joachim-Stand (sism560_V2, 5.60) in 5.137

Stand 2026-11-12, Branch `Integrate-Joachim-SW`. **Umgesetzt in 5.L1** (Details `project-status.md`, Update 5.L1), nicht am Geraet getestet.

## Quellen

| Rolle | Pfad | Version |
|---|---|---|
| Gemeinsame Basis | `C:\viaTraffic\GitHub\Projects\sism555` | 5.55, `T_def_hw_rev` = `J2` |
| Joachim | `C:\viaTraffic\GitHub\Projects\sism560_V2\sism560_V2` | 5.60, `T_def_hw_rev` = `V2` (keine Git-Historie) |
| Unser Stand | `sism555/` in diesem Repo | 5.137 |

Joachim hat gegenüber der Basis rund 570 Zeilen in 20 Dateien geändert, `btio.c` gar nicht (nur eine Zeile in
`btio.h`). Keine neuen Dateien, keine Änderung an `sism.uvprojx`. Unverändert bei ihm: `rtc.c`, `fio.c`,
`xmodem.c`, `ramcode.c`, `USB_tools.c`, alle Sprachdateien außer `sictxt.c`.

## Joachims Änderungen nach Themen

### A. Neue Hardware V2 (Pinbelegung, Transceiver Via-2)

| Datei | Änderung |
|---|---|
| `hard.h` | P1.16 `BT_ON` -> `DIS_BT` (BT-Versorgung, HIGH = aus); P1.20 `EN_ANA` -> `DA_SS`; neu P1.21 `DIS_MESS`; P1.26 `USB_RPWR` -> `FMin` (HF-Messeingang); neu P4.29 `DIS_IF`; `EMGOFF` (P0.20) wieder aktiv; IC37-Bits `HF_EN`/`ANT_EN1`/`ANT_EN2` -> `HF_DIS` (Bit 12); `Def_txrcon` 0; `Min_TXF`/`Max_TXF` 24150/24200; `DPWR` 40 -> 15 ms; neu `HF_DIV`, `HF_const`; Fix `Dis_CapCnt` (`T3CCR=T2CCR=0`); Prototyp `mess_hf()` |
| `hard.c` | Portinit ohne `EN_ANA`/`USB_RPWR`, mit `DIS_MESS`, `DIS_BT` (bei POR HIGH = BT aus), `DIS_IF`; neu `TIMER0_IRQHandler` und `mess_hf()` (HF-Frequenz über CAP0.0) |
| `flash.c` | Power-down: nur GSM aus, `DIS_BT` HIGH, IC37 P1 = 0x10 (HF aus) statt komplett 0 |
| `libtool.c` | `reboot()` analog (und **ohne** `MQTT_fast_disconnect`); `transceiver()` neu für `HF_DIS`, `DTXFEN` entfällt; BT-Einschalten (`DIS_BT` LOW) nach der Batterieanzeige, nur wenn `fp.btmodem` |
| `measure.c` | Transceiver per IC37-Schreiben statt `EN_ANA`; `DTXFEN` raus |
| `main.c` | `EN_ANA` raus; bei POR/Brown-out Anzeige-Treiber aus; Delays 250 -> 150 + 100 ms, 50 -> 30 ms |
| `i2cm.c/.h` | IC37-Init mit `HF_DIS`, P0/P1 alle Ausgänge; Gerätetabelle: `MCP4561` (0x5C, IC32, neuer Typ `DPPT`) statt `PTRTC` (0x64, Typ `RTC`) |
| `gpsio.c` | `CSFLASH2`-Umschalten in der GPS-Erkennung entfernt (war wohl Messhilfe) |
| `alilpc.h` | `PCLKSEL0` |

### B. Neue Funktionen (HF-Frequenz, Abgleich)

- `mess_hf()`, `hf_measurement()`; Taste `STRT` im Hauptmenü misst die Sendefrequenz (bei uns ist dort der Reboot auskommentiert).
- Transceiver-Menü: Frequenzanzeige, neue Punkte "Module Frequency Characteristic" (`f_characteristic`) und "Set Module Frequency" (`set_single_dpp`), Menü bis '5'.
- `set_dpp` für Verstärkerabgleich nur noch 2 Kanäle, MUX_A/B-Umschaltung auskommentiert; `test_i2c_device` kennt `DPPT`, beendet mit `break` statt `return`.
- Texte `T_tmenu`, `T_plsmin`, neu `T_selch`, `T_imp`.

### C. Sprache/OEM im Parameter `fp.sprache`

`fp.sprache` = `(LANGUAGE<<4)|(OEM<<2)|Sprachindex`; alle Leser maskieren `&0x03` (`sio.c`, `sicom.c`);
nach Firmware-Update werden LANGUAGE/OEM nachgetragen (`main.c`), Werksinit setzt alles (`flash.c`).
`sictxt.h`: `LANGUAGE d_f_i`, `OEM no_oem` - steht bei uns schon genauso.

### D. Abstimmungen

- `t_amp_en` 20/20 -> 25/15 (`main.c`), im Menü 20/10 -> 25/15 (`sicom.c`).
- GSM: CTS-Wartezeit `retry` 200 -> 250, PWRKEY-Puls 500 -> 600 ms; Flush `bxi=rxi` vor `+CFUN` (haben wir schon, `gsmio.c:123`).
- Statuszeile: `;COMchg`, wenn `comchange` gesetzt.

### E. Nicht übernehmen

- `REBOOT`-Ereignis und `T_reb[]` entfernt, dafür `#define T_reb "Rebooting"` in `btio.h`. Bei uns bleibt das
  Ereignis `REBOOT` (wird in `reboot()` protokolliert); `btio.c` vergleicht mit `T_reb` = "Reboot" (Präfix von
  "Rebooting", passt).
- `T_version` 5.60, auskommentierte Altfassungen (`transceiver`, `hf_calibration`).

## Überschneidungen mit unserem Stand

Wir haben dieselben Dateien stark geändert (u. a. `sicom.c` ~870, `sio.c` ~480, `gsmio.c` ~430, `hard.c` ~400,
`libtool.c` ~380 Zeilen). Joachims Hunks liegen meist in Funktionen, die wir nicht angefasst haben (Transceiver,
Portinit, Menüs), daher eher geringe Konfliktgefahr. Zu prüfen beim Einbau:

- `reboot()`: Joachim nimmt `MQTT_fast_disconnect` heraus - bei uns behalten.
- Portinit in `hard.c` und `PINMODE3` (bei uns P1.16 Pull-down "BT Modul ein"): auf V2 heißt LOW = BT an, passt.
- BT-Versorgung `DIS_BT`: Joachim schaltet BT nur ein, wenn `fp.btmodem` gesetzt ist. Nach Werksinit
  (`fp.btmodem` = 0) bliebe das Modul stromlos und unsere automatische Erkennung fände es nie. Muss geklärt werden.
- `DIS_BT` bietet erstmals einen Hardware-Reset des IF820 per Versorgung (offener Punkt "Modul hängt, nur
  Power-Cycle hilft").

## Entscheidung 2026-11-12: eine Firmware für alle Hardware-Stände

Anwendervorgabe: Die Firmware erkennt die Hardware selbst. Die Revisionsbuchstaben (`J2`, `V2`) sind nur eine Hilfe
für Menschen, die Firmware richtet sich nicht danach.

| Baugruppe | Typen | Erkennung heute |
|---|---|---|
| LTE-Modul | 2 | automatisch, `fp.gsm` (`gsmio.c`) |
| BT-Modul | 3 (Laird BT730, RN4678, IF820) | automatisch, `fp.btmodem` (`btio.c`) |
| GPS-Modul | 2 (LC76F = 4, LC76G = 5) | automatisch, `fp.gps` (`gpsio.c`, NMEA `GNTXT` bzw. `PQTMVER`); Joachim hat hier nur Messhilfen (`CSFLASH2`) entfernt |
| Radar-Empfänger | 2 (bisheriger Transceiver, Via-2) | **neu**, fehlt noch |

### Entscheidung: Selbsttest, Ergebnis speichern, beim Hochfahren nur prüfen (2026-11-12)

Wie bei GSM/GPS/BT: Der Radar-Typ wird per Selbsttest bestimmt und im Parameterblock gespeichert (neues Feld, z. B.
`fp.radar`, ans Ende der Struktur). Beim Hochfahren wird nur geprüft, ob der gespeicherte Typ noch stimmt; nur bei
"unbekannt" oder Abweichung läuft der volle Test. Die Pins werden nach dem gespeicherten Typ gesetzt, die
Schaltplanklärung entfällt. Die Pin-Fragen unten gelten nur noch für den allerersten Start ohne gespeicherten Typ
(dort bleiben die strittigen Pins Eingang, bis der Test durch ist).

Selbsttest: IC37 schreiben (auf beiden Ständen vorhanden; auf Via-2 heißt P1 = 0 "Transceiver ein"), dann I2C-
Adresse 0x5C (IC32, nur Via-2) ansprechen. Antwort -> Via-2, sonst bisheriger Transceiver. Danach Transceiver aus.
Annahme, am Gerät zu prüfen: Auf altem Board liegt nichts auf 0x5C.

### Konzept (erster Vorschlag, durch obige Entscheidung vereinfacht)

1. Neuer Typ "Radar-Empfänger", zur Laufzeit erkannt (Variable, nicht im Parameterblock, damit sich das
   Parameter-Layout nicht ändert). Werte: unbekannt / Transceiver alt / Via-2.
2. Portinit schaltet nur, was auf allen Ständen gleich ist. Pins mit unterschiedlicher Bedeutung bleiben bis zur
   Erkennung Eingang.
3. Erkennung direkt nach `Init_I2C()`, danach die passenden Pins setzen.
4. Alle Stellen mit Joachims Änderungen (Transceiver ein/aus, Power-down, Reboot, Messzeit, Menüs) fragen den
   erkannten Typ ab und führen den alten oder den neuen Zweig aus.
5. Anzeige des erkannten Typs im Info-/Statusbildschirm, Diagnosecode, wenn keiner erkannt wird.

### Pins mit unterschiedlicher Bedeutung (Klärung mit Hardware nötig)

| Pin | Bisher (Basis 5.55) | Joachim (V2) | Frage |
|---|---|---|---|
| P1.16 | `BT_ON`, nicht getrieben, Pull-down | `DIS_BT`, HIGH = BT aus | Auf altem Board aktiv HIGH oder LOW? |
| P1.20 | `EN_ANA`, HIGH = Analog/Transceiver ein | `DA_SS` (D/A-Wandler) | Was bewirkt HIGH auf dem neuen Board? |
| P1.21 | "Testkopplung", Eingang | `DIS_MESS`, Ausgang | Was hängt auf dem alten Board dran? |
| P1.26 | `USB_RPWR`, Ausgang HIGH = USB-5V-Regler ein | `FMin`, Messeingang | Wird der USB-Regler auf dem neuen Board anders versorgt? Darf P1.26 kurz Ausgang sein? |
| P4.29 | nicht benutzt | `DIS_IF`, LOW = Transceiver-Verstärker ein | Auf altem Board frei? |
| IC37 P1 | Bit 10 `HF_EN`, Bit 12/13 `ANT_EN1/2` | Bit 12 `HF_DIS` | - |
| I2C 0x64 / 0x5C | `PTRTC` | `MCP4561`/DS3501 (IC32) | Liegt 0x5C nur auf dem Via-2-Empfänger? Braucht er Transceiver-Versorgung? |

Offen außerdem: Sind diese Unterschiede am Mainboard oder am Radar-Empfänger? `USB_RPWR` spricht für eine
Mainboard-Änderung. Kann ein neues Mainboard mit altem Empfänger vorkommen (und umgekehrt)?
