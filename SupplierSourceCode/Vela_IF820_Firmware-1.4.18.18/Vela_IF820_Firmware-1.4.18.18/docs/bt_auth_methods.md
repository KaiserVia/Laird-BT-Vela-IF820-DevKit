# BT-Authentifizierung für IoT-Geräte — Alle 10 Methoden im Detail

---

## 1. Just Works (kein Schutz)

### Was passiert
BT Classic SSP mit IO-Capability "NoInputNoOutput" auf beiden Seiten.
Diffie-Hellman-Schlüsselaustausch, aber keine User-Bestätigung.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts. Factory Default = Just Works |
| **Host-MCU** | Nichts. Daten fließen direkt nach Connect |
| **App** | RFCOMM-Connect, fertig |
| **Cloud** | Nicht nötig |
| **Produktion** | Nichts |

### Ablauf
```
App → RFCOMM connect → IF820 → UART → MCU
     (sofort Daten möglich, keine Prüfung)
```

### Bewertung
- **Verschlüsselung:** Ja (ECDH)
- **MITM-Schutz:** Nein
- **Zugriffsschutz:** Keiner — jeder mit BT kann sich verbinden

---

## 2. Legacy PIN (sbtpin)

### Was passiert
BT Classic Legacy Pairing (pre-SSP). Fester PIN auf dem IF820, User tippt ihn bei Kopplung ein.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | `ssbp` auf Legacy-Modus setzen, `sbtpin,p=<hex>` setzen, `/scfg`, `/rbt` |
| **Host-MCU** | Nichts (PIN-Prüfung läuft auf BT-Layer, nicht auf App-Layer) |
| **App** | Beim Pairing: PIN eingeben (OS-Dialog) |
| **Cloud** | Nicht nötig |
| **Produktion** | PIN auf Gerät drucken oder in Anleitung beilegen |

### Ablauf
```
1. IF820 konfiguriert: sbtpin,p=3030323937 → PIN "00297"
2. User koppelt in Windows/Android/iOS → OS fragt nach PIN
3. User gibt "00297" ein → Pairing OK
4. Ab jetzt: RFCOMM connect ohne erneute PIN-Eingabe
```

### Bewertung
- **Verschlüsselung:** Ja (aber schwächerer Key-Exchange als SSP)
- **MITM-Schutz:** Schwach (PIN brute-forceable bei kurzer Länge)
- **Zugriffsschutz:** Nur wer PIN kennt, kann koppeln
- **Problem:** PIN ist für alle Geräte gleich ODER individuell (Verwaltungsaufwand)

---

## 3. SSP Numeric Comparison

### Was passiert
SSP mit IO-Capability "DisplayYesNo" auf beiden Seiten. Beide zeigen eine 6-stellige Zahl, User bestätigt dass sie übereinstimmen.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | `ssbp,m=51,b=0,k=10,p=0,i=01,f=01` (IO=DisplayYesNo, MITM) |
| **Host-MCU** | `EVENT_SMP_PASSKEY_DISPLAY_REQUESTED` von IF820 lesen → Zahl auf Display/LED anzeigen |
| **App** | OS zeigt 6-stellige Zahl → User bestätigt "stimmt überein" |
| **Cloud** | Nicht nötig |
| **Produktion** | Gerät braucht ein Display oder LED-Anzeige für die Zahl |
| **Hardware** | Display/7-Segment/OLED am Gerät ERFORDERLICH |

### Ablauf
```
1. App initiiert Pairing
2. IF820 → EVENT_SMP_PASSKEY_DISPLAY_REQUESTED → MCU zeigt "482917" auf Display
3. App/OS zeigt "482917" → User tippt "Ja, stimmt überein"
4. Pairing abgeschlossen, MITM-geschützt
```

### Bewertung
- **Verschlüsselung:** Ja (ECDH)
- **MITM-Schutz:** Ja (Numeric Comparison verhindert Man-in-the-Middle)
- **Zugriffsschutz:** Nur wer physisch am Gerät steht, kann bestätigen
- **Problem:** Gerät braucht Display — nicht für headless Geräte geeignet

