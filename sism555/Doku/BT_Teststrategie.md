# Teststrategie BT-Module: IF820-Verhalten gegen Regressionen sichern

> **Anlass:** Beim Bearbeiten von RN4678 / Laird BT730 darf das IF820-Verhalten nicht
> unbemerkt kaputtgehen — und umgekehrt. Bug 5.51 (CYSPP blieb als Ausgang HIGH hängen)
> lag **wochenlang unentdeckt** und wurde erst von der App-Seite als BLE-Symptom gemeldet.
> Genau diese Klasse soll künftig automatisch auffallen.
>
> **Stand:** 2026-07-30, nach Version 5.52. Noch nichts davon umgesetzt — Entscheidungsvorlage.

---

## 1. Wo liegt das Risiko wirklich?

Nicht in den modulspezifischen Kommandostrings (`SDN$`, `SN,`, `AT+BTN`) — die sind sauber
per `switch (fp.btmodem)` getrennt und stören sich nicht. Das Risiko liegt im **geteilten
Code**, den alle drei Module benutzen:

| # | Geteilte Ressource | Warum kritisch | Bereits real getroffen |
|---|---|---|---|
| K1 | **IC54 Bit 7** (`BT_LINK`) | Dieselbe physische Leitung für alle drei Module, aber **unterschiedliche Semantik**: RN4678/BT730 = reiner Status-Ausgang des Moduls, IF820 = bidirektionale CYSPP-Steuerleitung | **Ja** — 5.51, 5.52 |
| K2 | **UART1-Owner-Statemachine** (`uart1_request`/`uart1_release`, hard.c) | BT, GSM und GPS teilen UART1 über einen Analog-MUX. Reihenfolge- und Owner-Logik ist modulübergreifend | **Ja** — 5.48, 5.51 |
| K3 | **`connect`-Bitfeld + `concpy`** | Wird aus mehreren Threads und aus `bt_command()` heraus manipuliert; ein falscher Wert zerlegt das Terminal-Routing | **Ja** — 5.49 |
| K4 | **`BT_BAUD`-Makro** (btio.h) | Ternär über alle drei Module; eine Änderung für ein Modul trifft die Klammerung aller | nein |
| K5 | **PIN-/Startmessage-Flow** (`bt_pininit`, libtool.c) | Gemeinsamer Pfad für RN4678 **und** IF820 (`fp.btmodem==Roving \|\| fp.btmodem==IF820`) | nein |
| K6 | **Erkennungsreihenfolge** in `init_bluetooth()` | Die Module werden sequenziell geprobt (Baud-Scans, `/PING`, `$$$`, AT). Ein neuer Probe-Schritt für Modul X kann Modul Y verschatten oder dessen Parser verstellen | nein |
| K7 | **Timeout-/Disconnect-Pfad** (`BT_TIMEOUT`, `bt_disconnect`) | Modulspezifisch verzweigt, aber vom gemeinsamen Minutentick getrieben | **Ja** — 5.47b, 5.49 |

**Konsequenz:** Ein Test, der nur „verbindet sich das IF820?" prüft, ist der falsche Hebel.
Die Tests müssen an K1–K3 ansetzen — dort sind alle bisherigen Bugs entstanden.

## 2. Was heute existiert (und was daran fehlt)

| Vorhanden | Lücke |
|---|---|
| `Doku/BT-Evalkit-Tasks.md` — Testplan Test 1–12 | Prosa, manuell, kein Ergebnisprotokoll, nichts erzwingt die Ausführung |
| `Doku/UART1_Owner_StateMachine.md` — Mermaid-Diagramme des Sollverhaltens | Dokumentiert, aber **nicht nachgeprüft** — der 5.51-Bug widersprach dem Diagramm, ohne dass es auffiel |
| `bt_cyspp_monitor()` im Testmenü — Live-Anzeige IC54 Bit 7 | Nur Anzeige, kein PASS/FAIL, kein Szenario-Ablauf |
| `send_bt_info()` — Modulparameter auslesen | Momentaufnahme, kein Sollwertvergleich |
| Diagnose-Codes F/W/I + Fehlerspeicher | Meldet Symptome im Feld, ersetzt aber keinen Vorab-Test |
| **Nichts** | Kein automatisierter Test, kein CI, kein Ergebnis-Log, keine Regressionsschranke |

