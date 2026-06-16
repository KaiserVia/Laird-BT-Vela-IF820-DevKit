#!/usr/bin/env python3

"""RN4678 PIN-Prozess Simulation — Beide Seiten über IF820 + Windows BT.

Simuliert den kompletten RN4678-Kommunikationsprozess:
  - IF820 UART  = "Geräteseite" (RN4678-Firmware-Logik)
  - Windows RFCOMM Socket = "App-Seite" (verbindet sich und gibt PIN ein)

Der IF820 arbeitet im SPP Transparent Mode als Bridge:
  board.p_uart.send(data) → geht über BT SPP → kommt auf RFCOMM Socket an
  sock.send(data)          → geht über BT SPP → kommt auf IF820 UART an

Ablauf:
  1. IF820 Board initialisieren (Factory Reset oder Wiederverbindung)
  2. PC verbindet sich per RFCOMM Socket zum IF820
  3. IF820/UART-Seite sendet PIN-Prompt ("\r\nPIN = ") → App empfängt auf Socket
  4. App sendet PIN-Ziffern über Socket → IF820/UART empfängt sie
  5. IF820/UART verarbeitet gemäß RN4678-Logik, sendet '*' pro Ziffer zurück
  6. Bei korrekter PIN: Hauptmenü senden
  7. Test: Falscher PIN → erneuter Prompt
  8. Test: 'H' Reset
  9. Test: Korrekter PIN → Menü

Basiert auf docs/BT_PIN-Process_RN4678.md
"""

import argparse
import logging
import socket
import threading
import time
import sys
import tkinter as tk
from tkinter import messagebox

sys.path.append('./common_lib/libraries')
import EzSerialPort as ez_port
from If820Board import If820Board

# ─── KONFIGURATION ─────────────────────────────────────────────────────────

DEFAULT_PIN = '123456'          # 6-stellige PIN (simuliert fp.btpin)
BT_PIN_PAIRING = '00297'       # Legacy Pairing PIN für IF820
OTA_LATENCY = 0.5              # BT-Übertragungslatenz
RFCOMM_TIMEOUT = 15.0          # Socket-Verbindungstimeout
PIN_MAX_DIGITS = 6             # Maximale PIN-Länge

# Simuliertes Hauptmenü (wie RN4678-Firmware nach erfolgreicher PIN)
MAIN_MENU_TEXT = (
    "\r\n"
    "=============================\r\n"
    "    VIASIS Hauptmenue\r\n"
    "=============================\r\n"
    " 1 - Status\r\n"
    " 2 - Messwerte\r\n"
    " 3 - Konfiguration\r\n"
    " 4 - Diagnose\r\n"
    " 5 - Bluetooth\r\n"
    " 6 - System\r\n"
    "=============================\r\n"
    "Auswahl: "
)


def _mac_list_to_str(mac_list) -> str:
    """Convert IF820 MAC address (list of bytes, LSB first) to colon-separated
    string (MSB first)."""
    if isinstance(mac_list, (list, bytes, bytearray)):
        return ':'.join(f'{b:02X}' for b in reversed(mac_list))
    return str(mac_list)


# ─── GUI ────────────────────────────────────────────────────────────────────

def ask_connection_mode() -> str:
    """Popup: User wählt 'first' oder 'reconnect'."""
    result = [None]

    win = tk.Tk()
    win.title('IF820 RN4678 Simulation')
    win.resizable(False, False)
    win.attributes('-topmost', True)

    frame = tk.Frame(win, padx=20, pady=20)
    frame.pack()

    tk.Label(frame, text='RN4678 PIN-Simulation — Verbindungsmodus?',
             font=('Segoe UI', 12, 'bold')).pack(pady=(0, 15))

    def choose_first():
        result[0] = 'first'
        win.destroy()

    def choose_reconnect():
        result[0] = 'reconnect'
        win.destroy()

    tk.Button(frame, text='Erste Verbindung\n(Factory Reset, neu koppeln)',
              command=choose_first, width=45, height=3,
              font=('Segoe UI', 10)).pack(pady=5)

    tk.Button(frame, text='Wiederverbindung\n(Bereits gekoppelt)',
              command=choose_reconnect, width=45, height=3,
              font=('Segoe UI', 10)).pack(pady=5)

    win.protocol('WM_DELETE_WINDOW', win.destroy)
    win.mainloop()
    return result[0]


