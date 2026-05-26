#!/usr/bin/env python3

import logging
import argparse
import time
import sys
sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board

"""
Hardware Setup
This sample requires the following hardware:
-IF820 connected to PC via USB to act as a Bluetooth Peripheral
-IF820 connected to PC via USB to act as a Bluetooth Central
"""

SPP_DATA = "abcdefghijklmnop"
OTA_LATENCY = 0.5


def factory_reset(board: If820Board):
    logging.info("IF820 Factory Reset")
    ez_rsp = board.p_uart.send_and_wait(
        board.p_uart.CMD_FACTORY_RESET)
    If820Board.check_if820_response(
        board.p_uart.CMD_FACTORY_RESET, ez_rsp)
    logging.info("Wait for IF820 Reboot...")
    ez_rsp = board.p_uart.wait_event(
        board.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(
        board.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)


def wait_for_connection(board: If820Board):
    ez_rsp = board.p_uart.wait_event(
        board.p_uart.EVENT_BT_CONNECTED)
    If820Board.check_if820_response(
        board.p_uart.EVENT_BT_CONNECTED, ez_rsp)


def send_receive_data(sender: If820Board, receiver: If820Board, data: str):
    receiver.p_uart.clear_rx_queue()
    sender.p_uart.send(bytes(data, 'utf-8'))
    # wait to ensure all data is sent and received
    time.sleep(OTA_LATENCY)
    rx_data = receiver.p_uart.read()
    logging.info(
        f"received: {rx_data}")
    if (len(rx_data) == 0):
        sys.exit(f"Error!  No data received.")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    args, unknown = parser.parse_known_args()
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    boards = If820Board.get_connected_boards()
    if len(boards) < 2:
        logging.critical(
            "Two IF820 boards required for this sample.")
        exit(1)

    if820_board_c = boards[0]
    if820_board_p = boards[1]
    if820_board_c.open_and_init_board()
    if820_board_p.open_and_init_board()

    factory_reset(if820_board_c)
    factory_reset(if820_board_p)

    # Query the peripheral to get is Bluetooth Address
    peripheral_bt_mac = None
    ez_rsp = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(
        if820_board_p.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    peripheral_bt_mac = ez_rsp[1].payload.address

    logging.info("Connect to Peripheral")
    ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_CONNECT,
                                                address=peripheral_bt_mac,
                                                type=1)
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_CONNECT, ez_rsp)
    logging.info("Wait for central connection...")
    wait_for_connection(if820_board_c)
    logging.info("Wait for peripheral connection...")
    wait_for_connection(if820_board_p)

    logging.info("Send data from Central to Peripheral")
    send_receive_data(if820_board_c, if820_board_p, SPP_DATA)
    logging.info("Send data from Peripheral to Central")
    send_receive_data(if820_board_p, if820_board_c, SPP_DATA)

    logging.info("Data exchanged! Reset the boards...")
    if820_board_p.close_ports_and_reset()
    if820_board_c.close_ports_and_reset()
    logging.info("Done!")
