# Lichtsteuerung der LED-Anzeige (lightcontrol)

Stand: Firmware **5.s3** (Branch `DevelopLightSensor`, 2026-11-13). Quellen: `lightsens.c/.h`, `libtool.c`
(`get_dimmung()`), `hard.c` (`set_pwm()`, `adj_hell()`, `get_brightness()`), `i2cm.c` (`InitI2C_Devices()`).
Entstehung und Tests: `project-status.md`, Updates 5.s1 bis 5.s3.

## 1. Überblick

Die Anzeigehelligkeit folgt dem Umgebungslicht. Ein Lichtsensor misst die Helligkeit, die Firmware rechnet daraus
einen Dimmfaktor `dimm` (1…200) und stellt damit das PWM-Tastverhältnis der LED-Anzeige ein. Fällt der Sensor aus,
läuft die Anzeige mit einer festen Helligkeit und die Firmware legt einen Diagnose-Code ab.

Es gibt genau zwei Fälle:

| Fall | Anzeige |
|---|---|
| **Lichtsensor geht** | Regelung nach dem Sensor |
| **Fehlerfall** (Sensor antwortet nicht oder liefert kein Signal) | fest **30 %** (`LS_DIM_FEHLER`) |

## 2. Sensoren je Mainboard

| Mainboard | Sensor | Anschluss | Radar-Empfänger |
|---|---|---|---|
| **V1** | Fotodiode **SFH213** | analog über MUX IC39 an A/D-Kanal AD0.3 (`get_brightness()`, 0…1023, hoher Wert = dunkel) | immer bisheriger Transceiver |
| **V2** | Umgebungslichtsensor **Vishay VEML7700** (IC1) | I²C0 (P0.27 SDA / P0.28 SCL), feste 7-Bit-Adresse 0x10 (im Code `0x20`) | bisheriger Transceiver **oder** Via-2 |

Auf V2 ist keine SFH213 bestückt. Der Fotodioden-Eingang liest dort dauerhaft 1021…1023 (gemessen auf 26SC9983).
Firmware bis 5.137 kannte den VEML7700 nicht und hat auf V2 diesen Wert als "stockdunkel" gelesen: **V2-Boards
liefen damit dauerhaft mit ca. 1 % Helligkeit.**

Der TSL25721 (IC38) ist auf V2 vorgesehen, aber nicht bestückt und wird nicht unterstützt.

## 3. Erkennung des Mainboards (V1/V2)

Merkmal ist der Lichtsensor: Antwortet auf I²C-Adresse 0x10 ein VEML7700, ist es V2, sonst V1.

- **Speicherort:** `fp.radar` Bit 3:2 im Parameterblock (`BOARD_TYP`: 0 = unbekannt, `BOARD_V1`, `BOARD_V2`).
  Bit 1:0 ist der Radar-Empfänger (5.L1), Bit 7:4 sind frei. Das Layout des Parameterblocks ist unverändert.
- **Ablauf beim Hochfahren** (`board_detect()`, aus `InitI2C_Devices()` direkt nach der Radar-Erkennung), wie bei
  BT, LTE und GPS:
  1. Gespeichert V2 und der Sensor quittiert einen Schreibzugriff → fertig (ein I²C-Zugriff).
  2. Gespeichert V1 und auf 0x10 antwortet nichts → fertig.
  3. Sonst (unbekannt oder Abweichung) voller Test: Konfiguration schreiben und zurücklesen, bis zu 3 Versuche
     (`LS_DETECT_TRY`). Nur wenn genau der geschriebene Wert zurückkommt, gilt V2.
  4. Wechsel gegenüber einem bekannten Stand → **I17006** (Wert 1 = V1, 2 = V2).
  5. Sonderfall: gespeichert V2, aber der volle Test findet keinen Sensor → das Board bleibt V2 (der Stand liegt im
     Flash auf dem Mainboard selbst, ein Boardtausch nimmt ihn nicht mit), der Sensor gilt als defekt → **W17007**.
