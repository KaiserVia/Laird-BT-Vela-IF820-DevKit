# Konzept: CYSPP-Zustandsmaschine in der Firmware (Stand 2026-11-11)

> Phasen 0-3 sind umgesetzt (5.129-5.134). Die Abschnitte vor 6a sind der ursprüngliche Entwurf und
> beschreiben teils den verworfenen Ablauf mit `.CYSPPSP E=1`/`.CYSPPSTART` sowie die Zustände
> `GESPERRT`/`STOERUNG`, die es im Code nicht gibt (`btio.h` kennt `BT_ST_AUS`, `BEREIT`, `VERBUNDEN`,
> `DATEN`). Maßgeblich für den Ist-Stand sind Abschnitt 6a und die Phasentabelle.

Bisher steuert das IF820 den BT-Ablauf selbst (CYSPP "Enable + Autostart"), die Firmware reagiert nur auf den
CYSPP-Pin. Dieses Konzept beschreibt, was es heißt, den Ablauf in die Firmware zu holen: Die Firmware entscheidet,
wann das Gerät sichtbar und verbindbar ist, liest die Ereignisse des Moduls und öffnet den Datenkanal selbst.
Bisher als "Option B" im BLE-Durchsatz-Backlog (`project-status.md`) und als Transport-Erkennung in `CLAUDE.md`
zurückgestellt.

## 1. Was es bringt

| Nutzen | Heute | Mit Zustandsmaschine |
|---|---|---|
| Sichtbar erst, wenn die Firmware bereit ist | Modul wirbt ab Einschalten, Firmware braucht 15 bis 30 s; frühe Verbindung bekommt keinen PIN-Prompt | Werben und Classic-Verbindbarkeit erst am Ende des Hochlaufs |
| Unsichtbar, solange die Firmware nicht kann | während LTE-Sitzung, Firmware-Update, Flash-Schreiben verbindbar, aber taub | Werben dabei aus, danach wieder an |
| Classic oder BLE unterscheiden | nicht möglich (ein Pin für beides) | aus den Ereignissen (`C` = BLE, `BTCON` = Classic) |
| BLE-Verbindungsintervall | PC/Handy bestimmt (B: 60 ms, 3,1 KB/s) | `/UCP` vor dem Öffnen des Datenkanals (5.125 scheiterte, weil der Kanal schon offen war) |
| Trenngrund, Abbruch von Ymodem | Ymodem sendet nach Linkverlust minutenlang weiter (`F13010`) | `DIS`/`BTDIS` mit Grund, sofortiger Abbruch |
| Modulzustand | unbekannt (Ausfälle B/C wurden erst über den Sniffer sichtbar) | Firmware kennt den Zustand, kann einen hängenden Zustand erkennen und neu starten |

## 2. Was das Modul dafür anbietet (EZ-Serial-Guide)

| Mittel | Wirkung | Quelle |
|---|---|---|
| `.CYSPPSP E=1` (Enabled statt 2 = Autostart) | CYSPP ist "idle until start": kein Werben beim Booten, nach dem Trennen kein Neustart. Ereignisse bleiben im Command-Mode sichtbar, bis der Datenkanal offen ist | Guide 2.4.7.8 (Tabelle "CYSPP configuration and pin relationship"), 2.4.7.9 |
| `.CYSPPSTART` | startet CYSPP: Modul wirbt; abonniert der Client den Datenkanal, geht das Modul in den transparenten Modus und zieht den CYSPP-Pin selbst LOW (Pin bleibt beim Host floatend, wie heute) | 2.4.7.1, 7.2.8.1 |
| `SAP ... F=00` | kein automatisches Werben bei Boot/Trennen (Bit 0 gilt für beides) | 7.2.3.22; Hinweis dort: bei CYSPP-Autostart wirbt das Modul trotzdem, deshalb nur zusammen mit `E=1` |
| `/A`, `/AX` | Werben starten/stoppen | 7.2.3.7, 7.2.3.8 |
| `SBTP C=0/1`, `D=0/2` | Classic verbindbar/sichtbar ein/aus (ohne `/SCFG` nur im RAM) | 7.2.9.10 |
| Ereignisse `BOOT`, `C`, `CU`, `DIS`, `BTCON`, `BTDIS`, `.CYSPP,S=` | Modul gestartet, BLE verbunden/Parameter/getrennt, Classic verbunden/getrennt, CYSPP-Status (Bit 0/1 = Datenkanal abonniert) | 7.3.x |
| `/UCP` | BLE-Verbindungsparameter anfordern | 7.2.3.x, 5.125 |

