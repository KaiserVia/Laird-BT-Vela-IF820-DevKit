# Bluetooth PIN-Prozess beim RN4678 Modul

## Übersicht – Zweistufige Authentifizierung

Das System verwendet **zwei unabhängige Sicherheitsebenen**:

1. **Bluetooth-Pairing-Level** (RN4678 Modul-Hardware, BT-Protokoll)
2. **Firmware-Anwendungs-PIN** (Software-Zugangskontrolle über SPP-Kanal)

Die App muss zunächst eine BT-Verbindung zum Modul aufbauen (Ebene 1) und dann über den seriellen Kanal eine PIN eingeben (Ebene 2), bevor sie Zugriff auf das Gerätemenü erhält.

---

## Ebene 1: Bluetooth-Pairing (Modul-Level)

### Konfiguration bei Erstinitialisierung

Quellen: `btio.c: init_bluetooth()`, `btio.h`

Das RN4678-Modul wird bei der Erstinbetriebnahme mit folgenden Befehlen konfiguriert:

```
SA,2        → Authentifizierung: SSP "Just Works" (kein PIN beim Pairing)
SP,<PIN>    → Default-PIN gespeichert (6-stellig, aus Seriennummer berechnet)
SG,0        → Dual Mode (BLE + Classic gleichzeitig)
SS,SPP      → BT Classic Service Name: "SPP"
SY,4        → Maximale Sendeleistung
SO,,        → Statusmeldungen deaktiviert
SU,10       → Baudrate 307200
```

### Authentifizierungsmethode: SA,2 – SSP Just Works

| SA-Wert | Modus | Beschreibung |
|---------|-------|--------------|
| `SA,1` | SSP Confirm | 6-stellige PIN auf beiden Seiten angezeigt, Benutzer bestätigt |
| **`SA,2`** | **SSP Just Works** | **Kein PIN nötig ← im Projekt verwendet** |
| `SA,3` | SSP Input | PIN wird auf Remote-Seite angezeigt, lokal eingeben |
| `SA,4` | Legacy PIN | BT Classic 2.0, PIN über `SP`-Befehl |

**Bedeutung:** Das Pairing auf BT-Protokollebene erfolgt **ohne PIN-Eingabe**. Die App/das Smartphone verbindet sich per SPP (Serial Port Profile) – der Benutzer muss auf dem Handy lediglich "Verbinden" oder "Koppeln" bestätigen. Es wird keine PIN auf dem Bildschirm angezeigt oder abgefragt.

Die mit `SP,<PIN>` gesetzte PIN wird nur als **Fallback für Legacy-Geräte** (Bluetooth 2.0 ohne SSP) verwendet und spielt bei modernen Smartphones keine Rolle auf Pairing-Ebene.

### PIN-Berechnung aus Seriennummer

Quelle: `btio.c`, Zeile 95–98

```c
pin = atoi(fp.serno + 4);                   // letzte 4 Digits der Seriennummer
fp.btpin = atoi(fp.serno) * 10000 + pin;    // 6-stellig: komplette Seriennummer als Zahl
```

**Beispiel:** Seriennummer `"123456"` → `fp.btpin = 123456`

**Voraussetzungen:**
- `fp.serno[0]` muss eine Ziffer sein (Seriennummer gesetzt)
- Letzte 4 Digits dürfen nicht 0 ergeben
- Wenn eine dieser Bedingungen nicht erfüllt ist: Abbruch mit Fehlermeldung `"Viasis serial number not set"`

---

## Ebene 2: Firmware-PIN-Abfrage (Anwendungs-Level)

Dies ist die **eigentliche Zugangskontrolle** zum Gerätemenü über Bluetooth. Sie findet **nach** dem erfolgreichen BT-Pairing und SPP-Kanalaufbau statt.

### Auslöser: BT-Verbindungserkennung

Quelle: `libtool.c: communication_change()`, Zeile 1012 ff.

Die BT-Verbindung wird über einen **I2C-Expander (IC54)** erkannt. Wenn sich der BT_LINK-Status ändert (CTS-Signal vom RN4678):

```c
if ((comstate_neu ^ comstate) & BT_LINK & interfaces)  // BT-Statusänderung?
{
    if ((comstate_neu & BT_LINK) == 0)    // BT neu verbunden (Low-aktiv!)
    {
        // ... MQTT ggf. trennen ...
        
        if (Init_BT_ch(baud))             // UART1 + MUX auf BT konfigurieren
        {
            startmessage = 1;
            connect |= (UART1 | BT_LINK);     // Daten an UART1/BT senden
            bt_time = BT_TIMEOUT;              // 6 Minuten Inaktivitäts-Timeout
            
            if (fp.btmodem == Roving)
                bt_pininit = 1;                // *** PIN-Abfrage aktivieren! ***
        }
    }
}
```

### PIN-Prompt senden

Quelle: `sicom.c`, Zeile 1521

Sobald `bt_pininit` auf 1 gesetzt wird, ruft die Zeichenbearbeitung `mainmenu()` die Funktion `reset_bt_pin()` auf:

```c
void reset_bt_pin(void) 
{ 
    newline(); 
    putstr(T_pin);      // Sendet "PIN = " an die App
    menu = 0;           // Eingabe-Akkumulator reset
    bt_pininit = 1;     // Status: warte auf erste Ziffer
}
```

**Der Text `"PIN = "` wird über den SPP-Kanal an die App gesendet.**

### PIN-Eingabe und Verifikation

Quelle: `sicom.c: mainmenu()`, Zeile 2126–2148

Wenn `bt_pininit > 0`, wird jedes empfangene Zeichen als PIN-Eingabe interpretiert:

```c
else // bt_pininit > 0: Bluetooth Pinnummerabfrage aktiviert
{
    if (c == 'H') reset_bt_pin();      // 'H' = Reset der Eingabe
    else 
    {
        putc('*');                       // Echo: Stern zurück an App
        if (isdigit(c))
        {
            menu *= 10;                 // bisherige Ziffern × 10
            menu += (c & 0x0F);         // neue Ziffer addieren
        }
        bt_pininit++;                   // Zähler erhöhen

        if (fp.btpin == menu)           // Alle Ziffern korrekt?
        {
            putstr(T_main);             // Hauptmenü senden
            select('6', 0);             // Menüauswahl senden
            bt_pininit = 0;             // PIN-Sperre aufheben → Normalbetrieb
            menu = 0;
        }
        else if (bt_pininit > 6)        // Mehr als 6 Zeichen = Fehler
            reset_bt_pin();             // "PIN = " erneut senden
    }
}
```

### Besonderheiten der PIN-Logik

- Die PIN wird **zeichenweise** geprüft (nicht blockweise)
- Nach **jeder Ziffer** wird geprüft, ob der aktuelle Wert bereits mit `fp.btpin` übereinstimmt
- Theoretisch kann eine PIN auch kürzer als 6 Stellen erkannt werden, wenn die Zahl zufällig früher passt (z.B. PIN "000001" → nach der "1" erkannt)
- Nicht-Ziffern werden ignoriert (kein `menu`-Update), zählen aber als Eingabe (`bt_pininit++`)
- Nach 6 Eingabezeichen ohne Treffer: automatischer Reset und neue Abfrage
- `'H'` (Großbuchstabe) als Sonderzeichen: Reset der PIN-Eingabe jederzeit

---

## Kommunikationsprotokoll aus Sicht der Empfänger-App

### Verbindungsaufbau (einmalig beim ersten Mal)

| Schritt | Wer | Aktion |
|---------|-----|--------|
| 1 | App | Bluetooth-Scan → findet Gerät "VIASIS_<Seriennr>" |
| 2 | App | SPP-Verbindung anfordern |
| 3 | Smartphone OS | Pairing-Dialog: "Möchten Sie koppeln?" (kein PIN, da SA,2 Just Works) |
| 4 | Benutzer | Bestätigt Kopplung |
| 5 | RN4678 | SPP-Kanal geöffnet, CTS-Signal an MCU |
| 6 | Firmware | Erkennt Verbindung, setzt `bt_pininit=1` |
| 7 | Firmware → App | Sendet: `"\r\nPIN = "` |
| 8 | App → Firmware | Sendet 6 ASCII-Ziffern (z.B. `'1'` `'2'` `'3'` `'4'` `'5'` `'6'`) |
| 9 | Firmware → App | Echo pro Ziffer: `'*'` (6× Stern) |
| 10a | Firmware → App | **Erfolg:** Hauptmenü-Text + Menüauswahl (`select('6',0)`) |
| 10b | Firmware → App | **Fehler:** `"\r\nPIN = "` (erneute Abfrage) |

### Jede weitere Verbindung (bereits gepaart)

| Schritt | Wer | Aktion |
|---------|-----|--------|
| 1 | App | SPP-Verbindung aufbauen (Pairing bereits gespeichert, kein Dialog) |
| 2 | RN4678 | SPP-Kanal geöffnet, CTS-Signal an MCU |
| 3 | Firmware | Erkennt Verbindung, setzt `bt_pininit=1` |
| 4 | Firmware → App | Sendet: `"\r\nPIN = "` |
| 5 | App → Firmware | Sendet 6 ASCII-Ziffern |
| 6 | Firmware → App | Echo: `'*'` pro Ziffer |
| 7a | Firmware → App | **Erfolg:** Hauptmenü |
| 7b | Firmware → App | **Fehler:** Erneute PIN-Abfrage |

### Datenformat der Kommunikation

- **Baudrate:** 307200 bps (auf SPP-Ebene transparent)
- **Zeichenkodierung:** ASCII
- **Zeilenende:** `\r\n` (CRLF) bei Firmware-Ausgaben
- **PIN-Ziffern:** Einzelne ASCII-Zeichen `'0'`..`'9'` (0x30..0x39)
- **Kein Kommandoabschluss nötig:** Jede Ziffer wird sofort verarbeitet (kein CR/LF am Ende)

---

## Vergleich: Erstinitialisierung vs. Folgeverbindungen