**Kern der Lücke:** Es gibt keinen einzigen Test, den man in unter einer Minute laufen lassen
kann. Alles verlangt Hardware, ein Keil-Build und einen Menschen, der Ausgaben interpretiert.
Deshalb wird faktisch nichts getestet, wenn der Zeitdruck steigt.

---

## 3. Vorschlag A — PC-Unit-Tests der geteilten Zustandslogik (Off-Target)

**Idee:** Die UART1-Owner-Statemachine und die CYSPP-Pin-Logik gegen **Stubs** auf dem PC
kompilieren (gcc) und Szenarien als Eventfolgen durchspielen. Der PCA9554 wird zum Fake, der
Config- und Output-Register mitschreibt; die Tests behaupten danach Pegel und Owner.

**Beispiel — genau der 5.51-Bug, in Testform:**

```c
TEST(cyspp_freed_when_bt_link_lost_during_gsm_session)
{
  fake_reset(); fp.btmodem = IF820;
  connect |= BT_LINK;
  uart1_request(MUX_BT,  460800);   ASSERT_PIN_INPUT(7);   // Modul darf treiben
  uart1_request(MUX_GSM, 460800);   ASSERT_PIN_OUT_HIGH(7);// Verdraengung: CYSPP HIGH (gewollt)
  connect &= ~BT_LINK;                                     // BT-Link faellt weg
  uart1_release(MUX_GSM);           ASSERT_PIN_INPUT(7);   // <-- schlug vor 5.51 fehl
}
```

**Testfälle (Startumfang, ~15 Stück):**

| ID | Szenario | Erwartung | Schützt |
|---|---|---|---|
| A1 | BT verbunden → GSM holt UART1 → `BT_LINK` weg → GSM gibt frei | CYSPP wieder Eingang | K1, K2 (Bug 5.51) |
| A2 | BT verbunden → GSM holt → GSM gibt frei, `BT_LINK` noch gesetzt | CYSPP frei, Owner=BT, `connect\|UART1` | K2 |
| A3 | `uart1_request()` mit ungültigem Requester | CYSPP nicht getrieben zurückgelassen | K1 (5.52) |
| A4 | `fp.btmodem=Roving` bzw. `Laird` → `bt_cmdmode()` | **kein** I2C-Schreibzugriff (No-Op) | K1 (5.52) |
| A5 | `fp.btmodem=0` → `bt_cmdmode()` | treibt (Erkennung muss das können) | K1, K6 |
| A6 | I2C-Schreibfehler bei `bt_release()` | `bt_cyspp_out` bleibt 1, nächstes Sicherungsnetz greift | K1 |
| A7 | 50× GSM holen/freigeben mit `BT_LINK` gesetzt | begrenzte Pegelwechsel, keine Endlosschleife | K2 |
| A8 | `bt_disconnect()` | `connect` danach == vorher | K3 (Bug 5.49) |
| A9 | `BT_BAUD` je Modul | 460800 / 460800 / 307200 | K4 |
| A10 | Minutentick, CYSPP getrieben, kein `BT_LINK`, Owner≠BT | gibt frei; bei Owner==BT **nicht** | K1, K2 |

**Umsetzung — zwei Varianten:**

1. **Extraktion (empfohlen):** `uart1_request`/`uart1_release` aus hard.c in ein neues
   `uart1mux.c` (~60 Zeilen) verschieben; die Hardwareberührung dahinter als 4 Funktionen
   (`mux_select()`, `uart1_init()`, `cyspp_drive()`, `cyspp_float()`). Sauber testbar.
   *Kosten:* Eingriff in eine tragende Funktion → danach einmal am Gerät nachprüfen.
