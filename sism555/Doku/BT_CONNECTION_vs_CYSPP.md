# Bluetooth-Statussignale am IF820: "Link steht" vs. "Daten fließen"

Erklärung für Kollegen. Stand 2026-10-14, Firmware 5.116, Messungen auf Board 99SC9923
(Testplan `Testplan_CONNECTION_P0-19.md`, T0-T10 bestanden).

## Kurzfassung

Das IF820-Modul meldet dem Mikrocontroller über zwei getrennte Pins, in welchem Zustand die
Bluetooth-Verbindung ist:

| Signal | Bedeutung | Aussage |
|---|---|---|
| `CONNECTION` | Link steht | Ein Gerät ist per Funk verbunden. Es gibt noch nicht zwingend einen Datenkanal. |
| `CYSPP` | Daten fließen | Die transparente Brücke Funk <-> UART ist aktiv. Bytes kommen durch. |

Beide Signale sind **low-aktiv** (LOW = aktiv). Unsere Firmware startet Banner und PIN-Abfrage erst,
wenn `CYSPP` aktiv ist, nicht schon bei `CONNECTION`.

## Die beiden Begriffe

**Link steht.** Handy oder PC und das IF820 sind per Funk verbunden, und das Modul weiß davon. Mehr
nicht. Es gibt noch keinen Kanal, über den der Mikrocontroller Bytes senden oder empfangen kann.

**Daten fließen.** Zusätzlich ist die Brücke zwischen Funk und UART aktiv. Alles, was der PC schreibt,
kommt als Rohbytes am UART an, und alles, was die Firmware auf den UART schreibt, geht per Funk raus.

Vergleich: Ein Telefonat. "Link steht" heißt, die Leitung ist aufgebaut und es klingelt nicht mehr.
"Daten fließen" heißt, beide sprechen tatsächlich miteinander.

## Verhalten bei Classic (SPP) und BLE

**Bluetooth Classic (SPP).** Der Verbindungsaufbau enthält sofort den seriellen Kanal. Beide Pins
gehen im selben Moment auf LOW.

**BLE.** Der Aufbau hat zwei Schritte:

1. Der PC verbindet sich mit dem Modul. Der Link steht, `CONNECTION` geht auf LOW. Einen Datenkanal
   gibt es noch nicht.
2. Der PC schaltet die Benachrichtigungen der Daten-Characteristic ein (CCCD-Write, "Subscribe").
   Erst dann aktiviert das Modul die Brücke, und `CYSPP` geht auf LOW.

Zwischen beiden Schritten lagen bei Windows etwa 3 s. In dieser Zeit ist `CONNECTION` LOW und
`CYSPP` noch HIGH.

**Trennen.** Bei beiden Transporten gehen beide Pins gleichzeitig auf HIGH.

```
BLE:     Link-Aufbau      Subscribe                 Trennen
CONNECTION  HIGH ---------\_______________________________/--- HIGH
CYSPP       HIGH ----------------\_______________________/--- HIGH
                           |<- ca. 3 s ->|
                           Link steht,   Daten fließen
                           keine Daten
```

## Warum das für die Firmware wichtig ist

`BT_LINK` löst Banner und PIN-Abfrage aus. Das darf erst passieren, wenn Daten wirklich ankommen
können. Würden wir schon bei "Link steht" starten, ginge das Banner bei BLE ins Leere, weil der PC
noch nicht subscribed hat. Deshalb hängt `BT_LINK` an `CYSPP`.

Die Idee, `BT_LINK` aus `CONNECTION` abzuleiten, haben wir getestet und verworfen (Stufe B,
Testplan T7/T8).

## Zusatzbefunde aus dem Test

- **Funkabriss bei Classic.** Reißt die Funkverbindung ab, ohne dass ein Trennen gemeldet wird,
  bleiben beide Pins etwa 20 s LOW (Link-Supervision-Timeout, `SBTP T=7D00` x 0,625 ms) und gehen dann
  gemeinsam auf HIGH. Schließt Windows das Terminal oder schaltet Bluetooth aus, trennt es sauber.
- **Kein Split-Brain.** Der früher vermutete Fall "Modul hält `CYSPP` LOW, obwohl der Link tot ist"
  ist mit dem direkten `CONNECTION`-Pin nicht aufgetreten. Beide Pins waren in allen Classic-Fällen
  identisch.
- **Zyklentest.** 10 von 10 Classic-Verbindungen sauber (Connect-Zeit 50 / 1979 / 16763 ms
  min/avg/max, das Maximum ist der erste Aufbau mit Geräte-Suche). Am RS232 keine Fehlzeichen.

## Hardware-Verdrahtung

| Signal | Anschluss | Anmerkung |
|---|---|---|
| `CYSPP` | IC54 (PCA9554, I2C) Bit 7 | Die Firmware treibt den Pin nur beim bewussten Trennen aktiv, sonst bleibt er hochohmig. |
| `CONNECTION` | direkt an LPC-Pin P0.19 (nur auf umgebauten Boards) | Interner Pull-up, kein Interrupt. Das Pad `IO5` an IC54 ist dafür vom GND getrennt. |

Boards ohne diesen Umbau lesen an P0.19 durch den Pull-up dauerhaft HIGH. Seit 5.120 steht der
Schalter `BT_CONN_GPIO` in `btio.h` generell auf 0, Boards ohne Umbau brauchen keine eigene Einstellung.

## Empfehlung

`CONNECTION` ist als Diagnosesignal nützlich, weil man Link und Datenpipe getrennt sieht. Für den
Serienbetrieb bringt es keinen nachgewiesenen Vorteil und erfordert einen Hardware-Umbau. Die
Firmware stützt sich für `BT_LINK` deshalb weiter allein auf `CYSPP`.

Nachtrag 2026-11-11 (Stand 5.137): Der Gegentest mit `BT_CONN_GPIO 0` ist bestanden (5.124, keine
Stray-Zeichen in 30 Zyklen). Seit 5.137 nutzt die Firmware `CONNECTION` zusätzlich als Auslöser: LOW
während einer LTE-Sitzung beendet MQTT geordnet (`BT_CONN_TRIGGER`, `bt_conn_trigger()`; nur mit
P0.19-Draht wirksam). Die Messwerte oben stammen weiter von 5.116.
