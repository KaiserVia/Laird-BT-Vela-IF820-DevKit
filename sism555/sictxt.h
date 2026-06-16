/* Header Datei für sistxt.c, Projekt sis3003m */

#ifndef SISTXT_H_
#define SISTXT_H_

// Aliase für compiler
#include "hard.h"	

/* Tabelle Aliase Sprachdatei(-erweiterungen)	*/
#define d_f_i				1		// deutsch, französisch, italienische Texte
#define e_f_nl			2		// englisch, französisch, niederländisch
#define e_cz_sl			3		// englisch, französisch, slowenisch
#define e_es_pt 		4	  // englisch, spanisch, portugiesische Texte
#define d_f_pl			5		// deutsch, französisch, polnische Texte
#define e_es_i			6		// englisch, spanische, italienische Texte

#define no_oem			0
#define	Krycer			1
#define	Pol					2

#define LANGUAGE 		d_f_i
#define VSPCAM			1					// Viaspeedcam Programmvariante
#define OEM					no_oem

#define ANZSPRACHEN 	3				// Anzahl definierter Programmsprachen

// Definitionen Gerätenamen und Copyright 
#if (OEM == Krycer)
 #define T_name				"Trafficheck 3003M - "
 #define T_coprght		"Copyright Krycer bvba-sprl 2026\n"
#elif (OEM == Pol)
 #define T_name 			"V3003M - "
 #define T_coprght		"Copyright POL wegbebakening bv 2026\n"
#else
 #define T_name				"Viasis 3003M - "
 #define T_coprght		"Via traffic controlling 2026\n"
#endif

#if (LANGUAGE == e_cz_sl)
	extern text T_tag[7][29];
  extern text T_zone[4][22];
	extern text T_offon[2][22];
	extern text T_brsel[3][52];
	extern text T_spritm[ANZSPRACHEN][10];
#elif (LANGUAGE == e_f_nl)
	extern text T_tag[7][29];
	extern text T_zone[4][20];
	extern text T_offon[2][20];
	extern text T_brsel[3][52];
	extern text T_spritm[ANZSPRACHEN][10];
#elif ((LANGUAGE == d_f_i)|(LANGUAGE == d_f_pl))
  extern text T_tag[7][29];
	extern text T_zone[4][28];
	extern text T_offon[2][28];
	extern text T_brsel[3][52];
	extern text T_spritm[ANZSPRACHEN][10];
#elif ((LANGUAGE == e_es_pt) | (LANGUAGE == e_es_i) )
	#define WTAG_KURZ		3				// Wochentagskuerzel 3 Buchstaben lang
	extern text T_tag[7][35];
	extern text T_zone[4][25];
	extern text T_offon[2][25];
	extern text T_brsel[3][56];
	extern text T_spritm[ANZSPRACHEN][11];	
#endif

#ifndef	WTAG_KURZ
 #define WTAG_KURZ		2					// Wochentagskuerzel 2 Buchstaben lang  
#endif