- Nach Deep-Sleep- oder Update-Reset läuft keine Erkennung, es gilt der gespeicherte Stand.
- Bestehende Geräte (Bit 3:2 = 0) machen beim ersten Start mit 5.s2 oder neuer einmal den vollen Test.

## 4. Funktion

### 4.1 Ablauf je Messzyklus

`get_dimmung()` wird einmal je Messzyklus aufgerufen (`runcycle`, measure.c):

```
ls = lightsens_dimm()            // V2: Sensorwert oder Fehlerfall; -1 = kein VEML7700 (V1)
wenn ls >= 0:  sensor = ls
sonst (V1):    adc = get_brightness()
               wenn Fotodiode liefert Signal: sensor = alte Fotodioden-Kennlinie(adc)
               sonst (Fehlerfall):            sensor = LS_DIM_FEHLER
dimm += sensor/4 - dimm/4         // exponentielle Glättung
```

Nach dem Einschalten startet `dimm` mit 50 % und nähert sich dem Messwert um je ein Viertel pro Messzyklus.

### 4.2 Messung VEML7700 (V2)

- Die Anzeige-LEDs leuchten durch eine Aussparung in der Frontscheibe auf den Sensor. Gemessen wird deshalb **nur
  bei dunkler Anzeige** (PWM6 abgewählt und Treiber-OE\ HIGH) und **nur bei jedem 8. Aufruf** (`LS_EVERY`).
- Messung: Sensor einschalten, 150 ms warten (`LS_WAIT`, Integrationszeit 100 ms), ALS-Register lesen, Sensor
  wieder abschalten. Die Messung blockiert, weil die CPU ohne Verkehr nach jedem Messzyklus in den Deep Sleep geht
  und per Watchdog-Reset aufwacht. Zustand (Messwert, Verstärkung, Zähler, Fehlermerker) liegt deshalb im
  `__noinit`-RAM und übersteht diesen Reset.
- Verstärkung automatisch x2 / x1 / x1/4 / x1/8. Rohwert ≥ 60000 → unempfindlicher und neu messen; < 1500 →
  beim nächsten Mal empfindlicher. Auflösung bei x2/100 ms: 0,0336 lx je Zählwert. Messbereich bis ca. 35.000 lx (x1/8, Rohwert 65535).
- Keine Linearitätskorrektur über 1000 lx (Anzeige ist dort ohnehin fast voll hell).

### 4.3 Kennlinie VEML7700 (logarithmisch)

| Umgebung | Dimmwert | Anzeige (Werkseinstellung) |
|---|---|---|
| ≤ 1 lx | 3 | 1,5 % |
| 10 lx | ~52 | 26 % |
| 100 lx | ~101 | 50 % |
| 1.000 lx | ~151 | 75 % |
| ≥ 10.000 lx | 200 | 100 % |

Gemessen am Tisch: dunkler Raum 1,5 lx → Dimm 12; Raum Nordseite Winter 300…390 lx → Dimm 125…130;
Handy-Licht 1130 lx → Dimm 152; Sensor mit Finger abgedeckt ca. 3 lx → Dimm 25…35.

### 4.4 Kennlinie Fotodiode (V1, unverändert seit vor 5.s1)

`sensor = (200000 + b² − b³/1515 − 525·b) / 1000` mit b = A/D-Wert 0…1023:

| A/D | 0 | 100 | 300 | 500 | 700 | 900 | 1000 | 1023 |
|---|---|---|---|---|---|---|---|---|
| Anzeige | 100 % | 78 % | 57 % | 52 % | 48 % | 28 % | 7 % | 1 % |

### 4.5 Vom Dimmwert zur Anzeige

```
Tastverhältnis Anzeige = fp.hell[Farbe] × Kundenfaktor × dimm / 200     (set_pwm(), adj_hell() in hard.c)
Tastverhältnis extern  = fp.pwmex × dimm × (1 + fp.hoff) / 200
```

