//-----------------------------------------------------------------------------
//  FILE: Hard.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Hardwarenahe Routinen für LPC1766
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H2
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   06.11.2014
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:	22.04.2015:	Hardware version H1 -> H2
//									13.07.2015: Hardware version H2 -> H3
//              	
//-----------------------------------------------------------------------------

#include "hard.h"
#include "btio.h"
//#include "sio.h"
#include "ramcode.h"
#include "string.h"
#include "sictxt.h"

#if (DEBUGGING == 1)
uint const CRP __at (0x2FC)	=	CRPNONE;	 	// Code read protection undefined im Programmspeicher
#else
uint const CRP __at (0x2FC)	=	CRP2;				// Code read protection level 2 im Programmspeicher 
#endif

char const fp_cp_area[4096] __at (0x7000);	// Programmspeicher Sektor 7 für Kopie Parameterblock

#pragma arm section zidata = "non_init"		/* nicht Null initialisierte globale Variable */
// Funktionszeiger
int (*pchar) (int c)		__noinit;					// Funktionszeiger Zeichenausgabe

// Globale Variable
__align(4) flashparameter fp 	__noinit;		// Parameter, die im Flash gesichert werden
tcap tm									__noinit;					// Messpuffer mit Capture Zählerständen
char rbuf[PAGE]					__noinit;					// Allgemeiner Empfangsringpuffer eine Seite
char pbuf[PAGE]					__noinit;					// Empfangsringpuffer für Plus Matrix eine Seite
__align(4) char cbuf[CBUFSIZE] __noinit;	// char Arbeitspuffer (1056+256=1312 Byte)
uchar probuf[FLASHKGV]	__noinit;					// Puffer Protokolldaten
uchar vbuf[FLASHKGV]		__noinit;					// Puffer Geschwindigkeitsdaten
uchar flbuf[FBUFSIZE]		__noinit;					// uchar Arbeitspuffer u. a. für flash commands
uint iap_data[5]				__noinit;					// Globales Array für IAP Kommando/Ergebnis Übergabe
volatile uchar rpi 			__noinit;					// Empfangsindex Empfangsringpuffer MQTT und Plus Kommunikation
volatile uchar bpi 			__noinit;					// Bearbeitungsindex Empfangsringpuffer MQTT und Plus Kommunikation
volatile uchar comchange __noinit;				// > 0 bei Änderung Status Datenschnittstellen
ushort cind							__noinit;					// Index Char Zeichenpuffer
uint ledton, ledtoff		__noinit;					// Ein-/Ausschaltwerte für LED
uint flashsize					__noinit;					// Flash Speichergröße KB/MB				
uint maxpage						__noinit;					// letzter Flash Seitenindex
uint maxbyte						__noinit;					// Anzahl Byte pro Flash page
uchar pagepk						__noinit;					// Benötigte Anzahl Flashseiten zur Sicherung eines 1 k-Blocks
uchar *P_fpar						__noinit;					// Pointer auf Flash/Ymodem Transferdaten
uchar connect						__noinit;					// Aktive Schnittstellen Uarts, USB etc.
uchar concpy						__noinit __at(0x2007e1d8);					// Sicherungskopie connect
uchar comstate					__noinit;					// Schnittstellenstatus BT, GSM, Terminal
uchar interfaces				__noinit;					// Während der Initialisierung aufgefundene Schnittstellen
uchar pset							__noinit;					// Aktuell ausgeführter Parametersatz
uchar timeupdate				__noinit;					// 1 wenn zeitliche Aufgaben z. B. nach Änderung der Zeit
uchar tasks							__noinit;					// Aufgaben Bit1 - Messzeit, Bit2 -GSM Datenversand
volatile uint tsec 			__noinit;					// Test Echtzeituhr Sekundenzeitstempel
uchar ledbuf[6]					__noinit;					// Puffer für LED Anzeigeinformation
uchar tast[MAXTAST]			__noinit;					// Liste LED Tastverhältnisse für LED Farbanzeigen 
uint cycletime					__noinit;					// Dauer Grundzykluszeit
uint speed10						__noinit;					// 10 fache Geschwindigkeit
uint IFConst						__noinit;					// Doppler-Konstante ~44,7 Hz pro km/h
uint v_const						__noinit;					// Konstante zur Geschwindigkeitsberechnung
uint volatile t_mess		__noinit;					// Messzeitzähler
uint t_mess_max					__noinit;					// Maximale Messdauer
ushort i_reset					__noinit;					// Zusätzliche Reset Information
uint dimm								__noinit;					// LED Dimmfaktor 1 ... 200
uint cap20,cap30,cap31	__noinit;					// Capture Count Werte Timer 2 und 3
uchar gsmcall						__noinit;					// 1 wenn GSM Datenverbindung besteht
uchar gsmpower					__noinit;					// 0 wenn GSM 3V5 Spannungsversorgung aus
uchar mailtosend				__noinit;					// > 0 wenn Email zu versenden, Bit1/2/3 = 1 - Testmail/Datenmail/Fehlermail
uchar smstosend					__noinit;					// > 0 wenn SMS zu versenden, Bit1/2/3 = 1 - Test/Batteriespg niedrig/Speicher voll - SMS
uchar wait_min					__noinit;					// Minuten Wartezeit für GPS, EMAIL und SMS nach dem Einschalten oder Terminalverbindung 
//uint minuten						__noinit;					// Minutenlaufzeit seit dem Einschalten oder Terminalverbindung
uchar simpinset					__noinit;					// 1 wenn GSM SIM Pinnummer gesetzt
int vtst								__noinit;					// Testgeschwindigkeit für Anzeige- und Schaltersimulation
uchar gps_pending				__noinit;					// > 0 wenn GPS Positionsbestimmung gestartet
ushort gpsfix						__noinit;					// Minutenzähler für GPS Positionsbestimmung
uchar gpsintv						__noinit;					// Kopie fp.gpsintv Zeitintervall GPS Positionsbestimmung
uchar transferzeit			__noinit;					// Sperrzeit nach Winter auf Sommerzeit Rückstellung 3 -> 2 Uhr
uchar gbuf[PAGE]				__noinit;					// GPS Empfangsringpuffer 256 Byte
uchar mqbf[MQBFSIZE]		__noinit;					// MQTT Empfangs-Puffer 3072+256 Byte
volatile ushort mqtt_len	__noinit;				// MQTT Nachichtenlänge
volatile uchar mqtt_head 	__noinit;				// MQTT fixed header length + length remaining length bytes
mqttpacks mqttp[8]				__noinit;				// Structure für MQTT Packetverwaltung
uchar mqtt_mess_pend 		__noinit;					// > 0 wenn vom MQTTS unquittierte Nachricht(en)
mqtt_crc mcrc						__noinit;					// CRC Prüfsummen für Geräteparameter in MQTT Kommunikation
mqttd_pub mqtt_pbd			__noinit;					// Informationen DataMessage bei pending Messdatenversand
uchar poweron_min				__noinit;					// Einschaltminute wird zum stündlichen MQTT Datenversand benutzt
uchar mqtt_set					__noinit;					// MQTT Parameter/Bitmaps/Plus oder Certs abzuholen, Bit 7 Abholung läuft 
uchar mqtt_param_get		__noinit;					// MQTT Parameter/Bitmaps/Plus zu senden, Bit 7 Versand läuft
uchar mqtt_md_simu			__noinit;					// MQTT Messdatensimulation 0/1 - ein/aus
uchar mailfail					__noinit;					// Aufwärtszähler gescheiterte Mail Versandversuche
uchar smsfail						__noinit;					// Aufwärtszähler gescheiterte SMS Versandversuche
uchar wait_sms					__noinit;					// Warteminuten SMS Wiederholzeit
uchar wait_mail					__noinit;					// Warteminuten Email Wiederholzeit
short LastErrorEvent		__noinit;					// Letzter protokolliertes Fehlereignis
uchar mqtt_con_err			__noinit;					// MQTT Verbindungsfehler
uchar thread_comchange	__noinit;					// Schnittstellenwechsel während GSM/MQTT Verbindungsthread
uchar online						__noinit;					// 1 wenn Online Datenausgabe
char certname[16]				__noinit;					// Zertifikatsname für MQTT command CertSave
ushort certcrc					__noinit;					// Zertifikat CRC Prüfsumme
ushort certsize					__noinit;					// Zertifikat Dateigröße
char certs[3][16]				__noinit;					// Zertifikatsliste	
char iccid[23]					__noinit;					// SIM Karte ICCID Nummer
uchar t_amp_en					__noinit;					// Verstärker Übersteuerungszeit ersetzt Dampen
uchar bt_time						__noinit;					// Bluetooth Verbindungszeitüberwachung
uchar btmode						__noinit;					// BLE=48 oder SPP=47 Profil aktiv?
	
