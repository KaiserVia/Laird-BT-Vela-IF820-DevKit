#!/usr/bin/env python3
"""Reproduce CYSPP pin behavior using one IF820 DVK and a PC BLE central.

The DVK probe simulates the MCU-owned, low-active CYSPP line:
  --pin low    expected data-mode state
  --pin high   expected disconnect reproduction state
  --pin input  released/floating reproduction state
"""

import argparse
import asyncio
import json
import logging
import os
import sys
import threading
import time
import struct
import zlib
from datetime import datetime
from pathlib import Path
import tkinter as tk

SOURCE_ROOT = Path(__file__).resolve().parents[3]
sys.path.append(str(SOURCE_ROOT / 'common_lib' / 'libraries'))

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.backends.device import BLEDevice
from bleak.backends.winrt.util import uninitialize_sta
from If820Board import If820Board

CYSPP_DATA_UUID = '65333333-a115-11e2-9e9a-0800200ca102'
MCU_PROMPT = b'MCU-SIM PIN?\r\n'
APP_DATA = b'APP-SIM HELLO\r\n'
TRANSFER_MAGIC = b'CYS1'
TRANSFER_HEADER = struct.Struct('<4sIHI')
TRANSFER_ACK_MAGIC = b'ACK1'
TRANSFER_ACK = struct.Struct('<4sI')
TRANSFER_QUIET_TIME_SECONDS = 1.0
TRANSFER_RECEIVE_TIMEOUT_MIN_SECONDS = 20.0
TRANSFER_RECEIVE_TIMEOUT_MARGIN_SECONDS = 30.0
TRANSFER_PROGRESS_INTERVAL_SECONDS = 0.5
TRANSFER_ACK_TIMEOUT_SECONDS = 5.0
DEFAULT_TRANSFER_WIRE_RATE_BPS = 8000
MINIMUM_TRANSFER_WIRE_RATE_BPS = 1000
TRANSFER_RECOVERY_LIMIT = 3
THROUGHPUT_UART_BAUD = 1000000
CYSPP_PARAMETER_FIELDS = (
    'enable', 'role', 'company', 'local_key', 'remote_key', 'remote_mask',
    'sleep_level', 'server_security', 'client_flags',
)


