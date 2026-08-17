#!/usr/bin/env python3

"""SPP PC-Test — Zwei Use-Cases für BT Classic SPP mit IF820.

USE-CASE 1: ERSTE VERBINDUNG
  - Factory Reset des IF820 → sauberer BT Classic Peripheral-Modus
  - MAC auslesen
  - Anleitung: Gerät in Windows koppeln
  - RFCOMM verbinden → Datentest

USE-CASE 2: WIEDERVERBINDUNG
  - Gerät war schon gekoppelt (z.B. vor einer Woche)
  - Kein Factory Reset — IF820 ist nach Boot bereits im BT Classic Modus
  - MAC auslesen → direkt RFCOMM verbinden → Datentest

Alles synchron, kein Background-Thread.
"""

import argparse
import json
import logging
import os
import socket
import time
import sys
import threading
from datetime import datetime
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parent / 'common_lib' / 'libraries'))
import EzSerialPort as ez_port
from If820Board import If820Board

SPP_DATA = b'abcdefghijklmnop'
OTA_LATENCY = 0.5
RFCOMM_TIMEOUT = 15.0
BOOT_EVENT_TIMEOUT = 10.0
PUART_BAUD = 115200
PUART_CHUNK_SIZE = 32
PUART_SAFE_BYTE_RATE = 1000
GAP_NAME_TYPE_BLE = 0
GAP_NAME_TYPE_BT_CLASSIC = 1
BLE_ADV_INTERVAL = 400
REPORT_DIRECTORY = Path(os.environ.get(
    'BLUETOOTH_TEST_REPORT_DIR',
    Path(__file__).resolve().parent / 'tests' / 'Bluetooth-Simulation' / 'Testreports' / 'direct'))


def _mac_list_to_str(mac_list) -> str:
    """Convert IF820 MAC address (list of bytes, LSB first) to colon-separated
    string (MSB first), as expected by the OS BT stack."""
    if isinstance(mac_list, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(mac_list))
    return str(mac_list)


def _device_names(mac: str) -> tuple[str, str]:
    address = mac.replace(':', '').upper()
    return f'IF820_{address}_BT', f'IF820_{address}_BLE'


# ─── TERMINAL INTERACTION ───────────────────────────────────────────────────

def ask_connection_mode() -> str:
    """Ask for the Windows pairing state without opening a GUI dialog."""
    choices = {'1': 'first', '2': 'reconnect'}
    while True:
        print('\nBT Classic pairing mode:')
        print('1. First pairing (device is not paired with this PC)')
        print('2. Reconnect (device was paired with this PC before)')
        answer = input('Select [1-2]: ').strip()
        if answer in choices:
            return choices[answer]
        print('Please select 1 or 2.')


def show_pairing_instructions(mac: str):
    """Print first-time Just Works pairing instructions and wait for confirmation."""
    classic_name, _ = _device_names(mac)
    print('\nWindows pairing required:')
    print('1. Open Windows Settings > Bluetooth & devices.')
    print('2. Select Add device > Bluetooth.')
    print(f'3. Select "{classic_name}" (MAC: {mac}).')
    print('4. Confirm the pairing request; no PIN or six-digit comparison is required.')
    print('Do not configure a Windows COM port for this device.')
    input('Press Enter after pairing has completed. ')



# ─── IF820 BOARD OPERATIONS ────────────────────────────────────────────────

