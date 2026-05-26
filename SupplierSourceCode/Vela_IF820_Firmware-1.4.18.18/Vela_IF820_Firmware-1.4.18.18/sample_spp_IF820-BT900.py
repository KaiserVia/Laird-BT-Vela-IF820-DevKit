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
-BT900 connected to PC via USB to act as a Bluetooth Peripheral
-IF820 connected to PC via USB to act as a Bluetooth Central
"""

SPP_DATA = "abcdefghijklmnop"
OTA_LATENCY = 1

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-cp', '--connection_p',
                        required=True, help="BT900 Peripheral COM port")
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    args, unknown = parser.parse_known_args()
    logging.basicConfig(format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
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

    logging.info("IF820 Factory Reset")
    ez_rsp = if820_board_c.p_uart.send_and_wait(
        if820_board_c.p_uart.CMD_FACTORY_RESET)
    If820Board.check_if820_response(
        if820_board_c.p_uart.CMD_FACTORY_RESET, ez_rsp)
    logging.info("Wait for IF820 Reboot...")
    ez_rsp = if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)

    # bt900 query firmware version
    response = bt900_peripheral.get_bt900_fw_ver()
    logging.info(f"BT900 firmware version = {response}")

    # bt900 get mac address of peripheral
    bt900_mac = bt900_peripheral.get_bt900_bluetooth_mac()
    logging.info(f"BT900 bluetooth mac addr = {bt900_mac}")

    # bt900 enter command mode
    bt900_peripheral.enter_command_mode()

    # bt900 delete all previous bonds
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_BTC_BOND_DEL)
    BT900SerialPort.check_bt900_response(response)

    # bt900 set io cap
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_BTC_IOCAP)
    BT900SerialPort.check_bt900_response(response)

    # bt900 set pairable
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_SET_BTC_PAIRABLE)
    BT900SerialPort.check_bt900_response(response)

    # bt900 set connectable
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_SET_BTC_CONNECTABLE)
    BT900SerialPort.check_bt900_response(response)

    # bt900 set discoverable
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_SET_BTC_DISCOVERABLE)
    BT900SerialPort.check_bt900_response(response)

    # bt900 open spp port
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_SPP_OPEN)
    BT900SerialPort.check_bt900_response(response)

    # IF820(central) connect to BT900 (peripheral)
    logging.info("IF820 Connect to BT900")
    bt900_mac[1].reverse()
    response = if820_board_c.p_uart.send_and_wait(command=if820_board_c.p_uart.CMD_CONNECT,
                                                  address=bt900_mac[1],
                                                  type=1)
    If820Board.check_if820_response(
        if820_board_c.p_uart.CMD_CONNECT, response)

    # IF820 Event (Text Info contains "P")
    logging.info("Wait for IF820 Pairing Requested Event.")
    response = if820_board_c.p_uart.wait_event(
        event=if820_board_c.p_uart.EVENT_SMP_PAIRING_REQUESTED)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_SMP_PAIRING_REQUESTED, response)

    # bt900 event (Text Info contains "Pair Req")
    logging.info("Wait for BT900 Pair Request Event.")
    bt900_event = bt900_peripheral.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_peripheral.BT900_PAIR_REQ)

    # bt900 set pairable (Response is "OK")
    logging.info("Send BT900 Pair Request")
    response = bt900_peripheral.send(
        bt900_peripheral.BT900_CMD_BTC_PAIR_RESPONSE)
    BT900SerialPort.check_bt900_response(response)

    # IF820 Event (Text Info contains "PR")
    logging.info("Wait for IF820 SMP Pairing Result Event.")
    response = if820_board_c.p_uart.wait_event(
        event=if820_board_c.p_uart.EVENT_SMP_PAIRING_RESULT)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_SMP_PAIRING_RESULT, response)

    # bt900 event (Text Info contains "Pair Result")
    logging.info("Wait for BT900 SMP Pair Result.")
    bt900_event = bt900_peripheral.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_peripheral.BT900_PAIR_RESULT)

    # IF820 Event (Text Info contains "ENC")
    logging.info("Wait for IF820 SMP Encryption Status Event.")
    response = if820_board_c.p_uart.wait_event(
        event=if820_board_c.p_uart.EVENT_SMP_ENCRYPTION_STATUS)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_SMP_PASSKEY_DISPLAY_REQUESTED, response)

    # IF820 Event
    logging.info("Wait for IF820 Bluetooth Connected Event.")
    response = if820_board_c.p_uart.wait_event(
        event=if820_board_c.p_uart.EVENT_BT_CONNECTED)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_BT_CONNECTED, response)

    # bt900 event
    logging.info("Wait for BT900 SPP Connect Event.")
    bt900_event = bt900_peripheral.wait_for_response()
    BT900SerialPort.check_bt900_response(
        bt900_event, bt900_peripheral.BT900_SPP_CONNECT)

    logging.info("IF820->BT900 SPP Data")
    bt900_peripheral.clear_cmd_rx_queue()
    if820_board_c.p_uart.send(bytes(SPP_DATA, 'utf-8'))
    while True:
        try:
            rx_data = bt900_peripheral.wait_for_response(OTA_LATENCY)
            logging.info(f"BT900 RX Data = {bytes(rx_data, 'utf-8')}")
        except:
            break

    logging.info("BT900->IF820 SPP Data")
    if820_board_c.p_uart.clear_rx_queue()
    bt900_peripheral.send(
        bt900_peripheral.BT900_SPP_WRITE_PREFIX + SPP_DATA)
    time.sleep(OTA_LATENCY)
    rx_data = if820_board_c.p_uart.read()
    logging.info(f"IF820 RX Data = {rx_data}")

    logging.info("BT900 Disconnect SPP")
    bt900_peripheral.send(bt900_peripheral.BT900_SPP_DISCONNECT)
    logging.info("IF820 Wait for BT Disconnect Event...")
    response = if820_board_c.p_uart.wait_event(
        if820_board_c.p_uart.EVENT_BT_DISCONNECTED)
    If820Board.check_if820_response(
        if820_board_c.p_uart.EVENT_BT_DISCONNECTED, response)
    logging.info("BT900 exit command mode")
    bt900_peripheral.exit_command_mode()
    bt900_peripheral.close()

    logging.info("IF820 Factory Reset")
    if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_FACTORY_RESET)
    logging.info("Wait for IF820 Reboot...")
    if820_board_c.p_uart.wait_event(if820_board_c.p_uart.EVENT_SYSTEM_BOOT)
    logging.info("Reset IF820 DVK")
    if820_board_c.close_ports_and_reset()
    logging.info("Done!")
