# Parameterspeicher - Write-Once Rescue Block

## Problem

Bei undefinierten Spannungsschwankungen, Unterspannung oder unerwarteten Reboots können die Geräteparameter im AT45DB Flash korrumpiert werden. Wenn gleichzeitig die Backup-Kopie im LPC1766 Sektor 7 ebenfalls beschädigt ist (z.B. durch unterbrochenen Schreibvorgang in `EINT0_IRQHandler` oder `parameter_to_progmem()`), geht die Seriennummer und andere unveränderliche Gerätedaten verloren. Das Gerät führt dann eine Werkinitialisierung durch und ist ohne manuelle Neukonfiguration nicht mehr einsatzfähig.

## Aktuelle Speicherarchitektur (3 Kopien, alle überschreibbar)

```
┌──────────────────────────────────────────────────────────────┐
│ AT45DB Flash (extern, SPI)                                   │
│                                                              │
│  Seite 0 ... PROTOCOLPAGE-1    Messdaten                     │
│  PROTOCOLPAGE ... FREEPAGE-1   Protokoll (2k)                │
│  FREEPAGE ... PARAMETERPAGE-1  FREI (5k, für Param-Upload)   │
│  PARAMETERPAGE ... maxpage     Parameter fp (2k)             │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ LPC1766 interner Flash                                       │
│                                                              │
│  Sektor 0-6    Firmware (0x0000-0x6FFF)                      │
│  Sektor 7      fp_cp_area[4096] - Parameterkopie (0x7000)    │
│  Sektor 8-29   Firmware Fortsetzung                          │
└──────────────────────────────────────────────────────────────┘
```

### Schwachstellen

1. **AT45DB Parameterseite**: Wird bei jedem Parameterspeichern überschrieben (`flash_wbuf1`), bei Power-Down ISR, bei Werkinitialisierung. Ein unterbrochener Schreibvorgang zerstört die Daten.
2. **LPC Sektor 7**: Wird bei jedem `InitParameter()` via `parameter_to_progmem()` überschrieben (IAP: Erase + Write). Wenn die Quelle (AT45DB) bereits korrumpiert war, wird die Backup-Kopie mit kaputten Daten überschrieben.
3. **Kein unveränderlicher Anker**: Es gibt keinen Speicherbereich, der nur einmal geschrieben und danach nie wieder angefasst wird.

### Ablauf bei Stromausfall-Korruption

```
Normalbetrieb:  AT45DB-Parameter OK, LPC-Backup OK
     ↓
Spannungswackler während flash_wbuf1() → AT45DB-Parameter KORRUPT
     ↓
Nächster Boot: InitParameter() liest AT45DB → Kennung falsch
     ↓
Retry aus LPC Sektor 7 → eventuell noch OK → Rettung möglich
     ABER: danach sofort parameter_to_progmem() → überschreibt LPC-Backup
     ↓
Erneuter Spannungswackler → auch LPC-Backup KORRUPT
     ↓
Werkinitialisierung: werkparameter() → serno="", btpin=0, btmodem=0
     ↓
Gerät verloren, manuelle Neukonfiguration erforderlich
```

## Lösung: Write-Once Rescue Block im AT45DB

### Konzept

Eine dedizierte Flash-Seite im FREEPAGE-Bereich wird **genau einmal** beschrieben (wenn die Seriennummer zum ersten Mal gesetzt wird) und danach **nie wieder** angefasst — weder bei normalen Parametersicherungen, noch bei Werkinitialisierung, noch in der Power-Down ISR.

### Zu schützende Daten (Kandidaten)

| Feld | Größe | Begründung |
|------|-------|------------|
| `fp.serno` | 17 Bytes | Geräteidentität, BT-PIN wird daraus berechnet |
| `fp.btpin` | 4 Bytes | Bluetooth Pairing-PIN |
| `fp.hwVersion` | 6 Bytes | Hardware-Revision, bestimmt Peripherie-Konfiguration |

Weitere mögliche Kandidaten:
- `fp.TxF` — Sendefrequenz (individuell kalibriert?)
- `fp.simpin` — SIM-PIN
- `fp.apn` / `fp.server` — Netzwerkkonfiguration

### Datenstruktur

```c
#define RESCUE_KENNUNG  0xDEAD5AFE   // Magic Number
#define RESCUEPAGE      (FREEPAGE)   // Erste Seite im Freibereich nutzen

typedef struct {
    uint   kennung;          // 4 Byte  Magic: RESCUE_KENNUNG
    char   serno[17];        // 17 Byte Geräteseriennummer
    uint   btpin;            // 4 Byte  Bluetooth PIN
    char   hwVersion[6];     // 6 Byte  Hardware-Revision
    // ... weitere Felder nach Bedarf
    uint   checksum;         // 4 Byte  CRC32 oder einfache Prüfsumme
} rescue_data;               // ca. 35 Byte, passt in eine 256-Byte-Seite
```

### Speicherort im AT45DB

```
FREEPAGE (5k Freibereich):
  Seite 0: RESCUE BLOCK (write-once)     ← NEU
  Seite 1-19: weiterhin frei für Parameter-Upload etc.
```

Da `FREEBLOCKS=5` und eine Seite nur 256 Bytes belegt, wird die Kapazität für Parameter-Uploads nicht nennenswert eingeschränkt.

## Implementierungsplan

### 1. Neue Datenstruktur definieren (hard.h)

