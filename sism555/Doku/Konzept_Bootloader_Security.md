# Konzept: Software-Umbau fuer CRA / EN 18031 (Bootloader, signierte Updates, Haertung)

Stand 2026-11-27, Entwurf. Grundlage: `CRA_EN18031_LPC1766.md` (Anforderungen, Speicherabschaetzung),
`Werkzeug_Umstieg_Aufwand.md` (Werkzeuge). Code-Stand: `main` 5.158 (USB-Branch gemergt). Arbeitsbranch: `bootloader`, Uebersicht in
`Bootloader_Security_Uebersicht.md`.

## 1. Heute (aus dem Code)

**Update-Wege** - alle fuenf enden in `iap_programming()` (ramcode.c, laeuft aus dem RAM):

| Weg | Code | Herkunft der Datei |
|---|---|---|
| RS232 / BT (XMODEM) | sicom.c:79 | Terminal |
| USB-Kabel (XMODEM) | sicom.c (gleicher Pfad) | Terminal |
| USB-Stick | USB_tools.c:411 | Datei auf dem Stick |
| LTE (MQTT-Befehl 5) | mqtt.c:1137 | HTTP-Download ins Modem, dann `firmware_to_flash()` |

**Ablauf:** Datei wird blockweise (1 KB + CRC16) in den externen Datenflash ab **Seite 0** geschrieben - dort beginnen
auch die Messdaten (`MEASUREPAGE 0`), deshalb loescht ein Update die Messdaten (`delete_data()`).
`file_is_firmware()` (libtool.c) prueft den CRP-Schluessel im Abbild (CRP2/CRP3 Pflicht ausser Debug-Version), die
reservierten Vektorworte und traegt die Vektor-Pruefsumme ein. `iap_programming()` sperrt die Interrupts, loescht die
internen Sektoren 0-6 und 8-N (**Sektor 7 = Parameterkopie `fp_cp_area` bei 0x7000 bleibt**), kopiert Block fuer Block
und loest per Watchdog einen Reset aus.

**Schwaechen:**

1. **Keine Echtheitspruefung.** CRC16 und Vektor-Pruefsumme erkennen Uebertragungsfehler, nicht Faelschungen. Jede
   passende Binaerdatei wird ueber jeden der fuenf Wege angenommen.
2. **Stromausfall waehrend `iap_programming()` = Geraet tot** (Sektor 0 geloescht, kein Rueckweg ausser ISP in der
   Werkstatt).
3. Kein Schutz gegen das Einspielen alter Versionen mit bekannten Luecken.
4. Update loescht Messdaten (Abbild und Messdaten teilen sich den Speicher ab Seite 0).
5. CRP wird mit jedem Abbild neu geschrieben, Entwicklungsbuilds laufen mit CRPNONE.

## 2. Ziel-Architektur

```
Interner Flash (256 KB)                                    Externer Datenflash (4 MB)
0x00000 +---------------------------+                      +---------------------------+
        | Stufe 0 (Sektor 0, 4 KB)  |  nie geaendert, CRP  | Slot A: neues Abbild      |  Update-Wege
        |  CRP-Wort 0x2FC           |  -Wort, kein Krypto  |  (App oder Bootloader)    |  schreiben nur hierhin
0x01000 +---------------------------+                      +---------------------------+
        | Bootloader A (S. 1-3,     |  im Feld per Update  | Slot B: letztes gutes     |  fuer Rueckfall
        |  12 KB)                   |  austauschbar        |  App-Abbild               |
0x04000 +---------------------------+  (abwechselnd A/B,   +---------------------------+
        | Bootloader B (S. 4-6,     |   Abschnitt 3a)      | Messdaten                 |  bleiben beim Update
        |  12 KB)                   |                      |  (verschoben hinter die   |  erhalten
0x07000 +---------------------------+                      |   Slots)                  |
        | Parameterkopie fp (S. 7)  |  bleibt wie heute    |                           |
0x08000 +---------------------------+                      |                           |
        | Statussektor (S. 8, 4 KB) |  Update-Zustand,     |                           |
0x09000 +---------------------------+  Sicherheitszaehler  |                           |
        | Anwendung (S. 9-29,       |                      |                           |
        |  220 KB), Kopf + Signatur |                      |                           |
0x40000 +---------------------------+                      +---------------------------+

RAM: Stufe 0 und Bootloader benutzen nur 0x10000000 (wird von der App neu initialisiert),
     **nie** den AHB-RAM 0x2007C000 (non_init, fp, connect ...).
     Ausnahme: 32 Byte am oberen Ende von 0x10000000 fuer den Startzaehler der Stufe 0 (App-Linker 0x7FE0 statt 0x8000).
```