class HilRun:
    def __init__(self, pin_mode: str, transfer_bytes: int,
                 transfer_wire_rate_bps: int, transfer_protocol: str,
                 transfer_window: int, transfer_retries: int):
        self.data = {
            'run_id': datetime.now().strftime('%Y-%m-%d_%H-%M-%S'),
            'pin_mode': pin_mode,
            'pin_level': None,
            'if820_mac': None,
            'if820_module_firmware': None,
            'ble_device_name': None,
            'cyspp_parameters': None,
            'uart_configuration': {
                'baud': 115200,
                'rts_cts': False,
            },
            'uart_receive_mode': None,
            'events': [],
            'mcu_to_app': [],
            'app_to_mcu': [],
            'connection': {
                'attempts': 0,
                'successful_connections': 0,
                'disconnects': 0,
                'errors': 0,
                'unexpected_disconnects': 0,
            },
            'transfer': None if transfer_bytes == 0 else {
                'requested_payload_bytes': transfer_bytes,
                'protocol': transfer_protocol,
                'window_size': transfer_window,
                'initial_window_size': transfer_window,
                'retry_limit': transfer_retries,
                'target_wire_rate_bps': transfer_wire_rate_bps,
                'initial_wire_rate_bps': transfer_wire_rate_bps,
                'started_at': None,
                'completed_at': None,
                'duration_seconds': None,
                'write_without_response_limit': None,
                'frames_sent': 0,
                'wire_bytes_sent': 0,
                'uart_bytes_received': 0,
                'frames_received': 0,
                'lost_frames': 0,
                'lost_payload_bytes': 0,
                'unconfirmed_frames': 0,
                'unconfirmed_payload_bytes': 0,
                'corrupted_frames': 0,
                'corrupted_payload_bytes': 0,
                'duplicate_frames': 0,
                'unexpected_uart_bytes': 0,
                'write_errors': 0,
                'acks_sent': 0,
                'acks_received': 0,
                'ack_timeouts': 0,
                'retransmissions': 0,
                'recovery_actions': 0,
                'status': 'not_started',
            },
            'result': 'incomplete',
        }

    def event(self, message: str) -> None:
        self.data['events'].append({
            'time': datetime.now().astimezone().isoformat(timespec='milliseconds'),
            'message': message,
        })
        logging.info('[HIL] %s', message)

    def write_report(self) -> Path:
        artifact_dir = Path(os.environ.get(
            'BLUETOOTH_TEST_REPORT_DIR',
            Path(__file__).resolve().parents[1] / 'Testreports' / 'direct'))
        artifact_dir.mkdir(exist_ok=True)
        report = artifact_dir / f"cyspp-{self.data['run_id']}-{self.data['pin_mode']}.json"
        report.write_text(json.dumps(self.data, indent=2), encoding='ascii')
        return report

    def write_transfer_report(self) -> Path | None:
        transfer = self.data['transfer']
        if transfer is None:
            return None
        artifact_dir = Path(os.environ.get(
            'BLUETOOTH_TEST_REPORT_DIR',
            Path(__file__).resolve().parents[1] / 'Testreports' / 'direct'))
        artifact_dir.mkdir(exist_ok=True)
        report = artifact_dir / (
            f"cyspp-transfer-{self.data['run_id']}-{self.data['pin_mode']}.md")
        verified_bytes = (transfer['requested_payload_bytes'] -
                          transfer['lost_payload_bytes'] -
                          transfer['corrupted_payload_bytes'])
        duration = transfer['duration_seconds']
        throughput = verified_bytes / duration if duration else 0
        connection = self.data['connection']
        lines = [
            '# CYSPP 40 kB Transfer Report',
            '',
            f"- Result: **{self.data['result']}** (`{transfer['status']}`)",
            f"- IF820: `{self.data['ble_device_name']}` ({self.data['if820_mac']})",
            f"- Pin: `{self.data['pin_mode']}`, sampled level `{self.data['pin_level']}`",
            f"- Transfer protocol: `{transfer['protocol']}`",
            f"- ACK window / retry limit: `{transfer['window_size']}` / "
            f"`{transfer['retry_limit']}`",
            f"- Initial ACK window / wire rate: `{transfer['initial_window_size']}` / "
            f"`{transfer['initial_wire_rate_bps']}` B/s",
            f"- Target wire rate: `{transfer['target_wire_rate_bps']}` B/s "
            '(0 means unpaced)',
            f"- Measured duration: `{duration}` s",
            f"- Verified payload throughput: `{throughput:.1f}` B/s",
            '',
            '## Connection',
            '',
            '| Attempts | Successful | Disconnects | Unexpected disconnects | Errors |',
            '| ---: | ---: | ---: | ---: | ---: |',
            f"| {connection['attempts']} | {connection['successful_connections']} | "
            f"{connection['disconnects']} | {connection['unexpected_disconnects']} | "
            f"{connection['errors']} |",
            '',
            '## Integrity',
            '',
            '| Requested | UART bytes received | Verified | Unconfirmed | Lost at UART | Corrupt |',
            '| ---: | ---: | ---: | ---: | ---: | ---: |',
            f"| {transfer['requested_payload_bytes']} | {transfer['uart_bytes_received']} | "
            f"{verified_bytes} | {transfer['unconfirmed_payload_bytes']} | "
            f"{transfer['lost_payload_bytes']} | "
            f"{transfer['corrupted_payload_bytes']} |",
            '',
            '```mermaid',
            'pie showData',
            '  title Payload integrity (bytes)',
            f'  "Verified" : {verified_bytes}',
            f'  "Lost" : {transfer["lost_payload_bytes"]}',
            f'  "Corrupt" : {transfer["corrupted_payload_bytes"]}',
            '```',
            '',
            '| Frames sent | Frames verified | ACKs sent/received | Retransmits | ACK timeouts | Recoveries |',
            '| ---: | ---: | ---: | ---: | ---: | ---: |',
            f"| {transfer['frames_sent']} | {transfer['frames_received']} | "
            f"{transfer['acks_sent']}/{transfer['acks_received']} | "
            f"{transfer['retransmissions']} | {transfer['ack_timeouts']} | "
            f"{transfer['recovery_actions']} |",
            '',
            'CRC-32 and the expected frame payload are checked for each received frame.',
        ]
        report.write_text('\n'.join(lines), encoding='ascii')
        return report


def check_response(request: str, response) -> None:
    If820Board.check_if820_response(request, response)


def mac_to_string(address) -> str:
    return ':'.join(f'{byte:02X}' for byte in reversed(address))


def set_cyspp_pin(board: If820Board, pin_mode: str, run: HilRun) -> None:
    if pin_mode == 'input':
        board.probe.gpio_to_input(board.CYSPP)
    else:
        board.probe.gpio_to_output(board.CYSPP)
        if pin_mode == 'low':
            board.probe.gpio_to_output_low(board.CYSPP)
        else:
            board.probe.gpio_to_output_high(board.CYSPP)
    time.sleep(0.05)
    run.data['pin_level'] = board.probe.gpio_read(board.CYSPP)
    run.event(f'CYSPP pin={pin_mode}, sampled level={run.data["pin_level"]}.')


def record_cyspp_parameters(board: If820Board, run: HilRun) -> None:
    command = board.p_uart.CMD_P_CYSPP_GET_PARAMETERS
    try:
        response = board.p_uart.send_and_wait(command, consumeecho=0, rxtimeout=2)
        check_response(command, response)
        payload = response[1].payload
        run.data['cyspp_parameters'] = {
            name: getattr(payload, name) for name in CYSPP_PARAMETER_FIELDS
        }
        run.event(f'CYSPP parameters: {run.data["cyspp_parameters"]}.')
    except Exception as error:
        run.event(f'CYSPP parameter query unavailable: {error}')


