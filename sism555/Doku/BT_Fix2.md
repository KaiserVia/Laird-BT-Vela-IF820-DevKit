# BT / Zertifikatsupload - Arbeitsstand & Erkenntnisse (Fortsetzung Montag)

**Datum:** 2026-04-24
**Status:** [!] Code ist aktuell in einem ungetesteten / vermutlich defekten Zustand.
**Dringende Aktion Montag:** Vor jedem weiteren Schritt zuerst Encoding-Schaden reparieren (siehe Punkt 1).

---

## 1. Root-Cause des neu aufgetretenen BT-Muellzeichen-Problems

**Symptom:**
Nach den heutigen Aenderungen erscheint im BT-Terminal nur noch Zeichensalat:
```
ùÿëêèëõêÿèõîèèaeýêÿèòýñueáueáø...
```
Gleichzeitig laeuft das Zertifikats-Upload ueber RS232 in `Error Xmodem timeout`.

**Wahre Ursache (am Ende entdeckt):**
Die Dateien `btio.c`, `btio.h`, `sicom.c`, `gsmio.c` liegen im Repository in einer **Nicht-UTF-8-Codierung** (vermutlich CP437 / OEM-DE oder Windows-1252, Keil uVision Default).
Durch die heutigen Bearbeitungen sind in den String-Literalen und Kommentaren die Umlaute zerstoert worden:

| Vorher | Nachher (in Datei) |
|--------|--------------------|
| `ue` (0xFC in CP437 -> `3`) | `┐` (U+FFFD UTF-8 Replacement) |
| `ae` (`õ`) | `┐` |
| `oe` (`÷`) | `┐` |

Das betrifft **alle** String-Konstanten in den geaenderten Dateien, nicht nur die neu hinzugefuegten Zeilen. Damit werden beim Senden an BT/GSM falsche Bytes verschickt -> Zeichensalat und fehlgeschlagene AT-Antwort-Vergleiche.

**Nachweis:**
```bash
git diff --ignore-all-space -- btio.c
# zeigt massenhaft Umlaut-Diffs in unveraenderten Zeilen, z. B.
# -  // Ermittle Kommandolaenge
# +  // Ermittle Kommandol┐nge
```

**Konsequenz:** Selbst wenn die Logik-Aenderungen korrekt waeren, wuerde die FW nicht funktionieren, solange diese Encoding-Schaeden drin sind.

---

## 2. Empfohlener Weg Montag

### Schritt A - Sauberer Revert
```powershell
cd c:\viaTraffic\GitHub\Projects\sism555
git checkout HEAD -- btio.c btio.h sicom.c gsmio.c
```
Damit sind alle vier Dateien garantiert auf dem originalen Keil-Encoding-Stand.

### Schritt B - Editor-Encoding in VS Code einstellen
Bevor erneut editiert wird:
- VS Code: *File -> Preferences -> Settings* -> `files.encoding` fuer Projekt oder Workspace auf **`windows1252`** (oder was Keil erzeugt) setzen.
- `.vscode/settings.json` vorschlagen:
  ```json
  {
    "files.encoding": "windows1252",
    "files.autoGuessEncoding": false
  }
  ```
- Danach Dateien neu oeffnen: VS Code zeigt Umlaute korrekt an. Nur dann editieren.

### Schritt C - Gezielte Neuanwendung der Fixes
Nur diese Zeilen aendern (Byte-genau, keine automatische Reformatierung!):

#### btio.h
`BT_BAUD`-Makro hinzufuegen (Umlaute hier egal, nur ASCII):
```c
#define Laird   1
#define Roving  46

// BT modem baud rate: Laird 730-SA 460800, RN4678 307200
#define BT_BAUD (fp.btmodem==Laird ? 460800 : 307200)
```

#### gsmio.c - Include
Eine Zeile hinzufuegen:
```c
#include "gsmio.h"
#include "btio.h"      // <-- neu
#include "i2cm.h"
```

#### gsmio.c - change_radionet() (ca. Zeile 1310)
```c
FIO0SET=GSM_DTR;
Init_BT_ch(BT_BAUD);   // statt: MUX_BT_Select;
```
Begruendung: nach `AT+QCFG` steht UART1 auf GSM-Baud 460800. Ein reines MUX-Toggle wuerde BT mit falscher Baud bedienen.

#### sicom.c - upload_file() nach file_to_uc15 (ca. Zeile 450)
```c
readline (fname,sizeof(fname),'a');
Init_GSM_ch(460800);                        // NEU - MUX + Baud auf GSM (vorher: MUX_GSM_Select;)
if (file_to_uc15(filesize, fname)>0)
{
    Init_BT_ch(BT_BAUD);                    // NEU - MUX + Baud zurueck auf BT
    putln(T_csucc);
}
else
{
    Init_BT_ch(BT_BAUD);                    // NEU
    put2str(T_err,E_gsm);                   // NEU - Fehler war vorher still
    newline();
}
```
**Dies ist der eigentliche Bugfix** fuer das Zertifikats-Upload-Problem bei RN4678 (307200 != 460800 von `modem_start`).

