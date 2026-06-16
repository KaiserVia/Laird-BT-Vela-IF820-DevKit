# BT Classic SPP — App-Level Authentifizierung

## Architektur

```
┌─────────────┐         BT Classic SPP         ┌──────────┐    UART    ┌──────────┐
│  Handy-App  │ ◄──── RFCOMM (Just Works) ────► │  IF820   │ ◄────────► │ Host-MCU │
│  (Client)   │                                 │ (Bridge) │            │(Gatekeeper)│
└─────────────┘                                 └──────────┘            └──────────┘
```

**IF820 = dumme Pipe.** Leitet Daten 1:1 zwischen BT und UART weiter. Keine eigene Logik.

Die Authentifizierung passiert zwischen Handy-App und Host-MCU — der IF820 reicht nur durch.

---

## Ablauf

```
Zeit    Handy-App                    IF820              Host-MCU
─────────────────────────────────────────────────────────────────────
 t=0    RFCOMM connect ──────────►  EVENT_BT_CONNECTED
                                    (Pipe offen)   ──► UART: merkt "neue Verbindung"
                                                       → Startet AUTH-Timer (5s)
                                                       → Status: UNAUTHENTICATED

 t=1    Sendet: "AUTH:00297\n" ──►  Pipe ──────────►   Empfängt "AUTH:00297\n"
                                                       → Vergleicht mit gespeichertem Secret
                                                       → Match?

 t=2                                                   ✓ Richtig:
                                    Pipe ◄──────────   Sendet: "AUTH:OK\n"
        Empfängt "AUTH:OK\n" ◄────                     → Status: AUTHENTICATED
        → Datenverkehr beginnt                         → Akzeptiert ab jetzt alle Daten

        ODER:

 t=2                                                   ✗ Falsch / Timeout:
                                    Pipe ◄──────────   Sendet: "AUTH:FAIL\n"
        Empfängt "AUTH:FAIL\n" ◄──                     → CMD_DISCONNECT an IF820
                                    EVENT_BT_DISCONNECTED
        Verbindung getrennt                            → Ignoriert alle weiteren Daten
```

---

## Aufgaben pro Komponente

### Host-MCU (IoT-Gerät mit IF820)

| Aufgabe | Detail |
|---|---|
| Secret speichern | `"00297"` fest im Flash oder per Provisioning gesetzt |
| Verbindung erkennen | IF820 sendet `EVENT_BT_CONNECTED` über UART → MCU weiß: jemand ist da |
| Timer starten | 5 Sekunden warten auf `AUTH:xxxxx\n` |
| Validieren | Empfangenes Secret mit gespeichertem vergleichen |
| Antwort senden | `AUTH:OK\n` oder `AUTH:FAIL\n` über UART → IF820 → BT → App |
| Bei Fehler trennen | `CMD_DISCONNECT` an IF820 senden (oder Daten ignorieren) |
| Daten erst nach Auth | Solange Status ≠ AUTHENTICATED: alle eingehenden Daten verwerfen |

### Handy-App (Client)

| Aufgabe | Detail |
|---|---|
| Secret kennen | Fest eingebaut oder vom Benutzer eingegeben (z.B. bei Ersteinrichtung) |
| Nach Connect sofort senden | `"AUTH:00297\n"` als erstes Paket |
| Auf Antwort warten | Max. 5s auf `AUTH:OK\n` oder `AUTH:FAIL\n` |
| Bei OK | Normaler Datenverkehr beginnt |
| Bei FAIL/Timeout | Verbindung trennen, User informieren |

### IF820 (BT-Chip)

| Aufgabe | Detail |
|---|---|
| **Nichts.** | EZ-Serial-Modus = transparente Pipe. Keine Änderung nötig. |

---

## Protokoll-Format

```
Client → Device:    AUTH:<secret>\n
Device → Client:    AUTH:OK\n        (Erfolg)
Device → Client:    AUTH:FAIL\n      (Falsch)
Device → Client:    AUTH:TIMEOUT\n   (Zu spät)
```

Danach: beliebige Anwendungsdaten.

---

## Sicherheitsüberlegungen

| Aspekt | Bewertung |
|---|---|
| Secret über die Luft | BT-Verschlüsselung (SSP/ECDH) schützt vor Mithören |
| Replay-Attacke | Möglich, wenn jemand den Key kennt. Lösung: Challenge-Response |
| Brute-Force | 5-stellig = 100.000 Möglichkeiten. Timer + Disconnect macht Brute-Force impraktikabel |

---

## Upgrade-Option: Challenge-Response (kein Secret über die Luft)

```
Device → Client:    CHALLENGE:<random_nonce>\n
Client → Device:    RESPONSE:<HMAC-SHA256(nonce, shared_secret)>\n
Device → Client:    AUTH:OK\n  oder  AUTH:FAIL\n
```

Vorteil: Shared Secret geht nie über die Luft, selbst wenn BT-Verschlüsselung kompromittiert wäre.
