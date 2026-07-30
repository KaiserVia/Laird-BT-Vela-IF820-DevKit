# IF820 / EZ-Serial: Konfigurations-Kandidaten zur Prüfung

> **Zweck:** Systematischer Durchgang durch die komplette EZ-Serial-Kommandoreferenz
> (`Documentation/EZ-GUIDE-EZ-SERIAL-VELA-IF820_v2_2.pdf`) mit Blick auf unseren Use-Case.
> **Nichts davon ist umgesetzt** — das ist eine Entscheidungsvorlage. Bitte pro Fall abhaken:
> übernehmen / verwerfen / später.
>
> **Stand:** 2026-07-30, nach Version 5.54. Quelle für alle Defaults: der Guide v2.2, wörtlich belegt.

## Unser Use-Case (zur Erinnerung)

Zwei Wege auf dasselbe Gerätemenü: (1) **Classic SPP** — Windows/HyperTerminal, Textmenü +
X/Y-Modem-Massendownload (MB, stop-and-wait, Durchsatz wichtig). (2) **BLE/CYSPP** — iOS-App,
CYSPP-Data-Char abonnieren, App-Layer-PIN, kein BT-Pairing. Betriebsbaud 460800, Flow F=01.
Teils **netzbetrieben** (Strom egal), teils **batteriebetrieben** (Strom relevant, aber dann
niemand per BT verbunden). Bereits gesetzt: Sniff aus (`SBTP,F=01`), CoD seriell
(`SBTDC,C=00001F00`), Baud/Flow (`STU`), Name (`SDN`).

## Grundregeln (gelten für alle Fälle unten)

Persistenz: entweder `/SCFG` (Snapshot aller Runtime-Werte → Flash) **oder** `$` am Befehl
(`SPEM$,M=0`). Die meisten Werte wirken **sofort im RAM**, `/RBT` ist zum Wirksamwerden **nicht**
nötig. Ausnahme `STU` (Baud/Flow) ist „protected". `/RFAC` (Factory Reset) setzt auch die Baud
auf Werk zurück → danach kein 460800 mehr, Vorsicht.

**Wichtige Grenze vorab:** Es gibt **kein** Kommando für MTU, Data-Length-Extension oder 2M-PHY.
Der BLE-Rohdurchsatz hängt an der iOS-seitig ausgehandelten MTU und am Connection-Interval —
modulseitig bleiben nur `.CYSPPSK` (Paketierung) und ein `/UCP`-Update-Request. PHY/DLE sind
nicht einstellbar.

---

## Umsetzungsstand (2026-07-30)

- **A1, A2, A5 — umgesetzt in Firmware 5.57** (selbstheilend im Init, Schritte 3d/3e/3f).
- **A3 — umgesetzt in 5.55/5.56** (dauerhaftes Advertising, Low-Duty-Intervall 3 s).
- **A4 — verworfen (Entscheidung 2026-07-30).** Die App liest die Seriennummer weiter aus dem
  Advertising-Namen; die PIN-gleiche Zahl wird **nicht** zusätzlich broadcastet. Auslöser:
  Sicherheitsfund, dass die Zugangs-PIN **die** numerische Seriennummer ist
  (`fp.btpin = atoi(serno)*10000 + atoi(serno+4)`).
- **PIN-Schema (PIN = Seriennummer):** wird **separat** behandelt (nicht Teil dieser Config-Liste).

---

## A. Klarer Gewinn — Empfehlung: übernehmen

### A1 — Echo im Command-Mode aus (`SPEM,M=0`)  ✅ umgesetzt (5.57)
- **Default:** M=1 (Echo an).
- **Vorschlag:** `SPEM,M=0`
- **Nutzen:** Der Guide empfiehlt es ausdrücklich für MCU-Hosts: der LPC muss dann nur Antworten/
  Events parsen, nicht sein eigenes Echo. Vereinfacht unser UART-Parsing.
- **Risiko:** keins. Betrifft nur den UART-Command-Mode, nicht den SPP-/CYSPP-Datenmodus.
- **Wirkungsbereich:** beide Geräteklassen, unkritisch.

