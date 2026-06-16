#!/usr/bin/env python3

"""TX-Power Sweep Sample with PC-side RSSI Measurement (single-board variant).

ZWECK / PURPOSE
---------------
Dieses Sample ist eine Erweiterung von sample_advertiser_tx_power.py.
Es durchläuft automatisch alle 8 TX-Power-Stufen des IF820 und misst für jede
Stufe den RSSI am PC-Bluetooth-Adapter. Daraus wird eine Vergleichstabelle erstellt.

WAS WIRD GETESTET? / WHAT IS TESTED?
--------------------------------------
1. TX-Power-Konfiguration des IF820:
   - Alle 8 TX-Power-Stufen werden sequenziell gesetzt.
   - Der jeweils gültige dBm-Wert wird als AD-Typ 0x0A (TX Power Level) im
     Advertisement-Paket mitgesendet.

2. RSSI-Messung am PC:
   - Der PC empfängt die Advertisements via bleak (Windows BT-Adapter).
   - Für jede Stufe werden mehrere RSSI-Samples gesammelt und gemittelt.
   - Die Differenz zwischen behauptetem TX-Power und gemessenem RSSI ergibt
     den Path Loss (Freiraumdämpfung + Hardware).

WARUM BRAUCHT MAN DAS? / WHY IS THIS USEFUL?
---------------------------------------------
- Verifiziert, dass die TX-Power-Einstellung des IF820 korrekt wirkt.
- Misst den tatsächlichen RSSI pro Stufe — nützlich für Antennen-, Gehäuse-
  und Platinen-Charakterisierung.
- Path Loss = TXPower_claimed - RSSI_measured: Bleibt er über alle Stufen
  konstant, stimmt die TX-Power-Kalibrierung.
- Dient als Basis für Reichweitenabschätzungen im finalen Produkt.

ARCHITEKTUR / ARCHITECTURE
---------------------------
  PC (Hauptthread: asyncio/bleak)
  │
  ├── BleakScanner (Windows BT Adapter)
  │       └── Sammelt RSSI-Samples pro TX-Power-Stufe
  │
  └── Background-Thread (IF820 via USB/UART)
          └── Steuert TX-Power-Sweep:
                Stufe 1..8:
                  CMD_SET_TX_POWER
                  CMD_GAP_SET_ADV_DATA  ← TX Power AD Typ 0x0A
                  CMD_GAP_START_ADV
                  Warte 3s → nächste Stufe

VORAUSSETZUNGEN / REQUIREMENTS
--------------------------------
Hardware:
  - 1x IF820 DVK Board, per USB am PC angeschlossen
  - PC mit Bluetooth 4.0+ Adapter (BLE-fähig)
  - Gerät und PC in kurzem, konstantem Abstand (z.B. 30 cm auf Tisch)

Software:
  - pip install bleak

AUSGABE / EXPECTED OUTPUT
--------------------------
  [IF820] TX-Power Sweep — BLE Power Table: [-20, -12, -8, -4, 0, 2, 4, 8] dBm
  [IF820] Level 1: claimed = -20 dBm — advertising...
  [PC]    Level 1: mean RSSI = -72.3 dBm (12 samples) | path loss = 52.3 dB
  [IF820] Level 2: claimed = -12 dBm — advertising...
  [PC]    Level 2: mean RSSI = -64.1 dBm (14 samples) | path loss = 52.1 dB
  ...

  ┌────────────────────────────────────────────────────────┐
  │            TX-Power Sweep Results                      │
  ├───────┬──────────────┬─────────────┬───────────────────┤
  │ Level │ Claimed (dBm)│ RSSI (dBm)  │ Path Loss (dB)    │
  ├───────┼──────────────┼─────────────┼───────────────────┤
  │   1   │    -20       │   -72.3     │      52.3         │
  │   2   │    -12       │   -64.1     │      52.1         │
  │  ...  │    ...       │    ...      │      ...          │
  └───────┴──────────────┴─────────────┴───────────────────┘

Beenden mit Ctrl+C (oder läuft bis alle 8 Stufen abgeschlossen sind).
"""

import argparse
import asyncio
import logging
import time
import threading
import sys
sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board
from bleak import BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

ADV_MODE = ez_port.GapAdvertMode.NA.value
ADV_TYPE = ez_port.GapAdvertType.NON_CONNECTABLE_HIGH_DUTY_CYCLE.value
ADV_INTERVAL = 0x20        # 20 * 0.625ms = 12.5ms (fast for reliable detection)
ADV_CHANNELS = ez_port.GapAdvertChannels.CHANNEL_ALL.value
ADV_TIMEOUT = 0
ADV_FLAGS = ez_port.GapAdvertFlags.ALL.value

