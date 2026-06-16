#!/usr/bin/env python3

"""BLE Advertiser Sample with PC-side Scanner (single-board variant).

ZWECK / PURPOSE
---------------
Dieses Sample ist eine Abwandlung von sample_advertiser.py und testet die
BLE-Advertising-Funktionalität des IF820 DVK, ohne dass ein zweites IF820-Board
benötigt wird. Der PC übernimmt die Rolle des "Central" (Scanner) über den
eingebauten Bluetooth-Adapter und die Python-Bibliothek bleak.

WAS WIRD GETESTET? / WHAT IS TESTED?
--------------------------------------
1. BLE Advertising des IF820:
   - Das IF820 konfiguriert BLE-Advertisement-Parameter (Intervall, Typ, Kanäle).
   - Es sendet ein benutzerdefiniertes Advertisement-Payload mit:
       * Local Name: "my_sensor"
       * Manufacturer Specific Data (Company ID 0x0077) mit einem 1-Byte-Zähler.
   - Der Zähler wird alle 2 Sekunden inkrementiert und per CMD_GAP_SET_ADV_DATA
     live aktualisiert — ohne Advertising-Unterbrechung.

2. BLE-Empfang am PC (passive Scan):
   - Der PC scannt passiv über den Windows-BT-Stack (WinRT/bleak).
   - Jedes empfangene Advertisement des IF820 wird dekodiert und der
     Zählerwert + RSSI (Signalstärke in dBm) ausgegeben.
   - Kein Pairing, keine Verbindung notwendig — reiner Broadcast-Empfang.

WARUM BRAUCHT MAN DAS? / WHY IS THIS USEFUL?
---------------------------------------------
- Verifiziert, dass das IF820 korrekt advertisiert und vom BT-Stack empfangen wird.
- Ermöglicht einfache RSSI-Messung (Reichweite, Antenne, Platzierung).
- Dient als Basis für IoT-Sensor-Szenarien: IF820 sendet Messwerte per
  Advertisement, ohne dass eine Verbindung aufgebaut werden muss (energiesparend).
- Kann mit dem -l Flag aus sample_advertiser.py erweitert werden, um den
  Low-Power-Modus des IF820 zu evaluieren.

ARCHITEKTUR / ARCHITECTURE
---------------------------
  PC (Hauptthread: asyncio/bleak)
  │
  ├── BleakScanner (Windows BT Adapter) ──────── empfängt Advertisements
  │       └── detection_callback()               gibt Counter + RSSI aus
  │
  └── Background-Thread (IF820 via USB/UART)
          └── EZ-Serial Kommandos:
                CMD_GAP_SET_ADV_PARAMETERS
                CMD_GAP_SET_ADV_DATA         ← Counter-Update alle 2s
                CMD_GAP_START_ADV

VORAUSSETZUNGEN / REQUIREMENTS
--------------------------------
Hardware:
  - 1x IF820 DVK Board, per USB am PC angeschlossen
  - PC mit Bluetooth 4.0+ Adapter (BLE-fähig)
  - Bluetooth am PC eingeschaltet (Windows Einstellungen > Bluetooth & Geräte)

Software:
  - pip install bleak

AUSGABE / EXPECTED OUTPUT
--------------------------
  [IF820 Advertiser] Advertising started. Counter increments every 2s.
  [PC-BT Scanner]    addr=EE:72:DD:E4:86:13  counter=1  RSSI=-50 dBm
  [IF820 Advertiser] Sending counter=2
  [PC-BT Scanner]    addr=EE:72:DD:E4:86:13  counter=2  RSSI=-48 dBm
  ...

Beenden mit Ctrl+C.
"""

import argparse
import asyncio
import logging
import time
import threading
import sys
sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board
from bleak import BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

ADV_MODE = ez_port.GapAdvertMode.NA.value
ADV_TYPE = ez_port.GapAdvertType.UNDIRECTED_HIGH_DUTY_CYCLE.value
ADV_INTERVAL = 400          # 400 * 0.625ms = 250ms
ADV_UPDATE_INTERVAL_SECONDS = 2
ADV_CHANNELS = ez_port.GapAdvertChannels.CHANNEL_ALL.value
ADV_TIMEOUT = 0
ADV_FLAGS = ez_port.GapAdvertFlags.ALL.value

# Advertisement payload (same as original sample_advertiser.py):
#   [Flags] [Local Name: "my_sensor"] [Manufacturer Specific: company=0x0077, data=[0x00, counter]]
ADV_DATA = [0x02, 0x01, 0x06,
            0x0a, 0x08, 0x6d, 0x79, 0x5f, 0x73, 0x65, 0x6e, 0x73, 0x6f, 0x72,
            0x04, 0xff, 0x77, 0x00, 0x01]

