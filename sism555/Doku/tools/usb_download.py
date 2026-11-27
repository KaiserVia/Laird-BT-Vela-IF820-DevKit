#!/usr/bin/env python3
"""usb_download.py - Ymodem-Datendownload ueber den USB-COM-Port (CDC) statt ueber BT Classic.

Duenne Huelle um ble_mainboard_test.py (--mode download, Transport "classic" = beliebiger COM-Port): die
BT-Geraetesuche wird uebersprungen, der angegebene USB-Port direkt benutzt. Alle anderen Optionen werden
durchgereicht.

  python usb_download.py --port COM13 --records 20000
  python usb_download.py --auto --records 20000          # Port per VID 0x1FC9 suchen

Achtung: der Download-Modus loescht die Messdaten im Geraet und erzeugt Testmesswerte (wie bei BT).
"""
import os, sys
import serial.tools.list_ports as lp

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ble_mainboard_test as b  # noqa: E402


async def _make_transport(args):
    args.classic_device_name = "USB"
    args.classic_port = args.port
    return b.ClassicTransport(args)


def main():
    argv = sys.argv[1:]
    if "--auto" in argv:
        argv.remove("--auto")
        ports = [p.device for p in lp.comports() if p.vid == 0x1FC9]
        if not ports:
            sys.exit("kein USB-COM-Port (VID 0x1FC9) gefunden")
        argv += ["--port", ports[0]]
    if "--mode" not in argv:
        argv += ["--mode", "download"]
    b.make_transport = _make_transport
    sys.argv = [sys.argv[0], "--transport", "classic"] + argv
    return b.main()


if __name__ == "__main__":
    sys.exit(main())
