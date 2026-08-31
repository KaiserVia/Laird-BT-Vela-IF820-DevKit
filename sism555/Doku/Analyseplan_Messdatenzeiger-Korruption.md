# Analyseplan: Korruption von Feldern im FlashSavedParameter-Block

Stand: 2026-08-31 (ueberarbeitet nach Hex-Dump-Analyse). Bezug: `project-status.md`, Updates
5.96-5.99 sowie den urspruenglichen Eintrag "Offener Firmwarefehler: Messdatenexport kann leere
Flashseiten bis zum Ringpufferende senden". Dieses Dokument haelt fest, wie die Root-Cause-
Untersuchung strukturiert weitergehen soll - die bisherigen Fixes (5.96-5.99) behandeln
Einzelfeld-SYMPTOME (Bounds-Checks + Reset), nicht die URSACHE.

**Wichtige Praezisierung gegenueber der ersten Fassung dieses Dokuments:** Der Titel hiess
urspruenglich "Korruption von md_page/md_start_page/md_adr" - inzwischen erwiesen belegt, dass
die Korruption NICHT auf diese drei Felder beschraenkt ist (siehe Abschnitt 0).

## 0. Neue Evidenz seit der ersten Fassung (Board 269990)

- **Zwei weitere korrupte Felder gefunden**, beide ueber bislang ungeschuetzte Array-Zugriffe
  in der Menuefuehrung entdeckt (nicht ueber gezielte Suche): `fp.hoff` (Anzeigeoptimierung,
  `T_brsel[fp.hoff]` in `parametermenue()`, sicom.c) und mutmasslich `fp.sprache`
  (`T_ja`/`T_nein`/`T_spritm[fp.sprache]` in `sio.c`/`sicom.c`). Beide Zugriffe waren völlig
  ungeklammert (kein Bereichscheck) und fuehrten bei korruptem Feldwert zu endloser
  Garbage-Ausgabe (Lesen weit ausserhalb kleiner, fest dimensionierter Arrays, Ausgabe bis
  zufaellig ein Nullbyte gefunden wird). Mit 5.99 durch dieselbe Art Klammerung wie bereits in
  `sprachauswahl()` behoben - siehe project-status.md Update 5.99.
- **Rohen Parameterblock erstmals per Hex-Dump untersucht** (`Parameter_transfer()`,
  Hauptmenue -> 4 -> `D`, XMODEM-1K/CRC-Empfang). Byte-Offsets manuell aus der
  `FlashSavedParameter`-Struct-Definition (`hard.h`) berechnet und gegen echte Bytes verifiziert:
  - VOR einer vollen Werksinitialisierung: `dmode`=`0x10` (erwartet 0/1), **`sprache`=`0x64`=100
    (erwartet 0-2, direkter Byte-Beweis der Korruption)**. `md_page`-Familie war zu diesem
    Zeitpunkt bereits durch den 5.96/5.97-Fix saniert.
  - NACH `InitParameter(1)`/`werkparameter()`: `dmode`=0, `sprache`=0, **`hoff`=1 - exakt
    passend zu `fp.hoff=1;` in `werkparameter()`**, was zugleich die manuell berechnete
    Offset-Kette als korrekt bestaetigt.
  - Ab Offset ~1740 (E-Mail/APN/Server-Konfigurationsbereich) ein langer, unveraenderter Lauf
    `0xFF` (unbeschriebener/geloeschter Flash-Zustand) - vor UND nach dem Werksinit gleich,
    vermutlich harmlos (Felder fuer ungenutzte Funktionen, siehe Abschnitt 6).
- **Werksinitialisierung hat alle bisher gefundenen korrupten Felder korrekt repariert** -
  bestaetigt durch direkten Vorher/Nachher-Bytevergleich, nicht nur durch Symptomverschwinden.

**Damit kommt eine DRITTE Hypothese hinzu** (siehe Abschnitt 0a), zusaetzlich zu den beiden
bisherigen (Abschnitt 0b/0c).

