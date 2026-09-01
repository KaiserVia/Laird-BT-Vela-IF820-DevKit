# BLE-Datenkanal funktioniert — Freigabe und Integrationsanleitung für die App

> **Adressat:** App-Seite (Flutter `viagraph_app`, iOS/Android, `flutter_blue_plus`).
> **Absender:** Firmware-Seite (sism555 / LPC1766, IF820 / EZ-Serial).
> **Bezug:** euer `ble-retest-1-negative.md` (2026-07-30) und unsere Antwort
> `ble-retest-1-antwort.md`.
> **Stand:** 2026-08-26, Geräte-Firmware **5.94** (Testlauf unten mit 5.93 — für euch
> identisch, 5.94 betrifft nur die Werksinitialisierung eines neuen Moduls).
>
> **Kurzfassung:** *Der Bug ist behoben. Subscribe auf `…ca102` trennt nicht mehr, der
> transparente Datenkanal steht bidirektional. Nachgewiesen mit einer automatisierten
> Testsuite: 12/12 Verbindungszyklen (Haltezeiten 0…300 s) und **3 × 4,2 MB Ymodem-Download
> über BLE mit 0 Retries** bei ~6,5 kB/s. Euer Befund war richtig, eure Ursachenvermutung
> aus Frage 4 war halb richtig — Details unten. Für euch ist es jetzt tatsächlich der von euch
> angekündigte „reine UUID-Tausch", aber es gibt **acht Punkte, die die App beachten muss**,
> sonst läuft es nicht stabil. Die sind hier vollständig und mit Zahlen belegt.*

---

## 1. Was jetzt geht — und wie man es sieht

Testlauf vom 2026-08-26, Board `26SC9993`, Firmware 5.93, unsere PC-Testsuite
(`sism555/Doku/tools/ble_mainboard_test.py`, Report
`Testreports/mainboard-module-suite-2026-08-26_11-57-14.json`):

