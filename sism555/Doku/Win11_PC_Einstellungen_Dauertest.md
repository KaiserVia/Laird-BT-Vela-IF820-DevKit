# Windows 11: PC-Einstellungen für Dauertests (Standby, USB, Bluetooth)

Anlass: Im Lauf `2026-10-21_09-30-04_VIASIS_26SC9983_module-suite` brachen die letzten beiden BLE-Downloads ab, weil der
PC nach dem Sperren (Win+L) in den modernen Standby ging. Dabei verschwand der USB-RS232-Adapter an der Docking-Station
und Windows drosselte Bluetooth. Mit Firmware oder Modul hat das nichts zu tun.

Das Testskript hält den PC seit Version vom 2026-10-21 selbst wach (System und Bildschirm), das genügt aber nicht immer
(Sperrbildschirm, USB-Energiesparen, Docking-Station). Die folgenden Einstellungen machst **du** am PC. Sie sind
Systemeinstellungen und werden vom Testskript nicht verändert.

Notiere dir vorher die alten Werte (Abschnitt "Zurückstellen" am Ende).

---

## 1. Bildschirm und Energiesparmodus ausschalten (Netzbetrieb)

1. **Start** → **Einstellungen** (Zahnrad) öffnen (oder `Win+I`).
2. Links **System** → **Energie & Akku** (je nach Version "Netzbetrieb & Akku").
3. **Bildschirm, Ruhezustand und Energiesparmodus** aufklappen.
4. **Bildschirm ausschalten nach** → bei "Netzbetrieb": **Nie**.
5. **Gerät in den Energiesparmodus versetzen nach** → bei "Netzbetrieb": **Nie**.
6. Der Laptop muss während des Tests am Netzteil hängen (nicht im Akkubetrieb).

## 2. Zeitlimit des Sperrbildschirms abschalten

Wenn du mit `Win+L` sperrst, schaltet Windows den Bildschirm nach ca. 1 Minute ab, unabhängig von Schritt 1. Das löst den
Standby aus. Das Zeitlimit ist in der Oberfläche versteckt und wird per Befehl freigeschaltet:

1. **Start**-Menü → `cmd` eingeben → Rechtsklick auf **Eingabeaufforderung** → **Als Administrator ausführen**.
2. Option sichtbar machen:
   ```
   powercfg -attributes SUB_VIDEO 8EC4B3A5-6868-48c2-BE75-4F3044BE88A7 -ATTRIB_HIDE
   ```
3. Zeitlimit für Netzbetrieb auf "nie" (0 Sekunden) setzen:
   ```
   powercfg /setacvalueindex SCHEME_CURRENT SUB_VIDEO 8EC4B3A5-6868-48c2-BE75-4F3044BE88A7 0
   powercfg /setactive SCHEME_CURRENT
   ```
4. Alternativ: während des Tests **nicht sperren** (`Win+L` vermeiden). Wenn du den Platz verlässt, ist Schritt 2 aber
   die einzige Möglichkeit, trotzdem zu sperren.

## 3. Selektives USB-Suspend ausschalten

1. `Win+R` → `control powercfg.cpl` → Enter. (Oder: Systemsteuerung → Energieoptionen.)
2. Beim aktiven Energiesparplan **Energiesparplaneinstellungen ändern**.
3. **Erweiterte Energieeinstellungen ändern**.
4. **USB-Einstellungen** → **Einstellung für selektives USB-Suspend** → bei "Netzbetrieb": **Deaktiviert**.
5. Im gleichen Fenster unter **PCI Express** → **Verbindungszustand-Energieverwaltung** → bei "Netzbetrieb": **Aus**.
6. **Übernehmen** → **OK**.

## 4. Energiesparen für USB-Geräte und Bluetooth im Geräte-Manager ausschalten

