# Diagnose-Werkzeuge Bluetooth: Was und warum (Stand 2026-10-23)

Anlass: Die Auswertung aller Testreports (`BLE_Ursachensuche_Plan.md`) zeigt, dass beim Fehler "kein PIN-Prompt" das IF820 noch
keine Verbindung gemeldet hat, obwohl Windows "verbunden" meldet. Ob der PC zu spät verbindet oder das Modul, lässt sich mit
RS232-Mitschnitt und Testskript nicht klären. Dafür braucht es einen Blick auf die PC-Seite und auf den Funk.

## Übersicht

| Werkzeug | Sieht | Transport | Kosten | Klärt |
|---|---|---|---|---|
| `btvs.exe` (Microsoft) | HCI-Verkehr zwischen Windows und dem PC-Adapter | Classic + BLE | kostenlos | Wann Windows verbinden lässt, wann der Adapter "verbunden" meldet, Trenngründe, Adapterfehler |
| nRF52840-Dongle + Wireshark | Funkpakete zwischen PC und IF820 | nur BLE | ca. 10–15 € | Wer wann verbindet, wer trennt und warum, Verbindungsparameter |
| Android "Bluetooth HCI Snoop Log" | HCI-Verkehr im Handy | Classic + BLE | kostenlos | Dasselbe wie `btvs`, wenn das Handy statt des PCs testet |
| Ellisys / Teledyne LeCroy (Sodera, X240) | ganzes 2,4-GHz-Band | Classic + BLE + WLAN | grob fünfstellig, Miete möglich | alles, nur bei Classic-Problemen nötig |
| Ubertooth One | BLE, Classic nur teilweise | – | ca. 100–150 € | nicht empfohlen (verliert Classic-Verbindungen, kein EDR) |

**Empfehlung:** zuerst `btvs.exe` (kostenlos, sofort), dazu den nRF52840-Dongle für die BLE-Funkseite. Einen Classic-Sniffer nur
mieten, wenn Classic selbst Probleme macht (bisher stabil).

## nRF52840-Dongle (BLE-Sniffer)

**Was:** USB-Stick von Nordic Semiconductor (nRF52840 Dongle, PCA10059) mit Bluetooth-Chip. Mit der kostenlosen Firmware
"nRF Sniffer for Bluetooth LE" hört er passiv mit; er verbindet sich mit nichts. Anzeige in Wireshark (Plugin von Nordic).

**Warum:** Er zeigt das Verbindungspaket (`CONNECT_IND`) mit Zeitstempel, das Trennpaket mit Grund (z. B. Gegenseite beendet,
Supervision-Timeout) und die Verbindungsparameter (Intervall, Timeout). Damit ist "PC verbindet zu spät" von "Modul antwortet nicht"
eindeutig zu unterscheiden (Fehler F1), ebenso der Trenngrund beim Download-Abbruch (F2) und das 60-ms-Intervall von Board B (F4).

**Grenzen:** nur BLE. Muss der Verbindung ab dem Verbindungsaufbau folgen: Mitschnitt vor dem Verbinden starten.

**Hardware-Hinweise:**
- Der Original-Dongle hat **kein Gehäuse** (nackte Platine). Gehäuse gibt es von Drittanbietern; funktional gleichwertig ist der nackte Dongle.
- Nicht auf Metall legen (Kurzschluss an der Unterseite): Schrumpfschlauch, Kaptonband oder kleine Kunststoffschachtel reichen.
- Falls Gehäuse: nur Kunststoff (Antenne am Ende gegenüber dem Stecker) und der **Seitentaster** muss erreichbar bleiben (Bootloader zum Aufspielen der Sniffer-Firmware).
- **USB-Verlängerung 1–2 m** mitbestellen: Dongle neben Board und PC-Antenne legen, weg von USB-3-Buchsen (USB 3 stört das 2,4-GHz-Band).
- Varianten anderer Hersteller mit Gehäuse (z. B. Makerdiary nRF52840 USB Dongle) vor dem Kauf auf Unterstützung durch die Sniffer-Firmware prüfen.

**Einrichtung:** ca. 30 min einmalig (Firmware über nRF Connect for Desktop aufspielen, Wireshark-Plugin installieren).

## `btvs.exe` (Windows, Bluetooth Test Platform)

**Was:** Microsoft-Werkzeug aus der *Bluetooth Test Platform* (BTP), das den HCI-Verkehr zwischen Windows-Bluetooth-Stack und
Adapter mitschneidet und an Wireshark übergibt. Kein Funkmitschnitt, aber für Classic und BLE.

**Warum:** Zeigt, wann Windows dem Adapter den Verbindungsbefehl gibt und wann der Adapter "Verbindung steht" meldet. Damit lässt sich
prüfen, ob Windows an Python "verbunden" meldet, bevor die Verbindung wirklich steht. Außerdem Trenngründe und die Adapter-Aussetzer
(`BTHUSB` Ereignis 3).

**Hinweise:** Administratorrechte nötig; Windows lädt den Adapter dafür neu. Downloadquelle und genaue Bedienung noch nachzutragen.

## Bezug zu LTE

Die Lehren daraus (Diagnose-Zugänge von Anfang an im Layout vorsehen) sind für das LTE-Modul in
`C:\viaTraffic\Projekte\LTE-Modul-Vergleich\Diagnose-Zugaenge.md` festgehalten. Ein Funk-Sniffer wie für BLE ist bei LTE nicht
möglich (Fernmeldegeheimnis, Verschlüsselung); dort laufen Modem-Logs über USB/Debug-UART und AT-Abfragen.
