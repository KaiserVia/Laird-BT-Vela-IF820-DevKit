# Umstieg auf aktuelle Werkzeuge (LPC17xx) - Aufwand, Nutzen, Kosten, Parallelbetrieb

Stand 2026-11-27 (erste Fassung 2026-11-26). Betrifft die Geraetefirmware `sism555` (LPC1766).

## 1. Ausgangslage

| | heute im Projekt | aktuell verfuegbar | lokal schon installiert |
|---|---|---|---|
| IDE / Lizenz | uVision 5.28 (2019), **MDK Professional 5.28** | uVision 5.4x bzw. MDK v6 (Abo) | 5.28 |
| Compiler | Arm Compiler 5.06u6 (ARMCC, Support beendet) | Arm Compiler 6 (armclang) | AC 6.12 (in MDK 5.28 enthalten) |
| CMSIS | 4.2.0 (ca. 2014) | 6.4.0 | 4.2.0, 5.5.1, 5.8.0 |
| RTOS | RTX 4.75, CMSIS-RTOS v1 | CMSIS-RTX 5.9.1, CMSIS-RTOS2 | RTX5 in CMSIS 5.8 |
| Middleware (USB, File System) | MDK-Middleware 6.2.0 (ca. 2014) | MDK-Middleware 8.x | 6.2.0, 7.8.0 |
| Device-Pack | LPC1700_DFP 2.0.0 | 2.7.2 (letzte Version, Keil pflegt nicht mehr) | 2.0.0 (USB-Treiber aus 2.7.2 seit 5.u23/5.158 als Kopie im Projekt) |

Der LPC1766 selbst bleibt; "aktuellste 17xx-Version" heisst hier: aktuelle Werkzeuge und Bibliotheken fuer denselben Chip.

## 2. Was gemacht werden muss

Eigener Code ca. 19.600 Zeilen in 30 .c-Dateien. Gezaehlte Fundstellen:

| Bereich | Fundstellen | Aenderung |
|---|---|---|
| RTOS-Threads | 3 Threads (USB-Host, GSM, MQTT), `osThreadDef/Create` in measure.c | `osThreadNew` mit Attributen |
| RTOS-Signale | ca. 15 Stellen `osSignalSet/Wait/Clear` (gsmio, mqtt, measure, sictst, USB_tools) | Thread-Flags (`osThreadFlagsSet/Wait/Clear`) |
| `osDelay` | 176 Stellen | bleibt (1 Tick = 1 ms) |
| main.c | Kernel-Start, `osKernelInitialize` nur bei POR/EXTR/BOR, main laeuft als Thread (256 Worte) | Hauptschleife wird `app_main`-Thread, Sonderfall Deep-Sleep-Reset neu loesen |
| RTX-Konfiguration | `RTX_Conf_CM.c` (Stackpool, OS_CLOCK 24 MHz) | neues `RTX_Config.h`, Stack je Thread |
| `__noinit` / `__at` | Makros in alilpc.h (114 bzw. 4 Verwendungen), `#pragma arm section` in hard.c | AC6 kennt `zero_init`, `at` und `#pragma arm section` nicht -> Makros und hard.c umstellen, **non_init-Layout muss byte-gleich bleiben** (fp ueber Updates) |
| Scatter-Datei | `Objects/sism.sct` | RTX5-Bereiche, Sektionsnamen pruefen |
| File System | ca. 60 Stellen (`finit`, `fmount`, `fopen` ...) in 24 Dateien | API weitgehend gleich, stdio-Anbindung ab MW 8 ueber CMSIS-Compiler-Pack |
| USB Device/Host | ca. 33 Stellen in 5 Dateien, RTE-Konfigdateien | Konfigdateien neu, Callback-Vorlagen abgleichen; Treiber V2.14 (schon da) |
| Leere Warteschleifen | 4 | AC6 optimiert sie weg -> volatile/Timer |
| Inline-Assembler, ARMCC-Eigenheiten | pruefen (`__asm`, `__packed`, `__irq`) | auf AC6-Schreibweise umstellen |

