# Messprotokoll — CYSPP-Leitung diagnostizieren

> Zum Ausdrucken und Ausfüllen. Zwei Personen, ca. 45 min.
> Ziel: klären, ob das IF820 die CYSPP-Leitung beim **BLE**-Connect selbst LOW zieht —
> und ob beim aktiven LOW-Treiben durch den PCA9554 Bus-Contention auftritt.
> Angelegt: 2026-08-05 · Firmware 5.59 · Board Mainboard-V2-A1

Datum: ______________  Board-Serno: ______________  Beteiligte: ______________________

---

## 1. Messpunkte

Alle drei Punkte liegen auf 0603-Pads unmittelbar nebeneinander — kein Anlöten am
Modul-Feinpitch nötig.

| Kanal | Lötpunkt | Netz | Entspricht |
|---|---|---|---|
| **CH1** | **R77-Pad, obere Seite** (Pad 2) | `N$133` | IC36 Pad 12 (IO7) — Hostseite |
| **CH2** | **R78-Pad, Modulseite** (Pad 2) | `BLT-LINK` | IF820 Pin 12 (P13/CYSPP) — Modulseite |
| **GND** | **R77-Pad, untere Seite** (Pad 1) | `GND` | Massereferenz, direkt daneben |

**R77 ist unbestückt** — beide Pads sind frei und ideal als Messpunkt. Dazwischen liegt
R78 = 100 Ω, der bei Schritt 6 als Strom-Messwiderstand dient.

Vor dem Messen mit dem Durchgangsprüfer bestätigen:
- [ ] R77-Pad 1 hat Durchgang zu GND
- [ ] R77-Pad 2 hat Durchgang zu IC36 Pad 12
- [ ] R78-Pad 2 hat Durchgang zu IF820 Pin 12
- [ ] Zwischen R77-Pad 2 und R78-Pad 2 liegen ca. 100 Ω (= R78 ist bestückt)

---

## 2. Ausrüstung und Einstellungen

- Oszilloskop oder Logic-Analyzer, **2 Kanäle**, DC-Kopplung
- Zwei dünne Fädeldrähte, an CH1/CH2 angelötet (nicht mit der Hand halten — Wackelkontakt
  verfälscht die Messung)
- Logikschwelle 3,3 V · Zeitbasis ≥ 100 ms/div · beide Kanäle **gleiche Skalierung**
- Single-Shot-Trigger auf fallende CH2-Flanke, damit ein kurzer Puls nicht verloren geht
- Handy mit App (Classic-Verbindung **und** BLE-Probe mit Subscribe auf `…ca102`)
- RS232-Terminal am Gerätemenü

### Sicherheitsregel — nicht verhandelbar

**Nur EINE Versorgungsquelle.** Entweder Board-Versorgung **oder** DVK-USB, nie beides.
Beim Umstecken das Modul stromlos machen. Verstöße gegen diese Regel haben bereits zwei
Module und ein DVK zerstört (Back-Powering über die IO-Dioden).

---

## 3. Messablauf

### Schritt 1 — Ruhepegel

Gerät booten, **nichts** verbinden, 30 s warten.

| | CH1 (Hostseite) | CH2 (Modulseite) |
|---|---|---|
| Spannung | _____3.438_____ V | ___3.441_______ V |

Erwartung nach aktueller Theorie: **beide ≈ 3,3 V** (interner ~100-kΩ-Pull-up des PCA9554).

### Schritt 2 — Referenz: Classic SPP

Per Classic Bluetooth verbinden (das funktioniert bekanntermaßen).

| | CH1 | CH2 |
|---|---|---|
| Spannung | __________ V | __________ V |

Erwartung: **beide ≈ 0 V** — das Modul zieht die Leitung selbst LOW. Damit ist belegt, dass
das Modul den Pin überhaupt treiben *kann*. Diese Messung ist die Referenz für Schritt 4.

### Schritt 3 — Rückkehr in den Ruhezustand

Classic trennen. Wie lange bis der Ruhepegel aus Schritt 1 wieder erreicht ist?

Zeit: __________ ms   ·   CH1: __________ V   ·   CH2: __________ V

### Schritt 4 — Die entscheidende Messung: BLE

Single-Shot-Trigger armieren. Dann per BLE verbinden und in der App-Probe **Subscribe auf
`…ca102`** auslösen.

| | CH1 | CH2 |
|---|---|---|
| Vor dem Subscribe | __________ V | __________ V |
| Während/nach Subscribe | __________ V | __________ V |
| Falls ein Puls: Dauer | | __________ µs/ms |

Ankreuzen, was zutrifft:

- [ ] **CH2 bleibt HIGH** — Modul treibt bei BLE nicht LOW
- [ ] **CH2 geht kurz LOW, dann zurück auf HIGH** — Modul zieht, verliert
- [ ] **CH2 geht LOW und bleibt LOW** — Pin ist in Ordnung
- [ ] anderes: ______________________________________________

