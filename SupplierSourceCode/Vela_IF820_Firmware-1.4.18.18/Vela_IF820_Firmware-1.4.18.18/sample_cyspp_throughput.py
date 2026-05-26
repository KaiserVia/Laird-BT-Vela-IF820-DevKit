#!/usr/bin/env python3

import logging
import argparse
import time
import sys
import threading
import random
import string
sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board

"""
This sample creates a CYSPP (BLE) connection between two IF820 boards and sends data between them.
Data is sent from the Central to the Peripheral and then from the Peripheral to the Central.
Received data is compared to the sent data to ensure data integrity.
Each data chunk sent is randomly generated.

Hardware Setup
This sample requires the following hardware:
-IF820 connected to PC via USB to act as a Bluetooth Peripheral
-IF820 connected to PC via USB to act as a Bluetooth Central
-The jumper on J3 CP_ROLE must be placed.
"""

UART_BITS_PER_BYTE = 10
# UART baud rate to use for the test.
# This baud rate is only set for this session and not saved to flash.
BAUD_RATE = 1000000
# UART flow control is used for the test. It is required to prevent data loss. Set to 0 at your own risk
FLOW_CONTROL = 1
# How long to run the throughput test for.
THROUGHPUT_TEST_TIMEOUT_SECS = 10
# Length of the data chunk to send. This is the size of each chuck written in the send loop.
# To achieve maximum throughput, this should be set to greater than or equal to the number of bytes
# that can be sent in one send loop iteration. For example:
# SEND_DATA_CHUNK_LEN = THROUGHPUT_TEST_TIMEOUT_SECS * BAUD_RATE / UART_BITS_PER_BYTE
SEND_DATA_CHUNK_LEN = THROUGHPUT_TEST_TIMEOUT_SECS * \
    BAUD_RATE / UART_BITS_PER_BYTE / 2
# How long to wait for data to be received. This is used to determine when RX is finished.
RX_TIMEOUT_SECS = 1

SEND_CHUNK_LIMIT = 3000000

SCAN_MODE_GENERAL_DISCOVERY = ez_port.GapScanMode.NA.value
SCAN_FILTER_ACCEPT_ALL = ez_port.GapScanFilter.NA.value
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


def send_receive_data(sender: If820Board, receiver: If820Board):
    dev_name = sender.probe.id
    rx_done_event = threading.Event()
    tx_done_event = threading.Event()
    data_sent = []
    data_received = []
    tx_start_time = time.time()

    receiver.p_uart.clear_rx_queue()
    rx_thread = threading.Thread(target=lambda: __receive_data_thread(receiver,
                                                                      data_sent,
                                                                      data_received,
                                                                      rx_done_event,
                                                                      tx_start_time,
                                                                      tx_done_event),
                                 daemon=True)
    rx_thread.start()
    logging.info(
        f"{dev_name} start sending data for at least {THROUGHPUT_TEST_TIMEOUT_SECS} seconds...")
    packet_num = 0
    chunk_len = int(SEND_DATA_CHUNK_LEN)
    if chunk_len > SEND_CHUNK_LIMIT:
        chunk_len = SEND_CHUNK_LIMIT
    while (time.time() - tx_start_time) < THROUGHPUT_TEST_TIMEOUT_SECS:
        send = bytearray(''.join(random.choices(string.ascii_letters +
                                                string.digits, k=chunk_len)), 'utf-8')
        # Add a packet header that contains the packet number
        # This is useful for identifying the packet when looking at UART logic traces
        header = packet_num.to_bytes(4, byteorder='little')
        send[:4] = header
        send = bytes(send)
        sender.p_uart.send(send)
        data_sent.extend(list(send))
        packet_num += 1

    logging.info(
        f"{dev_name} sent {len(data_sent)} bytes, Wait for data to be received...")
    tx_done_event.set()
    if not rx_done_event.wait(THROUGHPUT_TEST_TIMEOUT_SECS * 2):
        logging.error("{dev_name} timeout waiting for data to be received")


