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
-P27 is the "Connection" indicator.  On the Laird Module this is pin 33 of the module, attached to GPIO_21 of the pico probe.
-The jumper on J3 CP_ROLE must be placed.
"""
SCAN_MODE_GENERAL_DISCOVERY = ez_port.GapScanMode.NA.value
SCAN_FILTER_ACCEPT_ALL = ez_port.GapScanFilter.NA.value
CYSPP_DATA = "abcdefghijklmnop"
CENTRAL_ROLE = 1
ENABLE_PLUS_AUTO_START = 2
CYSPP_RX_FLOW_CNTRL = 2
"""
There are two ways to set a device as a central.
Method 1:  GPIO
Method 2:  API Command
If using the API command method, additional api commands are required to connect to a device.
"""
GPIO_MODE = False
OTA_LATENCY = 1


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
    ez_rsp = board.p_uart.wait_event(
        board.p_uart.EVENT_GAP_ADV_STATE_CHANGED)
    If820Board.check_if820_response(
        board.p_uart.EVENT_GAP_ADV_STATE_CHANGED, ez_rsp)


def wait_for_cyspp_connection(board: If820Board, expected_status: int):
    wait_for_conn = True
    while (wait_for_conn):
        ez_rsp = board.p_uart.wait_event(
            board.p_uart.EVENT_P_CYSPP_STATUS)
        logging.info(ez_rsp)
        if ez_rsp[1].payload.status == expected_status:
            wait_for_conn = False


def send_receive_data(sender: If820Board, receiver: If820Board, data: str):
    receiver.p_uart.clear_rx_queue()
    sender.p_uart.send(bytes(data, 'utf-8'))
    # wait to ensure all data is sent and received
    time.sleep(OTA_LATENCY)
    rx_data = receiver.p_uart.read()
    logging.info(
        f"received: {rx_data}")
    if (len(rx_data) == 0):
        logging.error(f"No data received!")


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

    # Send Ping just to verify communication before proceeding
    ez_rsp = if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_PING)
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_PING, ez_rsp)
    ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_PING)
    If820Board.check_if820_response(if820_board_c.p_uart.CMD_PING, ez_rsp)

    # if820 get mac address of peripheral
    ez_rsp = if820_board_p.p_uart.send_and_wait(
        command=if820_board_p.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(
        if820_board_p.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    peripheral_addr = ez_rsp[1].payload.address
    logging.info(f'Peripheral MAC address: {peripheral_addr}')

    if GPIO_MODE:
        logging.info("Put IF820 into central mode via CP_ROLE pin")
        if820_board_c.probe.gpio_to_output(if820_board_c.CP_ROLE)
        if820_board_c.probe.gpio_to_output_low(if820_board_c.CP_ROLE)
        if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_REBOOT)

    else:
        # Get device into central mode using api.
        ez_rsp = if820_board_c.p_uart.send_and_wait(command=if820_board_c.p_uart.CMD_P_CYSPP_SET_PARAMETERS,
                                                    enable=ENABLE_PLUS_AUTO_START,
                                                    role=CENTRAL_ROLE,
                                                    company=305,
                                                    local_key=0,
                                                    remote_key=0,
                                                    remote_mask=0,
                                                    sleep_level=0,
                                                    server_security=0,
                                                    client_flags=CYSPP_RX_FLOW_CNTRL)
        If820Board.check_if820_response(
            if820_board_c.p_uart.CMD_P_CYSPP_SET_PARAMETERS, ez_rsp)

        logging.info("Scanning for peripheral device...")
        ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GAP_START_SCAN,
                                                    mode=SCAN_MODE_GENERAL_DISCOVERY,
                                                    interval=0x400,
                                                    window=0x400,
                                                    active=0,
                                                    filter=SCAN_FILTER_ACCEPT_ALL,
                                                    nodupe=1,
                                                    timeout=5)
        If820Board.check_if820_response(
            if820_board_c.p_uart.CMD_GAP_START_SCAN, ez_rsp)

        while True:
            ez_rsp = if820_board_c.p_uart.wait_event(
                if820_board_c.p_uart.EVENT_GAP_SCAN_RESULT)
            If820Board.check_if820_response(
                if820_board_c.p_uart.EVENT_GAP_SCAN_RESULT, ez_rsp)
            packet = ez_rsp[1]
            received_addr = packet.payload.address
            address_type = packet.payload.address_type
            if received_addr == peripheral_addr:
                logging.info('Found peripheral device!')
                break
            else:
                logging.debug(f'Not looking for {received_addr}')

        ez_rsp = if820_board_c.p_uart.send_and_wait(
            if820_board_c.p_uart.CMD_GAP_STOP_SCAN)
        If820Board.check_if820_response(
            if820_board_c.p_uart.CMD_GAP_STOP_SCAN, ez_rsp)

        logging.info('Connecting to peripheral device...')
        ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GAP_CONNECT,
                                                    address=received_addr,
                                                    type=address_type,
                                                    interval=24,
                                                    slave_latency=5,
                                                    supervision_timeout=500,
                                                    scan_interval=0x0100,
                                                    scan_window=0x0100,
                                                    scan_timeout=0)
        If820Board.check_if820_response(
            if820_board_c.p_uart.CMD_GAP_CONNECT, ez_rsp)

        res = if820_board_c.p_uart.wait_event(
            if820_board_c.p_uart.EVENT_GAP_CONNECTED)
        If820Board.check_if820_response(
            if820_board_c.p_uart.EVENT_GAP_CONNECTED, ez_rsp)
        logging.info('Central Connected!')
        res = if820_board_p.p_uart.wait_event(
            if820_board_p.p_uart.EVENT_GAP_CONNECTED)
        If820Board.check_if820_response(
            if820_board_p.p_uart.EVENT_GAP_CONNECTED, ez_rsp)
        logging.info('Peripheral Connected!')

    logging.info("Peripheral: Waiting for CYSPP connection...")
    wait_for_cyspp_connection(if820_board_p, 0x05)
    logging.info("Peripheral: CYSPP started")
    logging.info("Central: Waiting for CYSPP connection...")
    wait_for_cyspp_connection(if820_board_c, 0x35)
    logging.info("Central: CYSPP ready!")

    logging.info("Sending data from central to peripheral...")
    send_receive_data(if820_board_c, if820_board_p, CYSPP_DATA)

    logging.info("Sending data from peripheral to central...")
    send_receive_data(if820_board_p, if820_board_c, CYSPP_DATA)

    logging.info("Data exchanged! Reset the boards...")
    # clean everything up
    if820_board_p.close_ports_and_reset()
    if820_board_c.close_ports_and_reset()
    logging.info("Done!")
