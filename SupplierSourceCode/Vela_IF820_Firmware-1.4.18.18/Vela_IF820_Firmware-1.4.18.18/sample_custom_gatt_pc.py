#!/usr/bin/env python3

"""BLE Custom GATT Sample with PC-side GATT Client (single-board variant).

ZWECK / PURPOSE
---------------
Dieses Sample ist eine Abwandlung von sample_custom_gatt.py und testet den
BT SIG GATT Battery Service des IF820 DVK, ohne dass ein zweites IF820-Board
benötigt wird. Der PC übernimmt die Rolle des GATT-Clients (Central) über den
eingebauten Bluetooth-Adapter und die Python-Bibliothek bleak.

WAS WIRD GETESTET? / WHAT IS TESTED?
--------------------------------------
1. BLE GATT Peripheral auf dem IF820:
   - Das IF820 richtet den Standard-BT-SIG Battery Service (UUID 0x180F) ein.
   - Es sendet ein Advertisement-Payload mit Local Name "battery".
   - Alle 5 Sekunden wird der Batterielevel um 1 dekrementiert (100 → 0 → 100).
   - Jede Änderung wird als GATT-Notification an den verbundenen Client gesendet.

2. BLE GATT Client am PC:
   - Der PC scannt über bleak nach dem Gerät mit dem Local Name "battery".
   - Der PC verbindet sich und entdeckt den Battery Service automatisch per UUID.
   - Der PC abonniert Notifications auf der Battery Level Characteristic (0x2A19).
   - Jede empfangene Notification wird mit dem Batterielevel ausgegeben.

WARUM BRAUCHT MAN DAS? / WHY IS THIS USEFUL?
---------------------------------------------
- Verifiziert, dass der GATT-Stack des IF820 korrekt konfiguriert und erreichbar ist.
- Zeigt, wie ein Standard-BLE-Sensor (z.B. Batterie-Monitor) mit einem PC interagiert.
- Ermöglicht Tests ohne zweites IF820-Board — ideal für Einzel-Board-Setups.
- Dient als Vorlage für eigene GATT-Services (Custom UUIDs, mehrere Characteristics).

ARCHITEKTUR / ARCHITECTURE
---------------------------
  PC (Hauptthread: asyncio/bleak)
  │
  ├── BleakScanner  ──── findet IF820 per Local Name "battery"
  ├── BleakClient   ──── verbindet sich, entdeckt Battery Service
  │       └── start_notify(Battery Level) ── empfängt Batterie-Updates
  │
  └── Background-Thread (IF820 via USB/UART)
          └── EZ-Serial Kommandos:
                CMD_GATTS_CREATE_ATTR      ← Battery Service aufbauen
                CMD_GAP_START_ADV          ← Advertising starten
                CMD_GATTS_WRITE_HANDLE     ← Batterie-Wert setzen
                CMD_GATTS_NOTIFY_HANDLE    ← Notification senden

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
  [IF820 Peripheral] Battery Service configured. Advertising started.
  [PC-BT Central]    Scanning for 'battery'...
  [PC-BT Central]    Found: EE:72:DD:E4:86:13 — Connecting...
  [PC-BT Central]    Connected! Battery Service discovered.
  [PC-BT Central]    Subscribed to Battery Level notifications. Ctrl+C to stop.
  [IF820 Peripheral] Changed battery level: 99
  [PC-BT Central]    Battery Level Notification: 99 %
  [IF820 Peripheral] Changed battery level: 98
  [PC-BT Central]    Battery Level Notification: 98 %
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
from bleak import BleakScanner, BleakClient

# BT SIG Battery Service and Battery Level Characteristic UUIDs
BATTERY_SERVICE_UUID = "0000180f-0000-1000-8000-00805f9b34fb"
BATTERY_LEVEL_UUID = "00002a19-0000-1000-8000-00805f9b34fb"

# Advertisement payload — matches original sample_custom_gatt.py
# [Flags] [Local Name: "battery"] [16-bit Service UUID: 0x180F]
ADV_DATA = [0x02, 0x01, 0x06,
            0x08, 0x08, 0x62, 0x61, 0x74, 0x74, 0x65, 0x72, 0x79,
            0x03, 0x02, 0x0f, 0x18]
ADV_LOCAL_NAME = "battery"

ADV_MODE = ez_port.GapAdvertMode.NA.value
ADV_TYPE = ez_port.GapAdvertType.UNDIRECTED_LOW_DUTY_CYCLE.value
ADV_INTERVAL = 0x40
ADV_CHANNELS = ez_port.GapAdvertChannels.CHANNEL_ALL.value
ADV_TIMEOUT = 0
ADV_FLAGS = ez_port.GapAdvertFlags.ALL.value

DATA_UPDATE_INTERVAL_SECONDS = 5

battery_level = 100
battery_level_handle = 0

# Set when IF820 is advertising and ready for a BLE connection
if820_ready = threading.Event()


def quit_on_resp_err(resp: int):
    if resp != 0:
        sys.exit(f'Response err: {hex(resp)}')


def board_wait_awake(board: If820Board):
    pin = 0
    while not pin:
        pin = board.probe.gpio_read(board.BT_HOST_WAKE)
        if not pin:
            time.sleep(0.01)
        else:
            break
    return pin


def board_allow_sleep(board: If820Board):
    board.probe.gpio_to_output_low(board.LP_MODE)


def board_wake_up(board: If820Board):
    board.probe.gpio_to_output_high(board.LP_MODE)


def if820_peripheral_thread(board: If820Board, low_power: bool):
    """Configures IF820 as BLE GATT Peripheral with Battery Service.

    Runs in a background thread so asyncio/bleak can own the main thread
    (required by WinRT on Windows).
    """
    global battery_level, battery_level_handle

    try:
        if low_power:
            logging.info('[IF820 Peripheral] Setting up low-power mode...')
            board.probe.gpio_to_output(board.LP_MODE)
            board_wake_up(board)
            board.probe.gpio_to_input(board.BT_HOST_WAKE)
            board_wait_awake(board)
            ez_rsp = board.p_uart.send_and_wait(
                board.p_uart.CMD_SET_PARAMS,
                apiformat=ez_port.EzSerialApiMode.TEXT.value,
                link_super_time_out=0x7d00,
                discoverable=0,
                connectable=0,
                flags=0,
                scn=0,
                active_bt_discoverability=0,
                active_bt_connectability=0)
            If820Board.check_if820_response(board.p_uart.CMD_SET_PARAMS, ez_rsp)

        logging.info('[IF820 Peripheral] Configuring advertising parameters...')
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

        # ── Setup Battery Service ────────────────────────────────────────
        logging.info('[IF820 Peripheral] Creating Battery Service GATT attributes...')

        # Service declaration (UUID 0x180F)
        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GATTS_CREATE_ATTR,
            type=ez_port.GattAttrType.STRUCTURE.value,
            perm=ez_port.GattAttrPermission.READ.value,
            length=4,
            data=bytearray.fromhex('00280F18'))[0])

        # Characteristic declaration (UUID 0x2A19, handle 0x12, props: notify+read)
        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GATTS_CREATE_ATTR,
            type=ez_port.GattAttrType.STRUCTURE.value,
            perm=ez_port.GattAttrPermission.READ.value,
            length=7,
            data=bytearray.fromhex('0328121800192A'))[0])

        # Characteristic value (Battery Level = 100)
        res = board.p_uart.send_and_wait(
            board.p_uart.CMD_GATTS_CREATE_ATTR,
            type=ez_port.GattAttrType.VALUE.value,
            perm=ez_port.GattAttrPermission.READ.value | ez_port.GattAttrPermission.AUTH_WRITE.value,
            length=1,
            data=[battery_level])
        quit_on_resp_err(res[0])
        battery_level_handle = res[1].payload.handle

        # Client Characteristic Configuration (CCC) descriptor
        res = board.p_uart.send_and_wait(
            board.p_uart.CMD_GATTS_CREATE_ATTR,
            type=ez_port.GattAttrType.STRUCTURE.value,
            perm=ez_port.GattAttrPermission.READ.value | ez_port.GattAttrPermission.WRITE_ACK.value,
            length=4,
            data=bytearray.fromhex('02290000'))
        quit_on_resp_err(res[0])

        # Start advertising
        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GAP_START_ADV,
            mode=ADV_MODE,
            type=ADV_TYPE,
            channels=ADV_CHANNELS,
            high_interval=ADV_INTERVAL,
            high_duration=0,
            low_interval=ADV_INTERVAL,
            low_duration=0,
            flags=ADV_FLAGS,
            directAddr=[0, 0, 0, 0, 0, 0],
            directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])

        logging.info('[IF820 Peripheral] Battery Service configured. Advertising started.')
        if820_ready.set()

        logging.info('[IF820 Peripheral] Waiting for BLE connection from PC...')
        res = board.wait_for_ble_connection()
        logging.info(f'[IF820 Peripheral] Connected! [{res}]')
        con_handle = res.payload.conn_handle

        # ── Battery level update loop ────────────────────────────────────
        while True:
            time.sleep(DATA_UPDATE_INTERVAL_SECONDS)
            battery_level -= 1
            if battery_level < 0:
                battery_level = 100

            if low_power:
                board_wake_up(board)
                board_wait_awake(board)

            try:
                res = board.p_uart.send_and_wait(
                    board.p_uart.CMD_GATTS_WRITE_HANDLE,
                    attr_handle=battery_level_handle,
                    data=[battery_level])
                if res[0] == 0:
                    logging.info(f'[IF820 Peripheral] Changed battery level: {battery_level}')
                else:
                    logging.error(f'[IF820 Peripheral] Failed to write battery level [{hex(res[0])}]')
                    if low_power:
                        board_allow_sleep(board)
                    continue

                # Send GATT notification to connected client
                res = board.p_uart.send_and_wait(
                    board.p_uart.CMD_GATTS_NOTIFY_HANDLE,
                    conn_handle=con_handle,
                    attr_handle=battery_level_handle,
                    data=[battery_level])
                if res[0] != 0:
                    logging.error(f'[IF820 Peripheral] Failed to notify battery level [{hex(res[0])}]')

                if low_power:
                    board_allow_sleep(board)
            except Exception as e:
                logging.error(f'[IF820 Peripheral] Error in update loop: {e}')

    except Exception as e:
        logging.error(f'[IF820 Peripheral] Fatal error: {e}')


async def main(if820_board_p: If820Board, low_power: bool):
    # Start IF820 peripheral setup in background thread
    t = threading.Thread(
        target=if820_peripheral_thread,
        args=(if820_board_p, low_power),
        daemon=True)
    t.start()

    # Wait until IF820 is advertising before scanning
    logging.info('[PC-BT Central] Waiting for IF820 to finish setup...')
    await asyncio.get_event_loop().run_in_executor(None, if820_ready.wait)

    logging.info(f'[PC-BT Central] Scanning for "{ADV_LOCAL_NAME}"...')
    target_device = None
    while target_device is None:
        target_device = await BleakScanner.find_device_by_name(
            ADV_LOCAL_NAME, timeout=10.0)
        if target_device is None:
            logging.info('[PC-BT Central] Device not found yet, retrying...')

    logging.info(f'[PC-BT Central] Found: {target_device.address} ({target_device.name}) — Connecting...')

    def battery_notification_handler(characteristic, data: bytearray):
        level = data[0]
        print(f'[PC-BT Central]    Battery Level Notification: {level} %', flush=True)

    async with BleakClient(target_device) as client:
        logging.info(f'[PC-BT Central] Connected! MTU: {client.mtu_size}')

        # Get Battery Level Characteristic directly by UUID (searches all services)
        battery_char = client.services.get_characteristic(BATTERY_LEVEL_UUID)
        if battery_char is None:
            logging.error('[PC-BT Central] Battery Level Characteristic (0x2A19) not found!')
            return

        logging.info(f'[PC-BT Central] Battery Service discovered. '
                     f'Characteristic handle: {battery_char.handle}')

        # Read the initial value
        val = await client.read_gatt_char(battery_char)
        logging.info(f'[PC-BT Central] Initial Battery Level: {val[0]} %')

        # Subscribe to notifications
        await client.start_notify(battery_char, battery_notification_handler)
        print('[PC-BT Central] Subscribed to Battery Level notifications. Ctrl+C to stop.',
              flush=True)

        # Keep running until interrupted
        while True:
            await asyncio.sleep(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    parser.add_argument('-l', '--low-power', action='store_true',
                        help="Enable low power mode for the peripheral")
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    low_power = args.low_power

    boards = If820Board.get_connected_boards()
    if len(boards) < 1:
        logging.critical("No IF820 board found.")
        sys.exit(1)

    logging.info('Init IF820 board (Peripheral / GATT Server)...')
    if820_board_p = boards[0]
    if820_board_p.open_and_init_board(False)
    if820_board_p.close_ports_and_reset()
    time.sleep(3)
    boot_info = if820_board_p.open_and_init_board()
    logging.info(f'IF820 boot: {boot_info}')
    mac = boot_info.payload.address
    logging.info(f'IF820 MAC: {mac}')

    # Stop any running advertising before we configure ours
    if820_board_p.stop_advertising()

    try:
        # bleak (WinRT) requires asyncio on the main thread on Windows
        asyncio.run(main(if820_board_p, low_power))
    except KeyboardInterrupt:
        logging.info('Stopped.')
