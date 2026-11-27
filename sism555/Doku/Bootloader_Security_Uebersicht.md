# Branch `bootloader` - Uebersicht (Security, Bootloader, Parameter, Werkzeuge)

Angelegt 2026-11-27 von `main` 5.158 (nach dem Merge von `usb-verbindung`). Worktree:
`C:\viaTraffic\GitHub\Projects\Bootloader`. Firmware-Versionen in diesem Branch: **5.b1, 5.b2 ...**
Vor dem Weiterarbeiten `git merge main` nachziehen.

## Dokumente

| Dokument | Inhalt |
|---|---|
| `CRA_EN18031_LPC1766.md` | Was CRA und EN 18031 verlangen, was der LPC1766 kann, Luecken, Speicher-/Rechenzeit-Abschaetzung |
| `Konzept_Bootloader_Security.md` | Software-Umbau: Bootloader (Stufe 0 + A/B), signierte Abbilder, Bootloader-Updates, Umbau der App, Parameter-Handling (Einzelwerte, Tabelle, A/B-Speicherung), Uebergang fuer Geraete im Feld, Haertung, Aufwand, offene Entscheidungen |
| `Werkzeug_Umstieg_Aufwand.md` | Umstieg auf aktuelle Werkzeuge (AC6 oder lizenzfrei GCC + CMSIS-Toolbox, RTX5, MDK-Middleware 8), Kosten, Parallelbetrieb |
| `parameterspeicher.md` (schon in `main`) | Write-Once-Rettungsblock fuer Seriennummer & Co. - Teil des Parameter-Handlings |

## Reihenfolge (aus dem Konzept, Abschnitt 8)

0. Werkzeugumstieg (2-3 Wochen) - vorher `rst-start` nach `main` mergen
1. Krypto-Bibliothek waehlen und auf dem Board messen (0,5-1 Tag)
2. Bootloader (Stufe 0, A/B), Abbild-Format, Signier-Skript
3. App umstellen (Linker 0x9000, Update-Wege -> Slot)
4. Uebergangs-Abbild und Tests (Stromunterbrechung per Relais)
5. Haertung (PIN, BT-Pairing, Protokoll, Serien-Build ohne Testcode)
6. Parameter-Handling

Software zusammen ca. 10-13 Wochen. Stand 2026-11-27: nur Konzept, kein Code.

## Offene Entscheidungen (Anwender)

Siehe `Konzept_Bootloader_Security.md` Abschnitt 9: Signaturverfahren und Schluesselverwaltung, Secure Boot bei jedem
Start, CRP2/CRP3, Erstprogrammierung neuer Platinen, Bestaetigungsbedingung, Erst-PIN, Parameter-Rechte/-Format.
Dazu aus `Werkzeug_Umstieg_Aufwand.md`: AC6 (Keil-Lizenz) oder GCC (lizenzfrei), Lizenzwartung, wer sonst baut.