PWM-Auflösung 200 Stufen à 0,5 %, begrenzt auf 0,5…99,5 %. Mit Werkseinstellung (hell = 100, hoff = 1) gilt
Dimmwert 200 = 100 %. Auch der Fehlerfall (30 %) ist ein Dimmwert und wird mit Farbabgleich und Kundenfaktor
verrechnet. Der LED-Spot (TCA6507) wird aus `dimm` mit 15 Stufen gedimmt.

## 5. Fehlererkennung

| Code | Typ | Bedingung | Wert | Folge |
|---|---|---|---|---|
| **I17006** | Info | Mainboard-Version beim Hochfahren anders als gespeichert | 1 = V1, 2 = V2 | neue Version übernommen |
| **W17007** | Warnung | V2: VEML7700 antwortet nicht | 1 = beim Hochfahren, 2 = im Betrieb nach 5 Messversuchen in Folge (`LS_ERR_MAX`, ca. 1 min) | Fehlerfall 30 % |
| **W17008** | Warnung | 48 Betriebsstunden ohne Signal (`LS_STUCK_H`) | 1 = V1, 2 = V2, 3 = Mainboard unbekannt | Fehlerfall 30 % |
| 17009 | – | veraltet (nur 5.s2: Ersatz nach Uhrzeit) | – | – |

"Kein Signal" heißt:
- **V1:** A/D-Wert dauerhaft ≤ 2 (`LS_ADC_SIG_LO`) oder ≥ 1020 (`LS_ADC_SIG_HI`), also am Anschlag. Typisch für
  fehlende oder abgesteckte Fotodiode.
- **V2:** Rohwert dauerhaft < 3 (`LS_RAW_SIG`, ca. 0,1 lx) oder Anschlag bei kleinster Verstärkung. Typisch für
  abgedeckten oder blinden Sensor.

Eigenschaften:
- Ein konstanter, aber plausibler Wert (z. B. Halle mit gleichmäßigem Kunstlicht) löst **keinen** Fehler aus.
- Fehlalarm nur bei dauerhaft völliger Dunkelheit über 48 h. Harmlos: dort wäre die Anzeige ohnehin gedimmt.
- Jede Meldung kommt einmal je Ereignis. Liefert der Sensor wieder gültige Werte, endet der Fehlerfall von selbst.
- Die 48-h-Frist beginnt nach jedem Kaltstart neu (Zeitbasis `fp.hcount`).
- Die Meldungen landen über `puterror()` auf dem Terminal und im Protokoll. Wert 0 wird dort verworfen, deshalb
  beginnen alle Werte bei 1.

Grenze: Ein V2-Board, dessen VEML7700 schon beim allerersten Start defekt ist, wird als V1 erkannt und fällt erst
nach 48 h über W17008 (Wert 1) auf.

## 6. Parameter

### 6.1 Im Parameterblock (Flash, je Gerät)

| Parameter | Bedeutung | Werkswert | Einstellung |
|---|---|---|---|
| `fp.hell[0..3]` | Helligkeitsabgleich je LED-Farbe (Grundfarbe, Warnfarbe, Symbol, Smiley), 1…100 % | 100 | Werksmenü (Strg+W) → 3 "LED-Helligkeit" |
| `fp.pwmex` | Maximales Tastverhältnis der externen PWM (Zusatzanzeige), 1…100 % | 100 | Werksmenü → 3, letzter Punkt "Externe PWM" |
| `fp.hoff` | Kundenfaktor "Anzeigeoptimierung": 0 = Betriebsdauer (50 %), 1 = Ausgeglichen/Normal (100 %), 2 = Sichtbarkeit (150 %, oben begrenzt) | 1 | Hauptmenü 4 "Einstellungen" → 5 "Anzeigeoptimierung" (Kunde) |
| `fp.pwm` | Dimm-/PWM-Modus (permanent / if detection / if LED lit) | 1 | Werksmenü → 1 "Konfiguration ändern" |
| `fp.ledspot` | LED-Spot (TCA6507) vorhanden, wird mit `dimm` gedimmt | 0 | Werksmenü → 1 |
| `fp.radar` Bit 3:2 | Mainboard V1/V2 | 0 (unbekannt) | **automatisch**, nicht einstellbar |