1. `Win+X` → **Geräte-Manager**.
2. **Universeller Seriell-Bus-Controller** aufklappen. Für jeden Eintrag **USB-Root-Hub**, **USB Composite Device**,
   **Generic USB Hub** und die Prolific-Einträge:
   Doppelklick → Reiter **Energieverwaltung** → Haken bei **"Der Computer kann das Gerät ausschalten, um Energie zu
   sparen"** **entfernen** → OK. (Reiter fehlt bei manchen Einträgen, die dann überspringen.)
3. **Anschlüsse (COM & LPT)** aufklappen: bei **Prolific USB-to-Serial Comm Port** und den **LPC/USB-Seriell**-Einträgen
   (Mainboard-USB) ebenso den Haken entfernen.
4. **Bluetooth** aufklappen: beim **Funkadapter** (z. B. "Intel Wireless Bluetooth") Doppelklick →
   **Energieverwaltung** → Haken bei "Der Computer kann das Gerät ausschalten …" **entfernen**.
5. **Netzwerkadapter**: bei der WLAN-Karte (sitzt oft auf demselben Chip wie Bluetooth) ebenso.

## 5. Docking-Station

1. Das Netzteil der Docking-Station muss angesteckt sein; Laptop-Netzteil am Dock oder am Laptop.
2. Wenn möglich: **RS232-Adapter und Board-USB direkt am Laptop** anstecken statt an der Docking-Station. Damit entfällt
   der Ausfall der Dock-Ports bei Standby/Sperren. Danach die COM-Portnummern neu prüfen (`comports.bat`), sie ändern
   sich beim Umstecken.
3. Falls die Dock-Ports trotzdem ausfallen: im BIOS/UEFI oder im Hersteller-Tool der Docking-Station nach einer Option
   wie "USB-Ladung im Ruhezustand" bzw. "Dock-Energiesparen" suchen und ausschalten (Herstellerabhängig).

## 6. Windows Update soll nicht mitten im Test neu starten

1. **Einstellungen** → **Windows Update** → **Erweiterte Optionen** → **Nutzungszeit**.
2. **Manuell** wählen und den Zeitraum so setzen, dass er die Testdauer abdeckt (z. B. 08:00–20:00; maximal 18 h).
3. Optional **Updates für 1 Woche pausieren** (Windows Update → "Updates anhalten").

## 7. Kontrolle, dass es wirkt

Befehle in einer normalen Eingabeaufforderung:

```
powercfg /a
```
Zeigt, welche Standby-Zustände das Gerät kennt (z. B. "Standby (S0 mit niedrigem Energieniveau)"). Das ist nur eine
Information.

```
powercfg /requests
```
Während der Test läuft, sollte unter **SYSTEM** und **DISPLAY** ein Eintrag von `python.exe` stehen. Dann funktioniert
das Wachhalten des Skripts.

Nach dem Test die Windows-Ereignisse prüfen: im Report-Ordner liegt `pc_events.json`. Dort dürfen keine
`Kernel-Power` Ereignisse mit ID 506/507 (Standby) im Testzeitraum stehen. Das Skript meldet Standby und verlorene
COM-Ports außerdem live als `TESTUMGEBUNG GESTOERT` und wertet eine dabei gescheiterte Phase als **environment**
(orange), nicht als Fehler.

## 8. Zurückstellen nach den Tests

- Schritt 1: Zeiten wieder auf deine üblichen Werte (z. B. Bildschirm 10 min, Energiesparmodus 30 min).
- Schritt 2 (Befehl, als Administrator):
  ```
  powercfg /setacvalueindex SCHEME_CURRENT SUB_VIDEO 8EC4B3A5-6868-48c2-BE75-4F3044BE88A7 60
  powercfg /setactive SCHEME_CURRENT
  ```
  (60 = 60 Sekunden, der Windows-Standard.)
- Schritte 3 und 4: Einstellungen wieder auf Standard (selektives USB-Suspend "Aktiviert", Haken im Geräte-Manager
  wieder setzen). Das ist optional, spart nur Strom.
