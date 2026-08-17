#!/usr/bin/env python3
"""Drive or read individual IF820 DVK probe GPIOs."""

import argparse
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parents[1] / 'common_lib' / 'libraries'))

from If820Board import If820Board


PINS = {
    'bt-dev-wake': If820Board.BT_DEV_WAKE,
    'bt-host-wake': If820Board.BT_HOST_WAKE,
    'cy-role': If820Board.CP_ROLE,
    'cyspp': If820Board.CYSPP,
    'lp-mode': If820Board.LP_MODE,
    'connection': If820Board.CONNECTION,
}


def ask_choice(prompt: str, choices: tuple[str, ...]) -> str:
    """Prompt until the user enters one of the listed choices."""
    while True:
        answer = input(f'{prompt} [{"/".join(choices)}]: ').strip().lower()
        if answer in choices:
            return answer
        print(f'Please enter one of: {", ".join(choices)}')


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Drive or read an IF820 DVK probe control GPIO.')
    parser.add_argument('--pin', choices=sorted(PINS),
                        help='Logical DVK signal to control.')
    parser.add_argument('--state', choices=('high', 'low', 'input'),
                        help='Output level, or input to release the signal.')
    parser.add_argument('--list', action='store_true',
                        help='Read and display every known control GPIO.')
    args = parser.parse_args()

    if args.list:
        if args.pin or args.state:
            parser.error('--list cannot be combined with --pin or --state.')
    else:
        if args.state and not args.pin:
            parser.error('--state requires --pin.')
        if not args.pin:
            print('IF820 DVK GPIO control')
            args.pin = ask_choice('Pin', tuple(sorted(PINS)))
        if not args.state:
            args.state = ask_choice('State', ('high', 'low', 'input'))

    boards = If820Board.get_connected_boards()
    if not boards:
        print('No IF820 DVK probe found.', file=sys.stderr)
        return 1

    board = boards[0]
    board.probe.open()
    try:
        if args.list:
            for name, gpio in PINS.items():
                print(f'{name:14} GPIO {gpio:2}: {board.probe.gpio_read(gpio)}')
            return 0

        gpio = PINS[args.pin]
        if args.state == 'high':
            board.probe.gpio_to_output_high(gpio)
        elif args.state == 'low':
            board.probe.gpio_to_output_low(gpio)
        else:
            board.probe.gpio_to_input(gpio)

        print(f'{args.pin} (GPIO {gpio}) set to {args.state}; '
              f'sampled level={board.probe.gpio_read(gpio)}')
        return 0
    finally:
        board.probe.close()


if __name__ == '__main__':
    raise SystemExit(main())