**Stufe 1 - AC6 + RTX5/RTOS2 + MDK-Middleware 7.8 + CMSIS 5.8** (alles schon installiert, MDK 5.28 reicht):
Umsetzung ca. **3-5 Arbeitstage**, Testkampagne ca. **3-5 Tage** (BT kurz/voll, LTE/MQTT, USB-Geraet und USB-Stick,
Update ueber RS232/USB/Stick in beide Richtungen mit fp-Erhalt, Tiefschlaf, Radar/I2C, Dauerlauf), dazu Beobachtung auf
mehreren Boards. Zusammen ca. **2 Wochen**.

**Stufe 2 - CMSIS 6 + MDK-Middleware 8 + CMSIS-Compiler:** braucht ein neueres uVision (5.38 oder neuer, genaue
Mindestversion aus den Pack-Anforderungen pruefen). Nach Stufe 1 ca. **1-2 Tage** plus Kurztest.

## 3. Nutzen, Verbesserungen, Fehlerbehebungen

**Direkt fuer unser Projekt:**

1. **Stacks je Thread mit Ueberlaufpruefung (RTX5).** Heute teilen sich die Threads einen Stackpool ohne Kontrolle
   (Review 2026-11-11, Punkt 1.1 "knappe Thread-Stacks"). RTX5 legt jeden Stack einzeln an, prueft bei jedem
   Threadwechsel auf Ueberlauf und meldet ihn (`osRtxErrorNotify`) - statt dass Speicher still ueberschrieben wird.
   Im Debugger zeigt der Component Viewer Threadzustand und maximale Stacknutzung.
2. **Bessere Warnungen (AC6/Clang).** Findet z. B. uninitialisierte Variablen; genau so ein Fehler war 5.L2
   (`test_i2c_device()`, uninitialisiertes `typ`). Dazu striktere Typpruefung, C11/C17.
3. **Kleinerer Code.** AC6 erzeugt meist kleineren Code (`-Oz`); heute ca. 183 KB von 256 KB Flash belegt. RTX5 ist
   etwas groesser als RTX4, unterm Strich voraussichtlich mehr Reserve. Erst nach dem Build sicher.
4. **Fehlerbehebungen in der Middleware.** Zwischen MDK-Middleware 6.2 (ca. 2014) und 7.8/8.x liegen viele Versionen mit
   Korrekturen in USB Device, USB Host und File System. Die beiden schwersten USB-Fehler (F11, F12) lagen im Treiber
   und sind mit 5.u23 schon behoben; was die Middleware-Updates fuer uns konkret bringen, vor dem Umstieg aus den
   Release Notes auswerten.
5. **RTOS2-Funktionen:** Thread-Flags, Event-Flags, Timer und Message-Queues mit Zeitlimits, 32-bit-Tickzaehler,
   Event Recorder fuer Zeitmessungen ohne Terminalausgabe.

**Langfristig:**

6. **Arm Compiler 5 ist abgekuendigt.** Keine Fehlerbehebungen mehr, seit MDK 5.37 nicht mehr im Installer, in MDK v6
   nur noch in der Professional Edition als Altwerkzeug. Je laenger wir warten, desto groesser der Sprung.
7. **MDK-Middleware 8 ist auf Arm-Bausteinen kostenlos nutzbar**, auch kommerziell (Arm-eigene Lizenz, nicht
   Apache). Heute brauchen wir dafuer die MDK-Professional-Lizenz.
8. **Aktuelle Beispiele, Doku, Foren** beziehen sich auf AC6/RTOS2; neue Kollegen kennen die alten Werkzeuge kaum.
9. Spaeter moeglich: Bauen per Kommandozeile (CMSIS-Toolbox, Projektdateien als Text) und in VS Code - einfacher
   in Git, automatische Builds.

## 4. Kosten

| Posten | Stufe 1 | Stufe 2 |
|---|---|---|
| Arbeit Umstellung | 3-5 Tage | 1-2 Tage |
| Arbeit Tests | 3-5 Tage + Beobachtung | Kurztest 1 Tag |
| Lizenz | **0 €**: MDK Professional 5.28 vorhanden, AC6, CMSIS 5.8, MW 7.8 installiert; Packs sind kostenlos | neues uVision: mit gueltiger Wartung der 5.28-Lizenz evtl. ohne Mehrkosten (pruefen); sonst MDK v6 im Abo |
| MDK v6 Abo (Listenpreis Arm-Store, je Arbeitsplatz) | - | Essential 99 $/Monat (ca. 1.200 $/Jahr), Professional 199 $/Monat (ca. 2.400 $/Jahr) |