Anwendung heute 179,6 KB; mit GCC-Zuschlag ca. 200 KB -> passt in 220 KB. Passen Bootloader A/B nicht in je 12 KB
(nach Messung, Schritt 1 in Abschnitt 8), wandert der Notbetrieb (XMODEM) in die Stufe 1 einer Variante mit
2 x 16 KB und App ab 0xB000 (212 KB).

## 3. Bootloader

Eigenes, kleines Projekt (kein RTOS, keine Middleware), ca. 12-20 KB. Ablauf nach jedem Reset:

1. Takt (PLL, fuer die Pruefung gern 100 MHz), Watchdog, SPI zum Datenflash. Keine Pins/Peripherie anfassen, die die
   App mit festen Zustaenden erwartet (BT-RST P1.16, UART-MUX, LTE-Versorgung) - nur so wenig wie noetig, danach
   alles auf Reset-Zustand zurueck.
2. **Statussektor lesen.** Steht dort "Update bereit in Slot A":
   - Kopf von Slot A lesen, **Signatur pruefen**, Produkt-/Hardware-Kennung und **Sicherheitszaehler** pruefen
     (nicht kleiner als der gespeicherte Mindestwert).
   - Nur dann: aktuelle App nach Slot B sichern (falls noch nicht dort), App-Sektoren 9-29 loeschen, aus Slot A
     kopieren, Hash der kopierten App nachpruefen, Status "Probestart" schreiben.
   - **Stromausfall in dieser Phase ist unkritisch:** der Bootloader bleibt unberuehrt und setzt nach dem naechsten
     Einschalten wieder ein.
3. **App pruefen** (Kopf, Laenge, SHA-256; optional volle Signaturpruefung bei jedem Start = Secure Boot,
   ca. 0,2-1,5 s). Gueltig -> VTOR = 0x9000, Stackpointer setzen, in die App springen.
4. **Probestart / Rueckfall:** Nach einem Update muss die App innerhalb einer Frist "bestaetigt" melden (z. B. nach
   abgeschlossener Initialisierung und erstem erfolgreichem Messzyklus). Drei Resets ohne Bestaetigung -> Bootloader
   kopiert Slot B zurueck. Zaehler im Statussektor (die RTC-Register GPREG0-4 sind alle belegt, hard.h:286-290).
5. **Notbetrieb:** Ist keine gueltige App vorhanden und Slot B unbrauchbar: minimaler XMODEM-Empfang ueber RS232
   (UART0), nimmt **nur signierte** Abbilder an. Ersetzt im Feld die Werkstatt-ISP.
6. **Bootloader-API** an fester Adresse (Funktionstabelle): Signatur pruefen, Status setzen, "bestaetigt" melden.
   Die App benutzt dieselbe Kryptografie, statt sie ein zweites Mal mitzubringen (spart Flash).

Der Bootloader ist im Feld austauschbar, siehe Abschnitt 3a; nur die Stufe 0 bleibt fuer immer gleich.

**CRP:** Das CRP-Wort liegt bei 0x2FC, also im Bootloader. Serie mit **CRP2** (JTAG aus, ISP nur noch mit
Komplettloeschung -> Werkstatt kann neu programmieren, Auslesen geht nicht). CRP3 (ISP ganz aus) erst, wenn der
Notbetrieb aus Punkt 5 erprobt ist.

## 3a. Bootloader-Updates

