#!/usr/bin/env python3
"""Run a bidirectional BT Classic SPP integrity transfer test."""

import argparse
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bytes', type=int, default=40 * 1024)
    parser.add_argument('--mode', choices=('first', 'reconnect'))
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    if args.bytes < 1:
        parser.error('--bytes must be positive.')
    firmware_directory = Path(__file__).resolve().parents[3]
    command = [sys.executable, str(firmware_directory / 'sample_spp_pc.py'),
               '--transfer-bytes', str(args.bytes)]
    if args.mode:
        command.extend(('--mode', args.mode))
    if args.debug:
        command.append('--debug')
    return subprocess.run(command, cwd=firmware_directory, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())