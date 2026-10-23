# BLE-Ursachensuche: Belege aus allen Testreports und Plan (Stand 2026-10-23)

## Die Fehlerbilder

| Nr. | Fehlerbild | Wo gesehen |
|---|---|---|
| F1 | BLE "verbunden", Subscribe ok, kein PIN-Prompt, Windows meldet nach einigen Sekunden Trennung. | A und B, intermittierend |
| F2 | Link reißt mitten im BLE-Download, danach Gerät ca. 30 s nicht auffindbar, Firmware sendet Ymodem weiter ins Leere (`F13010`). | A: 19.10. (5.118) und 22.10. (5.126) |
| F3 | Board B kommt per BLE gar nicht mehr durch. | B, seit 21.10. nachmittags |
| F4 | Board B bekommt vom PC 60 ms Intervall (3,1 KB/s), A meist 7,5 ms. | B |

## Datenbasis

Alle 104 Testreports (2026-09-01 bis 2026-10-22, 5,7 GB Mitschnitte) maschinell ausgewertet:
`Doku/tools/trace_events_extract.py` (filtert die Traces) und `Doku/tools/trace_ble_attempts.py` (ein Datensatz je BLE-Verbindungsversuch).
Ergebnis: **387 BLE-Verbindungsversuche** mit Board, Firmware, Modus, Zeiten, Ergebnis und den Modul-Events `C`/`CU`/`DIS`
(ab 5.119 auf RS232). Zusätzlich das Windows-Systemprotokoll seit 01.09. (Bluetooth-Adapter, Standby).

**Wichtige Einschränkung:** Bei den Läufen mit Board B lief der RS232-Mitschnitt teils auf COM3 (Board A). Die `C`-Events
sind deshalb nur für Board A verlässlich zugeordnet; die Aussagen unten zu `C`-Events gelten für A.

## Befunde

### B1. F1 betrifft fast nur den ersten Verbindungsversuch eines Testprozesses (nach dem 15-s-Scan)

| Board | Art des Versuchs | ok | Fehler | Fehlerrate |
|---|---|---|---|---|
| A | erster Versuch im Prozess, nach Scan | 30 | 16 | **35 %** |
| A | Folgeversuche, direkt auf bekannte Adresse (cycle, Download) | 187 | 3 | **2 %** |
| B | erster Versuch, nach Scan | 14 | 14 | 50 % |
| B | Folgeversuche auf Adresse | 75 | 19 | 20 % (fast alle am 01.10. bzw. mit 5.125) |

Gleiches Board, gleiche Firmware, gleiche Funkstrecke, oft im selben Lauf: Der erste Versuch scheitert oft, die nächsten
fast nie. Die 3 Fehler bei A-Folgeversuchen stammen vom 14.10. (5.116, vor dem CTS-Fix 5.117).
**Folgerung:** Die Ursache hängt am Ablauf auf der PC-Seite (neuer Python-Prozess, Scan, erster Verbindungsaufbau über
Windows/bleak), nicht an Board, Modul oder Firmware-Version. Das bestätigt die Beobachtung vom 20.10. (smoke 40-50 %,
cycle 0/6) mit deutlich mehr Daten.

### B2. Bei F1 hat das IF820 der Firmware keine Verbindung gemeldet, obwohl Windows "verbunden + abonniert" meldet

Board A, smoke-Läufe ab 5.119 (Firmware gibt das Modul-Event `C` = gap_connected auf RS232 aus):

- **10 erfolgreiche Versuche:** das `C`-Event kommt immer, 0,4 bis 5,4 s nach Geräteauswahl und **vor** dem PIN-Prompt.
- **7 gescheiterte Versuche:** in 6 Fällen **kein** `C`-Event, solange das Testskript wartet. Im siebten (20.10. 14:14) kommt
  das `C`-Event **16,2 s** nach der Geräteauswahl, also **6 s nachdem** das Skript schon aufgegeben hat.
- Windows meldet in allen 7 Fällen trotzdem "verbunden" (1,1 bis 4,7 s) und "Subscribe ok" (ca. 30 ms).

