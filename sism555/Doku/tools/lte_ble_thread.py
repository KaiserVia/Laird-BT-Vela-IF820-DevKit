# BLE-Verbindung waehrend der GSM-/MQTT-Thread laeuft (Einwahl). Monitor 3->7 (5.136) zeigt Thread + CONNECTION.
# Ablauf: Monitor an, MQTT-Debug an (Modem an, Einwahl-Thread), bei "GSM-Thread aktiv" + delay s BLE verbinden,
# CYSPP-Notify abonnieren, bis hold s mitlesen (PIN-Prompt?), danach MQTT-Debug aus + Trennen, Monitor aus.
# python lte_ble_thread.py [delay_s] [hold_s] [x]   (x: erst den zweiten Thread, MQTT-Verbindungsaufbau, abwarten)
# Umgebungsvariablen: LTEBLE_PORT (RS232, Vorgabe COM3), LTEBLE_NAME (BLE-Namensanfang, Vorgabe VIASIS_26SC9966).
# Voraussetzung: Firmware ab 5.136, Geraet fuer MQTT eingerichtet (device_setup.py), MQTT-Debug aus.
# Sendet am Geraet nur H, 3, 7, 5, 2, Strg+G, 4, 8. Ergebnis 2026-11-09: Doku in project-status.md (LTE-BLE-Test).
import asyncio, os, re, sys, threading, time
import serial
from bleak import BleakClient, BleakScanner

PORT = os.environ.get("LTEBLE_PORT", "COM3"); NAME = os.environ.get("LTEBLE_NAME", "VIASIS_26SC9966")
CYSPP_TX = "65333333-a115-11e2-9e9a-0800200ca102"
delay = float(sys.argv[1]) if len(sys.argv) > 1 else 10
hold = float(sys.argv[2]) if len(sys.argv) > 2 else 90
t0 = time.time(); lines = []; stop = threading.Event()
s = serial.Serial(PORT, 115200, timeout=0.1)

def log(src, t):
    print(f"{time.time()-t0:7.1f} {src:5} {t}", flush=True)

def rd():
    b = b""
    while not stop.is_set():
        for ch in s.read(s.in_waiting or 1):
            if ch == 10:
                t = b.decode("latin-1").strip(); b = b""
                if t: lines.append((time.time(), t)); log("RS232", t)
            elif ch != 13: b += bytes([ch])
threading.Thread(target=rd, daemon=True).start()

def key(k, w=0.8): s.write(k); time.sleep(w)
def wait_for(rx, since, timeout):
    end = time.time() + timeout
    while time.time() < end:
        for ts, t in list(lines):
            if ts >= since and re.search(rx, t): return ts
        time.sleep(0.1)
    return None
def monitor(on):
    t = time.time(); key(b"H"); key(b"3", 1.2)
    running = wait_for(r"CYSPP monitor \(laeuft", t, 1) is not None
    if running != on: key(b"7", 1.5)
    key(b"H", 1)
def debug_menu():
    key(b"H"); key(b"5"); key(b"2"); key(b"\x07", 1.2)

DEV = None
async def find():
    return await BleakScanner.find_device_by_filter(lambda d, a: (d.name or "").startswith(NAME), timeout=15)
async def ble(hold):
    dev = DEV
    if not dev: log("PC", "BLE: nicht gefunden"); return
    rx = bytearray(); t = time.time()
    def cb(_, data):
        rx.extend(data); log("NOTIF", repr(bytes(data))[:120])
    try:
        log("PC", f"BLE: verbinde {dev.address}")
        async with BleakClient(dev, timeout=30) as c:
            log("PC", f"BLE: verbunden nach {time.time()-t:.1f} s")
            ts = time.time(); await c.start_notify(CYSPP_TX, cb); log("PC", f"BLE: Abo ok nach {time.time()-ts:.1f} s")
            end = time.time() + hold
            while time.time() < end and c.is_connected:
                await asyncio.sleep(0.5)
                if b"PIN" in rx: log("PC", f"BLE: PIN-Prompt nach {time.time()-t:.1f} s ab Verbindungsbeginn"); break
            log("PC", f"BLE: Ende, verbunden={c.is_connected}, {len(rx)} Bytes empfangen")
    except Exception as e:
        log("PC", f"BLE: Fehler nach {time.time()-t:.1f} s: {type(e).__name__} {e}")

DEV = asyncio.run(find()); log("PC", f"vorab gefunden: {DEV}")
try:
    monitor(False); monitor(True)                  # frisch starten (5-min-Laufzeit von vorn)
    t_dbg = time.time(); debug_menu()
    if not wait_for(r"MQTT debug mode:\s+aus", t_dbg, 3): raise SystemExit("Debug-Modus nicht 'aus' - Abbruch")
    key(b"4", 1.5); key(b"H", 1)                   # MQTT-Debug an -> Modem an, Einwahl
    log("PC", "MQTT-Debug an, warte auf GSM-Thread aktiv")
    ta = wait_for(r"GSM-Thread aktiv|Power GSM|Verbinde", t_dbg, 180)
    if len(sys.argv) > 3:   # zweiten Thread (MQTT-Verbindungsaufbau) abwarten
        tb = wait_for(r"GSM-Thread beendet", ta, 120); ta = wait_for(r"GSM-Thread aktiv", tb + 0.01, 120)
        if not ta: raise SystemExit("kein zweiter Thread")
    if not ta: raise SystemExit("kein 'GSM-Thread aktiv' innerhalb 180 s")
    time.sleep(max(0, delay - (time.time() - ta)))
    log("PC", f"--- BLE {delay:g} s nach Thread-Start")
    asyncio.run(ble(hold))
    wait_for(r"BT-Sitzung beendet|BEREIT", time.time() - 5, 20)
    time.sleep(5)
finally:
    t_end = time.time(); debug_menu()
    if wait_for(r"MQTT debug mode:\s+ein", t_end, 3): key(b"4", 1.5); key(b"\x07", 1.2)
    key(b"8", 3)                                   # vom Server trennen
    monitor(False)
    stop.set(); time.sleep(0.3); s.close()
