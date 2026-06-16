#!/usr/bin/env python3

"""SPP mit App-Level Authentifizierung — PC simuliert Handy-App + Host-MCU.

KONZEPT
-------
Der IF820 ist eine transparente BT-UART-Pipe. Die Sicherheit wird NICHT auf
BT-Ebene (PIN/SSP) gelöst, sondern auf Anwendungsebene:

  1. RFCOMM-Verbindung aufbauen (Just Works — jeder kann)
  2. Client (Handy-App / PC) sendet AUTH:<secret>
  3. Host-MCU prüft Secret:
     - Richtig → AUTH:OK → Datenverkehr freigegeben
     - Falsch/Timeout → AUTH:FAIL → Verbindung trennen

In diesem Test-Script übernimmt der PC BEIDE Rollen:
  - "App-Seite": Sendet AUTH-Token über RFCOMM-Socket
  - "MCU-Seite": Liest AUTH-Token von IF820-UART, validiert, antwortet

USE-CASES
---------
  1. Erste Verbindung: Factory Reset → Pairing → Auth → Datentest
  2. Wiederverbindung: Direkt verbinden → Auth → Datentest
  3. Falsche Auth: Verbindung wird nach Fehlversuch getrennt
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
AUTH_SECRET = '00297'
AUTH_TIMEOUT = 5.0  # Sekunden bis Auth-Timeout


def _mac_list_to_str(mac_list) -> str:
    if isinstance(mac_list, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(mac_list))
    return str(mac_list)


# ─── GUI ────────────────────────────────────────────────────────────────────

def ask_connection_mode() -> str:
    """Popup: User wählt Modus. Gibt 'first', 'reconnect' oder 'bad_auth' zurück."""
    result = [None]

    win = tk.Tk()
    win.title('IF820 SPP Auth Test')
    win.resizable(False, False)
    win.attributes('-topmost', True)

    frame = tk.Frame(win, padx=20, pady=20)
    frame.pack()

    tk.Label(frame, text='Welchen Test durchführen?',
             font=('Segoe UI', 12, 'bold')).pack(pady=(0, 15))

    def choose(mode):
        result[0] = mode
        win.destroy()

    tk.Button(frame, text='Erste Verbindung + Auth\n(Factory Reset → Pairing → Auth → Daten)',
              command=lambda: choose('first'), width=50, height=3,
              font=('Segoe UI', 10)).pack(pady=5)

    tk.Button(frame, text='Wiederverbindung + Auth\n(Bereits gekoppelt → Auth → Daten)',
              command=lambda: choose('reconnect'), width=50, height=3,
              font=('Segoe UI', 10)).pack(pady=5)

    tk.Button(frame, text='Falsche Authentifizierung\n(Falsches Secret → Verbindung wird getrennt)',
              command=lambda: choose('bad_auth'), width=50, height=3,
              font=('Segoe UI', 10), fg='red').pack(pady=5)

    win.protocol('WM_DELETE_WINDOW', win.destroy)
    win.mainloop()
    return result[0]


def show_pairing_instructions(mac: str):
    """Popup mit Kopplungsanleitung."""
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
        f'4. Koppeln (Just Works — kein PIN nötig)\n\n'
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
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(board.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    return _mac_list_to_str(ez_rsp[1].payload.address)


def factory_reset(board: If820Board) -> str:
    """Factory Reset → Reboot → MAC auslesen."""
    logging.info('[IF820] Factory Reset...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_FACTORY_RESET)
    If820Board.check_if820_response(board.p_uart.CMD_FACTORY_RESET, ez_rsp)

    logging.info('[IF820] Warte auf Reboot...')
    ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(board.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)
    time.sleep(0.5)
    board.p_uart.clear_rx_queue()

    mac = read_bt_mac(board)
    logging.info(f'[IF820] BT Classic MAC: {mac}')
    return mac


# ─── RFCOMM ───────────────────────────────────────────────────────────────

def rfcomm_connect(mac: str) -> socket.socket:
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
    logging.info('[IF820] Warte auf EVENT_BT_CONNECTED...')
    ez_rsp = board.p_uart.wait_event(
        board.p_uart.EVENT_BT_CONNECTED, rxtimeout=30)
    If820Board.check_if820_response(board.p_uart.EVENT_BT_CONNECTED, ez_rsp)
    logging.info('[IF820] BT Classic verbunden!')
    board.p_uart.clear_rx_queue()


# ─── AUTHENTIFIZIERUNG ─────────────────────────────────────────────────────

def app_send_auth(sock: socket.socket, secret: str):
    """App-Seite: Sendet AUTH:<secret> über BT-Socket."""
    auth_msg = f'AUTH:{secret}\n'.encode('utf-8')
    logging.info(f'[APP] Sende Auth: AUTH:{secret}')
    sock.send(auth_msg)


def mcu_check_auth(board: If820Board, expected_secret: str) -> bool:
    """MCU-Seite: Liest Auth-Token von UART, validiert, sendet Antwort.

    Returns:
        True wenn Auth erfolgreich, False sonst.
    """
    logging.info(f'[MCU] Warte auf AUTH-Token (max {AUTH_TIMEOUT}s)...')

    # Warte auf Daten von der App (kommen über IF820 UART)
    deadline = time.time() + AUTH_TIMEOUT
    rx_data = b''
    while time.time() < deadline:
        chunk = board.p_uart.read()
        if chunk:
            rx_data += bytes(chunk)
            if b'\n' in rx_data:
                break
        time.sleep(0.1)

    if not rx_data:
        logging.warning('[MCU] AUTH-Timeout — keine Daten empfangen.')
        # Antwort senden
        board.p_uart.send(b'AUTH:TIMEOUT\n')
        time.sleep(OTA_LATENCY)
        return False

    # Parse: erwarte "AUTH:<secret>\n"
    rx_str = rx_data.decode('utf-8', errors='replace').strip()
    logging.info(f'[MCU] Empfangen: {rx_str!r}')

    if rx_str.startswith('AUTH:'):
        received_secret = rx_str[5:]
        if received_secret == expected_secret:
            logging.info('[MCU] Auth OK!')
            board.p_uart.send(b'AUTH:OK\n')
            time.sleep(OTA_LATENCY)
            return True
        else:
            logging.warning(f'[MCU] Auth FALSCH! Erwartet={expected_secret!r}, '
                            f'Empfangen={received_secret!r}')
            board.p_uart.send(b'AUTH:FAIL\n')
            time.sleep(OTA_LATENCY)
            return False
    else:
        logging.warning(f'[MCU] Ungültiges Format: {rx_str!r}')
        board.p_uart.send(b'AUTH:FAIL\n')
        time.sleep(OTA_LATENCY)
        return False


def app_wait_auth_response(sock: socket.socket) -> str:
    """App-Seite: Wartet auf AUTH-Antwort vom Gerät.

    Returns:
        'OK', 'FAIL', 'TIMEOUT' oder '' bei Fehler.
    """
    try:
        sock.settimeout(AUTH_TIMEOUT + 2)
        data = b''
        while b'\n' not in data:
            chunk = sock.recv(256)
            if not chunk:
                break
            data += chunk
        response = data.decode('utf-8', errors='replace').strip()
        logging.info(f'[APP] Auth-Antwort: {response!r}')
        if response.startswith('AUTH:'):
            return response[5:]
        return response
    except socket.timeout:
        logging.warning('[APP] Timeout auf Auth-Antwort.')
        return 'TIMEOUT'


# ─── DATENTEST ─────────────────────────────────────────────────────────────

def run_data_test(board: If820Board, sock: socket.socket):
    """Sendet/empfängt Testdaten. Gibt (ok_pc_if820, ok_if820_pc, rx_if820, rx_pc) zurück."""
    # PC (App) → IF820 (MCU)
    logging.info(f'[APP] Sende Daten: {SPP_DATA}')
    sock.send(SPP_DATA)
    time.sleep(OTA_LATENCY + 0.5)

    rx_on_if820 = board.p_uart.read()
    rx_on_if820 = bytes(rx_on_if820) if rx_on_if820 else b''
    logging.info(f'[MCU] Empfangen: {rx_on_if820}')

    # IF820 (MCU) → PC (App)
    logging.info(f'[MCU] Sende Daten: {SPP_DATA}')
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
    logging.info(f'[APP] Empfangen: {rx_on_pc}')

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

    # 1) Modus wählen
    mode = ask_connection_mode()
    if mode is None:
        logging.info('Abgebrochen.')
        return

    # 2) Board init
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

    # 3) Je nach Modus
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

    # 5) IF820 Verbindung bestätigen
    wait_for_bt_connected(board)

    # 6) Authentifizierung
    # Bestimme welches Secret die "App" sendet
    if mode == 'bad_auth':
        send_secret = 'WRONG'  # absichtlich falsch
    else:
        send_secret = AUTH_SECRET

    # App sendet Auth
    app_send_auth(sock, send_secret)
    time.sleep(OTA_LATENCY)

    # MCU prüft Auth
    auth_ok = mcu_check_auth(board, AUTH_SECRET)
    time.sleep(OTA_LATENCY)

    # App empfängt Antwort
    auth_response = app_wait_auth_response(sock)

    if not auth_ok:
        logging.warning('[TEST] Authentifizierung fehlgeschlagen — Verbindung wird getrennt.')
        sock.close()

        result_text = (
            f'Gerät: EZ-Serial "{mac[-8:]}"\n'
            f'MAC: {mac}\n\n'
            f'Auth: FEHLGESCHLAGEN (Secret: {send_secret!r})\n'
            f'Antwort: AUTH:{auth_response}\n\n'
            f'Verbindung wurde getrennt.'
        )
        if mode == 'bad_auth':
            messagebox.showinfo('Ergebnis — Falsche Auth (erwartet)', result_text)
            print('\nAuth-Reject-Test: PASSED (Verbindung korrekt abgelehnt)')
        else:
            messagebox.showerror('Ergebnis — Auth fehlgeschlagen', result_text)
            print('\nAuth: FAILED')
        print('Done.')
        return

    # 7) Auth OK → Datentest
    logging.info('[TEST] Auth OK — starte Datentest...')
    ok_pc_if820, ok_if820_pc, rx_if820, rx_pc = run_data_test(board, sock)
    sock.close()

    # 8) Ergebnis
    print()
    print(f'Auth         : PASSED (Secret: {send_secret!r})')
    print(f'SPP (App→MCU): {"PASSED" if ok_pc_if820 else "FAILED"}  '
          f'(MCU empfing: {rx_if820!r})')
    print(f'SPP (MCU→App): {"PASSED" if ok_if820_pc else "FAILED"}  '
          f'(App empfing: {rx_pc!r})')

    result_text = (
        f'Gerät: EZ-Serial "{mac[-8:]}"\n'
        f'MAC: {mac}\n\n'
        f'Auth: OK ✓\n'
        f'App → MCU: {"PASSED" if ok_pc_if820 else "FAILED"}\n'
        f'MCU → App: {"PASSED" if ok_if820_pc else "FAILED"}'
    )
    title = 'Erste Verbindung' if mode == 'first' else 'Wiederverbindung'
    messagebox.showinfo(f'Ergebnis — {title} + Auth', result_text)
    print('Done.')


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        logging.info('Stopped.')
