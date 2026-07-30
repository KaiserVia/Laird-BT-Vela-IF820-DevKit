# BLE Bring-up-Report: neuer Chip „VIASIS_…_BLE" (Cypress/Infineon)

> **Adressat:** Die KI auf der **Firmware-/Chip-Seite**.
> **Absender:** KI auf der **App-Seite** (Flutter-App `viagraph_app`, iOS-Client).
> **Zweck:** Ein reproduzierbarer BLE-Defekt wurde eingekreist. Dieser Report
> gibt dir alles, was du brauchst, um den Fehler firmware-seitig zu finden — und
> beschreibt, wie die App-Seite den Chip ansprechen *will*, damit ihr das
> GATT-Interface aufeinander abstimmen könnt.
>
> **Kernaussage in einem Satz:** *Schreiben (TX) funktioniert einwandfrei, aber
> der Chip trennt die Verbindung sofort, sobald der Client Notifications/
> Indications aktiviert (Write auf CCCD `0x2902`).* → Kein RX möglich.

---

## 1. Setup / Umgebung

| Punkt | Wert |
|---|---|
| Client | iPhone, **iOS 26.5**, Apple CoreBluetooth |
| App | Flutter `viagraph_app`, BLE via `flutter_blue_plus` |
| Chip-Advertising-Name | `VIASIS_26SC9993_BLE` (auch `…9994…` = andere Einheit gesehen) |
| CoreBluetooth-Geräte-ID | `348F3206-6D31-447C-0AD0-D51EBB5A2698` (iOS liefert eine CBUUID, **keine MAC**) |
| Manufacturer ID (AdvData) | **`0x0131` = Cypress Semiconductor (heute Infineon)** |
| Manufacturer-Payload | `00 00 00 00` (noch nicht befüllt) |
| Beworbene Service-UUID | `65333333-a115-11e2-9e9a-0800200ca100` |
| MTU | initial 23, handelt korrekt auf **515** hoch |

> Hinweis: Die App-Seite hängt via VS-Code-Debug-Session am iPhone; alle Logs
> unten stammen aus der Dart-Debug-Konsole (`dart:developer log`). Der Test wird
> über eine temporäre Diagnose-Probe in der App ausgelöst (siehe Abschnitt 7).

---

## 2. Was funktioniert ✅

1. **Advertising / Scan** — Gerät ist sichtbar, `connectable=true`, RSSI ~ −49…−54 dBm (nah).
2. **Verbindungsaufbau** — GATT-Connect klappt zuverlässig.
3. **Service Discovery** — 2 Services werden gefunden (siehe Abschnitt 4).
4. **MTU-Negotiation** — steigt auf 515 (iOS-Maximum), also voll funktionsfähig.
5. **Schreiben (TX)** — Writes auf die beschreibbaren Chars werden mit ATT-Success
   quittiert, die Verbindung **bleibt stabil**. (Belegt in Abschnitt 5, Phase 1.)

## 3. Was NICHT funktioniert ❌

1. **Notifications/Indications aktivieren** — der Write auf die CCCD (`0x2902`,
   Value `0x0001`/`0x0002`) wird noch mit **ATT-Success** quittiert, **unmittelbar
   danach trennt der Chip die Verbindung**. Reproduzierbar in **3 von 3 Läufen**,
   **unabhängig davon, welche Characteristic** zuerst abonniert wird
   (`…ca101` indicate *und* `…ca102` notify lösen es beide aus).
   → **Das ist der zentrale Blocker.** Ohne Notify gibt es kein RX.
2. **Lesen (Read)** — die eine lesbare Char (`a47f7608-…`) **antwortet nicht**
   (ATT-Read läuft in Timeout). Sekundär, aber vermutlich derselbe „Handler nicht
   implementiert"-Cluster.

---

## 4. Vollständige GATT-Tabelle (vom Chip discovered)

### Service 1 — `65333333-a115-11e2-9e9a-0800200ca100` (primary, **auch im Advertising**)
Sieht aus wie der beabsichtigte **Transparent-/UART-Datenkanal**.

| Characteristic | Properties | Descriptors |
|---|---|---|
| `65333333-a115-11e2-9e9a-0800200ca101` | `write`, `indicate` | `0x2902` (CCCD) |
| `65333333-a115-11e2-9e9a-0800200ca102` | `writeWithoutResponse`, `notify` | `0x2902` (CCCD) |
| `65333333-a115-11e2-9e9a-0800200ca103` | `indicate` | `0x2902` (CCCD) |

### Service 2 — `ae5d1e47-5c13-43a0-8635-82ad38a1381f` (primary)
Zweck unklar (Konfig? OTA? Steuerung?).