---

## 4. Shared Secret (1 für alle Geräte)

### Was passiert
Nach BT-Connect (Just Works) sendet die App ein festes Passwort. MCU prüft.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Host-MCU** | Secret im Flash speichern. Nach `EVENT_BT_CONNECTED`: Timer starten, auf `AUTH:<secret>` warten, validieren, `AUTH:OK` oder `AUTH:FAIL` senden |
| **App** | Nach RFCOMM-Connect sofort `AUTH:<secret>\n` senden, auf Antwort warten |
| **Cloud** | Nicht nötig |
| **Produktion** | Secret einmalig in MCU-Firmware kompilieren |

### Ablauf
```
1. App → RFCOMM connect (Just Works)
2. App → sendet "AUTH:geheim123\n"
3. IF820 → leitet an UART weiter
4. MCU → empfängt, vergleicht mit "geheim123" im Flash
5. MCU → sendet "AUTH:OK\n" über UART → IF820 → App
6. Datenverkehr freigegeben
```

### Bewertung
- **Verschlüsselung:** Ja (BT-Layer + Secret nie im Klartext ohne BT-Verschlüsselung)
- **Zugriffsschutz:** Ja, aber alle Geräte haben gleiches Secret
- **Problem:** App-Reverse-Engineering → Secret → alle 10.000 Geräte offen

---

## 5. Individuelles Secret pro Gerät (MCU-Flash)

### Was passiert
Jedes Gerät bekommt bei der Produktion ein eigenes, zufälliges Secret in den MCU-Flash geschrieben.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Host-MCU** | Individuelles Secret im Flash. Auth-Logik wie bei #4 |
| **App** | Muss irgendwie das richtige Secret für dieses Gerät kennen (QR-Code, Cloud-Abfrage, oder User-Eingabe) |
| **Cloud** | Optional: Datenbank mit {Seriennummer → Secret} |
| **Produktion** | Jedes Gerät: Secret generieren → in MCU flashen → in DB speichern → QR-Code drucken |

### Ablauf
```
Produktion:
  Gerät #4711 → Secret "Xk9pQ2wR" → MCU-Flash + Aufkleber/QR

Inbetriebnahme:
  1. User scannt QR-Code → App kennt Secret für Gerät #4711
  2. App → RFCOMM connect
  3. App → "AUTH:Xk9pQ2wR\n"
  4. MCU → vergleicht → AUTH:OK
```

### Bewertung
- **Verschlüsselung:** Ja
- **Zugriffsschutz:** Individuell pro Gerät
- **Problem:** MCU-Flash ist mit Aufwand auslesbar (JTAG, Glitching)
- **Mitigation:** Read Protection aktivieren (RDP bei STM32, CRP bei NXP)

---

## 6. Derived Key (HMAC aus Master Key + Geräte-ID)

### Was passiert
Kein individuelles Secret wird gespeichert. Stattdessen berechnen beide Seiten das Secret aus einem Master Key + einer öffentlichen ID (z.B. Seriennummer).

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Host-MCU** | Master Key im Flash. Berechnet: `secret = HMAC-SHA256(master_key, eigene_seriennummer)`. Auth-Logik wie #4 |
| **App** | Kennt Master Key (eingebaut). Liest Seriennummer (aus BT-Name, Advertising, oder erstes Paket vom Gerät). Berechnet `secret = HMAC-SHA256(master_key, seriennummer)`. Sendet Auth |
| **Cloud** | Nicht nötig |
| **Produktion** | Master Key einmalig in Firmware kompilieren. Seriennummer pro Gerät setzen (kann MAC sein) |

### Ablauf
```
1. App liest Geräte-ID (z.B. aus BT-Gerätename "EZ-Serial 4711")
2. App berechnet: HMAC-SHA256("MasterKeyXYZ", "4711") → "a7b3c9..."
3. App → RFCOMM connect
4. App → "AUTH:a7b3c9...\n"
5. MCU berechnet: HMAC-SHA256("MasterKeyXYZ", "4711") → "a7b3c9..."
6. MCU vergleicht → Match → AUTH:OK
```

