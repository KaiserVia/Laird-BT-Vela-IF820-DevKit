#!/usr/bin/env python3
"""Run a BLE CYSPP integrity transfer test on one IF820 DVK."""

import argparse
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bytes', type=int, default=40 * 1024)
    parser.add_argument('--new-module', action='store_true')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    if args.bytes < 1:
        parser.error('--bytes must be positive.')
    command = [sys.executable, str(Path(__file__).with_name('cyspp_dvk_hil.py')),
               '--pin', 'input', '--transfer-bytes', str(args.bytes)]
    if args.new_module:
        command.append('--new-module')
    if args.debug:
        command.append('--debug')
    return subprocess.run(command, cwd=Path(__file__).parent, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())