| Aspekt | Erstinbetriebnahme | Jede weitere Verbindung |
|--------|-------------------|------------------------|
| BT-Pairing | Just Works, einmalig, Phone speichert Bonding | Automatisch (bereits gepaart) |
| Pairing-Dialog | Smartphone zeigt "Koppeln?"-Dialog | Keiner |
| SPP-Kanal | Wird geöffnet | Wird geöffnet |
| Firmware-PIN | **Muss eingegeben werden** | **Muss jedes Mal eingegeben werden** |
| PIN überspringbar? | **Nein** | **Nein** |
| Timeout | 6 Min. ohne Zeichen → Trennung | 6 Min. ohne Zeichen → Trennung |

**Wichtig:** Die Firmware-PIN wird **bei jeder einzelnen BT-Verbindung** abgefragt – auch bei bereits gekoppelten Geräten. Es gibt keinen Mechanismus zum "Merken" eines authentifizierten Geräts auf Firmware-Ebene.

---

## Timeout und Verbindungstrennung

Quelle: `hard.h`, Zeile 278; `main.c`

```c
#define BT_TIMEOUT  6    // 6 Minuten ohne Zeichenempfang → Trennung
```

- `bt_time` wird bei jeder neuen Verbindung auf `BT_TIMEOUT` (6) gesetzt
- Der Minutenzähler wird heruntergezählt
- Bei Zeichenempfang über BT wird `bt_time` wieder auf `BT_TIMEOUT` zurückgesetzt
- Erreicht `bt_time` den Wert 0: Verbindung wird getrennt

---

## Manuelle PIN-Änderung über Menü

Quelle: `btio.c: set_bt_pin()`, Zeile 395 ff.; Menüpunkt `0x0512`

Der Benutzer kann die PIN über das Bluetooth-Menü (oder Terminal) ändern:

1. Menüpfad: Hauptmenü → 5 (Bluetooth) → 2 (Pinnummer)
2. Firmware fragt: `"Pin = "` (mit `getline()` für 6 Ziffern)
3. Benutzer gibt neue 6-stellige PIN ein
4. Firmware sendet an RN4678:
   ```
   $$$         → CMD>
   SP,<PIN>\r  → AOK
   R,1\r       → Reboot
   $$$         → CMD>
   ---\r       → END
   ```
5. `fp.btpin` wird aktualisiert

**PIN-Länge beim RN4678:** Immer **6 Stellen** (beim Laird: 4 Stellen)

---

## Relevante Quellcode-Stellen

| Datei | Zeile | Funktion |
|-------|-------|----------|
| `btio.c` | 83–248 | `init_bluetooth()` – Erstinitialisierung RN4678 |
| `btio.c` | 95–98 | PIN-Berechnung aus Seriennummer |
| `btio.c` | 214 | `SA,2` Konfiguration (Just Works) |
| `btio.c` | 219–221 | `SP,<PIN>` Default-PIN setzen |
| `btio.c` | 395–453 | `set_bt_pin()` – Manuelle PIN-Änderung |
| `btio.h` | 46 | `#define Roving 46` – RN4678 Kennung |
| `btio.h` | 52 | `#define T_auth2 "SA,2\r"` – Just Works Kommando |
| `btio.h` | 54 | `#define T_sp "SP,"` – PIN-Setz-Kommando |
| `libtool.c` | 1012–1055 | `communication_change()` – BT-Verbindungserkennung |
| `libtool.c` | 1042 | `bt_pininit = 1` – PIN-Abfrage bei Roving aktivieren |
| `sicom.c` | 1521 | `reset_bt_pin()` – PIN-Prompt senden |
| `sicom.c` | 1542 | `mainmenu()` – Unterscheidung PIN-Modus vs. Normalbetrieb |
| `sicom.c` | 2126–2148 | PIN-Zeichenverarbeitung und Verifikation |
| `hard.h` | 278 | `BT_TIMEOUT` = 6 Minuten |
| `hard.h` | 399 | `fp.btpin` – gespeicherte PIN im Parameterblock |
| `hard.h` | 497 | `bt_pininit` – globale Variable PIN-Status |
| `sictxt.h` | 468 | `T_pin = "PIN = "` – Prompt-String |

---

## Zusammenfassung für App-Entwickler

1. **Verbindung:** SPP-Profil, "Just Works" Pairing (kein Pairing-PIN)
2. **Nach Verbindungsaufbau:** Warten auf String `"PIN = "` (oder `"\r\nPIN = "`)
3. **PIN senden:** 6 ASCII-Ziffern einzeln (Seriennummer des Geräts)
4. **Antwort prüfen:** 
   - Pro Ziffer kommt `'*'` zurück (Echo)
   - Nach korrekter PIN: Menütext (beginnt mit Hauptmenü)
   - Nach falscher PIN: erneut `"\r\nPIN = "`
5. **Sonderzeichen:** `'H'` resettet die PIN-Eingabe
6. **Timeout:** 6 Minuten ohne Datenverkehr → automatische Trennung
7. **PIN gilt pro Verbindung:** Bei jeder neuen SPP-Session muss die PIN erneut eingegeben werden