2. **Ohne Eingriff:** `hard.c` im Testbuild gegen ein Fake-`LPC17xx.h` kompilieren. Die
   MUX-Auswahl ist schon heute ein Makro (`MUX_BT_Select` → `FIO0CLR=…`), lässt sich also
   im Testbuild wegdefinieren. *Kosten:* Fake-Registerkarte für den halben LPC1766, brüchig
   und driftanfällig.

**Bewertung**

| | |
|---|---|
| Hätte 5.51 gefunden? | **Ja**, in Sekunden — und 5.49 und 5.52 ebenfalls |
| Aufwand Aufbau | 1–2 Tage (Variante 1), davon die Hälfte die Extraktion |
| Aufwand pro Lauf | Sekunden, `test\run_tests.bat`, keine Hardware, kein Keil |
| Deckt ab | K1–K5, K7 (Logik). **Nicht** K6, nicht Modul-/HW-Verhalten |
| Blinder Fleck | Testet das *Modell*, nicht das Modul. Ein Test kann grün sein, während die Leitung physisch klemmt |
| Haltbarkeit | hoch — Stubs altern langsam, Tests dokumentieren die Statemachine ausführbar |
| **Nutzen/Aufwand** | **klar das Beste.** Die einzige Option, die man *bei jeder Änderung* laufen lässt, weil sie nichts kostet |

---

## 4. Vorschlag B — On-Target-Selbsttest im RS232-Testmenü

**Idee:** Ein Menüpunkt „BT-Selbsttest", der die Szenarien aus A **auf dem echten Gerät mit
echtem I2C und echtem Modul** durchfährt und pro Schritt `PASS`/`FAIL` ausgibt. Entscheidend:
das **Config-Register des PCA9554 zurücklesen** (Register `0x03` ist lesbar) — das tut die
Firmware heute nirgends, sie schreibt nur blind.

```
BT-SELBSTTEST (Modul: IF820, fw 5.52)
 [1] IC54 Bit7 Grundzustand           PASS  (config=0xFF, alle Eingang)
 [2] bt_cmdmode -> Bit7 Ausgang HIGH  PASS  (config=0x7F, out=0x80)
 [3] bt_release -> Bit7 Eingang       PASS  (config=0xFF)
 [4] GSM-Verdraengung + Release       PASS  (config=0xFF nach Release)
 [5] Modul antwortet /PING            PASS  (@R,...,/PING,0000)
 [6] CYSPP-Pegel unverbunden          WARN  (floatet, erratisch - HW-Merker)
 ERGEBNIS: 5 PASS, 1 WARN, 0 FAIL
```

**Bewertung**

| | |
|---|---|
| Hätte 5.51 gefunden? | **Ja, wenn jemand ihn laufen lässt** — Schritt [4] trifft genau |
| Aufwand Aufbau | 0,5–1 Tag |
| Aufwand pro Lauf | Keil-Build + Flash + RS232 + Mensch liest mit: 10–15 Min |
| Deckt ab | K1, K2, K7 **plus** echtes I2C, echte Pegel, echtes Modulverhalten, HW-Fehler (Verdrahtung, Brownout, floatende Leitung) |
| Blinder Fleck | Läuft nicht automatisch; niemand macht das „nur mal schnell" |
| Kosten dauerhaft | Testcode liegt im Produktivimage (Flash/Menüplatz), muss mitgepflegt werden |
| **Nutzen/Aufwand** | **gut, aber als Ergänzung.** Deckt genau das ab, was A nicht kann — die Hardware. Als *einziger* Test zu langsam, um Gewohnheit zu werden |

**Nebeneffekt, der für sich schon lohnt:** Das Config-Register-Rücklesen kann als
Verifikation in `bt_release()` wandern — dann meldet die Firmware einen klemmenden
Expander im Feld als Diagnosecode, statt still BT-tot zu sein.

---

## 5. Vorschlag C — Automatisierte End-to-End-Strecke über BLE/SPP

