/* Header Datei f�r btio.c, Projekt viasis3003 */

#ifndef BTIO_H_
#define BTIO_H_

#include "hard.h"

#define Laird		1			// BT- Modem Laird 730-SA
#define Roving	46		// Microchip	RN4678APL 
#define IF820	82		// Ezurio Vela IF820 (EZ-Serial)
#define T_rn4678	" Micro"
#define T_laird		" Laird"
#define T_if820	" IF820"

#define BT_CYSPP  1   // CYSPP-Steuerung (IC54 Bit7): 1=normal (noetig! CYSPP ist Pflicht fuer SPP-Datenmodus), 0=Testmodus (Firmware fasst CYSPP nicht an -> BT verbindet NICHT). Test 5.37 bestaetigt: ohne CYSPP keine Verbindung.

#define IF820_BAUD         460800	// <<< Betriebsbaudrate IF820 - HIER zentral aendern (460800 bewaehrt; hoeher nur mit Oszi-Check!)
#define IF820_FACTORY_BAUD 115200	// Werks-Baud des IF820 (fix) - nur Erkennung/Erstkonfiguration
#define BT_BAUD  (fp.btmodem==IF820 ? IF820_BAUD : (fp.btmodem==Laird ? 460800 : 307200))	// Betriebsbaudrate BT-Modul (IF820 = IF820_BAUD)
#define GSM_BAUD gsmbaud		// einstellbare GSM-Baudrate (Variable gsmbaud, Default 460800)

// Laird 730-SA Texte nur f�r Bluetooth AT Kommando Kommunikation
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

// Microchip RN4678APL Texte f�r Bluetooth Kommunikation
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
#define T_qfv      "/QFV\r"       // system_query_firmware_version -> @R,...,/QFV,0000,E=..,S=..,P=..,H=..
#define T_qfvok    "/QFV"         // Antwort-Kennung (enthaelt Version E=/S=/P=/H=)
#define T_gba      "GBA\r"        // system_get_bluetooth_address -> @R,...,GBA,0000,A=<MAC>
#define T_gbaok    "GBA"          // Antwort-Kennung MAC-Adresse (A=)
#define T_cypget   ".CYSPPGP\r"   // p_cyspp_get_parameters -> @R,...,.CYSPPGP,0000,E=..,G=..,C=..,L=..,R=..,M=..
#define T_cypgetok "CYSPPGP"      // Antwort-Kennung CYSPP-Parameter
#define T_gtu      "GTU\r"        // system_get_uart_parameters
#define T_gtu_d    ",D="          // Feld direkt nach F= (zum Mitlesen von F)
#define T_stu_post ",A=00,C=00,F=01,D=08,P=00,S=01\r"  // STU-Suffix: Flow Control AN, 8N1 (Baud aus IF820_BAUD)
#define T_stuok    "STU,0000"
#define T_scfg     "/SCFG\r"      // system_store_config (RAM->Flash)
#define T_scfgok   "SCFG,0000"
#define T_sdn      "SDN$,N="    // gap_set_device_name, $ = direkt in Flash
#define T_sdnok    "SDN$,0000"
#define T_gdn      "GDN\r"     // gap_get_device_name
#define T_gbtdc    "GBTDC\r"   // bt_get_device_class (14/13) -> C=<24-Bit Class of Device>
                               // Windows leitet Geraetekategorie/Icon daraus ab. Werksdefault des IF820 ist
                               // 0x00240418 = Major Class 0x04 (Audio/Video), Minor 0x06 (Kopfhoerer),
                               // Service-Bits Audio+Rendering -> PC zeigt "Audiogeraet", legt keinen COM-Port an.
#define T_gbtp     "GBTP\r"    // bt_get_parameters (14/11) -> T=supervision,D=discoverable,C=connectable,
                               // F=flags,S=SCN,V=,N=  | ACHTUNG: das F-Feld ist laut Doku "always set to
                               // 0 in the current release" -> Sniff-Zustand ist NICHT ueber GBTP ruecklesbar.
                               // Nur informativ (D/C/S sind real). Idempotenz laeuft ueber die CoD (GBTDC).