def record_firmware_version(board: If820Board, run: HilRun) -> None:
    command = board.p_uart.CMD_QUERY_FW
    try:
        response = board.p_uart.send_and_wait(command, consumeecho=0, rxtimeout=2)
        check_response(command, response)
        payload = response[1].payload
        app = payload.app
        firmware = {
            'application': '.'.join(str((app >> shift) & 0xff)
                                    for shift in (24, 16, 8, 0)),
            'application_raw': f'0x{app:08X}',
            'stack_raw': f'0x{payload.stack:08X}',
            'protocol': payload.protocol,
            'hardware': payload.hardware,
        }
        run.data['if820_module_firmware'] = firmware
        run.event(f'IF820 firmware: {firmware}.')
    except Exception as error:
        run.event(f'IF820 firmware query unavailable: {error}')


def enable_uart_flow_control(board: If820Board, run: HilRun) -> None:
    command = board.p_uart.CMD_SET_UART_PARAMS
    response = board.p_uart.send_and_wait(
        command, baud=THROUGHPUT_UART_BAUD, autobaud=0, autocorrect=0,
        flow=1, databits=8, parity=0, stopbits=1, uart_type=0)
    check_response(command, response)
    board.reconfig_puart(THROUGHPUT_UART_BAUD, flow_control=True)
    board.p_uart.port.write_timeout = 5
    time.sleep(1.0)
    response = board.p_uart.send_and_wait(board.p_uart.CMD_PING)
    check_response(board.p_uart.CMD_PING, response)
    run.data['uart_configuration'] = {
        'baud': THROUGHPUT_UART_BAUD,
        'rts_cts': True,
    }
    run.event(f'PUART set to {THROUGHPUT_UART_BAUD} baud with RTS/CTS.')


def build_transfer_frames(payload_bytes: int, write_limit: int) -> tuple[list[bytes], dict[int, bytes]]:
    payload_limit = write_limit - TRANSFER_HEADER.size
    if payload_limit < 1:
        raise ValueError(f'Write-without-response limit {write_limit} is too small.')
    frames = []
    expected_payloads = {}
    offset = 0
    sequence = 0
    while offset < payload_bytes:
        size = min(payload_limit, payload_bytes - offset)
        payload = bytes((offset + index) % 256 for index in range(size))
        checksum = zlib.crc32(payload) & 0xffffffff
        frames.append(TRANSFER_HEADER.pack(TRANSFER_MAGIC, sequence, size, checksum) + payload)
        expected_payloads[sequence] = payload
        offset += size
        sequence += 1
    return frames, expected_payloads


def transfer_receive_timeout_seconds(transfer: dict) -> float:
    target_rate = transfer['target_wire_rate_bps']
    if target_rate == 0:
        return TRANSFER_RECEIVE_TIMEOUT_MIN_SECONDS
    expected_send_seconds = transfer['wire_bytes_sent'] / target_rate
    return max(TRANSFER_RECEIVE_TIMEOUT_MIN_SECONDS,
               expected_send_seconds + TRANSFER_RECEIVE_TIMEOUT_MARGIN_SECONDS)


def assess_transfer(received: bytes, expected_payloads: dict[int, bytes],
                    transfer: dict) -> None:
    valid_sequences: set[int] = set()
    corrupt_sequences: set[int] = set()
    buffer = bytearray(received)
    while buffer:
        magic_index = buffer.find(TRANSFER_MAGIC)
        if magic_index < 0:
            transfer['unexpected_uart_bytes'] += len(buffer)
            break
        if magic_index:
            transfer['unexpected_uart_bytes'] += magic_index
            del buffer[:magic_index]
        if len(buffer) < TRANSFER_HEADER.size:
            transfer['unexpected_uart_bytes'] += len(buffer)
            break
        _, sequence, length, checksum = TRANSFER_HEADER.unpack_from(buffer)
        expected_payload = expected_payloads.get(sequence)
        if expected_payload is None or length != len(expected_payload):
            transfer['corrupted_frames'] += 1
            del buffer[0]
            continue
        frame_size = TRANSFER_HEADER.size + length
        if len(buffer) < frame_size:
            transfer['unexpected_uart_bytes'] += len(buffer)
            break
        payload = bytes(buffer[TRANSFER_HEADER.size:frame_size])
        del buffer[:frame_size]
        if zlib.crc32(payload) & 0xffffffff != checksum or payload != expected_payload:
            corrupt_sequences.add(sequence)
            continue
        if sequence in valid_sequences:
            transfer['duplicate_frames'] += 1
            continue
        valid_sequences.add(sequence)

    transfer['frames_received'] = len(valid_sequences)
    transfer['corrupted_frames'] += len(corrupt_sequences)
    transfer['corrupted_payload_bytes'] = sum(
        len(expected_payloads[sequence]) for sequence in corrupt_sequences)
    missing_sequences = set(expected_payloads) - valid_sequences - corrupt_sequences
    transfer['lost_frames'] = len(missing_sequences)
    transfer['lost_payload_bytes'] = sum(
        len(expected_payloads[sequence]) for sequence in missing_sequences)
    transfer['unconfirmed_frames'] = len(missing_sequences)
    transfer['unconfirmed_payload_bytes'] = transfer['lost_payload_bytes']