**Unverändert bleibt die Pin-Regel** (Sackgasse aus `CLAUDE.md`): Der CYSPP-Pin darf vom Host nie getrieben werden.
Bei "Enabled" mit extern HIGH gehaltenem Pin wird der Kanal nie transparent. Die Zustandsmaschine nutzt den Pin
weiter nur lesend (IC54 Bit 7) und treibt ihn nur beim bewussten Trennen (`bt_disconnect()`).

## 2a. Updates und Rückweg: fail-safe statt dauerhaft umstellen (bevorzugt, 2026-11-02)

Die Modul-Konfiguration im Flash bleibt wie heute (`.CYSPPSP E=2` Autostart, `SAP F=01`). Die neue Firmware schaltet
bei **jedem** Start nur im RAM des Moduls um (`.CYSPPSP E=1`, `/AX`, `SBTP C=0`, ohne `/SCFG`) und startet das Werben
mit `.CYSPPSTART`, sobald sie bereit ist. Startet das Modul allein neu (Brown-out, `BT_TIMEOUT`-Abschaltung), gilt
wieder der Flash-Stand (Autostart); die Firmware erkennt das am `BOOT`-Ereignis und schaltet erneut um.

| Fall | Ergebnis |
|---|---|
| Update auf die neue Firmware (RS232, USB, Bluetooth) | läuft; Umschaltung beim ersten Start |
| Update von einem alten Stand im Feld (z. B. 5.99) | läuft: das Modul hat dort Autostart im Flash, genau davon geht die neue Firmware aus; kein Umstellungsschritt. Das Update selbst führt noch die Update-Routine der alten Firmware aus (unverändert) |
| Update über Bluetooth | Verbindung überlebt den LPC-Neustart (getestet 2026-11-02, `bt-update`), danach `.CYSPPSTART` |
| Rückweg auf ältere Firmware (z. B. 5.128) | Modul hat Autostart im Flash, Verhalten wie heute, kein Werksreset nötig |
| Firmware hängt oder verpasst ein Ereignis | Modul wirbt wie heute: Fehler gehen in die harmlose Richtung (sichtbar, nie unsichtbar) |

Preis: In den ersten ein bis zwei Sekunden nach dem Einschalten wirbt das Modul noch, bis die Firmware umschaltet.
Dafür muss die Umschaltung ganz an den Anfang des Hochlaufs (heute erreicht die Firmware das Modul erst nach ca. 13 s;
Ursache noch zu messen). Ob `.CYSPPSP E=1` im RAM sofort wirkt und laufendes Werben mit `/AX` endet, klärt Phase 0.
Damit entfällt das Risiko "Rückweg auf ältere LPC-Firmware" aus Abschnitt 5, ebenso der Flash-Teil von Phase 4.

Pflichttests vor jeder Freigabe (in `bt_testlauf.py` aufnehmen): `bt-update` (vorhanden) und **Hin- und Rückweg mit
echten Altständen** (alte Version, z. B. `fw_5.99`, flashen -> BT-Smoke -> neue Version über Bluetooth aufspielen ->
BT-Smoke -> alte Version wieder flashen -> BT-Smoke). Welche Stände im Feld sind, gibt der Anwender vor.

## 3. Zustände

