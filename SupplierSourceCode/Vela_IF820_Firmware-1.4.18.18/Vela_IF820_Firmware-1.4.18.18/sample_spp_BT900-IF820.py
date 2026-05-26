#!/usr/bin/env python3

import argparse
import logging
import time
import sys
sys.path.append('./common_lib/libraries')
from BT900SerialPort import BT900SerialPort
from If820Board import If820Board
import EzSerialPort as ez_port

"""
Hardware Setup
This sample requires the following hardware:
-BT900 connected to PC via USB to act as a Bluetooth Central
-IF820 connected to PC via USB to act as a Bluetooth Peripheral
"""

SPP_DATA = "abcdefghijklmnop"
OTA_LATENCY = 1

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-cc', '--connection_c',
                        required=True, help="BT900 Central COM port")
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
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_PING, ez_rsp)

    # if820 get mac address of peripheral
    ez_rsp = if820_board_p.p_uart.send_and_wait(
        command=if820_board_p.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(
        if820_board_p.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    str_mac = If820Board.if820_mac_addr_response_to_mac_as_string(
        ez_rsp[1].payload.address)
    logging.info(f"IF820 MAC Addr = {str_mac}")

    # bt900 enter command mode
    response = bt900_central.enter_command_mode()

    # bt900 delete all previous bonds
    response = bt900_central.send(
        bt900_central.BT900_CMD_BTC_BOND_DEL)
    BT900SerialPort.check_bt900_response(response)

    # bt900 set io cap
    response = bt900_central.send(
        bt900_central.BT900_CMD_BTC_IOCAP)
    BT900SerialPort.check_bt900_response(response)

    # bt900 set pairable
    response = bt900_central.send(
        bt900_central.BT900_CMD_SET_BTC_PAIRABLE)
    BT900SerialPort.check_bt900_response(response)

    # bt900 spp connect
    logging.info("BT900 Start SPP Connect")
    connect_command = bt900_central.BT900_SPP_CONNECT_REQ + str_mac
    response = bt900_central.send(connect_command, 5)
    BT900SerialPort.check_bt900_response(response)

    # IF820 Event (Text Info contains "P")
    logging.info("Wait for IF820 Pairing Requested Event.")
    response = if820_board_p.p_uart.wait_event(
        event=if820_board_p.p_uart.EVENT_SMP_PAIRING_REQUESTED)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_SMP_PAIRING_REQUESTED, response)

    # bt900 event (Text Info contains "Pair Req")
    logging.info("Wait for BT900 Pair Request Event.")
    bt900_event = bt900_central.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_central.BT900_PAIR_REQ)

    # bt900 pair response
    response = bt900_central.send(
        bt900_central.BT900_CMD_BTC_PAIR_RESPONSE)
    BT900SerialPort.check_bt900_response(response)

    # IF820 Event (Text Info contains "PR")
    logging.info("Wait for IF820 SMP Pairing Result Event.")
    response = if820_board_p.p_uart.wait_event(
        event=if820_board_p.p_uart.EVENT_SMP_PAIRING_RESULT)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_SMP_PAIRING_RESULT, response)

    # bt900 event (Text Info contains "Pair Result")
    logging.info("Wait for BT900 SMP Pair Result.")
    bt900_event = bt900_central.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_central.BT900_PAIR_RESULT)

    # IF820 Event (Text Info contains "ENC")
    logging.info("Wait for IF820 SMP Encryption Status Event.")
    response = if820_board_p.p_uart.wait_event(
        event=if820_board_p.p_uart.EVENT_SMP_ENCRYPTION_STATUS)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_SMP_PASSKEY_DISPLAY_REQUESTED, response)

    # IF820 Event
    logging.info("Wait for IF820 Bluetooth Connected Event.")
    response = if820_board_p.p_uart.wait_event(
        event=if820_board_p.p_uart.EVENT_BT_CONNECTED)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_BT_CONNECTED, response)

    # bt900 event
    logging.info("Wait for BT900 SPP Connect Event.")
    # bt900 event (Text Info contains "Pair Result")
    # Note: For some reason the BT900 sends the Pair Result event twice.
    bt900_event = bt900_central.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_central.BT900_PAIR_RESULT)
    # Wait for connect response
    bt900_event = bt900_central.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_central.BT900_SPP_CONNECT)

    logging.info("IF820->BT900 SPP Data")
    bt900_central.clear_cmd_rx_queue()
    if820_board_p.p_uart.send(bytes(SPP_DATA, 'utf-8'))
    while True:
        try:
            rx_data = bt900_central.wait_for_response(OTA_LATENCY)
            logging.info(f"BT900 RX Data = {bytes(rx_data, 'utf-8')}")
        except:
            break

    logging.info("BT900->IF820 SPP Data")
    if820_board_p.p_uart.clear_rx_queue()
    bt900_central.send(
        bt900_central.BT900_SPP_WRITE_PREFIX + SPP_DATA)
    time.sleep(OTA_LATENCY)
    rx_data = if820_board_p.p_uart.read()
    logging.info(f"IF820 RX Data = {rx_data}")

    logging.info("BT900 Disconnect SPP")
    bt900_central.send(bt900_central.BT900_SPP_DISCONNECT)
    logging.info("IF820 Wait for BT Disconnect Event...")
    response = if820_board_p.p_uart.wait_event(
        if820_board_p.p_uart.EVENT_BT_DISCONNECTED)
    If820Board.check_if820_response(
        if820_board_p.p_uart.EVENT_BT_DISCONNECTED, response)
    logging.info("BT900 exit command mode")
    bt900_central.exit_command_mode()
    bt900_central.close()

    logging.info("IF820 Factory Reset")
    if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_FACTORY_RESET)
    logging.info("Wait for IF820 Reboot...")
    if820_board_p.p_uart.wait_event(if820_board_p.p_uart.EVENT_SYSTEM_BOOT)
    logging.info("Reset IF820 DVK")
    if820_board_p.close_ports_and_reset()
    logging.info("Done!")
