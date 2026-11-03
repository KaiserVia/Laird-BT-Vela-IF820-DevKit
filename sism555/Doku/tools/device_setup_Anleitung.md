# device_setup.py - Mobilfunk/MQTT einstellen und pruefen

Stellt ein sism555-Geraet ueber RS232 so ein, wie es in `device_setup.json` steht, und prueft es.
Geaendert wird nur, was abweicht. Gedacht fuer neue Boards, nach einem Werksreset und vor Testlaeufen,
die das Geraet online brauchen.

## Voraussetzungen

- Geraet an RS232 (Standard `COM3`), kein Terminalprogramm auf dem Port.
- Modem in der Geraetekonfiguration eingetragen ("Schnittstellen: ... Modem/Email-LTE"). Fehlt es,
  bricht das Skript ab: das versteckte Menue "Change configuration" ruft es bewusst nicht auf (siehe unten).
- Zertifikate im Modem (`CA.CRT`, `<serno>.CRT`, `<serno>.KEY`, hochgeladen ueber Testmenue -> versteckte
  Konfiguration -> 6 Component tests -> 7 Upload files to modem). Das Skript laedt sie nicht hoch.
- SIM-Karte gesteckt. Die datamobile-SIM ist im Roaming: mit Roaming **aus** scheitert der APN-Test (F12005).

## Aufrufe

```
python device_setup.py --check              # nur lesen und vergleichen
python device_setup.py                      # Abweichungen einstellen, danach Nachpruefung
python device_setup.py --online             # zusaetzlich: MQTT-Debug an, auf Serververbindung warten, wieder aus
python device_setup.py --persist            # zusaetzlich: 5 s stromlos (Relais COM7/2), danach nachpruefen
python device_setup.py --net                # zusaetzlich: Netzregistrierung (bis 4 min, nur wenn offline)
python device_setup.py --config andere.json # andere Sollwerte
```

Rueckgabe 0 = alles wie gewuenscht, 1 = Abweichung/Fehler, 2 = Geraet antwortet nicht.
Rohmitschnitt jedes Laufs: `Testreports/<Datum>_device_setup.log`.

## Sollwerte (`device_setup.json`)

| Feld | Bedeutung |
|---|---|
| `servertyp` | `MQTT` oder `SMTP` |
| `apn`, `apn_user`, `apn_pass` | APN-Zugang (leer = nichts) |
| `roaming` | `true`/`false` |
| `mqtt_server`, `mqtt_port` | MQTT-Broker (User/Passwort bleiben unveraendert) |
| `set_clock`, `clock_tolerance_s` | Uhr nach PC-Zeit stellen, wenn sie mehr als n s abweicht |
| `keep_online` | `--online` laesst MQTT-Debug an (Geraet bleibt online, solange RS232 offen ist) |

## Was man wissen muss (aus Code und Versuchen, 2026-11-03)

- **Speichern:** Die Firmware schreibt die Parameter erst beim Ausschalten (Power-Down-Interrupt) oder bei
  einem Neustart ins Flash (`flash.c` `EINT0_IRQHandler`, `libtool.c` `reboot()`). `--persist` prueft das.
- **Firmware-Update** loescht nur Messdaten/Protokoll, die Einstellungen bleiben.
- **"Change configuration" mit "Modem/Email j"** setzt Servertyp, APN und Roaming zurueck.
- **Servertyp wechseln** setzt MQTT-Server und Port auf Standard (`cloud.viatraffic.com:8883`) zurueck,
  deshalb stellt das Skript den Server erst nach dem Servertyp ein.
- **Gesperrt waehrend online:** Servertyp und APN lassen sich nicht aendern, solange GSM/MQTT UART1 halten.
  Das Skript trennt dann zuerst (MQTT-Debug aus, Debug-Menue 8 "Disconnect from Server").
- **Waehrend online keine SIM-/Netzabfrage:** sie schickt AT-Kommandos in die laufende MQTT-Sitzung und
  meldet faelschlich "NOT REGISTERED". Online heisst ohnehin eingebucht.
- **Zeitfenster:** Board B ist Mo-Fr 08:00-18:00 regulaer online (Mobilfunk-Menue 1 Zeitplanung).
- **Eingaben:** leere Eingabe behaelt einen Textwert, ein einzelnes Leerzeichen loescht ihn (`readline()`).
- **Gefaehrliche Tasten:** im Test- und Einstellungsmenue setzt `X` alle Parameter auf Werkszustand.
  Das Skript sendet nur Ziffern, `H`, `y`/`n`, Strg+F und Strg+G.
