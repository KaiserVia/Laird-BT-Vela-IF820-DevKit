#!/usr/bin/env python3

"""
Python CLI for IF820 Firmware Upgrade

pyinstaller command to produce a single executable file:

pyinstaller --clean --console --noconfirm  --onefile --add-data "files/v1.4.18.18_int-ant/minidriver-20820A1-uart-patchram.hex:files/v1.4.18.18_int-ant/" --collect-all pyocd  --collect-all cmsis_pack_manager -p common_lib/libraries if820_flasher_cli.py

"""

import argparse
import logging
import textwrap
import os
import sys
sys.path.append('./common_lib/libraries')
from If820Board import If820Board
from HciProgrammer import HciProgrammer

LOG_MODULE_HCI_PORT = 'hci_port'
VERSION = '2.1.0'


def resource_path(relative_path):
    """ Get absolute path to resource, works for dev and for PyInstaller """
    base_path = getattr(sys, '_MEIPASS', os.path.dirname(
        os.path.abspath(__file__)))
    return os.path.join(base_path, relative_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog='if820_flasher_cli',
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     description=textwrap.dedent('''\
        CLI tool to flash an IF820 board (or compatible boards) with new firmware.
        If no COM port is specified, the tool will automatically detect the board and flash it.
        If there is more than one board detected, the user will be prompted to select the board to flash.
        The CLI supports chip erase, firmware update, and flashing firmware with chip erase.
                                        '''))
    parser.add_argument('-c', '--connection',
                        type=str, default=str(), help="HCI COM port")
    parser.add_argument('-ce', '--chip_erase', action='store_true',
                        help="perform full chip erase.")
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    parser.add_argument('-f', '--file',
                        help="application hex file to flash")
    parser.add_argument('-v', '--version', action='store_true',
                        help="Print the version of the tool and exit.")

    logging.basicConfig(
        format='%(asctime)s | %(levelname)s | %(message)s', level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    if args.version:
        print(f"{VERSION}")
        exit(0)

    print(f"IF820 Flasher CLI v{VERSION}")

    mini_driver = resource_path(
        f'files{os.sep}v1.4.18.18_int-ant{os.sep}minidriver-20820A1-uart-patchram.hex')
    com_port = args.connection
    firmware = args.file
    chip_erase = args.chip_erase

    # If the user specifies a COM port, flash firmware in manual mode
    if com_port:
        input("Ensure the board is in HCI download mode and press enter to continue...")
        p = HciProgrammer(mini_driver, com_port,
                          HciProgrammer.HCI_DEFAULT_BAUDRATE, chip_erase)
        if args.debug:
            logging.getLogger(LOG_MODULE_HCI_PORT).setLevel(logging.DEBUG)
        p.program_firmware(
            HciProgrammer.HCI_FLASH_FIRMWARE_BAUDRATE, firmware, chip_erase)
    else:
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
        board.flash_firmware(minidriver=mini_driver, firmware=firmware, chip_erase=chip_erase, verify=True)
