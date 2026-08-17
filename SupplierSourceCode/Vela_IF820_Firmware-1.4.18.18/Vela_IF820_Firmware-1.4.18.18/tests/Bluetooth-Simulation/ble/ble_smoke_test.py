#!/usr/bin/env python3
"""Run a BLE CYSPP connectivity smoke test on one IF820 DVK."""

import argparse
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--new-module', action='store_true')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    command = [sys.executable, str(Path(__file__).with_name('cyspp_dvk_hil.py')),
               '--pin', 'input']
    if args.new_module:
        command.append('--new-module')
    if args.debug:
        command.append('--debug')
    return subprocess.run(command, cwd=Path(__file__).parent, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())