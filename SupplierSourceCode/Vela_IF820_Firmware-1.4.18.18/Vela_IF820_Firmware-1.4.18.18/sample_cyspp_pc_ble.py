#!/usr/bin/env python3

"""Test the IF820 CYSPP BLE data channel with the PC as the central.

This is intentionally separate from sample_spp_pc.py: CYSPP uses BLE GATT,
whereas SPP uses Bluetooth Classic RFCOMM.

Use --new-module to factory-reset and test a new IF820. Use
--configured-module to test an already configured CYSPP peripheral without
changing its stored configuration.
"""

import argparse
import asyncio
import logging
import sys
import threading
import time
from pathlib import Path
import tkinter as tk

sys.path.append(str(Path(__file__).resolve().parent / 'common_lib' / 'libraries'))

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.backends.device import BLEDevice
from bleak.backends.winrt.util import uninitialize_sta
from If820Board import If820Board

CYSPP_DATA = b'abcdefghijklmnop'
OTA_LATENCY = 1.0

if820_mac: str | None = None
cyspp_ready = threading.Event()
pc_ready = threading.Event()
received_from_if820: list[bytes] = []
received_from_pc = b''
state_lock = threading.Lock()


def _mac_to_string(address) -> str:
    if isinstance(address, (list, bytes, bytearray)):
        return ':'.join(f'{byte:02X}' for byte in reversed(address))
    return str(address).upper()


def _check_response(request: str, response) -> None:
    If820Board.check_if820_response(request, response)


def _configure_if820_cyspp(board: If820Board, reset_module: bool) -> None:
    global if820_mac, received_from_pc

    try:
        if reset_module:
            logging.info('[IF820] Factory reset: starting CYSPP BLE peripheral...')
            board.p_uart.clear_rx_queue()
            board.p_uart.send(b'/RFAC\r\n')

            command = board.p_uart.EVENT_SYSTEM_BOOT
            response = board.p_uart.wait_event(command)
            _check_response(command, response)

            command = board.p_uart.EVENT_GAP_ADV_STATE_CHANGED
            response = board.p_uart.wait_event(command, rxtimeout=15)
            _check_response(command, response)
        else:
            logging.info('[IF820] Using existing CYSPP configuration without reset.')

        command = board.p_uart.CMD_GET_BT_ADDR
        response = board.p_uart.send_and_wait(command)
        _check_response(command, response)
        if820_mac = _mac_to_string(response[1].payload.address)
        logging.info('[IF820] CYSPP advertising as MAC %s.', if820_mac)

        command = board.p_uart.EVENT_GAP_CONNECTED
        response = board.p_uart.wait_event(command, rxtimeout=60)
        _check_response(command, response)
        logging.info('[IF820] BLE connected.')

        # The GATT CCC write is emitted when the PC subscribes to notifications.
        command = board.p_uart.EVENT_GATTS_DATA_WRITTEN
        response = board.p_uart.wait_event(command, rxtimeout=30)
        _check_response(command, response)
        cyspp_ready.set()

        pc_ready.wait(timeout=15)
        board.p_uart.send(CYSPP_DATA)
        logging.info('[IF820] Sent: %r', CYSPP_DATA)
        time.sleep(OTA_LATENCY)

        board.p_uart.clear_rx_queue()
        time.sleep(OTA_LATENCY)
        received = board.p_uart.read()
        if received:
            with state_lock:
                received_from_pc = bytes(received)
            logging.info('[IF820] Received: %r', received_from_pc)
        else:
            logging.warning('[IF820] No CYSPP data received from PC.')
    except Exception as error:
        logging.error('[IF820] CYSPP setup failed: %s', error)
        cyspp_ready.set()


def _find_cyspp_characteristic(client: BleakClient) -> BleakGATTCharacteristic | None:
    candidates = []
    for service in client.services:
        for characteristic in service.characteristics:
            properties = set(characteristic.properties)
            if {'notify', 'write-without-response'} <= properties:
                candidates.append(characteristic)

    if not candidates:
        return None
    return min(candidates, key=lambda characteristic: characteristic.handle)


async def _find_if820() -> BLEDevice:
    deadline = time.time() + 45
    while time.time() < deadline:
        for device in await BleakScanner.discover(timeout=5.0):
            if device.address.upper() == if820_mac:
                return device
        logging.info('[PC] IF820 not found yet; scanning again...')
    raise TimeoutError(f'IF820 {if820_mac} was not discovered.')


