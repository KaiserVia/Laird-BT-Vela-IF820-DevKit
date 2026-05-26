#!/usr/bin/env python3

"""The advertiser sample configures a peripheral device to advertise a custom
payload and a central device to scan for the peripheral device.
The custom advertisement payload contains a 1 byte counter value that is incremented
every two seconds.
The central device prints the counter value when it receives a new advertisement.
To run this test you need two IF820 DVK boards.

Use the -l option to enable low power mode for the advertiser.
This can be used to evaluate the power consumption of an IF820 advertiser.
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
ADV_TYPE = ez_port.GapAdvertType.UNDIRECTED_HIGH_DUTY_CYCLE.value
# The following two values can be adjusted to see how it affects the power consumption of the advertiser.
ADV_INTERVAL = 400  # 400 * 0.625ms = 250ms
ADV_UPDATE_INTERVAL_SECONDS = 2
ADV_CHANNELS = ez_port.GapAdvertChannels.CHANNEL_ALL.value
ADV_TIMEOUT = 0
ADV_FLAGS = ez_port.GapAdvertFlags.ALL.value
ADV_DATA = [0x02, 0x01, 0x06, 0x0a, 0x08, 0x6d, 0x79, 0x5f, 0x73, 0x65, 0x6e,
            0x73, 0x6f, 0x72, 0x04, 0xff, 0x77, 0x00, 0x01]
SCAN_MODE = ez_port.GapScanMode.NA.value
SCAN_INTERVAL = 0x40
SCAN_WINDOW = 0x40
SCAN_FILTER_ACCEPT_ALL = ez_port.GapScanFilter.NA.value
PERIPHERAL_ADDRESS = None


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
    """Thread to scan for the peripheral device and print the counter value.
    """
    last_counter = -1
    logging.info('Configure scanner...')
    logging.info(
        f'Scan mode: {SCAN_MODE}, interval: {SCAN_INTERVAL}, window: {SCAN_WINDOW}')
    quit_on_resp_err(if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GAP_START_SCAN,
                                                        mode=SCAN_MODE,
                                                        interval=SCAN_INTERVAL,
                                                        window=SCAN_WINDOW,
                                                        active=0,
                                                        filter=SCAN_FILTER_ACCEPT_ALL,
                                                        nodupe=0,
                                                        timeout=0)[0])
    while True:
        try:
            res = if820_board_c.p_uart.wait_event(
                if820_board_c.p_uart.EVENT_GAP_SCAN_RESULT)
            quit_on_resp_err(res[0])
            packet = res[1]
            logging.debug(f'Received {packet}')
            received_addr = packet.payload.address
            if received_addr == PERIPHERAL_ADDRESS:
                counter = packet.payload.data[-1]
                if counter != last_counter:
                    last_counter = counter
                    logging.info(f'Received value {counter}')
                else:
                    logging.debug('Received duplicate advert')
            else:
                logging.debug(f'Not looking for {received_addr}')
        except Exception as e:
            logging.warning(f'Scanner: {e}')
            if820_board_c.p_uart.close()
            time.sleep(0.5)
            if820_board_c.p_uart.open(
                if820_board_c.puart_port_name, if820_board_c.p_uart.IF820_DEFAULT_BAUD)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    parser.add_argument('-l', '--low-power', action='store_true',
                        help="Enable low power mode for the advertiser")
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

    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GAP_STOP_ADV)[0])

    # Start the scanner thread
    threading.Thread(target=scanner_thread,
                     daemon=True).start()

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
    logging.info(
        f'Advert mode: {ADV_MODE}, type:{ADV_TYPE}, high_interval: {ADV_INTERVAL}, high_duration:{ADV_TIMEOUT}, flags:{ADV_FLAGS}')
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_SET_ADV_DATA,
                                                        data=ADV_DATA)[0])
    quit_on_resp_err(if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_START_ADV,
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

    while (True):
        time.sleep(ADV_UPDATE_INTERVAL_SECONDS)
        counter = ADV_DATA[-1]
        counter = counter + 1
        if counter >= 256:
            counter = 0
        ADV_DATA[-1] = counter
        if low_power:
            board_wake_up(if820_board_p)
            board_wait_awake(if820_board_p)
        logging.info(f'Advertising value {counter}')
        quit_on_resp_err(if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_GAP_SET_ADV_DATA,
                                                            data=ADV_DATA)[0])
        if low_power:
            board_allow_sleep(if820_board_p)
