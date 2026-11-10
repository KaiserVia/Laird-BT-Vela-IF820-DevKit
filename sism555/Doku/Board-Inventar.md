# Board-Inventar

> Zweck: schneller Überblick "was weiß ich über DIESES Board", bevor ein neuer Test beginnt.
> Vor jedem Test kurz nachschauen, nach jedem Test die Zeile aktualisieren/ergänzen.
> Ausführliche Historie bleibt in `project-status.md` - hier nur der aktuelle Kurzstatus.
> Stand 2026-11-10.

| Seriennummer | Kurzname | HW-Besonderheiten | Zuletzt geflasht | Bekannter Status | Zuletzt getestet |
|---|---|---|---|---|---|
| **26SC9983** | B | **R125 nach GND umgebaut (Pull-down, 2026-11-10)**, **kein** P0.19-Draht; frueher Quarztausch am IF820 (+10 MΩ parallel) | 5.137 | Mit Pull-down `online-bt`: BLE waehrend LTE **3/3**, Classic 3/3 (vorher mit Pull-up BLE 0/3). Ohne LTE unauffaellig (volles Profil 5.134 bestanden). | 2026-11-10 |
| **26SC9966** | - | **R125 nicht bestückt** (Modul-CTS offen), CONNECTION -> P0.19 verdrahtet, LTE + SIM | 5.137 | BLE und Classic während LTE in allen Läufen bestanden, 5.137-Auslöser mit `/leave`/`/join` bestanden; Robustheit (gsm-mux, boot-race 18/18, power-sweep 27/27, cycle 12/12) bestanden. | 2026-11-09 |
| **99SC9923** | A | CONNECTION -> P0.19 verdrahtet (früher über IC54-Pad IO5) | 5.135 | module-suite 5.135 am 05.11. bis zum Abbruch ohne Fehler (Classic im langen Lauf langsam, ca. 2,6 KB/s). | 2026-11-05 |
| **26SC9994** | C | unverändert, HW V2 | 5.134 | Umfassender Test 5.134 bestanden (bis auf 1x kurze Unterbrechung 0,1 s); scheiterte vor 5.127 an UART1-Abschaltung im Leerlauf. | 2026-11-04 |
| **26SC9993** | - | ? | ? | IF820 startet bei jeder Verbindung neu (`BOOT`). Verdacht CONNECTION-Draht, nicht Versorgung. In Reparatur; Messanleitung `Anleitung_26SC9993_Neustart_beim_Verbinden.md`. | 2026-11-09 |
| **11SC1234** | - | früher als 12SC1234 bezeichnet; **Modem abgeschaltet** (Konfiguration Modem/Email = n) | 5.135 | Classic geht, BLE: PC bekommt keine GATT-Antwort, danach wirbt das Modul bis zum Stromzyklus nicht mehr. Ursache offen (PC oder Board). | 2026-11-05 |
| **12SC3456** | - | Hand-Umbau IF820 Pin 33/CONNECTION -> IC54 Pad IO5 (GND-Bahn aufgetrennt, 18 kΩ Pull-up) | 5.103 | Alter Umbau, Spannungsversorgungs-Verdacht seit 2026-06. Nicht ohne Prüfung weiterverwenden. | 2026-09-29 |
| **269996** | - | - | - | **Hardwaredefekt, bestätigt** (nicht reparabel ohne Bauteil-Nacharbeit) | 2026-08 |

## Hinweise

- **R125:** Ob R125 bestückt ist, entscheidet, ob BLE während LTE geht (Befund 2026-11-10, `project-status.md`). Bei jedem
  neuen Board notieren.
- **P0.19-Draht:** nur nötig für den 5.137-Auslöser (CONNECTION). Ohne Draht liest P0.19 dauerhaft HIGH, der Auslöser
  bleibt stumm, sonst keine Wirkung (`BT_CONN_GPIO` = 0 seit 5.120). Alle Boards dürfen `connection-pin`-Firmware bekommen.
- **26SC9983** hieß in `project-status.md` bis 2026-09-30 fälschlich "269983" (Tippfehler, durchgängig korrigiert).
- Bei neuen Boards: Zeile hier ergänzen, sobald der erste Test/Befund vorliegt.