| Characteristic | Properties | Descriptors |
|---|---|---|
| `a3dd50bf-f7a7-4e99-838e-570a086c661b` | `write`, `notify`, `indicate` | `0x2902` (CCCD) |
| `a2e86c7a-d961-4091-b74f-2409e72efe26` | `write` | — |
| `a47f7608-2e2d-47eb-913b-75d4edc4de4b` | `read` | — (Read läuft in Timeout) |

> **Nur Service 1 wurde bisher aktiv getestet** (Notify-Enable + Writes). Service 2
> hat dieselbe Notify/Indicate-Struktur — der CCCD-Bug betrifft ihn mit hoher
> Wahrscheinlichkeit ebenso, ist aber **noch nicht verifiziert** (bewusst nicht
> getestet, weil der erste Notify-Enable-Versuch die Verbindung sofort killt und
> ein separater Verbindungslauf nötig wäre).

---

## 5. Testmethodik + Rohlogs (Beweis)

Die App-seitige Diagnose-Probe läuft beim Verbinden in **3 Phasen** über Service 1:

- **Phase 1:** `H` und `V` an alle beschreibbaren Chars schreiben — **ohne** Notify.
- **Phase 2:** Notify/Indicate aktivieren (CCCD-Write), Notify (`…ca102`) zuerst.
- **Phase 3:** falls noch verbunden, erneut schreiben und auf eingehende Bytes lauschen.

`H` und `V` sind einzelne ASCII-Bytes (`0x48` / `0x56`) — die Handshake- bzw.
Identify-Kommandos des viasis-Serienprotokolls (siehe Abschnitt 6).

### Rohlog eines vollständigen Laufs

```
Gerät verbunden
Mtu: 23
Discovered 2 services.
┌─ LINK-PROBE · 348F3206-6D31-447C-0AD0-D51EBB5A2698
│ — Phase 1: write ohne Notify —
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca101 write "H" ok
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca102 write "H" ok
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca101 write "V" ok
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca102 write "V" ok
│ — Phase 2: Notify/Indicate aktivieren —
│ subscribe 65333333-a115-11e2-9e9a-0800200ca102 ok
│ ⚡ Verbindung während der Probe verloren
│ — Phase 3: write mit aktivem Notify —
│ subscribe 65333333-a115-11e2-9e9a-0800200ca101 Fehler: setNotifyValue | fbp-code: 6 | Device is disconnected
│ subscribe 65333333-a115-11e2-9e9a-0800200ca103 Fehler: setNotifyValue | fbp-code: 6 | device is not connected
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca101 write "H" Fehler: writeCharacteristic | fbp-code: 6 | device is not connected
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca102 write "H" Fehler: writeCharacteristic | fbp-code: 6 | device is not connected
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca101 write "V" Fehler: writeCharacteristic | fbp-code: 6 | device is not connected
│ ⇒ 65333333-a115-11e2-9e9a-0800200ca102 write "V" Fehler: writeCharacteristic | fbp-code: 6 | device is not connected
└─ LINK-PROBE Ende
```

### Advertising-Dump (ohne Verbindung)

```
┌─ DEEP-PROBE · Advertisement · 348F3206-6D31-447C-0AD0-D51EBB5A2698
│ rssi=-49 dBm  connectable=true
│ advName="VIASIS_26SC9993_BLE"
│ serviceUuids=65333333-a115-11e2-9e9a-0800200ca100
│ manufacturerData companyId=0x0131 data=[00 00 00 00]
└─ DEEP-PROBE · Advertisement Ende
```

### Interpretation der Logs

| Beobachtung | Schlussfolgerung |
|---|---|
| Phase 1: 4× `write … ok`, kein Disconnect | **TX/Schreibpfad ist intakt**, Link ohne CCCD stabil |
| Phase 2: `subscribe … ok` **direkt** gefolgt von `⚡` | **Der CCCD-Write ist der alleinige Auslöser** des Disconnects |
| CCCD-Write kommt noch mit `ok` zurück | Der Chip **quittiert** den Descriptor-Write, faultet **danach** |
| `fbp-code: 6` = „device is not connected" | Ab dem `⚡` ist der Link tot, alle Folge-Ops scheitern |

---

## 6. Wie die App den Chip ansprechen WILL (Zielbild)

Damit ihr das GATT-Interface abstimmen könnt — so funktioniert der **bestehende**
(RN4678-basierte) Weg, den die App produktiv nutzt:

- **Transparent-UART-Service** (alt, RN4678): `49535343-fe7d-4ae5-8fa9-9fafd205e455`
  - **Transparent-Data-Characteristic:** `49535343-1e4d-4bd9-ba61-23c647249616`
    mit `notify` + `write` → **eine** Char für bidirektionale Serial-Bridge.
- Ablauf der App: verbinden → `discoverServices` → **`setNotifyValue(true)` auf der
  Transparent-Char** → dann liest/schreibt sie serielle Bytes darüber.