### 6.2 Fest in der Firmware (`lightsens.h`, nur per Neubau änderbar)

| Konstante | Wert | Bedeutung |
|---|---|---|
| `LS_LUX_MIN` / `LS_DIM_MIN` | 1 lx / 3 | unterer Endpunkt der Kennlinie |
| `LS_LUX_MAX` / `LS_DIM_MAX` | 10.000 lx / 200 | oberer Endpunkt der Kennlinie |
| `LS_DIM_FEHLER` | 60 (30 %) | Helligkeit im Fehlerfall |
| `LS_EVERY` | 8 | nur bei jedem 8. Messzyklus messen |
| `LS_WAIT` | 150 ms | Wartezeit je Messung |
| `LS_DETECT_TRY` | 3 | Versuche beim vollen Erkennungstest |
| `LS_ERR_MAX` | 5 | Messversuche ohne Antwort bis W17007 |
| `LS_STUCK_H` | 48 h | Betriebsstunden ohne Signal bis W17008 |
| `LS_RAW_SIG` | 3 | V2: kleinster Rohwert, der als Signal gilt |
| `LS_ADC_SIG_LO` / `LS_ADC_SIG_HI` | 2 / 1020 | V1: A/D-Grenzen für "Signal" |

## 7. Fertigung / EOL

**Einstellen (Werksmenü Strg+W):**
1. **3 "LED-Helligkeit":** je Farbe `fp.hell[]` mit `+`/`−` abgleichen, bis die Farben gleich hell wirken; die
   Anzeige zeigt dabei den eingestellten Wert. Danach die externe PWM (`fp.pwmex`). Der Abgleich läuft mit fester
   PWM, ohne Lichtsensor.
2. **1 "Konfiguration ändern":** `fp.pwm`, LED-Farbcode, Anzahl Farben, LED-Spot.
3. Kundenfaktor `fp.hoff` bleibt auf "Ausgeglichen" (1), wenn der Kunde nichts anderes wünscht.

**Nicht einzustellen:** Die Mainboard-Version erkennt die Firmware selbst beim ersten Hochfahren. Die Kennlinie und
der Fehlerwert sind feste Konstanten.

**Prüfen (Testmenü 3 → 6 "Lichtsensor", Ende mit Return):**
- Kopfzeile `Mainboard = V2` (bzw. `V1`) – muss zur Bestückung passen.
- `Regelung = Sensor` – nicht `Fehlerfall, feste Helligkeit`.
- Kein `VEML7700 DEFEKT (W17007)`.
- V2: Zeile `Lichtsensor = VEML7700`, dann je Messung eine Zeile mit Lux, Rohwert, Verstärkung, Dimmwert und
  A/D-Wert des Fotodioden-Eingangs. Im Raumlicht plausible Lux (ca. 100…1000 lx). **Sensor abdecken** → Lux fällt
  deutlich (< 10 lx), **anleuchten** → Lux steigt. Der A/D-Wert steht auf V2 bei ca. 1023.
- V1: A/D-Wert der Fotodiode; abdecken → Wert steigt Richtung 1023, anleuchten → Wert fällt.
- Protokoll: kein W17007 / W17008 nach dem Prüflauf.

Automatisiert prüft `Doku/tools/radar_v2_test.py` (Test F3) auf V2, dass der VEML7700 einen Lux-Wert > 0 liefert.

## 8. Offen

- Fehlerfall (W17007, W17008, 30 %) und V1-Erkennung sind am Gerät noch nicht getestet (V2-Board ohne Eingriff nicht
  auslösbar, kein V1-Board am Testplatz).
- Umschalten der Verstärkung bei > 2000 lx noch nicht am Gerät beobachtet (braucht Sonnenlicht oder helle Lampe).
- Kennlinie am Gerät im Außeneinsatz abstimmen, ggf. Endpunkte als Parameter.
- Testmenü und Messzyklus können gleichzeitig auf den Sensor zugreifen (unkritisch beobachtet).
