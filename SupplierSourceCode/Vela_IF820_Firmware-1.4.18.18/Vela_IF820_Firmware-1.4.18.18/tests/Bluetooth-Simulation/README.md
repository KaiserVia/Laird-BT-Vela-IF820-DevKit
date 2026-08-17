# Bluetooth Simulation Tests

This directory contains the PC-driven IF820 Bluetooth test suite.

## Layout

- `ble/`: BLE CYSPP smoke and integrity-transfer tests.
- `classic/`: Windows BT Classic SPP smoke and integrity-transfer tests.
- `run_bluetooth_tests.py`: interactive and command-line test selector.
- `Testreports/`: generated report folders, one folder per runner invocation.

## Run

Run from the firmware directory:

```powershell
python tests/Bluetooth-Simulation/run_bluetooth_tests.py
python tests/Bluetooth-Simulation/run_bluetooth_tests.py --tests ble-smoke
python tests/Bluetooth-Simulation/run_bluetooth_tests.py --tests classic-transfer --mode reconnect --bytes 40960
```

Each runner invocation creates a local-time report folder using
`YYYY-MM-DD_hh-mm-ss`, for example:

```text
Testreports/2026-08-17_13-47-00/
  overview.md
  overview.json
  ble-smoke/
  ble-transfer/
  classic-smoke/
  classic-transfer/
```

`overview.md` contains the shared outcome and links to every individual report
created during that run. Historical reports that existed before this structure
are retained under `Testreports/legacy/ble/`.