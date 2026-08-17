# Antwort auf `ble-retest-1-negative.md` — eure Frage 4 ist vermutlich der Treffer

> **Adressat:** App-Seite (Flutter `viagraph_app`, iOS).
> **Absender:** Firmware-Seite (sism555, IF820).
> **Stand:** 2026-07-30, Geräte-Firmware **5.59**.
>
> **Kurzfassung:** *Danke für den sauberen Bericht — besonders für Frage 4. Ich halte sie für
> die richtige Spur: Unser Fix stellt sicher, dass wir die CYSPP-Leitung nicht mehr **HIGH**
> halten, aber wir treiben sie für eine BLE-Session auch nicht aktiv **LOW** — sie floatet.
> Für Classic SPP ist das egal (da treibt das **Modul** die Leitung selbst), für BLE-CYSPP
> vermutlich nicht. Das erklärt exakt „Classic läuft, BLE nicht".*

---

## Antworten auf eure fünf Fragen

**1. Firmware-Stand auf genau diesen Einheiten**

Verifizierbar **nur über das serielle Menü** (RS232), nicht über BLE/Advertising — das ist eine
Lücke, die wir schließen sollten. Zwei Stellen:
- Beim Booten: `BT init (fw 5.59): command mode ok`
- Im Hauptmenü-Kopf: `Viasis 3003M - Version 5.59`

**Wichtiger Verdacht zu eurem Test:** Wir haben in dieser Session **nur `26SC9996`** entwickelt
und geflasht. Board **`26SC9993`** ist mit hoher Wahrscheinlichkeit **nicht** auf 5.51+ — dort
wäre ein negativer Retest zu erwarten und sagt nichts über den Fix aus. Bitte prüft für beide
Boards die Version, bevor wir sie als „zwei unabhängige Belege" werten.

**2. Boot-Release wirklich erfolgt?**

Ja, seit 5.51 gibt `main.c` die Leitung **vor** dem ersten Interface-Status frei, für **jeden**
Reset-Grund (auch nach Reflash/Deep-Sleep — der Port-Expander behält sein Konfigurationsregister
sonst über einen Reflash hinweg). Ein echter Power-Cycle ist dafür nicht nötig, schadet aber
nicht.

**3. UART1-Owner zum Testzeitpunkt**

Auf unserem Prüfplatz **nein** — dort ist kein Modem/keine SIM (`F12001 Modem keine Antwort`),
UART1 liegt also frei. Zusätzlich ist der GSM-Task gesperrt, solange `BT_LINK` gesetzt ist. Auf
euren Boards kann das anders sein; falls dort GSM/GPS dauerhaft läuft, ist euer Einwand berechtigt
und wir müssten das getrennt betrachten.

**4. Wird der Pin aktiv LOW getrieben oder nur „released"? — DAS IST DER PUNKT**

**Nur released.** Unser Fix macht ausschließlich: „nicht mehr HIGH treiben" (Leitung auf Eingang,
hochohmig). Wir haben **keine** Funktion, die aktiv LOW treibt — im ganzen Code gibt es nur
`bt_cmdmode()` (HIGH treiben) und `bt_release()` (Eingang/floaten).

Warum das für Classic reicht, für BLE aber vermutlich nicht:
- **Classic SPP:** Beim Verbindungsaufbau zieht das **Modul selbst** die Leitung LOW (belegt in
  unseren Messungen und im DVK-Guide). Sie ist dann definiert → Datenmodus läuft.
- **BLE-CYSPP:** Wenn das Modul die Leitung hier **nicht** selbst treibt, sondern sie beim Start
  des Datenmodus als **Eingang abfragt**, dann liegt sie bei uns **floatend/undefiniert** an —
  und ein als HIGH gelesener Pegel heißt „de-asserted / Datenkanal beenden". Ergebnis: CYSPP endet
  **immer** genau beim Subscribe. Exakt euer Symptom, exakt reproduzierbar, unabhängig vom
  ursprünglichen Latch-Bug.

Ihr habt damit sehr wahrscheinlich einen **zweiten, unabhängigen Fehler** gefunden — der erste
(dauerhaft HIGH verriegelt) war real und ist gefixt, er hat den zweiten nur verdeckt.

**5. Classic vs. BLE — gleicher Pin-Pfad?**

Gleiche **physische Leitung**, aber unterschiedliche **Treiber-Situation** — siehe 4. Unser Beleg
„Leitung klemmt nicht mehr" stammt vom Classic-Massendownload und deckt den BLE-Fall deshalb
**nicht** ab. Euer Einwand ist berechtigt; ich hätte das im letzten Dokument klarer trennen müssen.

---

## Warum wir das nicht in fünf Minuten fixen können (Ehrlichkeit statt Schnellschuss)

Die naheliegende Lösung „dann treibt die Leitung eben aktiv LOW" hat bei uns einen Haken: **Bit 7
des Port-Expanders hat Doppelfunktion.** Wir benutzen dieselbe Leitung als **Eingang**, um eine
BT-Verbindung überhaupt zu **erkennen** (`BT_LINK` → Terminal-Routing, Startmeldung, PIN-Prompt,
Timeout). Treiben wir sie dauerhaft LOW, liest die Firmware „immer verbunden" und das
Verbindungsmanagement bricht zusammen.

Es braucht also eine saubere Lösung, nicht einen schnellen Hack. Kandidaten, die wir prüfen:
- CYSPP für eine BLE-Session **gezielt und zeitlich begrenzt** LOW treiben, statt dauerhaft.
- Verbindungserkennung von diesem Pin **entkoppeln** (z. B. über die separate `CONNECTION`-Leitung
  des Moduls, die bei uns bisher unverdrahtet ist) — dann darf CYSPP reiner Steuerausgang sein.
- Prüfen, ob sich der Datenmodus modulseitig **ohne** Pin-Assert erlauben lässt (CYSPP-Parameter),
  sodass das Subscribe allein genügt.

## Was wir von euch bräuchten (kostet euch wenig, hilft uns sehr)

1. **Firmware-Version beider Boards** (siehe Antwort 1) — insbesondere, ob `26SC9993` überhaupt
   auf 5.51+ läuft.
2. **Ein Retest auf `26SC9996`** (das Board, das nachweislich 5.59 hat), damit wir einen sauberen
   Einzelbeleg statt zwei möglicherweise unterschiedlicher Stände haben.
3. Falls möglich: die Zeit zwischen `subscribe ok` und `⚡` (Millisekunden). Ein *sofortiger*
   Abbruch spricht für den Pin-Pegel; ein Abbruch nach **hunderten Millisekunden** eher für etwas
   anderes (z. B. Supervision-Timeout).

## Zum Schluss: was inzwischen unabhängig davon funktioniert

Damit ihr den Stand einordnen könnt — auf der Classic-/Modulseite ist seit eurem letzten Bericht
einiges passiert (Durchsatz von ~1 kB/s auf 4 MB in unter 15 min, dauerhaftes Advertising,
Just-Works-Security aktiv festgeschrieben, Class of Device korrigiert). Für **euren** Pfad ändert
das nichts — der BLE-Datenkanal bleibt der offene Punkt, und er liegt bei uns, nicht bei euch.
Eure Vorarbeit (subscribe-first, Probe im Code) passt und bleibt gültig.
