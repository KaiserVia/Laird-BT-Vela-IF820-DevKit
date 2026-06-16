/* Header Datei f�r gsmio.c, Projekt vario */

#ifndef GSMIO_H_
#define GSMIO_H_

#include "hard.h"

#define Def_smtpport	25
#define Def_mqttport	8883				// verschl�sselt / 1883 - unverschl�sselt
#define UC15					15
#define EG91					91


// M10 GSM/GPRS/SMTP/SMS Textkonstanten
#define T_at				"AT"						// GSM und Konfiguration ab hier
#define T_def				"&F\n"
#define T_ati				"ATI"
//#define T_m10				"M10"
#define T_uc15			"UC15"
#define T_eg91			"EG91"
#define T_umts			"UMTS"
#define T_lte				"LTE"
#define T_cfg				"ATQ0;E0;S0=1;+IFC=2,2"				// UC15 und EG91
#define T_cfuc15		"AT+QURCCFG=\"URCPORT\",\"UART1\";+QCFG=\"NWSCANMODE\",0,1"		// UC15	spezifisch oder nicht AT&W speicherbar ;+QINDCFG=\"all\",0,1
#define T_cmee			"AT+CMEE=2"	//;S0=1;+QSCLK=1"							// UC15 Lange Errorcodes, Achtung: ATS0 muss wiederholt werden! und Sleep zulassen 	
#define T_cfsto			"AT+IPR=460800;&W"										// UC15 und EG91
#define T_gsmoff		"AT+QPOWD"														// UC15 normal power off
#define T_nsel			"AT+QCFG=\"NWSCANMODE\","							// Netzwerkauswahl GSM oder UMTS
#define T_RDY				"RDY\r"
#define T_pbdone		"PB DONE\r"
#define T_simpin		"SIM PIN"
#define T_cfun			"+CFUN: 1\r"
#define T_atcfun		"AT+CFUN?"
#define T_cpin			"+CPIN"
//#define T_ring		"RING"
#define T_conn			"00\r"									// Connect 9600, 19200 usw.
#define T_noca			"CARRIER\r"
#define T_csq				"AT+CSQ"
#define T_cgatt			"+CGATT=" 					// GPRS	ab hier
#define T_cgdcont		"+CGDCONT=1,\"IP\","
#define T_creg			"+CREG"
#define T_cgreg			"+CGREG"
#define T_cereg			"+CEREG"
#define T_creguc15	"AT+CREG?;+CGREG?"
#define T_cregeg91	"AT+CREG?;+CGREG?;+CEREG?"
#define T_qicsgp		"+QICSGP=1,"
#define T_tcpip			"AT+QIREGAPP"
#define T_act				"AT+QIACT"
#define T_act15			"AT+QIACT=1"
#define T_qilocip		"AT+QILOCIP"
#define T_qideact		"AT+QIDEACT"
#define T_qimode		"AT+QIMODE=1"
#define T_qidnsip		"AT+QIDNSIP="
#define T_dns				"AT+QIDNSGIP="
#define T_qiopen		"AT+QIOPEN=1,1,"
#define T_tcp				"\"TCP\","
#define T_qiclose		"AT+QICLOSE"
#define T_connect		"CONNECT\r\n"
#define T_Ehlo			"EHLO "						// SMTP ab hier
#define T_Helo			"HELO "
#define T_login			"AUTH LOGIN"
#define T_mailf			"MAIL FROM: <"
#define T_mquit			"QUIT"
#define T_mrcp			"RCPT TO: <"
#define T_mdata			"DATA"
#define T_hfrom			"From: "
#define T_hto				"To: "
#define T_hcc				"Cc:"
#define T_hsubj			"Subject: "	
#define T_csca			"AT+CSCA"					// SMS ab hier	
#define T_cmgf			"AT+CMGF=1"
#define T_csmp			"AT+CSMP=17,167,0,241"
#define T_cnmi			"AT+CNMI=2,1,0,0,0"
#define T_cmgs			"AT+CMGS="
#define T_ready			"READY"
#define T_wpass			"incorrect"
#define T_mid       "Message-ID: <"
#define T_edate			"Date: "

