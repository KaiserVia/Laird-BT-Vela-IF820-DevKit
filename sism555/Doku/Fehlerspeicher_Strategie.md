# Fehlerspeicher – Strategie & Umsetzung

Speichern von Fehlern im Gerät und späteres Auslesen. Baut auf den **stabilen Diagnose-Codes**
auf (`Diagnose-Codes_Konzept.md`, Katalog `Diagnose-Codes.csv`, 5-stellig 11xxx–99xxx).

## Gewählte Strategie (Entscheidungen)

- **Speicherform:** DTC-**Statustabelle** (ein Eintrag pro Code, nicht jedes Einzel-Event).
- **Löschen:** ja, durch Service (empfohlen: mit Service-PIN geschützt).
- **Auslesen über drei Wege:** RS232/BT-Menü (vor Ort), USB-Datei (CSV), Remote über LTE/MQTT.

## Datenmodell – Statustabelle (im Flash)

Ein Eintrag je aufgetretenem Code:

| Feld | Typ | Inhalt |
|------|-----|--------|
| code | uint16/uint32 | 5-stelliger Diagnose-Code (z. B. 11001) |
| count | uint16 | Anzahl Auftreten (deckelt bei Max, kein Überlauf) |
| first_time | RTC-Zeit | erstes Auftreten |
| first_hours | uint | Betriebsstunden beim ersten Auftreten (`fp.hcount`) |
| last_time | RTC-Zeit | letztes Auftreten |
| last_hours | uint | Betriebsstunden beim letzten Auftreten |
| status | uint8 | z. B. Bit0 = „in dieser Sitzung aktiv", Bit1 = „gespeichert" |

- **Betriebsstunden zusätzlich zur RTC**, weil die Uhr ungestellt sein kann → robustere Einordnung.
- **Feste Tabellengröße**, Vorschlag **64 Einträge** (anpassbar). Reicht, da Codes endlich/gruppiert.
- Voll? Strategie festlegen: entweder „voll = neue Codes verwerfen" **oder** den am längsten nicht
  gesehenen Eintrag verdrängen. Vorschlag: ältesten verdrängen (Ring-Verhalten auf Tabellenebene).

## Erfassung – an EINER Stelle

Alle Fehler laufen durch die zentrale Fehlerfunktion (`puterror`/`dtcerr` → die `*_dtc`-Funktionen
in `sio.c`). Dort wird **ein** Aufruf `record_dtc(code)` ergänzt:

- Code in der Tabelle suchen.
- Vorhanden → `count++`, `last_time`/`last_hours` aktualisieren, Status „aktiv" setzen.
- Neu → freien (oder ältesten) Platz belegen, `first_*`/`last_*` setzen, `count=1`.

So wird die Tabelle automatisch gepflegt, ohne Änderungen an jeder einzelnen Fehlerstelle.

## Flash-Speicherung & Verschleißschutz

- Tabelle **im RAM** halten (Arbeitskopie).
- In den Flash schreiben **nur bei Änderung**, dabei gepuffert/entprellt (z. B. verzögert oder
  beim sauberen Ausschalten/Leerlauf), **nicht** bei jedem Event → schont Schreibzyklen.
- Eigener kleiner Flash-Bereich (Sektor) oder Anbindung an die bestehende Flash-/`protocol()`-Logik.
- Magic/Version + Prüfsumme im Tabellenkopf → erkennt leeren/korrupten Speicher und initialisiert sauber.

## Auslese-Weg 1 – RS232/BT-Menü (vor Ort)

- Menüpunkt **„Fehlerspeicher"**: listet je Eintrag `Code | Kurztext | Zähler | zuletzt | Status`.
- Kurztext kommt aus dem Katalog (im Gerät eine knappe Variante; Voll-Beschreibung im CSV/PDF).
- Funktioniert über RS232 und – nach PIN – über Bluetooth (gleiche Menü-Ebene).

## Auslese-Weg 2 – USB-Datei (CSV)

- Menüpunkt **„Fehlerspeicher exportieren"** → schreibt die Tabelle als **CSV** auf USB-Stick:
  `Code;Zaehler;ErstZeit;ErstStunden;LetztZeit;LetztStunden;Status`
- In Excel auswertbar; per Code mit `Diagnose-Codes.csv` (Ursache/Abhilfe) verknüpfbar.
- Nutzt die vorhandene USB-Dateifunktion.

## Auslese-Weg 3 – Remote über LTE/MQTT

- Tabelle bzw. **Änderungen** periodisch oder bei neuem Fehler per MQTT ans Backend senden
  (kompaktes Format: Code, Zähler, Zeit/Stunden, Status, Geräte-Seriennummer, FW-Version).
- Offline → lokal gepuffert, beim nächsten Verbindungsaufbau nachsenden.
- Dashboard löst Code → Beschreibung über denselben Katalog (CSV/JSON) auf.

## Löschen / Quittieren

- Menüpunkt **„Fehlerspeicher löschen"** → Tabelle zurücksetzen (count=0, Einträge frei).
- **Empfehlung: mit Service-PIN** absichern (App-Layer-PIN ist vorhanden), gegen versehentliches Löschen.
- Optional „Löschen“-Ereignis selbst protokollieren (Zeit/Stunden), damit nachvollziehbar bleibt,
  wann zuletzt zurückgesetzt wurde.

## Abhängigkeit / Reihenfolge

1. **Zuerst** die Migration auf stabile Codes (`Diagnose-Codes_Umsetzungsplan.md`) – der
   Fehlerspeicher MUSS auf festen Codes basieren, sonst sind gespeicherte Codes über
   Versionen hinweg nicht deutbar.
2. **Danach** dieser Fehlerspeicher obendrauf (gleiche Codes, gleicher Katalog).

## Umsetzungsschritte

1. Datenstruktur + RAM-Tabelle + Flash-Layout (Magic/Version/Prüfsumme) definieren.
2. `record_dtc(code)` in `sio.c` einhängen (in den zentralen `*_dtc`-Funktionen).
3. Flash-Lesen beim Start (Init/Recovery bei leer/korrupt) + gepuffertes Schreiben bei Änderung.
4. Menüpunkt „Fehlerspeicher" (Liste) – RS232/BT.
5. Menüpunkt „Fehlerspeicher exportieren" (CSV auf USB).
6. Menüpunkt „Fehlerspeicher löschen" (mit Service-PIN).
7. MQTT-Versand (Tabelle/Änderungen) + Offline-Puffer.
8. Test: Fehler auslösen → Eintrag/Zähler/Zeit prüfen; Export, Remote, Löschen prüfen.

## Offene Parameter (Vorschläge, anpassbar)

- Tabellengröße: **64** Einträge.
- Zeitbasis: RTC **und** Betriebsstunden.
- Voll-Strategie: ältesten Eintrag verdrängen.
- Status-Bits: „aktiv in Sitzung" (beim Boot zurückgesetzt) + „jemals gespeichert".
- Flash-Schreibtakt: bei Änderung, entprellt (Detail in der Umsetzung festlegen).

## Konventionen

- Quelldateien ISO-8859-1/CRLF, byte-sicher editieren; SW-Version je Änderung +0.01.
- Beschreibungen/Texte zentral im Katalog `Diagnose-Codes.csv` (eine Quelle).
