/* Header Datei für btio.c, Projekt viasis3003 */

#ifndef BTIO_H_
#define BTIO_H_

#include "hard.h"

#define Laird		1			// BT- Modem Laird 730-SA
#define Roving	46		// Microchip	RN4678APL 
#define IF820	82		// Ezurio Vela IF820 (EZ-Serial)
#define T_rn4678	" Micro"
#define T_laird		" Laird"
#define T_if820	" IF820"

#define BT_BAUD  (fp.btmodem==IF820 ? 115200 : (fp.btmodem==Laird ? 460800 : 307200))	// Betriebsbaudrate BT-Modul (IF820 Testphase 115200)
#define GSM_BAUD 115200		// TESTPHASE IF820: war 460800 (UC15/EG91) -- spaeter zurueck

// Laird 730-SA Texte nur für Bluetooth AT Kommando Kommunikation
#define T_AT		"AT\r"
#define T_ATE0	"ATE0\r"
#define T_ATI0	"ATI0\r"
#define T_ATI3	"ATI3\r"
#define T_ATI4	"ATI4\r"
#define T_ATW		"AT&W\r"
#define T_ATZ		"ATZ\r"
#define T_BTDM	"AT+BTD*\r"
#define T_BTK		"AT+BTK=\""
#define T_BTN		"AT+BTN=\""
#define T_BTNG	"AT+BTN?\r"
#define T_BTT		"AT+BTT?\r" 
#define T_BTF		"AT+BTF=\""
#define T_F1		"AT&F1\r"
#define T_S502	"ATS502=1\r"
#define T_S504	"ATS504=1\r"
#define T_S507	"ATS507=2\r"
#define T_S508	"ATS508=1000\r"
#define T_S512	"ATS512=4\r"
#define T_S514	"ATS514=60\r"	
#define T_S521	"ATS521=460800\r"
#define T_S538	"ATS538=1\r"
#define T_S592	"ATS592=1\r"
#define T_OK		"OK\r\n"			
#define T_con		"CONNECT"

// Microchip RN4678APL Texte für Bluetooth Kommunikation
#define T_$			"$$$\r"
#define T_cmdp	"CMD>"
#define T_AOK		"AOK"
#define T_sf1		"SF,1\r"
#define T_sr1		"R,1\r"
#define T_sno		"SN,"
#define T_sdm		"SDM,"
#define T_auth1	"SA,1\r"
#define T_auth2	"SA,2\r"
#define T_auth4	"SA,4\r"
#define T_sp		"SP,"
#define T_class	"SG,2\r"
#define T_mble	"SG,1\r"
#define T_dual	"SG,0\r"
#define T_su		"SU,10\r"
#define T_so		"SO,,\r"
#define T_boot	"%REBOOT"
#define T_CR		"\r"
#define T_gk		"GK\r"
#define T_gkres "0,0,0"
#define T_min		"---\r"
#define T_end		"END"
#define T_V			"V\r"
#define T_gds		"GB\r"
#define T_gn		"GN\r"
#define T_ssp		"SS,SPP\r"
#define T_port	"SS,SerialPort\r"
#define T_powr	"SY,4\r"
#define E_errser "Viasis serial number not set"

// Ezurio Vela IF820 (EZ-Serial) Texte
#define T_ping		"/PING\r"		// system_ping Kommando
#define T_pingok	"/PING,0000"	// Erfolgsantwort @R,...,/PING,0000

extern bool Init_BT_ch (uint baudrate);	// Konfiguriere uart1 und setze MUX Kanal auf BT modem
extern bool bt_command (text * command, text * answer, int pause); // Bluetooth Kommando senden, Antwort empfangen
extern void init_bluetooth (void);			// Bluetooth Modem initialisieren
extern bool test_BT	(void);							// Prüfe ob BT Modul antwortet
extern void send_bt_info (void);				// Bluetooth Informationen ausgeben
extern void set_bt_name (void);					// Setze Bluetooth device (friendly) name
extern void set_bt_pin (void);					// Setze Pinnummer

#endif // BTIO_H_