### Bewertung
- **Verschlüsselung:** Ja
- **Zugriffsschutz:** Individuell pro Gerät (jedes hat anderes Secret)
- **Kein Provisioning pro Gerät nötig** (nur Master Key + Seriennummer)
- **Problem:** Master Key in App → Reverse-Engineering → alle Geräte kompromittiert
- **Mitigation:** App-Obfuskierung, oder Master Key nur im Backend (→ Methode #7)

---

## 7. QR-Code + Cloud-Backend

### Was passiert
Secret wird bei Produktion generiert und NUR auf dem Gerät (MCU) + im Cloud-Backend gespeichert. Die App fragt das Backend.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Host-MCU** | Secret im geschützten Flash. Auth-Logik wie #4 |
| **App** | 1) QR-Code scannen ODER 2) Backend fragen: "Gib mir Secret für Seriennummer X" (mit User-Login) |
| **Cloud** | REST-API: `GET /device/{serial}/secret` (nur für authentifizierte App-User). Datenbank: {serial → secret} |
| **Produktion** | Secret generieren → MCU-Flash + Cloud-DB + QR-Code auf Aufkleber |

### Ablauf
```
Produktion:
  Gerät #4711 → Secret "Xk9pQ2wR"
  → MCU-Flash
  → Cloud-DB: {4711: "Xk9pQ2wR"}
  → QR-Code auf Gehäuse: "4711:Xk9pQ2wR"

Inbetriebnahme (Option A — QR):
  1. User scannt QR → App hat Secret
  2. App → RFCOMM → AUTH:Xk9pQ2wR → MCU → OK

Inbetriebnahme (Option B — Cloud):
  1. App kennt Seriennummer (BT-Scan)
  2. App → HTTPS → Backend: "Secret für 4711?"
  3. Backend prüft User-Berechtigung → gibt Secret zurück
  4. App → RFCOMM → AUTH:Xk9pQ2wR → MCU → OK
```

### Bewertung
- **Verschlüsselung:** Ja
- **Zugriffsschutz:** Hoch — Secret nie dauerhaft in der App
- **Key-Rotation möglich:** Backend kann neues Secret pushen
- **Problem:** App braucht Internet beim Pairing (oder QR als Fallback)
- **Problem:** Cloud-Infrastruktur muss betrieben werden

---

## 8. Secure Element (ATECC608B, OPTIGA, SE050)

### Was passiert
Ein dedizierter Crypto-Chip auf dem PCB speichert den Key hardware-geschützt. Der Key verlässt den Chip NIE. Die MCU fragt den Chip: "Berechne HMAC für diese Daten" — bekommt Ergebnis, aber nie den Key.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Secure Element** | Speichert Key (bei Produktion provisioniert). Berechnet HMAC/Signatur auf Anfrage |
| **Host-MCU** | I²C-Kommunikation mit SE. Nach BT-Connect: Nonce generieren, an App senden, Response von App an SE zur Verifikation geben |
| **App** | Empfängt Challenge (Nonce). Berechnet Response mit eigenem Secret (aus QR/Cloud). Sendet Response zurück |
| **Cloud** | Optional: Provisioning-Backend für Key-Verteilung |
| **Produktion** | SE auf PCB löten. Key in SE provisionieren (Microchip/Infineon bieten Services). Gleichen Key in Cloud/QR hinterlegen |
| **Hardware** | ATECC608B/OPTIGA auf PCB + I²C-Leitungen zur MCU |