### Schritt D - NICHT machen
Folgende Aenderungen wurden heute probiert und fuehrten zu Regressionen -> Montag weglassen:

- [X] `bxi=rxi` in `Init_BT_ch` einbauen - wirkt global auf alle BT-Pfade, inkl. Detection.
- [X] Erste `MUX_BT_Select` in `upload_file` nach `modem_start()` durch `Init_BT_ch(BT_BAUD)` ersetzen - nicht notwendig, wenn Schritt C ordentlich umgesetzt ist, und birgt Risiko (`fp.btmodem` kann 0 sein).
- [X] `MUX_BT_Select` in `gsm_power()` else-Zweig (Zeile 146) umstellen - Originalverhalten beibehalten.

---

## 3. Hardware-Fakten (zur Referenz)

| MUX-Ziel | UART1-Baud |
|----------|------------|
| BT Laird 730-SA | 460800 |
| BT Roving RN4678 | 307200 |
| GSM UC15 / EG91 | 460800 |
| GPS | 9600 |

- UART1 ist physisch **eine** Peripherie, der Hardware-MUX (CSAF/CSBF) schaltet zwischen BT/GSM/GPS.
- Baudrate ist global pro UART1 - sie gilt fuer die Seite, auf die der MUX gerade zeigt.
- `MUX_BT_Select` / `MUX_GSM_Select` schalten **nur** den Pin, **nicht** die Baudrate.
- `Init_BT_ch(baud)` = MUX-BT + `Init_UART1(baud)` + CTS-Check.
- `Init_GSM_ch(baud)` = MUX-GSM + `Init_UART1(baud)`.

---

## 4. Testplan Montag

Nach Umsetzung Schritte A-C:

1. **Compile-Check**: `btio.c`/`gsmio.c`/`sicom.c` sauber bauen, keine Warnings (besonders keine `implicitly declared`-Warnings mehr).
2. **Normale BT-Verbindung** (Laird): Menue sichtbar, keine Muellzeichen.
3. **Normale BT-Verbindung** (RN4678): Menue sichtbar, keine Muellzeichen.
4. **Zertifikats-Upload ueber RS232** (Laird-Geraet): Funktioniert wie vor heute.
5. **Zertifikats-Upload ueber RS232** (RN4678-Geraet): Sollte jetzt funktionieren (war der eigentliche Bug).
6. **Zertifikats-Upload ueber BT** (Laird + RN4678): Testen - nach Schritt C sollte dies korrekt sein, da die Restore-Sequenz nach `file_to_uc15` BT sauber wiederherstellt.
7. **Netzumstellung** ueber `change_radionet`: Nach `AT+QCFG` muss BT-User weiter kommunizieren koennen.
8. **Firmware-Update ueber RS232** (Hauptmenue -> `3` -> `Strg+U`, Xmodem-1K).
9. **Firmware-Update ueber BT** (gleiche Tastenfolge) - sollte unveraendert funktionieren.

---

## 5. Selbstkritik / Lessons Learned

- Bei Dateien mit Nicht-UTF-8-Codierung **immer zuerst das Editor-Encoding pruefen**, bevor Bearbeitungen erfolgen. Sonst werden stumm alle Umlaute zerstoert.
- Aenderungen an Low-Level-Funktionen wie `Init_BT_ch` betreffen viele Pfade - nicht einfach einbauen, nur weil sie "gut aussehen".
- `MUX_*_Select` ist **reines Pin-Toggle**, keine Baudraten-Aenderung. Das ist ein subtiler aber kritischer Unterschied.
- Symptom "Zeichensalat" kann zwei grundverschiedene Ursachen haben: Baudraten-Mismatch ODER zerstoerte String-Literale. Nicht vorschnell auf Ersteres schliessen.

---

## 6. Referenz: Original-Dokument

Siehe [BT_MUX_Baudrate_Fix.md](BT_MUX_Baudrate_Fix.md) - beschreibt die urspruengliche Analyse. Die dort vorgeschlagenen Aenderungen sind in Schritt C oben bereits konsolidiert (nur die wirklich notwendigen Punkte).


---

## 7. Durchgefuehrte Aenderungen (2026-05-04)

### 7.1 Encoding-Reparatur

**Problem:**
Die Quelldateien `btio.c`, `btio.h`, `sicom.c`, `gsmio.c` lagen im Repository in
Windows-1252-Codierung (Keil uVision Standard). Durch Bearbeitung in VS Code mit
UTF-8-Default wurden alle Umlaute in String-Literalen und Kommentaren zerstoert.
Falsche Bytes wurden ueber BT gesendet -> Zeichensalat statt Menuetext.

