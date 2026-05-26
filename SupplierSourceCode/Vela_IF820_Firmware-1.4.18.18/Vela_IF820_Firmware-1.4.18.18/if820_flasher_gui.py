#!/usr/bin/env python3

"""
Python GUI for IF820 Firmware Upgrade

pyinstaller command to produce a single executable file:

pyinstaller --clean --windowed --noconfirm  --onefile --add-data "img/IF820_fw_upgrade_header.png:img" --add-data "files/v1.4.18.18_int-ant/minidriver-20820A1-uart-patchram.hex:files/v1.4.18.18_int-ant" --collect-all pyocd  --collect-all cmsis_pack_manager -p common_lib/libraries if820_flasher_gui.py

"""


import threading
import os
import wx
import logging
import sys
sys.path.append('./common_lib/libraries')
from If820Board import If820Board


LOG_MODULE_HCI_PORT = 'hci_port'
PROGRAM_TITLE = 'Vela IF820 Firmware Upgrade Tool'
LOGGING_FORMAT = '%(asctime)s | %(levelname)s | %(message)s'
VERSION = '2.1.0'

def resource_path(relative_path):
    """ Get absolute path to resource, works for dev and for PyInstaller """
    base_path = getattr(sys, '_MEIPASS', os.path.dirname(
        os.path.abspath(__file__)))
    return os.path.join(base_path, relative_path)


class WxTextCtrlLogHandler(logging.Handler):
    def __init__(self, ctrl: wx.TextCtrl):
        logging.Handler.__init__(self)
        self.ctrl = ctrl

    def emit(self, record):
        s = self.format(record) + '\n'
        wx.CallAfter(self.ctrl.AppendText, s)


HEADER_IMG = resource_path(f'img{os.sep}IF820_fw_upgrade_header.png')
MINIDRIVER = resource_path(f'files{os.sep}v1.4.18.18_int-ant{os.sep}minidriver-20820A1-uart-patchram.hex')