### Ablauf
```
1. App → RFCOMM connect
2. MCU generiert Nonce (Zufallszahl): "7f3a9b..."
3. MCU → IF820 → App: "CHALLENGE:7f3a9b...\n"
4. App berechnet: HMAC-SHA256("DeviceSecret", "7f3a9b...") → "c4d8e1..."
   (Secret aus QR-Code oder Cloud)
5. App → IF820 → MCU: "RESPONSE:c4d8e1...\n"
6. MCU → fragt SE über I²C: "Berechne HMAC von 7f3a9b..."
7. SE → MCU: "c4d8e1..."
8. MCU vergleicht App-Response mit SE-Ergebnis → Match → AUTH:OK
```

### Bewertung
- **Verschlüsselung:** Ja (BT-Layer + App-Layer)
- **Zugriffsschutz:** Sehr hoch — Key nicht aus Hardware extrahierbar
- **Auch wenn MCU kompromittiert:** Key bleibt im SE sicher
- **Problem:** Zusätzliche Hardware + PCB-Redesign + Provisioning-Prozess
- **Problem:** Höhere Stückkosten (0,50–1,50 € pro Chip)

---

## 9. Secure Element + Cloud Challenge-Response

### Was passiert
Maximale Sicherheit: NIEMAND außer dem SE (Gerät) und dem Cloud-HSM (Backend) kennt den Key. Nicht die MCU, nicht die App.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Secure Element** | Key gespeichert. Berechnet HMAC auf Anfrage |
| **Host-MCU** | I²C mit SE. Generiert Nonce, sendet Challenge, empfängt Response, lässt SE verifizieren |
| **App** | Empfängt Challenge. Leitet an Cloud weiter. Empfängt Response von Cloud. Sendet an Gerät |
| **Cloud** | HSM oder Key-Vault. Empfängt {device_id, nonce} → berechnet HMAC mit dem Key des Geräts → gibt Response zurück |
| **Produktion** | Key in SE + gleichen Key in Cloud-HSM provisionieren (paarweise) |
| **Hardware** | SE auf PCB + I²C |

### Ablauf
```
1. App → RFCOMM connect
2. MCU → Nonce: "7f3a9b..."
3. MCU → App: "CHALLENGE:7f3a9b...\n"
4. App → HTTPS → Cloud: {device: "4711", nonce: "7f3a9b..."}
5. Cloud-HSM berechnet: HMAC-SHA256(key_4711, "7f3a9b...") → "c4d8e1..."
6. Cloud → App: {response: "c4d8e1..."}
7. App → MCU: "RESPONSE:c4d8e1...\n"
8. MCU → SE (I²C): "Berechne HMAC von 7f3a9b..."
9. SE → MCU: "c4d8e1..."
10. MCU: Response von App == SE-Ergebnis → AUTH:OK
```

### Bewertung
- **Verschlüsselung:** Maximal (BT + HTTPS + Hardware-Key)
- **Zugriffsschutz:** Maximal — selbst bei vollem Zugriff auf App/MCU kein Key extrahierbar
- **Key-Rotation:** Möglich (Cloud + SE reprovisionieren)
- **Problem:** Höchster Aufwand, App MUSS Internet haben, Cloud-Ausfall = kein Auth möglich
- **Mitigation:** Offline-Fallback mit zeitlich begrenztem Token (wie OAuth)

---

## 10. MCU mit Read Protection (RDP/CRP)

### Was passiert
Kein Extra-Chip, aber der MCU-Flash wird hardware-seitig gegen Auslesen geschützt. JTAG/SWD wird deaktiviert.

### Was muss wo gemacht werden

