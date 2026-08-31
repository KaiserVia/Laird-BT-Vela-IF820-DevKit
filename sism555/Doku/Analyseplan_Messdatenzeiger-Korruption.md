# Analyseplan: Korruption von md_page/md_start_page/md_adr

Stand: 2026-08-31. Bezug: `project-status.md`, Updates 5.96/5.97 sowie den urspruenglichen
Eintrag "Offener Firmwarefehler: Messdatenexport kann leere Flashseiten bis zum Ringpufferende
senden". Dieses Dokument haelt fest, wie die Root-Cause-Untersuchung strukturiert weitergehen
soll - der eigentliche Fix (5.96/5.97) behandelt bisher nur das SYMPTOM (Bounds-Check + Reset),
nicht die URSACHE.

## Ausgangslage (kurz)

Auf Board 269990 wurde bewiesen: `fp.md_start_page=30913` bei einem 4-MB-Flash-Chip mit nur
`maxpage=8191` gueltigen Seiten - eindeutig eine korrupte, unmoegliche Zeigerposition. Leitende
Hypothese: `flash_wbuf1()` schreibt den Parameterblock beim Notfall-Speichern (Power-Loss-ISR)
seitenweise, nicht atomar (`maxbyte`=528 Byte/Seite, `sizeof(fp)` deutlich groesser). Verliert das
Geraet waehrend dieser laufenden Schreibsequenz tatsaechlich die Spannung, kann eine Seite in
einem undefinierten Zustand landen (unterbrochener Erase/Programm-Zyklus). Noch NICHT verifiziert.

## 1. Was soll analysiert werden

- **Layout von `struct fp`** (wo genau liegen `md_page`/`md_start_page`/`md_adr` innerhalb des
  Structs, in welcher der mehreren `flash_wbuf1()`-Teilschreibungen wuerden sie geschrieben -
  fruehe oder spaete Seite der Sequenz?). Bestimmt, wie wahrscheinlich/wie frueh ein Spannungs-
  verlust waehrend des Notfall-Saves genau diese Felder treffen wuerde.
- **Power-Loss-Erkennung selbst** (`EINT0_IRQHandler`, vermutlich in `hard.c` oder `main.c`):
  Wie wird der Spannungsausfall erkannt (Komparator-Pin? ADC-Schwelle?), wie viel Reservezeit/
  -energie bleibt typischerweise (Kondensatorgroesse, dokumentierte Vorwarnzeit), reicht das
  fuer die volle `flash_wbuf1()`-Sequenz oder ist ein Abbruch mitten in der Sequenz plausibel
  oder sogar der Normalfall?
- **`flash_ready()`/`flash_command()`-Timing:** wie lange dauert ein einzelner Erase+Programm-
  Zyklus real (Datenblatt AT45DB321/161 pruefen), wie oft wird die komplette `fp`-Struktur
  tatsaechlich seitenweise zerlegt (```ceil(sizeof(fp)/maxbyte)``` Seiten)?
- **Wie oft/wann wird der Notfall-Save ueberhaupt ausgeloest** - nur bei echtem Netzausfall,
  oder auch bei anderen Ereignissen (Batteriewechsel am Feld, Wackelkontakt Stromversorgung)?
  Haeufigkeit ist wichtig, um einzuschaetzen, ob dieser Pfad oft genug durchlaufen wird, um die
  auf mehreren Boards beobachtete Korruption zu erklaeren.
- **Gibt es bereits eine Absicherung** (Kennung-Doppelpruefung, CRC, Sequenznummer) fuer den
  Parameterblock, die eine torn write eigentlich erkennen sollte, aber hier versagt hat? (Bisher
  bekannt: nur `fp.Kennung`/`fp.pKennung` als einfache Magic-Value-Pruefung, keine CRC ueber den
  gesamten Block - zu bestaetigen.)
- **Sind `fp.pro_page`/`fp.pro_start_page`** (Protokollzeiger, gleiches Struct, gleicher Save-Pfad)
  ebenfalls schon einmal unplausibel beobachtet worden? Falls ja, staerkt das die Hypothese
  (systemisches Problem des Save-Mechanismus, nicht spezifisch fuer die Messdaten-Zeiger).

## 2. Was soll getestet werden

- **Kontrollierter Spannungsausfall-Test am Labornetzteil:** Geraet unter definierter Last
  betreiben, Spannung waehrend eines LAUFENDEN Notfall-Saves gezielt/wiederholt kappen (ggf.
  mehrfach mit leicht unterschiedlichem Timing), danach `MD_start`/`md_page`/`md_adr` ueber die
  bereits vorhandenen, rein lesenden Diagnosewege auslesen (Hauptmenue->1, Funkmodems->2->
  Ctrl-G->5) und mit dem Zustand VOR dem Test vergleichen. Ziel: Reproduktion der Korruption
  unter kontrollierten Bedingungen statt nur im Feld beobachtet.