def __receive_data_thread(receiver: If820Board,
                          data_sent: list,
                          data_received: list,
                          rx_done_event: threading.Event,
                          tx_start_time: float,
                          tx_done_event: threading.Event):
    dev_name = receiver.probe.id
    last_rx_time = time.time()
    logging.info(f"{dev_name} start receiving data...")
    data_received = []
    while time.time() - last_rx_time < RX_TIMEOUT_SECS or len(data_received) == 0 or not tx_done_event.is_set():
        rx_data = receiver.p_uart.read()
        if len(rx_data) > 0:
            last_rx_time = time.time()
            data_received.extend(list(rx_data))
    logging.info(
        f"{dev_name} all data received! Received {len(data_received)} bytes")
    if data_received != data_sent:
        logging.error(
            f"\r\n\r\n{dev_name} data received does not match data sent!\r\n")
        for index, (txd, rxd) in enumerate(zip(data_sent, data_received)):
            if txd != rxd:
                print(
                    f"{dev_name} RX mismatch at index: {index} (packet {hex(int(index/SEND_DATA_CHUNK_LEN))}), val: {hex(rxd)}")
                print(
                    f"{dev_name} tx[{index}]: {hex(data_sent[index])}, tx[{index + 1}]: {hex(data_sent[index + 1])}, tx[{index + 2}]: {hex(data_sent[index + 2])}")
                print(
                    f"{dev_name} rx[{index}]: {hex(data_received[index])}, rx[{index + 1}]: {hex(data_received[index + 1])}")
                data_received.insert(index, data_sent[index])
                # Print the first mismatch
                break
    bytes_per_sec = len(data_received) / (last_rx_time - tx_start_time)
    logging.info(
        f"{dev_name} Total time: {last_rx_time - tx_start_time:.1f} Throughput: {bytes_per_sec:.2f} Bps ({bytes_per_sec * 8:.2f} bps)")
    rx_done_event.set()


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

    try:
        factory_reset(if820_board_c)
        factory_reset(if820_board_p)

        logging.info(f"Set UART baud {BAUD_RATE} flow control {FLOW_CONTROL}")
        ez_rsp = if820_board_c.p_uart.send_and_wait(if820_board_c.p_uart.CMD_SET_UART_PARAMS,
                                                    baud=BAUD_RATE,
                                                    autobaud=0,
                                                    autocorrect=0,
                                                    flow=FLOW_CONTROL,
                                                    databits=8,
                                                    parity=0,
                                                    stopbits=1,
                                                    uart_type=0)
        If820Board.check_if820_response(
            if820_board_c.p_uart.CMD_SET_UART_PARAMS, ez_rsp)
        ez_rsp = if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_SET_UART_PARAMS,
                                                    baud=BAUD_RATE,
                                                    autobaud=0,
                                                    autocorrect=0,
                                                    flow=FLOW_CONTROL,
                                                    databits=8,
                                                    parity=0,
                                                    stopbits=1,
                                                    uart_type=0)
        If820Board.check_if820_response(
            if820_board_p.p_uart.CMD_SET_UART_PARAMS, ez_rsp)

        if820_board_c.reconfig_puart(BAUD_RATE)
        if820_board_p.reconfig_puart(BAUD_RATE)

        # Wait for the module to change its UART params
        time.sleep(1)

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
        send_receive_data(if820_board_c, if820_board_p)
        logging.info("Sending data from peripheral to central...")
        send_receive_data(if820_board_p, if820_board_c)

        logging.info("Running bidirectional throughput test...")
        dir1_thread = threading.Thread(target=lambda: send_receive_data(if820_board_c, if820_board_p),
                                       daemon=False)
        dir2_thread = threading.Thread(target=lambda: send_receive_data(if820_board_p, if820_board_c),
                                       daemon=False)
        dir1_thread.start()
        dir2_thread.start()
        while dir1_thread.is_alive() or dir2_thread.is_alive():
            time.sleep(1)
    except Exception as e:
        logging.critical(f"Error: {e}")
        raise e
    finally:
        logging.info("Closing ports and resetting IF820 boards")
        if820_board_c.close_ports_and_reset()
        if820_board_p.close_ports_and_reset()
