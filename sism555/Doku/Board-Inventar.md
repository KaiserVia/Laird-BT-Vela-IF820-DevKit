# Board-Inventar

> Zweck: schneller Überblick "was weiß ich über DIESES Board", bevor ein neuer Test beginnt.
> Vor jedem Test kurz nachschauen, nach jedem Test die Zeile aktualisieren/ergänzen.
> Ausführliche Historie bleibt in `project-status.md` - hier nur der aktuelle Kurzstatus.

| Seriennummer | HW-Umbau (connection-pin) | Zuletzt geflasht | Bekannter Status | Zuletzt getestet |
|---|---|---|---|---|
| **269996** | nein | - | **Hardwaredefekt, bestätigt** (nicht reparabel ohne Bauteil-Nacharbeit) | 2026-08 |
| **26SC9994** | nein | 5.99 | Unauffällig, Referenzboard (151 Betriebsstunden, seit Wochen stabil) | 2026-09-30 |
| **26SC9983** | nein | 5.108 | **Vermutlich Hardwaredefekt** — Verbindung hält keine ~20s, Factory-Reset+Power-Cycle ändert nichts, Konfiguration byte-identisch zu 26SC9994. Verdacht: fehlende Entkopplung C128/C58. Vorher fälschlich "269983" geschrieben. | 2026-09-30 |
| **12SC1234** | nein | 5.99 | War Contention-Test-Bug (bt_cyspp_out fehlte, behoben in 5.105) — sonst unauffällig | 2026-09-29 |
| **12SC3456** | **ja** — Hand-Umbau durch Anwender (IF820 Pin33/CONNECTION → IC54 Pad IO5, GND-Bahn dafür aufgetrennt, 18 kΩ Pull-up) | 5.103 (Stage B zurückgebaut) | Split-Brain bestätigt (CONNECTION verhält sich wie CYSPP, keine Divergenz). **Zusätzlich ältere, ungeklärte Historie:** Spannungsversorgungs-Verdacht seit 2026-06-21 (Brownout auf 3,3V-Schiene bei BT/LTE-Sendespitzen, siehe project-status.md ganz oben) — "PC findet Gerät nur schlecht, verliert Verbindung wieder". Nicht mehr auf Mainline flashen ohne den HW-Umbau! | 2026-09-29 |

## Hinweise

- **26SC9983** hieß in `project-status.md` bis 2026-09-30 fälschlich "269983" (Tippfehler, durchgängig korrigiert).
- **12SC3456** ist das einzige Board mit dem `connection-pin`-Hardware-Umbau — nur dieses Board darf den Branch `connection-pin` bekommen (siehe `CLAUDE.md`, Branch-Hinweis). Alle anderen Boards laufen auf `LTE-Update` (Mainline).
- Bei neuen Boards: Zeile hier ergänzen, sobald der erste Test/Befund vorliegt.