async def run_test(board: If820Board, reset_module: bool) -> bool:
    threading.Thread(
        target=_configure_if820_cyspp,
        args=(board, reset_module),
        daemon=True).start()

    deadline = time.time() + 30
    while if820_mac is None and time.time() < deadline:
        await asyncio.sleep(0.2)
    if if820_mac is None:
        logging.error('[PC] Timed out while waiting for the IF820 address.')
        return False

    logging.info('[PC] Looking for IF820 at %s...', if820_mac)
    device = await _find_if820()
    logging.info('[PC] Found %s as %r.', device.address, device.name)

    def notification_handler(_: BleakGATTCharacteristic, data: bytearray) -> None:
        received = bytes(data)
        with state_lock:
            received_from_if820.append(received)
        logging.info('[PC] Received: %r', received)

    async with BleakClient(device) as client:
        characteristic = _find_cyspp_characteristic(client)
        if characteristic is None:
            logging.error('[PC] CYSPP characteristic was not discovered.')
            return False

        logging.info('[PC] CYSPP characteristic: %s (handle %s).',
                     characteristic.uuid, characteristic.handle)
        await client.start_notify(characteristic, notification_handler)
        await asyncio.to_thread(cyspp_ready.wait, 15)
        pc_ready.set()

        await asyncio.sleep(OTA_LATENCY + 0.5)
        logging.info('[PC] Sending: %r', CYSPP_DATA)
        await client.write_gatt_char(characteristic, CYSPP_DATA, response=False)
        await asyncio.sleep(OTA_LATENCY + 0.5)
        await client.stop_notify(characteristic)

    with state_lock:
        if820_to_pc_ok = any(CYSPP_DATA in data for data in received_from_if820)
        pc_to_if820_ok = CYSPP_DATA in received_from_pc

    print(f'CYSPP IF820 -> PC: {"PASSED" if if820_to_pc_ok else "FAILED"}')
    print(f'CYSPP PC -> IF820: {"PASSED" if pc_to_if820_ok else "FAILED"}')
    return if820_to_pc_ok and pc_to_if820_ok


def ask_test_mode() -> bool | None:
    """Return whether to reset the module, or None when the user cancels."""
    reset_module: list[bool | None] = [None]
    window = tk.Tk()
    window.title('IF820 CYSPP BLE Test')
    window.resizable(False, False)
    window.attributes('-topmost', True)

    frame = tk.Frame(window, padx=20, pady=20)
    frame.pack()
    tk.Label(frame, text='Welches IF820 soll getestet werden?',
             font=('Segoe UI', 12, 'bold')).pack(pady=(0, 15))

    def choose_new_module() -> None:
        reset_module[0] = True
        window.destroy()

    def choose_configured_module() -> None:
        reset_module[0] = False
        window.destroy()

    tk.Button(frame, text='Neues Modul\n(Factory Reset und CYSPP-Test)',
              command=choose_new_module, width=43, height=3,
              font=('Segoe UI', 10)).pack(pady=5)
    tk.Button(frame, text='Bereits eingerichtetes Modul\n(kein Factory Reset)',
              command=choose_configured_module, width=43, height=3,
              font=('Segoe UI', 10)).pack(pady=5)
    window.protocol('WM_DELETE_WINDOW', window.destroy)
    window.mainloop()
    return reset_module[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d', '--debug', action='store_true')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        '--new-module',
        action='store_true',
        help='Factory-reset the IF820, then test its default CYSPP peripheral.')
    mode.add_argument(
        '--configured-module',
        action='store_true',
        help='Test the current CYSPP configuration without a factory reset.')
    args = parser.parse_args()
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.DEBUG if args.debug else logging.INFO)

    reset_module = args.new_module
    if not args.new_module and not args.configured_module:
        reset_module = ask_test_mode()
        if reset_module is None:
            logging.info('CYSPP-Test abgebrochen.')
            return 0

    uninitialize_sta()

    boards = If820Board.get_connected_boards()
    if not boards:
        logging.critical('No IF820 board found.')
        return 1

    board = boards[0]
    board.open_and_init_board(False)
    try:
        return 0 if asyncio.run(run_test(board, reset_module)) else 1
    finally:
        board.close_ports_and_reset()


if __name__ == '__main__':
    raise SystemExit(main())