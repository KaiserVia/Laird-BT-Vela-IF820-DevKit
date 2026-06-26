//-----------------------------------------------------------------------------
//  FILE: main.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Hauptprogramm
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H2
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   19.12.2014
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:	22.04.2015:	Hardware version H1 -> H2
//									13.07.2015: Hardware version H2 -> H3
//              	
//-----------------------------------------------------------------------------

#include "cmsis_os.h"
#include "rtc.h"
#include "sio.h"
#include "gsmio.h"
#include "gpsio.h"
#include "btio.h"
#include "sictxt.h"
#include "flash.h"
#include "USB_tools.h"
#include "i2cm.h"
#include "ramcode.h"
#include "libtool.h"
#include "measure.h"
#include "mqtt.h"
#include "dtc_codes.h"


 
int main (void)
{	
 uchar i=0;															// Laufvariable	
 uchar syserror=0;											// Sytemfehler für Smiley-Startanzeige	
 int result;	
		
 SystemCoreClockUpdate ();							// Setze SystemCoreClock Information
 if (RSID&(POR|EXTR|BODR)) osKernelInitialize (); 		// initialize RTOS Kernel bei Power-On
 osKernelStart();																			// Starte RTOS Kernel
	
 if ((i_reset!=SLEEP_RES)&(i_reset!=FIRM_UP_RES)) 		// Kein Firmware update oder Sleep Power Down Reset?
 {	
  if (!(RSID&WDTR)|(i_reset==BOOT_RES))	// Nicht bei Watchdog Reset ausser Reboot
  {																			// Wichtige Systemvariable initialisieren
	 fp.sprache=0; 												// Sprachsteuerung initialisieren	
	 fp.i2cdev=0;														// reset I2C Bus Devices  
	 if (i_reset!=BOOT_RES)	connect=UART0;	// Terminalausgabe aktivieren 
	 concpy=connect;												// Sicherungskopie von connect	
	 dimm=RES_PWM/2;											// LED Dimmfaktor 50%
	 pset=0;															// aktiver Parametersatz
   transferzeit=0;											// Reset Sperrzeit Sommer- auf Winterzeit Umstellung
	 gsmpower=0;													// GSM Modem ist aus
	}		
	if (i_reset!=BOOT_RES) i_reset=0;			// Watchdog reset Ursache Reset zurück setzen 	
  gsmcall=0;														// Reset dial in Verbindung 
	timeupdate=1;													// Zeitabhängige Aufgaben prüfen
	mqtt_md_simu=0;												// MQTT Messdatensimulation aus	 
	mqtt_com=0;	 													// MQTT Kommunikation reset
	mqtt_con_err=0;												// MQTT Verbindungsfehler reset
  thread_comchange=0;										// Reset Schnittstellenwechsel während GSM/MQTT Verbindungsthread
  simpinset=0;													// Pinnummer reset	
	while (i<3) certs[i++][0]=0;					// Zertifikatsliste nullen 
 }
	
 Init_Ports();													// Initialisiere Portpins 
 InitTimer ();													// Initialisiere Timer
 Init_UART0 ();													// Initialisiert Schnittstelle UART0 (Terminal)
 InitRTC ();														// Initialisierung der Echtzeituhr		
 InitWatchdog(LONG_WD_32);							// langes Watchdog Interval auf 32 Sekunden setzen		
 
 redirect_char_Out(putb);		// Zeichenausgabe an UART(s) leiten	
 																									
 if (!(RSID&WDTR)|(i_reset==BOOT_RES)) // Kein Watchdog reset ausser Reboot?
 {													
	osDelay(250); 
	 
	syserror|=InitSPIFlash();	// Initialisiere SPI und Flash   
	if (InitParameter(0)==1) 	// Parameter Power On und ggf. Werkinitialisierung
  {	
   putmstr(L_winit); 				// Meldung Werkinitialisierung
	 syserror++;							// Im Normalbetrieb sollte keine Werkinitialisierung vorkommen	
	}  
	Init_Peripheral ();				// Initialisiert Ports fuer Peripherie 
 }	
 
 if (Init_I2C()== ARM_DRIVER_OK) fp.i2cdev|=I2CB0;		// Initialisierung I2C Interface erfolgreich
 else	{ puterror (DTC_MA_I2C_BUS, -1);	syserror++; }		// oder nicht 
 
 	 
 if ((i_reset!=SLEEP_RES)&(i_reset!=FIRM_UP_RES)) 		// Kein Firmware update oder Sleep Power Down Reset? 
 { 
  if (fp.i2cdev&I2CB0) {																// I2C Bus erfolgreich initialisiert?			
   if (InitI2C_Devices() < 0)														// I2C Bauteile inititialisieren, fehlt eins? 
   {
    if (((fp.i2cdev&(IC37_b|IC54_b))!=(IC37_b|IC54_b)) 	// PCA Expander Analogsteuerung oder Schnittstellen-Link fehlt							
		 || (!(fp.i2cdev&IC58_b&&(fp.ex12>=2))))						// PCA Expander I/O für Plus, FT oder Viatext fehlt
		 puterror (DTC_MA_I2C_DEVICE, fp.i2cdev);
    if ((fp.i2cdev&IC35_b)!=IC35_b) puterror (DTC_MA_I2C_DPP, -1);					// DPP fehlt
	  if (!(fp.i2cdev&ICLED_b) && fp.ledspot) puterror (DTC_MA_TCA6507, -1);	// Fehler externe LED Spotlampe
		syserror++;	
  } } // end if I2C Bus erfolgreich initialisiert   
	
	if (RSID&(POR|BODR))								// Power On oder Brownout Reset?
	{		 
	 FIO2CLR =	DIS_TRB | G_SHIFT;			// EN\ 3V3 Anzeige - Treiber	
	 LED_188 (); 												// LED Einschaltanzeige "188" 
	 FIO2CLR = LE_TRB|DIS_TRB|G_SHIFT;	// Latch Enable LED Treiber, 3V3 Treiber aus, 74HC595 Ausgänge aktiv
   FIO2SET = PWM_TRB;									// PWM Pin Treiber HI	
	}	
	 
  if (flashsize!=0)																			// Flash vorhanden?
  {    
	 flash_erase (PARAMETERPAGE, PARAMBLOCKS*pagepk);			// Parameterseiten im Flash löschen	
	 flash_erase (fp.pro_page, 1);												// Aktuelle Protokollseite im Flash löschen
   flash_erase (fp.md_page, 1);													// Aktuelle Messdatenseite im Flash löschen	
  }
 } // end if Kein Firmware update oder Sleep Power Down Reset	
 
 comstate=get_com_status();															// Status Datenschnittstellen einlesen
 Init_Ext_Intr();																				// Initialisiere externe Interrupts (ohne Freigabe)
 
 if (i_reset!=SLEEP_RES) 										// Kein deep sleep watchdog reset?
 {	 
  if (i_reset!=FIRM_UP_RES)									// Kein Firmware update und kein power down?
	{		   
   if ((RSID&WDTR)&&(i_reset!=BOOT_RES))		// Watchdog Reset ausser Reboot?
	 {	
 	  if (comstate&RS232_LINK) connect=UART0;	// RS232 abgeschlossen?
		else connect=0;													// sonst alle Verbindungen deaktivieren		
    concpy=connect;		 
		if (fp.gsm)															// GSM installiert?
		{		
		 Init_GSM_ch (GSM_BAUD); 																// Kommunikationskanal öffnen
 	   if (fp.servertyp==MQTT) MQTT_fast_disconnect (0);	// reset ggf. MQTT und Funkmodem
		 else gsm_power(0);																	// sonst Funkmodem nur aus
		}	
		protocol(WATCHDOG_RESET); 			// Watchdog reset protokollieren
   }		
   else if (RSID&POR) 	// Power on reset? (jetzt exklusiv ohne Reboot)
	 {	 	 
    protocol(POWER_ON);						// Power on reset protokollieren
    poweron_min=MIN;											 
		if (comstate&RS232_LINK) startmessage=1;		// Einschaltmeldung	senden 	
	  osDelay(50);
		
    interfaces=RS232_LINK;					// RS232 Terminalanschluss ist immer vorhanden	
		
	  if (fp.usb) interfaces|=USB_LINK;		// USB definiert?	Ja, USB registrieren			 
		
    if (fp.btmodem)	{								// BT definiert?
		 if (test_BT())	interfaces|=BT_LINK;		// Modem antwortet?		 
		 else {                                     // BT antwortet nicht (z. B. geaenderte Zielbaud)
		   fp.btmodem=0; init_bluetooth();         // -> neu erkennen (Zwei-Baud) + ggf. auf IF820_BAUD umstellen
		   if (fp.btmodem) interfaces|=BT_LINK; else syserror++; } }	
		else if (fp.serno[0]) init_bluetooth();	// BT nicht konfiguriert aber Seriennummer vorhanden? -> automatisch konfigurieren
		 
		if (fp.gsm)											// GSM installiert?
		{	
     if (test_gsm(1)>0)							// Antwortet GSM Modem?
		 {	 
			interfaces|=GSM_LINK;					// Registrieren	 
			if (fp.gps)										// GPS installiert?
			{
			 result=test_gps(0);					// GPS Modul L70/LC76F antwortet?			 	
       if (result>0)								// Erfolg?
			 {	 
				MUX_BT_Select;							// MUX Bluetooth Leitungsauswahl    
        if (RSID&POR)								// Nur bei Power On
				{	
				 gpsintv=fp.gpsintv; 				// Kopiere Einstellung GPS Positionsbestimmungszeit 
         gpsfix=60*fp.gpsintv;	 		// Bestimme Position nach Power On 						
				}	
				interfaces|=GPS_LINK;				// Registrieren 
			 } 
			 gps_power(0); 								// GPS wieder ausschalten
			 gps_pending=0;								// Reset GPS Positionsbestimmung
		  } 
			gsm_power(0);							 		// GSM erst mal wieder abschalten
		 } // end if gsm antwortet
		 else syserror++;	
	  } // end GSM installiert 		
		
		if (syserror) num_to_LED (501,0);							// Sad smiley nach LED Anzeigepuffer
		else num_to_LED (503,0);											// Happy smiley nach LED Anzeigepuffer
		show_led(2000,0);															// Dimmung und Led Schaltregler einschalten
		
		if (!(comstate&RS232_LINK) && !fp.vspcam && !((fp.ex12==2)||(fp.ex12==4))) // Keine RS232 Verbindung, kein Plus und keine speedcam
		{	
		 connect&=~UART0;								// Ausgabeflag RS232 reset wg. cloud Betrieb
		 concpy=connect;								// connect Kopie aktualisieren	
		}	
		comstate^=RS232_LINK;						// RS232 Terminalzustand inverten, siehe communication_change ()
	 } // end if power on reset 
	 else if (i_reset==BOOT_RES)			// Reboot?
	 {
   }		 
	}	
	else														// Firmware update
	{  
   protocol(FIRMWARE_UPDATE); 		// Firmware update protokollieren 
	 clean_cpy (T_version, fp.pVersion, sizeof(fp.pVersion));	// Neue Programmversion nach Parameterblock	 	
   startmessage=1;								// Einschaltmeldung	senden
	 if (!(comstate&BT_LINK)) 			// Bluetooth Verbindung?	 
	 {	 
		Init_BT_ch (BT_BAUD);										// Konfiguriere uart1 und setze MUX Kanal auf BT modem
		bt_time=BT_TIMEOUT;						// Bluetooth Verbindungszeit Timeout 6 Minuten  
	 }	 
	 else if (gsmcall!=0) 					// GSM dial-in Verbindung besteht?
	 {
		gsmpower=1;										// Modem ist eingeschaltet
		simpinset=1;									// Pinnummer ist gesetzt
		gsm_power (2); 								// Uart1/MUX initialisieren und sleep disable			   
	 }
	 flash_erase (fp.md_page,1);		// Muss wg. reset aller Messdatenzeiger
  } // end if firmware update	
	
	wait_min=Def_wait;				// Setze 3 Warteminuten für nächste GPS, Email, SMS Aktivität XXX
	mailtosend=0;							// reset Email Versandaufträge
	smstosend=0;							// reset SMS Versandaufträge
	comchange=1;							// Initiale Prüfung Schnittstellen
  mqtt_md_simu=0;						// MQTT Messdatensimulation aus	
	mqtt_com=0;	 													// MQTT Kommunikation reset
  mailfail=0;								// reset Aufwärtszähler gescheiterte Mail
  smsfail=0;								// reset Aufwärtszähler gescheiterte SMS
	wait_sms=0; 							// reset Warteminuten SMS
  wait_mail=0;							// reset Warteminuten Email 	 
	LastErrorEvent=0;					// Letztes protokolliertes Fehlereignis
	if (fp.vmm<5) t_amp_en=20;	// Messverzögerung bei kleinsten gemessenen Geschwindigkeiten < 5 km/h vergrößern
	else t_amp_en=20;
 } // end if kein deep sleep reset			
 
 if (!(RSID&(POR|WDTR|EXTR))) protocol(BROWNOUT_ERROR);	// Brownout reset
  
 RSID|=RSID;									// Reset Identifikationsregister zurück setzen
 
 if (flashsize!=0)						// Flash vorhanden?
	NVIC_EnableIRQ(EINT0_IRQn);	// Enable EINT0 Power Down Interrupt
 NVIC_EnableIRQ(EINT1_IRQn);	// Enable EINT1 Expander Interrupt
 i_reset=0;										// reset Zusatzinfo Reset Quelle 
 FIO1CLR =	USB_PPWR;					// USB Device Versorgung ein
 FIO1SET = EN_ANA;						// Analogversorgung/Transceiver ein 
 Ledaus ();										// Led "188" Abschaltung
 runcycle ();									// Messzyklenbetrieb
}	

