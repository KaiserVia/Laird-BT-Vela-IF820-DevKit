#!/usr/bin/env python3

import argparse
import logging
import time
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

    board = If820Board.get_board()
    board.open_and_init_board()
    ezp = board.p_uart

    while (True):
        logging.info('Send reboot')
        If820Board.check_if820_response(
            ezp.CMD_REBOOT, ezp.send_and_wait(ezp.CMD_REBOOT))
        logging.info('Wait for boot...')
        If820Board.check_if820_response(
            ezp.EVENT_SYSTEM_BOOT, ezp.wait_event(ezp.EVENT_SYSTEM_BOOT))
        time.sleep(5)
