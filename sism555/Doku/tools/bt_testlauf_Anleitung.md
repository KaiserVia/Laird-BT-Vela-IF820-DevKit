# bt_testlauf.py - automatischer BT-Testlauf am Tisch (Stand 2026-11-02)

Ein Aufruf prüft ein angeschlossenes Board auf Bluetooth-Fehler nach Neustart, Stromausfall, beim Hochlauf, bei
LTE-Betrieb, beim Zeitlimit und beim Firmware-Update über Bluetooth. Das Skript erkennt das Board selbst, führt die
Einzeltests aus `ble_mainboard_test.py` in der richtigen Reihenfolge aus und schreibt eine Zusammenfassung.
Hintergrund und Begründung der Testfälle: `../Testplan_BT_Erweiterung_Strom_Grenzfaelle.md`.

## Aufbau

| Gerät | Anschluss | Hinweis |
|---|---|---|
| Board | 12 V über **Relais 2** des USB-Relais (NC-Kontakt), siehe `relay_power_Anleitung.md` | nur eine Versorgung, kein USB-Strom gleichzeitig |
| RS232 des Boards | externer USB-RS232-Adapter, **COM3** | nicht den USB-Port des Boards verwenden, der verschwindet beim Stromlos-Schalten |
| USB-Relais | **COM7** | |
| PC-Bluetooth | eingeschaltet | BLE geht immer; Classic nur, wenn das Board in Windows gekoppelt ist |

Kein Terminalprogramm darf COM3 oder COM7 offen haben.

## Starten

Im Ordner `sism555\Doku\tools`:

```
python bt_testlauf.py --plan          # zeigt nur, was laufen wuerde (Board-Erkennung inklusive)
python bt_testlauf.py --profil kurz   # ca. 25 min
python bt_testlauf.py                 # voller Lauf, ca. 1,5 bis 2 h
```

Oder `bt_testlauf.bat` doppelklicken (voller Lauf, Fenster bleibt am Ende offen).

Weitere Optionen:

| Option | Wirkung |
|---|---|
| `--nur nightly,boot-race` | nur diese Tests |
| `--ohne bt-update` | diese Tests auslassen |
| `--wiederholungen 5` | Wiederholungen je Test (statt Profil) |
| `--bin <datei>` | Firmware für `bt-update` (Default `sism555\sism.bin`), `--bin keins` lässt `bt-update` aus |
| `--no-classic` | Classic nicht prüfen, auch wenn gekoppelt |
| `--rs232-port`, `--relay-port`, `--relay` | andere Anschlüsse |

## Was passiert

1. **Vorprüfung:** Informationsseite über RS232 lesen (Seriennummer, Firmware, Schnittstellen). Daraus BLE-Name
   `VIASIS_<Seriennummer>` und PIN (erste 2 + letzte 4 Ziffern). Relais auf "Strom an". In der Windows-Registry
   nachsehen, ob es für das Board einen Classic-COM-Port gibt.
2. **Tests nacheinander** (Reihenfolge fest, weil `bt-update` das Geräteprotokoll löscht, das `nightly` braucht):

| Test | Prüft | Profil kurz | Profil voll |
|---|---|---|---|
| `nightly` | BT nach dem täglichen Neustart um 3:15 (Uhr wird auf 03:13:30 gestellt; danach zurück auf PC-Zeit). Bei Fehlschlag Stromzyklus und Gegenprobe. | 1x | 2x |
| `reboot-check` | BT nach 0,1 s Unterbrechung und nach 10 s Stromzyklus | je 1x | je 3x |
| `power-sweep` | BT nach Stromlos-Zeiten 0,05 bis 30 s | 0,1 / 1 / 10 s | 9 Zeiten, je 2x |
| `boot-race` | Verbinden 1 bis 12 s nach dem Einschalten | 2 und 8 s | 6 Zeiten, je 2x |
| `gsm-mux` | BT nach Modem-Terminal (UART1-MUX auf GSM) - nur bei Boards mit LTE | 1x | 2x |
| `bt-timeout` | Gerät trennt nach 6 min ohne Eingabe und ist danach wieder verbindbar (BLE, bei Kopplung auch Classic) | - | 1x |
| `bt-update` | Firmware-Update über Classic, danach PIN-Prompt über dieselbe Verbindung - nur bei Classic-Kopplung. **Löscht Protokoll und Messdaten.** | 1x | 1x |

3. Nach jedem fehlgeschlagenen Test: Stromzyklus 10 s und 45 s Hochlauf, damit der nächste Test sauber beginnt.
4. Am Ende: Relais immer auf "Strom an".

## Ergebnis

Ordner `Testreports\<Datum>_<Seriennummer>_testlauf\`:

- `testlauf.md` - Tabelle: Test, Ergebnis, Dauer, Erfolg je Wert (z. B. je Stromlos-Zeit), Name des Einzelberichts.
- `testlauf.json` - dasselbe maschinenlesbar.
- `NN_<test>.log` - vollständige Ausgabe jedes Tests.

Die Einzelberichte (HTML, JSON, Trace mit allen RS232-Zeilen) liegen wie gewohnt in eigenen Ordnern unter
`Testreports\`. Jeder Schritt enthält dort u. a. die Zahl der `[BT-Event]`-Zeilen: **0 Events bei einem
fehlgeschlagenen BLE-Versuch ist das Kennzeichen des UART1-Fehlers** (das Modul kann nichts an den LPC senden).

Rückgabewert: 0 = alles bestanden, 1 = mindestens ein Test nicht bestanden, 2 = Vorprüfung gescheitert.

## Bekannte Grenzen

- Nach dem 3:15-Neustart gibt die Firmware auf RS232 kein Banner aus; `nightly` weist den Neustart über den
  Protokolleintrag `Reboot;..;03:15:00` nach.
- `gsm-mux` braucht ein Board mit LTE-Modem (wird sonst ausgelassen).
- `nightly` setzt voraus, dass die Batteriespannung über der Grenze liegt (sonst unterdrückt die Firmware den Neustart).
- Ein erster BLE-Versuch scheitert gelegentlich am PC (bekanntes Windows-Verhalten); `smoke` wiederholt dann einmal.
  `boot-race` wertet den Erstversuch getrennt aus.

## Einzeltests direkt

Jeder Test lässt sich auch einzeln starten, z. B.:

```
python ble_mainboard_test.py --mode nightly --nightly-recover --name VIASIS_26SC9983 --pin 269983 --rs232-port COM3 --no-classic
```

Optionen der Einzeltests: `python ble_mainboard_test.py --help` und Abschnitte 3 und 5 im Testplan.