### 0a. NEUE Hypothese: veraltetes/nie vollstaendig neu initialisiertes Struct-Layout

Das `FlashSavedParameter`-Struct (`hard.h`) ist ueber sehr viele Firmwareversionen gewachsen
(BT/GSM/MQTT/GPS/viaspeedcam-Felder kamen sukzessive dazu, siehe die lange Versionshistorie in
`project-status.md`). `fp.Kennung`/`fp.pKennung` sind einfache feste Magic-Bytes OHNE Bezug zur
Struct-GROESSE oder -VERSION - ein Parameterblock, der zuletzt von einer AELTEREN Firmware mit
kleinerem/anders sortiertem Struct vollstaendig geschrieben wurde (per `werkparameter()`) und
seither nur durch normale Firmware-Updates (NIE durch einen erneuten vollen Werksinit) weiter-
gepflegt wurde, wuerde exakt das beobachtete Bild ergeben: manche Felder korrekt (weil sie schon
in der alten Struct-Version existierten und seither konsistent weitergeschrieben wurden), manche
mit plausibel aussehenden, aber falschen Werten (wenn sich die Feld-REIHENFOLGE geaendert hat -
ein Byte, das frueher zu Feld A gehoerte, wird jetzt als Feld B interpretiert), und manche
schlicht nie beschrieben (`0xFF`, wenn die Struct seither nur GEWACHSEN ist, ohne dass die neuen
Felder je initialisiert wurden). Passt zu ALLEN drei bisher beobachteten Auspraegungen
gleichzeitig, waehrend die beiden aelteren Hypothesen jeweils nur EINEN Mechanismus erklaeren.

**Wichtig:** ein voller Werksinit "repariert" das Symptom bei JEDER der drei Hypothesen
(torn write, Pufferueberlauf, veraltetes Layout) gleichermassen - das erfolgreiche Reparieren
heute ist also kein Beweis FUER diese neue Hypothese, nur ein Beleg, dass der Reparaturweg
funktioniert. Abschnitt 6 listet, wie sich die drei Hypothesen tatsaechlich unterscheiden liessen.

### 0b. Hypothese "Torn Write" (urspruenglich, aus erster Fassung)

`flash_wbuf1()` schreibt den Parameterblock beim Notfall-Speichern (Power-Loss-ISR) UND bei
jedem regulaeren `reboot()` seitenweise, nicht atomar (`maxbyte`=528 Byte/Seite, `sizeof(fp)`
deutlich groesser). Verliert das Geraet waehrend dieser laufenden Schreibsequenz tatsaechlich
die Spannung (oder feuert - bei `reboot()` - der selbst-programmierte Watchdog zu frueh), kann
eine Seite in einem undefinierten Zustand landen (unterbrochener Erase/Programm-Zyklus).
`reboot()` wird sehr viel haeufiger durchlaufen als ein echter Spannungsausfall (BT-Reconfig,
Werksinit, etc.) - deutlich testbarer als die urspruengliche Power-Loss-Variante.

### 0c. Hypothese "Pufferueberlauf aus benachbarten Arrays"

`md_start_page`/`md_page`/`md_adr` und `sprache`/`hoff`/`dmode` liegen alle im selben
allgemeinen Struct-Bereich, teils direkt hinter Arrays fester Groesse (`swno`/`swexp`/`swport`/
`nosw` vor der `md_*`-Gruppe). Ein Off-by-one oder falscher Schleifenbound beim Beschreiben
dieser Arrays koennte in die nachfolgenden Felder hineinschreiben. Noch nicht durch Code-Lesen
verifiziert (siehe Abschnitt 1).

## 1. Was soll analysiert werden