#pragma arm section zidata			/* Globale Variable initialisiert */
volatile uchar rxi=0;						// Empfangsindex UART Empfangspuffer
volatile uchar bxi=0;						// Bearbeitungsindex UART Empfangspuffer
volatile uchar rbuf_full=0;			// 0 wenn UART Empfangspuffer leer
volatile uchar tmi=0;						// Messindex Messpuffer IF und Phase
volatile uchar minute=0;				// Minutenwechsel der Echtzeituhr (RTC Interrupt)
volatile uint msdelay=0;				// Millisekunden Abwärtszähler feeded by Timer 0
volatile uint mstimer=0;				// Millisekunden Aufwärtszähler feeded by Timer 0	
osThreadId usbh_thread_id=0;		// USB Host Thread ID	
uchar simulation=0;							// 1 wenn Anzeige- und Schaltersimulation
uchar menrep=0;									// Flag Menüwiederholung
uint menu=0;										// Variable für Steuerung der Menüebene
uchar dset=0;										// Im Menü angezeigter Parametersatz
uchar startmessage=0;						// Startmeldung bei Verbindungsaufbau
uchar farbcode=0;								// Farbversorgungen COL1-5 - EN-POWER 1-5 Treiber
uchar shiftcnt=0;								// Base64 Schiebefortschritt letzter Puffer
uint shiftb64=0;								// Base64 Codierung 3 Byte bzw. 4 x 6 Bit Schieberegister
uchar b64crlf=0;								// CRLF Zähler, Base64 nach 57 Bytes
bool plustrans=0;								// transparenter Datentransfer Plus Matrix controller
uchar alcol=0;									// Farbwechsel Zyklus
uchar discharge=0;							// Entladung Clusterspannungs-Kondensatoren
uchar Dampen=10;								// Verstärker Übersteuerungszeit
volatile ushort rmi=0;					// Empfangsindex MQTT Empfangspuffer
volatile uchar rx2=0;						// Empfangsindex GPS Empfangspuffer
volatile uchar bx2=0;						// Empfangsindex GPS Empfangspuffer
uint mqtttime=0;								// Zeitmarke MQTT Anforderung
ushort PacketId=1000;						// PackageId zählt aufwärts
volatile uchar mqtt_com=0;			// MQTT Nachrichtstatus 0/1/2 - nichts empfangen/laufend/vollständig
volatile uchar mqtt_fail=0;			// MQTT Kommunikationsfehler
osThreadId gsmp_thread_id=0;		// GSM power and registration thread ID	
uchar osActiveThread=0;					// Gestartete Threads 
osThreadId mqtt_thread_id=0;		// MQTT Verbindungsaufbau thread ID
uchar mqtt_debug=0;							// Wenn 1 MQTT Debug Betrieb, d. h . Meldungsausgabe auf Terminal (UART0 oder USB)
uchar t_mqtt_status=0;					// MQTT Status Versendezeit
uchar mqtt_manual=0;						// MQTT 0/1 automatischer / nicht automatischer Betrieb
ushort LastEvent=0;							// MQTT letztes Protokollereignis
uchar mqtt_state=0;							// Kommunikationsstatus mit MQTTS, 0/1/2/3/4/... = disconnect/connected/joined/status send/subscribed to command/...
uchar bt_pininit=0;							// Wenn 1 Pinabfrage erforderlich

