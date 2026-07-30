# Sicherer BT/BLE-Zugriff: 10 Konzepte (Entscheidungsvorlage)

> **Anlass:** Der heutige Zugangsschutz ist eine App-Layer-PIN, die **gleich der numerischen
> Seriennummer** ist (`fp.btpin = atoi(serno)*10000 + atoi(serno+4)`). Die Seriennummer steht
> öffentlich im Advertising-Namen `VIASIS_<serno>_BLE` → die PIN ist trivial ableitbar. Ziel:
> ein belastbares Schema, das Gerätename und Manufacturer-Data sinnvoll einbindet.
>
> **Stand:** 2026-07-30. Brainstorm/Entscheidungsvorlage, nichts umgesetzt.
> **Hinweis:** Ich bin kein Sicherheits-Auditor. Vor dem Ausrollen bitte ein echtes Security-Review.

## Die eine Wahrheit vorab

**Advertising-Daten (Name + Manufacturer-Data) werden ausgesendet und sind für jeden Scanner
lesbar.** Was dort steht, kann **niemals das Geheimnis sein**. Diese Felder eignen sich als:
- **Identität** (welches Gerät ist das → Schlüssel-Lookup), und
- **frisches Nonce-/Challenge-Material** (gegen Replay).

Das eigentliche Geheimnis (ein Schlüssel/PIN) muss **außerhalb** des Advertisings zum Client
kommen: per QR-Aufkleber, per Firmen-Backend nach Login, oder als BLE-Pairing-Passkey. Jeder
Ansatz unten, der diese Regel bricht, ist Theater — siehe Kasten am Ende.

## Bedrohungsmodell (wogegen schützen wir?)

| Angreifer kann… | Abwehr nötig |
|---|---|
| Advertising mitlesen (Name, Manuf-Data) | Kein Geheimnis ins Advertising |
| Sich verbinden und das App-Protokoll sprechen | Challenge-Response / Auth vor Menüfreigabe |
| Eine echte Sitzung über die Luft mitschneiden | Frische Nonce/Zeit → Replay-Schutz; ideal: Link verschlüsseln |
| Ein Gerät physisch ausbauen und Flash auslesen | Idealerweise kein flottenweites Geheimnis im Gerät |

## Was die Hardware hergibt

- **LPC1766:** aktuell **keine** Krypto-Lib (TLS läuft im Modem). Eine kompakte HMAC-SHA256
  (~300 Zeilen, wenige kB) ist problemlos ergänzbar (102 kB Flash frei).
- **RTC + Netz-Zeitsync** (NITZ/QLTS) vorhanden → zeitbasierte Verfahren möglich.
- **IF820-Modul:** BLE **LE Secure Connections** + Passkey (Krypto im Modul), `/QRND` (Zufall),
  `.CYSPPSP S=1` (authentifizierter Link erzwingbar).

---

## Zur naheliegenden Idee: „eine geheime Berechnungsvorschrift in Gerät und App"

Der intuitive Ansatz: Gerät und App rechnen beide `code = f(name, manuf_data)` mit einer geheim
gehaltenen Formel `f`. **Die Form ist genau richtig** — aber das Geheimnis steckt an der falschen
Stelle, und das ist ein grundlegender Unterschied:

- **Geheime Formel = „Security by Obscurity".** Die Formel steckt in **jeder** installierten App
  (Flutter-Binary auf tausenden iPhones, dekompilierbar) **und** in jeder Firmware. Sie ist damit
  nicht wirklich geheim — sie muss nur **einmal** von irgendjemandem aus der App oder dem Flash
  herausgezogen werden. Danach ist die **ganze Flotte für immer offen**, und du kannst die Formel
  nicht wechseln, ohne alle Apps und alle Geräte zu aktualisieren.
- **Geheimer Schlüssel = der richtige Weg (Kerckhoffs-Prinzip).** Nimm **dieselbe** Struktur —
  Gerät und App rechnen `code = HMAC(K, name ‖ manuf_data)` — aber die Stärke liegt in einem
  **Schlüssel K** (eine echte Zufallszahl), nicht in der Geheimhaltung der Methode. Selbst wenn
  jemand die App komplett dekompiliert, kommt er ohne K nicht rein. Und K lässt sich pro Gerät
  wählen und bei Bedarf wechseln.

