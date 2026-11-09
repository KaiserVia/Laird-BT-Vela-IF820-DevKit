#!/usr/bin/env python3
"""
if820_boot_check.py - Neustarts des IF820 beim Verbinden sichtbar machen (Fehlerbild Board 26SC9993).

Oeffnet am Geraet ueber RS232 den Modus "Modulbefehl senden" (Hauptmenue 3 -> 8 -> 7, ab Firmware 5.129). In diesem
Modus zeigt die Firmware jede Ausgabe des IF820 (@E-Ereignisse, @R-Antworten) und jeden Wechsel der CYSPP-Leitung mit
Zeitstempel. Das Skript loest dann Verbindungsversuche vom PC aus und zaehlt die Modul-Neustarts (@E,...,BOOT,...).

  python if820_boot_check.py --port COM3 --idle 40                       # nur Leerlauf beobachten
  python if820_boot_check.py --port COM3 --ble VIASIS_26SC9993           # BLE-Verbindungsversuche (Name oder Adresse)
  python if820_boot_check.py --port COM3 --classic COM17                 # Classic: Bluetooth-COM-Port oeffnen
  python if820_boot_check.py --port COM3 --ble E8:05:D6:F7:F8:C3 --runden 20 --pause 5   # Dauerschleife fuers Oszi

Auf einem gesunden Board erscheint beim Verbinden @E,...,C,... (BLE) bzw. @E,...,ENC,... (Classic) und KEIN BOOT.
Auf 26SC9993 kam bei BLE etwa alle 10 s ein BOOT, bei Classic ca. 2,6 s nach ENC.

Das Skript sendet am Geraet nur H, 3, 8, 7, /PING und Enter. Es aendert keine Einstellungen.
Voraussetzungen: pip install pyserial bleak. Fuer --classic muss das Board in Windows gekoppelt sein.
"""
import argparse, asyncio, re, sys, threading, time

import serial

t0 = time.time()
boots = 0
lock = threading.Lock()


def log(src, text):
    with lock:
        print(f"{time.time()-t0:7.1f}  {src:6} {text}", flush=True)


def reader(s, stop):
    global boots
    line = b""
    while not stop.is_set():
        for ch in s.read(s.in_waiting or 1):
            if ch == 10:
                t = line.decode("latin-1").strip()
                line = b""
                if not t:
                    continue
                if ",BOOT," in t:
                    boots += 1
                    t += "   <<< MODUL-NEUSTART"
                log("RS232", t)
            elif ch != 13:
                line += bytes([ch])


def key(s, k, wait=0.8):
    s.write(k)
    time.sleep(wait)


async def ble_try(target, timeout):
    from bleak import BleakClient, BleakScanner
    if re.fullmatch(r"([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}", target):
        dev = await BleakScanner.find_device_by_address(target, timeout=15)
    else:
        dev = await BleakScanner.find_device_by_filter(lambda d, a: (d.name or "").startswith(target), timeout=15)
    if dev is None:
        log("PC", f"BLE: {target} nicht gefunden (wirbt nicht?)")
        return
    log("PC", f"BLE: verbinde mit {dev.name} {dev.address}")
    t = time.time()
    try:
        async with BleakClient(dev, timeout=timeout) as c:
            log("PC", f"BLE: verbunden nach {time.time()-t:.1f} s, {len(list(c.services))} Dienste")
            await asyncio.sleep(3)
        log("PC", "BLE: getrennt")
    except Exception as e:
        log("PC", f"BLE: Fehler nach {time.time()-t:.1f} s: {type(e).__name__} {e}")


def classic_try(port, hold):
    log("PC", f"Classic: oeffne {port}")
    t = time.time()
    try:
        c = serial.Serial(port, 115200, timeout=1)
        log("PC", f"Classic: {port} offen nach {time.time()-t:.1f} s, halte {hold} s")
        time.sleep(hold)
        c.close()
        log("PC", "Classic: geschlossen")
    except Exception as e:
        log("PC", f"Classic: Fehler nach {time.time()-t:.1f} s: {e}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="COM3", help="RS232 des Geraets (115200)")
    ap.add_argument("--idle", type=float, default=0, help="nur beobachten, Sekunden")
    ap.add_argument("--ble", help="BLE-Name (Anfang, z. B. VIASIS_26SC9993) oder Adresse")
    ap.add_argument("--classic", help="Bluetooth-COM-Port des gekoppelten Boards (ausgehend)")
    ap.add_argument("--runden", type=int, default=2, help="Anzahl Verbindungsversuche")
    ap.add_argument("--pause", type=float, default=5, help="Sekunden zwischen den Versuchen")
    ap.add_argument("--timeout", type=float, default=30, help="BLE-Verbindungs-Timeout in s")
    ap.add_argument("--halten", type=float, default=5, help="Classic: Port so lange offen halten (s)")
    a = ap.parse_args()
    if not (a.idle or a.ble or a.classic):
        ap.error("--idle, --ble oder --classic angeben")

    s = serial.Serial(a.port, 115200, timeout=0.1)
    stop = threading.Event()
    threading.Thread(target=reader, args=(s, stop), daemon=True).start()
    log("PC", "Modulbefehl-Modus oeffnen (H, 3, 8, 7)")
    key(s, b"H"); key(s, b"3"); key(s, b"8", 1.5); key(s, b"7", 1.5)
    for ch in b"/PING":
        s.write(bytes([ch])); time.sleep(0.05)
    s.write(b"\r"); time.sleep(1.5)
    try:
        if a.idle:
            log("PC", f"Leerlauf {a.idle:.0f} s")
            time.sleep(a.idle)
        for i in range(a.runden if (a.ble or a.classic) else 0):
            log("PC", f"--- Runde {i+1}/{a.runden}, Neustarts bisher {boots}")
            if a.ble:
                asyncio.run(ble_try(a.ble, a.timeout))
            if a.classic:
                classic_try(a.classic, a.halten)
            time.sleep(a.pause)
    except KeyboardInterrupt:
        log("PC", "abgebrochen")
    finally:
        s.write(b"\r"); time.sleep(2)        # leere Zeile beendet den Modulbefehl-Modus
        key(s, b"H", 1)
        stop.set(); time.sleep(0.3); s.close()
    print(f"\nModul-Neustarts (@E,...,BOOT): {boots}")
    return 1 if boots else 0


if __name__ == "__main__":
    sys.exit(main())