**Warum ueberhaupt:** Der Bootloader enthaelt die Signaturpruefung und die oeffentlichen Schluessel. Wird dort eine
Schwachstelle gefunden oder muss ein Schluessel getauscht werden, verlangt der CRA eine Korrektur im Feld. "Nur in der
Werkstatt" reicht deshalb nicht.

**Verworfen:** Die App ueberschreibt den Bootloader direkt - ein Stromausfall in diesem Moment macht das Geraet
unbrauchbar (Sektor 0 leer).

**Loesung: zweistufig mit zwei Bootloader-Plaetzen.**

- **Stufe 0** (Sektor 0, wenige hundert Zeilen): wird **nie** geaendert. Liest die Koepfe von Bootloader A und B
  (Magic, Version, Laenge, SHA-256), waehlt den gueltigen mit der hoechsten Version und springt hinein. Kein Krypto-
  Schluessel, keine Schnittstelle nach aussen, schreibt nie in den Flash - dadurch praktisch keine Angriffsflaeche und
  kaum Fehlerquellen. Traegt das CRP-Wort und die Vektortabelle fuer den ISP.
- **Bootloader A und B** (Stufe 1): je ein vollstaendiger Bootloader nach Abschnitt 3. Immer genau einer ist aktiv,
  der andere ist der Platz fuer das naechste Bootloader-Update.

**Ablauf eines Bootloader-Updates:**

1. Ein signiertes Abbild mit Typ "Bootloader" kommt ueber einen der fuenf normalen Update-Wege in Slot A des externen
   Flash (gleicher Weg wie App-Updates, gleiche Signaturpruefung in der App vorab).
2. Neustart. Der aktive Bootloader (z. B. A) prueft Signatur, Produktkennung und den **eigenen Sicherheitszaehler fuer
   Bootloader** (kein Zurueck auf einen Bootloader mit bekannter Luecke).
3. Er loescht nur den **inaktiven** Platz (B), schreibt das neue Abbild, prueft den Hash nach. Stromausfall dabei:
   B ist ungueltig, Stufe 0 startet weiter A, A beginnt von vorn. A wird nie angefasst.
4. Neustart. Stufe 0 sieht B gueltig und neuer -> startet B auf Probe.
5. **Probelauf:** B muss die App starten, und die App muss "bestaetigt" melden. Haengt B oder startet die App nicht,
   loest der Watchdog Resets aus; Stufe 0 zaehlt sie in den reservierten 32 Byte RAM (ueberleben einen Watchdog-Reset).
   Nach drei Fehlstarts startet Stufe 0 wieder A; A erklaert B im Statussektor fuer ungueltig. Stufe 0 selbst schreibt
   dabei nie in den Flash.
6. Nach der Bestaetigung traegt B sich im Statussektor als bestaetigt ein. Beim naechsten Bootloader-Update ist A der
   inaktive Platz.

**Schluesselwechsel:** Ein neuer Bootloader kann neue oeffentliche Schluessel mitbringen. Er selbst ist noch mit dem
alten Schluessel signiert (oder mit dem Reserveschluessel, falls der alte kompromittiert ist). Danach gelten nur die
neuen Schluessel fuer App- und Bootloader-Abbilder.

**Stufe 0 aendern** geht nur in der Werkstatt: CRP2 -> ISP-Komplettloeschung -> neu programmieren (Parameter vorher
sichern). Deshalb Stufe 0 minimal halten, vollstaendig reviewen und testen, vor Serienstart einfrieren.

**Begruendung ohne Signaturpruefung in Stufe 0:** Bootloader-Plaetze werden nur vom laufenden Bootloader beschrieben,
und der schreibt nur nach erfolgreicher Signaturpruefung. Mit CRP2 kann niemand von aussen in den Flash schreiben. Die
Echtheit wird also beim Einspielen geprueft, Stufe 0 prueft nur die Unversehrtheit. Eine volle Vertrauenskette
(Signaturpruefung schon in Stufe 0) waere moeglich, braucht aber ca. 8-12 KB fuer Stufe 0 und macht genau den Teil,
der nie mehr geaendert werden kann, komplex - nicht empfohlen.