**Der Unterschied ist winzig im Code, riesig in der Wirkung:** ersetze „geheime Formel" durch
„öffentliche Formel (HMAC) + geheimer Schlüssel". Genau das sind die Konzepte #1/#2. Wo K
herkommt (QR, Backend, Passkey) regeln #5/#6/#3.

Zwei Dinge, die auch bei der HMAC-Variante noch fehlen würden:
1. **Frische.** Sind `name` und `manuf_data` statisch, ist auch `HMAC(K, …)` statisch → jemand
   schneidet den Code einmal mit und spielt ihn beliebig oft ein (Replay). Deshalb muss ein
   **wechselndes** Element rein: eine rotierende Nonce in der Manuf-Data (#2), eine Geräte-Challenge
   (#1) oder die Zeit (#4).
2. **Schlüsselquelle.** K darf **nicht** aus der öffentlichen Seriennummer abgeleitet sein (sonst
   ist es das heutige Problem in grün). K muss echte Entropie haben und out-of-band kommen.

**Pragmatische Einordnung:** Wenn dein realer Gegner der neugierige Laie ist (nicht ein
motivierter Reverse-Engineer), hebt schon eine geheime Formel die Latte deutlich über „PIN =
Seriennummer". Aber die Obergrenze ist hart: **beim ersten Reverse-Engineering bricht alles, ohne
Rotationsmöglichkeit.** Für ein paar Zeilen mehr bekommst du mit HMAC+Schlüssel echte,
wechselbare Sicherheit — deshalb die klare Empfehlung dorthin.

---

## Die 10 Konzepte (grob von leicht → stark)

### 1. Challenge-Response mit HMAC, Challenge vom Gerät  ⭐ solide Basis
Bei Connect erzeugt das Gerät eine Zufalls-Challenge (per `/QRND`) und schickt sie an die App.
App antwortet `HMAC(K, challenge)`, Gerät verifiziert, erst dann Menüfreigabe.
- **Name/Manuf-Data:** Name = Geräte-ID für den Schlüssel-Lookup; Manuf-Data optional Key-ID/Version.
- **Secret K:** per-Gerät 128 bit, out-of-band (QR/Backend, siehe #5/#6).
- **Stärke:** stark gegen Replay (frische Challenge) und Brute-Force (HMAC).
- **Aufwand:** HMAC-SHA256 auf dem LPC ergänzen (klein). App: eine HMAC-Zeile.

### 2. Rotierende Nonce in der Manufacturer-Data + HMAC
Das Gerät legt einen rotierenden Zähler/Nonce (z. B. alle 5–30 s neu) in die Manuf-Data. Die App
liest ihn **schon aus dem Advertising**, sendet `HMAC(K, nonce)`.
- **Vorteil:** Handshake startet ohne Extra-Roundtrip (Nonce ist vor dem Connect bekannt).
- **Stärke:** etwas schwächer als #1 (Nonce ist öffentlich/teilweise vorhersehbar), mit Zeitfenster
  + Zähler aber ausreichend gegen Replay.
- **Aufwand:** wie #1, plus periodisches `.CYSPPSP`-Update der Manuf-Data.

### 3. BLE Secure Connections + Passkey — Krypto ins Modul auslagern  ⭐ wenig LPC-Aufwand
Statt App-Layer die BLE-Sicherheitsebene nutzen: `SSBP` auf **LE Secure Connections** (0x08) mit
Passkey. Der Link wird **verschlüsselt** und authentifiziert (MITM-Schutz).
- **Name/Manuf-Data:** nur Identität. Passkey out-of-band (QR/Aufkleber).
- **Stärke:** hoch — Link-Layer-Verschlüsselung schützt sogar die Nutzdaten vor Mitschnitt.
- **Preis:** ändert das bewusste „kein Pairing"-Design; die App muss den iOS-Pairing-Flow
  unterstützen. Gilt nur für BLE, nicht für Classic-SPP.

### 4. TOTP (zeitbasiert) — nutzt die vorhandene RTC
Gerät und App teilen K, beide rechnen `TOTP(K, Zeit)` (30-/60-s-Fenster). Manuf-Data trägt eine
grobe Gerätezeit zur Sync-Toleranz. App sendet den aktuellen Code, Gerät prüft im Fenster ±1.
- **Vorteil:** kein Roundtrip, App braucht kein Netz. Standard (RFC 6238).
- **Schwäche:** Uhrendrift bei Geräten **ohne** regelmäßige Zeitsync → Fenster großzügig wählen.
- **Aufwand:** HMAC + Zeitfenster-Logik.

### 5. Per-Device-Key vom Firmen-Backend  ⭐ skaliert ohne Aufkleber
`K = HMAC(Werks-Masterkey, serno)`. Der Name liefert die serno; die App holt K vom Firmen-Backend,
**nachdem sich der Nutzer dort eingeloggt hat** (nur das Backend kennt den Masterkey). Danach
Challenge-Response wie #1.
- **Vorteil:** skaliert über die ganze Flotte ohne per-Gerät-QR; Rechteentzug über das Backend.
- **Manuf-Data:** Key-Rotations-ID.
- **Kritisch:** der **Masterkey** ist der Kronjuwel — ein Leak kompromittiert **alle** Geräte.
  Braucht Online-Provisioning (App muss ans Backend kommen — evtl. offline im Feld ein Problem).

### 6. QR-Aufkleber mit per-Device-Key (Out-of-band-Bootstrap)  ⭐ robust, offline
Auf dem Gerät klebt ein QR mit einem **zufälligen 128-bit-Key** (nicht der Seriennummer!). Die App
scannt ihn → K → Challenge-Response (#1).
- **Vorteil:** sehr robust, **kein Backend/Netz** nötig, erzwingt physische Nähe (man muss am Gerät
  sein, um den QR zu scannen). Kein flottenweites Geheimnis.
- **Preis:** Produktionsschritt (QR pro Gerät drucken + in einer Liste registrieren); QR-Verlust =
  Neuprovisionierung.

### 7. Asymmetrisch / Signatur-Challenge — kein Geheimnis im Gerät  ⭐ höchste Robustheit
Das Gerät speichert nur den **öffentlichen** Schlüssel des Betreibers. Die App signiert die
Geräte-Nonce mit dem **privaten** Schlüssel (der nie das Gerät berührt). Gerät verifiziert.
- **Vorteil:** Wird ein Gerät ausgebaut und der Flash ausgelesen, ist **nichts Geheimes** drin →
  die Flotte bleibt sicher. Rechteentzug durch Schlüsselwechsel.
- **Manuf-Data:** Nonce + Key-ID.
- **Aufwand:** ECC-Verify (z. B. Ed25519/uECC) auf dem LPC — machbar, aber schwerer als HMAC.

### 8. Rolling Code (KeeLoq-artig), Zähler in der Manuf-Data
Gerät + App teilen K und einen **synchronen Zähler**; jede erfolgreiche Auth inkrementiert. Die
Manuf-Data broadcastet den aktuellen Zählerstand zur Resynchronisation.
- **Stärke:** verhindert Replay, sehr leichtgewichtig.
- **Schwäche:** schwächer als HMAC-Challenge; Zähler-Desync muss sauber abgefangen werden.

### 9. Zwei-Faktor: provisioniertes Secret + physischer Nonce
Name = Identität; ein **frischer Nonce** entsteht durch eine physische Aktion (Tastendruck am
Gerät, ein am Display angezeigter Wert) und/oder die Manuf-Data. Die App kombiniert das
provisionierte K mit dem frisch gelesenen Nonce.
- **Vorteil:** bindet den Zugriff an **physische Anwesenheit** → Fern-Angriffe scheitern.
- **Preis:** setzt eine physische Interaktionsmöglichkeit am Gerät voraus.

### 10. Echte Benutzer-PIN + Salt-in-Manuf-Data + PBKDF
Statt „PIN = Seriennummer" eine **echte, vom Betreiber gesetzte** PIN. Gerät rotiert einen Salt in
der Manuf-Data; Zugangscode = `PBKDF(user_pin, salt, serno)` mit vielen Iterationen.
- **Stärke:** macht Brute-Force teuer, Codes verfallen (Salt rotiert).
- **Schwäche:** nur so stark wie die gewählte PIN — schwache PINs bleiben schwach. Ist der
  minimal-invasive Fix, wenn das App-Layer-Modell bleiben soll.

---

## Vergleich

| # | Ansatz | Stärke | LPC-Aufwand | Netz/QR nötig | Link verschlüsselt |
|---|---|---|---|---|---|
| 1 | HMAC-Challenge (Gerät) | hoch | HMAC | Key-Provisioning | nein |
| 2 | Nonce in Manuf-Data + HMAC | mittel-hoch | HMAC | Key-Provisioning | nein |
| 3 | BLE LESC + Passkey | hoch | gering (Modul) | Passkey out-of-band | **ja** |
| 4 | TOTP | mittel-hoch | HMAC | Key-Provisioning | nein |
| 5 | Backend-Key aus serno | hoch* | HMAC | **Backend** | nein |
| 6 | QR-Key pro Gerät | hoch | HMAC | **QR** | nein |
| 7 | Signatur-Challenge (asym.) | sehr hoch | ECC-Verify | Public-Key-Deploy | nein |
| 8 | Rolling Code | mittel | klein | Key-Provisioning | nein |
| 9 | 2FA physisch + Secret | hoch | klein-mittel | phys. Faktor | nein |
| 10 | Benutzer-PIN + PBKDF | PIN-abhängig | KDF | PIN-Verteilung | nein |

\* #5 ist stark, solange der Masterkey nicht leakt.

## Empfehlung

- **Wenn der Link auch vertraulich sein soll** (Mitschnitt der Menü-/Downloaddaten verhindern):
  **#3 (BLE LESC + Passkey)**, Passkey per **#6 (QR)** provisioniert. Wenig LPC-Aufwand, echte
  Verschlüsselung. Gilt aber nur BLE — für Classic-SPP zusätzlich #1.
- **Wenn ihr beim App-Layer bleiben wollt** (ein Weg für BLE **und** Classic): **#1 (HMAC-Challenge)**
  als Kern, Schlüssel per **#6 (QR)** oder **#5 (Backend)**. Kompakte HMAC-SHA256 auf den LPC.
- **Wenn Diebstahl-/Auslese-Resistenz der Flotte oberste Priorität hat:** **#7 (Signatur-Challenge)**.
- **Minimal-invasiv, kurzfristig:** **#10** — wenigstens die PIN von der Seriennummer entkoppeln.

Der gemeinsame Nenner der guten Optionen: **Identität aus dem Namen, Frische aus einer Nonce
(Manuf-Data oder Geräte-Challenge), Geheimnis aus einer Out-of-band-Quelle (QR/Backend/Passkey),
Prüfung per HMAC/Signatur.**

## Was NICHT zählt (Sicherheitstheater — bewusst als Negativliste)

- Die Seriennummer „verschlüsseln"/XOR-en/als Prüfsumme ausgeben → der Angreifer liest den Namen ohnehin.
- Eine PIN aus öffentlich sichtbaren Feldern **ableiten** (genau das heutige Problem).
- Ein **fixes**, in allen Geräten gleiches Geheimnis im Flash (ein ausgelesenes Gerät bricht die Flotte).
- „Security by obscurity" (geheimes Format ohne echten Schlüssel) — hält keiner Analyse stand.
- Replay ohne Nonce/Zeit → mitgeschnittener Code funktioniert beliebig oft.

---

*Angelegt 2026-07-30 als Entscheidungsvorlage. Vor Umsetzung: Auswahl treffen +
Provisionierungsweg (QR vs. Backend vs. Passkey) festlegen + Security-Review.*
