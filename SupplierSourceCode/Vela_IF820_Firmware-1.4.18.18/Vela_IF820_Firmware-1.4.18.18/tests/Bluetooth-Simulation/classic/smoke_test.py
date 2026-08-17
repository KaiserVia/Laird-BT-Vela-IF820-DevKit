#!/usr/bin/env python3
"""Run a BT Classic SPP connection smoke test using the Windows Bluetooth stack."""

import argparse
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('first', 'reconnect'))
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    firmware_directory = Path(__file__).resolve().parents[3]
    command = [sys.executable, str(firmware_directory / 'sample_spp_pc.py'),
               '--smoke']
    if args.mode:
        command.extend(('--mode', args.mode))
    if args.debug:
        command.append('--debug')
    return subprocess.run(command, cwd=firmware_directory, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())