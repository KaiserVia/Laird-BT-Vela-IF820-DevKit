# Anleitung — `ble_cyspp_probe.py`

> BLE-CYSPP-Diagnose vom Windows-PC aus. Kein iOS-Gerät nötig, keine Zusatzhardware.
> Gehört zu `Doku/Messprotokoll_CYSPP_Diagnose.md`, Schritte 4 und 5.

---

## 1. Einmalig einrichten (5 Minuten)

Voraussetzung: Python 3.9 oder neuer, BT-Adapter mit **Bluetooth 4.0+** (jeder PC ab ca. 2014).

```
py -m pip install bleak
```

Prüfen, ob der Adapter gefunden wird und das Gerät sendet:

```
py ble_cyspp_probe.py --scan-only
```

Erwartete Ausgabe:

```
  AA:BB:CC:DD:EE:FF   VIASIS_26SC9996_BLE
```

Wird nichts gefunden: BT am PC einschalten, Advertising am Gerät prüfen, ggf. Windows-
Bluetooth-Einstellungen einmal öffnen (Windows startet den BLE-Stack teilweise erst dann).

---

## 2. Die eigentliche Messung

**Vorher:** Oszi/Logic-Analyzer auf Single-Shot armieren, Trigger auf fallende CH2-Flanke.
Der Kollege behält den Bildschirm im Auge, du startest das Skript.

```
py ble_cyspp_probe.py --runs 3 --csv ergebnis.csv
```

Das Skript macht pro Durchlauf: scannen → verbinden → GATT-Tabelle einlesen →
**Subscribe auf `…ca102`** → 8 s lauschen → Ergebnis protokollieren → 6 s Pause.

Die entscheidende Zeile im Log ist:

```
>>> SUBSCRIBE (CCCD-Write) — jetzt Oszi beobachten <<<
```

Genau in diesem Moment liest das Modul den CYSPP-Pin. Alles danach ist die Antwort auf
unsere Frage.

### Nützliche Varianten

| Aufruf | Wofür |
|---|---|
| `py ble_cyspp_probe.py` | Standard, 3 Durchläufe |
| `--runs 1` | ein einzelner Durchlauf, z. B. zum Einrichten des Triggers |
| `--name VIASIS_26SC9996` | wenn mehrere Boards in Reichweite sind |
| `--address AA:BB:...` | Scan überspringen, geht schneller |
| `--probe-write` | nach dem Subscribe zusätzlich `H` und `V` senden |
| `--csv ergebnis.csv` | Ergebnisse für die Doku mitschreiben |
| `-v` | beim Scan alle gefundenen Geräte auflisten |

---

## 3. Log lesen

Jede Zeile hat Wanduhrzeit **und** Millisekunden seit Programmstart — die zweite Zahl
ist die, die zur Oszi-Zeitachse passt.

**Fehlerfall (das erwartete Verhalten heute):**

```
16:42:03.118  [   4218.7 ms]  >>> SUBSCRIBE (CCCD-Write) — jetzt Oszi beobachten <<<
16:42:03.145  [   4245.9 ms]  Subscribe mit ATT-Success quittiert nach 27.2 ms.
16:42:03.151  [   4251.4 ms]  *** DISCONNECT — 33.1 ms nach dem Subscribe ***
```

**Erfolgsfall (so sieht es aus, wenn der Fix greift):**

```
16:42:03.118  [   4218.7 ms]  >>> SUBSCRIBE (CCCD-Write) — jetzt Oszi beobachten <<<
16:42:03.145  [   4245.9 ms]  Subscribe mit ATT-Success quittiert nach 27.2 ms.
16:42:03.512  [   4612.3 ms]  <== EMPFANGEN  len= 23  hex=[0D 0A ...]  ascii="..Bitte PIN eingeben: "
```

Ein **unaufgefordert** empfangener PIN-Prompt ist der Beweis, dass der Datenkanal steht —
so hat es die App-Seite in `ble-retest-1-negative.md` Abschnitt 5 als Kriterium definiert.

### Die Disconnect-Zeit ist die eigentliche Information

| Zeit nach Subscribe | Deutung |
|---|---|
| **< 150 ms** | Passt zum CYSPP-Pin-Pegel: das Modul liest HIGH und beendet den Kanal sofort. Das Skript schreibt dann „Abbruch SOFORT (Pin-Verdacht)" |
| **einige 100 ms bis Sekunden** | Eher Supervision-Timeout, RF-Problem oder Versorgungseinbruch — nicht der Pin |
| **streut über die Durchläufe** | Undefinierter Pegel oder Brownout. Dann sind die fehlenden Entkopplungskondensatoren C128/C58 der nächste Verdacht |

Genau diese Zahl konnte die App-Seite bisher nicht liefern — sie war Punkt 3 unserer
Rückfragen in `ble-retest-1-antwort.md`.

---

## 4. Zusammenspiel mit dem Oszilloskop

Beides zusammen ergibt die vollständige Antwort:

| Skript sagt | CH2 (Modulseite) zeigt | Schluss |
|---|---|---|
| Abbruch < 150 ms | bleibt HIGH | Modul treibt bei BLE nicht LOW → **P27 → IO5 verdrahten** |
| Abbruch < 150 ms | kurzer LOW-Puls, dann HIGH | Modul zieht schwach und verliert → **R77 bestücken** |
| Abbruch < 150 ms | geht LOW und bleibt LOW | Pin ist in Ordnung → Pin-Theorie fällt, anders weitersuchen |
| PIN-Prompt kommt | LOW | Datenkanal steht — fertig |

Die Zeile mit `>>> SUBSCRIBE` markiert den Moment, den man auf der Oszi-Kurve sucht.

---

## 5. Wenn es klemmt

**„Fehlt: bleak"** → `py -m pip install bleak`

**Kein Gerät gefunden** → mit `--scan-only -v` alle Geräte auflisten. Taucht das Board gar
nicht auf, ist es ein Advertising- oder Adapterproblem, kein CYSPP-Problem.

**Mehrere passende Geräte** → das Skript bricht bewusst ab und listet die Adressen.
Mit `--address` das richtige wählen.

**Connect schlägt fehl statt Subscribe** → das ist ein anderes Symptom als das der App.
Board einmal komplett stromlos machen (ein Reflash des LPC setzt das IF820 **nicht** zurück)
und erneut versuchen.

**Windows verbindet, trennt aber schon vor dem Subscribe** → falls das Gerät in den
Windows-Bluetooth-Einstellungen als gekoppeltes Gerät auftaucht, dort entfernen.
Classic-Pairing und BLE sind getrennte Stacks; ein alter Classic-Eintrag kann den BLE-
Verbindungsaufbau stören.

**Skript hängt beim Scannen** → Windows-Bluetooth aus- und einschalten. Der WinRT-Stack
bleibt gelegentlich stehen.

---

## 6. Ergebnisse sichern

Die CSV aus `--csv` und das Konsolen-Log gehören zusammen mit den Oszi-Screenshots in
`Doku/Messprotokoll_CYSPP_Diagnose.md` (Abschnitt 6, Notizen). Anschließend als eigenes
`## Update`-Kapitel in `project-status.md` festhalten — Symptom / Messwert / Schluss.
