#!/usr/bin/env python3

import argparse
import logging
import time
import sys
sys.path.append('./common_lib/libraries')
from BT900SerialPort import BT900SerialPort
from If820Board import If820Board
import EzSerialPort as ez_port

BT900_ADV_FLAGS = bytes([0x02, 0x01, 0x06])
BT900_ADV_NAME = bytes([0x0C, 0x09, 0x4C, 0x41, 0x49,
                       0x52, 0x44, 0x20, 0x42, 0x54, 0x39, 0x30, 0x30])
OTA_LATENCY = 0.2
CYSPP_DATA = "12345"

"""
Hardware Setup
This sample requires the following hardware:
-BT900 connected to PC via USB to act as a Bluetooth Peripheral.  SIO19 must be pulled to ground via jumper.
 *** The BT900 should be reset before the test to ensure it is advertising ***
-IF820 connected to PC via USB to act as a Bluetooth Central.
"""

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-cp', '--connection_p',
                        required=True, help="Peripheral COM port")
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
    bt900_peripheral = BT900SerialPort()
    bt900_peripheral.open(
        args.connection_p, bt900_peripheral.BT900_DEFAULT_BAUD)
    # IF820
    if820_board_c = If820Board.get_board()
    if820_board_c.open_and_init_board()

    # Put into central mode by setting CP_ROLE low
    logging.info("Put IF820 into central mode")
    if820_board_c.probe.gpio_to_output(if820_board_c.CP_ROLE)
    if820_board_c.probe.gpio_to_output_low(if820_board_c.CP_ROLE)
    ez_rsp = if820_board_c.p_uart.send_and_wait(
        if820_board_c.p_uart.CMD_REBOOT)
    If820Board.check_if820_response(if820_board_c.p_uart.CMD_REBOOT, ez_rsp)
    ez_rsp = if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)

    # IF820 Ping
    ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_PING)
    If820Board.check_if820_response(if820_board_c.p_uart.CMD_PING, ez_rsp)

    logging.info("Scanning for BT900...")
    bt900_addr = None
    while (bt900_addr == None):
        ez_rsp = if820_board_c.p_uart.wait_event(
            if820_board_c.p_uart.EVENT_GAP_SCAN_RESULT)
        packet = ez_rsp[1]
        if ((packet.payload.address_type == 0) and
            (packet.payload.data.startswith(BT900_ADV_FLAGS)) and
                (packet.payload.data.endswith(BT900_ADV_NAME))):
            logging.info("BT900 Found!")
            bt900_addr = packet.payload.address
            address_type = packet.payload.address_type
            logging.info(f'BT900 MAC address: {bt900_addr}')

    ez_rsp = if820_board_c.p_uart.send_and_wait(
        if820_board_c.p_uart.CMD_GAP_STOP_SCAN)
    If820Board.check_if820_response(
        if820_board_c.p_uart.CMD_GAP_STOP_SCAN, ez_rsp)
    if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_GAP_SCAN_STATE_CHANGED)

    # IF820(central) connect to BT900 (peripheral)
    conn_handle = None
    logging.info("Connecting to peripheral....")
    response = if820_board_c.p_uart.send_and_wait(command=if820_board_c.p_uart.CMD_GAP_CONNECT,
                                                  address=bt900_addr,
                                                  type=address_type,
                                                  interval=24,
                                                  slave_latency=5,
                                                  supervision_timeout=500,
                                                  scan_interval=0x0100,
                                                  scan_window=0x0100,
                                                  scan_timeout=0)
    If820Board.check_if820_response(
        if820_board_c.p_uart.CMD_GAP_CONNECT, response)
    logging.info("Wait for connection...")
    ez_rsp = if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_GAP_CONNECTED)
    logging.info("Connected!")
    conn_handle = ez_rsp[1].payload.conn_handle

    logging.info("Wait for connection events...")
    if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_GATTC_REMOTE_PROCEDURE_COMPLETE)

    logging.info("Enable characteristic notifications")
    if820_board_c.p_uart.clear_rx_queue()
    ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GATTC_WRITE_HANDLE,
                                                conn_handle=conn_handle,
                                                attr_handle=0x16,
                                                type=0,
                                                data=[0x01, 0x00])
    If820Board.check_if820_response(
        if820_board_c.p_uart.CMD_GATTC_WRITE_HANDLE, ez_rsp)
    ez_rsp = if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_GATTC_WRITE_RESPONSE)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_GATTC_WRITE_RESPONSE, ez_rsp)

    logging.info("Send data IF820->BT900")
    ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_GATTC_WRITE_HANDLE,
                                                conn_handle=conn_handle,
                                                attr_handle=0x13,
                                                type=1,
                                                data=list(bytes(CYSPP_DATA, 'utf-8')))
    If820Board.check_if820_response(
        if820_board_c.p_uart.CMD_GATTC_WRITE_HANDLE, ez_rsp)
    time.sleep(OTA_LATENCY)
    rx_data = bt900_peripheral.read()
    logging.info(f"BT900 RX Data = {rx_data}")

    logging.info("Send data BT900->IF820")
    bt900_peripheral.send_raw(CYSPP_DATA)
    ez_rsp = if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_GATTC_DATA_RECEIVED)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_GATTC_DATA_RECEIVED, ez_rsp)
    rx_data = ez_rsp[1].payload.data
    logging.info(f"IF820 RX Data = {bytes(rx_data)}")

    # End CYSPP Mode on both devices, and close open connections.
    if820_board_c.close_ports_and_reset()
    bt900_peripheral.close()