// Extern in C Dateien definierte mehrsprachige Texte
extern text T_ja[ANZSPRACHEN];
extern text T_nein[ANZSPRACHEN];
extern text T_jn[];
extern text T_tver[];
extern text T_err[];
extern text T_winit[];
extern text T_main[];
extern text T_zuruck[];
extern text T_ausw[];
extern text T_estmenu[];
extern text T_kein[];
extern text T_flash[];
extern text T_erase[];
extern text T_write[];
extern text T_read[];
extern text T_page[];
extern text T_ok[];
extern text T_cont[];
extern text T_check[];
extern text T_licht[];
extern text T_optmenu[];
extern text T_vopt[];	
extern text T_vmin[];
extern text T_vmax[];
extern text T_vblk[];
extern text T_vcolor[];
extern text T_smenu[];
extern text T_mcyc[];
extern text T_acyc[];
extern text T_nk[];	
extern text T_ende[];
extern text T_time[];
extern text T_date[];
extern text T_fdate[];
extern text T_ftime[];
extern text T_fstime[];
extern text T_infomenu[];
extern text T_serialno[];
extern text T_comment[];
extern text T_protocol[];
extern text T_delete[];
extern text T_weret[];
extern text T_eraseall[];
extern text T_batt[];
extern text T_anzmw[];
extern text T_daus[];
extern text T_anzpr[];
extern text T_bdir[];
extern text T_vlim[];
extern text T_units[];
extern text T_dismod[];
extern text T_voff[];
extern text T_vcor[];
extern text T_tmen[];
extern text T_pset[];
extern text T_somwin[];
extern text T_wtag[];
extern text T_ontage[];
extern text T_tein[];
extern text T_taus[];
extern text T_led[];
extern text T_ext[];
extern text T_thr[];
extern text T_symb[];
extern text T_symled[];
extern text T_swgrp[5][SWGLEN];
extern text T_swtyp[4][SWLEN];	
extern text T_vmmin[];
extern text T_brght[];
extern text T_parakt[];
extern text T_hell[];
extern text T_col2[];
extern text T_sim[];
extern text T_modem[];
extern text T_btmenu[];
extern text T_btprot[];
extern text T_ninst[];
extern text T_btbda[];
extern text T_btname[];
extern text T_btlst[];
extern text T_dellst[];
extern text T_noacc[];
extern text T_gsmmen[];
extern text T_gsmcom[];
extern text T_pinno[];
extern text T_txmail[];
extern text T_txsms[];
extern text T_mailset[];
extern text T_status[];
extern text T_mdaten[];
extern text T_memfull[];
extern text T_batlow[];
extern text T_emalarm[];
extern text T_smsno[];	
extern text T_smsalarm[];
extern text T_snow[];
extern text T_smsset[];
extern text T_syserr[];
extern text T_rsens[];
extern text T_change[];
extern text T_inter[];
extern text	T_symname[];
extern text	T_symgrn[];
extern text T_symdef[];
extern text T_newgrp[];
extern text T_notcon[];
extern text T_usbdis[];
// Mehrsprachige Texte GSM/GPRS Menüs etc.
extern text T_yconf[];
extern text T_vemail[];
extern text T_mailcyc[5][44];
extern text T_mtag[];
extern text T_service[4][7];
extern text T_server[2][5];
extern text T_mfstd[];
extern text T_mnow[];
extern text T_mgps[];
extern text T_gpsmen[];
extern text T_ctrlz[];
extern text T_gga[];
extern text T_tposfix[];
extern text T_gpsanz[];
extern text T_betr[];	
extern text T_sstr[];
extern text T_conmqtt[];
extern text T_tzone[];
extern text T_btdis[];
extern text T_wait[];
extern text T_gpscon[];
extern text T_bereit[];
extern text T_auto[];

// Feste einsprachige Texte, Tabellen und Konstanten
extern text T_nil[];						// not in list
extern text sense_tab[5];			  // Kanalauswahl MUX IC14 und IC19 Signalverstärkung
extern text T_viasis[];
extern text T_version[8];
extern text T_12mode[5][8];
extern text T_dimsel[3][13];
extern text T_uall[2][5];
extern text T_dmodes[2][33];
extern text T_symgr1[3][SYGLEN];
extern text T_defsym[NOSYM][4];
extern const ushort Def_sym_font[NOSYM];
extern text T_OKNZ[];
extern text T_CRLF[];	
extern text T_def_hw_rev[3];			// Default Hardware Revision
extern text T_nets[3][5];					// Funknetze GSM/...
extern text T_vsc[];							// viaspeedcam
extern text T_vspcam[3][6];
extern text T_secret[];

// Geräteidentifikation und anderes Einsprachiges
#define T_ident			"\n\nviasis3004 "
#define T_viatext		"\n\nSistext"							// viatext wird bereits für alte viasis 3002 Elektronik eingesetzt
#define T_rtc		 		"\n\nRTC "
#define T_utcr			"(UTC -11..12)"

// Texte USB flash disc
#define T_fusbi			"USBINFO.TXT"

#define T_usbcf			"\nUSB host configured.\n"
#define T_usbdcon		"\nUSB host disconnected."
#define T_uMsd 			"\nError storage device unformatted."
#define T_fsys			"\nFile system: "
#define T_unk				"Unknown"
#define T_fat				"FAT"
#define T_free			"\nFree disk space: "
#define T_ucon0			"\n\nUSB flash disc connected."
#define T_fldrv			"\nUSB flash disc configured:\n"
#define T_fldis			"\nDisconnect USB flash disc now!\n"

