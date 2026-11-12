# relay_power.py - Geraet per USB-Relais vom Strom trennen (Stand 2026-10-29)

Schaltet die 12-V-Versorgung des Geraets ueber ein **Devantech USB-RLY16L** (DFRobot/Devantech,
8 Relais, 16 A, **rastend**). Sinn: Board/BT-Modul fuer Messreihen und nach Ausfaellen ohne Handgriff
komplett stromlos machen (das IF820 braucht teils eine laengere Stromlos-Zeit, siehe `CLAUDE.md`).

## Verdrahtung

Nur die **Plus-Leitung** auftrennen, Minus bleibt direkt vom Netzteil zum Geraet.

| Klemme am Relais | Anschluss |
|---|---|
| `relay` (Mitte, COM) | 12 V Plus vom Netzteil |
| `0` (NC) | Plus-Eingang des Geraets |
| `1` (NO) | frei |

Relais aus = Geraet hat Strom. Relais ein (angezogen) = Geraet stromlos. Durchgang vorher mit dem
Multimeter pruefen: `relay`-`0` muss bei stromlosem Board durchgehen. Eingesetzt wird **Relais 2**.

**Regel:** pro Board nur eine Versorgungsquelle (Netzteil **oder** USB, nie beides).

## Anschluss am PC

- USB-**2.0**-Port verwenden (USB-3-Ports stoeren das 2,4-GHz-Band).
- Das Board erscheint als virtueller COM-Port (`USB\VID_04D8&PID_FFEE`), hier **COM7**.
  Die Baudrate ist egal, das Skript nimmt 19200.

## Benutzung (im Ordner `sism555\Doku\tools`)

```
python relay_power.py status
python relay_power.py aus          # Geraet stromlos
python relay_power.py ein          # Geraet unter Strom
python relay_power.py zyklus 15    # 15 s stromlos, danach wieder ein
```

Optionen: `--port COM7`, `--relay 2`. Das Skript braucht `pyserial`.

## Befehle des Boards (Einzelbytes)

| Byte | Wirkung |
|---|---|
| 100 / 110 | alle Relais ein / aus |
| 101..108 / 111..118 | Relais 1..8 ein / aus |
| 0x5A | Version lesen (2 Bytes) |
| 0x5B | Zustand lesen (Bit n-1 = Relais n angezogen) |

Quelle: Devantech-Handbuch des Schwestermodells und Foren, nicht aus einem Datenblatt der 16L.
Praktisch bestaetigt (2026-10-29): Version 15/3, Relais 2 ein/aus schaltet das Geraet wie erwartet.

## Hinweise

- Das Relais **rastet**: Es behaelt den Zustand auch ohne USB und nach PC-Neustart. Bricht ein Lauf
  im Zustand "stromlos" ab, bleibt das Geraet aus. Dann `status` pruefen und `ein` ausfuehren.
  `zyklus` schaltet bei Abbruch (Strg+C) selbst wieder ein.
- Beim Abziehen des USB-Kabels verschwindet der COM-Port, der Schaltzustand bleibt.
- Stromlos-Zeit: mit 10 bis 15 s anfangen; ob das fuer das IF820 reicht, im Test klaeren.