Essential reicht fuer Stufe 2 (AC6 und MW 8 enthalten); Professional braeuchte man nur, um in MDK v6 auch noch AC5
oder sicherheitszertifizierte Compiler zu nutzen. Jeder, der baut, braucht einen eigenen Platz (wir, ggf. Joachim,
ggf. Fertigung). Preise sind Listenpreise vom 2026-11-27, Angebot/Haendlerpreis einholen.

## 5. Nachteile und Risiken

- **Zeitverhalten aendert sich** (anderer Compiler, anderer Scheduler): BT-Handshake, XMODEM, Radar-Timing,
  Tiefschlaf nachmessen. Deshalb die volle Testkampagne.
- **non_init/fp-Layout:** ein Fehler verdreht beim Update die Parameter (vgl. 5.155). Vergleich gegen die alte Map ist
  Pflicht, dazu Update alt -> neu -> alt mit fp-Pruefung.
- **Zwei Wochen ohne neue Funktionen**, und in dieser Zeit laufen Aenderungen in anderen Branches auseinander
  (Merge-Konflikte in main.c, measure.c, hard.c).
- **Alle, die bauen, muessen mitziehen** (siehe Abschnitt 6). Joachims Stand muss vorher gemergt sein, sonst wird
  jeder spaetere Merge aufwendiger.
- **Stufe 2 bringt laufende Kosten** (Abo statt Dauerlizenz) und eine Online-Lizenzpruefung je Benutzer.
- Gewohnte ARMCC-Eigenheiten (Warnungsnummern, `__packed`, Scatter-Syntax) aendern sich; einmalige Einarbeitung.
- Kein Gewinn bei Funktionen, die heute schon laufen - der Nutzen ist vor allem Absicherung und Wartbarkeit.

## 6. Parallelbetrieb: muessen alle anderen Projekte mit?

**Nein.** Belegt an unserer Installation:

- **Packs:** Jedes Projekt legt seine Pack-Versionen fest (`versionMatchMode="fixed"` in `sism.uvprojx`). Auf diesem PC
  sind schon CMSIS 5.8 und MDK-Middleware 7.8 installiert, `sism` baut trotzdem unveraendert mit CMSIS 4.2 und MW 6.2.
  Neue Packs stoeren alte Projekte also nicht.
- **Compiler:** MDK 5.28 enthaelt AC5 **und** AC6; der Compiler wird je Projekt (Options for Target) gewaehlt.
- **Stufe 1** laeuft damit komplett parallel: neuer Branch mit AC6/RTX5, `main` und alle anderen Projekte bauen
  weiter wie bisher - auf demselben PC.
- **Stufe 2 (neues uVision/MDK v6):** in einen eigenen Ordner installieren (z. B. `C:\Keil_v6`) oder die 5.28-
  Installation behalten. Alte Projekte mit AC5 bleiben auf 5.28 (die Dauerlizenz gilt dafuer weiter); MDK v6
  Essential kann AC5 nicht nutzen.

**Was mitziehen muss:** nur wer **dieses** Projekt nach dem Merge baut (Joachim, Fertigung, Build-PC). Fuer Stufe 1
heisst das: MDK 5.28 Professional (haben sie vermutlich), dazu CMSIS 5.8 und MDK-Middleware 7.8 als kostenlose Packs.
Andere Firmware-Projekte koennen bleiben, wie sie sind, und spaeter einzeln umsteigen.

## 6a. Variante ohne uVision: GCC + CMSIS-Toolbox (lizenzfrei)

Anwenderfrage 2026-11-27: ohne uVision bauen, Debugger wird nicht gebraucht.

