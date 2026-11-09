# IF820 + UART-MUX: Wo gehoeren Pull-ups/-downs hin, wo nicht? (Stand 2026-11-09)

Quellen:
- **[DS]** `Documentation/EZ-DS-Vela IF820_v2_2.pdf`: Pin-Tabellen 18 (MHF4-Variante) und 21 (integrierte Antenne),
  Spalte "Pin When Not Used", Fussnoten 3-5, Abschnitt 9.4 Referenzschaltung.
- **[EZS]** `Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf`: 2.3.2 UART/GPIO, 2.4 CYSPP-Pinzustaende,
  2.4.7.8 CYSPP-Konfiguration, Abschnitt zu DATA_READY und CTS.
- **[TI]** TS5A23157-Datenblatt (SCDS165F, Jan 2019), Funktionstabelle 8.4 und Kennwerte 6.6. Gelesen ueber die TI-Webseite,
  nicht alle Kapitel waren lesbar.
- Schaltplan-Auswertung Mainboard-V2: `Fragen_Schaltplan_CTS_MUX.md`.

Es gelten zwei Grundtatsachen:
1. **Der TS5A23157 hat keine eigenen Pull-Widerstaende.** Laut Funktionstabelle ist der nicht gewaehlte Kanal einfach
   AUS (offen, Leckstrom max. +-1 uA). Steht der MUX auf LTE, haengen die vier BT-Leitungen also offen. Einen
   definierten Pegel gibt es dort nur, wenn das Modul selbst die Leitung treibt (Ausgaenge) oder ein externer
   Widerstand sie hochzieht bzw. herunterzieht (Eingaenge).
2. **Fuer die PUART-Pins des IF820 (P10, P11, P32, P37) nennt das Datenblatt keinen internen Pull** (Spalte I/O nur
   "I/O", kein "PU"). Bei den HCI-UART-Pins (BT_UART_*) steht dagegen ausdruecklich "PU". EZ-Serial nutzt die PUART
   [EZS 2.3.2]. Wie die Pins intern konfiguriert sind, verraet das Datenblatt nicht. Ein offener Eingang ist deshalb als
   **undefiniert** zu behandeln.

Hinweis zur Rueckspeisung: Jeder Pull-up an einer Modulleitung muss an die **geschaltete Modulversorgung** (N$184)
gehen, nicht an die dauerhafte 3V. Sonst speist er das Modul ueber den IO-Pin, wenn T10 es abschaltet. Rueckspeisung
hat schon zwei Module zerstoert (CLAUDE.md, Betriebsregel). R125 haengt richtig an N$184.

## Bewertung je Leitung

| Leitung (Modul-Pin) | Richtung am Modul | Bei MUX auf LTE | Pull noetig? | Empfehlung |
|---|---|---|---|---|
| **BLT-RXD** (P37, P_UART_RXD) | Eingang | offen, Rauschen | **ja** | **Pull-up 10-100 kOhm nach N$184.** Eine UART-Leitung ist im Ruhezustand HIGH. Ohne Pull-up liest das Modul Rauschen als Daten: in CYSPP/SPP gehen sie als Muell (`0xFF`) zur App, im Befehlsmodus als zufaellige API-Zeichen. Fehlt laut Schaltplan V2. |
| **BLT-RTS** = Modul-CTS (P10, P_UART_CTS) | Eingang (bei `STU F=01`) | ueber R125 HIGH = "Host nicht bereit" | ja, ein definierter Pegel (sonst schwebt der Eingang) | **Pull-up oder Pull-down: Abwaegung, siehe unten.** Heute R125 10k nach N$184. |
| **BLT-CTS** = Modul-RTS (P11, P_UART_RTS) | Ausgang (bei `STU F=01`) | wird vom Modul getrieben, geht ins Leere | nein | Kein Bauteil noetig. Ohne Flusskontrolle (Werksmodul, `F=00`) treibt das Modul den Pin nicht. Dann haelt der interne Pull-up des LPC P0.17/CTS1 die Leitung auf HIGH = "nicht senden". Das faengt die Firmware mit dem `cts_bypass`-Bootstrap ab (5.16). |
| **BLT-TXD** (P32, P_UART_TXD) | Ausgang | vom Modul getrieben, geht ins Leere | nein | Kein Bauteil noetig. Ein Pull-up waere nur dann sinnvoll, wenn der LPC-Eingang bei stromlosem Modul nicht schweben soll. Das deckt der interne Pull-up des LPC bereits ab. |
| **CYSPP** (P13, "CYSPP/SPP", Netz BLT-LINK) | Ein-/Ausgang, das Modul zieht ihn selbst | - | **nein, und auf keinen Fall Pull-down** | [EZS 2.4] "CYSPP pin left floating" ist der vorgesehene Betrieb (Autostart). LOW erzwingt immer den CYSPP-Modus, auch beim Booten. Ein Pull-down setzt das Modul also dauerhaft in den CYSPP-Modus und sperrt die API. Ein aktiver HIGH-Pegel bricht Verbindungen ab, das ist auf dem DVK nachgewiesen (CLAUDE.md). **R77 (10k) darf deshalb nicht als Pull-down bestueckt sein.** Laut Schaltplan V2 ist R77 nur in der Variante "ohne LTE" vorhanden; pruefen, wohin R77 zieht. R78 (100 Ohm, Serienwiderstand zu IC36 IO7) ist unbedenklich, solange IO7 Eingang bleibt. |
| **CONNECTION** (P27, "Connection Indication", Fussnote 5) | Ausgang | - | nein | Das Modul treibt den Pin (16 mA sink). Der interne Pull-up am LPC P0.19 reicht aus. Laut Schaltplan V2 direkt verbunden, das passt. |
| **CP_ROLE** (P1) | Eingang | - | **nein** | [EZS] Offen lassen heisst: Die Rolle kommt aus der Konfiguration (Peripheral). Ein Pull-down macht das Modul zum Central (das bekannte Fehlerbild "`.CYSPPSTART` scannt statt zu werben"). |
| **LP_MODE** (P12) | Eingang | - | nein | [EZS 2.3.2] "internally pulled up" = Schlaf aus. Offen lassen. |
| **RST_L** | Eingang | - | nein | [DS] "internal pull-up", "Pin When Not Used: floating". R124 nicht bestuecken. Empfehlung bleibt (CLAUDE.md): RST ueber das R124-Pad auf einen LPC-GPIO (Open-Drain) fuehren, damit der LPC ein haengendes Modul zuruecksetzen kann. |
| **BT_DEV_WAKE / BT_HOST_WAKE** | Ein-/Ausgang | - | nein | [DS] "floating". R139 nicht bestueckt, das ist in Ordnung. |
| **MUX-Steuerung CSAF** (LPC P0.1 -> IN von IC13/IC18) | Eingang des TS5A23157 | - | definierter Pegel | [TI 6.6, Fussnote 2]: "All unused digital inputs ... must be held at V+ or GND". Der LPC treibt den Pin. Waehrend Reset und Boot haelt der interne Pull-up des LPC ihn HIGH, der MUX steht dann also auf **LTE**. Das ist definiert, aber nur ueber den internen Pull-up. Ein externer Pull-up oder Pull-down waere nur noetig, wenn der Startzustand bewusst anders sein soll. |