**Tests:** Strom weg waehrend des Schreibens von B (Relais), absichtlich defekter B, B der haengt, B mit altem
Sicherheitszaehler, B mit falschem Schluessel, App-Update nach Bootloader-Wechsel, zweimal hintereinander wechseln.

Aufwand zusaetzlich zu Abschnitt 8: Stufe 0 und A/B-Logik ca. **1 Woche** inkl. Tests.

## 4. Abbild-Format und Signatur

App-Abbild = Kopf (256 Byte) + Code ab 0x9000.

| Feld | Inhalt |
|---|---|
| Magic, Kopfversion | Erkennung |
| Produkt- und Hardware-Kennung | verhindert Abbilder fuer andere Geraete/Platinen |
| Firmware-Version | Text wie `T_version`, nur zur Anzeige |
| Sicherheitszaehler | ganze Zahl; wird nur bei Sicherheitskorrekturen erhoeht -> Rollback-Schutz, normales Zurueckgehen auf aeltere Versionen gleicher Stufe bleibt moeglich |
| Laenge, SHA-256 des Codes | Integritaet |
| Schluessel-Nr. | erlaubt Schluesselwechsel |
| Signatur | ueber den Kopf |

**Verfahren:** ECDSA P-256 mit SHA-256 (Bibliothek z. B. micro-ecc) oder Ed25519 (z. B. Monocypher). Beides gaengig und
in Software auf dem M3 machbar; vor der Wahl mit BSI TR-02102 abgleichen und auf dem Board messen (Flash, Zeit).
Im Bootloader stehen **zwei** oeffentliche Schluessel (Serie + Reserve fuer den Wechsel), kein Geheimnis.

**Schluesselverwaltung beim Hersteller:** privater Schluessel nie im Git und nicht auf Entwickler-PCs; Signieren als
eigener Schritt auf einem geschuetzten Rechner (spaeter ggf. HSM/USB-Token). Getrennte Schluessel fuer Entwicklung
(Boards am Tisch) und Serie. Ein Signier-Skript (Python) erzeugt aus `sism.bin` die signierte Datei.

Verschluesselung des Abbilds: nicht noetig fuer CRA/EN 18031 (Schutz gegen Auslesen leistet CRP); waere nur ein
Know-how-Schutz beim Verteilen und braeuchte einen gemeinsamen Geheimschluessel in allen Geraeten - vorerst nicht.

## 5. Umbau der Anwendung

1. **Linker:** App ab 0x9000, Laenge 0x37000; Vektortabelle dort (VTOR setzt der Bootloader, `SystemInit` nicht
   zuruecksetzen lassen); Kopf als eigene Sektion; non_init bleibt byte-gleich.
2. **Alle fuenf Update-Wege schreiben nur noch in Slot A** und setzen "Update bereit", danach Reset. `iap_programming()`
   fuer das Gesamtabbild und `file_is_firmware()` entfallen; die App prueft die Signatur vorab ueber die
   Bootloader-API, damit der Bediener sofort "ungueltige Datei" sieht statt eines stillen Ruecksprungs.
3. **Messdaten bleiben erhalten:** Slots liegen getrennt vor den Messdaten (Messdatenbereich verschiebt sich einmalig;
   Zeiger `fp.md_page` usw. beim Uebergang anpassen). `delete_data()` beim Update entfaellt.
4. **IAP in der App** nur noch fuer Sektor 7 (Parameterkopie, flash.c:401). Eine gemeinsame Schreibfunktion verweigert
   jeden anderen Sektor - die App kann den Bootloader und den Statussektor nicht versehentlich loeschen.
5. **Bestaetigung:** nach erfolgreichem Hochlauf (Parameter ok, Messung laeuft, bei Bedarf Verbindung ok) die
   Bootloader-API "bestaetigt" aufrufen.
6. **Versionen anzeigen:** Infoseite/Menue zeigt App-Version, Sicherheitszaehler, Bootloader-Version; MQTT meldet sie.