| Baustein | Werkzeug | Lizenz |
|---|---|---|
| Compiler | Arm GNU Toolchain (arm-none-eabi-gcc) | frei (GPL fuer das Werkzeug, die erzeugte Firmware ist frei) |
| Build | CMSIS-Toolbox (`cbuild`, Projekt als `.csolution.yml`/`.cproject.yml`, Text in Git) | Apache 2.0 |
| CMSIS 6, CMSIS-RTX 5 (RTOS2) | Packs | Apache 2.0 |
| USB Device/Host, File System | MDK-Middleware 8 als Quelltext; laut Keil "can be built with ... Arm Compiler 6, GCC, and LLVM", ohne IDE-Lizenz | Arm-Lizenz, auf Arm-Bausteinen kostenlos auch kommerziell |
| USB-Treiber | unsere Kopien aus LPC1700_DFP 2.7.2 (USB_Driver_Patch) | Apache 2.0 |
| Flashen | wie heute per XMODEM ueber RS232/USB (`flash_xmodem.py`) | - |

Zusaetzlich zu Stufe 1+2 noetig:
- **Startup und Linker-Skript fuer GCC selbst schreiben:** das LPC1700_DFP 2.7.2 enthaelt nur `startup_LPC17xx.s` fuer
  den Arm-Compiler, kein GCC-Startup. Linker-Skript statt Scatter-Datei, mit `non_init` an derselben Adresse,
  Sektionen fuer die USB- und FS-Puffer (MW 8 verlangt eigene Speicherbereiche).
- `__noinit` (117 Stellen ueber das Makro), `__at` (3), `#pragma arm section` (2), `__align` (2) auf
  `__attribute__((section/aligned))` umstellen - dasselbe wie bei AC6.
- C-Bibliothek: newlib-nano statt Keil-Bibliothek (printf, stdio-Anbindung des File Systems, Heap).
- GCC erzeugt meist etwas groesseren Code als der Arm-Compiler (grob 5-15 %); bei 183 von 256 KB voraussichtlich
  ausreichend, nach dem ersten Build pruefen.
- Das Update-Format muss gleich bleiben: eine alte Firmware muss die neue per XMODEM laden koennen und umgekehrt,
  fp bleibt erhalten.

Aufwand: ca. **3 Wochen** (Stufe 1+2 plus ca. 1 Woche fuer Startup, Linker-Skript, Bibliothek, Groessenpruefung).
Kosten: **0 € Lizenz**, auch fuer Joachim, Fertigung und einen Build-Server. Nachteil: kein uVision-Debugger mehr fuer
dieses Projekt (Ersatz bei Bedarf: VS Code mit Cortex-Debug und pyOCD/OpenOCD, ebenfalls frei). Neue Platinen ohne
Firmware: Erstprogrammierung klaeren (heute vermutlich ULINK/uVision; frei: LPC-ISP ueber UART0 mit Flash Magic oder
lpc21isp, oder pyOCD mit Debug-Adapter). Parallelbetrieb wie in Abschnitt 6: die alten Projekte bleiben in uVision.

Quellen: MDK-Middleware-Pack (Toolchains, Lizenz) https://www.keil.arm.com/packs/mdk-middleware-keil ,
Migration File System V8 https://arm-software.github.io/MDK-Middleware/main/FileSystem/migration_guide.html .

## 7. Offene Fragen

1. Laeuft fuer die MDK-Professional-Lizenz noch eine Wartung (dann evtl. neues uVision ohne Abo)? Wie viele Plaetze?
2. Wer baut ausser uns (Joachim, Fertigung, Build-PC)? Welche MDK-Version und Packs haben sie?
3. Welche anderen Projekte laufen auf derselben Installation (andere Geraete, Bootloader)?
4. Zeitpunkt: `usb-verbindung` ist in 5.158 gemergt; vor dem Start noch `rst-start` mergen. Umsetzung im Branch
   `bootloader` (Werkzeugumstieg ist dort Schritt 0, siehe `Konzept_Bootloader_Security.md`).

Quellen: Arm Keil MDK v6 Editionen https://keil.arm.com/keil-mdk , Preise https://store.arm.com/mdk-6/ ,
MDK 5.40 Release Notes (AC5 nicht mehr im Installer) https://www.keil.com/update/relnotes/MDK540.htm ,
MDK-Middleware (Lizenz, Versionen) https://www.keil.arm.com/packs/mdk-middleware-keil .