// Texte Werkmenü
#define T_hwrev			"HW Revision"
#define T_sprache		"\nLanguage: "
#define T_sende			"Radar frequency [MHz]"
#define T_dimm			"PWM extern: "
#define T_battoff		"Voltage offset (1/10V)"
#define T_ledcol		"Led code"
#define T_parsets		"Number of parameter sets"
#define T_store			"\nStore data: "
#define T_colors		"Led colors"
#define T_spot			"Led spot"
#define T_12V				"12V extern: "
#define T_bt				"Bluetooth"
#define T_usb				"USB"
#define T_gsm				" Modem/Email"
#define T_gps				"GPS"
#define T_turn			"Turn switch"
#define T_spcam			"viaspeedcam "
#define T_cconf			"Change configuration"
#define T_symsw			"LED symbols and switches"
#define T_bright		"LED Brightness"
#define T_trx				"Amplifier & Transceiver"
#define T_kmenu			"\n5. Default parameter\n6. Component tests\n7. GSM/BT/GPS module communication"
#define T_serial		"\nSerial number"
#define	T_comp			"\nComponent tests:\n1. Write flash data\n2. Read flash pages\n3. LED font\n4. LED check\n5. I2C bus scan\n6. I2C Devices\n7. Upload files to modem"
#define T_flash_w_d	"\nSelect Data/Protocol (1/2)"
#define T_fpge			"\nPage"
#define T_plsmin		"Select channel with 1,2,3 or 4.\nChange with +/- key.\n\n ch1 ch2 ch3 ch4"
#define T_color			"- Color "
#define T_expwm			"PWM extern"
#define T_tmenu			"\n1. Adjust amplifier\n2. Transceiver on"
#define T_vtg				"Voltage"
#define	T_imp				" imp1/s imp2/s imp3/s imp4/s"
#define T_value 		"\nValue"
#define T_sbaud			"Select Baud rate\n1. 9600\n2. 115200\n3. 307200\n4. 460800"
#define T_smodem		"Select Modem\n1. Bluetooth\n2. GSM/GPRS\n3. GPS"
#define T_blue			"Select Bluetooth\n1. Laird 730SA\n2. RN4678"
#define	T_gpstyp		"GPS Typ: "

// Texte Werkmenü - I2C Komponententests
#define T_i2cfound	" devices found\n"
#define T_i2cdev	 	"I2C device "
#define T_i2creg		"\nRegister"
#define T_i2cval		"\tValue"
#define T_i2cresp		"\nResponding I2C devices:\n"
#define T_devadr		"\nDevice address"
#define T_duty			"LED duty cycle 1(0.5%) ... 200(100%) "

// Werkmenü - Menü Schalter und Symbole
#define T_conf 			"\nConfiguration:"
#define T_tsym 			"Symbol display time"
#define T_mswsym		"\n1. Define symbol groups\n2. Define symbols\n3. Test symbols\n4. Define switch groups\n5. Define switches\n6. Test switches"
#define T_nosymgr		"\nNumber of symbol groups"
#define T_tsymgr		"\nLed symbol group name: "
#define T_noswgrp		"\nNumber of switch groups"
#define T_tswgrp 		"\nSwitch group name: "
#define T_nosym 		"Number of symbols"
#define T_portno 		"Port (0..15, invers 16..31)"
#define T_font			"Font"
#define T_expno			"Expander (1/2 - IC58/None)"
#define T_nosw 			"Number of switches"
#define T_tsw				"\nSwitch name: "
#define T_tswno			"Switch number"
#define T_showsym 	"Show symbol"

// Firmware update dialog
#define T_firmup		"\n\nFirmware update"
#define T_fsend			"\nSend file with XModem(1K)\n"
#define T_rdat			"\nFile received!"
#define T_check			" Verify firmware... "
#define T_repro			"Reprogram"

// Datenausgabe/Ymodem/VTF
#define T_viafile		"#VIA_TRAFFIC_FILE#"
#define T_filefor		"FMT_VIASIS_3003"
#define T_sc 				"SC_"
#define T_numb			"\nEnter number"

