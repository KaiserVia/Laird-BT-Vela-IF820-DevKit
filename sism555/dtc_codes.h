/* dtc_codes.h - Feste Diagnose-Codes (DTC), erzeugt aus Diagnose-Codes.csv
   Ersetzt das zeilenbasierte dtc.h-Schema (DTCBASE + __LINE__).
   Jeder Code ist eindeutig, stabil und im CSV-Katalog dokumentiert.
   Encoding: ISO-8859-1, CRLF */

#ifndef DTC_CODES_H_
#define DTC_CODES_H_

/* --- Makros (ersetzen dtc.h) --------------------------------------- */
#undef  puterror
#undef  dtcerr
#define puterror(code,v)  puterror_dtc((code),(v))
#define dtcerr(code)      dctext(code)

/* --- Subsystem 11: Bluetooth (btio.c) ------------------------------ */
#define DTC_BT_PING             11001   /* PING keine Antwort */
#define DTC_BT_FLOWCTL_SET      11002   /* Flow Control setzen fehlgeschlagen */
#define DTC_BT_NAME_SET         11003   /* Name setzen fehlgeschlagen */
#define DTC_BT_NAME_VERIFY      11004   /* Name-Verifikation fehlgeschlagen */
#define DTC_BT_NOT_FOUND        11005   /* Kein BT-Modul gefunden */
#define DTC_BT_UNKNOWN_TYPE     11006   /* Unbekannter BT-Modultyp */
#define DTC_BT_FLOWCTL_VERIFY   11007   /* Flow Control Verifikation fehlgeschlagen */
#define DTC_BT_NO_SERIAL        11008   /* Seriennummer nicht gesetzt */
#define DTC_BT_UART_INIT        11009   /* BT UART Init fehlgeschlagen */
#define DTC_BT_NOT_CONFIGURED   11010   /* BT Modul nicht konfiguriert */
#define DTC_BT_AT_FAIL          11011   /* BT AT-Befehl fehlgeschlagen */
#define DTC_BT_BTT_FAIL         11012   /* BT Verbindungstest fehlgeschlagen */
#define DTC_BT_CR_FAIL          11013   /* BT Verbindungsaufbau fehlgeschlagen */
#define DTC_BT_PIN_FAIL         11014   /* BT PIN setzen fehlgeschlagen */

/* --- Subsystem 12: GSM/LTE (gsmio.c) ------------------------------ */
#define DTC_GSM_NO_RESP         12001   /* Modem keine Antwort */
#define DTC_GSM_SIM_PIN         12002   /* SIM-PIN-Problem */
#define DTC_GSM_CONN_TIMEOUT    12003   /* GSM Verbindung Timeout */
#define DTC_GSM_CONN_FAIL       12004   /* GSM Verbindungsfehler */
#define DTC_GSM_DATA_FAIL       12005   /* GSM Datentransfer fehlgeschlagen */
#define DTC_GSM_SMS_LEN         12006   /* GSM SMS Laenge fehlerhaft */
#define DTC_GSM_SMS_FAIL        12007   /* GSM SMS Senden fehlgeschlagen */
#define DTC_GSM_CRC_ERR         12008   /* Firmware CRC-Fehler GSM */

/* --- Subsystem 13: Menue/Kommunikation (sicom.c) ------------------- */
#define DTC_SI_PARAM_INIT       13001   /* Parametertransfer fehlerhaft */
#define DTC_SI_NO_FLASH         13002   /* Flash fehlt oder zu klein */
#define DTC_SI_XMODEM_ERR       13003   /* Firmware-Transfer Xmodem Fehler */
#define DTC_SI_IAP_PROG_ERR     13004   /* IAP Programmierfehler */
#define DTC_SI_CRC_ERR          13005   /* Firmware CRC-Fehler */
#define DTC_SI_NO_FIRMWARE      13006   /* Keine gueltige Firmware */
#define DTC_SI_GSM_UPLOAD_FAIL  13007   /* Firmware-Upload GSM Fehler */
#define DTC_SI_PARAM_DEF_ERR    13008   /* Default-Parameter Fehler */
#define DTC_SI_I2C_EXP_ERR      13009   /* I2C Expander Init fehlgeschlagen */
#define DTC_SI_COMM_ERR         13010   /* Kommunikations-Protokollfehler */