/**** Interrupt Priority Tabelle (statisch)
EINT0_IRQn		6							- Power down 12V Über/-Unterspannungsüberwachung IC4, LTC4365
RTC_IRQn 			8							
EINT2_IRQn	 	12						-	Detektionsfehler IF/IQ
TIMER1_IRQn 	13
TIMER2_IRQn	 	14
EINT1_IRQn 		15						- Interruptanforderung I2C Expander IC54, PCA9554
TIMER3_IRQn	 	16
PWM1_IRQn			28						-	LED Auffrisch/Farbwechsel Interrupt, Priorität siehe Init_Timer
UART1_IRQn 		29
UART0_IRQn 		30
UART2_IRQn 		31

*****/

void UART0_IRQHandler(void) 						// UART0 Sende- und Empfangsinterrupt
{
 volatile char c;				// Dummy

 switch (U0->IIR&0x000E)								// Verteile je nach Interruptquelle
 {	
  case 0x0002:	break;																	// Kein Puffer leer Sendeinterrupt
  case 0x0004: 	if (!plustrans)	rbuf[++rxi]=U0->RBR; 		// Zeichen in allgemeinen Empfangspuffer, präinkrement									
								else pbuf[++rpi]=U0->RBR;								// Zeichen in Plus Empfangspuffer, präinkrement
								break;		
  default: 		c=U0->LSR;	 							// Dummy read Rx Fehlerstatus  
 }
}

void UART1_IRQHandler(void) 						// UART1 Sende- und Empfangsinterrupt
{
 volatile char c;				// Dummy

 switch (U1->IIR&0x000E)								// Verteile je nach Interruptquelle
 {	
  case 0x0002:	break;												// Kein Puffer leer Sendeinterrupt
  case 0x0004: 	if ((connect&MQTT_LINK)&&(bpi!=(rpi+1))) 	// MQTTS Verbindung besteht und Platz in Puffer? 								
								 pbuf[++rpi]=U1->RBR; 										// Zeichen in Empfangspuffer, präinkrement	
								else if (!(connect&MQTT_LINK) && (bxi!=(rxi+1))) // Keine MQTTS Verbindung und Platz in Puffer?
								{	
								 rbuf[++rxi]=U1->RBR; 		// Sonst Zeichen in Empfangspuffer, präinkrement
								 bt_time=BT_TIMEOUT;			// Reset Bluetooth Verbindungstimeout	
								}	
								else											// Ringpufferindizes vor Überlauf? 
								{													// I. d. F. solange U1->RBR ungelesen RTS=High -> Empfangsstop
								 rbuf_full=1;										// Puffer voll Flag setzen
								 NVIC_DisableIRQ(UART1_IRQn);		// Disable UART1 Interrupt
								} 
								break;		
  default: 		c=U1->LSR;	 							// Dummy read Rx Fehlerstatus  
 }
}

void UART2_IRQHandler(void) 						// UART2 Sende- und Empfangsinterrupt
{
 volatile char c;				// Dummy

 switch (U2->IIR&0x000E)								// Verteile je nach Interruptquelle
 {	
  case 0x0002:	break;																		// Sendeinterrupt
  case 0x0004: 	if (bx2!=(rx2+1)) gbuf[++rx2]=U2->RBR; 		// Platz in Puffer? Ja, Zeichen in Empfangspuffer, präinkrement															   
								else c=U2->RBR;														// Dummy read und wegwerfen
								break;		
  default: 		c=U2->LSR;	 							// Dummy read Rx Fehlerstatus  
 }
}

void EINT1_IRQHandler (void)				// Interruptanforderung I2C Expander
{
 comchange=1;				// Schnittstellenstatus geändert
 EXTINT|=(1<<1);		// Anforderung löschen								 	
}	

void EINT2_IRQHandler (void)				// Interruptanforderung Detektionsfehler
{
 tmi=0;							// reset Messpufferindizes
 EXTINT|=(1<<2);		// Anforderung löschen								 	
}

void TIMER1_IRQHandler (void)					// Timer 1 1ms Match Interrupt
{
  T1MR0=T1TC+1000;										// nächstes 1 ms Match setzen
	if (t_mess) t_mess--;								// Messzeitzähler - 1 
  if (msdelay!=0) msdelay--;					// Dekrementiere 1 ms Abwärtszähler	
  mstimer++;													// Inkrementiere 1 ms Aufwärtszähler	
  cycletime++;												// Inkrementiere 1 ms Aufwärtszähler, Mess- und Anzeigesteuerung
	while (T1IR) T1IR|=T1IR;						// Anstehenden Match Interrupt löschen 	 
}

void TIMER3_IRQHandler (void)				// Timer 3 Count Interrupts
{
 if ((T2CCR&CAP1FE)==0)							// Transceiver-/Verstärkerabgleich
 {
	if (T3IR&(1<<4))									// CR0 Interrupt, D-IQA oder D-IQB
	{
	 cap30++;													// Inkrementiere Impulszähler D-IQA / D-IQB		
	 while (T3IR&(1<<4)) T3IR|=(1<<4);	// Lösche CR0 Interrupt Flag 	
	}
	else if (T3IR&(1<<5))							// CR1 Interrupt, D-IFB
	{
	 cap31++;													// Inkrementiere Impulszähler D-IFB
	 while (T3IR&(1<<5)) T3IR|=(1<<5);	// Lösche CR1 Interrupt Flag	
  }		
	else while (T3IR) T3IR|=T3IR;			// Lösche alle Interruptanforderungen
 }	 
}

