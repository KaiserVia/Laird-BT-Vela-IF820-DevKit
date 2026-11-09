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

## Gegenprobe am Board (falls der Schaltplan keine klare Antwort gibt)

Gerät online bringen (MQTT verbunden). Dann am IF820 den **CTS-Pin** mit dem Multimeter gegen GND messen, auf B und auf
26SC9966. Erwartung: B HIGH (ca. 3 V), 26SC9966 LOW. Zum Vergleich offline messen, dann treibt der LPC den Pin
(LOW = bereit).

## Was die Antwort entscheidet

- **CTS-Leitung erklärt den Unterschied:** Ein Pull-down am Modul-CTS (auf den nächsten Mustern bestücken) löst das
  Problem in Hardware. Der CONNECTION-Auslöser bleibt als Absicherung sinnvoll.
- **Erklärt ihn nicht:** Die Ursache liegt woanders. Der CONNECTION-Auslöser (Firmware 5.137, in Arbeit) ist dann der
  Weg, weil er den MUX auf BT schaltet, bevor der Handshake hängen bleibt.