- `rescue_data` Struct mit Kennung, serno, btpin, hwVersion, Checksum
- `#define RESCUE_KENNUNG 0xDEAD5AFE`
- `#define RESCUEPAGE (FREEPAGE)` — erste Seite im Freibereich

### 2. Neue Funktionen in flash.c

#### `rescue_write()` — Einmalig schreiben

```
Ablauf:
1. Rescue-Seite aus AT45DB lesen
2. Prüfe ob Kennung == RESCUE_KENNUNG → bereits geschrieben → ABBRUCH
3. Baue rescue_data Block auf (serno, btpin, hwVersion aus fp.*)
4. Berechne Checksum
5. Schreibe Block auf RESCUEPAGE
6. Lese zurück und verifiziere (Compare)
```

Aufruf: Nur wenn Seriennummer zum ersten Mal gesetzt wird (Menü Seriennummer eingeben).

#### `rescue_read()` — Rettungsdaten lesen

```
Ablauf:
1. Rescue-Seite aus AT45DB lesen
2. Prüfe Kennung == RESCUE_KENNUNG
3. Prüfe Checksum
4. Bei Erfolg: Daten in Übergabestruktur kopieren, return 1
5. Bei Fehler: return 0
```

#### `rescue_restore()` — In Parameter zurückspielen

```
Ablauf:
1. rescue_read() aufrufen
2. Bei Erfolg: fp.serno, fp.btpin, fp.hwVersion überschreiben
3. Protokolleintrag schreiben (RESCUE_RESTORE Event)
```

### 3. Integration in InitParameter() (flash.c)

```
Bestehender Ablauf:
  Parameter aus AT45DB lesen → Kennung prüfen
  ↓ Kennung falsch?
  Retry aus LPC Sektor 7 (fp_cp_area)
  ↓ Auch falsch?
  werkparameter() → alle Parameter auf Default

NEUER Ablauf (Ergänzung nach werkparameter):
  werkparameter() → alle Parameter auf Default
  ↓
  rescue_restore() → serno, btpin, hwVersion aus Rescue-Block wiederherstellen
  ↓
  Gerät hat zumindest seine Identität behalten
```

### 4. Integration in Seriennummer-Eingabe (sicom.c)

Wenn der Benutzer die Seriennummer setzt (Menü), danach `rescue_write()` aufrufen. Der Rescue-Block wird nur geschrieben wenn die Seite noch leer ist (Kennung ≠ RESCUE_KENNUNG).

### 5. Optionaler Menüpunkt: Rescue-Daten anzeigen/löschen

- Neuer Diagnosemenüpunkt: Rescue-Block lesen und anzeigen
- Rescue-Block löschen (nur für Servicetechniker, z.B. wenn Gerät eine neue Seriennummer bekommt)

## Sicherheitsbetrachtung

| Szenario | Verhalten |
|----------|-----------|
| Normaler Betrieb | Rescue-Block wird nie angefasst |
| Spannungsausfall während Param-Speichern | Rescue-Block unbetroffen (separate Seite) |
| Spannungsausfall während Power-Down ISR | Rescue-Block unbetroffen (ISR schreibt nur PARAMETERPAGE) |
| Werkinitialisierung | werkparameter() setzt Defaults, rescue_restore() spielt serno zurück |
| Firmware Update (LPC) | Rescue-Block im AT45DB, nicht betroffen |
| Firmware Update mit Param-Upload | FREEPAGE-Bereich wird für Upload genutzt — **ACHTUNG**: Upload-Routine muss RESCUEPAGE aussparen! |
| Seriennummer-Änderung | Nur möglich wenn Rescue-Block gelöscht wurde (neuer Menüpunkt) |

### Kritischer Punkt: Parameter-Upload

Die Funktion die den Parameter-Upload über FREEPAGE abwickelt muss angepasst werden, damit RESCUEPAGE nicht überschrieben wird. Alternativ RESCUEPAGE an eine andere Stelle legen (z.B. letzte Seite vor PARAMETERPAGE statt erste Seite in FREEPAGE).

## Dateien die geändert werden

| Datei | Änderung |
|-------|----------|
| `hard.h` | `rescue_data` Struct, Defines RESCUE_KENNUNG, RESCUEPAGE |
| `flash.h` | Funktionsdeklarationen rescue_write(), rescue_read(), rescue_restore() |
| `flash.c` | Implementierung der drei Rescue-Funktionen |
| `flash.c` | `InitParameter()` — rescue_restore() nach werkparameter() aufrufen |
| `sicom.c` | Seriennummer-Eingabe — rescue_write() aufrufen |
| `sicom.c` | Optional: Diagnosemenü Rescue-Block anzeigen/löschen |

## Offene Fragen

1. **Welche Felder genau?** — Nur serno, oder auch btpin, hwVersion, weitere?
2. **Rescue-Block löschbar?** — Soll ein Menüpunkt existieren um den Block zu löschen (Seriennummer-Änderung bei Gerätetausch)?
3. **Doppelte Absicherung?** — Rescue-Block auf zwei Seiten spiegeln (redundant) für noch mehr Sicherheit?
4. **AT45DB Sector Lockdown?** — Die AT45DB-Chips unterstützen Hardware-Sector-Lockdown (irreversibel). Soll das genutzt werden? Vorteil: absolut unzerstörbar. Nachteil: Nie wieder änderbar, Sektor-Granularität ist groß.