// MQTT - Kommunikation, TLS, File upload
#define T_cfgalive	"AT+QICFG=\"tcp/keepalive\",1,3,60,3"
#define T_sslcfg0		"AT+QSSLCFG=\"SSLVERSION\",1,3;+QSSLCFG=\"CIPHERSUITE\",1,0XFFFF;+QSSLCFG=\"SECLEVEL\",1,0"
#define T_sslcfg1		"AT+QSSLCFG=\"SSLVERSION\",1,3;+QSSLCFG=\"CIPHERSUITE\",1,0XFFFF;+QSSLCFG=\"SECLEVEL\",1,2;+QSSLCFG=\"cacert\",1,\"UFS:CA.CRT\""
#define T_sslcfg2		"AT+QSSLCFG=\"clientcert\",1,\"UFS:"
#define T_sslcfg3		".CRT\";+QSSLCFG=\"clientkey\",1,\"UFS:"
#define T_sslcfg4		".KEY\""
#define T_sslopen		"AT+QSSLOPEN=1,1,1,"
#define T_sslclose	"AT+QSSLCLOSE"
#define T_fopen1		"AT+QFOPEN="
#define T_fopen2		",1"
#define T_fresp			"+QFOPEN: "
#define T_fwrite		"AT+QFWRITE="
#define T_fclose		"AT+QFCLOSE="
#define T_csucc			"File succesfully loaded"
#define T_ctsw			"AT+QISWTMD=1,2"
#define T_fmove			"AT+QFMOV="
#define T_fmove2		",0,1"

#define T_htctxid		"AT+QHTTPCFG=\"contextid\","
#define T_htrsphd		";+QHTTPCFG=\"responseheader\","
#define T_htsslctx	";+QHTTPCFG=\"sslctxid\",1"
#define T_htrqhd		";+QHTTPCFG=\"requestheader\"," 
//#define T_htcfg1		"AT+QHTTPCFG=\"contextid\",1;+QHTTPCFG=\"responseheader\",0" //;+QHTTPCFG=\"sslctxid\",1"
#define T_htcfg2		"AT+QHTTPURL="
#define T_htcfg3		",80" 
#define T_htget			"AT+QHTTPGET="
//#define T_htget2		"GET /fw/vario202_d-f-i.bin HTTP/1.0"		// "GET /download/vario202_d-f-i.bin HTTP/1.0"
//#define T_htget3		"Host: vt.gonicus.de" 
#define T_htread		"AT+QHTTPREAD="
#define T_htrdfile	"AT+QHTTPREADFILE="
#define T_fwfile		"firmware.bin"
#define T_qflst			"AT+QFLST="
#define T_qseek			"AT+QFSEEK="
#define T_fread			"AT+QFREAD="
#define T_qccid			"AT+QCCID"


// Routinen extern bereit stellen
extern void Init_GSM_ch (uint baud);		// Uart1 initialisieren und GSM Kanal an MUX einstellen
extern void Init_GPS_ch (uint baud);		// Uart1 initialisieren und GPS Kanal an MUX einstellen
extern int gsm_power (uchar mode);			// GSM Modem ein-/ausschalten
extern void GSM_Registration (void const *argument);		// Modem Einschaltung und Registrierung im Funknetz
extern int modem_start (void);					// Einschaltsequenz Funkmodem
extern void init_gsm (void);						// Initialisierung GSM Modem
extern int test_gsm (uchar poweron);		// Pr�fe ob GSM/GPRS Modem antwortet
extern void gsm_com (uchar modemein);		// Direkte Kommunikation mit GSM Modem
extern void atcpin (ushort set, uchar manual);					// Setze Pinnummer oder pr�fe Status
extern int test_registration (void);										// Pr�fe ob Modem eingebucht ist
extern int set_gprs_config (uchar ausgabe);							// GPRS Konfiguration setzen und testen
extern void send_local_ip (void);					// Lokale IP Adresse ausgeben
extern uchar sendmail (uchar mode);				// Email versenden und konfigurieren
extern void send_messdaten (void);				// Messdaten als Email Anhang ausgeben
extern int sendsms (uchar ausgabe);				// SMS Service konfigurieren und SMS versenden
extern void get_sms_config (void);				// SMS Konfiguration einlesen
extern void get_email_config (void);			// Email Konfiguration einlesen
extern void get_apn_config (void);				// GPRS Konfiguration einlesen
extern void get_server_config (void);			// Einlesen der SMTP-Server Konfiguration
extern int wait_ok_time (uint timeout); 	// Warte timeout lang auf OK Antwort
extern void change_radionet (uchar net);	// Funknetzeinstellung GSM / UMTS �ndern
extern void set_server_default(uchar servertyp);				// Default Parameter f�r Server setzen
extern int cert_to_uc15 (uchar off);										// MQTT Zertikats-File upload ins UC15 Funkmodem
extern int file_to_uc15(uint filesize, char *Pfname);	// File upload UC15 Funkmodem
extern int activate_cert (void);												// Aktiviert im Funkmodem vorhandene neue TLS Zertifikate
//extern void putcbuf_line (uchar offset);	// Puffer terminieren und eine Zeile an Schnittstelle senden
extern int Download_firmware (const char *url, uchar mqtt_connect);	// Verbinde mit HTTP Server und lade Firmware herunter
extern int firmware_to_flash (int filecrc);	// Transfer Firmware vom Funkmodem in den Flash
extern void send3plus	(void);							// Umschaltung Modem vom data in den command mode
#endif // GSMIO_H_