# Manufacturer Specific company ID embedded in ADV_DATA (0x0077 little-endian = 119 decimal)
MANUFACTURER_ID = 0x0077
# Local name embedded in ADV_DATA
ADV_LOCAL_NAME = "my_sensor"

last_counter = -1


def quit_on_resp_err(resp: int):
    if resp != 0:
        sys.exit(f'Response err: {hex(resp)}')


def detection_callback(device: BLEDevice, advertisement_data: AdvertisementData):
    """Called by BleakScanner for every received BLE advertisement."""
    global last_counter

    # Identify the IF820 by local name or manufacturer ID
    name_match = (advertisement_data.local_name == ADV_LOCAL_NAME or
                  device.name == ADV_LOCAL_NAME)
    mfr_match = MANUFACTURER_ID in advertisement_data.manufacturer_data

    if not (name_match or mfr_match):
        return

    mfr_bytes = advertisement_data.manufacturer_data.get(MANUFACTURER_ID)
    if mfr_bytes is None or len(mfr_bytes) < 1:
        return

    counter = mfr_bytes[-1]
    rssi = advertisement_data.rssi if advertisement_data.rssi is not None else "?"
    if counter != last_counter:
        last_counter = counter
        print(f'[PC-BT Scanner] addr={device.address}  counter={counter}  RSSI={rssi} dBm', flush=True)


def if820_advertiser_thread(board: If820Board):
    """Runs the IF820 advertising loop in a background thread.
    Asyncio (bleak) owns the main thread on Windows."""
    try:
        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GAP_SET_ADV_PARAMETERS,
            mode=ADV_MODE,
            type=ADV_TYPE,
            channels=ADV_CHANNELS,
            high_interval=ADV_INTERVAL,
            high_duration=ADV_TIMEOUT,
            low_interval=ADV_INTERVAL,
            low_duration=ADV_TIMEOUT,
            flags=ADV_FLAGS,
            directAddr=[0, 0, 0, 0, 0, 0],
            directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])

        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GAP_SET_ADV_DATA,
            data=ADV_DATA)[0])

        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GAP_START_ADV,
            mode=ADV_MODE,
            type=ADV_TYPE,
            channels=ADV_CHANNELS,
            high_interval=ADV_INTERVAL,
            high_duration=ADV_TIMEOUT,
            low_interval=ADV_INTERVAL,
            low_duration=ADV_TIMEOUT,
            flags=ADV_FLAGS,
            directAddr=[0, 0, 0, 0, 0, 0],
            directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])

        logging.info('[IF820 Advertiser] Advertising started. Counter increments every 2s. Ctrl+C to stop.')

        while True:
            time.sleep(ADV_UPDATE_INTERVAL_SECONDS)
            counter = ADV_DATA[-1] + 1
            if counter >= 256:
                counter = 0
            ADV_DATA[-1] = counter
            logging.info(f'[IF820 Advertiser] Sending counter={counter}')
            quit_on_resp_err(board.p_uart.send_and_wait(
                board.p_uart.CMD_GAP_SET_ADV_DATA,
                data=ADV_DATA)[0])
    except Exception as e:
        logging.error(f'[IF820 Advertiser] {e}')


async def main(if820_board_p: If820Board):
    logging.info('[PC-BT Scanner] Starting BLE scan (Windows BT adapter)...')
    async with BleakScanner(detection_callback=detection_callback):
        # Start IF820 advertiser in background thread now that scanner is active
        t = threading.Thread(target=if820_advertiser_thread,
                             args=(if820_board_p,), daemon=True)
        t.start()
        # Run forever (Ctrl+C to stop)
        while True:
            await asyncio.sleep(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    boards = If820Board.get_connected_boards()
    if len(boards) < 1:
        logging.critical("No IF820 board found.")
        sys.exit(1)

    logging.info('Init IF820 board (Peripheral / Advertiser)...')
    if820_board_p = boards[0]
    if820_board_p.open_and_init_board(False)
    if820_board_p.close_ports_and_reset()
    time.sleep(3)
    boot_info = if820_board_p.open_and_init_board()
    logging.info(f'IF820 boot: {boot_info}')
    mac = boot_info.payload.address
    logging.info(f'IF820 MAC: {mac}')

    # Stop any running advertising before we configure ours
    quit_on_resp_err(
        if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_STOP_ADV)[0])

    try:
        # Run bleak scanner on main thread (required by WinRT on Windows)
        asyncio.run(main(if820_board_p))
    except KeyboardInterrupt:
        logging.info('Stopped.')
