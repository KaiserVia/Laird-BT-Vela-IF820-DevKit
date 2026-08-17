#!/usr/bin/env python3
"""Select and run IF820 BLE or BT Classic smoke and transfer tests."""

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


TEST_DIRECTORY = Path(__file__).resolve().parent
SOURCE_ROOT = TEST_DIRECTORY.parents[1]
sys.path.append(str(SOURCE_ROOT / 'common_lib' / 'libraries'))

from If820Board import If820Board


TESTS = {
    'ble-smoke': TEST_DIRECTORY / 'ble' / 'ble_smoke_test.py',
    'ble-transfer': TEST_DIRECTORY / 'ble' / 'ble_transfer_test.py',
    'classic-smoke': TEST_DIRECTORY / 'classic' / 'smoke_test.py',
    'classic-transfer': TEST_DIRECTORY / 'classic' / 'transfer_test.py',
}
REPORT_ROOT = TEST_DIRECTORY / 'Testreports'


def wait_for_board(timeout_seconds: int) -> bool:
    """Wait for a DVK probe that may still be re-enumerating after a reset."""
    deadline = time.monotonic() + timeout_seconds
    while True:
        if If820Board.get_connected_boards():
            return True
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        print(f'Waiting for IF820 DVK probe ({int(remaining)} s remaining)...', flush=True)
        time.sleep(min(5, remaining))


def summarize_artifacts(artifacts: list[Path]) -> dict:
    """Extract the result and important integrity counters from JSON artifacts."""
    detail = {}
    for artifact in artifacts:
        if artifact.suffix != '.json':
            continue
        try:
            data = json.loads(artifact.read_text(encoding='ascii'))
        except (OSError, ValueError):
            continue
        if artifact.name.startswith('cyspp-') and 'result' in data:
            detail['if820_result'] = data['result']
            detail['if820_mac'] = data.get('if820_mac')
            transfer = data.get('transfer')
            if transfer is not None:
                detail['transfer'] = {
                    'requested_bytes': transfer['requested_payload_bytes'],
                    'received_bytes': transfer['uart_bytes_received'],
                    'lost_bytes': transfer['lost_payload_bytes'],
                    'corrupt_bytes': transfer['corrupted_payload_bytes'],
                    'status': transfer['status'],
                }
        elif artifact.name.startswith('bt-classic-'):
            detail.update({key: value for key, value in data.items()
                           if key not in ('pc_to_if820', 'if820_to_pc')})
            for direction in ('pc_to_if820', 'if820_to_pc'):
                if direction in data:
                    detail[direction] = data[direction]
    return detail


def report_link(report_path: Path, artifact_path: Path) -> str:
    """Create a Markdown link relative to the overview report directory."""
    relative_path = os.path.relpath(artifact_path, report_path.parent).replace('\\', '/')
    return f'[{artifact_path.name}]({relative_path})'


def write_summary(results: list[dict], report_directory: Path) -> tuple[Path, Path]:
    """Write one overview that includes each test and its individual reports."""
    timestamp = report_directory.name
    json_path = report_directory / 'overview.json'
    markdown_path = report_directory / 'overview.md'
    passed = all(result['passed'] for result in results)
    report = {'timestamp_local': timestamp, 'passed': passed, 'tests': results}
    json_path.write_text(json.dumps(report, indent=2), encoding='ascii')
    lines = [
        '# Bluetooth Test Run',
        '',
        f'Generated (local time): `{timestamp}`',
        f'Overall result: `{"PASSED" if passed else "FAILED"}`',
        '',
        '| Test | Result | Exit code |',
        '| --- | --- | ---: |',
    ]
    for result in results:
        status = 'PASSED' if result['passed'] else 'FAILED'
        lines.append(f'| `{result["name"]}` | `{status}` | {result["exit_code"]} |')
    for result in results:
        lines.extend(['', f'## {result["name"]}', ''])
        lines.append(f'Result: `{"PASSED" if result["passed"] else "FAILED"}` '
                     f'(exit code `{result["exit_code"]}`)')
        detail = result.get('detail', {})
        if detail:
            lines.extend(['', '```json', json.dumps(detail, indent=2), '```'])
        artifacts = result.get('artifacts', [])
        if artifacts:
            lines.extend(['', 'Individual reports:'])
            for artifact in artifacts:
                lines.append(f'- {report_link(markdown_path, Path(artifact))}')
        else:
            lines.append('')
            lines.append('No individual artifact was produced before this test ended.')
    markdown_path.write_text('\n'.join(lines) + '\n', encoding='ascii')
    return json_path, markdown_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tests', choices=tuple(TESTS), nargs='+', default=list(TESTS),
                        help='Tests to execute; defaults to the full unattended suite.')
    parser.add_argument('--mode', choices=('first', 'reconnect'), default='reconnect',
                        help='Classic pairing mode; defaults to the existing Windows pairing.')
    parser.add_argument('--bytes', type=int, default=40 * 1024)
    parser.add_argument('--board-wait-seconds', type=int, default=60,
                        help='Seconds to wait for an IF820 DVK probe before failing.')
    parser.add_argument('--new-module', action='store_true')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    if args.bytes < 1:
        parser.error('--bytes must be positive.')
    if args.board_wait_seconds < 0:
        parser.error('--board-wait-seconds must not be negative.')
    selected = args.tests

    run_timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
    run_directory = REPORT_ROOT / run_timestamp
    run_directory.mkdir(parents=True)
    results = []
    if not wait_for_board(args.board_wait_seconds):
        message = f'No IF820 DVK probe found after waiting {args.board_wait_seconds} seconds.'
        print(message, file=sys.stderr)
        results.append({
            'name': 'preflight',
            'exit_code': 1,
            'passed': False,
            'artifacts': [],
            'detail': {'error': message},
        })
        json_path, markdown_path = write_summary(results, run_directory)
        print(f'JSON report: {json_path}')
        print(f'Markdown report: {markdown_path}')
        return 1
    for name in selected:
        command = [sys.executable, str(TESTS[name])]
        if name.endswith('transfer'):
            command.extend(('--bytes', str(args.bytes)))
        if name.startswith('ble-') and args.new_module:
            command.append('--new-module')
        if name.startswith('classic-') and args.mode:
            command.extend(('--mode', args.mode))
        if args.debug:
            command.append('--debug')
        print(f'Running {name}...', flush=True)
        test_report_directory = run_directory / name
        test_report_directory.mkdir()
        environment = os.environ.copy()
        environment['BLUETOOTH_TEST_REPORT_DIR'] = str(test_report_directory)
        exit_code = subprocess.run(
            command, cwd=TEST_DIRECTORY, env=environment, check=False).returncode
        created = sorted(
            path for path in test_report_directory.iterdir()
            if path.suffix in ('.json', '.md'))
        results.append({
            'name': name,
            'exit_code': exit_code,
            'passed': exit_code == 0,
            'artifacts': [str(path) for path in created],
            'detail': summarize_artifacts(created),
        })
    json_path, markdown_path = write_summary(results, run_directory)
    print(f'JSON report: {json_path}')
    print(f'Markdown report: {markdown_path}')
    return 0 if all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())