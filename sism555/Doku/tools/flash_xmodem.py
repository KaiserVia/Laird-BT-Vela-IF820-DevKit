#!/usr/bin/env python3
"""flash_xmodem.py - Firmware per RS232/USB (COM) ins sism555-Geraet flashen.

Ablauf wie von Hand: Hauptmenue -> 3 (Testfunktionen) -> Strg+U -> .bin per XMODEM-1K senden ->
Pruefung abwarten -> Reprogrammieren mit y bestaetigen. Das Geraet loescht dabei Protokoll und Messdaten.

  python flash_xmodem.py --port COM3 --bin ..\\..\\fw_5.99\\sism.bin [--expect-version 5.99]
"""
import argparse, re, sys, time
import serial

SOH, STX, EOT, ACK, NAK, CAN, SUB = 0x01, 0x02, 0x04, 0x06, 0x15, 0x18, 0x1A


def crc16(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


def read_for(ser, secs, until=None):
    end = time.time() + secs
    buf = b""
    while time.time() < end:
        d = ser.read(ser.in_waiting or 1)
        if d:
            buf += d
            if until and until in buf:
                break
    return buf


def txt(b):
    return b.decode("latin-1", "replace").replace("\r", "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--expect-version", help="z. B. 5.99: nach dem Neustart pruefen")
    a = ap.parse_args()
    data = open(a.bin, "rb").read()
    log(f"{a.bin}: {len(data)} Byte")
    ser = serial.Serial(a.port, a.baud, timeout=0.2)
    ser.reset_input_buffer()
    ser.write(b"\r")
    out = txt(read_for(ser, 2.0))
    log("Menue-Zustand: " + out.strip().splitlines()[-1] if out.strip() else "keine Ausgabe")
    ser.write(b"3")
    out = txt(read_for(ser, 3.0, b"1..."))
    log("nach '3': " + " | ".join(out.strip().splitlines()[:3]))
    ser.reset_input_buffer()
    ser.write(bytes([0x15]))
    out = read_for(ser, 4.0, b"C")
    log("nach Strg+U: " + repr(txt(out)[-120:]))
    # auf 'C' warten (Geraet sendet es periodisch)
    t0 = time.time()
    while b"C" not in out[-40:] and time.time() - t0 < 30:
        out += ser.read(ser.in_waiting or 1)
    if b"C" not in out:
        log("FEHLER: kein XMODEM-'C' vom Geraet")
        return 2
    ser.reset_input_buffer()
    blk = 1
    pos = 0
    total = (len(data) + 1023) // 1024
    while pos < len(data):
        chunk = data[pos:pos + 1024].ljust(1024, bytes([SUB]))
        pkt = bytes([STX, blk & 0xFF, 0xFF - (blk & 0xFF)]) + chunk + crc16(chunk).to_bytes(2, "big")
        for attempt in range(10):
            ser.write(pkt)
            r = ser.read(1)
            while r == b"C":
                r = ser.read(1)
            if r and r[0] == ACK:
                break
            log(f"Block {blk}: Antwort {r!r}, Wiederholung {attempt + 1}")
            time.sleep(0.2)
        else:
            log("FEHLER: Block nicht quittiert, Abbruch")
            ser.write(bytes([CAN, CAN]))
            return 3
        if blk % 20 == 0 or blk == total:
            log(f"Block {blk}/{total}")
        blk += 1
        pos += 1024
    ser.write(bytes([EOT]))
    r = ser.read(1)
    log(f"EOT -> {r!r}")
    if not r or r[0] != ACK:
        ser.write(bytes([EOT]))
        r = ser.read(1)
        log(f"EOT 2 -> {r!r}")
    out = txt(read_for(ser, 60, b"?"))
    log("Pruefung: " + " | ".join(out.strip().splitlines()[-6:]))
    if "ok" not in out.lower() and "OK" not in out:
        log("WARNUNG: keine OK-Meldung, sende KEIN y")
        return 4
    ser.write(b"y")
    log("y gesendet, warte auf Neustart ...")
    out = txt(read_for(ser, 40, b"Hauptmen"))
    log("nach Neustart: " + " | ".join(out.strip().splitlines()[-5:]))
    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