**Folgerung:** Zu dem Zeitpunkt, an dem Windows/bleak "verbunden und abonniert" meldet, besteht aus Sicht des Moduls noch keine
Verbindung. Die Firmware kann den PIN-Prompt erst nach dem Verbinden senden. **Firmware und Python-Logik nach dem Verbinden
sind damit als Ursache von F1 ausgeschlossen.** Offen bleibt nur, ob der PC die Verbindung zu spät bzw. gar nicht aufbaut
(Windows-Stack, Adapter) oder ob das Modul den Verbindungsaufbau verzögert. Das trennt nur ein BLE-Sniffer (Schritt 5).

### B3. F1 hängt nicht an der Firmware-Version

Erste smoke-Versuche auf A mit Fehlschlag: 5.118 1 von 4 (21.10. 16:34), 5.124 7 von 13 (20.10.), 5.126 1 von 3 (21.10.). Die Versionen
unterscheiden sich in der Rate nicht belastbar, und alle zeigen dasselbe Muster wie in B1.
**Hinweis:** `smoke.json` meldet bei einer erfolgreichen Wiederholung `passed`; der gescheiterte Erstversuch steht dann nur im
Trace. Die Aussage "7/7 bestanden" vom 21.10. war deshalb eigentlich 7/9 (16:34 und 16:42 brauchten eine Wiederholung).

### B4. F2 (Download-Abbruch) gab es auch schon mit 5.118

| Datum | Firmware | BLE-Downloads (4,2 MB) | Bemerkung |
|---|---|---|---|
| 15.10. (3 Läufe) | 5.118 | 9/9 | |
| 19.10. 08:23 | 5.118 | 3/3 | |
| 19.10. 10:04 | 5.118 | **1/3**: 2/3 riss bei ca. 96 % (11:33), 3/3 "nicht gefunden", danach `F13010` | PC war 11:19-11:30 im modernen Standby, Download lief darüber hinweg weiter |
| 19.10. 12:44 | 5.119 | 3/3 | |
| 22.10. 07:51 | 5.126 | **3/3** | in `project-status.md` bisher nicht erwähnt |
| 22.10. 09:27 | 5.126 | **0/3**: 1/3 riss nach 5 min, 2/3 und 3/3 "nicht gefunden", danach `F13010` | keine PC-Ereignisse |

Echte Linkverluste: 5.118 einer von 16, 5.126 einer von 4. Das Bild danach (nicht auffindbar, Firmware sendet weiter,
`F13010`) ist bei beiden Versionen **identisch**. Damit ist F2 **kein Rückschritt durch 5.119 bis 5.126**.
**Folge für den Plan:** Die geplante Gegenprobe (eine module-suite mit 5.118, 3 Downloads) kann bei einer Abbruchrate von
grob 5 bis 25 % pro Download **nichts entscheiden**. Dafür bräuchte es Dutzende Downloads je Version. Sie ist zurückgestellt.

### B5. Der PC-Adapter ist zeitweise gestört, erklärt F1 aber nicht

Windows-Systemprotokoll, `BTHUSB` Ereignis 3 ("Adapter hat nicht geantwortet"), insgesamt 975-mal: am 02.09., 21. bis 23.09.,
14.10. und 22.10. (14 bis 16 Uhr, 67-mal). Die beiden Läufe am 22.10. um 14:40 und 14:57 ("Gerät nicht gefunden") fallen in
diese Zeit und sind damit PC-seitig erklärt. An den F1-Tagen 20. und 21.10. gab es dagegen **keine** Adapterfehler, beim
Abbruch am 22.10. um 10:39 ebenfalls nicht.

### B6. Neuer Firmware-Befund: Nach "Flash schreiben" bzw. Download meldet die Firmware keine Modul-Events mehr, bis zum Neustart

