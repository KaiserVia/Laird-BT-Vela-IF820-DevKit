# Diagnose-Codes – Konzept: lesbar, stabil, 10-Jahre-tauglich

Ersetzt das bisherige zeilenbasierte DTC-Schema (Datei-Basis + `__LINE__`).
Grund: zeilenbasiert ist automatisch eindeutig, aber **kryptisch** (man braucht den
passenden Quellstand) und **instabil** (die Nummer verschiebt sich bei Code-Änderungen).

## Ziele

- **Lesbar:** Code → **eine** Tabelle → Bedeutung. Kein Quelltext nötig.
- **Stabil:** Ein Code ändert seine Bedeutung **nie** (append-only), auch über Firmware-Versionen.
- **Einfach erweiterbar:** neue Diagnose = nächste freie Nummer + **eine** Zeile im Katalog.
- **10+ Jahre:** Reserve je Subsystem, Versions-Spalte, offenes Dateiformat.

## Code-Schema: 5-stellig, Subsystem + laufende Nummer

Jeder Code ist **immer 5-stellig**: die **ersten zwei Ziffern = Subsystem (11–99)**,
die **letzten drei = laufende Nummer (001–999)** innerhalb des Subsystems.
Innerhalb eines Subsystems wird einfach **hochgezählt** (11001, 11002, 11003 …).
**Einmal vergeben = eingefroren:** nie ändern, nie wiederverwenden. Veraltete Codes
bekommen nur den Status „veraltet", werden aber nicht gelöscht.

**Start bei 11** (nicht 00–10): so ist jeder Code garantiert eine saubere 5-stellige Zahl
**ohne führende Null** – keine Verwechslung (01xxx vs. 1xxx), überall gleich breit. Der
Bereich **00000–10999 bleibt frei/reserviert** (allgemeine bzw. zukünftige Zwecke).

| Bereich | Subsystem | (heutige Quelle) |
|---------|-----------|------------------|
| 00000–10999 | **frei / reserviert** | – |
| 11000–11999 | Bluetooth | btio.c |
| 12000–12999 | GSM / LTE | gsmio.c |
| 13000–13999 | Menü / Kommunikation | sicom.c |
| 14000–14999 | MQTT / Server | mqtt.c |
| 15000–15999 | GPS | gpsio.c |
| 16000–16999 | Flash / Speicher | flash.c |
| 17000–17999 | System / Start | main.c |
| 18000–18999 | Werkzeuge | libtool.c |
| 19000–19999 | USB | USB_tools.c |
| 20000–20999 | Selbsttest | sictst.c |
| 21000–99999 | **Reserve** für neue Subsysteme | |

Damit sind **bis zu 89 Subsysteme** (11–99) mit je 1000 Codes möglich – über die
Lebensdauer mehr als genug, und das System kann nicht „volllaufen".

## Typ-Präfix (F / W / I)

Vor die Nummer kommt ein Buchstabe für den **Typ** (englische Konvention):

| Präfix | Bedeutung | Beispiel |
|--------|-----------|----------|
| **F** | Fault (Fehler) | `F11001` |
| **W** | Warning (Warnung) | `W12003` |
| **I** | Info (Ereignis, z. B. Messbeginn/Reset) | `I0033` |

Der Buchstabe sagt den **Typ/Schweregrad**, die ersten zwei Ziffern das **Subsystem**
(11 = Bluetooth …), der Rest die laufende Nummer. Beispiel: `F11001` = Fault, Bluetooth, Code 001.
Anzeige im Protokoll: `F11001 Bluetooth – PING keine Antwort`.

Im Katalog (`Diagnose-Codes.csv`) bekommt jede Zeile eine Spalte **Typ** (F/W/I).

## Katalog – Dateiformat: **CSV** (Empfehlung)

`Diagnose-Codes.csv` ist die **eine** maßgebliche Quelle für alle Codes.

Warum CSV:

- Öffnet in **Excel / LibreOffice** (Service, Reparaturteam) **und** in jedem Texteditor.
- **Versionsverwaltungs-freundlich** (reine Textdatei, sauber diffbar – man sieht, welcher
  Code wann dazukam und von wem).
- **Maschinenlesbar:** ein kleines Skript kann daraus den C-Header für die Firmware und
  ein druckbares **PDF** für die Werkstatt erzeugen.
- In 10 Jahren garantiert noch lesbar (kein proprietäres Binärformat).
- Trennzeichen **Semikolon** (deutsches Excel öffnet es direkt), Encoding **UTF-8**.

Spalten:

```
Code; Subsystem; Kurztext; Ursache; Abhilfe; SeitVersion; Status
```

- **Code** – die Nummer (z. B. 1001).
- **Subsystem** – Klartext (Bluetooth, GSM/LTE …).
- **Kurztext** – knappe Meldung wie am Gerät angezeigt.
- **Ursache** – was dahintersteckt (für Service).
- **Abhilfe** – konkrete Maßnahme (für Reparaturteam).
- **SeitVersion** – ab welcher Firmware der Code existiert.
- **Status** – `aktiv` oder `veraltet`.

Aus dem CSV ableitbar (optional, per Skript): **PDF** zum Ausdrucken für die Werkstatt,
**JSON** fürs Backend/Dashboard.

## Eine Quelle der Wahrheit (kein Auseinanderlaufen)

- **Die CSV ist die Quelle.** Ein kleines Generator-Skript erzeugt daraus
  `dtc_codes.h` (Firmware-Konstanten) – so können Code und Beschreibung nicht driften.
- Alternativ ohne Skript: `dtc_codes.h` und CSV von Hand parallel pflegen (einfacher
  aufzusetzen, aber kleines Drift-Risiko). Empfehlung: Generator.

## Firmware

- Neue Datei `dtc_codes.h` mit benannten Konstanten, z. B.:
  `#define DTC_BT_PING 11001`
- Fehlerstelle nutzt die Konstante: `puterror(DTC_BT_PING, -1)` bzw. `dtcerr(DTC_BT_PING)`.
- Ausgabe am Gerät: `Fehler 11001 Bluetooth` – der **Klartext bleibt**, die Nummer ist
  jetzt stabil und im Katalog beschrieben.
- Das bisherige zeilenbasierte `dtc.h` entfällt.

## Neue Diagnose hinzufügen (der „einfach hochzählen"-Weg)

1. Subsystem wählen.
2. **Nächste freie Nummer** im Bereich nehmen.
3. **Eine Zeile** in `Diagnose-Codes.csv` ergänzen (Kurztext, Ursache, Abhilfe, Version).
4. Konstante in `dtc_codes.h` ergänzen (oder Generator neu laufen lassen).
5. Konstante an der Fehlerstelle verwenden.

Bestehende Codes dabei **nie** ändern. Fertig.

## Migration vom zeilenbasierten DTC

- Die bereits inventarisierten Fehlerstellen bekommen je eine **feste** Nummer und einen
  Katalog-Eintrag.
- `dtc.h`-Umleitung entfernen, Aufrufe auf die Konstanten umstellen.
- Einmaliger Aufwand; danach dauerhaft stabil.

## Warum das 10 Jahre trägt

- **Append-only** → keine Verschiebung mehr (genau das Gegenteil des heutigen Problems).
- **1000 Codes Reserve je Subsystem** + beliebig neue Bereiche.
- **SeitVersion** dokumentiert die Einführung; veraltete Codes bleiben mit Status `veraltet`
  stehen (keine Wiederverwendung → alte Logs bleiben deutbar).
- **CSV** ist ein langlebiges, offenes, von jedem Werkzeug lesbares Format.