class Window(wx.Frame):
    """
    Class definition for the gui window and gui elements
    """

    # Initialization for window, panel, gui elements
    def __init__(self, *args, **kw):
        super(Window, self).__init__(*args, **kw)

        # Create a panel for gui elements
        panel = wx.Panel(self)

        # We start with a vertical BoxSizer for positioning ALL GUI elements
        vbox = wx.BoxSizer(wx.VERTICAL)

        # Place a nice image to the top
        bmpl = wx.Image(HEADER_IMG, wx.BITMAP_TYPE_ANY)
        img_header = wx.StaticBitmap(panel, -1, bmpl, (0, 0))
        hbox_image = wx.BoxSizer(wx.HORIZONTAL)
        hbox_image.Add(img_header)
        vbox.Add(hbox_image)

        # Find available boards from the system
        boards = If820Board.get_connected_boards()
        if len(boards) <= 0:
            wx.MessageBox('No boards connected.\n\nConnect an IF820 board and try again.',
                          PROGRAM_TITLE, wx.OK | wx.ICON_ERROR)
            wx.Exit()

        # Show text and combo box for board selection
        st_selboard = wx.StaticText(panel, label="Select Board:")
        font = st_selboard.GetFont()
        font = font.Bold()
        st_selboard.SetFont(font)
        board_list = []
        self.cb_selboard = wx.ComboBox(
            panel, style=wx.CB_READONLY, value=boards[0].probe.id, choices=board_list, size=(180, -1))
        for board in boards:
            self.cb_selboard.Append(board.probe.id, board)
        self.cb_selboard.SetSelection(0)

        # Add checkbox for chip erase
        self.ch_chiperase = wx.CheckBox(panel, label='Chip erase')

        # Add GUI elements into a horizontal BoxSizer and add to the main vertical BoxSizer
        hbox_selboard = wx.BoxSizer(wx.HORIZONTAL)
        hbox_selboard.Add(st_selboard, wx.SizerFlags().Border(wx.RIGHT, 10))
        hbox_selboard.Add(self.cb_selboard)
        hbox_selboard.AddStretchSpacer()
        hbox_selboard.Add(self.ch_chiperase)
        vbox.Add(hbox_selboard, flag=wx.EXPAND | wx.ALL, border=10)

        # Add file picker for firmware selection
        st_selfirmware = wx.StaticText(panel, label="Select firmware:")
        font = st_selfirmware.GetFont()
        font = font.Bold()
        st_selfirmware.SetFont(font)
        vbox.Add(st_selfirmware, flag=wx.LEFT | wx.RIGHT, border=10)
        self.picker_firmware = wx.FilePickerCtrl(
            panel, message='Select firmware hex file', wildcard='Firmware files (.hex, .hcd)|*.hex;*.hcd')
        vbox.Add(self.picker_firmware, flag=wx.EXPAND |
                 wx.LEFT | wx.RIGHT | wx.BOTTOM, border=10)

        # Add a log output textctrl
        st_logoutput = wx.StaticText(panel, label="Log output:")
        font = st_logoutput.GetFont()
        font = font.Bold()
        st_logoutput.SetFont(font)
        vbox.Add(st_logoutput, flag=wx.LEFT, border=10)
        self.tx_logoutput = wx.TextCtrl(
            panel, style=wx.TE_MULTILINE | wx.TE_READONLY | wx.TE_DONTWRAP, size=(440, 100))
        log_font = wx.Font(10, wx.FONTFAMILY_MODERN, wx.FONTSTYLE_NORMAL,
                           wx.FONTWEIGHT_NORMAL, False, u'Consolas')
        self.tx_logoutput.SetFont(log_font)
        vbox.Add(self.tx_logoutput, proportion=1,
                 flag=wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, border=10)

        # Add buttons to start firmware upgrade and quit
        self.bt_fwupgrade = wx.Button(panel, -1, "Upgrade Firmware")
        self.bt_fwupgrade.Bind(wx.EVT_BUTTON, self.ButtonUpdateFirmwareEvent)
        bt_quit = wx.Button(panel, -1, "Quit")
        bt_quit.Bind(wx.EVT_BUTTON, self.OnExit)

        # Add horizontal BoxSizer to show buttons side by side
        hbox_buttons = wx.BoxSizer(wx.HORIZONTAL)
        hbox_buttons.Add(self.bt_fwupgrade)
        hbox_buttons.AddStretchSpacer()
        hbox_buttons.Add(bt_quit)
        vbox.Add(hbox_buttons, flag=wx.EXPAND | wx.ALL, border=10)

        # Finally enable the vertical sizer
        panel.SetSizer(vbox)

        # Create a status bar and populate with version number
        self.CreateStatusBar()
        self.SetStatusText(f"Version {VERSION}")

        # Add a log handler to the root logger to show log messages in the GUI
        log_handler = WxTextCtrlLogHandler(self.tx_logoutput)
        formatter = logging.Formatter(LOGGING_FORMAT)
        log_handler.setFormatter(formatter)
        root = logging.getLogger()
        root.addHandler(log_handler)

    def flash_firmware_done(self):
        """Re-enable GUI elements after firmware upgrade is done.
        """
        self.bt_fwupgrade.Enable()
        self.cb_selboard.Enable()
        self.picker_firmware.Enable()
        self.ch_chiperase.Enable()

    def flash_firmware(self):
        """Task to flash firmware to the board.
        """
        try:
            self.selected_board.flash_firmware(
                minidriver=MINIDRIVER, firmware=f'{self.picker_firmware.GetTextCtrl().GetValue()}', chip_erase=self.ch_chiperase.GetValue(), verify=True)
        except Exception as e:
            # Log any error
            logging.error(e)
            self.selected_board.cancel_flash_firmware()
        wx.CallAfter(self.flash_firmware_done)

    def ButtonUpdateFirmwareEvent(self, event):
        """Firmware upgrade button event handler.
        """
        self.bt_fwupgrade.Disable()
        self.cb_selboard.Disable()
        self.picker_firmware.Disable()
        self.ch_chiperase.Disable()
        self.selected_board = self.cb_selboard.GetClientData(
            self.cb_selboard.GetSelection())
        self.tx_logoutput.AppendText(
            f'\n\nFlashing board {self.selected_board.probe.id}...\n')
        # Run programming in a separate thread to avoid blocking the GUI
        thread = threading.Thread(target=self.flash_firmware)
        thread.daemon = True
        thread.start()

    def OnExit(self, event):
        """Button press event to close the app
        """
        self.Close(True)


if __name__ == '__main__':
    logging.basicConfig(
        format=LOGGING_FORMAT, level=logging.INFO)

    # Create wxPython app, window and show it and start main loop to wait for user interaction
    app = wx.App()
    frm = Window(None, title=PROGRAM_TITLE,
                 style=wx.DEFAULT_FRAME_STYLE ^ wx.RESIZE_BORDER, size=(500, 600))
    frm.Show()
    app.MainLoop()