def ask_pin() -> str:
    """Popup: User gibt die 6-stellige Geräte-PIN ein."""
    result = [DEFAULT_PIN]

    win = tk.Tk()
    win.title('Geräte-PIN konfigurieren')
    win.resizable(False, False)
    win.attributes('-topmost', True)

    frame = tk.Frame(win, padx=20, pady=20)
    frame.pack()

    tk.Label(frame, text='6-stellige Geräte-PIN eingeben:',
             font=('Segoe UI', 11)).pack(pady=(0, 10))

    tk.Label(frame, text='(Seriennummer des simulierten Geräts)',
             font=('Segoe UI', 9), fg='gray').pack(pady=(0, 10))

    entry = tk.Entry(frame, font=('Consolas', 14), width=10, justify='center')
    entry.insert(0, DEFAULT_PIN)
    entry.pack(pady=(0, 15))
    entry.select_range(0, tk.END)
    entry.focus()

    def confirm():
        val = entry.get().strip()
        if len(val) == 6 and val.isdigit():
            result[0] = val
            win.destroy()
        else:
            messagebox.showwarning('Ungültig',
                                   'PIN muss genau 6 Ziffern haben.')

    tk.Button(frame, text='OK', command=confirm, width=20, height=2,
              font=('Segoe UI', 10, 'bold')).pack()

    entry.bind('<Return>', lambda e: confirm())
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
        f'4. Koppeln — PIN eingeben: {BT_PIN_PAIRING}\n\n'
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
    """Liest die BT Classic MAC vom IF820."""
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_GET_BT_ADDR)
    If820Board.check_if820_response(board.p_uart.CMD_GET_BT_ADDR, ez_rsp)
    return _mac_list_to_str(ez_rsp[1].payload.address)


