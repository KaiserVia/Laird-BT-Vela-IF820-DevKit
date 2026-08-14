#!/usr/bin/env python3
"""Run the CYSPP DVK pin matrix and create a Markdown evidence report."""

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


TEST_DIRECTORY = Path(__file__).resolve().parent
HIL_SCRIPT = TEST_DIRECTORY / 'cyspp_dvk_hil.py'
ARTIFACT_DIRECTORY = TEST_DIRECTORY / 'artifacts'
PIN_MODES = ('high', 'low', 'input')
DEFAULT_TRANSFER_BYTES = 40 * 1024


def format_bytes(size: int) -> str:
    if size % (1024 * 1024) == 0:
        return f'{size // (1024 * 1024)} MB'
    if size % 1024 == 0:
        return f'{size // 1024} kB'
    return f'{size} bytes'


def report_files() -> set[Path]:
    return set(ARTIFACT_DIRECTORY.glob('cyspp-*.json'))


def run_case(pin_mode: str, transfer_bytes: int = 0) -> tuple[dict, int]:
    before = report_files()
    command = [sys.executable, str(HIL_SCRIPT), '--pin', pin_mode]
    if transfer_bytes:
        command.extend(('--transfer-bytes', str(transfer_bytes)))
    completed = subprocess.run(
        command,
        cwd=TEST_DIRECTORY,
        check=False)
    created = report_files() - before
    if len(created) != 1:
        return {
            'pin_mode': pin_mode,
            'pin_level': None,
            'result': 'runner_error',
            'events': [{'message': 'Expected exactly one HIL JSON artifact.',
                        'output': f'HIL process exit code: {completed.returncode}.'}],
        }, completed.returncode
    return json.loads(created.pop().read_text(encoding='ascii')), completed.returncode


def verdict(result: str) -> str:
    return 'PASS' if result == 'passed' else 'FAIL'