def acknowledge_received_frames(board: If820Board, buffer: bytearray,
                                expected_payloads: dict[int, bytes],
                                acknowledged_sequences: set[int], run: HilRun) -> None:
    transfer = run.data['transfer']
    if transfer is None or transfer['protocol'] != 'ack':
        return
    while True:
        magic_index = buffer.find(TRANSFER_MAGIC)
        if magic_index < 0:
            buffer.clear()
            return
        if magic_index:
            del buffer[:magic_index]
        if len(buffer) < TRANSFER_HEADER.size:
            return
        _, sequence, length, checksum = TRANSFER_HEADER.unpack_from(buffer)
        expected_payload = expected_payloads.get(sequence)
        if expected_payload is None or length != len(expected_payload):
            del buffer[0]
            continue
        frame_size = TRANSFER_HEADER.size + length
        if len(buffer) < frame_size:
            return
        payload = bytes(buffer[TRANSFER_HEADER.size:frame_size])
        del buffer[:frame_size]
        if (zlib.crc32(payload) & 0xffffffff == checksum and
                payload == expected_payload):
            board.p_uart.send(TRANSFER_ACK.pack(TRANSFER_ACK_MAGIC, sequence))
            transfer['acks_sent'] += 1
            acknowledged_sequences.add(sequence)


def receive_transfer(board: If820Board, transfer_started: threading.Event,
                     transfer_send_complete: threading.Event,
                     receiver_ready: threading.Event, receiver_complete: threading.Event,
                     expected_payloads: list[dict[int, bytes] | None], run: HilRun) -> None:
    transfer = run.data['transfer']
    if transfer is None:
        return
    board.p_uart.clear_rx_queue()
    receiver_ready.set()
    if not transfer_started.wait(15):
        transfer['status'] = 'not_started'
        receiver_complete.set()
        return
    expected = expected_payloads[0]
    if expected is None:
        transfer['status'] = 'failed'
        receiver_complete.set()
        return
    received = bytearray()
    acknowledgement_buffer = bytearray()
    acknowledged_sequences: set[int] = set()
    run.event('Transfer UART receiver active until the sender completes.')
    last_data_time = time.monotonic()
    received_wire_bytes = sum(TRANSFER_HEADER.size + len(payload)
                              for payload in expected.values())
    last_progress_percent = -1
    while True:
        chunk = board.p_uart.read()
        if chunk:
            received.extend(chunk)
            acknowledgement_buffer.extend(chunk)
            acknowledge_received_frames(
                board, acknowledgement_buffer, expected, acknowledged_sequences, run)
            last_data_time = time.monotonic()
        elif transfer_send_complete.is_set() and (
                time.monotonic() - last_data_time >= TRANSFER_QUIET_TIME_SECONDS):
            break
        progress_percent = min(100, int(100 * len(received) / received_wire_bytes))
        if progress_percent > last_progress_percent:
            run.event(
                f"Transfer receiving: {progress_percent}% "
                f"({len(received)}/{received_wire_bytes} UART wire bytes); "
                f"sender complete={transfer_send_complete.is_set()}.")
            last_progress_percent = progress_percent
        time.sleep(0.01)
    transfer['uart_bytes_received'] = len(received)
    assess_transfer(bytes(received), expected, transfer)
    transfer['completed_at'] = datetime.now().astimezone().isoformat(timespec='milliseconds')
    if transfer['started_at'] is not None:
        transfer['duration_seconds'] = round(time.monotonic() - transfer['_started_monotonic'], 3)
    transfer['status'] = 'passed' if (
        transfer['lost_frames'] == 0 and transfer['corrupted_frames'] == 0 and
        transfer['write_errors'] == 0) else 'failed'
    run.event(
        f"Transfer {transfer['status']}: {transfer['uart_bytes_received']} UART bytes, "
        f"{transfer['lost_payload_bytes']} lost payload bytes, "
        f"{transfer['corrupted_payload_bytes']} corrupt payload bytes.")
    receiver_complete.set()