// --- 5.54: Classic-Konfiguration setzen (Durchsatz + Windows-Kategorie) ---
#define IF820_COD  "00001F00"  // Ziel-Class-of-Device (8 Hex = 32 Bit wie GBTDC-Readback): uncategorized
                               // (Major 0x1F, nicht Audio). SPP-COM-Port bleibt, da er am SDP-Service haengt.
#define T_sbtdc    "SBTDC,C=" IF820_COD "\r"                        // bt_set_device_class (14/12)
#define T_sbtdcok  "SBTDC,0000"
#define T_sbtp_ns  "SBTP,T=7D00,D=02,C=01,F=01,S=02,V=0000,N=0000\r"// wie Werksdefault, aber F=01 = Sniff AUS
#define T_sbtpok   "SBTP,0000"
// --- 5.55: BLE-Advertising dauerhaft (Kunde kann jederzeit verbinden) ---
// Werksdefault SAP: T=03,C=07,H=0030,D=001E,L=0800,O=003C,F=00 -> 30 s High + 60 s Low = stoppt nach 90 s.
// Wir setzen L=12C0 (Low-Duty-Intervall 3 s), O=0000 (Low-Duty-Dauer UNENDLICH) und F=01
// (Auto-Advertising bei Boot UND Disconnect).
// -> erste 30 s schnelles High-Duty-Advertising, danach dauerhaft im Low-Duty-Intervall 3 s.
// BEWUSST auch fuer Batteriegeraete (kein Netz/Batterie-Guard): Anforderung ist "Kunde kann
// jederzeit verbinden". 3 s (0x12C0 = 4800 * 0,625 ms) ist der gewaehlte Strom-/Auffind-Kompromiss.
// Bereich laut Doku: 0x0020 (20 ms) .. 0x4000 (10,24 s). Sparsamer -> L vergroessern.
#define T_sap      "SAP,T=03,C=07,H=0030,D=001E,L=12C0,O=0000,F=01\r"
#define T_sapok    "SAP,0000"
#define T_gap      "GAP\r"     // gap_get_adv_parameters -> ...,D=..,L=..,O=<low duration>,F=..
#define T_gap_end  ",F="       // Parse-Anker fuer GAP: O-Feld ist bei ",F=" komplett in cbuf
// --- 5.57: App-Config aus IF820_Config_Kandidaten.md ---
// A1: Command-Echo aus. Guide empfiehlt es fuer MCU-Hosts (der LPC muss dann nur Antworten
//     parsen, nicht sein eigenes Echo). Werksdefault M=1 (an). Betrifft nur den Command-Mode.
#define T_spem     "SPEM,M=0\r" // protocol_set_echo_mode (1/3)
#define T_spemok   "SPEM,0000"
#define T_gpem     "GPEM\r"     // protocol_get_echo_mode (1/4) -> ...,M=<00/01>  (M ist letztes Feld)
// A2: CYSPP-Paketierung. Werksdefault W=5 ms ist bei 460800 zu traege ("Safe Wait" laut Guide
//     = 1 ms). M=01 (Anticipate), L=14 (20 B), E=0D bleiben Werksdefault; nur W -> 1 ms.
#define T_cypsk    ".CYSPPSK,M=01,W=01,L=14,E=0D\r"   // p_cyspp_set_packetization (10/7)
#define T_cypskok  "CYSPPSK,0000"
#define T_cypgk    ".CYSPPGK\r"                       // p_cyspp_get_packetization (10/8) -> M=,W=,L=,E=
#define T_cypgk_end ",L="      // Parse-Anker: W-Feld ist bei ",L=" komplett in cbuf
// A5: Just-Works festschreiben (schuetzt gegen versehentliches Verstellen -> kein OS-Pairing-
//     Dialog). M=00 (kein MITM/Bond, Classic+BLE), B=00, K=10, P=00, I=03 (No Input/No Output),
//     F=01 (Auto-Accept). Hinweis: der Werksdefault fuer das M-Feld ist im Guide NICHT dokumentiert;
//     falls er bereits M=00 ist, schreibt A5 gar nicht - andernfalls einmaliger Write, danach
//     liest GSBP M=00 zurueck -> kein Reflash-Loop.
#define T_ssbp     "SSBP,M=00,B=00,K=10,P=00,I=03,F=01\r"  // smp_set_security_parameters (7/11)
#define T_ssbpok   "SSBP,0000"
#define T_gsbp     "GSBP\r"    // smp_get_security_parameters (7/12) -> M=,B=,K=,P=,I=,F=
#define T_gsbp_end ",B="       // Parse-Anker: M-Feld ist bei ",B=" komplett in cbuf
#define T_rbt      "/RBT\r"    // system_reboot
#define T_dis      "/DIS\r"    // gap_disconnect - offene Verbindung sauber schliessen
#define T_disok    "/DIS,0000"