**Idee:** Ein PC-Skript (Python, `bleak` für BLE bzw. zweites IF820-DVK für Classic) spielt
den Client: verbinden → CYSPP-Data-Char abonnieren → PIN senden → Menü navigieren →
Antworten gegen Sollwerte prüfen. Also genau der Ablauf aus `Doku/ble-chip-response.md`,
nur maschinell. Ezurio liefert dafür Python-Beispiele und Robot-Framework-Tests zum DVK
(nicht im Repo — `SupplierSourceCode/` enthält nur Firmware-Images und die Flasher-Tools).

**Bewertung**

| | |
|---|---|
| Hätte 5.51 gefunden? | **Ja, und als einzige Option das gemeldete Symptom selbst** (Disconnect beim Notify-Enable) |
| Aufwand Aufbau | 3–5 Tage: Rig (Gerät + DVK/BLE-Dongle + Test-PC), Skripte, Menü-Sollwerte pflegen |
| Aufwand pro Lauf | Minuten, aber nur am Rig-Arbeitsplatz |
| Deckt ab | **alles inklusive K6** und der GATT-/App-Sicht — die einzige Option, die den echten Nutzerpfad prüft |
| Blinder Fleck | keiner inhaltlich; dafür Flakiness (Funk, Timing, Advertising-Stop nach 90 s) |
| Kosten dauerhaft | **hoch.** Menütexte ändern sich → Sollwerte brechen. Ohne Pflege verrottet die Suite und wird ignoriert |
| **Nutzen/Aufwand** | **später, nicht jetzt.** Höchste Aussagekraft, aber der teuerste und zerbrechlichste Weg. Lohnt erst, wenn IF820 produktiv geht und Releases regelmäßig werden |

---

## 6. Empfehlung

**A jetzt, B direkt danach, C zurückstellen.**

Begründung: Alle vier real aufgetretenen Bugs (5.47b, 5.49, 5.51, 5.52) waren
**Zustandslogikfehler im geteilten Code** — kein einziger war ein Modul-, Funk- oder
GATT-Problem. A trifft diese Klasse vollständig und ist die einzige Option, deren Laufkosten
niedrig genug sind, um Gewohnheit zu werden. B kostet wenig zusätzlich und schließt den
Hardware-blinden Fleck von A. C prüft die schönste Strecke, aber gegen eine Fehlerklasse,
die uns bisher nie getroffen hat.

**Regel, unabhängig von der Wahl:** *Jeder gefundene Bug bekommt zuerst einen Test, der ihn
reproduziert, dann den Fix.* Rückwirkend wären das heute die Fälle A1 (5.51), A3/A4 (5.52)
und A8 (5.49) — drei Tests, die aus bereits bezahltem Lehrgeld entstehen.

**Sinnvoller erster Schritt, falls A gewählt wird:** nicht mit dem Testframework anfangen,
sondern mit der Extraktion von `uart1mux.c` plus **einem** einzigen Test (A1). Damit ist
bewiesen, dass die Strecke trägt, bevor Aufwand in Breite geht.

**Abdeckungsziel:** Für die extrahierte Statemachine (~60 Zeilen, hohe Hebelwirkung) nahe
100 % Branch-Coverage. Für modulspezifische Kommandopfade **kein** Coverage-Ziel — die sind
off-target nicht sinnvoll prüfbar und gehören nach B/C.

---

## 7. Ressourcenbudget: was darf On-Target-Testcode kosten?

Gemessen am Keil-Build von 5.52 (`Listings/sism.map`, `Objects/sism.build_log.htm`,
2026-07-30 09:13, **0 Errors / 0 Warnings**) — LPC1766 @ 100 MHz, Cortex-M3:

| Ressource | Belegt | Maximum | Frei | Auslastung |
|---|---|---|---|---|
| **Flash** (`LR_IROM1`) | 157 556 B | 262 144 B | **~102 kB** | 60 % |
| **RAM lokal** (`RW_IRAM1` @0x10000000) | 13 384 B | 32 768 B | **~19 kB** | 41 % |
| **RAM AHB** (`non_init` @0x2007C000) | 17 244 B | 32 736 B | **~15 kB** | 53 % |
| Code+RO / RW / ZI | 135 300 / 22 348 / 420 / 29 384 B | | | |

