# Via-2: Frequenzkanal, Abgleich, Nachführung und RE/-Zeit (ab 5.142)

Grundlage: Mail des HW-Kollegen vom 2026-11-13 ("Unabdingbar": Transceiver-Abgleich auf die Centerfrequenz mit
Frequenzmessung, Frequenznachführung 2 min nach dem Einschalten und dann alle 15 min, RE/-Zeit 15 ms, Messzeit
prüfen). Gilt nur für Geräte mit Via-2-Radarempfänger (`IS_VIA2`). Code: `hftrack.c/.h`.

## Frequenzkanäle

Benachbarte Geräte senden auf verschiedenen Frequenzen. Empfängt ein Gerät das Signal eines anderen, liegt die
Schwebung (Differenz der Sendefrequenzen) dann weit über dem Doppler-Messbereich (ca. 45 Hz pro km/h, bis ca. 11 kHz)
und wird weggefiltert.

| Kanal (Menü) | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Frequenz MHz | 24155 | 24160 | 24165 | 24170 | 24175 | 24180 | 24185 | 24190 | 24195 | 24200 |

- Abstand 5 MHz (größer als Drift plus Messstreuung von etwa ±2 MHz).
- Parameter `fp.hfkanal`: 0 = **automatisch** aus der letzten Ziffer der Seriennummer (Ziffer d -> Kanal d+1, z. B.
  26SC9983 -> Kanal 4 = 24170 MHz). Nacheinander gefertigte Geräte bekommen so verschiedene Kanäle. 1...10 = Kanal fest,
  z. B. für zwei Geräte am selben Mast.
- `fp.hfkanal` und `fp.re_ms` liegen in den früheren Füllbytes `fill2` (Layout bleibt 2048 Byte), also im Bereich, den
  ein Parameter-Upload überschreibt. Ein Upload mit Nullen bedeutet "automatisch".

## Messung

`mess_hf()` misst 8 Perioden der durch 2^23 geteilten Sendefrequenz. Einzelmessungen streuen bis 2-3 MHz nach unten.
Ein Messpunkt (`hf_measure_khz()`) schaltet den Transceiver wie im Betrieb kurz ein, verwirft eine Leermessung und nimmt
den Median aus 7 Messungen (Auflösung 0,1 MHz). Nach einem Potisprung schwingt die Frequenz langsam ein: Es wird
gemessen, bis zwei Messpunkte höchstens 0,3 MHz auseinanderliegen (50 ms Pause, höchstens 8 Messpunkte).

## Kennlinie IC32 (Board B 26SC9983, 2026-11-13)

Steigend, stark nichtlinear: Stufe 0 = 24157, 64 = 24162, 128 = 24171, 192 = 24191, 224 = 24214, 256 = 24270 MHz.
Unten ca. 0,1 MHz pro Stufe, oben bis 2,5 MHz pro Stufe. Ab etwa Stufe 205 liegt die Frequenz über 24200 MHz.

## Strg+T (ab 5.144)

Strg+T im Hauptmenue misst ueber denselben Weg (Median nach DPWR) und zeigt ganze MHz. Bis 5.143 mass es 2 ms nach dem
Einschalten und lag dadurch ca. 3,5 MHz hoeher als die Frequenz im Messbetrieb.

## Werksabgleich (Werksmenü Strg+W im Testmenü -> 4 -> 5 "Adjust To Channel Frequency")

Bisektion über den Wiper von IC32 (Richtung aus Stufe 0 und 128, Stufe 256 wird nicht angefahren), danach Kontrolle.
Das Ergebnis wird non-volatile in IC32 geschrieben, `fp.TxF` auf die gemessene Frequenz gesetzt und als I17011 (Wert =
MHz) protokolliert. Liegt das Ergebnis außerhalb 24150...24200 MHz, wird nichts gespeichert. Dauer ca. 10-15 s.
Auf Board B: Kanal 4, Stufe 126, 24169,9 MHz.

## Nachführung