def prepare_if820(board: If820Board, reset_module: bool, cccd_written: threading.Event,
                  transfer_started: threading.Event, transfer_send_complete: threading.Event,
                  receiver_ready: threading.Event, receiver_complete: threading.Event,
                  expected_payloads: list[dict[int, bytes] | None], run: HilRun,
                  received_from_pc: list[bytes], app_write_ready: threading.Event,
                  app_data_written: threading.Event) -> None:
    try:
        if reset_module:
            run.event('Factory reset requested.')
            board.p_uart.clear_rx_queue()
            board.p_uart.send(b'/RFAC\r\n')
            command = board.p_uart.EVENT_SYSTEM_BOOT
            response = board.p_uart.wait_event(command)
            check_response(command, response)
            run.data['if820_mac'] = mac_to_string(response[1].payload.address)
            command = board.p_uart.EVENT_GAP_ADV_STATE_CHANGED
            check_response(command, board.p_uart.wait_event(command, rxtimeout=15))
        else:
            run.event('Using existing CYSPP configuration without reset.')

        run.event(f'IF820 address: {run.data["if820_mac"]}.')

        command = board.p_uart.EVENT_GAP_CONNECTED
        check_response(command, board.p_uart.wait_event(command, rxtimeout=60))
        run.event('BLE connected.')
        deadline = time.monotonic() + 30
        while True:
            command = board.p_uart.EVENT_GATTS_DATA_WRITTEN
            response = board.p_uart.wait_event(
                command, rxtimeout=max(1, deadline - time.monotonic()))
            check_response(command, response)
            data = bytes(response[1].payload.data)
            if data == b'\x01\x00':
                break
            run.event(f'Ignoring non-CYSPP GATT write {data.hex()}.')
            if time.monotonic() >= deadline:
                raise TimeoutError('Timed out waiting for CYSPP notification CCCD write.')
        run.event('CYSPP notification CCCD write (0100) observed.')
        cccd_written.set()

        board.p_uart.send(MCU_PROMPT)
        run.event(f'MCU simulator sent {MCU_PROMPT!r}.')
        if run.data['transfer'] is not None:
            receive_transfer(board, transfer_started, transfer_send_complete,
                             receiver_ready, receiver_complete, expected_payloads, run)
            return
        board.p_uart.clear_rx_queue()
        app_write_ready.set()
        if not app_data_written.wait(15):
            run.data['uart_receive_mode'] = 'app_write_timeout'
            run.event('Timed out waiting for the app simulator data write.')
            return
        time.sleep(1.0)
        received = board.p_uart.read()
        if received:
            payload = bytes(received)
            received_from_pc.append(payload)
            if payload.startswith(b'@E,'):
                run.data['uart_receive_mode'] = 'ezserial_gatt_event'
                run.event(f'MCU simulator received EZ-Serial GATT event {payload!r}.')
            else:
                run.data['uart_receive_mode'] = 'transparent_data'
                run.event(f'MCU simulator received transparent data {payload!r}.')
        else:
            run.data['uart_receive_mode'] = 'no_uart_data'
            run.event('MCU simulator received no UART data.')
    except Exception as error:
        run.data['connection']['errors'] += 1
        run.event(f'IF820-side error: {error}')
        cccd_written.set()
        receiver_complete.set()


async def find_if820(address: str) -> BLEDevice:
    deadline = time.time() + 45
    while time.time() < deadline:
        for device in await BleakScanner.discover(timeout=5.0):
            if device.address.upper() == address:
                return device
        logging.info('[APP] IF820 not found yet; scanning again.')
    raise TimeoutError(f'IF820 {address} was not discovered.')