// Email/SMS
#define T_roam			"ROAMING"
#define T_regd			"REGISTERED"
#define T_nreg			"NOT REGISTERED"
#define T_apn				"APN: "
#define T_user			"USER: "
#define T_pass			"PASS: "
#define T_smtpsrv		"SMTP-SERVER: "
#define T_mqttsrv		"MQTT-SERVER: "
#define T_smtpport	"SMTP-PORT"
#define T_mqttport	"MQTT-PORT"
#define T_ehost			"EHOST: "
#define T_euser			"EUSER: "
#define T_epass			"EPASS: "
#define T_mfrom			"MAILFROM: "
#define T_mto				"MAILTO: "
#define T_mcopy			"MAILCOPY: "
#define T_dmsrv			"smtp.viaxmail.de"
#define T_dehost		"viaxmail.de"
#define T_deuser		"send@viaxmail.de"
#define T_dpass			"100%-Viasis"
#define T_dmfrom		"viasis@viaxmail.de"
#define T_vmime			"MIME-Version: 1.0\n"
#define T_mcont			"Content-type: "
#define T_mpart			"multipart/mixed; boundary="
#define T_mbound		"MsgBoundVia51381"
#define T_mtplain		"text/plain; charset=iso-8859-1\n"
#define T_mattach		"application/octet-stream\nContent-Disposition: attachment; filename="
#define T_mencode		"Content-transfer-encoding: base64\n"
#define T_smssc			"SMS SERVICE CENTER: "
#define T_gsmpower	"\nPower GSM/UMTS modem. Wait..."
#define T_fsize			"File size: "
#define T_fname			"Enter file name: "

// Minutentasks und Verbindungsbericht
#define T_report		"\n\nReport: Task="
#define T_rp_conn		"; Connect="
#define T_rp_gsm		"GSM: "
#define T_rp_sup		"PWR_REG="
#define T_rp_gpwr		";PWR="
#define T_rp_csq		";CSQ="
#define T_rp_sim		";SIMPIN="
#define T_rp_wait		";Wait[min]="
#define T_rp_mail		"Mail="
#define T_rp_wmail	";WaitMail="
#define T_rp_sms		"SMS="
#define T_rp_wsms		";WaitSMS="
#define T_rp_mcom		"MQTT:Com="
#define T_rp_fail		";Fail="
#define T_rp_pend		";Pend="
#define T_rp_tstat	";t_state="
#define T_rp_pid		";PacketId="
#define T_rp_mstat	";State="
#define T_rp_man		";Man="
#define T_rp_pon		";Pon_min="
#define T_rp_mind		"MQTT_ind="
#define T_rp_mpg		";MQTT_pg="
#define T_rp_madr		";MQTT_adr="
#define T_rp_mdpg		";md_pg="
#define T_rp_mdadr	";md_adr="
#define T_rp_spg		";md_s_pg="
#define T_rp_gpspw	"GPS: PWR="
#define T_rp_gpstyp	";Typ="
#define T_rp_gpspnd	";Pend="
#define T_rp_gpsfix	";Fix="
#define T_rp_thread ";Threads="
#define T_rp_mux		";CS_MUX="
//#define T_MSP 			";MSP="
//#define T_PSP 			";PSP="
//#define T_CNTR 			";CNTR="
#define T_rp_anz		";#Pos="
#define T_rp_gpsint	";Intv="

