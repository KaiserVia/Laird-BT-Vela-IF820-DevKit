//-----------------------------------------------------------------------------
//  FILE: sictxt.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Sprachunabhängige Texte 
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
//-----------------------------------------------------------------------------
//	COMPILATION: THUMB CODE
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   02.02.2015
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------


#include "../sictxt.h"	

// Konstanten
const ushort Def_sym_font[NOSYM] = {430,450,460,470,480,500,501,503};	// Voreingestellte Symbole
const char sense_tab[5]={6,4,2,0,1};			// Kanalauswahl MUX IC14 und IC19 Signalverst�rkung

// Nicht zu übersetzende Textarrays
text T_nil[]							=	"";					// not in list Zeiger
text T_viasis[]						= "VIASIS";		// als const char wg. USB Treiber Verzeichnisname etc.
text T_version[8]					= "5.s2";			// 5.s2 (TEST, Branch DevelopLightSensor): Mainboard V1/V2 per Lichtsensor erkennen, in fp.radar Bit 3:2 speichern, beim Hochfahren nur kurz pruefen (I17006 bei Wechsel); 5.138 (TEST): Zusammenfuehrung main + 5.L2 (Joachim 5.60, Mainboard V2/Via-2) + 5.s1 (Lichtsensor VEML7700 steuert die Anzeigehelligkeit); 5.L2 (TEST): test_i2c_device() typ vorbelegt (war uninitialisiert, wenn das Device nicht antwortet); 5.L1 (TEST): Integration Joachim 5.56-5.60 (Mainboard V2, Via-2-Radarempfaenger) - Radar-Empfaenger per Selbsttest erkannt (fp.radar), Pins/Transceiver/HF-Messung/Menues je nach Typ, Sprachbits OEM/Sprachpaket in fp.sprache; 5.137 (TEST): 5.137 (TEST): CONNECTION (P0.19) als zusaetzlicher Ausloeser - BT-Verbindung waehrend LTE beendet MQTT geordnet (/leave), auch wenn CYSPP HIGH bleibt; 5.136 (TEST): CONNECTION-Monitor (Testmenue 3->7) meldet GSM-Thread-Wechsel und P0.19 auch waehrend LTE (LTE-BLE-Test, temporaer); 5.135 (TEST): Verbergen im Hochlauf sendet /AX erst bei CTS LOW (kein W11015 mehr bei jedem Einschalten); 5.134 (TEST): keine BT-Sperre mehr, solange GSM/LTE UART1 haelt (BT hat Vorrang, beendet MQTT); 5.133 (TEST): Verbergen im Hochlauf gibt bei CYSPP LOW waehrend des Modulstarts nicht sofort auf (Board 99SC9923), Update-Sperre vor der XMODEM-Aufforderung; 5.132 (TEST): Phase 3 CYSPP-Zustandsmaschine - unsichtbar waehrend Firmware-Update (RS232/USB) und GSM-Sitzung, /UCP vor dem Abo wieder an; 5.131 (TEST): Phase 2 CYSPP-Zustandsmaschine - IF820 im Hochlauf unsichtbar (/AX, SBTP C=0 im RAM), sichtbar am Ende (/A, SBTP C=1); 5.130 (TEST): Phase 1 CYSPP-Zustandsmaschine (Ereignis-Parser, Zustand/Transport/Trenngrund, Ymodem-Abbruch bei Linkverlust), Fehler 4 (keine Schein-Verbindung bei stromlosem Modul); 5.129 (TEST): Testmenue 8->7 "Modulbefehl senden" (Phase 0 CYSPP-Zustandsmaschine); 5.128 (TEST): BT-UART nach jeder Reset-Art aktiv (3:15-Reboot, Brown-out), keine BT-Neuerkennung bei aktiver Verbindung; 5.127 (TEST): IF820-UART1 im Leerlauf aktiv lassen (RTS LOW) statt abschalten; 5.126: /UCP (BT_UCP_ENABLE) wieder aus, BLE scheiterte mit 5.125 auf 26SC9983; 5.125: BLE-Verbindungsintervall per /UCP anfordern; vorher: Programmvariante jetzt immer mit viaspeedcam parameter einstellbar, sc Varianten entfallen
text T_symgr1[3][SYGLEN] 	= {"30,50,60", "70,80,!", "Smileys"};
text T_defsym[NOSYM][4]		= {"30","50","60","70","80","!",":-(",":-)"};
text T_uall[2][5]					= {"km/h","mph"};
text T_12mode[5][8] 			= {"off","on","Plus","Plus FT","viatext"};
text T_dimsel[3][13]			= {"permanent","if detection","if LED lit"};
text T_OKNZ[]							= "OK\r\n";		// GSM Modem OK
text T_CRLF[]							= "\r\n";			// CRLF
text T_def_hw_rev[3]			= "J2";				// Default Hardware Revision
text T_nets[3][5]					= {"GSM","UMTS","LTE"};
text T_service[4][7]			= {"APN","SMS","Server","EMAIL"};
text T_server[2][5]				= {"SMTP","MQTT"};
text T_secret[]						= "secret";