- **Wiederholbarkeit:** mehrere Durchlaeufe auf demselben Board, um zu sehen, ob die Korruption
  reproduzierbar in einer bestimmten Zeitfensterlage relativ zum Zyklusstart auftritt (deutet auf
  eine bestimmte Seite/ein bestimmtes Feld) oder zufaellig verteilt ist.
- **Test mit 5.97 bereits geflasht:** denselben Spannungsausfall-Test wiederholen und pruefen, ob
  `F16003` im Protokoll erscheint (Bounds-Check greift) und ob der Reset auf saubere Werte
  tatsaechlich zuverlaessig funktioniert, auch wenn der torn write selbst weiter auftritt.
- **Test der bestehenden Kennung-Pruefung:** gezielt einen torn write in der Mitte der Sequenz
  erzwingen und pruefen, ob `fp.Kennung`/`fp.pKennung` dabei zufaellig VALID bleiben (waere der
  Beweis, dass die aktuelle Absicherung torn writes grundsaetzlich nicht zuverlaessig erkennt).

## 3. Was soll geaendert werden (Kandidaten, noch NICHT entschieden)

- **Kleinste Aenderung:** kritische Ringpuffer-Zeiger (`md_page`/`md_start_page`/`md_adr`,
  ggf. auch `pro_page`/`pro_start_page`/`pro_adr`) VOR dem Rest des Parameterblocks und in einem
  EIGENEN, moeglichst kleinen/schnellen Schreibvorgang sichern - maximiert die Chance, dass
  gerade diese Felder bei einem vorzeitigen Spannungsverlust bereits vollstaendig geschrieben
  sind, bevor die Zeit ausgeht.
- **Robuster, aber aufwendiger:** CRC oder Sequenznummer ueber den gesamten `fp`-Block (oder
  zumindest ueber die Ringpuffer-Zeiger separat), bei jedem Boot verifiziert - bei Mismatch
  gilt der Block als torn/ungueltig, unabhaengig davon ob die einzelnen Werte zufaellig noch in
  einem "plausiblen" Zahlenbereich liegen (der aktuelle 5.96/5.97-Bounds-Check erkennt NUR
  Werte ausserhalb des gueltigen Seitenbereichs, keine subtileren Bitfehler innerhalb des
  gueltigen Bereichs).
- **Am aufwendigsten:** Zwei-Slot-Speicherung (Double-Buffering) mit Sequenznummer, wie in
  vielen embedded Systemen fuer genau dieses Torn-Write-Problem ueblich - schreibt abwechselnd
  in zwei Bereiche, der jeweils aeltere/gueltige Slot bleibt bei einem Absturz waehrend des
  Schreibens des anderen Slots unberuehrt. Groesster Umbau, geringstes Risiko fuer zukuenftige
  Faelle dieser Fehlerklasse.
- **Ausdruecklich NICHT vorschnell entscheiden:** welche dieser Optionen (oder Kombination)
  sinnvoll ist, haengt von Schritt 1 (Analyse) und Schritt 2 (Testergebnisse) ab - nicht ohne
  diese Grundlage implementieren, siehe Lehre aus der 5.89-5.92-Regressionskaskade in diesem
  Projekt (unbedachte Aenderungen an gemeinsam genutztem Zustand haben wiederholt neue,
  schwerer auffindbare Bugs erzeugt).

## 4. Wie kann das validiert werden

- **Vor jeder Aenderung:** Nachbau des kontrollierten Spannungsausfall-Tests (Abschnitt 2) als
  wiederholbares Testprotokoll, damit Vorher/Nachher vergleichbar sind.
- **Nach der Aenderung:** derselbe Test darf die Korruption nicht mehr reproduzieren (oder,
  falls ein Bounds-/CRC-Check gewaehlt wird, muss er sie zuverlaessig ERKENNEN und sauber
  abfangen, auch wenn der torn write selbst nicht verhindert werden kann).
- **Regressionstest:** die bereits bestehende `ble_mainboard_test.py`-Testsuite (Cycle/Download,
  siehe fruehere Updates) muss weiterhin fehlerfrei durchlaufen - jede Aenderung an
  `InitParameter()`/`flash_wbuf1()` beruehrt sicherheitsrelevanten Boot-Code.