void TIMER2_IRQHandler (void)				// Timer 2 Capture Interrupt
{
 if (T2CCR&CAP1FE)									// Messerfassung
 {
	if (tmi>=LenTm) tmi=0;						// reset Messpufferindex am Pufferende  
  if (FIO2PIN&DETEKT) 							// Detektionssignal ok?
  {
   tm.cIF[tmi]=T2CR0;								// D-IFA, IF1 Zählerstand auslesen
   tm.cIP[tmi++]=T2CR1;							// Phase A, IP1 Zählerstand lesen
  }
  else tmi=0;												// Pufferindex reset 
 }
 else 															// Transceiver-/Verstärkerabgleich
 {	 
	cap20++;													// Inkrementiere Impulszähler D-IF-A	
 }	
 while (T2IR) T2IR|=T2IR;						// Anstehenden Capture Interrupt löschen	
}	

void sendspi	(uchar data)			// Daten über SPI versenden
{
 uint rdelay;												// Antwortzeit in us
 rdelay=T1TC;												// Timer 1 Stand laden	
 while (SSP1->SR&BSY)								// Warte bis Transmitter leer	
	if ((T1TC-rdelay) >= 10) return;	// Nicht bereit in 10 us -> Abbruch sofort
 SSP1->DR=data;											// Opcode Flash Statusregister in FIFO
 while (SSP1->SR&BSY)								// warte bis gesendet
  if ((T1TC-rdelay) >= 20) break;		// Abbruch kein Transfer in 20 us 
}

void PWM1_IRQHandler (void )			// PWM Interrupt, aktiv bei mehrfarbiger Anzeige
{
 uchar i;
 
 if (!(PINSEL0&(MOSI|SCK|MISO)))	// SPI nicht aktiv?
 {	 
  Init_SPI();														// SPI initialisieren	
	for (i=4;i;i--)	sendspi	(ledbuf[i]);	// Daten über SPI versenden  
  if (alcol==COL1) alcol=COL2; 			// Toggle Farbe
  else alcol=COL1; 
	sendspi	(alcol|discharge); 				// Farbinformation über SPI versenden
	FIO2SET = LE_TRB;									// Latch Schiebedaten 74HC595 
  while ((FIO2PIN & LE_TRB) == 0);	// Warte bis gesetzt 
  FIO2CLR = LE_TRB;									// Latch reset -> LED Treiber MBI5039 Datenübernahme   
	PCONP&=~PCSSP1;	 									// SSP1 power disable
  PINSEL0&=~(MOSI|SCK|MISO);				// Deselect SSP1 Pinfunktionen
 }	
 PWMIR|=PWMIR;	 										// Interrupt wieder löschen
}

void ResetWDT (void)								// Watchdog reset and feed
{
 __disable_irq();										// Interrupts sperren
 WDFEED=0xAA;												// Timed access sequence
 WDFEED=0x55; 	
 __enable_irq();										// Interrupts wieder freigeben
}	

void ResetWDTm (void)								// Watchdog reset and feed
{
 __disable_irq();										// Interrupts sperren
 WDFEED=0xAA;												// Timed access sequence
 WDFEED=0x55; 	
 __enable_irq();										// Interrupts wieder freigeben
}

void InitWatchdog(uint millsec)			// Initialisiere Watchdog 
{ 
 WDTC=millsec*(RCCLK/4/1000);				// Reset Zeitpunkt berechnen
 WDMOD=WDEN | WDRESET;							// enable Watchdog and Reset 
 ResetWDT ();												// Feed sequence
}

void Init_Ext_Intr (void)						// Initialisiere externe Interrupts
{																		// Pin connect siehe Init_Ports()
 NVIC_DisableIRQ(EINT2_IRQn);				// Disable externen Interupt 2, Detektionsfehler	
 NVIC_DisableIRQ(EINT1_IRQn);				// Disable externen Interupt 1, Expander Interrupt
 NVIC_DisableIRQ(EINT0_IRQn);				// Disable externen Interupt 0, Power down Interrupt	 
 EXTMODE=(1<<2)|(1<<1)|(1<<0);			// Externe Interrupts sind Flankeninterrupts	
 EXTPOLAR=0;												// Externe Interrupts auf fallende Flanke
 NVIC_SetPriority(EINT0_IRQn, 6);		// Set EINT0 high Interrupt Priority
 NVIC_SetPriority(EINT1_IRQn, 15);	// Set EINT1 medium Interrupt Priority	 
 NVIC_SetPriority(EINT2_IRQn, 12);	// Set EINT2 medium Interrupt Priority	
 EXTINT|=EXTINT;										// Clear all external interrupts	
 NVIC_ClearPendingIRQ(EINT0_IRQn);
 NVIC_ClearPendingIRQ(EINT1_IRQn);
 NVIC_ClearPendingIRQ(EINT2_IRQn);	
}	