// MQTT Server und Protokollfehler Texte
#define T_mqip			"cloud.viatraffic.com"
//#define T_mquser		"device1"
#define T_mqpass		"secret"
#define T_cmqtt			"MQTT Server connected"
#define T_conclose	"\nMQTT server closed connection"
#define T_conref		"MQTT Server connection refused - "
#define T_wprot			"Wrong MQTT version"
#define T_wclient		"Client not accepted"
#define T_nomqtt		"MQTT service not available"
#define T_baduser		"Bad user or password"
#define T_noauth		"No authorization"
#define T_unknown		"Unknown error"
#define T_discon		"MQTTS disconnect"
#define T_mqtt1			"\n\nGSM/GPS/MQTT test and debug menu\n1. Trigger GPS positioning\n2. Test radio network registration"
#define T_mqtt2			"\n5. Change MQTT data pointers\n6. PLUS text & bitmaps\n7. SIM ICCID\n8. Disconnect from Server"
#define T_mq_sim		"Random data simulation"
#define T_mq_dbg		"MQTT debug mode"
#define T_topbeg		"vt/1.0/device/"
#define T_join			"/join"
#define T_mstate		"/status"
#define T_cfout			"/configOut"
#define T_cfin			"/configIn"
#define T_tpar			"/parameter"
#define T_tbit			"/bitmap"
#define T_tplus			"/plus"
#define T_tcert			"/cert"
#define T_cmd				"/command"
#define T_leave			"/leave"
#define T_data			"/data"
#define T_debug			"/debug"
#define T_ttime			"/time"
#define T_pub				"PUBLISH "
#define T_sub				"SUBSCRIBE "
#define T_unsub			"UNSUBSCRIBE "
#define T_m_rxpgack	"\n PINGACK: "
#define T_m_rxpub		"\n PUBLISH (topic,pid,[cmd]):"
#define T_m_rxpback	"\n PUBACK (pid,time,typ):"
#define T_m_rxsback "\n SUBACK (pid,time,typ,pay):"
#define T_m_rxusbk 	"\n UNSUBACK (pid):"
#define	T_event			"/event"
#define	T_m_ind			"MQTT_Index"
#define	T_m_spg			"MQTT_spage"
#define	T_m_sadr		"MQTT_sadr"
#define	T_m_smin		"PON minute"
#define T_md_start	"MD_start"
#define	T_m_fail		"MQTT fail!"
#define T_m_pbackr	"PUBACK (pid): "
#define T_m_write		" written"
#define T_m_activ		" active"
#define T_m_none		"None"
#define T_confws		"Connect with Firmware Server..."
#define T_cp_fw			"Copy firmware to flash..."
#define T_fw_crc		"CRC check..."
#define T_fw_prog		"\nFirmware programming..."
#define T_crc				"\nCRC: "
#define T_block 		" block:   0"
#define T_url				"URL="
#define T_simiccid	"SIM ICCID= "
#define T_gpsres		"GPS position fix restartet"
#define T_gpsact		"GPS LC76 - Typ 4"
#define T_dmobile		"datamobile"
#define T_gpsdact		"GPS off"
#define T_varsym		"Vario Symbol nos. default set"
#define T_apnset		"APN datamobile"
#define T_gsmdact		"GSM + GPS off"
#define T_smtpserv	"Email Server set"
#define T_usbdeact  "USB off"
#define T_memreset	"Memory reset"
#define T_daylight	"Daylight time change activated"

// Bluetooth RN4678
#define T_spp				"SPP"
#define T_ble				"BLE"
#define T_pin				"PIN = "

// Sprachunabhängige Stringkürzel
#define T_h			"h"
#define T_MB		" MB"
#define T_KB		" KB"
#define T_ist		" = "
#define T_dpkt	": "
#define T_ms		"ms"
#define T_LF	  "\n"
#define T_2LF	  "\n\n"
#define T_ppm		" ppm"
#define T_Volt	" V"
#define T_mVolt	"mV"
#define T_MHz		"MHz\n"
#define T_perc	" %"
#define T_inprz	" in %"
#define T_col		", "
#define T_vtf		"VTF"
#define T_rqst	"?\n"
#define T_ip		"+IP: "
#define T_dot3	"..."
#define T_cb		">"
#define T_cbbl	"> "
#define T_mend	"\n."
#define T_ddash	"--"
#define T_stars	"****"
#define T_minus	"- "
#define T_ic		", IC"
#define T_dec		"d\t"
#define	T_ic37	"IC37"
#define T_all		"ALL"
#define T_bin		".BIN"
#define	T_par		".PAR"
#define T_plus	"+++"
#define T_close "\"\r\0"

// Einsprachige englische Fehlertexte
#define E_xmcancel		"Xmodem remote cancel"
#define E_xmretry			"Xmodem retry exceed"
#define E_xmsync			"Xmodem synchronisation"
#define E_xmlong			"Xmodem file too long"
#define E_xmpacket		"Xmodem packet type"
#define E_xmtimeout		"Xmodem timeout"
#define E_iapfile			"no iap firmware file"
#define E_crc					"CRC checksum"
#define E_program			"firmware programming"
#define E_i2cbus			"I2C bus"
#define E_i2cdev			"I2C device"
#define E_i2cdpp			"I2C DPP"
#define E_flash				"flash memory"
#define E_brownout		"CPU brownout"
#define E_rtc					"RTC device"
#define E_tca6507			"TCA6507 LED spot "
#define E_bt					"Bluetooth"
#define E_usb					"USB"
#define E_hard				"Hardware"
#define E_gsm					"GSM/GPRS"
#define E_simpin			"SIM PIN"
#define E_com					"communication timeout"
#define E_w_usb				"USB media write"
#define E_pardef			"illegal parameter definition"
#define E_gps					"GPS"
#define E_gpsfix			"GPS position fix time exceeded"
#define E_mqtt				"MQTT communication"

