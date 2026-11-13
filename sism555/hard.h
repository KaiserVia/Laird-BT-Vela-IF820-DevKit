/* Header Datei f�r hard.c, PROJECT: sis3000M */

#ifndef HARD_H_
#define HARD_H_ 

#define NOCRPCHECK 0							// Erstellt Firmware die keine CRP Check beim Firmware Upload ausf�hrt
#define DEBUGGING 0								// 1 C(ode)R(ead)P(rotection) None wird f�r Debugger gesetzt
#define SIMULATION 0							// Schaltet Hardware retry Zyklen f�r Dscope Simulator aus
#define NOSLEEP		0								// Schaltet Deep sleep f�r Debugger aus

#include "alilpc.h"
#include "cmsis_os.h"

// Aliase	Programmsteuerung Threads
#define GSM_REGISTRATION	(1<<0)	// Thread f�r GSM Registrierung aktiv
#define USB_HOST_COM			(1<<1)	// Thread USB Host Service
#define MQTT_CONNECTION		(1<<2)	// Thread MQTT Server Connection

// Aliase Kommunikationsstatus Datenschnittstellen Flags (comstate und connect)
#define	UART0				(1<<0)			// Flag erste serielle Schnittstelle RS232 Ausgabe
#define	UART1				(1<<1)			// Flag zweite serielle Schnittstelle BT, GSM Ausgabe
#define UART2				(1<<2)			// Flag dritte serielle Schnittstelle GPS Ausgabe
#define	GPS_LINK		(1<<2)			// 1 wenn GPS Schnittstelle installiert
#define	USB_LINK		(1<<3)			// 1 wenn USB (Host) Schnittstelle aktiviert
#define RS232_LINK	(1<<4)			// 1 wenn RS232 Terminal angeschlossen (MAX3226 Invalid\ = 1)
#define	MQTT_LINK		(1<<5)			// 1 wenn MQTT Verbindung aktiv
#define	GSM_LINK		(1<<6)			// 1 wenn GSM Verbindung aktiv oder n.v
#define BT_LINK			(1<<7)			// 1 wenn BT Verbindung aktiv oder n.v.

// Aliase auszuf�hrende Aufgaben (tasks)
#define	MESSTASK		(1<<0)			// Messaufgabe
#define GSMTASK			(1<<1)			// GSM Modem Aufgabe
#define GPSTASK			(1<<2)			// GPS Aufgabe
#define BOOTTASK		(1<<3)			// Reboot CPU

// GPIO Port 0 Pin Definitionen
#define CSBF				(1<<0)		// CS-B-FUNK f�r MUX Kanalauswahl IC55 und IC56
#define CSAF				(1<<1)		// CS-A-FUNK f�r MUX Kanalauswahl IC55 und IC56
#define CSFLASH2		(1<<6)		// Chip select Flash 2
#define SCLK				(1<<7)		// Serial Clock LED Treiber
#define SIN					(1<<8)		// Serial data in LED Treiber
#define SOUT				(1<<9)		// Serial data out LED Treiber
#define TXD_2				(1<<10)		// Sendedaten UART2
#define EX12_1			(1<<15)		// Steuerausgang f�r externe 12V - 12V-EXT-1
#define EX12_2			(1<<16)		// Steuerausgang f�r externe 12V - 12V-EXT-2
// #define	TERM_ON			(1<<17)		// MAX3225 Force On, entf�llt ForceOn gebr�ckt Invalid
#define GSM_DTR			(1<<18)		// DTR GSM Modul
//#define EMGOFF			(1<<20)		// M10 emergency off / UC 15 NReset 
#define CONN_PIN		(1<<19)		// P0.19 IF820 CONNECTION (CONN_IND/P27), low aktiv, reine Modul-Statusausgabe - NUR LESEN, nie treiben
#define PWR_GPS			(1<<21)		// GPS-Modul Spannungsversorgung
#define	DPLUS				(1<<29)		// USB D-Plus

// GPIO Port 1 Pin Definitionen
#define BT_ON				(1<<16)		// Power Enable Bluetooth Module
#define LED1				(1<<18)		// Status LED, Kathode - low aktiv	
#define USB_PPWR		(1<<19)		// Power Enable USB Host
#define EN_ANA			(1<<20)		// Power Enable Analog - Transceiver, Verst�rker
#define	MUX_B				(1<<23)		// P1.23 MUX B Select IC36, IC39 Kanal
#define	MUX_A				(1<<24)		// P1.24 MUX A Select IC36, IC39 Kanal
#define USB_RPWR		(1<<26)		// P1.26 Power Enable 5V USB Regulator
#define PWRKEY			(1<<28)		// P1.28 Powerkey Quectel M10
#define GSMPWR			(1<<29)		// P1.29 M10 Versorgung 
#define VBUS				(1<<30)		// USB Host Versorgung
#define	DIR					(1u<<31)	// Bewegungsrichtung 
// 5.L1: Dieselben Pins haben auf dem Mainboard mit Via-2-Radarempfaenger (Joachim 5.56-5.60) eine andere Bedeutung.
// Welche Belegung gilt, bestimmt fp.radar (Selbsttest radar_detect() in i2cm.c). EN_ANA und USB_RPWR werden nur beim
// bisherigen Transceiver als Ausgang geschaltet, DIS_MESS und DIS_IF nur bei Via-2 (Init_Radar_Ports() in hard.c).
#define DIS_BT			(1<<16)		// Via-2-Board: PWR-EN\-BT, HIGH = BT-Modul aus (nicht getrieben, Pull-down = ein)
#define DA_SS				(1<<20)		// Via-2-Board: Enable D/A-Wandler (statt EN_ANA)
#define DIS_MESS		(1<<21)		// Via-2-Board: HIGH = Ausgabe Fhf/8388608 aus
#define FMin				(1<<26)		// Via-2-Board: Messeingang HF-Frequenz/8388608, CAP0.0 (statt USB_RPWR)

#define MUX_B_b			23				// Bitposition MUX B Select IC36, IC39

// GPIO Port 2 Pin Definitionen
#define CTS					(1<<2)		// Modems CTS Signal
#define G_SHIFT			(1<<3)		// Gate 74HC595 Schieberegister
#define V_TRB				(1<<4)		// NC auf aktuellem viasis Frontend
#define PWM_TRB			(1<<5)		// PWM Signal Anzeigetreiber
#define	LE_TRB			(1<<6)		// Latch enable LED Treiber	
#define DIS_TRB			(1<<8)		// Disable 3V3 Treiberversorgung
#define USBCON			(1<<9)		// USB connect
#define DETEKT			(1<<12)		// Detektionssignal