- **Existiert bereits ein Struct-Versions-/Groessen-Stempel**, der bei einem Firmware-Update
  automatisch einen vollen Werksinit erzwingt, wenn sich `sizeof(fp)` oder das Layout geaendert
  hat? (Vermutung: NEIN, nur `Kennung`/`pKennung` als reine Magic-Bytes ohne Versionsbezug -
  zu verifizieren.) Zentral fuer Hypothese 0a.
- **Code, der `swno`/`swexp`/`swport`/`nosw` beschreibt** (`set_exp_switch()` und verwandte
  Funktionen in `libtool.c`/`sicom.c`) auf Bounds-Fehler pruefen - zentral fuer Hypothese 0c.
- **Layout von `struct fp`**: vollstaendige, maschinell verifizierte Offset-Tabelle erstellen
  (die aktuelle Tabelle in Abschnitt 0 wurde von Hand berechnet und nur an zwei Stellen -
  `port`=25, `hoff`=1 - gegen echte Werte verifiziert). Ein kleines Hilfsprogramm/Makro
  (`offsetof()`) waere zuverlaessiger als weiteres Hand-Auszaehlen.
- **Power-Loss-Erkennung** (`EINT0_IRQHandler`) und **`flash_ready()`/`flash_command()`-Timing**:
  wie viel Reservezeit bleibt, reicht das fuer die volle `flash_wbuf1()`-Sequenz? (weiterhin
  relevant fuer Hypothese 0b, unveraendert aus der ersten Fassung.)
- **Ist die Notfall-Speicherroutine/`reboot()` die EINZIGEN Stellen**, an denen der externe
  Flash-Parameterblock geschrieben wird? (unveraendert relevant.)

## 2. Was soll getestet werden

- **Parameterblock-Hex-Dump als Standardwerkzeug etablieren** (NEU, bereits erfolgreich genutzt):
  `Parameter_transfer()` (Hauptmenue -> 4 -> `D`, 30 s Fenster, XMODEM-1K/CRC-Empfang) liefert
  den rohen Block ohne Hardwarezugriff/Bench-Test. Deutlich schneller als ein Spannungsausfall-
  Test und liefert die Byte-genaue Wahrheit statt indirekter Symptome. Vor JEDER weiteren
  Untersuchung: aktuellen Zustand eines auffaelligen Boards so sichern, BEVOR irgendetwas
  daran veraendert wird (kein Werksinit, kein Reflash) - genau das fehlte bisher immer.
- **Auf einem NEUEN, nie zuvor initialisierten Board** (frisch ab Werk, garantiert mit
  aktuellem Struct-Layout beschrieben) denselben Hex-Dump ziehen und mit einem "alten" Board
  vergleichen - direkter Test von Hypothese 0a: wenn das neue Board an denselben Offsets
  durchgehend saubere Werte zeigt und ein altes, lange gelaufenes Board nicht, stuetzt das
  die Layout-Hypothese stark.
- **Kontrollierter Spannungsausfall-Test am Labornetzteil** (aus der ersten Fassung,
  unveraendert relevant fuer Hypothese 0b): Spannung waehrend eines laufenden Notfall-Saves
  gezielt kappen, Hex-Dump vorher/nachher vergleichen.
- **`reboot()` gezielt oft ausloesen** (z. B. wiederholte BT-Reconfig) und nach jedem Aufruf
  einen Hex-Dump ziehen - viel einfacher zu wiederholen als ein Spannungsausfall-Test, deckt
  Hypothese 0b ab, falls der Watchdog dabei tatsaechlich zu frueh feuert.

## 3. Was soll geaendert werden

**Empfehlung nach der Hex-Dump-Analyse (Aenderung gegenueber der ersten Fassung):** die
CRC-basierte Variante ist nicht mehr nur "robuster, aber aufwendiger" - sie ist die einzige der
drei urspruenglichen Optionen, die AUCH Hypothese 0a (veraltetes Layout) und 0c (Pufferueberlauf)
abdeckt, nicht nur 0b (torn write). Ein CRC-Mismatch ist unabhaengig von der tatsaechlichen
Ursache ein zuverlaessiges Signal "diesem Block ist nicht zu trauen".

