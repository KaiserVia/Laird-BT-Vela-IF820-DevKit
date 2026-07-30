# Antwort der Firmware-/Chip-Seite auf `ble-new-chip-bringup.md`

> **Adressat:** KI/Entwickler auf der **App-Seite** (Flutter `viagraph_app`, iOS).
> **Absender:** Firmware-Seite (sism555, LPC1766, BT-Modul **Vela IF820 / Ezurio**,
> EZ-Serial-Firmware 1.4.18.18).
> **Stand:** 2026-07-30, Firmware-Version **5.51**.
>
> **Kernaussage in einem Satz:** *Der Report war exakt richtig eingekreist — die Ursache lag
> aber nicht in der GATT-Implementierung des Chips, sondern darin, dass unsere Host-Firmware
> die **CYSPP-Steuerleitung** dauerhaft auf HIGH hielt. Das ist laut Chip-Doku das Signal
> „Datenkanal beenden" — und wird genau in dem Moment wirksam, in dem der Client subscribed.*

---

## 1. Root Cause

Der von euch entdeckte Service ist kein proprietärer viasis-Service, sondern der
**CYSPP-Service** („Cable Replacement", Cypress Serial Port Profile) der EZ-Serial-Firmware:

`65333333-a115-11e2-9e9a-0800200ca100`

Das Aktivieren von Notifications/Indications ist der **dokumentierte Trigger**, der den
CYSPP-**Datenmodus** startet. Zum Beenden steht in der EZ-Serial-Doku:

> *„The data pipe will remain open until the peripheral device disconnects, **or the CYSPP pin
> is de-asserted locally**."*

Der CYSPP-Pin ist **low-aktiv**: LOW = Datenmodus, HIGH (extern getrieben) = „Command-Mode /
Datenkanal beenden". Bei uns hängt dieser Pin am I²C-Port-Expander IC54 (PCA9554), Bit 7.

Unsere Firmware treibt den Pin absichtlich HIGH, wenn das BT-Modul die gemeinsame UART1 an
das GSM-/GPS-Modem verliert (sonst leitet das Modul Leitungsrauschen als Junk weiter). Sie
gab die Leitung aber nur auf einem einzigen Rückweg wieder frei. Ging die BT-Verbindung
verloren, **während** GSM die UART hielt, wurde dieser Rückweg nie erreicht — die Leitung
blieb **bis zum Reboot** als Ausgang HIGH. Das ist selbstverriegelnd: das erzwungene HIGH
trennt die Verbindung, und der Verbindungsverlust verhindert die Freigabe.

**Damit erklären sich alle vier Beobachtungen aus eurem Report:**

| Eure Beobachtung | Erklärung |
|---|---|
| Connect, Discovery, MTU 515 laufen | GATT-Server läuft normal; der CYSPP-Pin blockiert nur den *Datenmodus*, nicht GAP/GATT |
| Writes → ATT-Success, Link stabil | Der GATT-Server quittiert korrekt. Ohne aktiven Datenmodus werden die Bytes aber **verworfen** — sie erreichen unsere MCU nie |
| CCCD-Write → Success, **danach** Disconnect | Der Chip startet den Datenmodus, sieht den Pin extern de-asserted und beendet die CYSPP-Session → GAP-Disconnect |
| 3 von 3, unabhängig von der Characteristic | Der Pin gilt für die ganze CYSPP-Session, nicht pro Characteristic |

Euer To-Do 2 (Security-Requirement auf der CCCD) lässt sich **streichen** — euer Argument war
korrekt: bei fehlender Berechtigung käme ATT-Error `Insufficient Authentication`, nicht Success.

---

## 2. Was firmware-seitig geändert wurde (Version 5.51)

1. Zustandsmerker `bt_cyspp_out` — es ist jetzt jederzeit bekannt, ob der Host die CYSPP-Leitung
   treibt.
2. Beim Freigeben der UART1 durch GSM/GPS wird CYSPP bedingungslos freigegeben.
3. **Selbstheilung im Minutentakt:** wird CYSPP getrieben, obwohl keine BT-Verbindung besteht
   und die UART1 nicht dem BT-Modul gehört, gibt die Firmware die Leitung frei. Ein solcher
   Zustand hält also maximal eine Minute, statt bis zum Reboot.

Details: `project-status.md`, Kapitel „Update 5.51".

> **Noch nicht am Gerät verifiziert.** Die Firmware ist noch nicht in Keil gebaut und nicht
> getestet. Bitte den Retest erst nach unserer Freigabe fahren — wir melden uns.

---

## 3. GATT-Interface: die von euch erbetene TX/RX-Zuordnung

Service 1 ist der CYSPP-Service. Die Zuordnung ist **nicht** frei wählbar, sie ist von
EZ-Serial vorgegeben:

| Characteristic | Rolle in CYSPP | Richtung / Nutzung durch die App |
|---|---|---|
| `…0800200ca101` | **Acknowledged Data** | Client schreibt (write **mit** Response) = TX Phone→Gerät; Client aktiviert **Indications** = RX Gerät→Phone. Zuverlässig, langsamer |
| `…0800200ca102` | **Unacknowledged Data** | Client schreibt (writeWithoutResponse) = TX; Client aktiviert **Notifications** = RX. Schneller |
| `…0800200ca103` | **RX Flow Control** | Nur Indications, optional. Das Gerät meldet damit, ob neue Writes gerade sicher sind |

Jede der beiden Data-Chars ist also **selbst schon bidirektional** — genau wie die
RN4678-Transparent-Char `49535343-1e4d-4bd9-ba61-23c647249616`, die ihr heute produktiv nutzt.
Die Umstellung ist damit ein reiner UUID-Tausch, keine Architekturänderung.

**Empfehlung:**

- **Genau EINE** Data-Characteristic verwenden — nicht beide. Empfehlung `…ca102`
  (notify + writeWithoutResponse), analog zum RN4678-Pfad.
- `…ca103` optional zusätzlich abonnieren, wenn ihr bei größeren Transfers (X/Y-Modem)
  Flusskontrolle wollt. Für Menü/Handshake nicht nötig.
- Service 2 (`ae5d1e47-…`) gehört zur modul-eigenen GATT-DB (nicht von uns bespielt) und ist
  für den Datenpfad **irrelevant**. Bitte nicht verdrahten.

---

## 4. Wichtig für die Probe: Reihenfolge und PIN

Zwei Dinge, die im aktuellen Probe-Ablauf zu falschen Schlüssen führen würden — auch nach
einem erfolgreichen Fix:

**a) Erst subscriben, dann schreiben.** Phase 1 („write ohne Notify") kann **nie**
funktionieren: ohne aktiven Datenmodus verwirft der Chip die Bytes, quittiert den Write aber
mit ATT-Success. Das ATT-Success ist hier also kein Erfolgsnachweis. Richtige Reihenfolge:

1. connect → `discoverServices`
2. `setNotifyValue(true)` auf **einer** Data-Char (`…ca102`)
3. **Verbindung ~1 s halten** — kommt hier kein Disconnect, ist der Datenkanal offen
4. erst jetzt schreiben und auf eingehende Bytes lauschen

**b) Das Gerät fragt zuerst die PIN ab.** Sobald der Datenkanal steht, erkennt unsere Firmware
eine neue BT-Verbindung und schickt von sich aus **Einschaltmeldung + PIN-Aufforderung**
(App-Layer-PIN, nicht der BLE-Passkey — das ist derselbe Mechanismus wie beim RN4678). Erst
nach korrekter PIN kommt das Hauptmenü, und erst danach reagiert das Gerät auf `H`/`V`.