- Geregelt wird direkt auf die gemessene Frequenz, eine Temperatur gibt es auf dem Board nicht.
- Erste Nachführung 2 min nach dem Einschalten, danach alle 15 min, nur im Messzyklus (Messaufgabe oder Online-Messen),
  vor einer Messung. Fälligkeit nach der RTC, Zustand im `__noinit`-RAM (übersteht den Deep-Sleep-Reset).
- Totband ±1,5 MHz. Außerhalb: Wiper um -Abweichung/Steigung Stufen verstellen, höchstens 16 Stufen je Schritt, bis
  3 Schritte je Durchgang. Die Steigung wird mit einem Probeschritt von 8 Stufen gelernt und laufend nachgeführt.
- Die Frequenz verlässt dabei nie das Band 24150...24200 MHz (Schritt wird sonst zurückgenommen).
- Endstellung non-volatile in IC32 (übersteht Stromausfall; bei 15 min ca. 35000 Schreibvorgänge pro Jahr).
- Die gemessene Frequenz geht in die Geschwindigkeitsberechnung ein (`IFConst`).
- Ein Durchgang dauert 0,5-3 s, in diesem Messzyklus fällt die Fahrzeugmessung aus.
- Fehler: W17010 einmal je Einschaltvorgang und Grund (1 Messung, 2 Kennlinie flach, 3 Anschlag/Band, 4 Ziel nicht
  erreicht).
- Von Hand: Menüpunkt 8 "Track Frequency Now" (mit Ausgabe).

Geprüft auf Board B: Kanal 5 fest (+5,4 MHz Abweichung) -> Probeschritt 8 Stufen, Korrektur 12 Stufen, -0,3 MHz.
Automatisch: nach Stromzyklus +3,9 MHz (Stufe 146), nach 167 s Online-Messen Stufe 122, -1,5 MHz.

## RE/-Zeit und Messzeit

- RE/ (IC37 P0.7 = `AMP_BW`): beim Einschalten zuerst minimale Verstärkung, nach der RE/-Zeit volle Verstärkung.
  **Bisher waren es 30 ms** (DPWR_VIA2 15 + t_amp_en 15) bzw. 40 ms bei kleinster Messgeschwindigkeit < 5 km/h, jetzt
  die RE/-Zeit ab dem Einschalten: Standard **15 ms**, bei < 5 km/h **25 ms**.
- Einstellbar im Via-2-Transceivermenü, Punkt 7 "RE/ Time" (`fp.re_ms`, 5...100 ms, 0 = automatisch).
- Die maximale Messzeit (`t_mess_max`) und die dynamische Messzeitüberwachung in `measure()` rechnen jetzt mit derselben
  Einschaltzeit (`t_on_delay()`). Das Transceivermenü zeigt die max. Messzeit an (Board B: 131 ms bei 15 ms RE/-Zeit;
  der Wert hängt von der kleinsten Messgeschwindigkeit `fp.vmm` und der Sendefrequenz ab).
- Annahme: RE/ aus der Mail (Expander "IC29 P0_7") ist im Code IC37 Port 0 Bit 7 (`AMP_BW`). Der Kollege sollte die
  Zeit am Oszilloskop prüfen.
- Bisheriger Transceiver: `t_amp_en` jetzt immer 20 ms (bisher nach Neustart 20, nach einer Menüänderung bis zum
  nächsten Neustart 10 ms).

## Progressive Schritte (Menüpunkt 3 "Set Module Frequency")

Taste + oder - gehalten (Wiederholung < 400 ms): Schritte 1, 2, 4, 8, 16 Stufen.

## Offen

- RE/-Zeit und Messzeitverlängerung am Gerät mit Fahrzeugen bzw. Oszilloskop prüfen (HW-Kollege).
- Kennlinie und Einschwingzeit an weiteren Via-2-Modulen prüfen.
- Optionale Punkte aus der Mail (EN/-RS232, CS-A-Funk auf P1[15], RS232-2) noch nicht umgesetzt.