- **Empfohlen: CRC-16 ueber den gesamten `fp`-Block**, gespeichert als Teil des Blocks, bei
  jedem Laden verifiziert (`InitParameter()`, gleichrangig zum bestehenden `Kennung`/
  `pKennung`-Vergleich behandelt - bei Mismatch derselbe `werkparameter()`/`default_parameter()`-
  Neuaufbau wie bei ungueltiger Kennung). Neuer Diagnose-Code vorgeschlagen: `F16004
  Parameterblock-Pruefsummenfehler` (siehe project-status.md, letztes Update). Muss an ALLEN
  Schreibstellen ergaenzt werden: `reboot()` (libtool.c), Power-Loss-ISR (flash.c), Parameter-
  Upload-Pfad (`Parameter_transfer()`/`Set_parameter()`, sicom.c) - vor der Umsetzung alle drei
  Stellen auflisten und pruefen (Lehre aus 5.89-5.92, siehe `feedback_shared_state_review`).
- **Ergaenzend, falls Hypothese 0a sich bestaetigt:** `pKennung` (oder ein neues Feld) an
  `sizeof(fp)` oder eine explizite Struct-Versionsnummer koppeln, die bei jeder Struct-Aenderung
  im Code manuell erhoeht wird - erzwingt automatisch einen Werksinit auf alten Boards nach
  einem Firmware-Update mit geaendertem Layout, unabhaengig von einer CRC.
- **Kleinste Einzelmassnahme (bereits so gehandhabt bei den bisherigen Funden):** ungeschuetzte
  Array-Zugriffe auf `fp.*`-Enum-/Index-Felder weiter einzeln klammern, sobald sie auffallen
  (wie in 5.99) - reicht als alleinige Strategie nicht aus (siehe Abschnitt 0: zwei Felder
  wurden nur durch Zufall beim Navigieren gefunden, nicht durch systematische Suche), aber
  guter, risikoarmer Sofortschutz bis die CRC-Loesung steht.
- **Zwei-Slot-Speicherung (Double-Buffering):** weiterhin als aufwendigste, aber gruendlichste
  Option verfuegbar, falls die CRC-Loesung allein sich als unzureichend erweist (z. B. wenn
  torn writes so haeufig sind, dass ein staendiger Werksinit-Reset selbst zum Problem wird).
- **Ausdruecklich NICHT vorschnell entscheiden:** vor Umsetzung Abschnitt 1 (Analyse,
  insbesondere die Frage nach einem bestehenden Versionsstempel) abschliessen.

## 4. Wie kann das validiert werden

- **Hex-Dump-Vergleich als primaere Validierungsmethode** (staerker als reine Symptom-
  Beobachtung): vor und nach jeder Aenderung denselben Parameterblock-Dump ziehen und Byte fuer
  Byte vergleichen, nicht nur einzelne Menuewerte pruefen.
- **Neues Board vs. altes Board:** wenn die Layout-Hypothese (0a) zutrifft, sollte ein
  brandneues, nie aktualisiertes Board niemals `F16004` (CRC-Fehler) zeigen, waehrend alte,
  ueber viele Versionen gelaufene Boards es zumindest einmalig zeigen sollten (danach durch den
  automatischen Reinit behoben).
- **Regressionstest:** die bestehende `ble_mainboard_test.py`-Testsuite (Cycle/Download) muss
  weiterhin fehlerfrei durchlaufen - jede Aenderung an `InitParameter()`/den Schreibpfaden
  beruehrt sicherheitsrelevanten Boot-Code.
- **Langzeitbeobachtung im Feld:** `F16003`/`F16004` ueber mehrere Boards und Wochen beobachten.

## 5. Aufgabenverteilung nach Claude-Modell

