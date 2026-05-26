#!/usr/bin/env python3

"""
Hardware Setup
This sample requires the following hardware:
-BT900 connected to PC via USB to act as a Bluetooth Central
-IF820 connected to PC via USB to act as a Bluetooth Peripheral

*Note the BT900 has to be manually reset between runs
as there is no way to take it out of data mode.
"""

import argparse
import logging
import threading
import time
import sys
sys.path.append('./common_lib/libraries')
from BT900SerialPort import BT900SerialPort
from If820Board import If820Board

CYSPP_DATA = "abcdefghijklmnop"
BT900_MAC_PREFIX = "01"
OTA_LATENCY = 0.5


def __connection_thread():
    # IF820 Event (Text Info contains "C" for connect)
    logging.info("Wait for IF820 Connected Event...")
    response = if820_board_p.p_uart.wait_event(
        if820_board_p.p_uart.EVENT_GAP_CONNECTED)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_GAP_CONNECTED, response)

    # IF820 Event (Text Info contains "CU" for connection updated)
    logging.info("Wait for IF820 Connection Updated Event.")
    response = if820_board_p.p_uart.wait_event(
        if820_board_p.p_uart.EVENT_GAP_CONNECTION_UPDATED)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_GAP_CONNECTION_UPDATED, response)

    # bt900 open gattc
    logging.info("BT900 opening GATT client...")
    bt900_central.clear_cmd_rx_queue()
    response = bt900_central.send(
        bt900_central.BT900_GATTC_OPEN)
    BT900SerialPort.check_bt900_response(response)

    # bt900 enable notifications
    logging.info("BT900 enabling notifications...")
    response = bt900_central.send(
        bt900_central.BT900_ENABLE_CYSPP_NOT)
    BT900SerialPort.check_bt900_response(response)

    # IF820 Event (Text Info contains "W" for gatts data written)
    logging.info("Wait for IF820 Gatts Data Written Event.")
    response = if820_board_p.p_uart.wait_event(
        if820_board_p.p_uart.EVENT_GATTS_DATA_WRITTEN)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_GATTS_DATA_WRITTEN, response)

    # BT900 wait for notify
    bt900_central.wait_for_response()

    # The two devices are connected. We can now send data on CYSPP.
    # send data from IF820 -> BT900
    logging.info("Sending data from IF820 -> BT900...")
    bt900_central.clear_cmd_rx_queue()
    if820_board_p.p_uart.send(bytes(CYSPP_DATA, 'utf-8'))
    time.sleep(OTA_LATENCY)
    rx_data = bt900_central.read()
    logging.info(f"IF820->BT900 Data = {rx_data}")

    # send data from BT900 -> IF820
    logging.info("Sending data from BT900 -> IF820...")
    if820_board_p.p_uart.clear_rx_queue()
    bt900_central.send(
        bt900_central.BT900_CYSPP_WRITE_DATA_STRING + CYSPP_DATA)
    time.sleep(OTA_LATENCY)
    rx_data = if820_board_p.p_uart.read()
    string_utf8 = rx_data.decode('utf-8', 'ignore')
    logging.info(f"BT900->IF820 Data = {string_utf8}")

    logging.info("Success!  Close connection...")
    # End SPP Mode on both devices, and close open connections.
    if820_board_p.close_ports_and_reset()
    bt900_central.send(bt900_central.BT900_SPP_DISCONNECT)
    bt900_central.exit_command_mode()
    bt900_central.close()
    logging.info("Done!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-cc', '--connection_c',
                        required=True, help="Central COM port")
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    args, unknown = parser.parse_known_args()
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    # open devices
    # bt900
    bt900_central = BT900SerialPort()
    bt900_central.open(
        args.connection_c, bt900_central.BT900_DEFAULT_BAUD)

    # IF820
    if820_board_p = If820Board.get_board()
    if820_board_p.open_and_init_board()

    # bt900 query firmware version
    response = bt900_central.get_bt900_fw_ver()
    logging.info(f"BT900 firmware version = {response}")

    # IF820 Ping
    ez_rsp = if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_PING)
    logging.info(type(ez_rsp))
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_PING, ez_rsp)

    response = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GAP_STOP_ADV)
    If820Board.check_if820_response(
        if820_board_p.p_uart.CMD_GAP_STOP_ADV, response)
    if820_board_p.p_uart.wait_event(
        if820_board_p.p_uart.EVENT_GAP_ADV_STATE_CHANGED)

    # if820 get mac address of peripheral
    response = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(
        if820_board_p.p_uart.CMD_GET_BT_ADDR, response)
    # note the BT900 requires a 01 prefix to the start of the MAC address
    str_mac = BT900_MAC_PREFIX + If820Board.if820_mac_addr_response_to_mac_as_string(
        response[1].payload.address)
    logging.info(f'IF820 MAC address: {str_mac}')

    # bt900 enter command mode
    response = bt900_central.enter_command_mode()

    # if820 advertise
    response = if820_board_p.p_uart.send_and_wait(
        if820_board_p.p_uart.CMD_GAP_START_ADV,
        mode=2,
        type=3,
        channels=7,
        high_interval=0x30,
        high_duration=0x1e,
        low_interval=0x800,
        low_duration=0x3c,
        flags=0,
        directAddr=0,
        directAddrType=0)

    if820_board_p.p_uart.wait_event(
        if820_board_p.p_uart.EVENT_GAP_ADV_STATE_CHANGED)

    # Start connection thread
    conn_thread = threading.Thread(target=__connection_thread,
                     daemon=True)
    conn_thread.start()

    # bt900 cyspp connect
    # note:  the bt900 central could scan for devices and pick out the appropriate device to connect to.
    # however for simplicity since we already know its address we will skip that step and just connect.
    logging.info("BT900 Connecting to IF820...")
    connect_command = bt900_central.BT900_CYSPP_CONNECT + \
        str_mac + " 50 30 30 50"
    response = bt900_central.send(connect_command)
    BT900SerialPort.check_bt900_response(response)

    while conn_thread.is_alive():
        time.sleep(1)