async def run_hil_test(board: If820Board, reset_module: bool, pin_mode: str,
                       boot_address, use_uart_flow_control: bool, run: HilRun) -> None:
    cccd_written = threading.Event()
    transfer_started = threading.Event()
    transfer_send_complete = threading.Event()
    receiver_ready = threading.Event()
    receiver_complete = threading.Event()
    app_write_ready = threading.Event()
    app_data_written = threading.Event()
    expected_payloads: list[dict[int, bytes] | None] = [None]
    received_from_pc: list[bytes] = []
    received_from_if820: list[bytes] = []
    acknowledgements: asyncio.Queue[int] = asyncio.Queue()
    acknowledgement_buffer = bytearray()
    if not reset_module:
        run.data['if820_mac'] = mac_to_string(boot_address)
    board.probe.gpio_to_input(board.CYSPP)
    if use_uart_flow_control:
        try:
            enable_uart_flow_control(board, run)
        except Exception as error:
            run.data['result'] = 'flow_control_setup_failed'
            run.event(f'UART flow-control setup failed: {error}')
            return
            record_firmware_version(board, run)
    record_cyspp_parameters(board, run)
    set_cyspp_pin(board, pin_mode, run)
    threading.Thread(target=prepare_if820,
                   args=(board, reset_module, cccd_written, transfer_started,
                       transfer_send_complete, receiver_ready, receiver_complete,
                       expected_payloads, run, received_from_pc, app_write_ready,
                       app_data_written),
                     daemon=True).start()

    deadline = time.time() + 30
    while run.data['if820_mac'] is None and time.time() < deadline:
        await asyncio.sleep(0.1)
    if run.data['if820_mac'] is None:
        run.data['result'] = 'failed_before_scan'
        run.event('Timed out waiting for IF820 address.')
        return

    device = await find_if820(run.data['if820_mac'])
    run.data['ble_device_name'] = device.name
    run.event(f'App simulator found {device.address} as {device.name!r}.')

    def on_disconnect(_: BleakClient) -> None:
        connection = run.data['connection']
        connection['disconnects'] += 1
        transfer = run.data['transfer']
        if transfer is not None and transfer['status'] == 'sending':
            connection['unexpected_disconnects'] += 1
        run.event('BLE disconnected.')

    def on_notification(_: BleakGATTCharacteristic, data: bytearray) -> None:
        received = bytes(data)
        received_from_if820.append(received)
        run.data['mcu_to_app'].append(received.hex())
        acknowledgement_buffer.extend(received)
        acknowledged = False
        while len(acknowledgement_buffer) >= TRANSFER_ACK.size:
            magic_index = acknowledgement_buffer.find(TRANSFER_ACK_MAGIC)
            if magic_index < 0:
                acknowledgement_buffer.clear()
                break
            if magic_index:
                del acknowledgement_buffer[:magic_index]
            if len(acknowledgement_buffer) < TRANSFER_ACK.size:
                break
            _, sequence = TRANSFER_ACK.unpack_from(acknowledgement_buffer)
            del acknowledgement_buffer[:TRANSFER_ACK.size]
            acknowledgements.put_nowait(sequence)
            acknowledged = True
        if not acknowledged and not received.startswith(TRANSFER_ACK_MAGIC):
            run.event(f'App simulator received {received!r}.')

    try:
        run.data['connection']['attempts'] += 1
        async with BleakClient(device, disconnected_callback=on_disconnect) as client:
            run.data['connection']['successful_connections'] += 1
            characteristic = client.services.get_characteristic(CYSPP_DATA_UUID)
            if characteristic is None:
                run.data['result'] = 'cyspp_characteristic_not_found'
                run.event('CYSPP data characteristic ...ca102 was not found.')
                return
            run.event('Enabling notifications on CYSPP ...ca102.')
            await client.start_notify(characteristic, on_notification)
            if not await asyncio.to_thread(cccd_written.wait, 15):
                run.data['result'] = 'cccd_event_timeout'
                run.event(
                    'IF820 did not report the CYSPP CCCD write within 15 seconds; '
                    'not sending application data.')
                return
            await asyncio.sleep(1.0)
            if not client.is_connected:
                run.data['result'] = 'disconnected_after_subscribe'
                run.event('BLE link closed after notification enable.')
                return
            if run.data['transfer'] is None:
                if not await asyncio.to_thread(app_write_ready.wait, 10):
                    run.data['result'] = 'uart_receiver_not_ready'
                    run.event('IF820 UART receiver was not ready for application data.')
                    return
                run.event(f'App simulator writes {APP_DATA!r}.')
                await client.write_gatt_char(characteristic, APP_DATA, response=False)
                app_data_written.set()
                await asyncio.sleep(2.0)
            else:
                prompt_deadline = time.monotonic() + 15
                while not any(MCU_PROMPT in packet for packet in received_from_if820):
                    if time.monotonic() >= prompt_deadline or not client.is_connected:
                        run.data['result'] = 'transfer_start_failed'
                        run.event('MCU prompt was not received before the transfer.')
                        return
                    await asyncio.sleep(0.05)
                if not await asyncio.to_thread(receiver_ready.wait, 10):
                    run.data['result'] = 'transfer_start_failed'
                    run.event('MCU UART receiver was not ready for the transfer.')
                    return
                transfer = run.data['transfer']
                write_limit = min(244, int(characteristic.max_write_without_response_size))
                frames, expected = build_transfer_frames(
                    transfer['requested_payload_bytes'], write_limit)
                expected_payloads[0] = expected
                transfer['write_without_response_limit'] = write_limit
                transfer['frames_sent'] = len(frames)
                transfer['wire_bytes_sent'] = sum(len(frame) for frame in frames)
                transfer['started_at'] = datetime.now().astimezone().isoformat(timespec='milliseconds')
                transfer['_started_monotonic'] = time.monotonic()
                transfer['status'] = 'sending'
                transfer_started.set()
                run.event(
                    f"Sending {transfer['requested_payload_bytes']} payload bytes in "
                    f"{len(frames)} frames of up to {write_limit} bytes at "
                    f"{transfer['target_wire_rate_bps']} B/s using "
                    f"{transfer['protocol']} protocol.")
                try:
                    sent_wire_bytes = 0
                    last_progress_percent = -1
                    next_frame_index = 0
                    acknowledged_sequences: set[int] = set()
                    pending: dict[int, dict] = {}
                    while len(acknowledged_sequences) < len(frames):
                        while (next_frame_index < len(frames) and
                               (transfer['protocol'] == 'burst' or
                                len(pending) < transfer['window_size'])):
                            frame = frames[next_frame_index]
                            sequence = next_frame_index
                            await client.write_gatt_char(characteristic, frame, response=False)
                            pending[sequence] = {
                                'frame': frame,
                                'sent_at': time.monotonic(),
                                'attempts': 1,
                            }
                            next_frame_index += 1
                            sent_wire_bytes += len(frame)
                            if transfer['protocol'] == 'burst':
                                continue
                            target_rate = transfer['target_wire_rate_bps']
                            if target_rate:
                                next_frame_time = (
                                    transfer['_started_monotonic'] +
                                    sent_wire_bytes / target_rate)
                                delay = next_frame_time - time.monotonic()
                                if delay > 0:
                                    await asyncio.sleep(delay)

                        if transfer['protocol'] == 'burst':
                            acknowledged_sequences = set(range(len(frames)))
                            break

                        try:
                            acknowledged_sequence = await asyncio.wait_for(
                                acknowledgements.get(), timeout=0.05)
                            if acknowledged_sequence in pending:
                                del pending[acknowledged_sequence]
                                acknowledged_sequences.add(acknowledged_sequence)
                                transfer['acks_received'] += 1
                            else:
                                run.event(f'Ignoring duplicate ACK for frame {acknowledged_sequence}.')
                        except TimeoutError:
                            pass

                        now = time.monotonic()
                        exhausted_sequences = [
                            sequence for sequence, state in pending.items()
                            if now - state['sent_at'] >= TRANSFER_ACK_TIMEOUT_SECONDS and
                            state['attempts'] > transfer['retry_limit']
                        ]
                        if exhausted_sequences:
                            if transfer['recovery_actions'] >= TRANSFER_RECOVERY_LIMIT:
                                raise TimeoutError(
                                    f'Frames {exhausted_sequences} exceeded '
                                    f'{transfer["retry_limit"]} retries after '
                                    f'{TRANSFER_RECOVERY_LIMIT} recoveries.')
                            transfer['recovery_actions'] += 1
                            transfer['window_size'] = 1
                            if transfer['target_wire_rate_bps']:
                                transfer['target_wire_rate_bps'] = max(
                                    MINIMUM_TRANSFER_WIRE_RATE_BPS,
                                    transfer['target_wire_rate_bps'] // 2)
                            for state in pending.values():
                                state['attempts'] = 0
                                state['sent_at'] = 0.0
                            run.event(
                                f"ACK recovery {transfer['recovery_actions']}/"
                                f"{TRANSFER_RECOVERY_LIMIT}: window reduced to 1, "
                                f"wire rate reduced to "
                                f"{transfer['target_wire_rate_bps']} B/s; retrying "
                                f"{len(pending)} pending frames.")

                        for sequence, state in list(pending.items()):
                            if now - state['sent_at'] < TRANSFER_ACK_TIMEOUT_SECONDS:
                                continue
                            transfer['ack_timeouts'] += 1
                            if transfer['recovery_actions'] == 0:
                                transfer['recovery_actions'] = 1
                                transfer['window_size'] = 1
                                if transfer['target_wire_rate_bps']:
                                    transfer['target_wire_rate_bps'] = max(
                                        MINIMUM_TRANSFER_WIRE_RATE_BPS,
                                        transfer['target_wire_rate_bps'] // 2)
                                run.event(
                                    f"ACK backpressure detected: window reduced to 1, "
                                    f"wire rate reduced to "
                                    f"{transfer['target_wire_rate_bps']} B/s.")
                            await client.write_gatt_char(
                                characteristic, state['frame'], response=False)
                            state['attempts'] += 1
                            state['sent_at'] = time.monotonic()
                            transfer['retransmissions'] += 1
                            run.event(
                                f'Retransmitting frame {sequence}, attempt {state["attempts"]}.')

                        elapsed = time.monotonic() - transfer['_started_monotonic']
                        completed = len(acknowledged_sequences)
                        progress_percent = int(100 * completed / len(frames))
                        if progress_percent > last_progress_percent:
                            rate = sent_wire_bytes / elapsed if elapsed else 0
                            run.event(
                                f"Transfer sending: {completed}/{len(frames)} ACKed "
                                f"({progress_percent}%), {len(pending)} pending, "
                                f"{sent_wire_bytes}/{transfer['wire_bytes_sent']} wire bytes, "
                                f"{rate:.0f} B/s.")
                            last_progress_percent = progress_percent
                except Exception:
                    transfer['write_errors'] += 1
                    raise
                finally:
                    transfer_send_complete.set()
                transfer['status'] = 'receiving'
                run.event('Transfer sender complete; waiting for UART to become idle.')
                await asyncio.to_thread(receiver_complete.wait,
                                        transfer_receive_timeout_seconds(transfer) + 2)
            await client.stop_notify(characteristic)
    except Exception as error:
        run.data['connection']['errors'] += 1
        transfer = run.data['transfer']
        if transfer is not None:
            transfer['status'] = 'ble_error'
            transfer_send_complete.set()
        run.data['result'] = 'ble_error'
        run.event(f'BLE-side error: {error}')
        return

    run.data['app_to_mcu'] = [packet.hex() for packet in received_from_pc]
    prompt_received = any(MCU_PROMPT in packet for packet in received_from_if820)
    data_received = any(APP_DATA in packet for packet in received_from_pc)
    transfer = run.data['transfer']
    if transfer is not None:
        run.data['result'] = 'passed' if (
            prompt_received and transfer['status'] == 'passed' and
            run.data['connection']['unexpected_disconnects'] == 0) else 'transfer_failed'
    else:
        run.data['result'] = 'passed' if prompt_received and data_received else 'data_failed'
    run.event(f'Result: {run.data["result"]}.')


def ask_pin_mode() -> str | None:
    """Select the simulated MCU CYSPP pin state for an interactive run."""
    pin_mode: list[str | None] = [None]
    window = tk.Tk()
    window.title('IF820 CYSPP HIL Test')
    window.resizable(False, False)
    window.attributes('-topmost', True)

    frame = tk.Frame(window, padx=20, pady=20)
    frame.pack()
    tk.Label(frame, text='CYSPP-Pin beim BLE-Subscribe:',
             font=('Segoe UI', 12, 'bold')).pack(pady=(0, 15))

    def choose(mode: str) -> None:
        pin_mode[0] = mode
        window.destroy()

    tk.Button(frame, text='LOW\n(Datenmodus erwartet)',
              command=lambda: choose('low'), width=37, height=2,
              font=('Segoe UI', 10)).pack(pady=4)
    tk.Button(frame, text='HIGH\n(Disconnect reproduzieren)',
              command=lambda: choose('high'), width=37, height=2,
              font=('Segoe UI', 10)).pack(pady=4)
    tk.Button(frame, text='Input / Floating\n(Leitung freigeben)',
              command=lambda: choose('input'), width=37, height=2,
              font=('Segoe UI', 10)).pack(pady=4)
    window.protocol('WM_DELETE_WINDOW', window.destroy)
    window.mainloop()
    return pin_mode[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pin', choices=('low', 'high', 'input'))
    parser.add_argument('--new-module', action='store_true',
                        help='Factory-reset the IF820 before testing.')
    parser.add_argument('--transfer-bytes', type=int, default=0,
                        help='Send this many framed payload bytes after CYSPP connects.')
    parser.add_argument('--transfer-rate', type=int, default=DEFAULT_TRANSFER_WIRE_RATE_BPS,
                        help='Maximum BLE wire bytes per second; use 0 for an unpaced burst.')
    parser.add_argument('--transfer-protocol', choices=('ack', 'burst'), default='ack',
                        help='Use end-to-end ACK credits or a non-acknowledged burst.')
    parser.add_argument('--transfer-window', type=int, default=4,
                        help='Maximum unacknowledged frames when using ACK protocol.')
    parser.add_argument('--transfer-retries', type=int, default=3,
                        help='Retransmissions allowed per frame after an ACK timeout.')
    parser.add_argument('--uart-flow-control', action='store_true',
                        help='Use 1 Mbaud PUART with RTS/CTS for transfer testing.')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    logging.basicConfig(format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
                         level=logging.DEBUG if args.debug else logging.WARNING)

    pin_mode = args.pin or ask_pin_mode()
    if pin_mode is None:
        logging.info('HIL test cancelled.')
        return 0
    if args.transfer_bytes < 0 or args.transfer_rate < 0:
        parser.error('--transfer-bytes and --transfer-rate must be zero or positive integers.')
    if args.transfer_window < 1 or args.transfer_retries < 0:
        parser.error('--transfer-window must be positive and --transfer-retries non-negative.')

    run = HilRun(pin_mode, args.transfer_bytes, args.transfer_rate,
                 args.transfer_protocol, args.transfer_window, args.transfer_retries)
    boards = If820Board.get_connected_boards()
    if not boards:
        logging.critical('No IF820 board found.')
        return 1
    board = boards[0]
    boot_event = board.open_and_soft_reset_board()
    try:
        uninitialize_sta()
        asyncio.run(run_hil_test(
            board, args.new_module, pin_mode, boot_event.payload.address,
            args.uart_flow_control, run))
    finally:
        board.probe.gpio_to_input(board.CYSPP)
        board.close_ports_and_reset(reset_probe=False)
        if run.data['transfer'] is not None:
            run.data['transfer'].pop('_started_monotonic', None)
        logging.info('HIL report: %s', run.write_report())
        transfer_report = run.write_transfer_report()
        if transfer_report is not None:
            logging.info('Transfer report: %s', transfer_report)
    return 0 if run.data['result'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())