- **Sonnet (dieses Modell, laufende Session):** Code-Archaeologie (Abschnitt 1), Hex-Dump-
  Analyse (bereits erfolgreich demonstriert - Offset-Berechnung, Byte-Vergleich vor/nach
  Werksinit). Reine Lese-/Rechercheaufgabe bzw. Dateninterpretation, kein Hardwarezugriff noetig.
- **Opus (fuer die CRC-Fix-Entscheidung und -Umsetzung, sobald Abschnitt 1 abgeschlossen ist):**
  bewusst ein staerkeres Modell, weil hier die Lehre aus den 5.89-5.92-Regressionen direkt
  greift - ein Fix an gemeinsam genutztem Boot-/Flash-Zustand, der an DREI verschiedenen
  Schreibstellen konsistent ergaenzt werden muss, darf nicht iterativ ueber mehrere fehlerhafte
  Versionen gefunden werden. Anschliessend eigene Code-Review-Runde (`code-review`-Skill) auf
  dem fertigen Diff, bevor gebaut/geflasht wird.
- **Haiku (fuer mechanische/Wiederholungsaufgaben):** wiederholte Hex-Dump-Ziehungen (Parameter-
  block-Download ueber mehrere `reboot()`-Zyklen oder mehrere Boards) protokollieren und
  tabellarisch in `project-status.md` nachtragen - keine Architekturentscheidungen.
- **Mensch (Hardware-Zugriff zwingend erforderlich):** Parameterblock-Downloads am jeweiligen
  Board ausloesen und Dump-Dateien liefern (inzwischen der Standardweg, kein Labornetzteil
  mehr fuer die meisten Tests noetig); der Spannungsausfall-Bench-Test (Hypothese 0b) bleibt
  die einzige Ausnahme, die echten Hardwarezugriff mit Netzteil braucht.

## 6. Offene Zusatzfragen

- **Wie unterscheiden sich die drei Hypothesen empirisch voneinander?** Ein neues, nie
  aktualisiertes Board sollte unter 0a niemals Korruption zeigen (unabhaengig von `reboot()`-
  Haeufigkeit), unter 0b/0c aber schon (rein code-/timing-abhaengig, nicht versionsabhaengig).
  Das waere der klarste Unterscheidungstest.
- Warum wurde dieses Verhalten nicht schon frueher beobachtet, wenn der Mechanismus so
  grundsaetzlich ist? Passt gut zu 0a (nur Boards mit LANGER Update-Historie betroffen) -
  passt weniger gut zu 0b/0c (die muessten unabhaengig vom Board-Alter gleich haeufig auftreten).
- Ist die Notfall-Speicherroutine/`reboot()` wirklich die einzige Stelle, an der der externe
  Flash-Parameterblock beschrieben wird?
- Wie verhaelt sich `fp_cp_area` (die LPC-interne Kopie, `parameter_to_progmem()`) - wird sie
  VOR oder NACH dem externen Flash-Save aktualisiert, und koennte sie als zusaetzliche
  Plausibilisierungsquelle dienen?
- Der anhaltende `0xFF`-Lauf im E-Mail/APN/Server-Bereich (Offset ~1740+, siehe Abschnitt 0):
  legitim (Felder fuer ungenutzte Funktionen) oder ebenfalls ein Symptom von Hypothese 0a
  (Felder, die in der urspruenglich initialisierenden Firmware-Version noch gar nicht
  existierten)? Liesse sich klaeren, indem geprueft wird, ob `werkparameter()`/
  `default_parameter()` diese Felder ueberhaupt jemals explizit beschreiben (falls nicht, waere
  `0xFF` dort IMMER zu erwarten, auch bei einem taufrischen Board - dann harmlos).
- Sollte `pKennung` um eine echte Versionsnummer erweitert werden (siehe Abschnitt 3), die bei
  jeder strukturellen Aenderung an `FlashSavedParameter` manuell hochgezaehlt wird?