def read_bt_mac(board: If820Board) -> str:
    """Liest die BT Classic MAC vom IF820 (NVM-stabil)."""
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(board.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    return _mac_list_to_str(ez_rsp[1].payload.address)


def configure_just_works_pairing(board: If820Board):
    """Configure headless SSP so Windows does not request numeric comparison."""
    security_cmd = 'ssbp,m=00,b=0,k=10,p=0,i=03,f=01\r'
    logging.info('[IF820] Configure Just Works pairing (NoInputNoOutput)...')
    board.p_uart.send(security_cmd.encode('ascii'))
    time.sleep(0.3)
    board.p_uart.clear_rx_queue()

    logging.info('[IF820] Speichere Config (NVM)...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_STORE_CONFIG)
    If820Board.check_if820_response(board.p_uart.CMD_STORE_CONFIG, ez_rsp)


def set_neutral_bt_device_class(board: If820Board):
    """Clear audio service and class flags from the Bluetooth Classic CoD."""
    logging.info('[IF820] Setze neutrale BT-Gerateklasse (kein Audio)...')
    board.p_uart.send(b'sbtdc,c=0000\r')
    time.sleep(0.3)
    board.p_uart.clear_rx_queue()


def configure_dual_mode_names(board: If820Board, mac: str):
    """Store separate Bluetooth Classic and BLE names derived from the MAC."""
    classic_name, ble_name = _device_names(mac)

    cmd = board.p_uart.CMD_GAP_STOP_ADV
    ez_rsp = board.p_uart.send_and_wait(cmd)
    If820Board.check_if820_response(cmd, ez_rsp)
    cmd = board.p_uart.EVENT_GAP_ADV_STATE_CHANGED
    ez_rsp = board.p_uart.wait_event(cmd)
    If820Board.check_if820_response(cmd, ez_rsp)

    for name_type, name in ((GAP_NAME_TYPE_BT_CLASSIC, classic_name),
                            (GAP_NAME_TYPE_BLE, ble_name)):
        cmd = board.p_uart.CMD_GAP_SET_DEVICE_NAME
        ez_rsp = board.p_uart.send_and_wait(cmd, type=name_type, name=name)
        If820Board.check_if820_response(cmd, ez_rsp)

    # Include the BLE name in the advertisement so it is visible to scanners.
    advertisement = [0x02, 0x01, 0x06, len(ble_name) + 1, 0x09]
    advertisement.extend(ble_name.encode('ascii'))
    cmd = board.p_uart.CMD_GAP_SET_ADV_DATA
    ez_rsp = board.p_uart.send_and_wait(cmd, data=advertisement)
    If820Board.check_if820_response(cmd, ez_rsp)

    cmd = board.p_uart.CMD_GAP_SET_ADV_PARAMETERS
    ez_rsp = board.p_uart.send_and_wait(
        cmd,
        mode=ez_port.GapAdvertMode.NA.value,
        type=ez_port.GapAdvertType.UNDIRECTED_HIGH_DUTY_CYCLE.value,
        channels=ez_port.GapAdvertChannels.CHANNEL_ALL.value,
        high_interval=BLE_ADV_INTERVAL,
        high_duration=0,
        low_interval=BLE_ADV_INTERVAL,
        low_duration=0,
        flags=ez_port.GapAdvertFlags.ALL.value,
        directAddr=[0, 0, 0, 0, 0, 0],
        directAddrType=ez_port.GapAddressType.PUBLIC.value)
    If820Board.check_if820_response(cmd, ez_rsp)

    cmd = board.p_uart.CMD_STORE_CONFIG
    ez_rsp = board.p_uart.send_and_wait(cmd)
    If820Board.check_if820_response(cmd, ez_rsp)

    cmd = board.p_uart.CMD_GAP_START_ADV
    ez_rsp = board.p_uart.send_and_wait(
        cmd,
        mode=ez_port.GapAdvertMode.NA.value,
        type=ez_port.GapAdvertType.UNDIRECTED_HIGH_DUTY_CYCLE.value,
        channels=ez_port.GapAdvertChannels.CHANNEL_ALL.value,
        high_interval=BLE_ADV_INTERVAL,
        high_duration=0,
        low_interval=BLE_ADV_INTERVAL,
        low_duration=0,
        flags=ez_port.GapAdvertFlags.ALL.value,
        directAddr=[0, 0, 0, 0, 0, 0],
        directAddrType=ez_port.GapAddressType.PUBLIC.value)
    If820Board.check_if820_response(cmd, ez_rsp)
    logging.info(f'[IF820] BT Classic name: {classic_name}')
    logging.info(f'[IF820] BLE name: {ble_name}')


def factory_reset(board: If820Board) -> str:
    """Factory reset, configure Just Works pairing, and return the Bluetooth address."""
    logging.info('[IF820] Factory Reset...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_FACTORY_RESET)
    If820Board.check_if820_response(board.p_uart.CMD_FACTORY_RESET, ez_rsp)

    logging.info('[IF820] Warte auf Reboot...')
    ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(board.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)
    time.sleep(0.5)
    board.p_uart.clear_rx_queue()

    configure_just_works_pairing(board)
    set_neutral_bt_device_class(board)

    mac = read_bt_mac(board)
    logging.info(f'[IF820] BT Classic MAC: {mac}')
    configure_dual_mode_names(board, mac)
    return mac


# ─── RFCOMM VERBINDUNG + DATENTEST ────────────────────────────────────────

def rfcomm_connect(mac: str) -> socket.socket:
    """Versucht RFCOMM-Verbindung auf SCN 1-8. Gibt Socket zurück oder None."""
    for scn in range(1, 9):
        s = socket.socket(socket.AF_BLUETOOTH,
                          socket.SOCK_STREAM,
                          socket.BTPROTO_RFCOMM)
        s.settimeout(RFCOMM_TIMEOUT)
        try:
            logging.info(f'[PC] RFCOMM SCN={scn} auf {mac}...')
            s.connect((mac, scn))
            logging.info(f'[PC] Verbunden auf SCN={scn}!')
            return s
        except (OSError, TimeoutError) as e:
            s.close()
            logging.debug(f'[PC] SCN {scn} fehlgeschlagen: {e}')
    return None


def wait_for_bt_connected(board: If820Board):
    """Wartet auf EVENT_BT_CONNECTED vom IF820."""
    logging.info('[IF820] Warte auf EVENT_BT_CONNECTED...')
    ez_rsp = board.p_uart.wait_event(
        board.p_uart.EVENT_BT_CONNECTED, rxtimeout=30)
    If820Board.check_if820_response(board.p_uart.EVENT_BT_CONNECTED, ez_rsp)
    logging.info('[IF820] BT Classic verbunden!')
    board.p_uart.clear_rx_queue()


def log_board_connection(board: If820Board, phase: str):
    """Log the UART and probe state at a board-initialization checkpoint."""
    serial_port = board.p_uart.port
    logging.info(
        '[DEBUG] %s: PUART=%s, baud=%s, flow-control=%s, serial-open=%s, '
        'probe-id=%s, probe-open=%s, initialized=%s, rx-queue=%r',
        phase,
        board.puart_port_name,
        getattr(serial_port, 'baudrate', None),
        getattr(serial_port, 'rtscts', None),
        getattr(serial_port, 'is_open', None),
        board.probe.id,
        board.probe.is_open,
        board.is_initialized,
        getattr(board.p_uart, '_rx_queue', None))


def wait_for_boot_event_with_diagnostics(board: If820Board):
    """Wait for boot without allowing a stalled packet parser to stall the test."""
    result = []
    completed = threading.Event()

    def wait_for_event():
        try:
            result.append(board.p_uart.wait_event(
                board.p_uart.EVENT_SYSTEM_BOOT, rxtimeout=1))
        except Exception as error:
            result.append(error)
        finally:
            completed.set()

    worker = threading.Thread(target=wait_for_event, daemon=True,
                              name='if820-boot-event-waiter')
    worker.start()
    board.probe.reset_target()
    deadline = time.monotonic() + BOOT_EVENT_TIMEOUT
    poll_number = 0
    while not completed.wait(timeout=1):
        poll_number += 1
        raw_bytes = bytes(board.p_uart.get_rx_queue())
        logging.info(
            '[DEBUG] boot wait %d: worker-alive=%s, rx-bytes=%d, '
            'rx-hex=%s, rx-ascii=%r',
            poll_number, worker.is_alive(), len(raw_bytes), raw_bytes.hex(' '),
            raw_bytes.decode('ascii', errors='replace'))
        if time.monotonic() >= deadline:
            log_board_connection(board, 'boot event timeout')
            raise RuntimeError(
                f'No {board.p_uart.EVENT_SYSTEM_BOOT} event received on '
                f'{board.puart_port_name} within {BOOT_EVENT_TIMEOUT:.1f} s; '
                f'parser-worker-alive={worker.is_alive()}. Check the reported '
                'raw UART bytes, IF820 power, the current-measurement jumper, '
                'boot-mode jumpers, and PUART routing.')

    if not result:
        raise RuntimeError('Boot-event worker completed without a result.')
    if isinstance(result[0], Exception):
        raise RuntimeError(f'Boot-event parser failed: {result[0]!r}') from result[0]
    return result[0]


def initialize_board_with_diagnostics(board: If820Board):
    """Reset the IF820 with bounded boot-event waiting and clear diagnostics."""
    logging.info('[DEBUG] Init phase 1/2: open PUART and probe; reset without waiting for boot.')
    board.open_and_init_board(False)
    log_board_connection(board, 'after phase 1 reset')

    logging.info('[DEBUG] Closing ports and rebooting DVK probe before phase 2.')
    board.close_ports_and_reset()
    logging.info('[DEBUG] Waiting 3 seconds for the DVK probe reboot.')
    time.sleep(3)

    logging.info('[DEBUG] Init phase 2/2: open PUART and probe.')
    board.open_and_init_board(False)
    log_board_connection(board, 'before phase 2 target reset')

    logging.info('[DEBUG] Resetting IF820 target; waiting up to %.1f s for %s.',
                 BOOT_EVENT_TIMEOUT, board.p_uart.EVENT_SYSTEM_BOOT)
    reset_started = time.monotonic()
    board.p_uart.clear_rx_queue()
    boot_event = wait_for_boot_event_with_diagnostics(board)
    elapsed = time.monotonic() - reset_started
    if boot_event[0] != 0:
        log_board_connection(board, f'boot event missing after {elapsed:.2f} s')
        raise RuntimeError(
            f'No {board.p_uart.EVENT_SYSTEM_BOOT} event received on '
            f'{board.puart_port_name} within {BOOT_EVENT_TIMEOUT:.1f} s; '
            f'response={boot_event!r}. Check IF820 power, the current-measurement '
            'jumper, boot-mode jumpers, and PUART routing.')
    If820Board.check_if820_response(board.p_uart.EVENT_SYSTEM_BOOT, boot_event)
    time.sleep(If820Board.BOOT_DELAY)
    board._is_initialized = True
    logging.info('[DEBUG] Boot event received after %.2f s: %r', elapsed, boot_event)
    log_board_connection(board, 'after successful phase 2 reset')
    return boot_event[1]


def read_uart_payload(board: If820Board, expected_size: int, timeout: float) -> bytes:
    """Collect transparent UART data until the expected payload is complete."""
    received = bytearray()
    deadline = time.monotonic() + timeout
    while len(received) < expected_size and time.monotonic() < deadline:
        chunk = board.p_uart.read()
        if chunk:
            received.extend(chunk)
        else:
            time.sleep(0.01)
    return bytes(received)


def read_socket_payload(sock: socket.socket, expected_size: int, timeout: float,
                        received: bytearray, completed: threading.Event) -> None:
    """Drain RFCOMM while the IF820 is writing, avoiding receive-buffer loss."""
    deadline = time.monotonic() + timeout
    try:
        sock.settimeout(0.2)
        while len(received) < expected_size and time.monotonic() < deadline:
            try:
                chunk = sock.recv(1024)
            except socket.timeout:
                continue
            if not chunk:
                break
            received.extend(chunk)
    finally:
        completed.set()


def send_uart_payload(board: If820Board, payload: bytes) -> None:
    for offset in range(0, len(payload), PUART_CHUNK_SIZE):
        chunk = payload[offset:offset + PUART_CHUNK_SIZE]
        board.p_uart.send(chunk)
        time.sleep(len(chunk) / PUART_SAFE_BYTE_RATE)


def run_data_test(board: If820Board, sock: socket.socket, payload: bytes):
    """Sendet/empfängt Testdaten über SPP. Gibt (pc_to_if820_ok, if820_to_pc_ok) zurück."""
    timeout = max(15.0, len(payload) / PUART_SAFE_BYTE_RATE + 15.0)
    # PC → IF820
    logging.info('[PC] Sende %d Bytes.', len(payload))
    board.p_uart.clear_rx_queue()
    sock.sendall(payload)

    rx_on_if820 = read_uart_payload(board, len(payload), timeout)
    logging.info('[IF820] Empfangen: %d/%d Bytes.', len(rx_on_if820), len(payload))

    # IF820 → PC
    logging.info('[IF820] Sende %d Bytes.', len(payload))
    received_on_pc = bytearray()
    socket_reader_done = threading.Event()
    socket_reader = threading.Thread(
        target=read_socket_payload,
        args=(sock, len(payload), timeout, received_on_pc, socket_reader_done),
        daemon=True)
    socket_reader.start()
    send_uart_payload(board, payload)
    socket_reader_done.wait(timeout)
    rx_on_pc = bytes(received_on_pc)
    logging.info('[PC] Empfangen: %d/%d Bytes.', len(rx_on_pc), len(payload))

    ok_pc_to_if820 = payload == rx_on_if820
    ok_if820_to_pc = payload == rx_on_pc
    return ok_pc_to_if820, ok_if820_to_pc, rx_on_if820, rx_on_pc


def write_classic_report(report: dict) -> tuple[Path, Path]:
    """Persist the BT Classic result in machine- and human-readable formats."""
    REPORT_DIRECTORY.mkdir(exist_ok=True)
    timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
    base_name = f'bt-classic-{timestamp}-{report["test"]}'
    json_path = REPORT_DIRECTORY / f'{base_name}.json'
    markdown_path = REPORT_DIRECTORY / f'{base_name}.md'
    json_path.write_text(json.dumps(report, indent=2), encoding='ascii')
    lines = [
        '# BT Classic Test Report',
        '',
        f'Generated (local time): `{timestamp}`',
        f'Test: `{report["test"]}`',
        f'Result: `{"PASSED" if report["passed"] else "FAILED"}`',
        f'Connection mode: `{report["mode"]}`',
        f'IF820 address: `{report["mac"]}`',
        '',
    ]
    if report['test'] == 'transfer':
        lines.extend([
            '| Direction | Result | Expected bytes | Received bytes |',
            '| --- | --- | ---: | ---: |',
            f'| PC -> IF820 | `{report["pc_to_if820"]["result"]}` | '
            f'{report["pc_to_if820"]["expected_bytes"]} | '
            f'{report["pc_to_if820"]["received_bytes"]} |',
            f'| IF820 -> PC | `{report["if820_to_pc"]["result"]}` | '
            f'{report["if820_to_pc"]["expected_bytes"]} | '
            f'{report["if820_to_pc"]["received_bytes"]} |',
        ])
    markdown_path.write_text('\n'.join(lines) + '\n', encoding='ascii')
    return json_path, markdown_path


# ─── MAIN ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=('first', 'reconnect'),
                        help='Pair a new device or use an existing Windows pairing.')
    parser.add_argument('--smoke', action='store_true',
                        help='Verify the BT Classic SPP connection, without transferring data.')
    parser.add_argument('--transfer-bytes', type=int, default=len(SPP_DATA),
                        help='Bidirectional SPP payload size; defaults to the legacy 16-byte payload.')
    parser.add_argument('-d', '--debug', action='store_true')
    args, _ = parser.parse_known_args()
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.DEBUG if args.debug else logging.WARNING)

    # 1) Popup: Verbindungsmodus wählen
    if args.transfer_bytes < 1:
        parser.error('--transfer-bytes must be positive.')
    mode = args.mode or ask_connection_mode()
    if mode is None:
        logging.info('Abgebrochen.')
        return 1

    # 2) Board initialisieren
    boards = If820Board.get_connected_boards()
    if not boards:
        logging.error('No IF820 board found.')
        return 1

    logging.info('Board init...')
    board = boards[0]
    initialize_board_with_diagnostics(board)

    # 3) Je nach Modus: Factory Reset oder direkt MAC lesen
    if mode == 'first':
        mac = factory_reset(board)
        show_pairing_instructions(mac)
    else:
        mac = read_bt_mac(board)

    # 4) RFCOMM verbinden
    logging.info(f'[PC] Verbinde mit {mac}...')
    sock = rfcomm_connect(mac)
    if sock is None:
        logging.error(
            'RFCOMM connection to %s failed. Check Windows pairing, ensure no '
            'Windows COM port is configured for the device, and verify PC Bluetooth.', mac)
        return 1

    # 5) IF820 Verbindungsbestätigung abwarten
    wait_for_bt_connected(board)

    if args.smoke:
        sock.close()
        json_path, markdown_path = write_classic_report({
            'test': 'smoke', 'passed': True, 'mode': mode, 'mac': mac,
        })
        print('BT Classic SPP smoke test: PASSED')
        print(f'JSON report: {json_path}')
        print(f'Markdown report: {markdown_path}')
        return 0

    # 6) Datentest
    payload = (b'IF820-SPP-' * ((args.transfer_bytes + 9) // 10))[:args.transfer_bytes]
    ok_pc_if820, ok_if820_pc, rx_if820, rx_pc = run_data_test(board, sock, payload)
    sock.close()

    # 7) Ergebnis
    print()
    print(f'SPP (PC→IF820) : {"PASSED" if ok_pc_if820 else "FAILED"} '
          f'({len(rx_if820)}/{len(payload)} bytes)')
    print(f'SPP (IF820→PC) : {"PASSED" if ok_if820_pc else "FAILED"} '
          f'({len(rx_pc)}/{len(payload)} bytes)')

    result_text = (
        f'Gerät: EZ-Serial "{mac[-8:]}"\n'
        f'MAC: {mac}\n\n'
        f'PC → IF820: {"PASSED" if ok_pc_if820 else "FAILED"}\n'
        f'IF820 → PC: {"PASSED" if ok_if820_pc else "FAILED"}'
    )
    title = 'Erste Verbindung' if mode == 'first' else 'Wiederverbindung'
    print(f'\n{title}\n{result_text}')
    passed = ok_pc_if820 and ok_if820_pc
    json_path, markdown_path = write_classic_report({
        'test': 'transfer', 'passed': passed, 'mode': mode, 'mac': mac,
        'pc_to_if820': {
            'result': 'PASSED' if ok_pc_if820 else 'FAILED',
            'expected_bytes': len(payload), 'received_bytes': len(rx_if820),
        },
        'if820_to_pc': {
            'result': 'PASSED' if ok_if820_pc else 'FAILED',
            'expected_bytes': len(payload), 'received_bytes': len(rx_pc),
        },
    })
    print(f'JSON report: {json_path}')
    print(f'Markdown report: {markdown_path}')
    print('Done.')
    return 0 if passed else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        logging.info('Stopped.')
