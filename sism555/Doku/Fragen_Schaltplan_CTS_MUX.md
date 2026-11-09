# Fragen an den Schaltplan: Was sieht das IF820, solange der MUX auf LTE steht?

Stand 2026-11-09. Anlass: BLE-Verbindung während MQTT online geht auf Board 26SC9966, scheitert aber auf Board B
(26SC9983). Auf B schließt das Modul den Handshake (Abo des Datenkanals) nicht ab. Verdacht: Das IF820 sieht seinen
CTS-Eingang als "nicht bereit", solange der UART-MUX auf LTE steht. Details: `sism555/project-status.md`, Abschnitt
"LTE-BLE-Test auf 26SC9966 mit 5.136".

## Was wir schon wissen (aus `Doku/BT-Evalkit-Tasks.md`)

```
Daten:  IC18 (TS5A23157)  1Y0 = BLT-TXD -> LPC RXD1     2Y0 = BLT-RXD <- LPC TXD1
Flow:   IC13 (TS5A23157)  1Y0 = BLT-RTS <- LPC RTS1     2Y0 = BLT-CTS -> LPC CTS1
Select: CSAF (P0.1)  LOW = BT,  HIGH = LTE (Y1-Seite: LTE-TXD/-RXD/-RTS/-CTS)
```

Der TS5A23157 ist ein Analogschalter. Ist LTE gewählt, ist die BT-Seite (Y0) **offen**: Die Leitungen zum IF820
werden dann von niemandem getrieben. Welchen Pegel das Modul dann sieht, hängt nur an den Widerständen an diesen
Leitungen und an den internen Pull-ups/-downs des Moduls.

## Die Fragen

**1. Modul-CTS (Netz `BLT-RTS`, IC13 Pin 1Y0 → IF820 CTS-Pin)** - die wichtigste Frage
- Sitzt an diesem Netz ein Pull-up (nach 3V/VDDIO) oder ein Pull-down (nach GND)? Welcher Wert?
- Ist er in der Variante `Abwärtskompatibel mit LTE` bestückt? In der anderen Variante?
- Gibt es einen Serienwiderstand oder sonst etwas zwischen IC13 und dem Modul?
- Erwartung: Pull-up oder nichts → Modul sieht "nicht bereit" → passt zum Fehler auf B. Pull-down → Modul darf
  senden → passt zu 26SC9966.

**2. Modul-RXD (Netz `BLT-RXD`, IC18 Pin 2Y0 → IF820 RXD-Pin)**
- Pull-up vorhanden und bestückt? Falls nicht: Das erklärt die `0xFF`-Bytes, die die App vor dem Banner bekommt.
  Das Modul reicht das Rauschen der offenen Leitung als Daten weiter.

**3. Modul-TXD und Modul-RTS (Netze `BLT-TXD`, `BLT-CTS`)**
- Pull-ups/-downs vorhanden, bestückt? (Weniger wichtig, der Vollständigkeit halber.)

**4. Bestückungsvarianten**
- Welche Bauteile im Bereich IF820 / IC13 / IC18 / IC36 / IC54 unterscheiden sich zwischen den Varianten?
- In welcher Variante sind **B (26SC9983)** und **26SC9966** gebaut? (B meldet HW `V2`.) Weicht die echte Bestückung
  vom Eagle-Variantenflag ab? Das ist schon einmal passiert, siehe C128/C58.
- Was genau wurde am 2026-11-04 an 26SC9966 geändert ("HW-Maßnahme" gegen die Modulneustarts)? Und was beim
  CONNECTION-Umbau (IO5 von GND getrennt, Draht auf P0.19)?

**5. CONNECTION (IF820 Pin 33 → IC36 IO5 bzw. P0.19)**
- Hängt am Netz noch etwas, z. B. ein Pull-up oder die alte GND-Verbindung, je nach Variante?

## Antworten aus dem Schaltplan Mainboard-V2 (Stand 05.11.2026, ausgewertet 2026-11-09)

Quelle: `Mainboard-V2-A1_05_11_2026.sch/.brd` (Layout-Rev.1). **Einschränkung:** Das ist das NEUE Board V2. Ob die alten
Boards B (26SC9983) und 26SC9966 an diesen Leitungen gleich beschaltet sind, ist nicht belegt - dafür bleibt die Messung
(Abschnitt "Gegenprobe").