| Zustand | Bedeutung | Modul | Firmware-Ausgang |
|---|---|---|---|
| `AUS` | BT nicht installiert, Modul aus (`BT_TIMEOUT`-Abschaltung) oder Hochlauf noch nicht fertig | still, nicht verbindbar | - |
| `BEREIT` | Firmware bereit, wartet auf Kunden | wirbt (BLE), Classic verbindbar | nach `.CYSPPSTART` + `SBTP C=1` |
| `VERBUNDEN` | BLE (`C`) oder Classic (`BTCON`) verbunden, Datenkanal noch zu | Command-Mode, Ereignisse lesbar | BLE: `/UCP` senden, auf Abo warten |
| `DATEN` | Datenkanal offen (CYSPP-Pin LOW, `.CYSPP,S` Bit 0/1) | transparent | PIN-Prompt, Menü (wie heute `BT_LINK`) |
| `GESPERRT` | Firmware kann gerade nicht bedienen (LTE-Sitzung, Update, langes Flash-Schreiben) | Werben aus (`/AX`, `SBTP C=0`) | danach zurück nach `BEREIT` |
| `STOERUNG` | Modul antwortet nicht oder Zustand unklar | ? | Neustart des Moduls (`/RBT`, im Notfall Stromabschaltung wie bei `BT_TIMEOUT`) |

| Übergang | Auslöser | Aktion |
|---|---|---|
| `AUS` -> `BEREIT` | Ende des Hochlaufs (main.c), Modul-`BOOT`-Ereignis, Ende von `GESPERRT` | `.CYSPPSTART`, `SBTP C=1,D=2` (RAM) |
| `BEREIT` -> `VERBUNDEN` | Ereignis `C` (BLE) oder `BTCON` (Classic) | Transport merken; BLE: `/UCP` falls Intervall zu lang |
| `VERBUNDEN` -> `DATEN` | CYSPP-Pin LOW (EINT1/IC54 wie heute) bzw. `.CYSPP,S` mit Bit 0/1 | heutiger Connect-Zweig in `communication_change()` |
| `VERBUNDEN`/`DATEN` -> `BEREIT` | `DIS`/`BTDIS`, Pin HIGH | Grund protokollieren, Ymodem abbrechen, **`.CYSPPSTART` erneut** (sonst bleibt das Gerät unsichtbar) |
| `BEREIT` -> `GESPERRT` | `uart1_request(MUX_GSM)`, Firmware-Update, `write_flash()` | `/AX`, `SBTP C=0` vor dem MUX-Wechsel |
| beliebig -> `STOERUNG` | kein `@R` auf Befehle, `BEREIT` ohne Werbe-Bestätigung, Minutenprüfung erfolglos | Modul neu starten, Fehlercode |

**Sicherheitsnetz (wichtig, weil ein verpasstes Ereignis das Gerät unsichtbar machen würde):** Im Minutentakt im
Zustand `BEREIT` den Modulzustand abfragen (z. B. `/PING` plus CYSPP-Status) und bei Zweifel `.CYSPPSTART` erneut
senden. Das ist die Gegenstelle zur heutigen `bt_release()`-Selbstheilung im Minutentakt.

## 4. Was sich in der Firmware ändert

| Stelle | Änderung |
|---|---|
| `btio.c` neu | Ereignis-Parser auf `rbuf1` (Basis: `bt_event_line()` aus 5.119, heute nur Anzeige), Zustandsvariable, Übergangsfunktion, Sicherheitsnetz |
| `init_bluetooth()` | Ensure-Schritte: `.CYSPPSP E=1` (alle übrigen Felder aus `.CYSPPGP` übernehmen, siehe Warnung in Option A), `SAP ... F=00`; Classic im Flash nicht verbindbar (`SBTP C=0`), zur Laufzeit ein |
| `main.c` | Ende des Hochlaufs: `AUS` -> `BEREIT` (ersetzt den 5.128-Aufruf nicht, sondern ergänzt ihn) |
| `measure.c` | Hauptschleife: Parser/Zustandsmaschine pollen (wie `bt_sniff_poll()`); Minutentakt: Sicherheitsnetz; `BT_TIMEOUT`: nach Wiedereinschalten des Moduls auf `BOOT` warten |
| `hard.c` `uart1_request()`/`uart1_release()` | vor GSM `GESPERRT`, danach zurück |
| `libtool.c` `communication_change()` | Connect/Trennen wie heute über den Pin, zusätzlich Zustand und Transport setzen |
| `sicom.c`, `xmodem.c` | Ymodem bei `DIS` abbrechen; Firmware-Update setzt `GESPERRT` |
| `btio.h` | Schalter `BT_CYSPP_SM 0/1` (wie `BT_UCP_ENABLE`), damit der alte Ablauf per Neuübersetzung zurückkommt |