#if (VSPCAM)
text T_vsc[]							= "VSC";			// Via speed cam Option aktiviert
text T_vspcam[3][6]				= {"off","500ms","250ms"};
#endif

// Einsprachige Ereignistexte
text T_watchdog[]	= "Watchdog reset";
text T_update[]		= "Firmware update";
text T_certsav[]	= "Certificate loaded";
text T_mqttcon[]	= "MQTT connect";
text T_reb[]			= "Reboot";

const ushort evno[MAXMESSAGES] = {	// Liste der Ereignisnummern
POWER_ON, 								// 1.
WERK_INIT,								// 2.
WATCHDOG_RESET,						// 3.
FIRMWARE_UPDATE,					// 4.
SOMMERZEIT, 							// 5.
WINTERZEIT, 							// 6.
MESSBEGINN, 							// 7.
MEMFULL,									// 8.
DEF_PARAMETER,						// 9.
PARAMETER_INIT,						// 10.
BATTERYLOW,								// 11.
CERT_SAVED,								// 12.	
MQTT_CONNECTED,						// 13.
REBOOT	
};												

text * const evtxt [MAXMESSAGES] = {	// Liste der Ereignistexte
T_poweron,								// 1.
T_winit,									// 2.
T_watchdog,								// 3.
T_update,									// 4.		
T_sommer,									// 5.
T_winter,									// 6.
T_messstart,							// 7.
T_memfull,								// 8.
T_defpar,									// 9.
T_parainit,								// 10.
T_batlow,									// 11.
T_certsav,								// 12.	
T_mqttcon, 								// 13.
T_reb	
};							
 
const ushort errno [MAXERRORTEXT]= {	// Liste der Fehlernummern
XMODEM_REMOTECANCEL,				// 1.
XMODEM_RETRYEXCEED,					// 2.	
XMODEM_OUTOFSYNC,						// 3.	
XMODEM_FILE_TOO_LONG,				// 4.
XMODEM_PACKET_TYPE,					// 5.	
XMODEM_TIMEOUT,							// 6.
IAP_ERROR_NO_ARM_CODE,			// 7.
CRC_ERROR,									// 8.
IAP_PROGRAMM_ERROR,					// 9.
BROWNOUT_ERROR,							// 10.
FLASH_ERROR,								// 11.
I2C_BUS_ERROR,							// 12.
I2C_DEVICE_ERROR,						// 13.
I2C_DPP_ERROR,							// 14.							
TCA6507_ERROR,							// 15.		
RTC_ERROR,									// 16.
BLUETOOTH_ERROR,						// 17.
USB_ERROR,									// 18.
PARAMETER_INIT_ERROR,				// 19.
HARDWARE_ERROR,							// 20. 
GSM_ERROR,	   							// 21.
SIM_ERROR,	  							// 22.
COMMUNICATION_TIMEOUT,			// 23.
USB_WRITE,									// 24.
PARAMETER_DEF_ERROR,				// 25.
GPS_ERROR,									// 26.
GPS_TIME_ERROR,							// 27.
MQTT_ERROR									// 28.
};					

text * const errtxt [MAXERRORTEXT] = {	// Liste der Fehlertexte
E_xmcancel,								// 1.
E_xmretry,								// 2.
E_xmsync,									// 3.
E_xmlong,									// 4.
E_xmpacket,								// 5.
E_xmtimeout,							// 6.
E_iapfile,								// 7.
E_crc,										// 8.
E_program,								// 9.
E_brownout,							  // 10.
E_flash,									// 11.
E_i2cbus,									// 12.
E_i2cdev,									// 13.
E_i2cdpp,									// 14.					
E_tca6507,								// 15.
E_rtc,										// 16.
E_bt,								    	// 17.
E_usb,										// 18.
T_parainit,								// 19. 
E_hard,										// 20.
E_gsm,										// 21.
E_simpin,									// 22.
E_com,										// 23.
E_w_usb,									// 24.
E_pardef,									// 25.
E_gps,										// 26.
E_gpsfix,									// 27.
E_mqtt										// 28.
};

// Zeichenkettenlisten
text * const L_start[] = {T_2LF,T_name,T_tver,T_version,T_LF,T_coprght,T_nil};				// Einschaltmeldung ohne Hauptmenue
text * const L_winit[] = {T_2LF,T_winit,T_LF,T_nil};								// Werkinitialisierung
text * const L_conf[]	= {T_conf,T_serial,T_dpkt,T_nil};							// Werksmenue Konfig.
text * const L_info[]	= {T_name,T_tver,T_version,T_col,T_nil}; 			// Information
text * const L_dpp[]	= {T_ende,T_LF,T_plsmin,T_nil};								// DPP einstellen
text * const L_gsm[]	= {T_tmen,T_gsm,T_nil};												// Zeitplanung GSM/Email
text * const L_mime[]	= {T_vmime,T_mcont,T_mpart,T_nil};						// Email Mime Header
text * const L_mbnd[]	= {T_LF,T_ddash,T_mbound,T_LF,T_mcont,T_nil};	// Email Mime boundary und Content	