// Rückgabe Ereignismeldungen
#define MAXMESSAGES			15		// Maximal Ereignismeldungen definiert
#define POWER_ON				1			// 1. Einschaltmeldung
#define MESSBEGINN			3			// 2. Messung begonnen
#define SOMMERZEIT			5			// 3. Umstellung Winter- auf Sommerzeit
#define WINTERZEIT			6			// 4. Umstellung Sommer- auf Winterzeit
#define REBOOT					7			// Reboot Ereignis
#define MEMFULL					11		// 5. Speicher voll, Daten werden überschrieben 	
#define BATTERYLOW			12		// 6. Batterie Unterspannung
#define WERK_INIT				30		// 7. Werkinitialisierung der Parameter
#define WATCHDOG_RESET	31		// 8. Watchdog reset
#define FIRMWARE_UPDATE 32		// 9. Firmware Update
#define DEF_PARAMETER		33		// 10. Reset auf Default Parameter 
#define PARAMETER_INIT	34		// 11. Parameter geladen
#define CERT_SAVED			35		// 12. Zertifikat geladen
#define MEMFULL95				40		// Speicher 95% voll, keine Nummer da keine Protokollmeldung
#define MQTT_CONNECTED	50		// 13. MQTT Verbindung aufgebaut


extern const ushort evno[MAXMESSAGES];		// Liste der Ereignisnummern
extern text * const evtxt[MAXMESSAGES];		// Liste der Ereignistexte

// Rückgabe Fehlercodes 
#define MAXERRORTEXT		28		// 28 Textfehlermeldungen definierbar
#define XMODEM_REMOTECANCEL 		201		// 1
#define XMODEM_RETRYEXCEED  		202		// 2
#define XMODEM_OUTOFSYNC	  		203		// 3
#define XMODEM_FILE_TOO_LONG 		204		// 4
#define XMODEM_PACKET_TYPE			205		// 5
#define XMODEM_TIMEOUT					206		// 6
#define IAP_ERROR_NO_ARM_CODE		207		// 7
#define CRC_ERROR								208		// 8
#define	COMMUNICATION_TIMEOUT	  210		// 9
#define	USB_WRITE								211		// 10
#define IAP_PROGRAMM_ERROR			220		// 11
#define BROWNOUT_ERROR					300		// 12
#define HARDWARE_ERROR					500		// 13
#define FLASH_ERROR				 			510		// 14
#define I2C_BUS_ERROR						520		// 15
#define I2C_DEVICE_ERROR 				521		// 16
#define I2C_DPP_ERROR						522		// 17	
#define TCA6507_ERROR						524		// 18
#define RTC_ERROR								530		// 19
#define BLUETOOTH_ERROR			    540		// 20
#define USB_ERROR			    			550		// 21
#define GSM_ERROR								600		// 22
#define SIM_ERROR								650		// 23
#define GPS_ERROR								700		// 24
#define GPS_TIME_ERROR					701		// 25
#define MQTT_ERROR							800		// 26
#define PARAMETER_INIT_ERROR	 1000		// 27
#define PARAMETER_DEF_ERROR	   1001		// 28
//#define FORMAT_ERROR			   1002
//#define RANGE_ERROR			   		1003
		

extern const ushort errno [MAXERRORTEXT]; 	// Liste der Fehlernummern
extern text * const errtxt [MAXERRORTEXT];	// zeigerliste Fehlertexte

// Zeichenkettenlisten (Ausgabe mit putmstr(* text)
extern text * const L_start[];	// Einschaltmeldung
extern text * const L_winit[];	// Werkinitialisierung
extern text * const L_conf[];		// Werksmenü Konfig.
extern text * const L_info[]; 	// Information
extern text * const L_dpp[];		// DPP einstellen
extern text * const L_gsm[];		// Zeitplanung GSM/Email
extern text * const L_mime[];		// Email Mime Header
extern text * const L_mbnd[];		// Email Mime boundary und Content

// Ereignistexte (Protokollmeldungen)
extern text T_poweron[];
extern text T_watchdog[];
extern text T_update[];
extern text T_sommer[];
extern text T_winter[];
extern text T_messstart[];
extern text T_memfull[];
extern text T_defpar[];
extern text T_parainit[];
extern text T_batlow[];
extern text T_mqttcon[];
extern text T_reb[];
//text T_test[]		= "Testeintrag";

#endif /*SISTXT_H_*/