**Loesung:**
1. `git checkout HEAD -- btio.c btio.h sicom.c gsmio.c` (Revert auf Original-Encoding)
2. `.vscode/settings.json` angelegt:
   ```json
   {
       "files.encoding": "windows1252",
       "files.autoGuessEncoding": false
   }
   ```
   VS Code oeffnet/speichert die Dateien nun korrekt in Windows-1252.

---

### 7.2 Baudrate-Umstellung: Alle Module auf 115200

**Motivation:**
Die bisherige Konfiguration verwendete unterschiedliche Betriebsbaudraten:
- Laird 730-SA: 460800
- RN4678: 307200
- LTE (UC15/EG91): 460800 (Betrieb), gespeichert als 115200

Dies fuehrte zu Baudrate-Mismatches nach Verbindungsaufbau und MUX-Umschaltungen.
Vereinheitlichung auf **115200** fuer alle Module beseitigt diese Fehlerquelle.

**Neue Betriebsbaudraten:**

| Modul | Vorher | Nachher |
|-------|--------|---------|
| Laird 730-SA (BT) | 460800 | **115200** |
| RN4678 (BT) | 307200 | **115200** |
| UC15/EG91 (LTE) | 460800 | **115200** |

---

### 7.3 Geaenderte Dateien

#### btio.h
- `T_S521`: `ATS521=460800` -> `ATS521=115200` (Laird Zielbaudrate)
- `T_su`: `SU,10` -> `SU,03` (RN4678: 307200 -> 115200)

#### btio.c
- `init_bluetooth()`: Ersterkennung startet mit 115200 (vorher 460800)
- Laird-Fallback: 4 Versuche (115200, 9600, 307200, 460800) statt 2 (115200, 9600)
- RN4678-Fallback: 4 Versuche (115200, 9600, 307200, 460800) statt 2 (115200, 307200)
- Nach Laird ATS521: UART auf 115200 (vorher 460800)
- Nach RN4678 Reboot: Init_BT_ch(115200) (vorher 307200)
- `test_BT()`: baud=115200 (vorher 307200/460800)
- `send_bt_info()`: baud=115200 (vorher 460800/307200)
- `set_bt_name()`: baud=115200 (vorher 460800/307200)
- `set_bt_pin()`: baud=115200 (vorher 460800/307200)

#### main.c
- Zeile 138: `Init_GSM_ch(460800)` -> `Init_GSM_ch(115200)` (Watchdog-Reset Handler)
- Zeile 207-208: `if (Laird) Init_BT_ch(460800) else Init_BT_ch(307200)` ->
  `Init_BT_ch(115200)` (Firmware-Update BT Wiederherstellung)

#### libtool.c
- BT-Verbindungsaufbau (comstate change): `baud=307200` + `if Laird baud=460800` ->
  `baud=115200` (einheitlich fuer beide Module)

#### gsmio.c
- `gsm_power()` mode&2: `Init_GSM_ch(460800)` -> `Init_GSM_ch(115200)`
- `init_gsm()` Fallback: erweitert von (115200, 460800, 9600) auf
  (115200, 9600, 307200, 460800)

#### gsmio.h
- `T_cfsto` war bereits `AT+IPR=115200;&W` (keine Aenderung noetig)

---

### 7.4 Fallback-Erkennung

Beide BT-Module und das LTE-Modul werden bei der Erst-Erkennung mit
4 Baudraten-Stufen angesprochen:

```
115200 -> 9600 -> 307200 -> 460800
```

Damit werden Module gefunden, die noch mit einer alten Firmware-Version
auf hoeherer Baudrate konfiguriert waren.

---

### 7.5 Nicht geaenderte Stellen

| Datei | Zeile | Kontext | Warum belassen |
|-------|-------|---------|----------------|
| libtool.c | 624-625 | Manuelles Debug-Testmenue Baudrate | Frei waehlbar fuer Diagnose |
| btio.c | 112-113, 174-175 | Fallback Init_UART1(307200/460800) | Erkennung alter Module |
| gsmio.c | 210-211 | Fallback Init_UART1(307200/460800) | Erkennung alter Module |

---

### 7.6 Testplan

1. Firmware compilieren und flashen
2. BT-Verbindung (Laird): Hauptmenue muss korrekt angezeigt werden
3. BT-Verbindung (RN4678): Hauptmenue muss korrekt angezeigt werden
4. LTE-Modem: `gsm_power(3)` muss +CFUN:1 erhalten
5. Firmware-Update ueber RS232 und BT
6. Watchdog-Reset: GSM-Modem sauber herunterfahren
