# Umstieg auf aktuelle Werkzeuge - Aufwandsschaetzung (2026-11-26)

Stand im Projekt: uVision/MDK 5.28 (2019), Arm Compiler 5 (ARMCC), CMSIS 4.2.0, RTX 4.75 (CMSIS-RTOS v1),
MDK-Middleware 6.2.0 (USB Device CDC, USB Host MSC, File System), LPC1700_DFP 2.0.0.
Aktuell (keil.com, 2026-11-26): MDK-Middleware 8.4.0, CMSIS 6.4.0, CMSIS-RTX 5.9.1 (RTOS2), Arm Compiler 6,
LPC1700_DFP 2.7.2 (eingestellt, letzter Stand). Lokal schon installiert: CMSIS 5.8.0, MDK-Middleware 7.8.0, ARMCLANG.

## Was im Code betroffen ist (gezaehlt, eigener Code ca. 19.600 Zeilen in 30 .c-Dateien)

| Bereich | Fundstellen | Aenderung |
|---|---|---|
| RTOS-Threads | 3 Threads (USB-Host, GSM, MQTT), `osThreadDef/Create` in measure.c | `osThreadNew` mit Attributen |
| RTOS-Signale | ca. 15 Stellen `osSignalSet/Wait/Clear` (gsmio, mqtt, measure, sictst, USB_tools) | Thread-Flags (`osThreadFlagsSet/Wait/Clear`) |
| `osDelay` | 176 Stellen | bleibt (1 Tick = 1 ms) |
| main.c | Kernel-Start, `osKernelInitialize` nur bei POR/EXTR/BOR, main laeuft als Thread (256 Worte) | Hauptschleife wird `app_main`-Thread, Sonderfall Deep-Sleep-Reset neu loesen |
| RTX-Konfiguration | `RTX_Conf_CM.c` (Stackpool, OS_CLOCK 24 MHz) | neues `RTX_Config.h`, Stacks je Thread (loest nebenbei den zu kleinen Stackpool) |
| `__noinit` / `__at` | Makros in alilpc.h (114 bzw. 4 Verwendungen), `#pragma arm section` in hard.c | AC6 kennt `zero_init`, `at` und `#pragma arm section` nicht -> Makros und hard.c umstellen, **non_init-Layout muss byte-gleich bleiben** (fp ueber Updates) |
| Scatter-Datei | `Objects/sism.sct` | RTX5-Bereiche, Sektionsnamen pruefen |
| File System | ca. 60 Stellen (`finit`, `fmount`, `fopen` ...) in 24 Dateien | API weitgehend gleich, stdio-Anbindung ab MW 8 ueber CMSIS-Compiler-Pack |
| USB Device/Host | ca. 33 Stellen in 5 Dateien, RTE-Konfigdateien | Konfigdateien neu, Callback-Vorlagen abgleichen; Treiber V2.14 |
| Leere Warteschleifen | 4 | AC6 optimiert sie weg -> volatile/Timer |

## Stufen

**Stufe 1 - AC6 + RTX5/RTOS2 + MDK-Middleware 7.8 + CMSIS 5.8 + USB-Treiber V2.14** (alles schon installiert,
MDK 5.28 reicht voraussichtlich). Umsetzung am Tisch ca. **3-5 Arbeitstage**, danach Testkampagne (BT kurz/voll,
LTE/MQTT, USB-Geraet und USB-Stick, Update ueber RS232/USB/Stick in beide Richtungen mit fp-Erhalt, Tiefschlaf,
Radar/I2C, Dauerlauf) ca. **3-5 Tage**, dazu Beobachtung auf mehreren Boards. Zusammen ca. **2 Wochen**.

**Stufe 2 - CMSIS 6 + MDK-Middleware 8 + CMSIS-Compiler**, braucht ein aktuelles MDK/uVision (Version und Lizenz
pruefen). Nach Stufe 1 ca. **1-2 Tage** plus Kurztest.

## Risiken

- Zeitverhalten aendert sich (anderer Compiler, anderer Scheduler): BT-Handshake, XMODEM, Radar-Timing nachmessen.
- non_init/fp-Layout: ein Fehler verdreht beim Update die Parameter (vgl. 5.155). Vergleich gegen die alte Map ist Pflicht.
- Flash: heute ca. 183 KB von 256 KB; AC6 -Oz meist kleiner, RTX5 etwas groesser.
- Lizenz: MDK-Middleware 6.2 laeuft unter MDK-Pro; MDK-Middleware 8 ist nach meinem Kenntnisstand quelloffen
  (nicht geprueft). Neues MDK braucht eventuell eine gueltige Wartung der Keil-Lizenz.

## Offene Fragen

1. Welche Keil-Lizenz (MDK-Pro? Wartung bis wann?) und welche MDK-Version duerfen wir nutzen?
2. Baut ausser uns noch jemand (Joachim, Fertigung)? Alle brauchen dieselbe Werkzeugkette.
3. Eigener Branch fuer den Umstieg, nach dem USB-Merge?
