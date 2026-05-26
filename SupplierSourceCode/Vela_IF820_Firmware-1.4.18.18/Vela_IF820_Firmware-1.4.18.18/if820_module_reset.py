#!/usr/bin/env python3

import argparse
import logging
import textwrap
import sys
sys.path.append('./common_lib/libraries')
from If820Board import If820Board

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="if820_module_reset",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent(
            """\
        Tool to hard reset the IF820 module.
                                        """
        ),
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable verbose debug messages"
    )

    logging.basicConfig(
        format="%(asctime)s | %(levelname)s | %(message)s", level=logging.INFO
    )
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    boards = If820Board.get_connected_boards()
    if len(boards) == 0:
        logging.error("No boards found")
        exit(1)

    choice = 0
    if len(boards) > 1:
        print("Which board do you want to flash?")
        for i, board in enumerate(boards):
            print(f"{i}: {board.probe.id}")
        choice = int(input("Enter the number of the board: "))
    board = boards[choice]
    board.probe.open()
    board.probe.reset_target()
    board.probe.close()