Umfang grob: 400 bis 600 Zeilen neu oder geändert, Schwerpunkt `btio.c`.

## 5. Risiken und Gegenmaßnahmen

| Risiko | Folge | Gegenmaßnahme |
|---|---|---|
| Ereignis verpasst (rbuf1 voll, Hauptschleife blockiert) | Gerät bleibt unsichtbar | Sicherheitsnetz im Minutentakt; `rbuf1` bei Bedarf vergrößern |
| Modul startet allein neu (Brown-out, `BT_TIMEOUT`-Abschaltung) | mit `E=1` still, bis die Firmware startet | `BOOT`-Ereignis auswerten -> `.CYSPPSTART` |
| **Rückweg auf ältere LPC-Firmware** | alte Firmware kennt `.CYSPPSTART` nicht -> Modul wirbt nie, BT tot | vor dem Flashen alter Firmware Modul-Werksreset (Testmenü 8 -> 4, `/RFAC`) oder alte Firmware setzt `E=2` zurück; in der Service-Anleitung festhalten |
| Classic hängt anders an CYSPP als vermutet | Classic nicht mehr verbindbar oder Daten nicht transparent | vor der Umsetzung klären (Phase 0) |
| Verhalten der Modul-Firmware 1.4.18.18 (neue Module) | andere Standardwerte (`SPPM`), "CYSPP HIGH beendet SPP" | Phase 0 auch mit 18.18 wiederholen, sobald ein Modul da ist |
| Regression in BLE und Classic, beides gleichzeitig | BT im Feld tot | Schalter `BT_CYSPP_SM`; volle Testreihe (`bt_testlauf.py`, `module-suite`) auf drei Boards vor Freigabe |

## 6. Vorher am Gerät zu klären (Phase 0, ohne Firmware-Umbau)

1. Mit `E=1` und `F=00` (nur RAM, kein `/SCFG`, Neustart stellt alles zurück): Wirbt das Modul wirklich nicht? Ist Classic noch verbindbar?
2. Nach `.CYSPPSTART`: Werben, Verbinden, Abo -> wird der Kanal transparent und geht der CYSPP-Pin LOW (IC54 Bit 7)? Kommen `C`, `.CYSPP,S=..` vorher im Command-Mode?
3. Nach dem Trennen: still bis zum nächsten `.CYSPPSTART`? Welche Ereignisse (`DIS` mit Grund)?
4. Classic mit `SBTP C=0/1` zur Laufzeit: verbindbar ein/aus? Wird ein Classic-Kanal mit `E=1` transparent, und meldet er `BTCON` vorher?
5. `/UCP` zwischen `C` und Abo: wird das Intervall übernommen (`CU`), bevor der Kanal öffnet?
6. `/AX` und `SBTP C=0` während `BEREIT`: Gerät sofort unsichtbar? Was passiert mit einer laufenden Verbindung?

Werkzeug dafür: ein Testmenüpunkt "Befehl an das Modul senden" (Eingabe über RS232, Antwort und Ereignisse
anzeigen) plus ein Python-Skript, das die Schritte über RS232 und BLE/Classic vom PC aus automatisch abarbeitet.
Alternativ das IF820-DVK direkt am PC (ohne Firmware-Änderung).

### 6a. Ergebnisse Phase 0 (2026-11-02, Board B 26SC9983, Firmware 5.129, `Doku/tools/cyspp_phase0.py`)

Details und Berichtsordner: `project-status.md`, "Phase-0-Läufe am Gerät".