// GPIO Port 3 Pin Definitionen
#define PWMEX				(1<<26)		// Externe PWM, meist mit PWM3 gesteuert				

// GPIO Port 4 Pin Definitionen
#define CSFLASH1		(1<<28)		// Chip select Flash 1	
#define DIS_IF			(1<<29)		// Via-2-Board: LOW = Transceiver-Verstaerker ein (EN\-IF)

// GPIO Pinsel 0 Definitionen
#define SCK					(2<<14)		// SSP1 - SCK
#define MISO				(2<<16)		// SSP1 - MISO
#define MOSI				(2<<18)		// SSP1 - MOSI
#define TXD0				(1<<4)		// TXD0
#define RXD0				(1<<6)		// RXD0
#define CAP20				(3<<8)		// Capture 0 Timer 2
#define CAP21				(3<<10)		// Capture 1 Timer 2
#define TXD2				(1<<20)		// TXD2
#define RXD2				(1<<22)		// RXD2

// GPIO Pinsel 1 Definitionen
#define CAP30				(3<<14)		// Capture 0 Timer 3
#define CAP31				(3<<16)		// Capture 1 Timer 3
#define	AD02				(1<<18)		// AD Wandler Eingang Y IC36
#define AD03				(1<<20)		// AD Wandler Eingang Y IC39 �ber Impedanzwandler IC62 OP224

// GPIO Pinsel 3 Definitionen
#define	USB_UP_LED	(1<<4)		// USB connect LED Anzeige
#define USB_VPWR		(1<<7)	// USB Port Power VBUS

// GPIO Pinsel 4 Definitionen
#define	TXD1				(1<<1)		// Transmit UART1
#define RXD1				(1<<3)		// Receive UART1
#define CTS1				(1<<5)		// CTS UART1
#define	PWM5				(1<<8)		// Anzeige PWM LED Versorung MBI6037 OE 
#define	PWM6				(1<<10)		// Anzeige PWM LED Treiber MBI5039 OE/
#define RTS1				(1<<15)		// RTS UART1
#define EINT0				(1<<20)		// Interrupt Power Down
#define EINT1				(1<<22)		// Interrupt Expander IC54
#define EINT2				(1<<24)		// Interrupt Detektionsfehler

// GPIO Pinsel 7 Definitionen
#define	PWM3				(3<<20)		// Externer PWM Ausgang

// GPIO Pinmode Bitflags		
#define	PULLUP			0
#define	REPEATER		1
#define NOPULL			2
#define	PULLDOWN		3

// Diverse Bitflags Hardwareansteuerung
#define AUS3V3		(1<<0)					// 3V3 auf Treiber abschalten
#define	AUSVCL		(1<<1)					// Bit Enable Clusterspannungsregler Anzeigetreiber
#define	COL1			(1<<2)					// Versorgung Grundfarbe LED Ziffern (gelb)
#define COL2			(1<<3)					// Versorgung Warnfarbe LED Ziffern (rot)
#define	COL3			(1<<4)					// Versorgung Farbe Symbolziffern (wei�)
#define COL4			(1<<5)					// Symbolfarbe 1 (rot)
#define COL5			(1<<6)					// Symbolfarbe 2 (gr�n)
#define DISCHRG		(1<<7)					// Discharge Clusterspannung


// Allgemeine Definitionen
#define		CCLK					24000000	// CPU Clock rate
#define		RCCLK					4000000		// interner RC Oszillator 
#define		TDIV					24				// Timer Vorteiler
#define		MAXFARBEN			3					// Maximale Anzahl Anzeigefarben 
#define		MAXTAST				4					// Maximale Anzahl verwendeter Anzeigetastverh�ltnisse
#define		MAXPPM				50				// Maximal zul�ssige Abweichung RTC/CPU Oszillator in ppm	
#define 	Min_TXF    		24000    	// kleinste zul�ssige Sendefrequenz in MHz
#define 	Max_TXF     	24250   	// gr��te zul�ssige Sendefrequenz in MHz
#define 	Min_TXF_VIA2	24150    	// 5.L1: kleinste zulaessige Sendefrequenz in MHz mit Via-2
#define 	Max_TXF_VIA2	24200   	// 5.L1: groesste zulaessige Sendefrequenz in MHz mit Via-2
#define		SPGMUL12			75				// Batteriespannungsumrechnung Multiplikator
#define		SPGDIV12			10				// Batteriespannungsumrechnung Teiler
#define 	SPGMUL5				6600			// Multiplikator Spannungsumrechnung in mV bei 1:2 Teiler
#define		SPGDIV5				4095			// Teiler Spannungsumrechnung in mV bei 1:2 Teiler

#define		RES_PWM				200				// PWM Aufl�sung 1/200 = 0,5%
#define		F_PWM					120				// PWM Frequenz 120 Hz

// Definitionen Speicherorte Flash (variabel je nach Speichergr��e)
#define		BLOCKSIZE			1024										// 1k Block 1024 Byte
#define		PAGE					256											// 1 Seite 256 Byte		
#define		FLASHKGV			1056										// zur Berechnung Flash Seitenanzahl
#define		CBUFSIZE			(3*BLOCKSIZE+PAGE)			// Gr��e globaler Zeichenpuffer
#define		MQBFSIZE			(3*BLOCKSIZE+PAGE)			// Gr��e MQTT Empfangspuffer
#define		FBUFSIZE			16	
#define 	PARAMBLOCKS		2												// Anzahl 1k Parameterblocks
#define		FREEBLOCKS		5												// Anzahl frei zu haltende 1k Seiten z. B. f�r Parameter Upload
#define		PROTBLOCKS		2												// Anzahl 1k Protokollbl�cke
#define 	PARAMETERPAGE	(maxpage+1-(PARAMBLOCKS*pagepk))		// Parameterstartseite													
#define		FREEPAGE			(PARAMETERPAGE-(FREEBLOCKS*pagepk))	// 5k zwischen Protokoll und Parametern frei
#define		PROTOCOLENDPG	(FREEPAGE-1)												// Protokollendseite 
#define 	PROTOCOLPAGE	(FREEPAGE-PROTBLOCKS*pagepk)				// Protokollstartseite
#define 	MEASUREPAGE		0												// Messdaten beginnen bei erster Seite 0
#define		PARA_LEN			PARAMBLOCKS*BLOCKSIZE		// L�nge Parameterblock	2k
#define		PROT_LEN			PROTBLOCKS*BLOCKSIZE		// L�nge Protokoll in Datenausgabe
#define		VTF_LEN				BLOCKSIZE								// L�nge VTF Header	Datenausgabe
#define		PSATZLEN			8												// Protokollsatzl�nge 8 Byte
#define		VSATZLEN			10											// Messdatensatzl�nge 10 Byte
#define		SERNO_LEN			8												// L�nge Ger�teseriennummer
#define 	MD_PER_PG			(((BLOCKSIZE/pagepk)/VSATZLEN)+1)
#define 	FIRMWARE_MAX_FILESIZE   (256*1024)		// LPC1766 max. 256 k einlesen
#define 	MQTT_PKG			5												// MQTT Versand Paketgr��e