Konkret heißt das für den Erfolgsnachweis: **die allererste `⇐`-Zeile nach dem Subscribe wird
die PIN-Aufforderung sein — ungefragt, ohne dass die App etwas gesendet hat.** Das ist bereits
der Beweis, dass RX funktioniert. Ihr müsst dafür also gar nichts schreiben; wenn die Probe
schreibt, bevor die PIN durch ist, gehen die Kommandos ins Leere.

**c) Timeout:** Empfängt das Gerät **6 Minuten** keine Zeichen, trennt es die Verbindung
aktiv (`BT_TIMEOUT`). Ein Disconnect nach mehreren Minuten Funkstille ist also erwartetes
Verhalten und kein Bug.

---

## 5. Erwartetes Log nach dem Fix

```
│ — Phase 2: Notify aktivieren —
│ subscribe 65333333-…-ca102 ok          ← und DANACH KEIN "⚡"
│ ⇐ 65333333-…-ca102 [..] ascii="… PIN …"   ← unaufgefordert: RX funktioniert
│ — Phase 3: PIN senden, dann Menü —
│ ⇒ 65333333-…-ca102 write "<PIN>\r" ok
│ ⇐ 65333333-…-ca102 [..] ascii="… Hauptmenü …"
```

---

## 6. Zu euren restlichen To-Dos

| Euer Punkt | Antwort |
|---|---|
| 1. CCCD-Handler reparieren | Kein Chip-Bug. Ursache war der CYSPP-Pin auf Host-Seite, gefixt in 5.51 (Abschnitt 1/2) |
| 2. Security-Requirement der CCCD | Ausgeschlossen — bei fehlender Berechtigung käme ATT-Error, nicht Success |
| 3. Read-Handler `a47f7608-…` | Gehört zur modul-eigenen GATT-DB (Service 2), nicht von uns implementiert. Für den Datenpfad ohne Bedeutung, wird nicht weiter verfolgt |
| 4. TX/RX-Zuordnung dokumentieren | Erledigt, Abschnitt 3 |
| 5. Manufacturer-Payload befüllen | Backlog, niedrige Priorität. Bis dahin bitte weiter die Seriennummer aus dem Advertising-Namen `VIASIS_<seriennr>_BLE` lesen |

---

## 7. Was wir von der App-Seite brauchen

1. **Probe-Reihenfolge anpassen** (Abschnitt 4a) und **PIN-Handling in die Probe** aufnehmen
   (4b) — sonst ist der Retest nicht aussagekräftig.
2. **Retest erst nach unserer Freigabe** — die 5.51 ist noch nicht gebaut und nicht am Gerät
   verifiziert.
3. Beim Retest bitte notieren, **ob unmittelbar vorher eine Mobilfunk-Aktion lief** (LTE/MQTT/
   SMS). Das war der Auslöser des Bugs und ist der aussagekräftigste Testfall: BT verbinden →
   GSM-Aktion → BT trennen lassen → GSM-Session beenden → erneut verbinden + subscriben.

---

## Quellen

- [EZ-Serial Cable Replacement (CYSPP) Quick Start — Vela IF820 User Guide, Ezurio](https://lairdcp.github.io/guides/vela_if820_user_guide/1.0/ez_serial_cyspp.html)
- [EZ-Serial Firmware Platform User Guide for CYW20822 modules (Infineon, PDF)](https://www.infineon.com/assets/row/public/documents/non-assigned/44/infineon-ez-serial-firmware-platform-user-guide-for-cyw20822-module-usermanual-en.pdf)
- Intern: `project-status.md` (Update 5.51, 5.48, 5.99), `Doku/IF820_EZSerial_Konzept_Mapping.md`,
  `Doku/UART1_Owner_StateMachine.md`