Zuordnung (IF820 = IC37, Flow-Mux = IC13, Daten-Mux = IC18):

| Netz | MUX-Pin | IF820-Pin | Bauteil am Netz |
|---|---|---|---|
| BLT-RTS (= Modul-CTS) | IC13 Pin 9 (1Y0) | Pin 14 (P10) | **R125 10 kΩ als Pull-up nach N$184** |
| BLT-CTS | IC13 Pin 7 | Pin 16 (P11) | keins |
| BLT-RXD | IC18 Pin 7 | Pin 15 (P37) | keins |
| BLT-TXD | IC18 Pin 9 | Pin 17 (P32) | keins |

N$184 ist die geschaltete 3V-Versorgung des Moduls (P-MOSFET T10 DMP1045U-7 über R107 0 Ω).

1. **Modul-CTS:** R125 10 kΩ ist ein **Pull-up** nach 3V, in **beiden** Varianten bestückt (kein Variantenflag), kein
   Serienwiderstand zwischen IC13 und Modul. Bei MUX auf LTE ist die BT-Seite offen, das Modul sieht HIGH = "nicht
   bereit". Das passt zum Fehler auf Board B. **Fix:** R125 als **Pull-down** nach GND statt nach 3V. Vorher im
   IF820-Datenblatt prüfen, ob P10 beim Start ein Strapping-Pin ist (nicht geprüft).
2. **Modul-RXD (BLT-RXD):** kein Pull-up. Die Leitung schwebt bei LTE, das erklärt die `0xFF`-Bytes vor dem Banner.
   Gegenmaßnahme: Pull-up (z. B. 100 kΩ) auf BLT-RXD.
3. **BLT-TXD und BLT-CTS:** ebenfalls ohne Pull-up/-down.
4. **Varianten:** "Abwertskompatibel ohne LTE" und "Abwärtskompatibel mit LTE". IF820, IC13, IC18, R124/R125/R126 haben
   kein Flag (immer bestückt). **R139** (0 Ω BT_DEV_WAKE <-> BLT-LINK) ist in beiden Varianten **nicht** bestückt,
   BT_DEV_WAKE (Pin 28) hängt offen. R77 (10 kΩ) nur in "ohne LTE", R78 (100 Ω) nur in "mit LTE"; beide am Netz N$133
   zu IC36 IO7. Welche Variante B und 26SC9966 haben und was am 2026-11-04 geändert wurde, steht nicht im Schaltplan.
5. **CONNECTION:** IF820 Pin 33 (P27, im Plan "CYSPP/SPP") geht **direkt an LPC P0.19**, ohne Bauteil dazwischen (die
   Umverdrahtung per Draht ist hier fest verdrahtet). Pin 12 (P13, Netz BLT-LINK) geht über R78 bzw. R77 an IC36 IO7.

Folge: Die Hardware-Vermutung (Pull-up am Modul-CTS) trifft im neuen Schaltplan zu. Offen bleibt, ob die alten Boards
gleich aufgebaut sind - das entscheidet die Messung.

## Gegenprobe am Board (falls der Schaltplan keine klare Antwort gibt)

Gerät online bringen (MQTT verbunden). Dann am IF820 den **CTS-Pin** mit dem Multimeter gegen GND messen, auf B und auf
26SC9966. Erwartung: B HIGH (ca. 3 V), 26SC9966 LOW. Zum Vergleich offline messen, dann treibt der LPC den Pin
(LOW = bereit).

## Was die Antwort entscheidet

- **CTS-Leitung erklärt den Unterschied:** Ein Pull-down am Modul-CTS (auf den nächsten Mustern bestücken) löst das
  Problem in Hardware. Der CONNECTION-Auslöser bleibt als Absicherung sinnvoll.
- **Erklärt ihn nicht:** Die Ursache liegt woanders. Der CONNECTION-Auslöser (Firmware 5.137, in Arbeit) ist dann der
  Weg, weil er den MUX auf BT schaltet, bevor der Handshake hängen bleibt.
