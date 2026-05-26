#!/usr/bin/env python3

"""The custom GATT sample configures a peripheral device to advertise a custom
payload and sets up the BT SIG GATT Battery Service (https://www.bluetooth.com/specifications/specs/battery-service/). Every 5 seconds, the peripheral will
change the battery level.
A central device scans for the peripheral device and connects to it.
The central device will received the battery level every time it is updated.
To run this test you need two IF820 DVK boards.

Use the -l option to enable low power mode for the peripheral.
This can be used to evaluate the power consumption of an IF820 peripheral in a connection.
"""

import argparse
import logging
import time
import threading
import sys
sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board

ADV_MODE = ez_port.GapAdvertMode.NA.value
ADV_TYPE = ez_port.GapAdvertType.UNDIRECTED_LOW_DUTY_CYCLE.value
ADV_INTERVAL = 0x40
ADV_CHANNELS = ez_port.GapAdvertChannels.CHANNEL_ALL.value
ADV_TIMEOUT = 0
ADV_FLAGS = ez_port.GapAdvertFlags.ALL.value
ADV_DATA = [0x02, 0x01, 0x06,
            0x08, 0x08, 0x62, 0x61, 0x74, 0x74, 0x65, 0x72, 0x79,
            0x03, 0x02, 0x0f, 0x18]
SCAN_MODE_GENERAL_DISCOVERY = ez_port.GapScanMode.NA.value
SCAN_FILTER_ACCEPT_ALL = ez_port.GapScanFilter.NA.value
SCAN_INTERVAL = 0x100
SCAN_WINDOW = 0x100
# The following 4 values can be adjusted to see how it affects the power consumption of the peripheral.
CONNECTION_INTERVAL = 400  # 400 * 1.25ms = 500ms
SLAVE_LATENCY = 3
SUPERVISION_TIMEOUT = 500  # 500 * 10ms = 5s
DATA_UPDATE_INTERVAL_SECONDS = 5
PERIPHERAL_ADDRESS = None
battery_level = 100
battery_level_handle = 0
batter_level_ccc_handle = 0


def board_wait_awake(board: If820Board):
    """Wait for the board to be ready to accept commands.
    BT_HOST_WAKE is an output from the IF820 to the host.
    When the IF820 is awake and ready to accept commands, this pin will be high.

    Args:
        board (If820Board): board to wait for
    """
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


def quit_on_resp_err(resp: int):
    """Exit the program if the response code is not 0.

    Args:
        resp (int): response code
    """
    if resp != 0:
        sys.exit(f'Response err: {hex(resp)}')