// Allgemeine Definitionen Flashparameter
#define		FLASHKENNUNG	0xA5					// Kennung f�r Werkinititialisierung im Flash
#define		PARAKENNUNG		221						// Kennung f�r Parameterblock (nie �ndern!)
#define		NOSWGR				3							// Anzahl definierbarer Schaltgruppen
#define		NOSYGR				3							// Anzahl definierbarer Symbolgruppen
#define		NOSW					12						// Anzahl definierbarer Schalter
#define		NOSYM					8							// Anzahl Anzeigesymbole 30, 50, Smileys
#define		SWGLEN				48						// Maximale	L�nge Schaltergruppenbezeichner
#define		SWLEN					40						// Maximale L�nge Schalterbezeichner
#define		SYGLEN				40						// Maximale	L�nge Led Symbolgruppenbezeichner
#define		SYLEN		    	12						// Maximale L�nge Bezeichner Led Anzeigesymbol
#define		MAX_PARSET		5							// Maximal 5 Parameters�tze einstellbar

// Konstanten Programmsteuerung/Daten�bertragung
#define		LONG_WD_32						32000	// Langes Watchdog Timeout Interval 32s
#define   SHORT_WD_3						3000	// Kurzes Watchdog Timeout Interval 3s
#define		TERMINAL_CHAR_TIMEOUT	30000	// Max. Wartezeit Terminaleingabe
#define 	XMODEM_BLOCK_TIMEOUT	3000	// Max. Wartezeit auf Block in ms
#define 	XMODEM_CHAR_TIMEOUT		500		// Max. Wartezeit auf Zeichen in ms
#define 	BT_CHAR_TIMEOUT				300		// Max. Wartezeit auf Zeichen in ms

// Bitflags zu fp.trxcon - Transceiver und Verst�rkersteuerung
#define		GAIN_INH	(1<<0)				// 1 wenn Verst�rkungs-MUX disable
#define		GAIN_A		(1<<1)				// Verst�rkungs-MUX Select A
#define		GAIN_B		(1<<2)				// Verst�rkungs-MUX Select B
#define		BW_INH		(1<<3)				// 1 wenn Bandbreite-MUX disable
#define		BW_A			(1<<4)				// Bandbreite-MUX Select A
#define		BW_B			(1<<5)				// Bandbreite-MUX Select B
#define		AMP_EN		(1<<6)				// 
#define		AMP_BW		(1<<7)				// 
//#define		EN_5V_3V3	(1<<8)				// 1 wenn Schaltregler Analog 5v/3V3 ein
#define		HF_EN			(1<<10)				// 1 wenn Transceiver ein
#define		ANT_EN1		(1<<12)				// Transceiver 1 enable
#define		ANT_EN2		(1<<13)				// Transceiver 2 enable
#define		HF_DIS		(1<<12)				// Via-2: 1 = Transceiver aus (EN\-MD-RF, IC37 P1.4, gleiches Bit wie ANT_EN1)

// 5.L1: Radar-Empfaenger in fp.radar, per Selbsttest beim Hochfahren bestimmt (radar_detect() in i2cm.c)
#define		RADAR_UNBEKANNT	0			// noch nicht bestimmt
#define		RADAR_TRX			1				// bisheriger Transceiver (Analogversorgung EN_ANA, IC37 HF_EN/ANT_EN1/ANT_EN2)
#define		RADAR_VIA2		2				// Via-2-Empfaenger (IC37 HF_DIS, Digitalpoti IC32 auf 0x5C, HF-Frequenzmessung)
#define		RADAR_MASK		0x03		// 5.s2: fp.radar ist das Hardware-Byte, Bit 1:0 Radar-Empfaenger, Bit 3:2 Mainboard
#define		RADAR_TYP			(fp.radar&RADAR_MASK)
#define		IS_VIA2				(RADAR_TYP==RADAR_VIA2)

// 5.s2: Mainboard-Version in fp.radar Bit 3:2, per Selbsttest beim Hochfahren bestimmt (board_detect() in lightsens.c).
// Merkmal ist der Lichtsensor: V1 hat die Fotodiode SFH213 und immer den bisherigen Transceiver, V2 den VEML7700 (IC1,
// I2C 0x10) und den bisherigen Transceiver oder Via-2. Gespeichert wie die anderen Baugruppen: beim Hochfahren nur
// kurz pruefen, bei "unbekannt" oder Abweichung voller Test. Parameterlayout unveraendert, alte Parameter haben hier 0.
#define		BOARD_MASK		0x0C
#define		BOARD_UNBEKANNT	0x00		// noch nicht bestimmt
#define		BOARD_V1			0x04		// Mainboard V1, Fotodiode
#define		BOARD_V2			0x08		// Mainboard V2, Lichtsensor VEML7700
#define		BOARD_TYP			(fp.radar&BOARD_MASK)
#define		IS_BOARD_V2		(BOARD_TYP==BOARD_V2)

// Voreinstellungen Werksinitialisierung
#define		Def_sprache	  0				// Erste indizierte Sprache
#define		Def_hell			100			// Anzeige Helligkeit 100%
#define		Def_farben		2				// 2 LED Farben
#define		Def_vmin			5				// 5 kleinste angezeigte Geschwindigkeit
#define		Def_vmax			199			// 199 gr��te angezeigte Geschwindigkeit
#define		Def_mcycle		1500		// Messzykluszeit in ms
#define		Def_TxF				24165		// Default Sendefrequenz 24165 MHz
#define		Def_mph				0				// Geschwindigkeitseinheit 0/1 kmh/mph
#define		Def_vcor			100			// Default Geschwindigkeitskorrekturfaktor 100%
#define 	Def_port_ic58	0x8000	// Default Voreinstellung IC58 Port (Plus Kommunikationsflag high)
#define		Def_U_offset	0				// Default Batteriespannungsoffset
#define		Def_pwm				1				// Default LED PWM Dimmmodus
#define		Def_paraset		5				// Default Anzahl Parameters�tze
#define		Def_txrcon		(ushort)(ANT_EN2|ANT_EN1)	// Default Verst�rker-Transceiver Voreinstellung
#define		Def_gpsintv		24			// Default GPS Positionsfix Zeitinterval
#define 	Def_wait			3				// Default Verbindungswartezeit GSM,MQTT,Email,SMS
#define		Def_vspcam		0				// Deaktiviert