void InitTimer (void)				// Initialisiere Timer für Millisekunden Zähler
{
 PCONP|=(PCTIM1|PCTIM2|PCTIM3|PCPWM1);		// Power Timer 1, 2 und 3	und PWM1

 T1CR = 0x02;								// stop and reset Timer 1
 T1PR = 0x02;								// 1 MHz update, 24MHz/8/3	
 T1MCR = 0x01;							// MR0 Interrupt
 T1MR0=T1TC+1000;						// 1 ms Match 
 T1IR|=T1IR;								// Reset evtl. anstehende Interrupts	
 T1CR=0x01;									// starte Timer 1	
	
 NVIC_SetPriority(TIMER1_IRQn, 13);		// Set priority
 NVIC_EnableIRQ(TIMER1_IRQn);					// Enable Timer Interrupt
	
 T3CR = T2CR = 0x02;				// stop and reset Timer 2	and 3
 T3PR = T2PR = 0x02;				// 1 MHz update, 24MHz/8/3
 
 T2IR|=T2IR;								// Reset evtl. anstehende Interrupts	
 T3IR|=T3IR;								// Reset evtl. anstehende Interrupts
 Dis_CapCnt;								// Disable Captures und Interrupts	
 //T2CCR=CAP0FE|CAP1FE;			// Timer 2, falling edge Captures D-IF-A und Phase A
 //T3CCR=CAP0FE|CAP1FE;			// Timer 3, falling edge Captures MUX(Phase B, D-IQ-A/B) und D-IF-B 	
 T3TC=1;										// Synchronisiere Timer 2 und 3	
 T3CR=T2CR=0x01;						// start Timer 2 und 3
 
 PWMMCR=PWMMR0R;											// Reset bei PWM 0 match
 PWMPR=(CCLK/4/F_PWM/RES_PWM)-1;			// PWM Vorteiler
 PWMPCR=PWMENA6|PWMENA5|PWMENA3|PWMSEL5;			// Enable PWM 3, 5 und 6 output to Pin, PWM5 double edge control
	
 NVIC_SetPriority(TIMER2_IRQn, 14);		// Set priority
 NVIC_EnableIRQ(TIMER2_IRQn);					// Enable Timer 2 Interrupt
 NVIC_SetPriority(TIMER3_IRQn, 16);		// Set priority
 NVIC_EnableIRQ(TIMER3_IRQn);					// Enable Timer 3 Interrupt
 NVIC_SetPriority(PWM1_IRQn, 28);			// Set priority
 NVIC_EnableIRQ(PWM1_IRQn);						// Enable PWM1 Interrupt
}	

void Init_UART0 (void)			// Initialisiert Schnittstelle UART0 (Terminal)
{	
 PCONP|=PCUART0;						// Power Uart 0	
 U0->IER = 0x00;						// Disable Interrupts 
 U0->FCR = 0x07; 						// FIFO control enable and reset buffers	
 U0->LCR = 0x00000083;			// Line control 8,N,1, DLAB=1
 U0->DLL = 0x04;						// Baudrate Ganzzahlteiler für 115200 Baud
 U0->LCR = 0x00000003;			// Line control 8,N,1, DLAB=0 	
 U0->FDR = (8<<4)|0x05;			// Fractional divider	5/8 			 
 U0->IER = 0x01;						// Enable Rx Interrupt
 NVIC_SetPriority(UART0_IRQn, 30);
 NVIC_EnableIRQ(UART0_IRQn);	
}

void Init_UART1 (uint baud)					// Initialisiert zweiten UART1 (BT, GSM)
{																		// Übergaben baud - 9600 oder sonst immer 460800 
 if (!baud)													// Baudrate=0, de-initialisiere UART1
 {
	NVIC_DisableIRQ(UART1_IRQn); 			// Sperre Uart Interrupts wieder
	PINSEL4&=~(TXD1|RXD1|CTS1|RTS1);	// Uart1 Pins trennen -> wieder Eingänge mit Pull-up 
	PCONP&=~PCUART1;									// Power Uart 1 wegnehmen
  connect&=~UART1;									// Uart 1 deaktiviert	
  MUX_NO_Select;										// MUX auf unbenutzten Kanal	 
	return; 
 } 	 
	
 PCONP|=PCUART1;						// Power Uart 1 
	
 U1->IER = 0x00;						// Disable Interrupts	
 U1->FCR = 0x07; 						// FIFO control enable and reset buffers	
 U1->LCR = 0x00000083;			// Line control 8,N,1, DLAB=1	
 
 if (baud==9600)						// Baudrateeinstellung bei 24 MHz und PCLK=CCLK
 {	 												// 9600 Baud + 0,15%
	U1->DLL = 0x68;						// BT/GSM Erstinitialisierung 	
	U1->FDR = (2<<4)|0x01;		// Fractional divider	1/2 
 }	 
 else if (baud==115200)			// +0,16%
 {
	U1->DLL = 0x08;						// Baudrate Ganzzahlteiler	
  U1->FDR = (8<<4)|0x05;		// Fractional divider	5/8
 } 	 
 else	if (baud==460800) 
 {	 					 							// 460800 Baud +1,6%
	U1->DLL = 0x03;						// Baudrate Ganzzahlteiler  	
	U1->FDR = (12<<4)|0x01;		// Fractional divider	1/12
 } 
 else if (baud==307200)			// 307200 Baud + 0,16%	 
 {
	U1->DLL = 0x03;					 	
	U1->FDR = (8<<4)|0x05;		// 24MHz/(16x3x1,625)	
 }	 
 U1->LCR = 0x00000003;			// Line control 8,N,1, DLAB=0
 U1->MCR|=(1<<7)|(1<<6);		// Enable RTS/CTS flow control
 U1->IER = 0x01;						// Enable Rx Interrupt 
 
 PINSEL4|=(TXD1|RXD1|CTS1|RTS1);		// Uart1 Pins verbinden
 
 NVIC_SetPriority(UART1_IRQn, 29);
 NVIC_EnableIRQ(UART1_IRQn);
}

// UART1 MUX Resource Owner State Machine
uchar uart1_owner = MUX_NONE;				// Current UART1 owner, default: no owner

uchar uart1_request (uchar requester, uint baud)	// Request UART1 resource
{																											// Returns TRUE if granted
 __disable_irq();												// Critical section: protect owner + connect
 if (uart1_owner == requester)						// Already owns UART1?
 {
	__enable_irq();
	Init_UART1(baud);											// Only update baud rate
	return(TRUE);
 }
 if (uart1_owner == MUX_BT)							// BT currently owns UART1?
	connect &= ~UART1;										// Suspend BT terminal output
 uart1_owner = requester;								// Claim ownership atomically
 __enable_irq();												// End critical section
 
 osDelay(3);														// Wait for last byte to finish
 switch (requester)											// Switch MUX to requested channel
 {
	case MUX_BT:  MUX_BT_Select;  break;
	case MUX_GSM: MUX_GSM_Select; break;
	case MUX_GPS: MUX_GPS_Select; break;
	default: return(FALSE);
 }
 Init_UART1(baud);											// Configure UART1 baud rate
 osDelay(3);														// Wait for MUX settling
 
 __disable_irq();												// Critical section: restore connect
 if ((requester == MUX_BT) && (connect & BT_LINK))	// BT requested and BT connected?
	connect |= UART1;										// Restore BT terminal output
 __enable_irq();

 return(TRUE);
}