# Advertisement payload: [Flags] [TX Power Level AD type 0x0A, value placeholder]
ADV_DATA = [0x02, 0x01, 0x06,
            0x02, 0x0A, 0x00]   # 0x0A = TX Power Level, last byte = dBm value

SAMPLES_PER_LEVEL = 3           # seconds to collect samples per TX-power level
IF820_MAC = None                # filled from boot info

# Shared state between IF820 thread and bleak scanner
current_level: dict = {}        # {'level': int, 'claimed_dbm': int}
rssi_samples: list = []
level_ready = threading.Event()
level_measured = threading.Event()
sweep_done = threading.Event()
results: list = []              # [(level, claimed_dbm, mean_rssi, path_loss), ...]
_lock = threading.Lock()


def quit_on_resp_err(resp: int):
    if resp != 0:
        sys.exit(f'Response err: {hex(resp)}')


def _addr_to_mac_str(addr) -> str:
    """Convert IF820 address (list of bytes, LSB first) to 'AA:BB:CC:DD:EE:FF'."""
    if isinstance(addr, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(addr))
    return str(addr).upper()


def detection_callback(device: BLEDevice, advertisement_data: AdvertisementData):
    """Collect RSSI samples from the IF820 device."""
    global rssi_samples
    if IF820_MAC is None:
        return
    if device.address.upper() != IF820_MAC.upper():
        return
    rssi = advertisement_data.rssi
    if rssi is not None:
        with _lock:
            rssi_samples.append(rssi)


def if820_sweep_thread(board: If820Board):
    """Runs the TX-power sweep on the IF820 in a background thread."""
    try:
        # Discard any events buffered during board initialisation before
        # sending the first EZ-Serial command (prevents protocol mismatch).
        board.p_uart.clear_rx_queue()

        # Stop any current advertising
        board.p_uart.send_and_wait(board.p_uart.CMD_GAP_STOP_ADV)
        try:
            board.p_uart.wait_event(board.p_uart.EVENT_GAP_ADV_STATE_CHANGED,
                                    rxtimeout=3)
        except Exception:
            pass

        # Set advertising parameters (same for all levels)
        quit_on_resp_err(board.p_uart.send_and_wait(
            board.p_uart.CMD_GAP_SET_ADV_PARAMETERS,
            mode=ADV_MODE,
            type=ADV_TYPE,
            channels=ADV_CHANNELS,
            high_interval=ADV_INTERVAL,
            high_duration=ADV_TIMEOUT,
            low_interval=ADV_INTERVAL,
            low_duration=ADV_TIMEOUT,
            flags=ADV_FLAGS,
            directAddr=[0, 0, 0, 0, 0, 0],
            directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])

        # Read TX power table from chip; values are signed int8 in a bytearray
        # (e.g. 0xFE = -2 dBm, not 254).  Convert explicitly.
        res = board.p_uart.send_and_wait(board.p_uart.CMD_GET_TX_POWER)
        power_array = res[1].payload.power_array
        ble_tx_power_table = [
            b if b < 128 else b - 256 for b in power_array[-8:]
        ]
        logging.info(f'[IF820] TX-Power Sweep — BLE Power Table: '
                     f'{ble_tx_power_table} dBm')

        for level in range(1, 9):
            claimed_dbm = ble_tx_power_table[level - 1]

            # Set TX power
            quit_on_resp_err(board.p_uart.send_and_wait(
                board.p_uart.CMD_SET_TX_POWER,
                power=level,
                power_array=[])[0])

            # Update ADV data with current TX power dBm value.
            # BLE AD type 0x0A uses a signed int8, but the list passed to
            # CMD_GAP_SET_ADV_DATA must contain unsigned bytes (0-255).
            # Use & 0xFF to re-encode: -2 → 0xFE, negative values wrap correctly.
            ADV_DATA[-1] = claimed_dbm & 0xFF
            quit_on_resp_err(board.p_uart.send_and_wait(
                board.p_uart.CMD_GAP_SET_ADV_DATA,
                data=ADV_DATA)[0])

            # Start advertising
            quit_on_resp_err(board.p_uart.send_and_wait(
                board.p_uart.CMD_GAP_START_ADV,
                mode=ADV_MODE,
                type=ADV_TYPE,
                channels=ADV_CHANNELS,
                high_interval=ADV_INTERVAL,
                high_duration=ADV_TIMEOUT,
                low_interval=ADV_INTERVAL,
                low_duration=ADV_TIMEOUT,
                flags=ADV_FLAGS,
                directAddr=[0, 0, 0, 0, 0, 0],
                directAddrType=ez_port.GapAddressType.PUBLIC.value)[0])

            logging.info(f'[IF820] Level {level}: claimed = {claimed_dbm} dBm '
                         f'— advertising...')

            # Signal bleak scanner that a new level is ready
            with _lock:
                current_level['level'] = level
                current_level['claimed_dbm'] = claimed_dbm
                rssi_samples.clear()
            level_measured.clear()
            level_ready.set()

            # Wait for PC to finish measuring this level
            level_measured.wait()

            # Stop advertising before moving to next level
            board.p_uart.send_and_wait(board.p_uart.CMD_GAP_STOP_ADV)
            time.sleep(0.2)

        sweep_done.set()

    except Exception as e:
        logging.error(f'[IF820] Fatal error in sweep thread: {e}')
        sweep_done.set()