| Phase | Transport | Ergebnis | Messwerte |
|---|---|---|---|
| BLE Smoke | BLE | **passed** | Connect 1383 ms · GATT-Discovery 2 ms · **Subscribe 39 ms** · PIN-Prompt gesehen · PIN → Hauptmenü ok · Verbindung 8 s gehalten, **kein Disconnect** |
| BLE Verbindung | BLE | **passed** | **12/12 Zyklen**, Haltezeiten 0/1/2/5/10/20/30/60/120/180/240/**300 s** — jedes Mal PIN + Menü ok |
| BLE Download 1–3 | BLE | **passed** | je **4 199 424 Byte** Ymodem, 632 / 679 / 642 s → **6639 / 6184 / 6537 B/s**, **Retries: 0 / 0 / 0** |
| Classic Download 1+3 | Classic | passed | 7024 / 7289 B/s, Retries 17 / 8 |
| Classic Download 2/3 | Classic | *failed* | siehe Abschnitt 5 — **Classic-only, nicht BLE** |

**Das ist der direkte Gegenbeweis zu eurem Retest #1:**

- Euer Symptom war „`subscribe …ca102 ok` → sofort `⚡`". Jetzt: Subscribe in 39 ms, danach
  kommt der **unaufgeforderte** Banner + PIN-Prompt über Notify, die Verbindung hält 300 s und
  länger. Genau euer Abnahmekriterium aus Abschnitt 5 eures Dokuments ist erfüllt.
- Der Datenkanal ist nachweislich **bidirektional**: die Testsuite navigiert allein über BLE
  durch PIN-Eingabe, Hauptmenü, Untermenüs und startet dort den echten Ymodem-Export. Ohne
  funktionierenden RX-Pfad wäre das unmöglich.
- 3 × 4,2 MB mit **null** Retries ist der stärkste Beleg — über BLE sogar fehlerfreier als über
  Classic SPP (dort 8–17 Retries).

**Wie ihr es selbst in Sekunden verifiziert:** subscriben auf `…ca102`, dann kommt ohne weitere
Aktion die Byte-Folge
`\r\n\r\nViasis 3003M - Version 5.94\r\nVia traffic controlling 2026\r\nPIN = `.
Steht `PIN = ` im Notify-Strom, ist der Kanal in beide Richtungen offen. Praktischer Nebeneffekt
für eure **Frage 1** aus Retest #1: die Firmware-Version steht in diesem Banner — sie ist damit
**jetzt über BLE ablesbar**, RS232 ist dafür nicht mehr nötig.

---

## 2. Ursache — euer Frage-4-Volltreffer, mit einer wichtigen Korrektur

Ihr habt in Frage 4 die richtige Leitung benannt: es war der **CYSPP-Pin**. Eure Schlussfolgerung
war aber, der Pin müsse für BLE **aktiv LOW getrieben** werden. Das ist falsch, und diese
Korrektur ist wichtig, weil sie auch für euer Verständnis des Verhaltens zählt.

Wir haben das am IF820-Entwicklungskit unabhängig durchgemessen, alle drei Zustände bei sonst
identischer Konfiguration:

| Zustand der Host-Leitung | Ergebnis |
|---|---|
| aktiv LOW getrieben | Fehler — kein nutzbarer Datenpfad |
| aktiv HIGH getrieben | Fehler — BLE-Writes erscheinen als EZ-Serial-GATT-Ereignisse, **nicht** als UART-Nutzdaten |
| **Eingang / hochohmig** | **Pass** — CCCD `0100`, UART↔BLE transparent in beide Richtungen |

Der richtige Zustand ist also **weder HIGH noch LOW, sondern hochohmig**. Unsere Firmware hat die
Leitung an vier Stellen aktiv HIGH getrieben (bei Reconfig, beim Trennen, beim Umschalten der
UART auf GSM/GPS und beim Deinstallieren des BT-Moduls) — jedes dieser Treiben beendete den
Datenkanal. Ein Detail, das uns lange getäuscht hat und das euch interessieren dürfte: **ein
gemessenes HIGH ist kein Beweis für den korrekten Zustand** — „aktiv HIGH" und „hochohmig" sind
beide als HIGH messbar, verhalten sich am Modul aber nachweislich unterschiedlich.

Behoben in den Firmware-Ständen 5.65 bis 5.87; danach kamen noch mehrere Puffer-Fixes
(5.89–5.93), die für die Stabilität der langen Downloads nötig waren. Eine
Hardware-Änderung war **nicht** erforderlich.

Zu euren übrigen Fragen aus Retest #1, kurz nachgezogen:

- **Frage 1 (Firmware-Stand der Einheiten):** euer Verdacht war berechtigt, die beiden Boards
  liefen nicht auf demselben Stand. Ab jetzt über den Banner prüfbar (s. o.).
- **Frage 2 (Boot-Release):** ja, erfolgt bei jedem Reset-Grund. War aber nicht das Problem.
- **Frage 3 (UART1-Owner):** relevant, aber anders als vermutet — siehe Punkt 8 unten, das ist
  ein Punkt, den die App aktiv behandeln muss.
- **Frage 5 (Classic vs. BLE gleicher Pin-Pfad):** ja, dieselbe physische Leitung, und ja, euer
  Einwand war berechtigt. Die Firmware unterscheidet Classic und BLE bis heute **nicht** — für
  euch ist das eine gute Nachricht, siehe Punkt 0 der Integrationsliste.

---

## 3. Was die App tun muss — die vollständige Liste

### 0. Grundsatz: Protokoll ist identisch zu Classic/RN4678

Die Firmware unterscheidet **nicht**, ob eine Verbindung über Classic SPP oder BLE-CYSPP
hereinkommt. `BT_LINK` ist ein einziges, transport-neutrales Signal („irgendein transparenter
Datenkanal ist aktiv"). Menü, PIN, Ymodem, Hotkeys — alles ist byteweise identisch. Euer Satz
„die Integration ist ein reiner UUID-Tausch" trifft also zu, mit den folgenden acht
Einschränkungen.

### 1. GATT: genau eine Characteristic benutzen

Verifizierte GATT-Tabelle (aus dem Testreport):

| Service | Characteristic | Properties | Verwendung |
|---|---|---|---|
| `65333333-a115-11e2-9e9a-0800200ca100` | `…ca101` | indicate, write | **nicht benutzen** |
| | **`…ca102`** | **notify, write-without-response** | **RX + TX — nur diese** |
| | `…ca103` | indicate | **nicht benutzen** (RX Flow Control) |
| `00001801-…` (Generic Attribute) | `00002a05` | read, indicate | **nicht abonnieren** |
| `ae5d1e47-5c13-43a0-8635-82ad38a1381f` | drei Chars | — | Modul-eigen, nicht benutzen |

- **Subscribe:** `setNotifyValue(true)` auf `…ca102` — das ist der Trigger, der die
  CYSPP-Bridge im Modul startet.
- **Senden:** `write(..., withoutResponse: true)` auf **dieselbe** `…ca102`.
- **Nicht** `…ca101` und `…ca102` parallel abonnieren oder beschreiben.
- Wenn ihr eine Service-Changed-Indication (`2A05`) oder einen CCCD-Wert `0200` sieht:
  **ignorieren**. Nur `0100` auf `…ca102` bedeutet „Datenpipe steht".

### 2. Erst auf `PIN = ` warten, dann senden

Die Firmware **verwirft ihren Empfangspuffer**, unmittelbar bevor sie den PIN-Prompt ausgibt.
Alles, was die App zwischen Subscribe und Prompt sendet, ist weg — ohne Fehlermeldung.

Ablauf, mit gemessenen Zeiten:

1. Subscribe → Modul startet die Bridge (~40 ms).
2. Firmware wartet **200 ms**, dann weitere **250 ms** (bewusste Entprellung).
3. Banner + `PIN = ` kommen als Notify.
4. **Ab jetzt** darf die App senden.

Also: nicht auf einen Timer triggern, sondern auf die Zeichenfolge `PIN = `.

### 3. PIN: Ziffern, kein CR/LF

- **Ableitung:** die ersten 2 Ziffern + die letzten 4 Ziffern der Seriennummer.
  `26SC9993` → **`269993`**. Die Seriennummer steht im Advertising-Namen.
- Nur ASCII-Ziffern `0`–`9` werden verarbeitet; **alles andere wird still verworfen** (kein
  Fehlversuch, keine Meldung). Ein angehängtes `\r` ist also harmlos, aber unnötig.
- **Kein Abschluss-Zeichen nötig und nicht erwünscht** — die PIN gilt, sobald die 6. Ziffer
  passt. Bitte nicht auf ein Echo von `\r\n` warten.
- Pro Ziffer sendet das Gerät ein `*` zurück — daran kann die App den Fortschritt anzeigen.
- Nach 6 falschen Ziffern kommt der Prompt neu. Keine Sperre, keine Verzögerung, unbegrenzte
  Versuche (der Zugangsschutz liegt bewusst auf App-Layer-Ebene, es gibt **keinen**
  BT-Passkey und **kein** OS-Pairing-Dialog — Security ist „Just Works", ohne Bonding).
- **Jede neue Verbindung verlangt die PIN erneut.**

### 4. Textkodierung: ISO-8859-1 / CP1252 — nicht UTF-8

Das Gerät sendet **kein UTF-8**. „Hauptmenü" enthält `0xFC` als Einzelbyte. Eine
UTF-8-Dekodierung in Dart wirft dort eine Exception oder liefert Ersatzzeichen. Bitte
`latin1.decode(...)` verwenden.

Zeilenenden sind immer `CRLF`. Bannertexte variieren mit Firmware-Version, OEM-Variante und
Sprache — **nicht auf feste Länge oder festen Wortlaut parsen**, sondern auf Marker wie `PIN = `
und `?` triggern.

### 5. Notify-Grenzen sind keine Nachrichtengrenzen

Das Modul paketiert konfiguriert mit maximaler Paketlänge **20 Byte**, End-Zeichen `CR`,
Idle-Wartezeit 1 ms. Konsequenz: eine Menüseite von 145 Byte kommt als ~8 Notifications an,
aufgeteilt an beliebigen Stellen. **Reassemblieren ist Pflicht**, ein Notification-Callback ist
kein Datensatz.

Umgekehrt gilt für eure Writes: **≤ 20 Byte pro Write**. Eure Log-Zeile `Mtu: 23` passt dazu —
eine größere MTU brächte für Notifications ohnehin nichts, weil das Modul auf 20 Byte
paketiert.

### 6. Datenrate: Fluss über Geräteantworten takten, nicht über einen Timer

Harte Grenzen der Gerätefirmware:

- Empfangs-Ringpuffer **256 Byte**.
- Die Menüverarbeitung liest maximal **16 Byte pro Hauptschleifendurchlauf**.
- Bei ~8000 B/s Dauerlast lief der Puffer im Test zuverlässig über und der Empfang stand still.

Deshalb: Bursts deutlich **unter 250 Byte** ohne Pause halten und den nächsten Block erst
senden, wenn die Antwort/das Echo des Geräts da ist. Zeitgesteuertes Senden ohne Rückkanal
funktioniert nicht zuverlässig.

### 7. Keine Binärdaten über den Kanal

Es gibt globale Einzeltasten-Hotkeys, die **unabhängig vom Menüzustand** feuern:
`H` (Hauptmenü), `R` (eine Ebene zurück), `T` (Datum/Zeit senden *oder laden*), `V`
(Geräteidentifikation), `ESC` (0x1B), `0x06`, `0x07`, im Testmenü zusätzlich `0x15`, `0x17`,
`X`, `D`.

Ein Binär-Payload trifft davon statistisch fast immer etwas und lässt das Gerät „von selbst"
durchs Menü wandern — das war bei uns eine ganze Bugklasse. Nur ASCII senden; Binärdaten
ausschließlich im Ymodem-Kontext (Abschnitt 4).

### 8. Verbindungsparameter, Timeouts und Pausen — der Teil, der am ehesten schiefgeht

| Punkt | Wert / Verhalten | Was die App tun muss |
|---|---|---|
| **Connect-Dauer** | gemessen **1,4 – 6,1 s** (Advertising ist bewusst nur Low-Duty mit 200 ms Intervall, High-Duty hat den Durchsatz zerstört) | Connect-Timeout **≥ 15 s**, nicht 2–3 s |
| **Supervision-Timeout** | Die Firmware setzt PPCP **nicht** — der Central (also ihr / das OS) bestimmt Intervall, Latency und Supervision-Timeout allein. In unseren Logs traten vom Central gewählte Werte von **1 s** bis 9,6 s auf. | Bewusst ein **großzügiges Supervision-Timeout (≥ 5 s)** und ein kurzes Intervall anfordern. 1 s ist zu knapp und war eine plausible Ursache früherer Abbrüche. |
| **Trennen** | CCCD-Rückschreiben auf `0x0000` löste bei uns reproduzierbar einen Disconnect mit **~5 s Verzögerung** aus | **Kein `stop_notify()` vor dem Disconnect.** Einfach die GATT-Verbindung schließen. Ein sauberes Trennen dauert dann ~130 ms. |
| **Reconnect** | Ein Reconnect **unmittelbar** nach einer langen Session schlug im Test einmal fehl, 9 s später klappte er (siehe Abschnitt 5) | Nach dem Trennen **einige Sekunden warten**, bevor neu verbunden wird. Keine Reconnect-Schleife ohne Backoff. |
| **Inaktivität** | Nach **6 Minuten** ohne empfangenes Byte trennt das Gerät aktiv | Gelegentlich ein harmloses Byte senden **oder** mit einem Reconnect rechnen und ihn sauber behandeln |
| **Gerät antwortet zeitweise nicht** | Wenn GSM/LTE oder GPS die interne UART übernehmen, wird die BT-Terminalausgabe **still suspendiert** und beim Rückweg wiederhergestellt. Die BLE-Verbindung bleibt dabei bestehen. | Antwortpausen von mehreren Sekunden **nicht** als Verbindungsabbruch werten. Timeouts auf Kommandoebene entsprechend großzügig. |
| **Gerätename** | BLE: `VIASIS_<serno>_BLE` · Classic: `VIASIS_<serno>` | Scan-Filter auf Prefix `VIASIS`; Seriennummer aus dem Namen extrahieren (→ PIN, s. Punkt 3) |

---

## 4. Ymodem-Datenexport — falls ihr den Messdaten-Download in die App holt

Über BLE nachgewiesen funktionsfähig: 4,2 MB in ~10,5 min, 0 Retries.

- **Navigation:** Hauptmenü → `1` (Datenausgabe) → auf die Ja/Nein-Abfrage mit **`C`**
  antworten (nicht `j`) — das startet den Ymodem-Export.
- **Headerblock:** `SOH`, Blocknummer `0`, 128 Byte Daten: Dateiname + Dateilänge in Byte.
  Danach erwartet die Firmware ein weiteres `C`.
- **Datenblöcke:** `STX`, Blocknummer + Einerkomplement, **1024 Byte**, dann CRC.
- **Prüfsumme:** CRC-16/XMODEM (Polynom `0x1021`, Init `0x0000`, MSB first).
- **Antwort:** `ACK` je Block, erwartet innerhalb **6 s**; `CAN` bricht ab.
- **Retries:** 8 pro Block, dann Abbruch.
- **Abschluss:** `EOT` (bis 5×), danach auf ein weiteres `C` hin der Ymodem-Nullblock.
- Dateiname-Schema wie gemessen: `SC_26SC9993_260826_034.VTF`.

Achtung: das ist der **einzige** Kontext, in dem Binärdaten über den Kanal laufen dürfen
(vgl. Punkt 7).

---

## 5. Der eine Fehlschlag im Testreport — betrifft euch nicht direkt, aber die Lehre schon

`Classic Download 2/3` ist nach 8,3 s mit „PIN-Prompt nicht gesehen; Download-Navigation wird
nicht gestartet" gescheitert. Randbedingungen:

- Rein **Classic-SPP** über einen Windows-COM-Port — kein BLE-Vorgang.
- Der Versuch startete **9 Sekunden** nach dem Ende eines 10-minütigen 4,2-MB-Downloads.
- Der Wiederholungsversuch 9 s später (11:14:42) lief vollständig durch (4,2 MB, 8 Retries).

Einordnung: sehr wahrscheinlich ein Wiederverbinden zu kurz nach einer langen Session (auf
Classic-Seite kommt der Windows-COM-Port-Neuaufbau dazu). **Die Lehre gilt aber auch für BLE**
und steht deshalb oben in Punkt 8: nach dem Trennen ein paar Sekunden Pause, kein sofortiger
Reconnect. Wir untersuchen den Classic-Fall separat weiter.

Zwei Klarstellungen zum Report, damit ihr ihn richtig liest:

- Das Feld `unexpected_disconnect: true` in der BLE-Smoke-Phase ist ein **Artefakt unseres
  Testskripts** — der Disconnect-Callback feuert auch bei unserem eigenen, gewollten Trennen am
  Ende. Die Verbindung hielt die vollen 8 s, siehe `disconnect_reason:
  hold_time_elapsed_still_connected`.
- Das Gesamtergebnis `failed` bezieht sich ausschließlich auf `Classic Download 2/3`. **Alle
  fünf BLE-Phasen sind passed.**

---

## 6. Was wir von euch brauchen

1. **Ein Retest mit der App** auf Board `26SC9993` — euer Diagnose-Block in
   `bluetoothPlus.dart` sollte jetzt `subscribe …ca102 ok` **ohne** folgendes `⚡` und danach den
   PIN-Prompt zeigen.
2. Bitte im Retest-Log mitschicken: die tatsächlich verhandelte **MTU**, das **Connection
   Interval** und das **Supervision-Timeout**, die iOS/Android ausgehandelt haben. Weil wir
   PPCP nicht setzen, bestimmt eure Seite diese Werte — und sie sind der wahrscheinlichste
   Grund, falls es bei euch trotzdem noch Abbrüche gibt.
3. Falls doch noch etwas abbricht: den **zeitlichen Abstand** zwischen letzter erfolgreicher
   Aktion und `⚡` in Millisekunden. Sofort (< 50 ms) deutet auf einen Protokollfehler,
   hunderte Millisekunden bis Sekunden auf die Verbindungsparameter aus Punkt 2.

---

## 7. Offene Punkte auf unserer Seite, die euch betreffen könnten

- **Durchsatz:** ~6,5 kB/s über BLE ist stabil, aber nicht optimiert. Zwei Optionen (Modul-Sleep
  im Datenmodus abschalten; Verbindungsintervall aktiv erzwingen) sind bekannt und im Backlog,
  bisher nicht umgesetzt. Falls euer Anwendungsfall mehr braucht, sagt Bescheid — dann
  priorisieren wir.
- **Transport-Erkennung** (Classic vs. BLE firmwareseitig unterscheiden) ist bewusst nicht
  implementiert. Für euch aktuell kein Nachteil (s. Punkt 0), aber die Firmware kann euch
  deshalb auch nicht mitteilen, über welchen Weg ihr sprecht.
- **Seltener Modul-Hänger:** die meisten „Modul antwortet nicht mehr"-Fälle waren ein eigener
  Puffer-Bug und sind mit 5.92/5.93 behoben. Ein älterer, ungeklärter Restfall bleibt: das Modul
  kann in einen Zustand fallen, aus dem nur ein längerer stromloser Power-Cycle des Geräts hilft
  (der Prozessor kann das Modul nicht selbst zurücksetzen — die Reset-Leitung ist am Mainboard
  unbeschaltet). Bitte behandelt einen nicht antwortenden Kanal robust: Timeout, klare
  Fehlermeldung an den Nutzer, kein endloses Warten.

---

*Quellen dieses Dokuments: Testreport
`sism555/Doku/tools/Testreports/mainboard-module-suite-2026-08-26_11-57-14.json` (+ `.html`),
Firmware-Quellcode `sism555/` Stand 5.94, `sism555/project-status.md` (Updates 5.65–5.94),
`sism555/Doku/ble-Dvk-request.md` (DVK-Referenzmessung),
`Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf`.*