In jedem Lauf mit "Flash schreiben" (Board A, ab 5.119) erscheinen die `C`-Events bis einschließlich dieser Phase; danach
fehlen sie bei **allen** weiteren Verbindungen, auch in den Folgeläufen ohne Neustart:
19.10. 12:41 bis 12:44, 20.10. 09:07 (Soak), 22.10. 07:51 bis 09:27 (Lauf 09:27: 30 erfolgreiche Verbindungen, **kein einziges**
`C`-Event). Die Verbindungen selbst funktionieren in diesem Zustand (cycle 12/12, PIN ok).
**Bedeutung:** (a) Fehlt das `C`-Event, beweist das nur dann etwas, wenn derselbe Zustand kurz davor und danach Events
zeigte (bei B2 erfüllt: Erfolge und Fehlschläge wechseln sich minutenweise ab). (b) Irgendetwas in der Firmware ändert sich
dauerhaft: UART1-Empfang im Command-Mode, Besitzer von UART1 (`uart1_owner`, Routing in `UART1_IRQHandler`, `hard.c`)
oder die `rbuf1`-Verarbeitung. Das ist dieselbe Stelle, an der schon der 5.92-Fehler saß. Ob es mit F2 zusammenhängt, ist
offen (der Lauf 07:51 lief in diesem Zustand 3/3 durch).

## Zwischenstand je Verdächtigem

| Verdächtiger | F1 (kein PIN-Prompt) | F2 (Download-Abbruch) |
|---|---|---|
| Firmware | **entlastet** (B2, B3) | Rückschritt ausgeschlossen (B4); Firmware-Anomalie B6 offen |
| Python-Testprogramm | nach dem Verbinden entlastet (B2); der Ablauf "neuer Prozess + Scan + erster Connect" ist der Auslöser (B1) | kein Hinweis |
| Windows-PC (Stack/Adapter) | **Hauptverdacht** (B1, B2) | möglich (19.10. Standby, Adapter zeitweise gestört) |
| IF820-Modul | möglich: verzögerter Verbindungsaufbau, nur per Sniffer trennbar | möglich: Trenngrund unbekannt |
| Board-Hardware | kein Hinweis (beide Boards gleich, Folgeversuche 2 %) | kein Hinweis |

## Plan (angepasst an die Befunde)

| Schritt | Was | Trennt |
|---|---|---|
| 1 | **Handy statt PC:** nRF Connect, 20x verbinden, Notify auf `...ca102` einschalten, PIN-Prompt abwarten. Jeweils App neu starten (entspricht "neuer Prozess"). | PC gegen Modul (F1) |
| 2 | **smoke mit `--address` statt Scan** (20x) gegen smoke mit Scan (20x). Fällt die Rate auf ca. 2 %, ist der Scan vor dem Connect der Auslöser. | Scan gegen ersten Connect |
| 3 | **Minimal-Skript** (bleak, ca. 30 Zeilen): Scan, Connect, Notify, 20 s auf `C`-Event (RS232) und PIN-Prompt warten. Misst, wann das Modul wirklich verbunden ist. | Testprogramm gegen Windows |
| 4 | **BLE-Sniffer** (nRF52840-Dongle + Wireshark): wann kommt `CONNECT_IND`, wer trennt, mit welchem Grund. Gleichzeitig einen F2-Abbruch mitschneiden. | PC gegen Modul, endgültig |
| 5 | **Firmware B6 klären:** nach "Flash schreiben" über RS232 BLE verbinden und prüfen, ob `[BT-Event]` fehlt; dann `uart1_owner`/`rbuf1_full` im Testmenü anzeigen lassen. | Firmware-Zustand |
| 6 | **5.127:** Ymodem bei Linkverlust sofort abbrechen; zusätzlich den Trenngrund (`DIS`, `R=`) auch bei aktiver Brücke erfassen, damit F2 einen Grund im Bericht hat. | F2-Diagnose |
| 7 | Testskript: `smoke.json` soll gescheiterte Erstversuche ausweisen; Mitschnitt-Port je Board prüfen (B lief teils mit COM3 = A). | saubere Statistik |

**Empfohlene Reihenfolge:** 2 und 1 zuerst (je ca. 30 min, kein Material). Den Sniffer (4) bestellen. Die Gegenprobe
5.118/5.126 entfällt in der bisherigen Form (B4).