- **Protokoll darüber ist seriell/UART-artig.** Kommandos sind ASCII-Einzelzeichen,
  z.B. `H` (Handshake), `V` (Identify/Geräteerkennung), gefolgt von Antworten;
  größere Transfers laufen per X/Y-Modem. **Kritisch: Die App ist zwingend auf
  Notifications angewiesen — das ist der einzige RX-Weg.**

**Konsequenz für den neuen Chip:** Sobald der Notify-Pfad steht, mappt die App den
neuen Service (`65333333-…`) analog: eine Char als TX (Phone→Chip), eine als RX
(Chip→Phone via notify). **Bitte klärt/dokumentiert die beabsichtigte TX/RX-Zuordnung**
der Chars `…ca101` / `…ca102` / `…ca103` (aktuell nur aus den Properties geraten:
`…ca102` [writeNR, notify] wirkt wie der bidirektionale Datenpipe).

---

## 7. Konkrete To-Dos für die Firmware-Seite

Priorisiert:

1. **[BLOCKER] CCCD-/Notification-Enable-Handler reparieren.**
   Reproduktion: Client schreibt CCCD `0x2902` = `0x0001` (notify) bzw. `0x0002`
   (indicate) auf **irgendeine** Char in Service `65333333-…`. Der Write wird mit
   ATT-Success beantwortet, danach trennt der Chip. → Im Notification-Subsystem
   nach einem Fault/Assert/Reset **nach** dem Descriptor-Write suchen (z.B. Senden
   ohne Puffer, Null-Callback, Stack-Overflow im Notify-Task).
2. **Security-Requirement der CCCD prüfen.** Falls die CCCD als
   „encryption/authentication required" konfiguriert ist: Entweder Pairing/Bonding
   sauber unterstützen (dann würde iOS Pairing anstoßen) **oder** die Anforderung
   entfernen. *Weniger wahrscheinlich* (der Write wird mit `ok` quittiert statt mit
   ATT-Error `Insufficient Authentication`), aber ausschließen.
3. **Read-Handler für `a47f7608-…` implementieren/fixen** (aktuell Timeout).
4. **TX/RX-Zuordnung dokumentieren** (welche Char ist Phone→Chip, welche Chip→Phone).
5. **Manufacturer-Payload** ist noch `00 00 00 00` — falls Seriennummer/Version
   geplant ist, hier befüllen (die App-Seite liest die Seriennummer sonst aus dem
   Advertising-Namen `VIASIS_<seriennr>_BLE`).

---

## 8. Retest nach Firmware-Fix

Die temporäre **Diagnose-Probe bleibt in der App-Codebasis** (bewusst nicht
entfernt), damit der nächste Test sofort läuft. Ort:
`lib/services/communication/bluetoothPlus.dart` → als `TEMP … Deep-Probe`-Block
markiert (Methoden `_dumpAdvertisement`, `_dumpGattTable`, `_linkBringUpProbe`),
aufgerufen im `connect()`-Pfad. **Nur Konsole (`dart:developer log`), kein
persistentes Logging.**

**Erfolgs-Kriterium nach dem Fix** (im Log sichtbar):
```
│ — Phase 2: Notify/Indicate aktivieren —
│ subscribe 65333333-…-ca102 ok        ← und DANACH KEIN "⚡"
│ — Phase 3: write mit aktivem Notify —
│ ⇒ 65333333-…-ca101 write "H" ok
│ ⇐ 65333333-…-ca102 [.. .. ..] ascii="…"   ← ANTWORT DES CHIPS = RX gefunden
```
Sobald eine `⇐`-Zeile nach einem `⇒ write` erscheint, ist der Transparent-Kanal
funktionsfähig; die App-Seite verdrahtet dann Service/TX/RX fest (analog zum
RN4678-Pfad) und die Diagnose-Probe wird entfernt.

---

## 9. Status App-Seite (bereits erledigt, unabhängig vom Chip)

Zwei App-Fixes wurden im Zuge dieser Analyse gemacht und sind **verifiziert**
(siehe `CHANGELOG.md`, `[Unreleased]`):

1. **Scan-Anzeige:** Neue `VIASIS_…_BLE`-Module erscheinen jetzt in der Geräteliste
   (vorher wurden auf iOS nur Geräte mit `rn4678` im Namen durchgereicht).
2. **Kein Endlos-„Verbinde…" mehr:** Fehlt der erwartete Ziel-Service, trennt die
   App sauber und geht in `NICHT_VERBUNDEN`, statt im Verbindungsspinner zu hängen.

Diese sind **produktiv** und unabhängig vom Chip-Fix. Der Rest (Deep-Probe/
Link-Probe) ist temporär und verlässt die Codebasis, sobald der Chip kommuniziert.
