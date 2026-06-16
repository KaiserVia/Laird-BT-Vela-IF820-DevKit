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
import logging
import socket
import time
import sys
import tkinter as tk
from tkinter import messagebox

sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board

SPP_DATA = b'abcdefghijklmnop'
OTA_LATENCY = 0.5
RFCOMM_TIMEOUT = 15.0
BT_PIN = '00297'


def _mac_list_to_str(mac_list) -> str:
    """Convert IF820 MAC address (list of bytes, LSB first) to colon-separated
    string (MSB first), as expected by the OS BT stack."""
    if isinstance(mac_list, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(mac_list))
    return str(mac_list)


# ─── GUI ────────────────────────────────────────────────────────────────────

def ask_connection_mode() -> str:
    """Popup: User wählt 'first' oder 'reconnect'. Gibt None zurück bei Abbruch."""
    result = [None]

    win = tk.Tk()
    win.title('IF820 SPP Test')
    win.resizable(False, False)
    win.attributes('-topmost', True)

    frame = tk.Frame(win, padx=20, pady=20)
    frame.pack()

    tk.Label(frame, text='Wie soll verbunden werden?',
             font=('Segoe UI', 12, 'bold')).pack(pady=(0, 15))

    def choose_first():
        result[0] = 'first'
        win.destroy()

    def choose_reconnect():
        result[0] = 'reconnect'
        win.destroy()

    tk.Button(frame, text='Erste Verbindung\n(Gerät noch nie mit diesem PC gekoppelt)',
              command=choose_first, width=45, height=3,
              font=('Segoe UI', 10)).pack(pady=5)

    tk.Button(frame, text='Wiederverbindung\n(War schon gekoppelt, komme zurück)',
              command=choose_reconnect, width=45, height=3,
              font=('Segoe UI', 10)).pack(pady=5)

    win.protocol('WM_DELETE_WINDOW', win.destroy)
    win.mainloop()
    return result[0]


def show_pairing_instructions(mac: str):
    """Popup mit Kopplungsanleitung. Blockiert bis User 'OK' klickt."""
    win = tk.Tk()
    win.title('Windows-Kopplung erforderlich')
    win.resizable(False, False)
    win.attributes('-topmost', True)

    frame = tk.Frame(win, padx=20, pady=20)
    frame.pack()

    instructions = (
        f'Bitte das Gerät jetzt in Windows koppeln:\n\n'
        f'1. Windows → Einstellungen → Bluetooth & Geräte\n'
        f'2. "Gerät hinzufügen" → Bluetooth\n'
        f'3. Gerät  EZ-Serial "{mac[-8:]}"  wählen\n'
        f'   (vollständige MAC: {mac})\n'
        f'4. Koppeln — PIN eingeben: {BT_PIN}\n\n'
        f'⚠ WICHTIG: Keinen COM-Port konfigurieren!\n'
        f'   (COM-Port → Windows blockiert den Python-Socket)\n\n'
        f'Wenn die Kopplung abgeschlossen ist → OK drücken.'
    )

    tk.Label(frame, text=instructions, font=('Segoe UI', 10),
             justify='left', anchor='w').pack(pady=(0, 15))

    tk.Button(frame, text='OK — Kopplung abgeschlossen',
              command=win.destroy, width=30, height=2,
              font=('Segoe UI', 10, 'bold')).pack()

    win.protocol('WM_DELETE_WINDOW', win.destroy)
    win.mainloop()



# ─── IF820 BOARD OPERATIONS ────────────────────────────────────────────────

