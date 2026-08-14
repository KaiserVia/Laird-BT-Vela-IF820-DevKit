# CYSPP DVK HIL Test

This directory contains a USB-only reproduction of the IF820 CYSPP pin
behavior described in the accompanying historical reports. It does not need an
external MCU, UART adapter, or separate power supply.

## Topology

```text
PC app simulator (Bleak) <--- BLE CYSPP ---> IF820 DVK
                                             | USB PUART: EZ-Serial and UART data
                                             ` DVK probe GPIO 19: MCU CYSPP pin
```

`GPIO 19` is `If820Board.CYSPP`. The test owns this pin and returns it to input
mode when it finishes.

## Run

Run from the firmware directory:

```powershell
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin low --new-module
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin high --new-module
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin input --new-module
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin input --transfer-bytes 40960
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin input --transfer-bytes 40960 --transfer-window 4 --transfer-retries 3
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin input --transfer-bytes 40960 --transfer-protocol burst
python tests/BLE_SPP_Simulation/cyspp_dvk_hil.py --pin input --transfer-bytes 40960 --uart-flow-control
```

`--new-module` intentionally issues a factory reset and is required for the
first reference run. Omit it only after the preceding run has established and
left the desired CYSPP peripheral configuration in place.

The script always uses the CYSPP unacknowledged data characteristic
`65333333-a115-11e2-9e9a-0800200ca102`: it subscribes first, waits for the
link to remain open, receives an MCU-simulated prompt, then writes a test
payload back to the IF820 UART.

`--transfer-bytes 40960` adds a one-way 40 kB PC-to-IF820 transfer after the
CYSPP prompt. It frames data below the BLE write-without-response limit and
checks every payload using its sequence number and CRC-32. The JSON artifact
records duration, connection attempts, disconnects, BLE errors, write errors,
lost data, and corrupted data. A companion Markdown report contains an
integrity table and Mermaid pie chart.

The default transfer protocol is `ack`: the PC sends a window of up to four
frames and admits each next frame only after the UART-side simulator has
CRC-validated a frame and returned `ACK1 + sequence` through CYSPP
notifications. Timed-out frames are retransmitted up to three times; duplicate
frames are acknowledged again but not counted twice. This end-to-end credit
prevents the BLE sender from overflowing the IF820-to-UART path.
`--transfer-window` and `--transfer-retries` adjust these limits.
`--transfer-protocol burst` disables the control for overflow comparison only.

When an ACK window exhausts its retries, the sender performs up to three
automatic recovery cycles before failing the run. It switches subsequent data
to stop-and-wait operation (window `1`), halves the configured wire rate down
to `1000` B/s, and retries the current pending window. The transfer report
distinguishes `Unconfirmed` data from data observed as corrupt: an aborted
sender cannot prove that frames it never received an ACK for were physically
lost at the UART.

The 2026-08-13 reference run transferred 40 kB with window `4`, no retries,
no loss, and no corruption at approximately `7.9 kB/s` verified payload rate.

`--uart-flow-control` configures the IF820 PUART for 1,000,000 baud with
RTS/CTS (`flow=1`), then reopens the PC COM port at the matching speed with
RTS/CTS enabled. It verifies the transition with an EZ-Serial ping. The current
DVK USB-PUART path completed that ping, but corrupted an asynchronous BLE
status event before the transfer started. Therefore it is implemented as an
opt-in diagnostic mode and is not included in the aggregate matrix report yet.

## Test Matrix

| Pin mode | Meaning | Observed result |
| --- | --- | --- |
| `low` | MCU actively drives CYSPP low | BLE discovery and characteristic write occur, but no CCCD event or transparent UART data appears |
| `high` | MCU actively drives CYSPP high | The write reaches the UART as an EZ-Serial GATT event, not transparent CYSPP data |
| `input` | MCU releases CYSPP | Full bidirectional transparent CYSPP transfer passes |

## Evidence

Every run writes a JSON file under `artifacts/`. It records the sampled GPIO
level, IF820 address, BLE identity, time-ordered events, UART payloads, and a
result. `run_cyspp_dvk_matrix.py` runs all three states in isolated processes
and then runs a separate 40 kB `input`-pin transfer. It writes one aggregate
JSON and Markdown report with pin and payload-integrity Mermaid diagrams.
Preserve the aggregate report and per-case evidence for the MCU handoff.

## Current Result

The clean matrix run on 2026-08-13 used identical CYSPP settings for all
three pin modes: `enable=2`, `role=0`, and `client_flags=2`. Released input,
sampled HIGH, was the only state that delivered the MCU prompt to the PC app
and returned `APP-SIM HELLO` as transparent UART data. Actively driven HIGH,
also sampled HIGH, instead returned the app payload as an EZ-Serial GATT
event. Actively driven LOW, sampled LOW, did not produce a CCCD event or
transparent transfer. Pin ownership and drive mode therefore matter in
addition to the sampled electrical level.

## MCU Handoff Rules

- At CYSPP subscribe time, log both the configured pin mode and sampled level.
- For this tested configuration, leave CYSPP released (`input`); do not drive
  the line HIGH or LOW during transparent CYSPP operation.
- A BLE reconnect must return the MCU-owned CYSPP pin to its released state.
- Preserve the raw JSON report and any logic-analyzer capture with the firmware
  build identifier and test case name.

External-MCU electrical simulation remains a phase-2 option after this DVK
matrix produces a stable reference result.