// Aliase Speicher, Email und SMS Versand Flags in smstosend und mailtosend
#define		SYSERROR	(1<<4)				// Systemfehler 
#define		LOWBAT		(1<<5)				// Unterspannung 
#define		MEM95			(1<<6)				// Speicher zu 95% voll
#define		FULMEM		(1<<7)				// Speicher voll 
 
#define		TESTMAIL	(1<<0)				// Testmail	(mailtosend)
#define		DATAMAIL	(1<<1)				// Datenmail (mailtosend)
#define		ALARMMAIL	(1<<2)				// Fehler bzw. Statusmail (mailtosend)

#define 	TESTSMS		(1<<0)				// Test SMS	 	(smstosend)

#define 	SMTP					0					// Email Versand Servertyp SMTP 					
#define 	MQTT					1					// MQTT Servertyp

// Aliase Watchdog Reset Informationen
#define		FIRM_UP_RES		0xA501
#define		SLEEP_RES			0xA502
#define		BOOT_RES			0xB603

// Aliase f�r Nutzung nichtfl�chtige GPREG0-5 Register
#define	RTC_INIT_CODE			GPREG0		// RTC Initialisierung
#define RESET_IDENTIFIER	GPREG1		// Ablage Watchdog Reset-Sleep/Firmware Update
#define MQTT_send					GPREG2		// MQTT Sendezeiger (upper 16 Bit page, lower 16 Bit address)
#define MQTT_index				GPREG3		// Fortlaufender Index gesendeter Messdatens�tze
#define md_pg_adr					GPREG4		// Kopie des Messdatenzeigers (upper 16 Bit page, lower 16 Bit address)

// Messkonstanten und Definitionen
#define		Defminspeed		1							// kleinste anzeigbare Geschwindigkeit
#define		Defmaxspeed		199						// gr��te anzeigbare Geschwindigkeit
#define		BASECYCLE			250						// Grundzyklus 250 ms
#define		MIN_MESSCYCLE	2*BASECYCLE		// K�rzester Messzyklus 500 ms
#define		MAX_MESSCYCLE	20*BASECYCLE	// L�ngster Messzyklus 5 s
#define		MAX_LED_HOLD	40*BASECYCLE	// L�ngste Anzeigezeit 10 s
#define		DPWR_TRX			40						// ms Wartezeit nach Einschaltung Transceiver (bisheriger Transceiver)
#define		DPWR_VIA2			15						// ms Wartezeit nach Einschaltung Transceiver (Via-2)
#define		DPWR					(IS_VIA2 ? DPWR_VIA2 : DPWR_TRX)	// 5.L1: abhaengig vom Radar-Empfaenger
#define		DTXFEN				5							// ms Wartezeit nach Enable Transceiver
#define		DTXFEN_T			(IS_VIA2 ? 0 : DTXFEN)	// 5.L1: Via-2 hat keine getrennte Freigabestufe
#define		IFDIV	(3.6 * 299792456 / 2 / 1000)	// Teiler zur Bestimmung der Geschwindigkeitskonstanten ( 44 pro Hz/km/h)              
#define   MILE					1609					// engl. Mile in Meter
#define		LenTm					24						// Messpuffergr��e / Anzahl Dopplerperioden 
#define		MAX_VCOR   		150						// Maximal einstellbare Geschwindigkeitskorrektur in %
#define		PHASENMINIMUM 30						// Phasenlage IF1 zu IF2 minimal
#define		PHASENMAXIMUM	150						// Phasenlage IF1 zu IF2 maximal
#define		MAXFEHLER			10						// Maximaler Messfehler Puffervergleich
#define		LOWBATLIMIT		11500					// Warngrenze Batterie Unterspannung
#define		GPS_MAX_LIM		60						// Nach 60 Minuten Positionsbestimmung abschalten
#define		GPS_POS				25						// Maximal 25 GPS Positionen im Parameterblock sichern
#define		BT_TIMEOUT		6							// 6 Minuten Bluetooth Timeout ohne Zeichenempfang		
#define		HF_DIV				8388608				// Via-2: 1024 x 8192 Modulteiler Frequenzausgang
#define		HF_const			(192*8388608)	// Via-2: 8 * 24 * HF_DIV

// UART1 MUX Owner State Machine
enum { MUX_NONE=0, MUX_BT, MUX_GSM, MUX_GPS };	// UART1 resource owner states
extern uchar uart1_owner;													// Current UART1 MUX owner

extern uchar uart1_request (uchar requester, uint baud);	// Request UART1 ownership, returns TRUE if granted
extern void  uart1_release (uchar requester);							// Release UART1, auto-restores BT if connected

// Makrokommandos
#define		En_Capture	 	T2IR|=T2IR; T2CCR|=CAP0I|CAP0FE|CAP1FE	// Reset and enable captures und Interrupt f�r Periodendauermessung														
#define		En_Count			T2IR|=T2IR; T3IR|=T3IR;	T2CCR|=CAP0I|CAP0FE; T3CCR|=CAP0I|CAP0FE|CAP1I|CAP1FE		// Res. u. Def. Timer 2/3 Capt. u. Int.												 			
#define		Dis_CapCnt		T3CCR=T2CCR=0														// Disable Timer 2 und 3 captures und Interrupt	
#define 	MUX_GSM_Select FIO0CLR=CSBF; FIO0SET=CSAF 						// MUX GSM Kanal init & auswahl
#define 	MUX_BT_Select  FIO0CLR=CSAF|CSBF					 						// MUX BT Kanal init & auswahl
#define		MUX_GPS_Select FIO0CLR=CSAF; FIO0SET=CSBF 						// MUX GPS Typ 1 Kanal init & Auswahl
#define		MUX_NO_Select	 FIO0SET=CSAF|CSBF											// Kein Interface ausgew�hlt 

typedef struct tcapture {							// Capture Z�hlerst�nde	
	uint	cIF[LenTm+2];									// Z�hlerst�nde Signal In-Phase Frequenz A (IF1 Kanal 1)   
	uint	cIP[LenTm+2];									// Z�hlerst�nde Signal Phasendifferenz IF1-IF2
	uint	IF[LenTm+2];									// Periodendauer IF1
} tcap;

