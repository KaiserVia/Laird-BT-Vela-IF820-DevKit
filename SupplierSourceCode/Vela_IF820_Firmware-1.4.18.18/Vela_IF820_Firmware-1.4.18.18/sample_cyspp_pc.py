#!/usr/bin/env python3

"""CYSPP Sample with PC-side Central (single-board variant).

ZWECK / PURPOSE
---------------
Dieses Sample ist eine Abwandlung von sample_cyspp_IF820-IF820.py und testet
das CYpressSerialPortProfile (CYSPP) des IF820 DVK, ohne dass ein zweites
IF820-Board oder ein BT900 benötigt wird.
Der PC übernimmt die Rolle des CYSPP-Centrals über den eingebauten
Bluetooth-Adapter und die Python-Bibliothek bleak.

WAS WIRD GETESTET? / WHAT IS TESTED?
--------------------------------------
1. CYSPP Peripheral auf dem IF820:
   - Das IF820 startet nach Factory Reset im CYSPP-Peripheral-Modus.
   - Es wartet auf eine BLE-Verbindung und den CYSPP-Handshake.
   - Danach sendet es Test-Daten via CYSPP-Notify zum Central (PC).
   - Empfangene Daten vom Central werden ausgegeben.

2. CYSPP Central am PC (bleak):
   - Der PC verbindet sich via BleakClient.
   - Per GATT-Discovery wird die CYSPP-Characteristic automatisch gefunden:
       Kennzeichen: properties = notify + write-without-response
   - Der PC abonniert Notifications (Daten vom IF820 → PC).
   - Der PC sendet Test-Daten zum IF820 per Write-Without-Response.

CYSPP GATT-PROFIL (auto-discovered, kein hardcodierter UUID nötig)
-------------------------------------------------------------------
Die CYSPP-Characteristic hat folgende GATT-Properties:
  - notify           : Daten vom Peripheral (IF820) zum Central (PC)
  - write-without-response: Daten vom Central (PC) zum Peripheral (IF820)
Das Script findet diese Characteristic automatisch per Property-Matching.
Falls mehrere Candidates gefunden werden, wird die mit der niedrigsten
Handle-Nummer verwendet (entspricht dem Verhalten des BT900: handle 17).

DATENFLUSS / DATA FLOW
-----------------------
  PC (bleak)                              IF820 (CYSPP Peripheral)
  ─────────                               ──────────────────────────
  write-without-response ─────────────▶  empfangen auf UART
  start_notify           ─────────────▶  CYSPP Notify aktiviert
                         ◀─────────────  IF820 UART-Daten → CYSPP Notify

ARCHITEKTUR / ARCHITECTURE
---------------------------
  PC (Hauptthread: asyncio/bleak)
  │
  ├── BleakScanner  ──── findet IF820 per MAC-Adresse
  ├── BleakClient   ──── verbindet, GATT-Discovery, Notify-Subscribe
  │       └── notification_handler() ── gibt empfangene Daten aus
  │
  └── Background-Thread (IF820 via USB/UART)
          └── EZ-Serial Kommandos:
                CMD_FACTORY_RESET          ← CYSPP Peripheral-Modus
                EVENT_P_CYSPP_STATUS       ← warte auf CYSPP connected (0x05)
                p_uart.send(CYSPP_DATA)    ← sende Testdaten über UART/CYSPP

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
  [IF820] Factory reset — CYSPP Peripheral mode starting...
  [IF820] BLE MAC: EE:72:DD:E4:86:13
  [PC]    Scanning for IF820 (EE:72:DD:E4:86:13)...
  [PC]    Found! Connecting...
  [PC]    Connected. Discovering GATT services...
  [PC]    CYSPP Characteristic found: UUID=00035b03-... handle=17
  [PC]    Subscribed to CYSPP notifications.
  [IF820] CYSPP connected (status=0x05)
  [IF820] Sending: 'abcdefghijklmnop'
  [PC]    Received from IF820: b'abcdefghijklmnop'
  [PC]    Sending to IF820:    b'abcdefghijklmnop'
  [IF820] Received from PC:    b'abcdefghijklmnop'
  Done.

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
from bleak import BleakScanner, BleakClient
from bleak.backends.characteristic import BleakGATTCharacteristic

CYSPP_DATA = b'abcdefghijklmnop'
OTA_LATENCY = 1.0             # seconds to wait for OTA data delivery

# Shared state
if820_mac: str = None       # filled as 'AA:BB:CC:DD:EE:FF' string
cyspp_ready = threading.Event()      # IF820 CYSPP connection established
pc_connected = threading.Event()     # PC bleak client connected
received_from_if820: list = []       # notifications received on PC side
received_from_pc: bytes = b''        # data received by IF820 from PC
_lock = threading.Lock()


def quit_on_resp_err(resp: int):
    if resp != 0:
        sys.exit(f'Response err: {hex(resp)}')


def _addr_to_mac_str(addr) -> str:
    """Convert IF820 address (list of bytes, LSB first) to 'AA:BB:CC:DD:EE:FF'.
    If addr is already a string it is returned as-is (uppercased).
    """
    if isinstance(addr, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(addr))
    return str(addr).upper()


def if820_peripheral_thread(board: If820Board):
    """Factory-resets the IF820 (enters CYSPP peripheral mode) and
    exchanges test data once CYSPP is connected.

    CYSPP mode is the factory default — no explicit GATT setup needed.
    The IF820 firmware handles everything internally.
    """
    global received_from_pc
    try:
        logging.info('[IF820] Factory reset — CYSPP Peripheral mode starting...')
        ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_FACTORY_RESET)
        If820Board.check_if820_response(board.p_uart.CMD_FACTORY_RESET, ez_rsp)

        logging.info('[IF820] Waiting for reboot...')
        ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_SYSTEM_BOOT)
        If820Board.check_if820_response(board.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)

        # After factory reset the IF820 starts CYSPP advertising automatically
        ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_GAP_ADV_STATE_CHANGED)
        If820Board.check_if820_response(
            board.p_uart.EVENT_GAP_ADV_STATE_CHANGED, ez_rsp)

        # Read BT address (used by PC to find the device)
        ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_GET_BT_ADDR)
        If820Board.check_if820_response(board.p_uart.CMD_GET_BT_ADDR, ez_rsp)
        bt_addr = ez_rsp[1].payload.address
        global if820_mac
        if820_mac = _addr_to_mac_str(bt_addr)
        logging.info(f'[IF820] BLE MAC: {if820_mac} — CYSPP advertising started.')

        # Wait for BLE connection from PC
        logging.info('[IF820] Waiting for BLE connection from PC...')
        ez_rsp = board.p_uart.wait_event(
            board.p_uart.EVENT_GAP_CONNECTED, rxtimeout=60)
        If820Board.check_if820_response(board.p_uart.EVENT_GAP_CONNECTED, ez_rsp)
        logging.info('[IF820] BLE connected.')

        # Wait for connection parameters update (standard BLE negotiation step)
        logging.info('[IF820] Waiting for connection update...')
        board.p_uart.wait_event(
            board.p_uart.EVENT_GAP_CONNECTION_UPDATED, rxtimeout=10)

        # Wait for the notification-enable CCC write from bleak (start_notify)
        # This matches the BT900 sample pattern: after CCC is written, CYSPP
        # data mode is active and the UART forwards data bidirectionally.
        logging.info('[IF820] Waiting for GATT CCC write (notification enable)...')
        ez_rsp = board.p_uart.wait_event(
            board.p_uart.EVENT_GATTS_DATA_WRITTEN, rxtimeout=30)
        logging.info('[IF820] CCC written — CYSPP data mode active.')

        cyspp_ready.set()

        # Wait until PC is ready to receive
        pc_connected.wait(timeout=15)
        time.sleep(0.3)

        # Send test data via UART → CYSPP notify to PC
        logging.info(f'[IF820] Sending: {CYSPP_DATA}')
        board.p_uart.send(CYSPP_DATA)
        time.sleep(OTA_LATENCY)

        # Clear buffered EZ-Serial events (connection update, CCC write, CYSPP status)
        # before reading incoming CYSPP data, matching the BT900 sample pattern.
        board.p_uart.clear_rx_queue()

        # Read data sent from PC → arrives on IF820 UART via CYSPP
        time.sleep(OTA_LATENCY)
        rx = board.p_uart.read()
        if rx:
            with _lock:
                received_from_pc = bytes(rx)
            logging.info(f'[IF820] Received from PC: {bytes(rx)}')
        else:
            logging.warning('[IF820] No data received from PC.')

    except Exception as e:
        logging.error(f'[IF820] Fatal error: {e}')
        cyspp_ready.set()   # unblock main thread on error


def _find_cyspp_characteristic(client: BleakClient) -> BleakGATTCharacteristic | None:
    """Auto-discover the CYSPP data characteristic.

    CYSPP data characteristic = has BOTH 'notify' AND 'write-without-response'.
    If multiple candidates exist, the one with the lowest handle number is chosen
    (matches BT900 behavior: handle 17).
    """
    candidates = []
    for service in client.services:
        for char in service.characteristics:
            props = set(char.properties)
            if 'notify' in props and 'write-without-response' in props:
                candidates.append(char)
                logging.debug(f'[PC]    CYSPP candidate: UUID={char.uuid} '
                               f'handle={char.handle} props={char.properties}')

    if not candidates:
        return None
    # Lowest handle = data characteristic (RX flow control has higher handle)
    candidates.sort(key=lambda c: c.handle)
    return candidates[0]


async def main(if820_board_p: If820Board):
    # Start IF820 peripheral thread
    t = threading.Thread(target=if820_peripheral_thread,
                         args=(if820_board_p,), daemon=True)
    t.start()

    # Wait for IF820 MAC to be populated (set by peripheral thread after boot)
    logging.info('[PC]    Waiting for IF820 MAC address...')
    deadline = time.time() + 30
    while if820_mac is None and time.time() < deadline:
        await asyncio.sleep(0.2)
    if if820_mac is None:
        logging.error('[PC]    Timeout waiting for IF820 MAC. Aborting.')
        return

    logging.info(f'[PC]    Scanning for IF820 ({if820_mac})...')
    target_device = None
    while target_device is None:
        devices = await BleakScanner.discover(timeout=5.0)
        for d in devices:
            if d.address.upper() == if820_mac.upper():
                target_device = d
                break
        if target_device is None:
            logging.info('[PC]    Device not found yet, retrying...')

    logging.info(f'[PC]    Found! ({target_device.name}) — Connecting...')

    def notification_handler(characteristic: BleakGATTCharacteristic,
                              data: bytearray):
        with _lock:
            received_from_if820.append(bytes(data))
        print(f'[PC]    Received from IF820: {bytes(data)}', flush=True)

    async with BleakClient(target_device) as client:
        logging.info('[PC]    Connected. Discovering GATT services...')

        # Print all discovered services/characteristics for diagnostics
        for service in client.services:
            logging.debug(f'[PC]    Service: {service.uuid}')
            for char in service.characteristics:
                logging.debug(f'[PC]      Char: {char.uuid} '
                               f'handle={char.handle} props={char.properties}')

        # Find CYSPP characteristic by property-matching
        cyspp_char = _find_cyspp_characteristic(client)
        if cyspp_char is None:
            logging.error('[PC]    CYSPP characteristic (notify + '
                          'write-without-response) not found. '
                          'Run with -d to see all discovered services.')
            return

        logging.info(f'[PC]    CYSPP Characteristic found: '
                     f'UUID={cyspp_char.uuid}  handle={cyspp_char.handle}')

        # Subscribe to notifications (data from IF820 → PC)
        await client.start_notify(cyspp_char, notification_handler)
        logging.info('[PC]    Subscribed to CYSPP notifications.')

        # Wait for IF820 to confirm CYSPP is ready before exchanging data
        logging.info('[PC]    Waiting for IF820 CYSPP ready signal...')
        await asyncio.to_thread(cyspp_ready.wait, 15)

        # Signal IF820 thread that PC is ready to receive
        pc_connected.set()

        # Wait for IF820 to send its test data
        await asyncio.sleep(OTA_LATENCY + 0.5)

        # Send test data from PC → IF820 (write without response)
        logging.info(f'[PC]    Sending to IF820:    {CYSPP_DATA}')
        await client.write_gatt_char(cyspp_char, CYSPP_DATA,
                                     response=False)

        # Give IF820 time to receive and print
        await asyncio.sleep(OTA_LATENCY + 0.5)

        await client.stop_notify(cyspp_char)

    # Results
    print()
    with _lock:
        rx_pc = list(received_from_if820)
        rx_if820 = received_from_pc

    ok_if820_to_pc = any(CYSPP_DATA in d for d in rx_pc) if rx_pc else False
    ok_pc_to_if820 = CYSPP_DATA in rx_if820 if rx_if820 else False

    print(f'IF820 → PC : {"OK  " if ok_if820_to_pc else "FAIL"} '
          f'(received: {rx_pc})')
    print(f'PC → IF820 : {"OK  " if ok_pc_to_if820 else "FAIL"} '
          f'(received: {rx_if820!r})')
    if ok_if820_to_pc and ok_pc_to_if820:
        print('CYSPP data exchange: PASSED')
    else:
        print('CYSPP data exchange: FAILED — check logs with -d')


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

    logging.info('Init IF820 board...')
    if820_board_p = boards[0]
    if820_board_p.open_and_init_board(False)
    if820_board_p.close_ports_and_reset()
    time.sleep(3)
    if820_board_p.open_and_init_board()

    try:
        asyncio.run(main(if820_board_p))
    except KeyboardInterrupt:
        logging.info('Stopped.')
