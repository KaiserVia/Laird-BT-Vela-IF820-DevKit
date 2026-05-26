#!/usr/bin/env python3

import argparse
import logging
import sys
sys.path.append('./common_lib/libraries')
from If820Board import If820Board

"""
Hardware Setup
This sample requires the following hardware:
-IF820 connected to PC via USB
-Jumpers on PUART_TXD, PUART_RXD must be installed.
"""

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    if820_board_p = If820Board.get_board()
    if820_board_p.open_and_init_board()
    logging.info('Sending ping command...')
    res = if820_board_p.p_uart.send_and_wait(if820_board_p.p_uart.CMD_PING)
    If820Board.check_if820_response(if820_board_p.p_uart.CMD_PING, res)