### A2 — CYSPP-Paketierung an die Baud anpassen (`.CYSPPSK,W=01`)  ✅ umgesetzt (5.57)
- **Default:** M=1 (Anticipate), W=5 ms, L=20 Byte, E=0x0D.
- **Vorschlag:** `.CYSPPSK,M=1,W=01,L=14` (nur Wait 5 ms → 1 ms).
- **Nutzen:** Die Baud-Tabelle im Guide nennt für 460800 einen „Safe Wait" von **1 ms** für
  20-Byte-Pakete. Der Default 5 ms ist bei unserer Baud zu träge und bremst den BLE-Datenfluss
  Gerät→App unnötig.
- **Risiko:** gering; zu große Pakete würden ohnehin an die ausgehandelte MTU angepasst.
- **Wirkungsbereich:** nur BLE/CYSPP-Datenmodus. Batteriegeräte ohne Verbindung unberührt.

### A3 — BLE-Advertising dauerhaft (nur netzbetriebene Geräte) (`SAP …,D=0,O=0,F=01`)
- **Default:** Advertising stoppt nach ~90 s (High-Duty 30 s + Low-Duty 60 s), F=0x00.
- **Vorschlag (netzbetrieben):** `SAP,T=4,C=07,H=00A0,D=0,L=0140,O=0,F=01`
  (undirected low-duty, ~200 ms Intervall, Dauer unendlich, Auto-Advertising bei Boot/Disconnect).
- **Nutzen:** Die App findet das Gerät jederzeit, nicht nur im 90-s-Fenster nach dem Boot.
- **Risiko:** Dauer-Advertising kostet Strom → **nicht** pauschal für Batteriegeräte (siehe B2).
- **Wirkungsbereich:** getrennt parametrieren, Netz vs. Batterie.

### A4 — Seriennummer in die Manufacturer-Data (`.CYSPPSP …,L=<serno>`)  ❌ VERWORFEN (2026-07-30)
- **Default:** Company C=0x0131 (Cypress), local_key L=0 → **erklärt die beobachteten `00000000`**.
- **Vorschlag:** `.CYSPPSP,E=2,G=0,C=0131,L=<serno_hex>,R=0,M=0,P=1,S=0,F=2`
- **Nutzen:** Die Seriennummer erschiene im BLE-Advertising. Die App liest sie heute aber schon
  aus dem Namen `VIASIS_<serno>_BLE` — der Zusatznutzen ist also gering.
- **SICHERHEITSFUND (neu):** Die **Zugangs-PIN ist die numerische Seriennummer**
  (`fp.btpin = atoi(serno)*10000 + atoi(serno+4)`, geprüft in `btio.c`/`sicom.c`). Das `L`-Feld
  ist zudem der CYSPP-„local_key". A4 würde diese PIN-gleiche Zahl **roh** ins Advertising legen.
  Sie ist zwar aus dem Namen ohnehin ableitbar (wer die Formel kennt), aber roh gesendet wäre sie
  direkt lesbar. Deshalb: **Entscheidung durch den Nutzer nötig.**
- **Optionen:** (a) A4 überspringen (App nutzt weiter den Namen); (b) A4 mit der rohen Zahl (Info
  ist ohnehin quasi öffentlich); (c) A4 mit einer anderen Kodierung, die nicht der rohen PIN
  entspricht — dann muss die App die Kodierung kennen.
- **Unabhängiger Befund:** Dass die PIN aus der öffentlichen Seriennummer trivial ableitbar ist,
  ist ein schwaches Zugangsschema — sollte separat überdacht werden, egal wie A4 ausgeht.

### A5 — Just-Works explizit festschreiben (`SSBP …` + CYSPP `S=0`)  ✅ umgesetzt (5.57)
- **Default:** io=3 (No Input/No Output), BLE-Nibble 0x00 (no bond/no MITM), F=0x01 (auto-accept),
  CYSPP S=0 (kein authentifizierter Link) → **ist bereits unsere Wunschkonstellation**.
- **Vorschlag:** `SSBP,M=00,B=0,K=10,P=0,I=03,F=01` (und CYSPP `S=0` beibehalten).
- **Nutzen:** Verhindert, dass je ein OS-Pairing-Dialog erzwungen wird — genau das alte
  iOS/CCCD-Thema. Explizit festschreiben schützt gegen versehentliches Verstellen.
- **Risiko:** **Kein MITM und kein Legacy-PIN setzen** — das würde einen Passkey-Dialog verlangen.
- **Wirkungsbereich:** BLE-Zugang. Passt zur App-Layer-PIN.