typedef struct geogps {				// GPS Positions- und Zeitdaten, Codierung siehe Definition viasis VTF Datei
	uint	utd;									// UTC Zeit und Datum vom Satelliten
	uint	lat;									// Latitude
	uint	lng;									// Longitude
} gpsgeo;					

typedef struct mqtt_package {	// Liste MQTT Nachrichtenverwaltung
	uchar mstyp;						// Nachrichtentyp
	uchar t_min;						// Minutenstempel Sendezeit
	ushort mpid;						// MQTT package Id
	uint t_ms;							// Zeitstempel Packetlaufzeit
} mqttpacks;

typedef struct mqtt_pub_data {	// Informationen DataMessage bei pending MQTT Messdatenversand
	ushort bytes_send;				// wird nach PUBACK auf DataMessage f�r Indexaktualisierung ben�tigt
	ushort send_page;					// Sendezeiger Flashseite
	ushort send_adr;					// Sendezeiger Flashadresse
} mqttd_pub;

typedef struct parameter_crc { // CRC Pr�fsummen f�r MQTT Kommunikation
	ushort param;							// Pr�fsumme Parameterblock
	ushort bitmap;						// Pr�fsumme Symbolbitmaps
 	ushort plus;							// Pr�fsumme PLUS Bitmaps und Textseiten
} mqtt_crc;	

typedef struct FlashSavedParameter { 	// Parameter, die dauerhaft im Flash gesichert werden
 uchar Kennung;  								// Pr�fkennung Werkinitialisierung
 char pVersion[8];							// Programmversion null terminiert
 char hwVersion[6];							// Hardwareversion null terminiert
 char serno[17]; 								// Ger�teseriennummer null terminiert
 uint i2cdev;										// Installierte I2C Baugruppen
 uint flash;										// Installierter Speichertyp 
 char swgrname[NOSWGR][SWGLEN];	// Schaltergruppen Bezeichner
 char swname [NOSW][SWLEN];			// Schalter Bezeichner
 uchar swgrp;										// Installierte Schaltergruppen, max. 3 Gruppen
 uchar swno[NOSW];							// Beigeordnete Schalternummer, z. B. Relais 1 oder 2 ...
 uchar swexp[NOSW];							// PCA Expander
 uchar swport[NOSW];						// Expander Port 0.0 bis 1.7 = 0 ... 15
 uchar nosw [NOSWGR];						// Anzahl Schalter in Gruppe
 ushort pro_start_page;					// Seitenzeiger auf Protokollstart im Flash
 ushort pro_page;								// Zeiger auf aktuelle Protokollseite im Flash
 ushort pro_adr;								// Protokoll Adresszeiger
 ushort md_start_page;					// Seitenzeiger auf Messdatenbeginn im Flash
 ushort md_page;								// Zeiger auf aktuelle Messdatenseite im Flash
 ushort md_adr;									// Messdaten Adresszeiger 
 uchar mph;											// Messeinheit km/h oder mph
 uchar uoff;										// Offset Batteriespannungsanzeige
 uchar pwm;											// LED PWM Dimmmodus
 uchar nk;											// Nachkommastelle 
 uchar dmode;										// Anzeigemodus 0/1 absolut/differentiell 
 uchar sprache;									// Programmtextsprache
 uchar hell[MAXTAST];						// Helligkeitswerte f�r Farbeinstellung
 uchar farben;									// Anzahl LED Farben der Anzeige
 uchar vspcam;									// viaspeedcam Option 0,1,2 - Aus/ 500ms Messzyklus/ 250ms Messzyklus
 uchar vcor;										// Geschwindigkeitskorrekturfaktor 
 uchar sz;											// Sommerzeit/Winterzeit, 1 wenn Sommerzeit
 uchar psets;										// Anzahl definierter Parameters�tze
 uchar vmm;											// kleinste gemessene Geschwindigkeit
 ushort TxF;										// Sendefrequenz in MHz
 ushort mcycle;									// Messzykluszeit in Millisekunden
 ushort acycle;									// Anzeigehaltezeit in Millisekunden  
 ushort scycle;									// Anzeigedauer Symbole/numerische Geschwindigkeit in Millisekunden
 ushort defic58;								// Default Expander IC58 Schaltereinstellung
 ushort defic68;								// Default Expander IC68 (Textseiten) Porteinstellung
 uint fileno;										// Fortlaufende Dateinummer f�r ymodem-Ausgabe 
 ushort u_usb;									// Letzter Spannungsmesswert 5V/3V3 Versorgung in mV
 ushort u_in;										// Letzter Eingangsspannungsmesswert in mV
 ushort u_hf;										// Letzter Spannungsmesswert USB 5V Versorgung in mV  
 uchar btmodem;									// Installiertes Bluetooth Modem
 uchar gsm;											// Installiertes GSM/GPRS Modem
 uchar usb;											// Installierte USB Schnittstelle
 uchar symbol;									// LED Anzeigesymbole, Bit 0 <30,50,70,!> Symbole, Bit 1 Smiley Symbole  
 uchar symgr;										// Installierte Led Anzeige-Symbolgruppen
 uchar nosym[NOSYGR];						// Symbolgruppe (-men�) des Anzeigsymbols
 char symgrname[NOSYGR][SYGLEN]; // Bezeichner Led Anzeigesymbolgruppe
 char symname [NOSYM][SYLEN];		// Namen Led Anzeigesymbole 
 ushort symfont[NOSYM];					// Fontnummer des Led Anzeigsymbols
 uchar ex12;										// 12V extern Bereitstellung
 uchar ledspot;									// >0 wenn LED Spotlampe installiert
 ushort simpin;									// GSM Modem SIM Pinnummer
 uchar pinerr;									// > 0 bei falscher Pinnummer
 uchar sense;										// Radar sensitivity 0...4 f�r 20% - 100%
 ushort txrcon;									// Transceiver/Verst�rkersteuerung
 uchar turnsw;									// 0/1 externe Drehschalter Parametersatzauswahl nicht installiert/installiert
 uchar fine;										// Viasis Plus fine mode ein/aus
 uchar gps;											// 0/1 GPS Module L70 nicht installiert/installiert
 uchar gpsintv;									// GPS Positionsfix Zeitintervall in Stunden
 uchar gpsanz;									// Anzahl GPS Positionsbestimmungen 0 ... 25
 uchar gi;											// Index letzte GPS Positionsbestimmung 0 ... 24
 uchar pwmex;										// Max. Tastverh�ltnis f�r externes PWM Signal
 uchar ledcode;									// Best�ckte LED Farben codiert, siehe Software Doku
 uchar csq;											// Funknetz Signalst�rke
 uchar opmode;									// MQTT Betriebsmodus Paket-/Einzel-Messdatenversand
 uint hcount;										// Betriebsstunden  
 uchar mqtt_packetsize;					// Messdaten Packetgr��e
 uchar mqtt_pdbq;								// Erweiterte Debug Protokollfunktion
 uchar autotime_off;
 uchar radar;										// 5.L1: Radar-Empfaenger RADAR_xxx (vorher fuellbyte1, fuellt ersten 1k Block, Layout unveraendert)
 uint btpin;										// Bluetooth Pin-/Pairing Nummer

 uchar pKennung;								// Pr�fkennung Parameterblock !!! Ab hier �berschreibt der Parameter Upload
 uchar bdir;										// Bidirektionale Messung 
 uchar s_w_zeit;								// 0 = Sommer-/Winterzeitumstellung aus, 1..3 = Umstellung f�r UTC+0..2
 uchar vmin[MAX_PARSET];				// Angezeigte kleinste Geschwindigkeit
 uchar vmax[MAX_PARSET];				// Angezeigte gr��te Geschwindigkeit
 uchar vblk[MAX_PARSET];				// Schwelle blinkende Anzeige  
 uchar vcol[MAX_PARSET];				// Schwelle 2. Anzeigefarbe
 uchar vmix[MAX_PARSET];				// Schwelle LED Mischfarbe
 uchar vlim[MAX_PARSET];				// Tempolimit
 uchar vsym[MAX_PARSET][2*NOSYM];	// Ein/Ausschaltschwellen LED Symbole 30,50,70,! und Smileys
 uchar vswi[MAX_PARSET][2*NOSW];	// Ein/Ausschaltschwellen Schalter
 uchar voff;										// Anzeigeoffset differentielle Anzeige
 uchar led[MAX_PARSET];					// Task Led Anzeige ein/aus
 uchar eintage[MAX_PARSET];			// Einschalttage des Parametersatzes
 ushort tein[MAX_PARSET];				// Einschaltzeit des Parametersatzes
 ushort taus[MAX_PARSET];				// Ausschaltzeit des Parametersatzes 
 char comment[81];							// Kommentar/Messort null terminiert
 uchar gsmtage;									// Einschalttage des GSM Modems
 ushort tgsmein;								// Einschaltzeit des GSM Modems
 ushort tgsmaus;								// Ausschaltzeit des GSM Modems
 ushort temail;									// Zeit Email Versand
 uchar ewtag;										// Wochentag Email Versand 0 .. 6
 uchar emtag;										// Monatstag Email Versand 1 .. 28 (29-31 unzul�ssig)
 char apn[32];									// GPRS access point name
 char user[16];									// APN user name
 char pass[16];									// APN password
 char server[32];								// SMTP oder MQTT Server URL oder IP
 char ehost[32];								// Email host name 
 char euser[32];								// Email user name
 char epass[32];								// Email password
 char emfrom[32];								// Sender (from) email address
 char emto[48];									// Receiver (to) email address
 char emcopy[48];								// Mail copy (to) email address
 char smsno[17];								// SMS phone number
 char smssc[17];								// SMS service center
 ushort port;										// Server Portnummer
 uchar mailalarm;								// 0/1 Email Alarm Nachrichten senden in-/aktiv
 uchar smsalarm;								// 0/1 SMS Alarm Nachrichten senden in-/aktiv
 uchar roaming;									// 0/1 deaktiviere/aktiviere roaming Versand
 uchar mailmode;								// 0...4 Email Versand Modus: keiner, Speicher voll, t�glich, w�chentlich, monatlich 
 uchar hoff;										// Helligkeitsoffset Kundeneinstellung
 uchar radnet;									// Funknetzwerk 0/1 f�r GSM/UMTS
 uchar servertyp;								// Access 0/1 f�r SMTP/MQTT Server
 int8_t utc;										// Zeitzone -11 < utc < 12
 uchar fill2[6];								// F�llbytes damit GPS Datenfeld, die letzen 300 Bytes im 2k Block belegt
 gpsgeo	gp[GPS_POS];						// GPS Datenfeld 25 Positionen � 3 x 4 = 300 Byte
} flashparameter;