| Komponente | Aufgabe |
|---|---|
| **IF820** | Nichts (transparente Pipe) |
| **Host-MCU** | Secret im Flash (wie #5). ZUSÄTZLICH: Read Protection aktivieren |
| **App** | Auth wie bei #4/#5 |
| **Cloud** | Optional (wie #7 für Secret-Verteilung) |
| **Produktion** | Secret flashen → RDP/CRP Level setzen (ACHTUNG: danach kein Debugging mehr!) |

### MCU-spezifische Read Protection

| MCU-Familie | Mechanismus | Level |
|---|---|---|
| **STM32** | RDP (Read-Out Protection) | Level 0: offen, Level 1: kein Debug-Lesen, Level 2: permanent gesperrt |
| **NXP LPC** | CRP (Code Read Protection) | CRP1: kein ISP-Lesen, CRP2: kein Schreiben, CRP3: permanent |
| **Nordic nRF** | APPROTECT | Aktiviert: kein Debug-Zugriff |
| **ESP32** | Flash Encryption + Secure Boot | Flash verschlüsselt, nur signierte Firmware bootbar |

### Ablauf
```
Produktion:
  1. Firmware + Secret flashen
  2. RDP Level 2 setzen (STM32) oder CRP3 (NXP)
  3. Ab jetzt: Flash NICHT mehr per JTAG/SWD auslesbar
  4. ACHTUNG: Auch kein Debugging mehr möglich!

Runtime:
  Wie #4 oder #5 — normaler Auth-Handshake
```

### Bewertung
- **Verschlüsselung:** Ja (BT-Layer)
- **Zugriffsschutz:** Mittel-Hoch (Flash nicht trivial auslesbar)
- **Kein Extra-Chip nötig** — nur MCU-Konfiguration
- **Problem:** Voltage-Glitching kann RDP bei manchen MCUs umgehen (~1000 € Equipment, Expertise nötig)
- **Problem:** Kein Debugging nach Aktivierung — Firmware muss fehlerfrei sein
- **Problem:** Firmware-Update wird komplexer (Secure Bootloader nötig)

---

## Aufwandsübersicht (Entwicklung + Produktion)

| # | Methode | MCU-Entwicklung | App-Entwicklung | Produktion (pro Gerät) |
|---|---|---|---|---|
| 1 | Just Works | 0 h / 0 € | 0 h / 0 € | 0 € |
| 2 | Legacy PIN | 2–4 h / 200 € | 0 h (OS-Dialog) | ~0,10 € (PIN drucken) |
| 3 | SSP Numeric Comparison | 8–16 h / 1.500 € | 0 h (OS-Dialog) | 5–15 € (Display-Hardware) |
| 4 | Shared Secret (1 für alle) | 8–16 h / 1.500 € | 4–8 h / 800 € | 0 € |
| 5 | Individuelles Secret | 8–16 h / 1.500 € | 8–16 h / 1.500 € | 0,50–2 € (QR + DB-Eintrag) |
| 6 | Derived Key (HMAC) | 16–24 h / 2.500 € | 8–16 h / 1.500 € | 0 € |
| 7 | QR + Cloud-Backend | 8–16 h / 1.500 € | 24–40 h / 4.000 € | 0,50–2 € (QR) + Cloud ~50 €/Monat |
| 8 | Secure Element | 40–80 h / 8.000 € | 16–24 h / 2.500 € | 1–3 € (SE-Chip + Provisioning) |
| 9 | SE + Cloud Challenge | 60–120 h / 12.000 € | 40–60 h / 6.000 € | 2–5 € + Cloud ~100 €/Monat |
| 10 | MCU Read Protection | 4–8 h / 500 € | 0 h | 0,10 € (RDP-Schritt in Flasher) |

> **Hinweis:** Stundenangaben = reine Implementierung ohne Test/Debug. Reale Projekte: ×1,5–2.

---

## Kombinationsempfehlung

Die Methoden sind **kombinierbar**:

| Kombination | Sicherheitslevel | Typischer Einsatz |
|---|---|---|
| #1 (Just Works) allein | Niedrig | Entwicklung, Labortests |
| #4 + #10 | Mittel | Kostengünstige Consumer-Produkte |
| #5 + #7 + #10 | Hoch | IoT-Produkte mit App |
| #8 + #9 | Maximal | Medizin, Automotive, Industrie |

---

## Empfehlung: Einfach, sicher, langlebig (10+ Jahre)

### Empfohlene Kombination: **#5 (Individuelles Secret) + #10 (Read Protection)**

| Kriterium | Bewertung |
|---|---|
| **Aufwand MCU** | ~16–24 h einmalig (Auth-Handshake + RDP-Setup) |
| **Aufwand App** | ~8–16 h einmalig (QR-Scan + Auth-Logik) |
| **Produktion** | ~0,50 € pro Gerät (QR-Aufkleber) |
| **Laufende Kosten** | 0 € (keine Cloud nötig!) |
| **Langlebigkeit** | ✓ Kein Server der abgeschaltet werden kann |
| **10-Jahres-Sicherheit** | ✓ Secret pro Gerät = 1 kompromittiertes Gerät ≠ alle offen |
| **Einfachheit** | ✓ Kein Internet, kein Backend, kein Extra-Chip |

### Warum genau diese Kombination?

1. **Keine Cloud-Abhängigkeit** — In 10 Jahren existiert euer Backend vielleicht nicht mehr. QR-Code auf dem Gerät funktioniert IMMER.
2. **Individuelles Secret** — Selbst wenn jemand ein Gerät knackt, sind die anderen 9.999 sicher.
3. **Read Protection** — Flash nicht trivial auslesbar (Glitching-Angriff kostet ~1.000 € + Expertise = für 99% der Angreifer zu teuer).
4. **Kein Wartungsaufwand** — Einmal implementiert, läuft ewig. Keine Server-Updates, keine Zertifikats-Erneuerung.
5. **App bleibt einfach** — QR scannen, Secret merken, fertig. Keine Login-Flows, keine Token-Refreshes.

### Ablauf im Feld

```
Produktion:
  Gerät #4711 → Secret zufällig generiert → MCU-Flash + QR-Aufkleber
  MCU: RDP Level 1 (STM32) oder APPROTECT (nRF) aktiviert

10 Jahre später, Inbetriebnahme:
  1. Techniker scannt QR-Code am Gerät mit App
  2. App speichert Secret lokal (verschlüsselt)
  3. App → BT RFCOMM → "AUTH:<secret>\n"
  4. MCU vergleicht → AUTH:OK
  5. Fertig. Funktioniert offline, ohne Internet, ohne Cloud.
```

### Detaillierter Produktionsprozess

#### Übersicht: Was muss pro Gerät passieren?

```
┌─────────────────────────────────────────────────────────────────┐
│  Produktionsschritt pro Gerät (Taktzeit ~30–60 Sekunden)        │
├─────────────────────────────────────────────────────────────────┤
│  1. Firmware flashen (normal, wie bisher)                       │
│  2. Secret generieren (zufällig, 16–32 Bytes)                  │
│  3. Secret in MCU-Flash schreiben (eigener Flash-Sektor)        │
│  4. Read Protection aktivieren (RDP/APPROTECT)                  │
│  5. QR-Code drucken + auf Gerät kleben                          │
└─────────────────────────────────────────────────────────────────┘
```

#### Schritt 1: Firmware flashen

| Aspekt | Detail |
|---|---|
| **Was** | Normale Firmware inkl. Auth-Logik auf MCU flashen |
| **Wie** | SWD/JTAG-Programmer (J-Link, ST-Link, etc.) |
| **Besonderheit** | Firmware ist für ALLE Geräte identisch (kein individuelles Build) |
| **Dauer** | ~5–10 s |

#### Schritt 2: Secret generieren

| Aspekt | Detail |
|---|---|
| **Was** | 16 Bytes (128 Bit) kryptographisch zufälliges Secret erzeugen |
| **Wo** | Auf dem Produktions-PC (Python-Script) |
| **Wie** | `os.urandom(16)` → Hex-String z.B. `"a7b3c9f1e2d4..."` |
| **Wichtig** | CSPRNG verwenden, NICHT `random.random()` |

```python
# Produktions-Script (Beispiel)
import os
secret = os.urandom(16).hex()  # z.B. "a7b3c9f1e2d456789abcdef012345678"
```

#### Schritt 3: Secret in MCU-Flash schreiben

| Aspekt | Detail |
|---|---|
| **Was** | Secret in einen dedizierten Flash-Sektor der MCU schreiben |
| **Wie** | Über SWD/JTAG NACH dem Firmware-Flash, BEVOR RDP aktiviert wird |
| **Wo im Flash** | Feste Adresse, z.B. letzter Sektor (bei STM32F4: `0x080E0000`) |
| **Format** | 16 Bytes raw + 4 Bytes CRC32 als Integritätscheck |

**Option A: Direkt per Programmer-Script**
```python
# Secret an feste Flash-Adresse schreiben (J-Link/pyOCD)
import pyocd
session = pyocd.core.helpers.ConnectHelper.session_with_chosen_probe()
target = session.board.target
target.write_memory_block8(0x080E0000, bytes.fromhex(secret))
```

**Option B: Über UART-Bootloader (STM32)**
```bash
# STM32 built-in UART bootloader
stm32flash -w secret.bin -S 0x080E0000 /dev/ttyUSB0
```

**Option C: Über IF820 UART-Pipe (kein extra Programmer nötig!)**
```
Falls MCU einen eigenen "Provisioning-Modus" hat:
1. MCU startet im Provisioning-Mode (z.B. Pin high bei Boot)
2. Produktions-PC → IF820 UART → MCU: "PROVISION:<secret>\n"
3. MCU schreibt Secret in Flash, antwortet "PROVISION:OK\n"
4. MCU wird neugestartet → normaler Betrieb
```

| Option | Vorteil | Nachteil |
|---|---|---|
| A (SWD direkt) | Schnell, zuverlässig | Braucht SWD-Zugang am fertigen Board |
| B (UART-Bootloader) | Kein SWD nötig | Nur STM32, BOOT0-Pin nötig |
| C (IF820-Pipe) | Kein extra Programmer, Gerät kann fertig verbaut sein | MCU braucht Provisioning-Firmware-Logik |

#### Schritt 4: Read Protection aktivieren

| Aspekt | Detail |
|---|---|
| **Was** | MCU-Flash gegen Auslesen schützen |
| **Wann** | NACH Secret-Write, als LETZTER Schritt |
| **Achtung** | Danach kein Debugging/Auslesen mehr möglich! |

| MCU | Befehl | Effekt |
|---|---|---|
| STM32 (ST-Link) | `STM32_Programmer_CLI -ob RDP=1` | JTAG-Lesen gesperrt, Chip-Erase setzt zurück |
| STM32 (pyOCD) | `target.write32(FLASH_OPTCR, RDP_LEVEL_1)` | Gleiches per Script |
| nRF52 (Nordic) | `nrfjprog --rbp ALL` | APPROTECT aktiv |
| ESP32 | `espefuse.py burn_efuse JTAG_DISABLE` | JTAG permanent aus |

```python
# Beispiel: STM32 RDP Level 1 setzen
import subprocess
subprocess.run(["STM32_Programmer_CLI", "-c", "port=SWD", "-ob", "RDP=1"])
# ⚠️ Ab jetzt: kein Flash-Lesen mehr möglich!
```

> **⚠️ ACHTUNG:** RDP Level 2 (STM32) ist IRREVERSIBEL — kein Firmware-Update mehr möglich!
> **Empfehlung:** RDP Level 1 verwenden. Flash ist geschützt, aber Chip-Erase + Neuflash bleibt möglich.

#### Schritt 5: QR-Code drucken + kleben

| Aspekt | Detail |
|---|---|
| **Inhalt** | Seriennummer + Secret, z.B. `"SN:4711;KEY:a7b3c9f1e2d456789abcdef012345678"` |
| **Format** | QR-Code (Version 3, ~77 Zeichen passen problemlos) |
| **Druck** | Thermotransfer-Etikettendrucker (z.B. Zebra ZD421, ~500 €) |
| **Material** | Polyester-Etiketten (wetterfest, UV-beständig, 10+ Jahre haltbar) |
| **Größe** | ~15×15 mm reicht für den Dateninhalt |
| **Kosten** | ~0,03–0,10 € pro Etikett (Material) |
| **Platzierung** | Innengehäuse, Klemmendeckel, oder Beipackzettel |

```python
# QR-Code erzeugen (Produktions-Script)
import qrcode
qr_data = f"SN:{serial};KEY:{secret}"
img = qrcode.make(qr_data)
img.save(f"labels/{serial}.png")
# → An Etikettendrucker senden
```

**Alternative zu QR-Aufkleber:**
| Alternative | Pro | Contra |
|---|---|---|
| QR in Verpackung/Anleitung | Kein Kleben am Gerät | Verpackung geht verloren |
| QR per Laser graviert | Unzerstörbar, 30+ Jahre | Teurer (~1–3 € pro Gerät) |
| NFC-Tag am Gerät | App scannt kontaktlos | +0,20 € pro Tag, App braucht NFC |
| Secret auf Beipackkarte | Einfach | Karte geht verloren |

#### Komplettes Produktions-Script (Zusammenfassung)

```python
#!/usr/bin/env python3
"""Produktions-Flasher: Firmware + Secret + RDP + QR"""
import os
import subprocess
import qrcode

# --- Konfiguration ---
FIRMWARE_PATH = "firmware_v1.0.bin"
SECRET_FLASH_ADDR = 0x080E0000
SERIAL_PREFIX = "VT"

def produce_device(serial_number: str):
    print(f"[1/5] Firmware flashen...")
    subprocess.run(["STM32_Programmer_CLI", "-c", "port=SWD",
                    "-w", FIRMWARE_PATH, "0x08000000"], check=True)

    print(f"[2/5] Secret generieren...")
    secret = os.urandom(16).hex()

    print(f"[3/5] Secret in Flash schreiben @ {hex(SECRET_FLASH_ADDR)}...")
    secret_bin = bytes.fromhex(secret)
    # Secret als temp-Datei → flashen
    with open("_secret.bin", "wb") as f:
        f.write(secret_bin)
    subprocess.run(["STM32_Programmer_CLI", "-c", "port=SWD",
                    "-w", "_secret.bin", hex(SECRET_FLASH_ADDR)], check=True)

    print(f"[4/5] Read Protection (RDP Level 1) aktivieren...")
    subprocess.run(["STM32_Programmer_CLI", "-c", "port=SWD",
                    "-ob", "RDP=1"], check=True)

    print(f"[5/5] QR-Code erzeugen...")
    qr_data = f"SN:{serial_number};KEY:{secret}"
    img = qrcode.make(qr_data)
    img.save(f"labels/{serial_number}.png")

    print(f"✓ Gerät {serial_number} fertig. Secret: {secret[:8]}...")
    
    # Optional: In lokale CSV loggen (Backup)
    with open("production_log.csv", "a") as f:
        f.write(f"{serial_number},{secret}\n")

# Verwendung:
# produce_device("VT-00001")
```

#### Zeitaufwand pro Gerät in der Serie

| Schritt | Dauer |
|---|---|
| Firmware flashen | ~10 s |
| Secret generieren | <1 s |
| Secret in Flash schreiben | ~2 s |
| RDP aktivieren | ~3 s |
| QR drucken + kleben | ~15 s (manuell) / ~5 s (automatisch) |
| **Gesamt** | **~30–35 s pro Gerät** |

> Bei 1.000 Geräten: ~8–10 Stunden reine Flasher-Zeit (1 Person, 1 Programmer).
> Mit 2 Programmern parallel: ~5 Stunden.

---

### Gesamtaufwand

| Phase | Aufwand | Kosten |
|---|---|---|
| MCU-Firmware (Auth + RDP) | ~20 h | ~2.000 € |
| App (QR + Auth) | ~12 h | ~1.200 € |
| Produktions-Tooling (Secret-Gen + QR-Druck) | ~8 h | ~800 € |
| **Gesamt einmalig** | **~40 h** | **~4.000 €** |
| **Pro Gerät laufend** | — | **~0,50 €** (QR-Aufkleber) |
| **Server/Cloud** | — | **0 €** |