/* --- Subsystem 14: MQTT (mqtt.c) ----------------------------------- */
#define DTC_MQ_CONN_FAIL        14001   /* MQTT Verbindung fehlgeschlagen */
#define DTC_MQ_IAP_PROG_ERR     14002   /* IAP Programmierfehler MQTT */

/* --- Subsystem 15: GPS (gpsio.c) ----------------------------------- */
#define DTC_GPS_NO_RESP         15001   /* GPS Keine Antwort */
#define DTC_GPS_INIT_FAIL       15002   /* GPS Initialisierung fehlgeschlagen */
#define DTC_GPS_CMD_FAIL        15003   /* GPS Kommando fehlgeschlagen */
#define DTC_GPS_ERROR           15004   /* GPS Fehler */

/* --- Subsystem 16: Flash/Speicher (flash.c) ------------------------ */
#define DTC_FL_WRITE_FAIL       16001   /* Schreiben fehlgeschlagen */
#define DTC_FL_PARAM_INIT       16002   /* Parameter-Initialisierung fehlgeschlagen */

/* --- Subsystem 17: System (main.c) --------------------------------- */
#define DTC_MA_I2C_BUS          17001   /* I2C-Bus Fehler beim Start */
#define DTC_MA_I2C_DEVICE       17002   /* I2C Geraet fehlt */
#define DTC_MA_I2C_DPP          17003   /* I2C DPP fehlt */
#define DTC_MA_TCA6507          17004   /* LED-Treiber TCA6507 fehlt */

/* --- Subsystem 18: Hardware/libtool (libtool.c) -------------------- */
#define DTC_LIB_GSM_ERR         18001   /* GSM-Fehler in libtool */
#define DTC_LIB_TCA6507         18002   /* LED-Treiber TCA6507 Fehler */
#define DTC_LIB_HW_ERR          18003   /* Hardware-Fehler */
#define DTC_LIB_COMM_TIMEOUT    18004   /* Kommunikations-Timeout */

/* --- Subsystem 19: USB (USB_tools.c) ------------------------------- */
#define DTC_USB_DEV_INIT        19001   /* USB Device Init fehlgeschlagen */
#define DTC_USB_DEV_CONN        19002   /* USB Device Connect fehlgeschlagen */
#define DTC_USB_DEV_DISC        19003   /* USB Device Disconnect fehlgeschlagen */
#define DTC_USB_DEV_UNINIT      19004   /* USB Device Uninit fehlgeschlagen */
#define DTC_USB_FS_INIT         19005   /* USB Dateisystem Init fehlgeschlagen */
#define DTC_USB_FS_INFO         19006   /* USB Dateisystem Info fehlgeschlagen */
#define DTC_USB_MEM_FULL        19007   /* USB Speicher voll */
#define DTC_USB_FILE_ERR        19008   /* USB Datei-Fehler */
#define DTC_USB_HOST_INIT       19009   /* USB Host Init fehlgeschlagen */
#define DTC_USB_NO_FW           19010   /* Keine Firmware auf USB-Stick */
#define DTC_USB_PARAM_INIT      19011   /* Parameter-Init fehlgeschlagen USB */
#define DTC_USB_ERROR           19012   /* USB allgemeiner Fehler */
#define DTC_USB_IAP_PROG_ERR    19013   /* IAP Programmierfehler USB */

/* --- Subsystem 20: Systemtest (sictst.c) --------------------------- */
#define DTC_TST_I2C_DPP         20001   /* I2C DPP Test fehlgeschlagen */
#define DTC_TST_I2C_EXPANDER    20002   /* I2C Expander Test fehlgeschlagen */
#define DTC_TST_CALIB_FAIL      20003   /* Kalibrierung Test fehlgeschlagen */
#define DTC_TST_SENSOR_FAIL     20004   /* Sensor Test fehlgeschlagen */
#define DTC_TST_RTC_FAIL        20005   /* RTC Test fehlgeschlagen */
#define DTC_TST_I2C_IC34        20006   /* I2C IC34 Expander Fehler */
#define DTC_TST_TCA6507         20007   /* LED-Treiber Test fehlgeschlagen */

#endif /* DTC_CODES_H_ */