extern char const fp_cp_area[4096];					// Bereich f�r Kopie Parameterblock

// Globale Variable
// *** Programm- und Zeitsteuerung ***
extern flashparameter fp;							// Kopie Flash Parameter Structure, siehe oben
extern volatile uint msdelay;					// Millisekunden Abw�rtsz�hler feeded by Timer 3
extern volatile uint mstimer;					// Millisekunden Aufw�rtsz�hler feeded by Timer 3
extern uint cycletime;								// Millisekunden Dauer Anzeigezyklus feeded by Timer 3
extern uchar tasks;										// Aufgaben Bit1 - Messzeit, Bit2 -GSM Datenversand
extern uchar timeupdate;							// 1 wenn zeitliche Aufgaben z. B. nach �nderung der Zeit
extern volatile uchar minute;					// Minutenwechsel der Echtzeituhr (RTC Interrupt)
extern ushort i_reset;								// Zus�tzliche Reset Information, z. B. bei Firmware Update
extern uchar transferzeit;						// Sperrzeit nach Winter auf Sommerzeit R�ckstellung 3 -> 2 Uhr
extern short LastErrorEvent;					// Ger�teprotokoll letztes Fehlerereignis
extern uchar t_amp_en;								// Verst�rker �bersteuerungszeit ersetzt Dampen