void uart1_release (uchar requester)		// Release UART1 resource
{
 __disable_irq();												// Critical section: check owner
 if (uart1_owner != requester)					// Not the owner?
 { __enable_irq(); return; }						// Ignore
 __enable_irq();
 
 if ((requester != MUX_BT) && (connect & BT_LINK))	// Non-BT releasing, BT still connected?
	uart1_request(MUX_BT, BT_BAUD);				// Auto-restore BT terminal (sets owner atomically)
 else
 {
	Init_UART1(0);												// Deinitialize UART1 (clears connect&UART1 internally)
	__disable_irq();											// Critical section: final state
	MUX_BT_Select;												// MUX to BT (idle default state)
	uart1_owner = MUX_NONE;								// No owner
	__enable_irq();
 }
}

void Init_UART2 (uint baud)			// Initialisiert Schnittstelle UART2 für GPS Direktverbindung
{																// Übergabe baud rate 0=off, 9600 und 115200 benötigt 

 if (!baud)													// Baudrate=0, de-initialisiere UART2
 {
	connect&=~UART2;									// Sperre UART2 Ausgabe
  concpy=connect;										// Sperre UART2 Ausgabe	 
	NVIC_DisableIRQ(UART2_IRQn); 			// Sperre Uart 2 Interrupts wieder
	PINSEL0&=~(TXD2|RXD2);						// Uart2 Pins trennen -> wieder Eingänge mit Pull-up
  	 
	PCONP&=~PCUART2;									// Power Uart 2 wegnehmen  	 
	return; 
 }	
 if ((baud==460800)||(baud==115200)||(baud==9600))
 {	 
	if (PCONP&PCUART2)								// Uart 2 aktiv
  {
	 NVIC_DisableIRQ(UART2_IRQn);
	}		
	else  PCONP|=PCUART2;							// Power Uart 2
	PINSEL0|= (TXD2|RXD2);						// Uart2 Pins verbinden 	 
	U2->IER = 0x00;										// Disable Interrupts 
  U2->FCR = 0x07; 									// FIFO control enable and reset buffers	
  U2->LCR = 0x00000083;							// Line control 8,N,1, DLAB=1
	if (baud==9600) U2->DLL = 0x030;				// Baudrate Ganzzahlteiler für 9600 Baud
	else if (baud==115200) U2->DLL = 0x04;	// Baudrate Ganzzahlteiler für 115200 Baud
	else U2->DLL = 0x001;										// Baudrate Ganzzahlteiler für 460800 Baud 
  U2->LCR = 0x00000003;							// Line control 8,N,1, DLAB=0 	
  U2->FDR = (8<<4)|0x05;						// Fractional divider	5/8 			 
  U2->IER = 0x01;										// Enable Rx Interrupt
  NVIC_SetPriority(UART2_IRQn, 31);
  NVIC_EnableIRQ(UART2_IRQn);	 
 }	
}

void Init_Ports (void)	// Initialisiert Ports und Pinfunktion
{
	PINSEL0 = TXD0|RXD0|CAP20|CAP21;	// Uart0 - RXD0,TXD0 und Capture 2(0) und 2(1)
	PINSEL1 = AD02|AD03|CAP30|CAP31;	// AD Eingänge AD02 und 03 und Capture 3(0) und 3(1)
	PINSEL4 = EINT0|EINT1|EINT2;			// Externe Interrupts EINT0,1 und 2
	//PINSEL7 = PWM3;									// Externe PWM

	FIO0SET = CSFLASH2;
	FIO0CLR = GSM_DTR|PWR_GPS|TXD_2;						// GPS erstmal aus, GSM DTR low
  FIO1SET =	USB_PPWR|MUX_B|USB_RPWR;					// USB Port Versorgung aus, Uart1 MUXB, USB 5V Regler ein
	FIO1CLR = PWRKEY|GSMPWR|LED1|MUX_A|EN_ANA;	// GSM/GPS erst aus, LED ein, Transceiver/Analogteil aus
	if (RSID&POR)																// Power On Reset
	{	
	 FIO2CLR = PWM_TRB|LE_TRB;									// Latch Enable LED Treiber
	 FIO2SET = G_SHIFT|DIS_TRB;									// PWM Pin Treiber HI, 74HC595 Ausgänge Tristate, 3V3 Treiber aus
	}	
	else																				// Im getakteten Messbetrieb 
	{
	 FIO2CLR = LE_TRB|DIS_TRB|G_SHIFT;					// Latch Enable LED Treiber, 3V3 Treiber aus, 74HC595 Ausgänge aktiv
   FIO2SET = PWM_TRB;													// PWM Pin Treiber HI 	 		
  }		
	FIO4SET	= CSFLASH1;													// FLASH 1 deselect
	
  if ((RSID&WDTR)&&((i_reset==FIRM_UP_RES)||(i_reset==SLEEP_RES))) // WDTR Firmware update oder Sleep
	{
	 if (gps_pending||gsmpower)	FIO1SET = GSMPWR;					// GSM und GPS Versorgung weiter ein
	 if (gps_pending&&(fp.gps!=3)) FIO0SET = PWR_GPS;			// GPS Typ 1 und 2 weiter ein
   if (gsmpower) FIO1SET = PWRKEY; 											//	GSM Modem weiter ein	
  }	
  else gps_pending=0;																	// kein Update, kein Sleep ->reset gps_pending

 	FIO0DIR = PWR_GPS|EX12_1|EX12_2|CSFLASH2|CSAF|CSBF|GSM_DTR|TXD_2;					// Port 0 Ausgänge		
	FIO1DIR = USB_PPWR|USB_RPWR|LED1|MUX_A|MUX_B|GSMPWR|PWRKEY|EN_ANA;								// Port 1 Ausgänge		
	FIO2DIR = DIS_TRB|LE_TRB|G_SHIFT|PWM_TRB;																					// Port 2 Ausgänge		
	FIO4DIR	= CSFLASH1;						 																										// Port 4 Ausgänge
	
	PINMODE0 = (PULLDOWN<<14)|(PULLDOWN<<18);						// Pulldown P0.7/9 SPI1 SCK und MOSI
	PINMODE1 = (NOPULL<<18)|(NOPULL<<20);								// No pull resistor P0.25/P0.26 - AD02/AD03	
	PINMODE3 = (NOPULL<<10)|(NOPULL<<18)|(NOPULL<<28)|(PULLDOWN<<0);	// No pull resistor P1.21-Testkopplung, P1.25-FSK, P1.30 - VBUS, BT Modul ein	  
	Init_Peripheral ();																	// Initialisiert Ports fuer Peripherie
}	

