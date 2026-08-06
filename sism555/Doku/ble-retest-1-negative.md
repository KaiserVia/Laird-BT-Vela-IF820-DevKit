# Retest #1 nach Firmware-Fix — Ergebnis: **NEGATIV** (2 Boards)

> **Adressat:** Firmware-/Chip-Seite (IF820 / EZ-Serial, sism555 / LPC1766).
> **Absender:** App-Seite (Flutter `viagraph_app`, iOS 26.5, `flutter_blue_plus`).
> **Bezug:** eure Antwort `ble-chip-response.md` (Fix CYSPP-Pin, Version 5.51 ff.).
> **Stand:** 2026-07-30.
>
> **Kernaussage:** *Der Retest nach eurem Fix ist negativ. Das Symptom ist
> unverändert — der Chip trennt die Verbindung sofort nach dem Notify-Enable
> (CCCD-Write) auf der CYSPP-Data-Char `…ca102`. **Reproduziert auf zwei
> verschiedenen Boards.** Wir haben eure Vorgaben aus Abschnitt 4a (erst
> subscriben) umgesetzt, also kein Phase-1-Fehlschluss mehr.*

---

## 1. Testbedingungen (nach euren Vorgaben angepasst)

- Probe-Reihenfolge auf **subscribe-first** umgestellt (euer Abschnitt 4a):
  connect → `discoverServices` → **`setNotifyValue(true)` auf `…ca102` ZUERST**
  → lauschen → *danach* erst `H`/`V`.
- **PIN wird bewusst nicht gesendet** — laut euch ist die erste unaufgeforderte
  `⇐`-Zeile (PIN-Prompt) bereits der RX-Beweis.
- Advertising war dauerhaft sichtbar, Scan mit ausreichend Timeout.

## 2. Ergebnis: unverändert, auf **zwei** Boards

| Board (Advert-Name) | iOS-Geräte-ID | Ergebnis |
|---|---|---|
| `VIASIS_26SC9993_BLE` | `348F3206-…-D51EBB5A2698` | `subscribe …ca102 ok` → **sofort `⚡` Disconnect** |
| `VIASIS_26SC9996_BLE` | `ED0E9007-…-CE36470BF2D0` | `subscribe …ca102 ok` → **sofort `⚡` Disconnect** |

Roh-Log (Board 26SC9996, Board 26SC9993 identisch):

```
Gerät verbunden
Mtu: 23
Discovered 2 services.
┌─ LINK-PROBE (CYSPP) · ED0E9007-E300-7EC5-6747-CE36470BF2D0
│ subscribe …ca102 ok — Datenmodus aktiv, lausche auf PIN-Prompt …
│ ⚡ Verbindung während der Probe verloren
│ ⇒ write "H" Fehler: writeCharacteristic | fbp-code: 6 | Device is disconnected
│ ⇒ write "V" Fehler: writeCharacteristic | fbp-code: 6 | device is not connected
└─ LINK-PROBE (CYSPP) Ende
```

Die GATT-Tabelle ist unverändert (Service 1 `65333333-…ca100` mit
`…ca101`/`…ca102`/`…ca103`, Service 2 `ae5d1e47-…`).

## 3. Was app-seitig ausgeschlossen ist

- **Peripherie-initiierter Disconnect:** Die App sendet keinerlei Disconnect;
  `device.connectionState` wechselt von sich aus auf `disconnected`. Das `⚡` fällt
  **exakt** im Moment des CCCD-Writes — 1:1 euer beschriebenes CYSPP-Pin-Verhalten.
- **Kein Phase-1-Fehlschluss mehr:** Wir schreiben erst *nach* dem Subscribe.
- **CCCD-Write wird noch mit ATT-Success quittiert**, Disconnect kommt danach —
  identisch zum Zustand *vor* dem Fix.
- **Reproduzierbar über 2 Boards** → spricht gegen „ein einzelnes, gerade
  verriegeltes Gerät".

## 4. Offene Fragen an euch (bitte gezielt beantworten)

1. **Firmware-Stand auf genau diesen Einheiten:** Laufen `26SC9993` **und**
   `26SC9996` tatsächlich auf sism555 **5.51+**? Wie lässt sich die laufende
   Version verifizieren (idealerweise ohne Zerlegen — geht das über BLE/Advert
   oder das serielle Menü)?
2. **Boot-Release wirklich erfolgt?** Euer 5.51-Punkt „Freigabe beim Boot für
   jeden Reset-Grund" soll den Pin beim Start bedingungslos freigeben. Sind diese
   Boards seit dem Flash **vollständig stromlos** gewesen (echter Power-Cycle)?
3. **UART1-Owner zum Testzeitpunkt:** Läuft auf diesen Boards dauerhaft GPS/GSM,
   sodass die UART1 permanent bei GSM liegt? Dann greift die „Minutentakt"-
   Selbstheilung (nur bei *fehlender* BT-Verbindung **und** UART1 nicht bei GSM)
   nie — der Pin bliebe verriegelt.
4. **[Technischer Kernpunkt] Wird der CYSPP-Pin nach dem Fix aktiv LOW getrieben
   (= Datenmodus *freigegeben*), oder nur „released"?** Der Pin ist low-aktiv.
   Wird er beim BLE-Connect nicht aktiv **asserted (LOW)**, sondern nur
   losgelassen, liegt er extern (Pull-up?) auf HIGH = „Datenkanal beenden" — dann
   endet CYSPP beim Subscribe **immer**, unabhängig vom Latch-Bug. Bitte prüfen,
   ob der Fix den Pin für eine **BLE**-Session tatsächlich auf „Datenmodus
   erlaubt" stellt.
5. **Classic vs. BLE:** Euer Beleg „Leitung klemmt nicht mehr" stammt vom
   **Classic-SPP**-Massendownload. Nutzen Classic-SPP und BLE-CYSPP denselben
   Pin-/Enable-Pfad? Falls der Classic-Pfad den Pin anders (aktiv) treibt als der
   BLE-CYSPP-Pfad, wäre der BLE-Fall vom Fix evtl. nicht abgedeckt.

## 5. Unser Retest-Kriterium (unverändert)

Sobald der Fix auf einem Board greift, erwarten wir:
```
│ subscribe …ca102 ok        ← und DANACH KEIN "⚡"
│ ⇐ [..] ascii="… PIN …"     ← unaufgefordert = RX/Datenmodus steht
```
Die Diagnose-Probe bleibt in der App (`bluetoothPlus.dart`, TEMP-Block), der
nächste Retest ist also jederzeit in Sekunden möglich.

## 6. Was wir vorbereitet haben (App-Seite, wartet nur auf euren Fix)

Sobald der Datenkanal steht, ist die Integration ein **reiner UUID-Tausch**: Wir
verdrahten CYSPP `…ca102` (writeNR + notify) analog zur RN4678-Transparent-Char in
den Produktiv-`connect()`-Pfad; PIN/Handshake/Menü laufen dann über die
bestehende App-Logik. Es fehlt ausschließlich der funktionierende Notify-Pfad
vom Chip.