def set_bt_pin(board: If820Board, pin: str):
    """Setzt Legacy-Pairing-Modus und BT Classic PIN."""
    legacy_cmd = 'ssbp,m=00,b=0,k=10,p=0,i=02,f=00\r'
    logging.info('[IF820] Setze Security Mode auf Legacy (kein SSP)...')
    board.p_uart.send(legacy_cmd.encode('ascii'))
    time.sleep(0.3)
    board.p_uart.clear_rx_queue()

    pin_hex = pin.encode('ascii').hex()
    cmd = f'sbtpin,p={pin_hex}\r'
    logging.info(f'[IF820] Setze BT Pairing PIN: {pin}')
    board.p_uart.send(cmd.encode('ascii'))
    time.sleep(0.3)
    board.p_uart.clear_rx_queue()

    logging.info('[IF820] Speichere Config (NVM)...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_STORE_CONFIG)
    If820Board.check_if820_response(board.p_uart.CMD_STORE_CONFIG, ez_rsp)


def factory_reset(board: If820Board) -> str:
    """Factory Reset → Reboot → PIN setzen → MAC auslesen."""
    logging.info('[IF820] Factory Reset...')
    ez_rsp = board.p_uart.send_and_wait(board.p_uart.CMD_FACTORY_RESET)
    If820Board.check_if820_response(board.p_uart.CMD_FACTORY_RESET, ez_rsp)

    logging.info('[IF820] Warte auf Reboot...')
    ez_rsp = board.p_uart.wait_event(board.p_uart.EVENT_SYSTEM_BOOT)
    If820Board.check_if820_response(board.p_uart.EVENT_SYSTEM_BOOT, ez_rsp)
    time.sleep(0.5)
    board.p_uart.clear_rx_queue()

    set_bt_pin(board, BT_PIN_PAIRING)

    mac = read_bt_mac(board)
    logging.info(f'[IF820] BT Classic MAC: {mac}')
    return mac


# ─── RFCOMM VERBINDUNG (PC/APP-SEITE) ─────────────────────────────────────

def rfcomm_connect(mac: str) -> socket.socket:
    """Versucht RFCOMM-Verbindung auf SCN 1-8. Gibt Socket zurück oder None."""
    for scn in range(1, 9):
        s = socket.socket(socket.AF_BLUETOOTH,
                          socket.SOCK_STREAM,
                          socket.BTPROTO_RFCOMM)
        s.settimeout(RFCOMM_TIMEOUT)
        try:
            logging.info(f'[APP] RFCOMM SCN={scn} auf {mac}...')
            s.connect((mac, scn))
            logging.info(f'[APP] Verbunden auf SCN={scn}!')
            return s
        except (OSError, TimeoutError) as e:
            s.close()
            logging.debug(f'[APP] SCN {scn} fehlgeschlagen: {e}')
    return None


def wait_for_bt_connected(board: If820Board):
    """Wartet auf EVENT_BT_CONNECTED vom IF820."""
    logging.info('[IF820] Warte auf EVENT_BT_CONNECTED...')
    ez_rsp = board.p_uart.wait_event(
        board.p_uart.EVENT_BT_CONNECTED, rxtimeout=30)
    If820Board.check_if820_response(board.p_uart.EVENT_BT_CONNECTED, ez_rsp)
    logging.info('[IF820] BT Classic verbunden!')
    board.p_uart.clear_rx_queue()


# ─── RN4678 FIRMWARE-SIMULATION (GERÄTESEITE / IF820 UART) ────────────────

class RN4678DeviceSimulation:
    """Simuliert die RN4678-Firmware auf der IF820-UART-Seite.

    Läuft in einem eigenen Thread. Liest Daten die vom PC-Socket über SPP
    auf der UART ankommen, verarbeitet sie gemäß PIN-Logik, und sendet
    Antworten zurück über die UART (→ SPP → PC-Socket).
    """

    def __init__(self, board: If820Board, device_pin: str):
        self.board = board
        self.device_pin = int(device_pin)
        self.pin_accumulator = 0
        self.pin_char_count = 0
        self.authenticated = False
        self._stop = False

    def send_to_app(self, data: str):
        """Sendet Daten über UART → SPP → App-Socket."""
        self.board.p_uart.send(data.encode('ascii'))

    def send_pin_prompt(self):
        """Sendet PIN-Prompt und resettet Akkumulator."""
        self.pin_accumulator = 0
        self.pin_char_count = 0
        self.authenticated = False
        self.send_to_app("\r\nPIN = ")
        logging.info('[GERÄT] PIN-Prompt gesendet')

    def process_char(self, c: str) -> str:
        """Verarbeitet ein Zeichen gemäß RN4678-Logik.

        Returns:
            'prompt'        - PIN-Prompt wurde erneut gesendet
            'star'          - Stern-Echo gesendet, weiter warten
            'authenticated' - PIN korrekt, Menü gesendet
        """
        if c == 'H':
            self.send_pin_prompt()
            return 'prompt'

        # Echo: Stern
        self.send_to_app('*')

        # Ziffer akkumulieren
        if c.isdigit():
            self.pin_accumulator = self.pin_accumulator * 10 + int(c)

        self.pin_char_count += 1

        # Prüfung
        if self.pin_accumulator == self.device_pin:
            self.authenticated = True
            self.send_to_app(MAIN_MENU_TEXT)
            logging.info('[GERÄT] PIN korrekt → Hauptmenü gesendet')
            return 'authenticated'

        # Zu viele Zeichen → Reset
        if self.pin_char_count >= PIN_MAX_DIGITS:
            self.send_pin_prompt()
            return 'prompt'

        return 'star'

    def run(self):
        """Thread-Hauptschleife: liest UART, verarbeitet PIN-Logik."""
        time.sleep(0.3)
        self.send_pin_prompt()

        while not self._stop:
            rx = self.board.p_uart.read()
            if not rx:
                time.sleep(0.02)
                continue

            if isinstance(rx, (bytes, bytearray)):
                data = rx
            elif isinstance(rx, list):
                data = bytes(rx)
            else:
                data = bytes([rx])

            for byte_val in data:
                if self._stop:
                    return
                c = chr(byte_val)
                result = self.process_char(c)
                if result == 'authenticated':
                    # Bleibe im Echo-Modus
                    self._echo_loop()
                    return

    def _echo_loop(self):
        """Nach Auth: Echo-Modus (empfangene Daten zurücksenden)."""
        while not self._stop:
            rx = self.board.p_uart.read()
            if not rx:
                time.sleep(0.02)
                continue
            if isinstance(rx, (bytes, bytearray)):
                self.board.p_uart.send(rx)
            elif isinstance(rx, list):
                self.board.p_uart.send(bytes(rx))

    def stop(self):
        self._stop = True


# ─── APP-SEITE (PC / RFCOMM SOCKET) ───────────────────────────────────────

def sock_recv_all(sock: socket.socket, timeout: float = 3.0) -> bytes:
    """Liest alle verfügbaren Daten vom Socket (non-blocking nach erstem Byte)."""
    sock.settimeout(timeout)
    data = b''
    try:
        while True:
            chunk = sock.recv(256)
            if not chunk:
                break
            data += chunk
            sock.settimeout(0.3)  # Nach erstem Chunk: kurzer Timeout für Rest
    except socket.timeout:
        pass
    return data


def run_app_test(sock: socket.socket, device_pin: str):
    """Führt die App-seitigen Tests durch (liest/schreibt auf RFCOMM-Socket).

    Test 1: Falscher PIN → erwartet erneuten Prompt
    Test 2: 'H' Reset → erwartet erneuten Prompt
    Test 3: Korrekter PIN → erwartet Hauptmenü
    """
    results = []

    # ─── Schritt 1: PIN-Prompt empfangen ───────────────────────────────
    print()
    print('━' * 60)
    print('  APP-SEITE: RN4678 PIN-Protokoll Test')
    print('━' * 60)

    logging.info('[APP] Warte auf initialen PIN-Prompt...')
    prompt = sock_recv_all(sock, timeout=5.0)
    prompt_str = prompt.decode('ascii', errors='replace')
    logging.info(f'[APP] Empfangen: {repr(prompt_str)}')

    if 'PIN = ' in prompt_str:
        print(f'  [✓] Initialer Prompt empfangen: {repr(prompt_str.strip())}')
    else:
        print(f'  [✗] Kein PIN-Prompt! Empfangen: {repr(prompt_str)}')
        results.append(('Initialer Prompt', False, prompt_str))
        return results

    # ─── Test 1: Falscher PIN (6 Ziffern) ─────────────────────────────
    print()
    print('  --- Test 1: Falsche PIN senden ---')
    wrong_pin = '000000' if device_pin != '000000' else '999999'
    logging.info(f'[APP] Sende falsche PIN: {wrong_pin}')

    for digit in wrong_pin:
        sock.send(digit.encode('ascii'))
        time.sleep(0.1)  # Zeichenweise, wie echte App

    time.sleep(OTA_LATENCY + 0.5)
    response = sock_recv_all(sock, timeout=3.0)
    resp_str = response.decode('ascii', errors='replace')
    logging.info(f'[APP] Antwort: {repr(resp_str)}')

    # Erwartung: 6× '*' + "\r\nPIN = "
    stars = resp_str.count('*')
    has_reprompt = 'PIN = ' in resp_str

    if stars == 6 and has_reprompt:
        print(f'  [✓] 6 Sterne + erneuter Prompt (falsche PIN abgelehnt)')
        results.append(('Falsche PIN → Reprompt', True, resp_str))
    else:
        print(f'  [✗] Unerwartet: {repr(resp_str)}')
        results.append(('Falsche PIN → Reprompt', False, resp_str))

    # ─── Test 2: 'H' Reset ────────────────────────────────────────────
    print()
    print('  --- Test 2: "H" senden (Reset) ---')
    logging.info('[APP] Sende "H" für PIN-Reset')

    sock.send(b'H')
    time.sleep(OTA_LATENCY + 0.3)
    response = sock_recv_all(sock, timeout=3.0)
    resp_str = response.decode('ascii', errors='replace')
    logging.info(f'[APP] Antwort: {repr(resp_str)}')

    if 'PIN = ' in resp_str:
        print(f'  [✓] "H" → erneuter PIN-Prompt')
        results.append(('H-Reset → Reprompt', True, resp_str))
    else:
        print(f'  [✗] Unerwartet: {repr(resp_str)}')
        results.append(('H-Reset → Reprompt', False, resp_str))

    # ─── Test 3: Korrekter PIN ────────────────────────────────────────
    print()
    print(f'  --- Test 3: Korrekte PIN senden ({device_pin}) ---')
    logging.info(f'[APP] Sende korrekte PIN: {device_pin}')

    for digit in device_pin:
        sock.send(digit.encode('ascii'))
        time.sleep(0.1)

    time.sleep(OTA_LATENCY + 0.5)
    response = sock_recv_all(sock, timeout=5.0)
    resp_str = response.decode('ascii', errors='replace')
    logging.info(f'[APP] Antwort: {repr(resp_str)}')

    # Erwartung: Sterne + Hauptmenü-Text
    has_menu = 'Hauptmenue' in resp_str or 'VIASIS' in resp_str
    stars_correct = resp_str.count('*') >= len(device_pin)

    if has_menu and stars_correct:
        print(f'  [✓] PIN korrekt → Hauptmenü empfangen!')
        results.append(('Korrekte PIN → Menü', True, resp_str))
    else:
        print(f'  [✗] Unerwartet: {repr(resp_str)}')
        results.append(('Korrekte PIN → Menü', False, resp_str))

    # ─── Test 4: Daten im authentifizierten Modus ─────────────────────
    print()
    print('  --- Test 4: Daten senden nach Authentifizierung ---')
    test_data = b'Hello RN4678!'
    logging.info(f'[APP] Sende Testdaten: {test_data}')
    sock.send(test_data)
    time.sleep(OTA_LATENCY + 0.3)

    response = sock_recv_all(sock, timeout=3.0)
    logging.info(f'[APP] Echo: {repr(response)}')

    if test_data in response:
        print(f'  [✓] Echo korrekt: {repr(response)}')
        results.append(('Echo nach Auth', True, response.decode('ascii', errors='replace')))
    else:
        print(f'  [✗] Echo fehlerhaft: {repr(response)}')
        results.append(('Echo nach Auth', False, response.decode('ascii', errors='replace')))

    return results


# ─── MAIN ──────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='RN4678 PIN-Prozess Simulation — Beide Seiten über IF820 + Windows BT')
    parser.add_argument('-d', '--debug', action='store_true',
                        help='Debug-Logging aktivieren')
    parser.add_argument('-p', '--pin', type=str, default=None,
                        help='6-stellige Geräte-PIN (ohne GUI-Abfrage)')
    args, _ = parser.parse_known_args()

    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s',
        level=logging.DEBUG if args.debug else logging.INFO)

    # 1) Verbindungsmodus wählen
    mode = ask_connection_mode()
    if mode is None:
        logging.info('Abgebrochen.')
        return

    # 2) Geräte-PIN festlegen
    if args.pin and len(args.pin) == 6 and args.pin.isdigit():
        device_pin = args.pin
    else:
        device_pin = ask_pin()

    # 3) Board initialisieren
    boards = If820Board.get_connected_boards()
    if not boards:
        messagebox.showerror('Fehler', 'Kein IF820 Board gefunden!')
        return

    logging.info('Board init...')
    board = boards[0]
    board.open_and_init_board(False)
    board.close_ports_and_reset()
    time.sleep(5)
    board.open_and_init_board()

    # 4) Je nach Modus: Factory Reset oder direkt MAC lesen
    if mode == 'first':
        mac = factory_reset(board)
        show_pairing_instructions(mac)
    else:
        mac = read_bt_mac(board)
        logging.info(f'[IF820] BT Classic MAC: {mac}')

    # 5) RFCOMM-Verbindung vom PC aufbauen (App-Seite)
    logging.info(f'[APP] RFCOMM-Verbindung zu {mac}...')
    sock = rfcomm_connect(mac)
    if sock is None:
        messagebox.showerror('Fehler',
                             f'RFCOMM-Verbindung zu {mac} fehlgeschlagen.\n\n'
                             'Mögliche Ursachen:\n'
                             '- Gerät nicht in Windows gekoppelt\n'
                             '- COM-Port konfiguriert (blockiert Socket)\n'
                             '- Bluetooth am PC aus')
        return

    # 6) Warte auf BT_CONNECTED Event auf IF820-Seite
    wait_for_bt_connected(board)

    # 7) RN4678-Firmware-Simulation im Hintergrund starten (UART-Seite)
    device_sim = RN4678DeviceSimulation(board, device_pin)
    sim_thread = threading.Thread(target=device_sim.run, name='RN4678-Sim',
                                  daemon=True)
    sim_thread.start()

    # 8) App-seitige Tests ausführen (Socket-Seite)
    try:
        results = run_app_test(sock, device_pin)
    finally:
        device_sim.stop()
        sock.close()
        sim_thread.join(timeout=3.0)

    # 9) Ergebnis-Zusammenfassung
    print()
    print('═' * 60)
    print('  ERGEBNIS — RN4678 PIN-Protokoll Simulation')
    print('═' * 60)
    all_passed = True
    for name, passed, detail in results:
        status = '✓ PASS' if passed else '✗ FAIL'
        print(f'  [{status}] {name}')
        if not passed:
            all_passed = False
            print(f'         Detail: {repr(detail[:80])}')
    print('═' * 60)

    if all_passed:
        print('  ALLE TESTS BESTANDEN')
    else:
        print('  EINIGE TESTS FEHLGESCHLAGEN')
    print()

    result_text = (
        f'Gerät: EZ-Serial "{mac[-8:]}"\n'
        f'MAC: {mac}\n'
        f'PIN: {device_pin}\n\n'
        + '\n'.join(f'{"✓" if p else "✗"} {n}' for n, p, _ in results)
    )
    messagebox.showinfo('RN4678 Simulation — Ergebnis', result_text)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\n[SIM] Abgebrochen (Ctrl+C).')
        logging.info('Stopped.')