void Init_Peripheral (void)		// Initialisiert Ports fuer Peripherie
{															// Flash muss zuvor gelesen sein
	
 if (fp.ledspot) FIO0SET = EX12_1; 																		// Externe Spotlampe 12V-EXT1 einschalten
 else FIO0CLR = EX12_1;
 if ((fp.ex12==1) || (fp.ex12==2) || (fp.ex12==4)) FIO0SET = EX12_2;	// 12V extern=ein,Viasis Plus oder Viatext
 else FIO0CLR = EX12_2;																								// 12V extern=aus oder =Viasis Plus FT		
	
 if (fp.ex12==3) PINMODE7 = (PULLDOWN<<20);														// Pulldown PWM extern Ausgang für Plus FT	
}

void uswait (uint delay)			// µs Warten
{
 uint t1time=T1TC;						// Timer 1 Zählerstand
 while (T1TC-t1time<delay);	 	// Differenz bilden
}	

uint get_ad (uchar chanel)		// A/D Wert ermitteln
{															// Übergabe chanel - A/D Kanal
 uint wert;										// Hilfsvariable A/D Wert

 PCONP|=PCADC;								// A/D Wandler Versorgung einschalten

 ADCR=PDN|(2<<8)|(1<<chanel);	// Kanal auswählen, enable A/D Wandler, ADCLK = 24MHz/8/3 = 1 MHz

 ADCR|=(STRTNOW);							// Wandlung starten
 while (!(ADSTAT&(1<<chanel)));	// warte bis DONE des indiv. Kanals gesetzt
 wert=(*((volatile ulong *)((uint)&(ADDR0)+chanel*4))>>4)&0xFFF;	// A/D Kanal Datenregister auslesen
 ADCR=0;											// A/D Wandler wieder aus
 PCONP&=~PCADC;								// A/D Wandler Versorgung ausschalten
 return (wert);
}

uint get_brightness (void)		// Umgebungshelligkeit messen, A/D-Wert SFH213 Fotodiode  
{
 uint wert;
 uint pindef=PINSEL4;			// Portdefinition sichern 			

 if (PINSEL4&PWM6)				// Sind LED aktiv? 
	PINSEL4 &= ~(PWM6);			// Disable Versorgungsregler und MBI5039 outputs, trenne PWM -> OE\ hi durch pull-up	  
 
 FIO1CLR = MUX_A|MUX_B;		// Helligkeit über MUX IC39 an CPU A/D Eingang 
 	
 wert=get_ad(3);					// A/D Kanal 3 (AD0.3) auslesen 
 
 PINSEL4=pindef;					// Portdefinition wiederherstellen
 
 return (wert>>2);				// SFH213 A/D-Wert auf 1024 Byte normiert zurück 
}

void shift_to_LED (text * const sequence)	// LED Anzeigedaten an Treiberplatine senden
{															// Übergabe sequence - Zeiger auf Sendesequenz oder T_nil
															// Wenn sequence = T_nil Ausgabe Anzeigepuffer ledbuf 
								
 uchar i=5;										// Laufvariable
 bool bicolor=FALSE;
	
 if ((ledbuf[0]&(COL1|COL2))==(COL1|COL2)) bicolor=TRUE; // zweifarbige numerische Anzeige
 PWMMCR&=~PWMMR0I;									// Lösche Interrupt vorausgehende Zweifarbdarstellung	
 Init_SPI ();												// SPI initialisieren 		
 FIO2CLR = LE_TRB;									// LE low
	
 if (!(PINSEL4&PWM6))								// Sind LED aus?
 {	  	
	sendspi	(AUSVCL);									// Null Byte sende -> 3V3 ein, VCluster noch aus   

  FIO2SET = LE_TRB;									// Latch Schiebedaten 74HC595 
  while ((FIO2PIN & LE_TRB) == 0);	// Warte bis gesetzt
  FIO2CLR = LE_TRB;									// Schieberegister Out enable -> Treiber ein und Latch reset
	if (bicolor) Clear_SPI ();				// Disable SPI  
	osDelay (3); 											// Warte bis 3V3 durchgeschaltet und stabil
 }	// end if LED aus 
 
 if (*sequence!='\0') 								// Sendesequenz angegeben?
	 memcpy(ledbuf,sequence,5);					// Ja -> in Anzeigepuffer kopieren
 
 if (bicolor) PWMMCR|=PWMMR0I;				// zweifarbige num. Anzeige -Interrupt aktivieren	 
 else																	// einfarbige Anzeige
 {
	while (i--) sendspi	(ledbuf[i]);		// Daten/Anzeigepuffer über SPI versenden	

  FIO2SET = LE_TRB;										// Latch Schiebedaten 74HC595 
  while ((FIO2PIN & LE_TRB) == 0);		// Warte bis gesetzt 
  FIO2CLR = LE_TRB;										// Latch reset -> LED Treiber MBI5039 Datenübernahme    
 }	
 PINSEL4 |= PWM6; 										// Enable Taktung Versorgungsspannungsregler
 Clear_SPI ();												// Disable SPI
}	