| Frage | Antwort |
|---|---|
| 1 | Mit `E=1` (+`F=00`) wirbt das Modul **weiter**; erst `/AX` beendet das Werben sofort. Classic bleibt verbindbar und transparent. |
| 2 | **`.CYSPPSTART` startet mit `E=1` einen Central-Scan statt Werben** (`SSC,S=02`, 60-70 Scan-Events/s auf dem UART), obwohl CP_ROLE offen und `G=00`. Werben startet `/A`, aber nur **ohne** `SAP F=00` (sonst `0107`). Danach: `C`, Discovery, `W`, `CU` im Command-Mode, Pin HIGH; nach dem Abo `.CYSPP`, Pin LOW, transparent. |
| 3 | `DIS,R=0913`; mit `F=01` wirbt das Modul danach **von selbst weiter** (nicht still). |
| 4 | `SBTP C=0/1` im RAM schaltet Classic sofort aus/ein; `BTCON,...,T=01` vor den Daten, `BTDIS,R=0000`. |
| 5 | `/UCP` zwischen `C` und Abo wirkt: `CU,I=000C` vor dem Abo, danach transparent. |
| 6 | `/AX` + `SBTP C=0` macht sofort unsichtbar; eine laufende BLE-Verbindung (ohne Abo) bleibt und wird nach dem Abo transparent. |

**Folgen für das Konzept:**
- Phase 1 (Beobachtung) ist machbar und als 5.130 umgesetzt.
- Für Phase 2 entfällt `.CYSPPSTART` (und `SAP F=00`). Tragfähiger Ablauf: beim Start `/AX` + `SBTP C=0` (RAM),
  wenn bereit `/A` + `SBTP C=1`; `F=01` bleibt, das Modul wirbt nach jedem Trennen selbst weiter. Das entschärft das
  Hauptrisiko "nach dem Trennen unsichtbar" (Abschnitt 5) und macht das Sicherheitsnetz einfacher. Offen: ob `E=1`
  dafür überhaupt nötig ist oder `E=2` (Flash wie heute) mit `/AX`/`/A` genügt; ob Werben nach einem Modul-Neustart
  (`BOOT`, Flash = Autostart) sofort wieder per `/AX` gestoppt werden muss.
- **Nachtrag Frage 7 (12:04): `E=2` genügt.** `/AX` + `SBTP C=0` halten das Gerät dauerhaft unsichtbar, `/A` + `SBTP
  C=1` machen es sichtbar und transparent verbindbar. Ein Modul-Neustart zur Laufzeit braucht kein `/AX`: Die Firmware
  ist dann bereit, Werben ist gewollt. Umgesetzt als 5.131 (Phase 2).
- `/UCP` vor dem Abo (Phase 3) funktioniert im Zustand VERBUNDEN.

## 7. Vorgehen in Phasen

| Phase | Inhalt | Ergebnis | Risiko fürs Feld |
|---|---|---|---|
| 0 | Fragen aus Abschnitt 6 am Gerät klären (Testmenüpunkt + Skript) | **erledigt (5.129), Ergebnisse 6a** | keins |
| 1 | Ereignis-Parser und Zustandsanzeige, **ohne Verhaltensänderung** (E bleibt 2) | **umgesetzt (5.130)**: Zustand, Transport, Trenngrund; Ymodem-Abbruch bei Linkverlust (Pin, da `DIS` im Datenmodus nicht ankommt) | gering |
| 2 | `/AX` + `SBTP C=0` früh im Hochlauf, `/A` + `SBTP C=1` am Ende, alles im RAM bei `E=2` (siehe 6a), Minutentakt als Sicherheitsnetz, Schalter `BT_CYSPP_SM` | **umgesetzt (5.131)**: unsichtbar ab ca. 3,7 s bis zum Ende des Hochlaufs | mittel (fail-safe: Modul-Neustart = Werben wie bisher) |
| 3 | `GESPERRT` (LTE, Update, Flash), `/UCP` vor dem Abo | **umgesetzt (5.132)** für Update über RS232/USB (`bt_sm_lock()`/`bt_sm_unlock()`); die Sperre während der GSM-Sitzung wurde in 5.134 wieder entfernt (BT hat Vorrang und beendet MQTT); `/UCP` wieder an. Offen: lange Flash-Schreibvorgänge | mittel |
| 4 | Konfigurations-Umstellung im Feld, Rückweg, Service-Anleitung | ausrollbar | - |

Phase 1 lohnt sich auch allein (Trenngrund, Ymodem-Abbruch) und ist die Grundlage für alles Weitere.
