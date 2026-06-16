//-----------------------------------------------------------------------------
//  FILE: sictxt.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Sprachunabh�ngige Texte 
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

// Nicht zu �bersetzende Textarrays
text T_nil[]							=	"";					// not in list Zeiger
text T_viasis[]						= "VIASIS";		// als const char wg. USB Treiber Verzeichnisname etc.
text T_version[8]					= "5.78";			// Programmvariante jetzt immer mit viaspeedcam parameter einstellbar, sc Varianten entfallen
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
text * const L_start[] = {T_2LF,T_name,T_tver,T_version,T_LF,T_coprght,T_nil};				// Einschaltmeldung ohne Hauptmen�
text * const L_winit[] = {T_2LF,T_winit,T_LF,T_nil};								// Werkinitialisierung
text * const L_conf[]	= {T_conf,T_serial,T_dpkt,T_nil};							// Werksmen� Konfig.
text * const L_info[]	= {T_name,T_tver,T_version,T_col,T_nil}; 			// Information
text * const L_dpp[]	= {T_ende,T_LF,T_plsmin,T_nil};								// DPP einstellen
text * const L_gsm[]	= {T_tmen,T_gsm,T_nil};												// Zeitplanung GSM/Email
text * const L_mime[]	= {T_vmime,T_mcont,T_mpart,T_nil};						// Email Mime Header
text * const L_mbnd[]	= {T_LF,T_ddash,T_mbound,T_LF,T_mcont,T_nil};	// Email Mime boundary und Content	