## Die eigentliche Abwaegung: Modul-CTS (R125) Pull-up oder Pull-down?

Beim Modul-CTS ist es keine reine Fehlerfrage, beide Varianten haben einen Preis:

- **Pull-up (heute):** Laut [EZS, Abschnitt DATA_READY/CTS] heisst CTS HIGH: "the module waits to send any outgoing UART
  data". Das ist die vorgesehene Bedeutung. Solange LTE die Leitungen hat, sammelt das Modul seine Ausgaben, statt sie
  ins Leere zu schicken. Das Problem: Die Modulfirmware schliesst dann offenbar auch den BLE-Handshake nicht ab (Abo des
  Datenkanals, gesehen auf B und schon in 5.127 beim CCCD-Write). Eine Verbindung haengt, bis der MUX zurueck auf BT
  schaltet. Diesen Weg geht die Firmware mit 5.137 (CONNECTION schaltet sofort zurueck).
- **Pull-down:** Das Modul darf immer senden, der Handshake laeuft durch. Alles, was das Modul waehrend LTE ausgibt,
  geht aber **verloren**: Ereignisse (`@E,...,C`, `DIS`, ...) und in CYSPP/SPP auch echte Nutzdaten. Die
  Ereignisauswertung der Firmware (Zustandsmaschine ab 5.130) wuerde dann Verbindungs- und Trennereignisse verpassen.
  Ob P10 beim Start eine Sonderfunktion hat (Strapping), steht nicht im Datenblatt. Weder Tabelle 18 noch Tabelle 21
  nennt so etwas. P10 ist dort ein normaler GPIO (KSO2, ADC, PA_Ramp optional).

**Empfehlung:** R125 nicht ungeprueft umdrehen. Wenn 5.137 die Verbindung waehrend LTE zuverlaessig rettet, ist der
Pull-up die sauberere Loesung (es gehen keine Daten verloren). Ein Pull-down kommt nur in Frage, wenn die Firmwareloesung
nicht reicht. Dann muss die Firmware damit leben, dass Ereignisse waehrend LTE fehlen.

Noch offen ist ein Widerspruch: Ist R125 auf allen Boards gleich bestueckt, muesste auch 26SC9966 waehrend LTE haengen.
Das tut es nicht. Klaeren laesst sich das mit einer Messung am Modul-CTS waehrend MQTT online, auf B und auf 26SC9966.

## Kurzfassung: Was aendern?

1. **BLT-RXD: Pull-up 10-100 kOhm nach N$184 nachruesten.** Eindeutig sinnvoll, beseitigt den `0xFF`-Muell.
2. **R77:** pruefen, wohin er zieht. Als Pull-down an CYSPP darf er nicht bestueckt sein.
3. **R125 (Modul-CTS):** vorerst lassen. Entscheiden erst nach dem Ergebnis von 5.137 und der CTS-Messung.
4. Alle anderen Leitungen (TXD, Modul-RTS, CONNECTION, CP_ROLE, LP_MODE, RST, WAKE-Pins): **kein Pull noetig**.
   Bei CP_ROLE und CYSPP sind Pulls sogar schaedlich.
5. Pull-ups an Modulleitungen immer an N$184 (geschaltete Modulversorgung) anschliessen, nie an die dauerhafte 3V.

Nicht geprueft: die Referenzschaltung (Abschnitt 9.4/10.4) und die GPIO-Tabelle im EZ-Serial-Guide (Abschnitt 8.1).
Beide sind nur als Bild in der PDF, der Text liess sich nicht auslesen. Nach dem Text der Referenzschaltung verlangt sie nur
100-nF-Abblockkondensatoren an 3P0V/VDDIO und empfiehlt den 32-kHz-Quarz, Pull-Widerstaende nennt sie nicht.