extern bool Init_BT_ch (uint baudrate);	// Konfiguriere uart1 und setze MUX Kanal auf BT modem
extern bool bt_command (text * command, text * answer, int pause); // Bluetooth Kommando senden, Antwort empfangen
extern void init_bluetooth (void);			// Bluetooth Modem initialisieren
extern bool test_BT	(void);							// Pr�fe ob BT Modul antwortet
extern void send_bt_info (void);				// Bluetooth Informationen ausgeben
extern void set_bt_name (void);					// Setze Bluetooth device (friendly) name
extern void set_bt_pin (void);					// Setze Pinnummer
extern int  bt_get_flowcontrol (void);    // Flow Control abfragen: -1=unbek.,0=aus,1=an
extern bool bt_set_flowcontrol (void);    // RTS/CTS modulspezifisch aktivieren (+persist.)
extern bool bt_ensure_flowcontrol (void); // generisch: pruefen, setzen, verifizieren
extern int  bt_get_cod (void);            // CoD abfragen: -1=unbek., 0=abweichend, 1=== Zielwert IF820_COD
extern bool bt_set_btcfg (void);          // IF820: Sniff AUS + CoD seriell -> RAM + /SCFG
extern bool bt_ensure_btcfg (void);       // generisch: pruefen, bei Bedarf setzen, verifizieren
extern int  bt_get_adv (void);            // Adv-Dauer abfragen: -1=unbek., 0=begrenzt, 1=unendlich (O=0000)
extern bool bt_set_adv (void);            // IF820: dauerhaftes Advertising -> RAM + /SCFG
extern bool bt_ensure_adv (void);         // generisch: pruefen, bei Bedarf setzen, verifizieren
extern int  bt_get_echo (void);           // Echo abfragen: -1=unbek., 0=an, 1=aus (M=0)
extern bool bt_set_echo (void);           // A1: Command-Echo aus -> RAM + /SCFG
extern int  bt_get_pkt (void);            // Paketierung abfragen: -1=unbek., 0=anders, 1=W=01
extern bool bt_set_pkt (void);            // A2: CYSPP-Wait auf 1 ms -> RAM + /SCFG
extern int  bt_get_sec (void);            // Security abfragen: -1=unbek., 0=anders, 1=Just-Works (M=00)
extern bool bt_set_sec (void);            // A5: Just-Works festschreiben -> RAM + /SCFG
extern int  bt_get_name (void);           // 1=Name==VIASIS_<serno>, 0=anders, -1=unbek.
extern bool bt_set_name (void);           // Name=VIASIS_<serno> schreiben (SDN$, Flash)
extern bool bt_ensure_name (void);        // generisch: pruefen, bei Bedarf schreiben
extern void bt_show_name (void);          // aktuellen BT-Namen vom Modul lesen + anzeigen
extern volatile uchar bt_cyspp_out;       // 1 = Host treibt CYSPP (IC54 Bit7) als Ausgang HIGH -> Modul kann KEINEN Datenkanal oeffnen
extern void bt_cmdmode (void);            // CYSPP HIGH: SPP trennen + Command-Mode erzwingen
extern void bt_release (void);            // CYSPP loslassen: Status lesbar, bereit fuer Verbindung
extern void bt_disconnect (void);         // IF820: offene Verbindung sauber trennen (Command-Mode + /DIS)
extern void bt_cyspp_monitor (void);      // Live-Anzeige CYSPP/BT_LINK (IC54 Bit7) - Testmenue
extern void bt_show_counters (void);      // Durchsatz-Diagnose: CTS-Bremse/osDelay-Verluste anzeigen
extern void bt_reset_counters (void);     // Diagnosezaehler nullen (bei neuer BT-Verbindung)

#endif // BTIO_H_

