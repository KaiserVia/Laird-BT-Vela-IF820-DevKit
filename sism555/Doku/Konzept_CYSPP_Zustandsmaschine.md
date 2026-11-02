# Konzept: CYSPP-Zustandsmaschine in der Firmware (Stand 2026-11-02, Entwurf)

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

## 7. Vorgehen in Phasen

| Phase | Inhalt | Ergebnis | Risiko fürs Feld |
|---|---|---|---|
| 0 | Fragen aus Abschnitt 6 am Gerät klären (Testmenüpunkt + Skript) | Machbarkeit belegt oder verworfen | keins |
| 1 | Ereignis-Parser und Zustandsanzeige, **ohne Verhaltensänderung** (E bleibt 2) | Firmware kennt Transport, Trenngrund; Ymodem-Abbruch bei `DIS` | gering |
| 2 | Zustandsmaschine mit `E=1`, `F=00`, `.CYSPPSTART`, Sicherheitsnetz, hinter `BT_CYSPP_SM` | Sichtbar erst wenn bereit, nach jedem Trennen neu gestartet | hoch, deshalb Schalter und volle Testreihe |
| 3 | `GESPERRT` (LTE, Update, Flash), `/UCP` vor dem Abo | unsichtbar während Sperrzeiten, kürzeres BLE-Intervall | mittel |
| 4 | Konfigurations-Umstellung im Feld, Rückweg, Service-Anleitung | ausrollbar | - |

Phase 1 lohnt sich auch allein (Trenngrund, Ymodem-Abbruch) und ist die Grundlage für alles Weitere.