## 5a. Parameter-Handling: einzelne Parameter statt ganzer Block

### Heute (aus dem Code)

- Alle Parameter stehen in einer Struktur `fp` (`flashparameter`, hard.h, ca. 124 Felder), auf **2048 Byte
  eingefroren** (`fp_layout_frozen_check`). Erste Haelfte ab `Kennung`: feste Geraetedaten (u. a. Seriennummer),
  zweite Haelfte ab `pKennung`: veraenderbare Parameter.
- Speicherung: ganze 2 KB im AT45DB (`PARAMETERPAGE`) plus Kopie in Sektor 7 (`parameter_to_progmem()`, IAP
  Loeschen + Schreiben); Schwaechen bei Stromausfall siehe `parameterspeicher.md`.
- **Cloud (MQTT):** `SET` meldet nur eine CRC; das Geraet abonniert `configIn/parameter`, der Server schickt den
  **kompletten Binaerblock** (Laenge muss genau `sizeof(fp)` sein), das Geraet prueft die CRC ueber fuenf Bereiche und
  kopiert den variablen Teil `pKennung..gp` (mqtt.c:1207). `GET` vergleicht Block-CRCs.
- **Parameterdatei** (RS232/USB/BT per XMODEM, USB-Stick): ebenfalls der Binaerblock, 2 KB bzw. 5 KB mit Plus-Daten
  (`Set_parameter()`, sicom.c).
- **Menue am Terminal:** einzelne Werte sind schon heute einstellbar (z. B. Transceivermenue), aber jede Menuestelle
  prueft und speichert auf eigene Weise.

**Folgen:** Der Server muss das genaue Binaerlayout jeder Firmware-Version kennen (Byte-Offsets, Compiler-Alignment);
ein einzelner Wert aendern heisst den ganzen Block ersetzen - mit dem Risiko, andere Werte mit veralteten Server-Daten
zu ueberschreiben. Keine Pruefung der Wertebereiche beim Block-Upload, keine Rechte je Parameter, keine
Protokollierung, wer was geaendert hat.

### Ziel: Parametertabelle

Eine zentrale Tabelle in der Firmware beschreibt jeden Parameter **einmal**:

| Spalte | Beispiel |
|---|---|
| ID (feste Nummer, nie wiederverwenden) | 117 |
| Name (kurz, eindeutig) | `re_vor` |
| Typ, Groesse, Ort in `fp` | uint8, `offsetof(flashparameter, re_vor)` |
| Min / Max / Standardwert | 0 / 99 / 1 |
| Rechte | lesen: alle; schreiben: Benutzer, Cloud, Werk / nur Werk / nie (z. B. Seriennummer) |
| Merkmale | geheim (Passwoerter, PIN: nie im Klartext ausgeben), Neustart noetig, sicherheitsrelevant (Aenderung protokollieren), seit Layout-Version |

Darauf bauen alle Wege auf - **eine** Pruef- und Speicherfunktion fuer Cloud, Terminal, Datei und Menue:

1. **Cloud:** neues Topic `configIn/set` mit Text-Nutzlast `re_vor=1;re_nach=10` (oder kompaktes JSON).
   - Antwort auf `configOut`: je Parameter ok / ausser Bereich / nicht erlaubt, dazu die neue Block-CRC, damit der
     Server synchron bleibt.
   - `get` liefert einzelne Werte oder alle als Text (geheime als `***`).
   - Mehrere Werte in einer Nachricht werden **gemeinsam** uebernommen (alles oder nichts) und einmal gespeichert.
   - Der alte Blockweg bleibt, bis der Server umgestellt ist, laeuft aber intern ueber dieselbe Pruefung.
2. **Terminal/BT/USB:** Befehle `par get <name>`, `par set <name> <wert>`, `par list` (nach PIN-Anmeldung);
   die Menues rufen intern dieselbe Funktion auf.
3. **Parameterdatei als Text** (`name=wert` je Zeile) statt Binaerblock: lesbar, vergleichbar, unabhaengig vom
   Compiler-Layout; unbekannte Namen werden gemeldet und uebersprungen. Binaerdatei weiter lesbar fuer alte Bestaende.
