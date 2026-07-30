# Antwort der Firmware-/Chip-Seite auf `ble-new-chip-bringup.md`

> **Adressat:** KI/Entwickler auf der **App-Seite** (Flutter `viagraph_app`, iOS).
> **Absender:** Firmware-Seite (sism555, LPC1766, BT-Modul **Vela IF820 / Ezurio**).
> **Geräte-Firmware sism555:** **5.54**. **Modul-Firmware (EZ-Serial):** **1.4.12.12** auf der
> getesteten Einheit (das Eval-Board lief mit 1.4.18.18 — für den BLE-Pfad kein relevanter
> Unterschied festgestellt).
> **Stand:** 2026-07-30.
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

## 2. Was firmware-seitig geändert wurde

**Version 5.51 — der eigentliche Fix:**

1. Zustandsmerker `bt_cyspp_out` — es ist jetzt jederzeit bekannt, ob der Host die CYSPP-Leitung
   treibt.
2. Beim Freigeben der UART1 durch GSM/GPS wird CYSPP bedingungslos freigegeben.
3. **Selbstheilung im Minutentakt:** wird CYSPP getrieben, obwohl keine BT-Verbindung besteht
   und die UART1 nicht dem BT-Modul gehört, gibt die Firmware die Leitung frei. Ein solcher
   Zustand hält also maximal eine Minute, statt bis zum Reboot.
4. Freigabe zusätzlich beim Boot für **jeden** Reset-Grund — der Port-Expander behält sein
   Konfigurationsregister über einen Reflash, ein hängengebliebener Zustand überlebte also
   bisher sogar ein Firmware-Update.

**Version 5.52 — Nebenbefund, für euch ohne Auswirkung:** Die CYSPP-Leitung ist geräteseitig
dieselbe physische Leitung, die bei den beiden älteren BT-Modulen (RN4678, Laird BT730) ein
reiner Statusausgang ist. Das Treiben ist jetzt auf den IF820 eingegrenzt. Betrifft nur die
Altmodule, nicht den BLE-Pfad.

**Version 5.54 — für euch relevant:** Sniff-Mode am Modul abgeschaltet (Durchsatz) und die
**Class of Device auf „seriell" gesetzt** (vorher „Kopfhörer" → Windows sortierte das Gerät als
Audiogerät ein). Betrifft primär den Classic-Pfad; für die BLE-App ohne direkte Auswirkung, aber
falls ihr das Gerät auch in einem generischen BLE-Scanner seht, ist die Kategorie jetzt neutral.

Details: `project-status.md`, Kapitel „Update 5.51"–„Update 5.54".

> **Status:** Firmware ist **gebaut und am Gerät im Einsatz** (Version 5.54). Der Classic-SPP-Pfad
> läuft stabil (Massendownloads erfolgreich) — das ist der direkte Beleg, dass die CYSPP-Leitung
> **nicht mehr klemmt**, denn genau dieselbe Leitung blockierte vorher auch den BLE-Datenkanal.
> **Damit ist der Retest auf eurer Seite jetzt sinnvoll** (vorher hatten wir darum gebeten zu
> warten). Der finale Nachweis „Notify-Enable trennt nicht mehr" steht auf der **BLE-Seite** aber
> noch aus — den könnt nur ihr mit der Probe liefern.

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

## 3b. NEU und wichtig: 90-Sekunden-Advertising-Fenster

Das Modul **advertised nach dem Boot nur ~90 Sekunden** (30 s High-Duty + 60 s Low-Duty), danach
**stoppt es**. Das ist Werksverhalten von EZ-Serial, kein Fehler. Praktische Folgen für die App:

- Ein Gerät, das schon länger läuft, ist per BLE **nicht auffindbar**, bis es neu gebootet wird.
  Wenn eure Scans „nichts finden", ist das der wahrscheinlichste Grund — nicht ein Verbindungsbug.
- Für einen reproduzierbaren Test: Gerät stromlos machen / rebooten und **innerhalb von 90 s**
  scannen und verbinden.

**Erledigt (ab Firmware 5.55):** Das Advertising ist jetzt **dauerhaft** — das Gerät bewirbt sich
die ersten 30 s im schnellen Intervall, danach unbegrenzt im stromsparenden Low-Duty-Intervall
(3 s), und startet nach jedem Disconnect automatisch neu. Ein Gerät ist damit **jederzeit
auffindbar**, nicht nur im 90-s-Fenster nach dem Boot. Ihr müsst also nicht mehr innerhalb von
90 s scannen — der Hinweis oben gilt nur für Geräte mit älterer Firmware (< 5.55).

## 3c. Kein OS-Pairing — jetzt mit Gerätedaten bestätigt

Wir haben die CYSPP-/Security-Parameter am Gerät ausgelesen:

```
.CYSPPGP → E=02, G=00, C=0131, L=00000000, R=..., M=..., P=01, S=00, F=02
```

`S=00` heißt **„kein authentifizierter Link erforderlich"** — der Client darf die CYSPP-Data-Char
ohne Pairing/Bonding beschreiben und abonnieren. Zusammen mit den SMP-Defaults (IO-Capability
„No Input/No Output", Auto-Accept) bedeutet das: **iOS soll beim Verbinden oder beim CCCD-Write
keinen Passkey-/Pairing-Dialog zeigen.** Falls doch je einer erscheint, stimmt etwas nicht — dann
bitte melden, das wäre ein echter Befund. Der Zugangsschutz bleibt ausschließlich die
**App-Layer-PIN** (siehe 4b), nicht die BT-Sicherheitsebene.

## 3d. Durchsatz / MTU — was steuerbar ist und was nicht

- **MTU** (ihr habt 515 gesehen): Das bestimmt **der Client (iOS)**, nicht das Modul. Es gibt
  modulseitig **kein** MTU-Kommando, ebenso keins für Data-Length-Extension oder 2M-PHY. Ihr
  treibt die MTU also selbst hoch — gut so, nichts von uns nötig.
- **Paketierung:** Der einzige modulseitige Durchsatz-Hebel ist die CYSPP-Paketierung
  (`.CYSPPSK`). Der Werks-Wartewert (5 ms) ist bei unserer UART-Baud (460800) zu träge; wir
  prüfen, ihn auf 1 ms zu setzen. Das betrifft die Richtung **Gerät→App**.
- **Connection Interval:** dominiert den BLE-Durchsatz und wird von **iOS** vorgegeben (deckelt
  typisch bei 30 ms). Als Peripheral können wir nur ein kürzeres Intervall *anfragen* (`/UCP`),
  iOS darf ablehnen. Für Menü/Handshake ist das egal; nur bei großen Transfers relevant.

Kurz: Wenn ihr mehr BLE-Durchsatz braucht, liegt der größte Hebel bei **euch** (MTU, Connection
Interval), nicht bei uns.

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
| 5. Manufacturer-Payload befüllen | **Ursache gefunden:** die Manufacturer-Data sind `C=0x0131` (Cypress) + `L=0` → daher die `00000000`. Wir können `L` per `.CYSPPSP` mit der Seriennummer füllen. **Frage an euch:** braucht ihr die Seriennummer in der Manufacturer-Data, oder reicht sie euch aus dem Advertising-Namen `VIASIS_<seriennr>_BLE`? Wenn gebraucht, bauen wir es ein |

---

## 7. Was wir von der App-Seite brauchen

1. **Retest ist jetzt freigegeben** — 5.54 läuft am Gerät, der Classic-Pfad bestätigt, dass die
   CYSPP-Leitung nicht mehr klemmt. Bitte **innerhalb von 90 s nach einem Geräte-Reboot** scannen
   (Abschnitt 3b).
2. **Probe-Reihenfolge anpassen** (Abschnitt 4a) und **PIN-Handling in die Probe** aufnehmen (4b)
   — sonst ist der Retest nicht aussagekräftig. Erwartet als ersten `⇐`-Empfang die
   **unaufgeforderte PIN-Aufforderung**, nicht eine Antwort auf `H`/`V`.
3. Beim Retest bitte notieren, **ob unmittelbar vorher eine Mobilfunk-Aktion lief** (LTE/MQTT/
   SMS). Das war der Auslöser des Bugs und ist der aussagekräftigste Testfall: BT verbinden →
   GSM-Aktion → BT trennen lassen → GSM-Session beenden → erneut verbinden + subscriben.
4. Zwei kurze Rückfragen, damit wir passend konfigurieren: **(a)** braucht ihr **dauerhaftes
   Advertising** (Abschnitt 3b)? **(b)** braucht ihr die **Seriennummer in der Manufacturer-Data**
   (Abschnitt 6, Punkt 5), oder reicht der Advertising-Name?

---

## Quellen

- Maßgeblich (Classic **und** BLE, inkl. Kommandoreferenz): **EZ-Serial User Guide Vela IF820
  v2.2**, liegt im Repo unter `Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf` (Ezurio).
  Der Infineon-CYW20822-Guide ist BLE-only und für die Classic-Seite unvollständig.
- [EZ-Serial Cable Replacement (CYSPP) Quick Start — Vela IF820 User Guide, Ezurio](https://lairdcp.github.io/guides/vela_if820_user_guide/1.0/ez_serial_cyspp.html)
- Intern: `project-status.md` (Update 5.51–5.54), `Doku/IF820_EZSerial_Konzept_Mapping.md`,
  `Doku/UART1_Owner_StateMachine.md`, `Doku/IF820_Config_Kandidaten.md`
