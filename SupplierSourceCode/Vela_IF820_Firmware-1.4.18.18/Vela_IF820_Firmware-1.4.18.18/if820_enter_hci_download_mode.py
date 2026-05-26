#!/usr/bin/env python3

import argparse
import logging
import sys
sys.path.append('./common_lib/libraries')
from If820Board import If820Board


if __name__ == '__main__':
    """Put a board into HCI download mode.
    If there is more than one board the user will be prompted to select a board.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--connection',
                        type=str, default=str(), help="Optional: HCI COM port")
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")

    logging.basicConfig(
        format='%(asctime)s | %(levelname)s | %(message)s', level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    hci_port = args.connection

    boards = If820Board.get_connected_boards()

    if len(boards) == 0:
        logging.error("No boards found")
        exit(1)

    choice = 0
    if len(boards) > 1 and not hci_port:
        print("Which board do you want to enter HCI download mode?")
        for i, board in enumerate(boards):
            print(f"{i}: {board.probe.id}")
        choice = int(input("Enter the number of the board: "))

    board = boards[choice]
    res = board.enter_hci_download_mode(hci_port)
    if res < 0:
        exit(res)
    if not hci_port:
        hci_port = board.hci_port_name

    logging.info(
        f"Board {board.probe.id} [{hci_port}] entered HCI download mode")