4. **Protokoll:** jede Aenderung mit Quelle (Cloud/Terminal/Datei/Menue), Parameter, alt -> neu (geheime ohne Wert) in
   den Fehlerspeicher/das Protokoll - gehoert zur CRA-Protokollierung.

### Speichern (darunter)

- Intern bleibt `fp` als 2-KB-Block (klein, schnell, kompatibel zu Bootloader/Uebergang). Neu ist nur der Weg dorthin.
- **Zwei abwechselnde Kopien im AT45DB** mit Folgenummer und CRC: geschrieben wird immer die aeltere; ein Stromausfall
  trifft hoechstens die gerade geschriebene, die andere bleibt gueltig. Zusammen mit dem Write-Once-Block aus
  `parameterspeicher.md` (Seriennummer & Co.) geht kein Geraet mehr durch einen Spannungseinbruch verloren.
- Sektor 7 nur noch schreiben, wenn sich der Inhalt wirklich geaendert hat (heute bei jedem `InitParameter()`),
  spart Loeschzyklen und schliesst das Stromausfall-Fenster beim Start.
- **Layout-Version** im Block: neue Firmware ergaenzt neue Parameter in der Reserve mit Standardwert aus der Tabelle;
  das Layout der bestehenden Felder bleibt eingefroren. Beim Werkzeugumstieg (AC6/GCC) sichern
  `_Static_assert(offsetof(...))` fuer jedes Feld, dass sich kein Offset verschiebt.

### Aufwand

Firmware ca. **1,5-2 Wochen** (Tabelle fuer ca. 124 Felder, gemeinsame Pruef-/Speicherfunktion, MQTT-Topics,
Terminalbefehle, Textdatei, A/B-Speicherung, Tests mit Stromunterbrechung). Server-Seite (neues Topic, Textformat)
ist zusaetzlich und nicht geschaetzt. Kann unabhaengig vom Bootloader umgesetzt werden, sinnvoll nach dem
Werkzeugumstieg.

## 6. Uebergang fuer Geraete im Feld (einmalig)

Alte Geraete haben keinen Bootloader; ihr `iap_programming()` schreibt ab Adresse 0 und laesst Sektor 7 aus. Deshalb:

- **Uebergangs-Abbild** = Stufe 0 (ab 0x0, mit CRP2 und gueltiger Vektortabelle) + Bootloader A + Bootloader B leer + Luecke fuer
  Sektor 7 + Statussektor leer + App ab 0x9000, als eine Datei bis 256 KB. Das alte Update schreibt sie wie bisher; `file_is_firmware()`
  sieht im ersten Block den Bootloader mit CRP2 und gueltigen Vektoren -> wird angenommen.
- Sektor 7 (fp-Kopie) und der non_init-RAM bleiben unberuehrt -> Parameter bleiben erhalten.
- Diese eine Datei ist zwangslaeufig **nicht signiert pruefbar** (die alte Firmware kann das nicht) und hat noch das
  heutige Stromausfall-Risiko. Nur ueber eigene Kanaele verteilen, Geraet waehrend des Updates sicher versorgen.
- Der Datenflash-Bereich fuer Slots/Messdaten wird beim ersten Start der neuen App umgestellt.
- Pruefen, dass ein 256-KB-Abbild in den heutigen Download-Bereich und durch alle Wege (XMODEM, Stick, MQTT) passt.

Neue Platinen: in der Fertigung Bootloader + App als ein Abbild per ISP/Debug-Adapter, danach CRP2 aktiv.

## 7. Weitere Haertung (ohne Bootloader-Bezug)