def _print_results_table(res: list):
    sep = '├───────┼──────────────┼─────────────┼───────────────────┤'
    print()
    print('┌────────────────────────────────────────────────────────┐')
    print('│            TX-Power Sweep Results                      │')
    print('├───────┬──────────────┬─────────────┬───────────────────┤')
    print('│ Level │ Claimed (dBm)│  RSSI (dBm) │  Path Loss (dB)   │')
    print(sep)
    for (lvl, claimed, mean_rssi, path_loss, n) in res:
        print(f'│  {lvl:>3}  │   {claimed:>6}     │  {mean_rssi:>7.1f}    │'
              f'     {path_loss:>7.1f}       │')
    print('└───────┴──────────────┴─────────────┴───────────────────┘')
    print()


async def main(if820_board_p: If820Board):
    # Start TX-power sweep in background thread
    t = threading.Thread(target=if820_sweep_thread,
                         args=(if820_board_p,), daemon=True)
    t.start()

    async with BleakScanner(detection_callback=detection_callback):
        while not sweep_done.is_set():
            # Wait for IF820 to signal next level (poll every 2 s so sweep_done
            # is noticed quickly after the last level without a long timeout).
            ready = await asyncio.get_event_loop().run_in_executor(
                None, lambda: level_ready.wait(timeout=2))
            if sweep_done.is_set():
                break
            if not ready:
                continue  # keep polling until sweep is done or level arrives
            level_ready.clear()

            with _lock:
                lvl = current_level.get('level', '?')
                claimed_dbm = current_level.get('claimed_dbm', 0)

            # Collect RSSI samples for SAMPLES_PER_LEVEL seconds
            await asyncio.sleep(SAMPLES_PER_LEVEL)

            with _lock:
                samples = list(rssi_samples)

            if samples:
                mean_rssi = sum(samples) / len(samples)
                path_loss = claimed_dbm - mean_rssi
                logging.info(
                    f'[PC]    Level {lvl}: mean RSSI = {mean_rssi:.1f} dBm '
                    f'({len(samples)} samples) | path loss = {path_loss:.1f} dB')
            else:
                mean_rssi = float('nan')
                path_loss = float('nan')
                logging.warning(f'[PC]    Level {lvl}: no RSSI samples received '
                                 f'(check BT adapter and proximity)')

            results.append((lvl, claimed_dbm, mean_rssi, path_loss, len(samples)))
            level_measured.set()

    _print_results_table(results)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    parser.add_argument('-s', '--samples', type=int, default=SAMPLES_PER_LEVEL,
                        help=f"Seconds to collect RSSI samples per level "
                             f"(default: {SAMPLES_PER_LEVEL})")
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    SAMPLES_PER_LEVEL = args.samples

    boards = If820Board.get_connected_boards()
    if len(boards) < 1:
        logging.critical("No IF820 board found.")
        sys.exit(1)

    logging.info('Init IF820 board...')
    if820_board_p = boards[0]
    if820_board_p.open_and_init_board(False)
    if820_board_p.close_ports_and_reset()
    time.sleep(3)
    boot_info = if820_board_p.open_and_init_board()
    logging.info(f'IF820 boot: {boot_info}')

    IF820_MAC = _addr_to_mac_str(boot_info.payload.address)
    logging.info(f'IF820 MAC (filter): {IF820_MAC}')

    try:
        asyncio.run(main(if820_board_p))
    except KeyboardInterrupt:
        logging.info('Stopped.')
        if results:
            _print_results_table(results)
