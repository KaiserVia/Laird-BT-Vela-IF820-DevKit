#!/usr/bin/env python3

import logging
import argparse
import time
import sys
sys.path.append('./common_lib/libraries')
from If820Board import If820Board
import EzSerialPort as ez_port

SYS_DEEP_SLEEP_LEVEL = 2
HIBERNATE = False
SLEEP_TIME = 60

"""
Hardware Setup
This sample requires the following hardware:
-IF820 connected to PC via USB to act as a Bluetooth Peripheral
-Jumpers on PUART_TXD, PUART_RXD, LP_MODE, and BT_HOST_WAKE must be installed.
"""


def wait_board_ready(board: If820Board):
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


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    logging.basicConfig(format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    if820_board_p = If820Board.get_board()
    if820_board_p.open_and_init_board()

    # Disable sleep via gpio
    logging.info(f'GPIO Init')
    if820_board_p.probe.gpio_to_output(if820_board_p.LP_MODE)
    if820_board_p.probe.gpio_to_output_high(if820_board_p.LP_MODE)
    if820_board_p.probe.gpio_to_input(if820_board_p.BT_HOST_WAKE)
    res = if820_board_p.probe.gpio_read(if820_board_p.BT_HOST_WAKE)
    logging.info(f'BT_HOST_WAKE: {res}')

    # Send Ping just to verify coms before proceeding
    logging.info(f'Send Ping')
    ez_rsp = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_PING)
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_PING, ez_rsp)

    ez_rsp = if820_board_p.p_uart.send_and_wait(
        command=if820_board_p.p_uart.CMD_GAP_GET_ADV_PARAMETERS)
    If820Board.check_if820_response(
        if820_board_p.p_uart.CMD_GAP_GET_ADV_PARAMETERS, ez_rsp)

    if HIBERNATE:
        # set hibernate mode
        ez_rsp = if820_board_p.p_uart.send_and_wait(command=if820_board_p.p_uart.CMD_SET_SLEEP_PARAMS,
                                                    level=SYS_DEEP_SLEEP_LEVEL, hid_off_sleep_time=0)
        If820Board.check_if820_response(
            if820_board_p.p_uart.CMD_SET_SLEEP_PARAMS, ez_rsp)

    else:
        logging.info(f'Stop BLE advertising.')
        ez_rsp = if820_board_p.p_uart.send_and_wait(
            if820_board_p.p_uart.CMD_GAP_STOP_ADV)
        If820Board.check_if820_response(
            if820_board_p.p_uart.CMD_GAP_STOP_ADV, ez_rsp)

        logging.info(f'Stop Bluetooth classic discovery.')
        ez_rsp = if820_board_p.p_uart.send_and_wait(
            if820_board_p.p_uart.CMD_SET_PARAMS,
            link_super_time_out=0x7d00,
            discoverable=0,
            connectable=0,
            flags=0,
            scn=0,
            active_bt_discoverability=0,
            active_bt_connectability=0)
        If820Board.check_if820_response(
            if820_board_p.p_uart.CMD_SET_PARAMS, ez_rsp)

    # Enable Sleep via GPIO
    logging.info("Put device to sleep.")
    if820_board_p.probe.gpio_to_output_low(if820_board_p.LP_MODE)

    # Device will now sleep until awoken by the host.
    # This code can be commented out to keep the device sleeping.
    logging.info(f"Device sleeping for {SLEEP_TIME} seconds.")
    time.sleep(SLEEP_TIME)
    logging.info("Wake the device.")
    if820_board_p.probe.gpio_to_output_high(if820_board_p.LP_MODE)

    if HIBERNATE:
        # device will reboot when waking up from hibernate
        ez_rsp = if820_board_p.p_uart.wait_event(
            if820_board_p.p_uart.EVENT_SYSTEM_BOOT)

    # Wait for module to wake up
    wait_board_ready(if820_board_p)

    # Send Ping just to verify coms before proceeding
    ez_rsp = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_PING)
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_PING, ez_rsp)

    # Close the open com ports
    if820_board_p.close_ports_and_reset()