---

## B. Abwägen — Entscheidung nötig

### B1 — Stärkere CYSPP-Bündelung für Bulk (`.CYSPPSK,L=80`)
- **Vorschlag:** `.CYSPPSK,M=1,W=05,L=80` (bis 128-Byte-Bündel).
- **Trade-off:** mehr Durchsatz beim Massendownload durch weniger GATT-Overhead, aber mehr Latenz;
  realer Gewinn hängt an der iOS-MTU/Connection-Interval, die wir nicht steuern. **Nur messen.**

### B2 — Advertising-Intervall bei Batteriegeräten
- **Trade-off:** kurzes Intervall = schneller gefunden, mehr Strom; langes Intervall (1–2 s) =
  weniger Strom, langsameres Finden. Für Batteriegeräte langes Intervall oder Adv nur bei Bedarf.
  Getrennt von A3 parametrieren.

### B3 — TX-Power für Classic-Reichweite (`STXP,P=8`)
- **Default:** P=7 = 10 dBm (für BLE bereits das Maximum).
- **Vorschlag:** `STXP,P=8` → +2 dB **nur für Classic** (BLE bleibt 10 dBm).
- **Trade-off:** etwas mehr Reichweite/Link-Margin gegen etwas mehr Sendestrom. Für BLE bringt es
  nichts. Nur sinnvoll, wenn die Classic-Reichweite real knapp ist.

### B4 — Deep Sleep für Batteriegeräte (`SSLP,L=2`)
- **Default:** L=1 (EPDS/normal sleep, kann noch BT/BLE empfangen).
- **Vorschlag geprüft:** L=2 (Hibernate) = 1,2 µA — **aber** deaktiviert BLE/Advertising komplett
  („no active resources until FW restarts") und selbst L=1 verlangt korrekte **LP_MODE-Pin**-
  Steuerung durch den LPC, sonst UART-Datenverlust.
- **Empfehlung:** nur mit sauberer LP_MODE-Verdrahtung. Ohne die Hardware: **nicht anfassen.**

### B5 — Privacy / Random Address (`SPRV`)
- **Default:** M=0x04 (Random Address an).
- **Trade-off:** Random Address wechselt je Verbindung die MAC. Nur ändern, falls die Flutter-App
  auf eine **feste MAC** filtert. Sonst Default lassen. **App-Team fragen.**

---

## C. Eher nicht / nur zur Kenntnis

- **`SFPK` (Fixed Passkey):** vom Guide als obsolet bezeichnet, würde Just Works brechen. Finger weg.
- **`SBTPIN` (Classic Legacy-PIN):** nur bei Legacy-PIN-Pairing relevant, das wir nicht nutzen.
- **MITM-Modi in `SSBP`:** erzwingen OS-Dialog — genau das, was wir vermeiden.
- **`SDA` (Appearance):** reiner BLE-Icon-Wert, kein App-Nutzen.
- **`/WUD`/`/RUD` (User-Flash 256 B):** könnte Seriennummer/Version modul-lokal ablegen, aber der
  LPC hält das ohnehin; Flash-Endurance 100.000 Zyklen. Nur relevant, falls A4 die Seriennummer
  modul-lokal spiegeln soll.
- **`SSLP,L=0` / `.CYSPPSP,P=0`:** würden nur Strom kosten ohne Nutzen.
- **`/RFAC` (Factory Reset):** nur als Reset-Weg. **Setzt auch die Baud auf Werk zurück** → danach
  kein 460800; nur bewusst einsetzen.
- **Nicht möglich (kein Kommando):** MTU, Data-Length-Extension, 2M-PHY.

---

## Vorschlag fürs Vorgehen

Wenn du zustimmst, würde ich **A1, A4, A5** in dieselbe selbstheilende Init-Logik einbauen wie
5.54 (prüfen → bei Bedarf setzen → `/SCFG`), analog zu Sniff/CoD. **A2/A3** ebenfalls, aber A3
gekoppelt an eine Geräteklasse (Netz vs. Batterie), sobald B2 entschieden ist. Die B-Fälle würde
ich erst nach einer Messung bzw. Rückfrage ans App-Team angehen. Was aus A/B soll rein?
