# Bring-up-Plan: neues Mainboard (TEMPORAER - eigener Git-Branch)

Kontext: neues Mainboard, **Verkabelung unklar** (TXD/RXD/CTS/RTS), und **unklar, welche Firmware
auf dem BT-Modul laeuft**. Wir brauchen erst den einfachsten Verbindungs-Check und eine simple
Debug-Konsole, um manuell (HyperTerminal) mit dem Modul zu reden und /PING wieder zum Laufen zu
bringen. **Alle Aenderungen sind temporaer und werden spaeter zurueckgenommen.**

## Schalter / Rückbau

- Zentraler Schalter: `#define BRINGUP_DEBUG 1` in **btio.h**.
  - `1` = Bring-up-Modus aktiv (Auto-Init aus, Debug-Konsole an).
  - `0` = normales Verhalten (alle Bring-up-Teile inaktiv).
- **Revert vor Merge:** `BRINGUP_DEBUG` auf 0, dann `bt_bringup()` (btio.c), den Menue-Hook
  (sicom.c) und den main.c-Guard wieder entfernen. Alle Stellen sind mit `BRINGUP` markiert.

## Aenderungen

1. **Auto-Init deaktiviert (main.c).** Die Routine "BT nicht gefunden -> init_bluetooth()"
   (Self-Heal + automatische Konfiguration) wird bei `BRINGUP_DEBUG` **nicht** ausgefuehrt.
   Grund: bei unklarer Verkabelung soll nichts automatisch konfigurieren/umstellen.

2. **Debug-Konsole `bt_bringup()` (btio.c)** - bewusst **CTS-unabhaengig**:
   - Hardware-Auto-RTS/CTS wird ausgeschaltet, gesendet wird direkt ueber `U1->THR`
     (nur THRE-Pruefung, **kein** Warten auf CTS). So blockiert nichts, auch wenn CTS/RTS
     (noch) nicht verdrahtet sind.
   - Alle Antworten werden **roh als hex + ASCII** ausgegeben (Muell wird sichtbar).
   - Funktionen:
     - **Loopback-Test** (Jumper TXD<->RXD am Stecker): sendet Testtext, zeigt was zurueckkommt.
       Bestaetigt MCU-UART1 + Leitung bis zum Stecker, unabhaengig vom Modul.
     - **/PING senden**: bei eingestellter Baud, Rohantwort anzeigen.
     - **Baud-Scan /PING**: probiert 9600/19200/38400/57600/115200/230400/460800/921600.
     - **Eigenes Kommando**: frei eintippen (z. B. `AT`, `$$$`, `/PING`) -> ans Modul -> Rohantwort.
       Damit lassen sich verschiedene Modul-Firmwares (EZ-Serial/RN4678/Laird) durchprobieren.
     - **Baud waehlen** (durchschalten).

3. **Menue-Hook (sicom.c).** Im Konfig-Ablauf "Bluetooth (j/n)?" startet bei `j` und
   `BRINGUP_DEBUG` die **Debug-Konsole** statt `init_bluetooth()`. Minimaler, leicht
   rueckbaubarer Eingriff (eine Stelle).

## Bedienung (ueber RS232 / HyperTerminal)

Hauptmenue -> Einstellungen/Konfiguration -> "Bluetooth (j/n)? **j**" -> Debug-Konsole.
(RS232 115200 8N1 zum Mainboard wie gewohnt.)

## Empfohlene Test-Reihenfolge

1. **Loopback:** am BT-Stecker **TXD mit RXD bruecken** (Jumper), Loopback-Test starten.
   - Kommt der Text zurueck -> MCU-UART1 + MUX + Verdrahtung bis zum Stecker sind ok.
   - Kommt nichts -> Problem zwischen MCU und Stecker (MUX/Leitung/Pegel).
2. **Jumper entfernen, Modul anschliessen.** /PING bei 115200 senden.
   - `@R,...,/PING,0000` -> Modul ist EZ-Serial (IF820) und TXD/RXD korrekt.
3. **Keine Antwort -> Baud-Scan /PING.** Findet die Baud, falls das Modul auf einer anderen laeuft.
4. **Weiter unklar -> Eigenes Kommando.** `AT` (Laird/AT-Firmware), `$$$` (RN4678),
   `/PING` (EZ-Serial) ausprobieren -> an der Antwort erkennt man die Modul-Firmware.
   - Tipp: bei vertauschten TXD/RXD kommt gar nichts; bei falscher Baud kommt Muell (hex zeigt es).

## Wenn /PING wieder geht

Dann ist die Verkabelung bestaetigt. Danach `BRINGUP_DEBUG` auf 0, Bring-up-Code entfernen und
auf dem normalen Stand weiterarbeiten (Auto-Init wieder aktiv).