**Referenzgröße zum Kalibrieren:** Die komplette CYSPP-Logik aus 5.51+5.52 kostet laut Map
`bt_cmdmode` = **48 B**, `bt_release` = **26 B**, `bt_cyspp_out` = **1 B RAM**. Ein
Selbsttestschritt mit Registerrücklesen und Textausgabe liegt in derselben Größenordnung,
grob **50–250 B**. Bei 102 kB freiem Flash sind das **einige Hundert Schritte** — Flash ist
also **nicht** die Grenze, auch nicht annähernd.

**Laufzeit.** `write_i2c_dev()`/`read_i2cdev()` sind blockierend, aber **hart begrenzt**
(Timer-1-Timeout 200 µs pro Byte, danach `ARM_I2C_ABORT_TRANSFER`). Ein `bt_release()` kostet
damit ≤ **0,4 ms** — auch im Fehlerfall, kein Hängerisiko. Die Selbstheilung aus 5.51 läuft
1× pro Minute: **≈ 7 ppm CPU-Last.** Praktisch nicht messbar.

**Die echten Grenzen sind nicht Flash und nicht Takt:**

| Grenze | Wert | Konsequenz |
|---|---|---|
| **Watchdog** | 32 s (`LONG_WD_32`) | Ein blockierender Testablauf darf ohne `ResetWDT()` nie länger laufen. Bei Schritten mit `osDelay(600)` o. ä. gilt: `ResetWDT()` in jeden Schritt |
| **Geteilter I2C-Bus** | IC37, IC54, IC58, IC35/DPP, LED-Treiber | Zyklische Selbsttests dürfen den Bus nicht belasten, während andere Teilnehmer ihn brauchen. On-Demand-Tests sind hier unkritisch |
| **Geteilte UART1/MUX** | BT / GSM / GPS | Ein Test, der `uart1_request()` provoziert, muss den Owner am Ende sauber restaurieren — sonst testet man sich selbst kaputt |
| **Thread-Stacks** | 256 / 256 / 512 Wörter (USB / GSM / MQTT) | Testcode gehört in den Main-Thread, nicht in die Threads. Tiefe Aufrufketten mit lokalen Puffern dort vermeiden |
| **Wartungsaufwand** | — | Der wirklich knappe Posten. Nicht Bytes |

**Abgeleitete Regeln:**

1. **On-Demand-Tests (RS232-Menü): großzügig erlaubt.** Sie kosten ausschließlich Flash,
   zur Laufzeit exakt null. Der komplette Selbsttest aus Vorschlag B (≈ 10–15 Schritte)
   landet bei **1–3 kB**, also **1–3 % des freien Flash**. Unbedenklich.
2. **Zyklische Selbstprüfungen: streng budgetieren.** Richtwert **≤ 1 ms Arbeit pro Sekunde**
   (0,1 % CPU) und **niemals in einer ISR**. Die Minuten-Selbstheilung liegt vier
   Größenordnungen darunter — es ist also reichlich Luft für weitere Prüfungen dieser Art,
   solange sie im Minutentakt und nicht im Messzyklus hängen.
3. **Nie zyklisch prüfen, was ein anderer Owner gerade benutzt.** Immer erst Owner/Link-Status
   abfragen (Muster: `!(connect&BT_LINK) && uart1_owner!=MUX_BT`), dann handeln.
4. **`#if BT_SELFTEST` um den Testblock**, damit er sich für ein Produktivimage notfalls
   ausschließen lässt — bei 102 kB Reserve aber kein akuter Grund dazu.
5. **PC-Tests (Vorschlag A) kosten auf dem Controller exakt nichts.** Das ist das stärkste
   Argument, dort anzufangen: unbegrenzt viele Testfälle bei null Ressourcenverbrauch im Gerät.

---

*Angelegt 2026-07-30 nach Version 5.52. Noch keine Entscheidung getroffen.*