def read_bt_mac(board: If820Board) -> str:
    """Liest die BT Classic MAC vom IF820 (NVM-stabil)."""
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(board.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    return _mac_list_to_str(ez_rsp[1].payload.address)


def set_bt_pin(board: If820Board, pin: str):
    """Setzt Legacy-Pairing-Modus und BT Classic PIN, speichert in NVM."""
    # SSP deaktivieren → Legacy Pairing mit PIN erzwingen
    # m=00: kein SSP, b=0: no bonding store, k=10: keysize,
    # p=0: normal, i=02: KeyboardOnly, f=00: kein SC
    legacy_cmd = 'ssbp,m=00,b=0,k=10,p=0,i=02,f=00\r'
    logging.info('[IF820] Setze Security Mode auf Legacy (kein SSP)...')
    board.p_uart.send(legacy_cmd.encode('ascii'))
    time.sleep(0.3)
    board.p_uart.clear_rx_queue()

    # PIN setzen
    pin_hex = pin.encode('ascii').hex()
    cmd = f'sbtpin,p={pin_hex}\r'
    logging.info(f'[IF820] Setze BT PIN: {pin}')
    board.p_uart.send(cmd.encode('ascii'))
    time.sleep(0.3)
    board.p_uart.clear_rx_queue()

    # Config in NVM speichern
    logging.info('[IF820] Speichere Config (NVM)...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_STORE_CONFIG)
    If820Board.check_if820_response(board.p_uart.CMD_STORE_CONFIG, ez_rsp)


def factory_reset(board: If820Board) -> str:
    """Factory Reset → Reboot → PIN setzen → MAC auslesen. Gibt MAC zurück."""
    logging.info('[IF820] Factory Reset...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_FACTORY_RESET)
    If820Board.check_if820_response(board.p_uart.CMD_FACTORY_RESET, ez_rsp)

    logging.info('[IF820] Warte auf Reboot...')
    ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(board.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)
    time.sleep(0.5)
    board.p_uart.clear_rx_queue()

    set_bt_pin(board, BT_PIN)

    mac = read_bt_mac(board)
    logging.info(f'[IF820] BT Classic MAC: {mac}')
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


def run_data_test(board: If820Board, sock: socket.socket):
    """Sendet/empfängt Testdaten über SPP. Gibt (pc_to_if820_ok, if820_to_pc_ok) zurück."""
    # PC → IF820
    logging.info(f'[PC] Sende: {SPP_DATA}')
    sock.send(SPP_DATA)
    time.sleep(OTA_LATENCY + 0.5)

    rx_on_if820 = board.p_uart.read()
    rx_on_if820 = bytes(rx_on_if820) if rx_on_if820 else b''
    logging.info(f'[IF820] Empfangen: {rx_on_if820}')

    # IF820 → PC
    logging.info(f'[IF820] Sende: {SPP_DATA}')
    board.p_uart.send(SPP_DATA)
    time.sleep(OTA_LATENCY)

    rx_on_pc = b''
    try:
        sock.settimeout(3.0)
        while len(rx_on_pc) < len(SPP_DATA):
            chunk = sock.recv(256)
            if not chunk:
                break
            rx_on_pc += chunk
    except socket.timeout:
        pass
    logging.info(f'[PC] Empfangen: {rx_on_pc}')

    ok_pc_to_if820 = SPP_DATA in rx_on_if820
    ok_if820_to_pc = SPP_DATA in rx_on_pc
    return ok_pc_to_if820, ok_if820_to_pc, rx_on_if820, rx_on_pc


# ─── MAIN ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true')
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.INFO)
    args, _ = parser.parse_known_args()
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # 1) Popup: Verbindungsmodus wählen
    mode = ask_connection_mode()
    if mode is None:
        logging.info('Abgebrochen.')
        return

    # 2) Board initialisieren
    boards = If820Board.get_connected_boards()
    if not boards:
        messagebox.showerror('Fehler', 'Kein IF820 Board gefunden!')
        return

    logging.info('Board init...')
    board = boards[0]
    board.open_and_init_board(False)
    board.close_ports_and_reset()
    time.sleep(3)
    board.open_and_init_board()

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
        messagebox.showerror('Fehler',
                             f'RFCOMM-Verbindung zu {mac} fehlgeschlagen.\n\n'
                             'Mögliche Ursachen:\n'
                             '- Gerät nicht in Windows gekoppelt\n'
                             '- COM-Port konfiguriert (blockiert Socket)\n'
                             '- Bluetooth am PC aus')
        return

    # 5) IF820 Verbindungsbestätigung abwarten
    wait_for_bt_connected(board)

    # 6) Datentest
    ok_pc_if820, ok_if820_pc, rx_if820, rx_pc = run_data_test(board, sock)
    sock.close()

    # 7) Ergebnis
    print()
    print(f'SPP (PC→IF820) : {"PASSED" if ok_pc_if820 else "FAILED"}  '
          f'(IF820 empfing: {rx_if820!r})')
    print(f'SPP (IF820→PC) : {"PASSED" if ok_if820_pc else "FAILED"}  '
          f'(PC empfing: {rx_pc!r})')

    result_text = (
        f'Gerät: EZ-Serial "{mac[-8:]}"\n'
        f'MAC: {mac}\n\n'
        f'PC → IF820: {"PASSED" if ok_pc_if820 else "FAILED"}\n'
        f'IF820 → PC: {"PASSED" if ok_if820_pc else "FAILED"}'
    )
    title = 'Erste Verbindung' if mode == 'first' else 'Wiederverbindung'
    messagebox.showinfo(f'Ergebnis — {title}', result_text)
    print('Done.')


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        logging.info('Stopped.')