/*** Programmsteuerung/Threads/etc ***/
extern osThreadId usbh_thread_id;			// USB Host Thread ID
extern osThreadId gsmp_thread_id;			// GSM power and registration thread ID
extern osThreadId mqtt_thread_id;			// MQTT Verbindungsaufbau thread ID
extern uchar osActiveThread;					// Gestartete Threads 
// *** Schnittstellensteuerung ***			
extern uchar connect;									// Aktive Schnittstelle(n) Uarts, USB			
extern uchar concpy __at(0x2007e1d8);	// Sicherungskopie connect - aktive Verbindungen
extern uchar interfaces;							// W�hrend der Initialisierung aufgefundene Schnittstellen
extern volatile uchar comchange;			// 1 bei �nderung Status Datenschnittstellen oder Hex-Schalter
extern volatile uint comchange_count;	// TEMPORAER (5.69, CYSPP-Diagnose): EINT1-Interrupts seit Boot
extern uchar comstate;								// Status Datenschnittstellen an IC54 Expander
extern bool plustrans;								// 1 wenn transparenter Datentransfer Plus Matrix controller
// *** Uart-Ein/Ausgabe/Pufferindizes ***
extern int (*pchar)(int c); 					// Funktionszeiger auf Routine Zeichenausgabe			
extern volatile uchar rxi;						// Empfangsindex UART Empfangspuffer
extern volatile uchar bxi;						// Bearbeitungsindex UART Empfangspuffer
extern volatile uchar rbuf_full;			// 0 wenn UART Empfangspuffer leer
extern volatile uchar rxi1;					// Empfangsindex rbuf1 (UART1 Command-Mode, siehe 5.89)
extern volatile uchar bxi1;					// Bearbeitungsindex rbuf1
extern volatile uint u0_ovf;				// 5.111: von UART0-ISR verworfene Bytes (Diagnose)
extern volatile uchar rbuf1_full;		// 0 wenn rbuf1 leer
extern volatile uchar rpi;						// Empfangsindex Empfangsringpuffer MQTT und Plus Kommunikation
extern volatile uchar bpi;						// Bearbeitungsindex Empfangsringpuffer MQTT und Plus Kommunikation
extern ushort cind;										// Index Char Zeichenpuffer
/*** GSM/EMAIL/SMS ***/
extern uint  gsmbaud;                   // einstellbare GSM-Baudrate (Default 460800)
extern uchar gsmpower;								// 0 wenn GSM 3V5 Spannungsversorgung aus
extern uchar simpinset;								// 1 wenn GSM SIM Pinnummer gesetzt
extern uchar gsmcall;									// 1 bei transparenter GSM Einwahlverbindung
extern uchar mailfail;								// Aufw�rtsz�hler gescheiterte Mail Versandversuche
extern uchar smsfail;									// Aufw�rtsz�hler gescheiterte SMS Versandversuche
extern uchar wait_min;								// Minuten Wartezeit f�r GPS, EMAIL und SMS nach dem Einschalten oder Terminalverbindung 
extern uchar wait_sms;								// Warteminuten SMS Wiederholzeit
extern uchar wait_mail;								// Warteminuten Email Wiederholzeit
extern uchar mailtosend;							// > 0 wenn Email zu versenden
extern uchar smstosend;								// > 0 wenn SMS zu versenden, Bit1/2/3 = 1 - Test/Batteriespg niedrig/Speicher voll - SMS
extern uchar bt_time;									// Bluetooth Verbindungszeit�berwachung
extern uchar bt_pininit;							// Wenn 1 Bluetooth Pinabfrage erforderlich
/*** Email Versand Base64 Codierung	***/
extern uchar shiftcnt;								// Base64 Schiebefortschritt Puffer				
extern uint shiftb64;									// Base64 Codierung 3 Byte bzw. 4 x 6 Bit Schieberegister
extern uchar b64crlf;									// CRLF Z�hler, Base64 nach 57 Bytes
/*** Flashspeicher ***/
extern uint flashsize;								// Flash Speichergr��e KB/MB							 
extern uint maxpage;									// letzter Flash Seitenindex
extern uint maxbyte;									// Anzahl Byte pro Flash page
extern uchar pagepk;									// Ben�tigte Anzahl Flashseiten zur Sicherung eines 1 k-Blocks
extern uchar *P_fpar;									// Pointer auf Flash/Ymodem Transferdaten
/*** Men�steuerung ***/
extern uchar online;									// 1 wenn Online Datenausgabe							
extern uchar simulation;							// 1 wenn Anzeige- und Schaltersimulation
extern int vtst;											// Testgeschwindigkeit f�r Anzeige- und Schaltersimulation
extern uchar menrep;									// Flag Men�wiederholung
extern uint menu;											// Variable f�r Steuerung der Men�ebene
extern uchar startmessage;						// Startmeldung bei Verbindungsaufbau
extern uchar dset;										// Im Men� angezeigter Parametersatz
/*** Messparameter/Messung ***/
extern uchar pset;										// Aktuell ausgef�hrter Parametersatz		
extern uint IFConst;									// Doppler-Konstante ~44,7 Hz pro km/h
extern uint v_const;									// Konstante zur Geschwindigkeitsberechnung
extern uint t_mess_max;								// Maximale Messdauer
extern uchar Dampen;									// Verst�rker Enable �bersteuerungszeit
extern volatile uint t_mess;					// Messzeitz�hler
extern tcap tm;												// Messpuffer structur array mit Capture Z�hlerst�nden
extern volatile uchar tmi;						// Messindex Messpuffer IF und Phase
extern uint speed10;									// 10 fache Geschwindigkeit
extern volatile uint tsec;						// Test Echtzeituhr Sekundenzeitstempel
extern uint cap20,cap30,cap31;				// Capture Count Werte Timer 2 und 3 f�r Abgleich
/*** LED Anzeige ***/
extern uchar colors;									// LED Anzeigefarbe(n)										
extern uchar tast[MAXTAST];						// LED Tastverh�ltnisse f�r LED Farben
extern uint dimm;											// LED Dimmfaktor 1 ... 200
extern uchar farbcode;								// Farbversorgungen COL1-5 - EN-POWER 1-5 Treiberleitungen
extern uchar discharge;								// Entladung Clusterspannungs-Kondensatoren
/*** GPS Kommunikation ***/
extern uchar gbuf[PAGE];							// GPS Empfangsringpuffer 256 Byte
extern volatile uchar rx2;						// Empfangsindex GPS Empfangspuffer
extern volatile uchar bx2;						// Empfangsindex GPS Empfangspuffer
extern uchar gpsintv;									// Kopie fp.gpsintv Zeitintervall GPS Positionsbestimmung
extern uchar gps_pending;							// > 0 wenn GPS Positionsbestimmung gestartet
extern ushort gpsfix;									// Minutenz�hler f�r GPS Positionsbestimmung
/*** MQTT Kommunikation ***/
extern uchar mqbf[MQBFSIZE];					// MQTT Empfangs-Puffer 3072+256 Byte
extern volatile uchar mqtt_com;				// MQTT Nachrichtstatus 0/1/2 - nichts empfangen/laufend/vollst�ndig 
extern volatile ushort mqtt_len;			// MQTT Nachichtenl�nge
extern volatile uchar mqtt_head;			// MQTT fixed header length (incl.length remaining length bytes)
extern volatile ushort rmi;						// Empfangsindex MQTT Empfangspuffer
extern uint mqtttime;									// Zeitmarke MQTT Anforderung
extern ushort PacketId;								// PackageId
extern volatile uchar mqtt_fail;			// MQTT Kommunikationsfehler
extern mqttpacks mqttp[8];						// Structure f�r MQTT Packetverwaltung
extern uchar mqtt_state;							// Kommunikationsstatus mit MQTTS
extern uchar mqtt_mess_pend;					// > 0 wenn vom MQTTS unquittierte Nachricht(en)
extern uchar t_mqtt_status;						// MQTT Status Versendezeit
extern uchar mqtt_par;								// Parameterstatus Bit 0/1/2 Parameterblock/Symbolbl�cke/Plusdaten ge�ndert
extern uchar mqtt_manual;							// MQTT 0/1 automatischer / nicht automatischer Betrieb
extern mqtt_crc mcrc;									// CRC Pr�fsummen f�r Ger�teparameter in MQTT Kommunikation
extern mqttd_pub mqtt_pbd;						// Informationen DataMessage bei pending Messdatenversand
extern uchar poweron_min;							// Einschaltminute wird zum st�ndlichen MQTT Datenversand benutzt
extern ushort LastEvent;							// MQTT letztes Protokollereignis
extern uchar mqtt_param_get;					// MQTT Parameter/Bitmaps/Plus zu senden, Bit 7 Versand l�uft
extern uchar mqtt_set;								// MQTT Parameter/Bitmaps/Plus/Certs abzuholen, Bit 7 Abholung l�uft
extern uchar mqtt_md_simu;						// MQTT Messdatensimulation 0/1 - ein/aus
extern uchar mqtt_debug;							// Wenn 1 MQTT Debug Betrieb, d. h . Meldungsausgabe auf Terminal (UART0 oder USB)
extern uchar mqtt_con_err;						// MQTT Verbindungsfehler
extern uchar thread_comchange;				// Schnittstellenwechsel w�hrend GSM/MQTT Verbindungsthread
extern char certname[16];							// Zertifikatsname f�r MQTT command CertSave
extern ushort certcrc;								// Zertifikat CRC Pr�fsumme
extern ushort certsize;								// Zertifikat Dateigr��e
extern char certs[3][16];							// Zertifikatsliste
extern char iccid[23];								// SIM Karte ICCID Nummer
extern uchar btmode;									// BLE=48 oder SPP=47 Profil aktiv?