### Schritt 5 — Reproduzierbarkeit

Schritt 4 zweimal wiederholen.

Durchlauf 2: ____________________  ·  Durchlauf 3: ____________________

Gleiches Ergebnis? ☐ ja  ☐ nein (erratisch → Verdacht auf Brownout/Float, siehe
`HW_Massnahmen_Bewertung_2026-08.md` Maßnahme 3)

### Schritt 6 — Contention-Test *(nur mit vorbereiteter Firmware)*

Voraussetzung: ein Menüpunkt, der IC36 IO7 als Output/LOW setzt. **Vorher Bescheid sagen,
dann baue ich den ein.** Ohne diesen Menüpunkt Schritt 6 überspringen.

Messung **zwischen** CH1 und CH2 (Differenz über R78):

ΔU = __________ mV  →  I = ΔU / 100 Ω = __________ mA

**Messung kurz halten (< 1 s), dann sofort zurücknehmen.**

- [ ] ΔU < 50 mV (< 0,5 mA) → keine Contention, LOW-Treiben ist elektrisch erlaubt
- [ ] ΔU 50 mV … 1 V → Grenzfall, Ergebnis mit mir durchsprechen
- [ ] ΔU > 1 V (> 10 mA) → **sofort abbrechen**, Modul treibt dagegen

---

## 4. Auswertung

| Ergebnis Schritt 4 | Bedeutung | Konsequenz |
|---|---|---|
| CH2 bleibt HIGH | Modul treibt bei BLE nicht LOW; der Pegel kommt nur vom PCA9554-Pull-up | Theorie bestätigt. Verbindungserkennung muss vom Pin weg → **P27 → IO5** verdrahten, danach IO7 als reinen Steuerausgang nutzen |
| CH2 kurz LOW, dann HIGH | Modul zieht schwach und verliert gegen die 100 kΩ | Ein definierter Ruhepegel genügt evtl. → **R77 bestücken**. Erkennung bleibt trotzdem betroffen, weil Ruhe- und Verbundzustand danach beide LOW sind |
| CH2 LOW und bleibt LOW | Der Pin ist nicht die Ursache | Theorie fällt. BLE-Abbruch hat eine andere Ursache — dann Richtung Supervision-Timeout, MTU oder Modul-Firmware 1.4.12.12 weitersuchen |
| erratisch über die Durchläufe | Pegel ist undefiniert oder Versorgung bricht ein | Erst Entkopplung nachrüsten (C128/C58 fehlen!), dann neu messen |

| Ergebnis Schritt 6 | Konsequenz |
|---|---|
| ΔU < 50 mV | Weg **B** ist frei: P27 verdrahten + IO7 aktiv LOW treiben. Kein R77 nötig |
| ΔU > 1 V | Weg **C**: P27 verdrahten + R77 bestücken. IO7 **nicht** aktiv LOW treiben |

---

## 5. Nebenbefunde, die sich gleich mitprüfen lassen

Kostet 10 Minuten, klärt aber die Punkte aus `HW_Massnahmen_Bewertung_2026-08.md`.
Der Schaltplan sagt „nicht bestückt" — Eagle-Variantenflags sind aber nicht immer gepflegt,
deshalb visuell nachsehen:

| Bauteil | Soll laut Schaltplan | Tatsächlich bestückt? | Relevanz |
|---|---|---|---|
| **R77** (10 kΩ) | nicht bestückt | ☐ ja ☐ nein | CYSPP-Ruhepegel |
| **C128** (1 µF) | nicht bestückt | ☐ ja ☐ nein | Entkopplung Modul — Brownout |
| **C58** (1 µF) | nicht bestückt | ☐ ja ☐ nein | Entkopplung Modul — Brownout |
| **Q3** (32,768 kHz) | nicht bestückt | ☐ ja ☐ nein | Sleep-Clock-Genauigkeit |
| **R124** (10 kΩ an RST) | nicht bestückt | ☐ ja ☐ nein | Reset-Pegel beim Flashen |
| R78 (100 Ω) | bestückt | ☐ ja ☐ nein | Serienwiderstand CYSPP |
| C77 (10 nF) | bestückt | ☐ ja ☐ nein | Filter auf CYSPP |

Zusatzmessung, falls ein Oszilloskop-Kanal frei ist: **Versorgung des Moduls** (Netz `N$169`,
IF820 Pin 2 / Pin 22) während einer Classic-Übertragung mitschreiben. Bricht die Spannung
unter Sendelast ein, ist Maßnahme 3 (Entkopplung) bestätigt.

Einbruch beobachtet? ☐ nein  ☐ ja, auf __________ V

---

## 6. Notizen

```
```