| Punkt | Umbau |
|---|---|
| PIN | keine einheitliche Standard-PIN; Erst-PIN geraeteindividuell (z. B. Etikett) oder beim ersten Einrichten setzen; Sperre mit wachsender Wartezeit nach Fehlversuchen (Review 2.7) |
| BT | Pairing mit Verschluesselung (Secure Connections) im IF820 einrichten, oder begruendete Ausnahme in der Risikoanalyse; Sichtbarkeit wie heute nur bei Bedarf |
| Test-/Diagnosecode | in Serien-Builds entfernen bzw. abschalten: Testmenue-Punkte 8->2..8->7 (u. a. "Modulbefehl senden"), `bt_transfer_echo`, Monitore (Liste in CLAUDE.md "Aufraeumen"); Debug-Schalter `DEBUGGING`/`NOCRPCHECK` nur in Entwicklungsbuilds |
| Protokollierung | sicherheitsrelevante Ereignisse in den Fehlerspeicher: Anmeldung ok/falsch, Sperre, Update angenommen/abgelehnt (Grund), Rueckfall, Parameter geaendert; neue Diagnose-Codes |
| Eingaben | Grenzen pruefen: Menue-Eingaben, MQTT-Nachrichten (`mqbf`-Indizes, URL-Laenge), XMODEM, Dateinamen und FAT-Daten vom Stick |
| LTE/TLS | pruefen, dass das Modem das Server-Zertifikat tatsaechlich prueft (`AT+QSSLCFG` seclevel), Zertifikats-Update absichern; Modem-Firmware-Stand dokumentieren |
| Komponenten | Werkzeugumstieg (gepflegte Middleware, RTX5), SBOM je Version (Compiler, CMSIS, RTX, Middleware, Krypto-Bibliothek, Modem- und BT-Modul-Firmware) |

## 8. Reihenfolge und Aufwand (Schaetzung)

| # | Schritt | Aufwand |
|---|---|---|
| 0 | Werkzeugumstieg (GCC oder AC6, RTX5, MW 8) | 2-3 Wochen |
| 1 | Krypto-Bibliothek waehlen, auf dem Board messen (Flash, Zeit bei 24/100 MHz) | 0,5-1 Tag |
| 2 | Bootloader, Abbild-Format, Signier-Skript | 1-1,5 Wochen |
| 3 | App umstellen (Linker 0x9000, fuenf Update-Wege -> Slot, Messdatenbereich, IAP-Sperre, Bestaetigung) | ca. 1 Woche |
| 4 | Uebergangs-Abbild, Tests: Stromunterbrechung waehrend des Kopierens (Relais am Pruefplatz), Rueckfall, alle Wege, fp- und Messdaten-Erhalt, falsche/alte/fremde Abbilder | 1-1,5 Wochen |
| 5 | PIN, BT-Pairing, Protokollierung, Serien-Build ohne Testcode, Eingabepruefungen | 1-2 Wochen |
| 6 | Parameter-Handling: Tabelle, Einzelwerte ueber Cloud/Terminal/Textdatei, A/B-Speicherung (Abschnitt 5a) | 1,5-2 Wochen |

Software zusammen ca. **10-13 Wochen** (inkl. Bootloader-Updates, Abschnitt 3a, und Parameter-Handling, Abschnitt 5a). Nicht enthalten: Risikoanalyse, Gap-Analyse EN 18031-1/CRA, Prozesse
(Schwachstellenmeldung, Supportzeitraum), Konformitaetsdokumentation.

## 9. Offene Entscheidungen

1. Signaturverfahren (ECDSA P-256 oder Ed25519) und Ort/Verfahren der Schluesselverwaltung.
2. Secure Boot bei jedem Start (Signatur jedes Mal pruefen, +0,2-1,5 s Hochlauf) oder nur beim Update?
3. CRP2 oder CRP3 in der Serie; wie repariert die Werkstatt?
4. Wie werden neue Platinen heute erstmals programmiert (ULINK/uVision oder ISP)?
5. Wann ist ein Update "bestaetigt" (welche Bedingung im Hochlauf)?
6. Erst-PIN: Etikett, Seriennummer-basiert oder Einrichtung beim Kunden?
7. Parameter: Textformat fuer Cloud (Schluessel=Wert oder JSON)? Wer pflegt die Server-Seite? Welche Parameter
   darf die Cloud aendern, welche nur der Benutzer vor Ort, welche nur das Werk?