- **Langzeitbeobachtung im Feld:** `F16003` (und ein etwaiger neuer CRC-Fehlercode) im Protokoll
  mehrerer Boards ueber Wochen beobachten - haeufiges Auftreten wuerde die Hypothese stark
  stuetzen, seltenes/keinAuftreten wuerde sie schwaechen bzw. auf einen selteneren Ausloeser
  hindeuten.

## 5. Aufgabenverteilung nach Claude-Modell

- **Sonnet (dieses Modell, laufende Session):** Code-Archaeologie (Abschnitt 1) - Struct-Layout
  bestimmen, `EINT0_IRQHandler`/Power-Loss-Erkennung lokalisieren und lesen, Timing-Konstanten
  aus dem Code extrahieren, bestehende Kennung/CRC-Absicherung pruefen. Reine Lese-/Rechercheaufgabe,
  kein Hardwarezugriff noetig - passt zum aktuellen Modell und Kontext dieser Session.
- **Opus (fuer die Fix-Entscheidung und -Umsetzung, sobald Abschnitt 1+2 abgeschlossen sind):**
  bewusst ein staerkeres Modell fuer diesen Schritt, weil hier die Lehre aus den 5.89-5.92-
  Regressionen direkt greift - ein Fix an gemeinsam genutztem Boot-/Flash-Zustand muss von Anfang
  an alle Verbraucher durchdenken (siehe `feedback_shared_state_review`-Merkregel), nicht iterativ
  ueber mehrere fehlerhafte Versionen gefunden werden. Sorgfaeltiges, langsameres Modell fuer die
  eigentliche Fix-Entscheidung (Abschnitt 3) und Implementierung ist hier gerechtfertigt.
  Anschliessend eigene Code-Review-Runde (z. B. `code-review`-Skill) auf dem fertigen Diff, bevor
  gebaut/geflasht wird.
- **Haiku (fuer mechanische/Wiederholungsaufgaben):** Testprotokolle wiederholt ausfuehren und
  Ergebnisse strukturiert protokollieren (sobald ein Testskript/Ablauf feststeht), z. B. mehrere
  Spannungsausfall-Testdurchlaeufe protokollieren und die ausgelesenen Diagnosewerte tabellarisch
  in `project-status.md` nachtragen - keine Architekturentscheidungen, nur Datenerfassung.
- **Mensch (Hardware-Zugriff zwingend erforderlich):** der eigentliche Spannungsausfall-Test
  (Labornetzteil, definiertes Timing) kann kein Claude-Modell selbst ausfuehren - das Ergebnis
  (Diagnosewerte vor/nach) muss jeweils zurueckgemeldet werden, damit die Session (Sonnet/Opus)
  es auswerten kann.

## 6. Offene Zusatzfragen

- Warum wurde dieses Verhalten nicht schon frueher (auf anderen Boards/Projekten mit demselben
  Speicher-Subsystem) beobachtet, wenn der Mechanismus so grundsaetzlich ist? Ist die Haeufigkeit
  von echten Spannungsausfaellen im Feld bei diesem Geraetetyp vielleicht bisher einfach gering
  gewesen, oder gibt es einen zusaetzlichen, board-spezifischen Faktor (z. B. eine schwaechere/
  fehlende Pufferung der Versorgungsspannung auf GENAU den betroffenen Boards 269990/269992)?
- Ist die Notfall-Speicherroutine ueberhaupt die EINZIGE Stelle, an der der externe Flash-
  Parameterblock beschrieben wird, oder gibt es einen weiteren, regulaeren (nicht notfall-
  getriebenen) Speicherpfad, der ebenfalls von einem unguenstigen Timing betroffen sein koennte?
- Wie verhaelt sich `fp_cp_area` (die LPC-interne Kopie, `parameter_to_progmem()`) in diesem
  Szenario - wird sie VOR oder NACH dem externen Flash-Save aktualisiert, und koennte sie als
  zusaetzliche Plausibilisierungsquelle dienen (aehnlich der GPREG-Idee aus dem vario245-Incident),
  ohne die Kernursache zu beheben?
- Gibt es eine Moeglichkeit, die verbleibende Energie beim Power-Loss-Ereignis zu MESSEN (statt nur
  zu vermuten), um zu bestaetigen, ob die volle `flash_wbuf1()`-Sequenz normalerweise ausreichend
  Zeit hat oder strukturell zu knapp bemessen ist?
- Sollte die Reihenfolge, in der Felder im `fp`-Struct deklariert sind, bewusst so umsortiert
  werden, dass sicherheitsrelevante/haeufig gelesene Zeiger IMMER in der zuerst geschriebenen
  Seite landen - unabhaengig davon, welche der Fix-Optionen aus Abschnitt 3 gewaehlt wird?