// Globale Arrays
extern char cbuf[CBUFSIZE];					// Char Arbeitspuffer (1056+256=1312 Byte)
extern char wcbuf[16];							// Char Arbeitspuffer
extern char rbuf[PAGE];							// Allgemeiner UART Zeichen Empfangsringpuffer, eine Seite lang
extern char rbuf1[PAGE];							// UART1 Command-Mode-Puffer (Protokoll/Status, siehe 5.89)
extern char pbuf[PAGE];							// Empfangsringpuffer f�r Plus Matrix eine Seite
extern uchar flbuf[FBUFSIZE];				// uchar Arbeitspuffer u. a. f�r flash und i2c transfers
extern uchar probuf[FLASHKGV];			// Puffer Protokolldaten
extern uchar vbuf[FLASHKGV];				// Puffer Geschwindigkeitsdaten
extern uchar ledbuf[6];							// Puffer f�r LED Anzeigeinformation
extern uint	iap_data[5];						// Globales Array f�r IAP Kommando/Ergebnis �bergabe

// Funktionen extern bereit stellen
extern void UART0_IRQHandler(void); 		// UART0 Sende- und Empfangsinterrupt - RS232
extern void UART1_IRQHandler(void); 		// UART1 Sende- und Empfangsinterrupt - BT, GSM/GPRS oder GPS(alt)
extern void UART2_IRQHandler(void); 		// UART2 Sende- und Empfangsinterrupt - GPS(neu)
extern void RIT_IRQHandler (void); 			// RI Timer match interrupt LED Steuerung
extern void TIMER2_IRQHandler (void);			// Timer 2 Capture Interrupt
extern void TIMER3_IRQHandler (void);		// Timer 3 1ms Match Interrupt
extern void PWM1_IRQHandler (void);			// PWM Interrupt, aktiv bei mehrfarbiger Anzeige
extern void Init_Ports (void);					// Initialisiert Ports und Pinfunktion
extern void Init_Peripheral (void);			// Initialisiert Ports fuer Peripherie
extern void InitTimer (void);						// Initialisiere Timer
extern void Init_UART0 (void);					// Initialisiert Schnittstelle UART0 (Terminal)
extern void Init_UART1 (uint baud);			// Initialisiert zweiten UART1 (BT, GSM oder GPS �ber MUX)
extern void Init_UART2 (uint baud);			// Initialisiert Schnittstelle UART2 f�r GPS Direktverbindung
extern void Init_RIT (void);						// RIT Timer f�r LED Steuerung initialisieren
extern void Init_Ext_Intr (void);				// Initialisiere externe Interrupts
extern void ResetWDT (void);						// Watchdog reset and feed
extern void ResetWDTm (void);						// Watchdog reset and feed measure
extern void InitWatchdog(uint millsec);	// Initialisiere Watchdog
extern void set_LED (uchar schema);			// Setze Einschalt-/Ausschaltzeit f�r LED
extern uint get_ad (uchar chanel);			// A/D Wandler Kanalwert ermitteln
extern uint get_brightness (void);			// Umgebungshelligkeit messen, A/D-Wert SFH213 Fotodiode
extern uint adj_hell (void);						// Kundenfaktor fp.hoff und Abgleichwerte fp.hell[] in Helligkeit ber�cksichtigen 
extern void set_pwm (uchar hell, uchar hellx);	// Setze interne und externe LED PWM			
extern void sendspi	(uchar data);				// Anzeigedaten �ber SPI versenden
extern void shift_to_LED (text * const sequence);	// numerische LED Anzeige 
extern void Ledaus (void);							// Abschaltung LED Anzeige
extern void wait_uarts_empty (void);		// Warte bis alle Zeichen auf uarts versendet
extern void DeepSleep (void);						// Bringe CPU in Deep power down
extern uint mess_hf (void);							// 5.L1: Via-2 HF-Sendefrequenz messen (Captureabstand von 8 Perioden)
extern void Init_Radar_Ports (void);		// 5.L1: Pins abhaengig vom Radar-Empfaenger setzen

#endif /* HARD_H_*/