def write_markdown(matrix: list[dict], transfer_case: dict, timestamp: str) -> Path:
    report_path = ARTIFACT_DIRECTORY / f'cyspp-matrix-{timestamp}.md'
    firmware = transfer_case.get('if820_module_firmware')
    lines = [
        '# CYSPP DVK Pin Matrix',
        '',
        f'Generated: `{timestamp}` UTC',
        f'IF820 module firmware (read using /QFV): `{firmware}`',
        '',
        'The PC simulated the BLE app. The DVK probe simulated the MCU-owned',
        'CYSPP line. Each case ran in a separate Python process so a pending',
        'UART event wait from one case cannot affect the next case.',
        '',
        '| Pin mode | Sampled level | Result | UART receive mode | BLE device |',
        '| --- | ---: | --- | --- | --- |',
    ]
    for case in matrix:
        lines.append(
            f"| `{case['pin_mode']}` | `{case['pin_level']}` | "
            f"**{verdict(case['result'])}** (`{case['result']}`) | "
            f"`{case.get('uart_receive_mode')}` | `{case.get('ble_device_name')}` |")

    transfer = transfer_case.get('transfer')
    if transfer is not None:
        transfer_label = format_bytes(transfer['requested_payload_bytes'])
        verified_bytes = (
            transfer['requested_payload_bytes'] - transfer['lost_payload_bytes'] -
            transfer['corrupted_payload_bytes'])
        duration = transfer['duration_seconds']
        throughput = verified_bytes / duration if duration else 0
        connection = transfer_case['connection']
        lines.extend([
            '',
            f'## {transfer_label} Transfer',
            '',
            'This separate `input`-pin reference connection sends framed data after',
            'the CYSPP prompt. Frame sequence numbers and CRC-32 detect loss and',
            'corruption even when UART reads are fragmented.',
            '',
            '| Result | Duration | Verified throughput | BLE write errors | Unexpected disconnects |',
            '| --- | ---: | ---: | ---: | ---: |',
            f"| **{verdict(transfer_case['result'])}** (`{transfer_case['result']}`) | "
            f"{duration} s | {throughput:.1f} B/s | {transfer['write_errors']} | "
            f"{connection['unexpected_disconnects']} |",
            '',
            '| Sent payload | UART bytes received | Verified | Lost | Corrupt |',
            '| ---: | ---: | ---: | ---: | ---: |',
            f"| {transfer['requested_payload_bytes']} | {transfer['uart_bytes_received']} | "
            f"{verified_bytes} | {transfer['lost_payload_bytes']} | "
            f"{transfer['corrupted_payload_bytes']} |",
            '',
            '```mermaid',
            'pie showData',
            f'  title {transfer_label} payload integrity (bytes)',
            f'  "Verified" : {verified_bytes}',
            f'  "Lost" : {transfer["lost_payload_bytes"]}',
            f'  "Corrupt" : {transfer["corrupted_payload_bytes"]}',
            '```',
            '',
            '| Connection attempts | Successful | Disconnects | BLE errors | Frames sent | Valid frames |',
            '| ---: | ---: | ---: | ---: | ---: | ---: |',
            f"| {connection['attempts']} | {connection['successful_connections']} | "
            f"{connection['disconnects']} | {connection['errors']} | "
            f"{transfer['frames_sent']} | {transfer['frames_received']} |",
        ])

    lines.extend([
        '',
        '```mermaid',
        'flowchart LR',
        '  PC[PC app simulator] -->|BLE subscribe ...ca102| IF820[IF820 DVK]',
        '  DVK[DVK probe GPIO 19] -->|CYSPP pin state| IF820',
    ])
    for case in matrix:
        node_id = f"{case['pin_mode']}_{verdict(case['result']).lower()}"
        lines.append(
            f"  IF820 --> {node_id}[{case['pin_mode']} / level "
            f"{case['pin_level']} / {verdict(case['result'])}]")
    lines.extend(['```', '', '## Per-Case Evidence', ''])

    for case in matrix:
        lines.extend([
            f"### `{case['pin_mode']}` - {verdict(case['result'])}",
            '',
            f"- Sampled GPIO 19 level: `{case['pin_level']}`",
            f"- CYSPP parameters: `{case.get('cyspp_parameters')}`",
            f"- UART receive mode: `{case.get('uart_receive_mode')}`",
            f"- IF820 address: `{case.get('if820_mac')}`",
            '',
            'Events:',
        ])
        for event in case['events']:
            lines.append(f"- `{event.get('time', 'n/a')}` {event['message']}")
        lines.append('')

    report_path.write_text('\n'.join(lines), encoding='ascii')
    return report_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--transfer-bytes', type=int, default=DEFAULT_TRANSFER_BYTES,
                        help='Payload bytes for the input-pin transfer (default: 4096000).')
    args = parser.parse_args()
    if args.transfer_bytes < 1:
        parser.error('--transfer-bytes must be a positive integer.')

    ARTIFACT_DIRECTORY.mkdir(exist_ok=True)
    matrix = []
    exit_codes = []
    for pin_mode in PIN_MODES:
        print(f'Running CYSPP matrix case: {pin_mode}', flush=True)
        case, exit_code = run_case(pin_mode)
        matrix.append(case)
        exit_codes.append(exit_code)

    print(f'Running CYSPP transfer case: input ({format_bytes(args.transfer_bytes)})',
          flush=True)
    transfer_case, transfer_exit_code = run_case('input', args.transfer_bytes)
    exit_codes.append(transfer_exit_code)

    timestamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    json_path = ARTIFACT_DIRECTORY / f'cyspp-matrix-{timestamp}.json'
    aggregate = {
        'if820_module_firmware': transfer_case.get('if820_module_firmware'),
        'pin_matrix': matrix,
        'transfer': transfer_case,
    }
    json_path.write_text(json.dumps(aggregate, indent=2), encoding='ascii')
    markdown_path = write_markdown(matrix, transfer_case, timestamp)
    print(f'Matrix JSON report: {json_path}')
    print(f'Matrix Markdown report: {markdown_path}')
    return 0 if all(code == 0 for code in exit_codes) else 1


if __name__ == '__main__':
    raise SystemExit(main())