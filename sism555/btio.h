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

#define IF820_BAUD         460800	// <<< Betriebsbaudrate IF820 - HIER zentral aendern (460800 bewaehrt; hoeher nur mit Oszi-Check!)
#define IF820_FACTORY_BAUD 115200	// Werks-Baud des IF820 (fix) - nur Erkennung/Erstkonfiguration
#define BT_BAUD  (fp.btmodem==IF820 ? IF820_BAUD : (fp.btmodem==Laird ? 460800 : 307200))	// Betriebsbaudrate BT-Modul (IF820 = IF820_BAUD)
#define GSM_BAUD gsmbaud		// einstellbare GSM-Baudrate (Variable gsmbaud, Default 460800)

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
#define T_gtu      "GTU\r"        // system_get_uart_parameters
#define T_gtu_d    ",D="          // Feld direkt nach F= (zum Mitlesen von F)
#define T_stu_post ",A=00,C=00,F=01,D=08,P=00,S=01\r"  // STU-Suffix: Flow Control AN, 8N1 (Baud aus IF820_BAUD)
#define T_stuok    "STU,0000"
#define T_scfg     "/SCFG\r"      // system_store_config (RAM->Flash)
#define T_scfgok   "SCFG,0000"
#define T_sdn      "SDN$,N="    // gap_set_device_name, $ = direkt in Flash
#define T_sdnok    "SDN$,0000"
#define T_gdn      "GDN\r"     // gap_get_device_name
#define T_rbt      "/RBT\r"    // system_reboot
#define T_dis      "/DIS\r"    // gap_disconnect - offene Verbindung sauber schliessen
#define T_disok    "/DIS,0000"

extern bool Init_BT_ch (uint baudrate);	// Konfiguriere uart1 und setze MUX Kanal auf BT modem
extern bool bt_command (text * command, text * answer, int pause); // Bluetooth Kommando senden, Antwort empfangen
extern void init_bluetooth (void);			// Bluetooth Modem initialisieren
extern bool test_BT	(void);							// Prüfe ob BT Modul antwortet
extern void send_bt_info (void);				// Bluetooth Informationen ausgeben
extern void set_bt_name (void);					// Setze Bluetooth device (friendly) name
extern void set_bt_pin (void);					// Setze Pinnummer
extern int  bt_get_flowcontrol (void);    // Flow Control abfragen: -1=unbek.,0=aus,1=an
extern bool bt_set_flowcontrol (void);    // RTS/CTS modulspezifisch aktivieren (+persist.)
extern bool bt_ensure_flowcontrol (void); // generisch: pruefen, setzen, verifizieren
extern int  bt_get_name (void);           // 1=Name==VIASIS_<serno>, 0=anders, -1=unbek.
extern bool bt_set_name (void);           // Name=VIASIS_<serno> schreiben (SDN$, Flash)
extern bool bt_ensure_name (void);        // generisch: pruefen, bei Bedarf schreiben
extern void bt_show_name (void);          // aktuellen BT-Namen vom Modul lesen + anzeigen
extern void bt_cmdmode (void);            // CYSPP HIGH: SPP trennen + Command-Mode erzwingen
extern void bt_release (void);            // CYSPP loslassen: Status lesbar, bereit fuer Verbindung

#endif // BTIO_H_