void Ledaus (void)
{
 PINSEL4 &= ~(PWM6);								// Disable Anzeige PWM
 if (PCONP&PCPWM1)									// PWM1 ein?
  PWMMCR&=~PWMMR0I;									// Reset Farbwechsel Interrupt
 FIO2CLR = LE_TRB; 									// Reset Latch Treiber und Regler aus	
 Init_SPI ();												// SPI initialisieren
 sendspi(AUSVCL);										// 3V3 auf Treiber aus, Clusterspannungsregler aus	
 FIO2SET = LE_TRB;									// Latch Schiebedaten 74HC595 
 while ((FIO2PIN & LE_TRB) == 0);		// Warte bis gesetzt 
 FIO2CLR = LE_TRB;									// Latch reset -> LED Treiber MBI5039 Datenübernahme	
 Clear_SPI ();											// Disable SPI	   
}	

uint adj_hell (void)		// Kundenfaktor (0,1 und 2) und Abgleichwert in Helligkeit berücksichtigen 
{																
 uint hellwert;									
 uchar tastind=0;											// Index Helligkeits-Abgleichwerte
 
 if (farbcode&COL1) tastind=0; 				// Berechnet wird der Helligkeitswert für die
 else if (farbcode&COL2) tastind=1; 	// aktuell gültige farbcode Variable
 else if (farbcode&COL3) tastind=2;	
 else if (farbcode&COL5) tastind=3;	
	
 hellwert=fp.hell[tastind]; 						// Abgleichwert und 
 return ((hellwert*(5+fp.hoff*5))/10);	// Kundenfaktor (0,1 und 2) berücksichtigen
}

void set_pwm (uchar hell, uchar hellx)		// Setze externe und LED PWM 
{																					// Übergabe hell	-	Helligkeit LED 0 = variabel oder 1...200 * 1/2% 
																					// 					hellx - dto. externe PWM		
 uint pwm3,pwm6;													// Hilfsvariable Tastverhältnisberechnung	LED PWM Tastverhältnis in %
	
 if (!hell) pwm6=(adj_hell()*dimm)/100;		// Variable Helligkeit - Lichtsensor, Kundenfaktor und LED Farbe bestimmen Tastverhältnis
 else pwm6=hell;  												// sonst Vorgabe übernehmen	
 if (!hellx) 															// Externe PWM variable Helligkeit
	pwm3=fp.pwmex*dimm*(1+fp.hoff)/200;			// Lichtsensor und Kundenfaktor bestimmen Tastverhältnis
 else pwm3=hellx;   											// sonst Vorgabe übernehmen
 
 if (pwm6==0) pwm6=1;
 else if (pwm6>=RES_PWM) pwm6=RES_PWM-1;	// 1 bis 200 Tastverhältnis 0,5% bis 100%
 if (pwm3==0) pwm3=1;
 else if (pwm3>=RES_PWM) pwm3=RES_PWM-1;	// Korrektur wg. Kundenfaktor bis 150%	
										
 PWMMR6=RES_PWM-1-pwm6;											// LED Tastverhältnis/Helligkeit einstellen
 PWMMR3=pwm3;																// Externe PWM Tastverhältnis/Helligkeit einstellen
 PWMMR4=RES_PWM-1-pwm6;											// Versorgungregler Gegentaktsignal PWM-2 setzen
 if (pwm6>=(RES_PWM-1)) PWMMR5=RES_PWM;			// PWM Dauer Hi	= Versorgungregler permanent ein
 else PWMMR5=RES_PWM-1;											// Versorgungregler Gegentaktsignal reset	
 PWMMR0=RES_PWM-1; 													// Duty Cycle einstellen
 PWMLER=(1<<6)|(1<<5)|(1<<4)|(1<<3)|(1<<0);		// Enable PWM 0, 3, 4, 5 und 6 Latch
 if (!((PINSEL4&PWM6)||(PINSEL7&PWM3)))		// Kein aktives PWM-Signal								
 {	 
  PWMTC=PWMMR0-1;													// nächster count -> match	
  PWMTCR=(1<<3)|(1<<0);										// Enable Counter und PWM 
 }	 
 
 PINSEL7 |= PWM3;													// Enable externe PWM 	 	 
}	

void wait_uarts_empty (void)	// Warte bis alle Zeichen auf uarts versendet
{
 while (1)
  if ((U0->IER&THRE_INT_EN)==0) // Sendepuffer Interrupt UART0 deaktiviert?
   if (U0->LSR&THRE)						// UART0 FIFO leer?
    //if (U1LSR&THRE)						// UART1 FIFO leer?
	 break; 
}

void DeepSleep (void)										// Bringe CPU in Deep sleep mode
{
 i_reset=SLEEP_RES;											// Setze Resetursache		
 if (fp.pwm) SCB_SCR|=(1<<2);						// Deep sleep Bit setzen
 PLL0CON &= ~(1<<1);										// Disconnect PLL0, siehe errata sheet
 PLL0FEED = 0xAA;										
 PLL0FEED = 0x55;	
 while ((PLL0STAT & (1<<25)) != 0x00); 	// Wait for PLL0	disconnected
 PLL0CON &= ~(1<<0); 										// Turn off PLL0
 PLL0FEED = 0xAA;										
 PLL0FEED = 0x55;
 while ((PLL0STAT & (1<<24)) != 0x00); 	// Wait for PLL0 shut down
 CCLKCFG=0x01;	 												// Main oscillator undivided
 CLKSRCSEL=0;														// IRC oscillator clock source	 
 PCON=0x00;							 								// Instruct deep sleep
 __WFE();																// Enter deep sleep	
 ResetWDT ();														// Interrupt resume - Watchdog Timer reset	
 i_reset=0; 														// Interrupt Resume daher nullen		
 SystemInit();													// PLL neu starten
 InitWatchdog(LONG_WD_32);							// Watchdog Timeout 32 Sekunden setzen 
 SystemCoreClockUpdate ();							// Setze SystemCoreClock Information	
}

	