def scanner_thread():
    """Thread to scan for the peripheral device and connect to it.
    """
    logging.debug('Central: stop advertising after boot')
    if820_board_c.stop_advertising()
    logging.debug('Central: start scanning...')
    quit_on_resp_err(if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GAP_START_SCAN,
                                                        mode=SCAN_MODE_GENERAL_DISCOVERY,
                                                        interval=SCAN_INTERVAL,
                                                        window=SCAN_WINDOW,
                                                        active=0,
                                                        filter=SCAN_FILTER_ACCEPT_ALL,
                                                        nodupe=1,
                                                        timeout=0)[0])
    while True:
        res = if820_board_c.p_uart.wait_event(
            if820_board_c.p_uart.EVENT_GAP_SCAN_RESULT)
        quit_on_resp_err(res[0])
        packet = res[1]
        logging.debug(f'Received {packet}')
        received_addr = packet.payload.address
        address_type = packet.payload.address_type
        if received_addr == PERIPHERAL_ADDRESS:
            logging.info('Found peripheral device!')
            break
        else:
            logging.debug(f'Not looking for {received_addr}')

    logging.debug('Stop scanning')
    quit_on_resp_err(if820_board_c.p_uart.send_and_wait(
        if820_board_c.p_uart.CMD_GAP_STOP_SCAN)[0])
    logging.debug('Wait for scan to stop...')
    quit_on_resp_err(if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_GAP_SCAN_STATE_CHANGED)[0])
    logging.info('Connecting...')
    quit_on_resp_err(if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GAP_CONNECT,
                                                        address=received_addr,
                                                        type=address_type,
                                                        interval=CONNECTION_INTERVAL,
                                                        slave_latency=SLAVE_LATENCY,
                                                        supervision_timeout=SUPERVISION_TIMEOUT,
                                                        scan_interval=SCAN_INTERVAL,
                                                        scan_window=SCAN_WINDOW,
                                                        scan_timeout=0)[0])

    logging.debug('Central: Wait for connection...')
    res = if820_board_c.wait_for_ble_connection()
    logging.info(f'Central: Connected! [{res}]')
    con_handle = res.payload.conn_handle

    # Enable notifications for battery level
    logging.debug(
        f'Enable notifications for battery level [{hex(batter_level_ccc_handle)}]')
    res = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GATTC_WRITE_HANDLE,
                                             conn_handle=con_handle,
                                             attr_handle=batter_level_ccc_handle,
                                             type=0,
                                             data=[0x01, 0x00])
    quit_on_resp_err(res[0])

    while True:
        try:
            res = if820_board_c.p_uart.wait_event(
                if820_board_c.p_uart.EVENT_GATTC_DATA_RECEIVED)
            if res[0] == 0 and res[1].payload.attr_handle == battery_level_handle:
                logging.info(
                    f'Received battery level {res[1].payload.data[0]}')
            else:
                logging.error(f'Error receiving data: {hex(res[0])}')
        except:
            pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    parser.add_argument('-l', '--low-power', action='store_true',
                        help="Enable low power mode for the peripheral")
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    low_power = args.low_power

    boards = If820Board.get_connected_boards()
    if len(boards) < 2:
        logging.critical(
            "Two IF820 boards required for this sample.")
        exit(1)

    logging.info('Init boards...')
    if820_board_c = boards[0]
    if820_board_p = boards[1]
    # Reset the DVK Probe to clear all IO states
    if820_board_p.open_and_init_board(False)
    if820_board_c.open_and_init_board(False)
    if820_board_p.close_ports_and_reset()
    time.sleep(0.5)
    if820_board_c.close_ports_and_reset()
    # Wait for boards to re-enumerate over USB
    time.sleep(5)
    # Init the modules
    boot_info_p = if820_board_p.open_and_init_board()
    boot_info_c = if820_board_c.open_and_init_board()
    logging.info(f'Advertiser: {boot_info_p}')
    logging.info(f'Scanner: {boot_info_c}')
    PERIPHERAL_ADDRESS = boot_info_p.payload.address

    logging.debug('Stop peripheral device advertising...')
    if820_board_p.stop_advertising()

    res = if820_board_c.p_uart.send_and_wait(
        if820_board_c.p_uart.CMD_GAP_GET_CONN_PARAMS)
    quit_on_resp_err(res[0])
    logging.debug(f'Default connection parameters: {res[1]}')

    # Start scanner thread
    threading.Thread(target=scanner_thread,
                     daemon=True).start()

    logging.info('Configure advertiser...')

    if low_power:
        logging.info(f'A: Keep module awake')
        if820_board_p.probe.gpio_to_output(if820_board_p.LP_MODE)
        board_wake_up(if820_board_p)
        if820_board_p.probe.gpio_to_input(if820_board_p.BT_HOST_WAKE)
        res = board_wait_awake(if820_board_p)
        logging.info(f'A: BT_HOST_WAKE: {res}')

        logging.info(f'A: Stop Bluetooth classic discovery.')
        ez_rsp = if820_board_p.p_uart.send_and_wait(
            if820_board_p.p_uart.CMD_SET_PARAMS,
            apiformat=ez_port.EzSerialApiMode.TEXT.value,
            link_super_time_out=0x7d00,
            discoverable=0,
            connectable=0,
            flags=0,
            scn=0,
            active_bt_discoverability=0,
            active_bt_connectability=0)
        If820Board.check_if820_response(
            if820_board_p.p_uart.CMD_SET_PARAMS, ez_rsp)

    # Set advertising parameters
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_SET_ADV_PARAMETERS,
                                                        mode=ADV_MODE,
                                                        type=ADV_TYPE,
                                                        channels=ADV_CHANNELS,
                                                        high_interval=ADV_INTERVAL,
                                                        high_duration=ADV_TIMEOUT,
                                                        low_interval=ADV_INTERVAL,
                                                        low_duration=ADV_TIMEOUT,
                                                        flags=ADV_FLAGS,
                                                        directAddr=[
                                                            0, 0, 0, 0, 0, 0],
                                                        directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])
    # Set custom advertising data
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_SET_ADV_DATA,
                                                        data=ADV_DATA)[0])

    # Setup Battery Service
    # Create battery service descriptor
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GATTS_CREATE_ATTR,
        type=ez_port.GattAttrType.STRUCTURE.value,
        perm=ez_port.GattAttrPermission.READ.value,
        length=4,
        data=bytearray.fromhex('00280F18'))[0])
    # Create battery level characteristic descriptor
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GATTS_CREATE_ATTR,
        type=ez_port.GattAttrType.STRUCTURE.value,
        perm=ez_port.GattAttrPermission.READ.value,
        length=7,
        data=bytearray.fromhex('0328121800192A'))[0])
    # Create battery level characteristic value descriptor
    res = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GATTS_CREATE_ATTR,
        type=ez_port.GattAttrType.VALUE.value,
        perm=ez_port.GattAttrPermission.READ.value | ez_port.GattAttrPermission.AUTH_WRITE.value,
        length=1,
        data=[battery_level])
    quit_on_resp_err(res[0])
    battery_level_handle = res[1].payload.handle
    # Create battery level Client Characteristic Configuration descriptor
    res = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GATTS_CREATE_ATTR,
        type=ez_port.GattAttrType.STRUCTURE.value,
        perm=ez_port.GattAttrPermission.READ.value | ez_port.GattAttrPermission.WRITE_ACK.value,
        length=4,
        data=bytearray.fromhex('02290000'))
    quit_on_resp_err(res[0])
    batter_level_ccc_handle = res[1].payload.handle

    # Start advertising
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_START_ADV,
                                                        mode=ADV_MODE,
                                                        type=ADV_TYPE,
                                                        channels=ADV_CHANNELS,
                                                        high_interval=ADV_INTERVAL,
                                                        high_duration=0,
                                                        low_interval=ADV_INTERVAL,
                                                        low_duration=0,
                                                        flags=ADV_FLAGS,
                                                        directAddr=[
                                                            0, 0, 0, 0, 0, 0],
                                                        directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])

    logging.debug('Peripheral: Wait for connection...')
    res = if820_board_p.wait_for_ble_connection()
    logging.info(f'Peripheral: Connected! [{res}]')
    con_handle = res.payload.conn_handle

    while (True):
        time.sleep(DATA_UPDATE_INTERVAL_SECONDS)
        battery_level -= 1
        if battery_level < 0:
            battery_level = 100

        if low_power:
            board_wake_up(if820_board_p)
            board_wait_awake(if820_board_p)

        try:
            res = if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GATTS_WRITE_HANDLE,
                                                     attr_handle=battery_level_handle,
                                                     data=[battery_level])
            if res[0] == 0:
                logging.info(f'Changed battery level: {battery_level}')
            else:
                logging.error(
                    f'Failed to change battery level {battery_level} [{hex(res[0])}]')
                if low_power:
                    board_allow_sleep(if820_board_p)
                continue

            # Notify that battery level has changed to any connected device subscribed to notifications
            res = if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GATTS_NOTIFY_HANDLE,
                                                     conn_handle=con_handle,
                                                     attr_handle=battery_level_handle,
                                                     data=[battery_level])
            if res[0] != 0:
                logging.error(
                    f'Failed to notify battery level {battery_level} [{hex(res[0])}]')
            if low_power:
                board_allow_sleep(if820_board_p)
        except:
            pass
