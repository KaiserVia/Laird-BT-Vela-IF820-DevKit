//-----------------------------------------------------------------------------
//  FILE: measure.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Hauptprogramm
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H2
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   24.02.2015
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:    	24.02.2015: File creation
//											22.04.2015:	Hardware version H1 -> H2
//											13.07.2015 Hardware version H2 -> H3
//-----------------------------------------------------------------------------

#include "hard.h"
#include "flash.h"
#include "sio.h"
#include "btio.h"
#include "gsmio.h"
#include "gpsio.h"
#include "mqtt.h"
#include "rtc.h"
#include "sicom.h"
#include "i2cm.h"
#include "sictxt.h"
#include "libtool.h"
#include "USB_tools.h"
#include "core_cm3.h"

osThreadDef(USB_HostService, osPriorityNormal, 1, 256); 	// define USB_HostService as thread function
osThreadDef(GSM_Registration, osPriorityNormal, 1, 256);	// GSM Modem Einschaltung und Registrierung als thread function
osThreadDef(MQTT_Connect, osPriorityNormal, 1, 512);			// MQTT Verbindungsaufbau als thread function

int measure (uchar speichern)	// Geschwindigkeitsmessung
{
 uchar tbi=0;									// Bearbeitungsindex Messpuffer IF und Phase
 uint vsum=0,psum=0;					// Geschwindigkeits- und Phasensumme
 uint pwinkel;								// Phasenwinkel
 uint vfehler=0;							// Mess- oder Beschleunigungsfehler
 uint speed=0;								// Geschwindigkeit
 uchar kommt=0;								// Ankommende Richtung	
	
 transceiver(2);							// Transceiver und Verstärker einschalten
 set_measure_constants();			// Messkonstanten bestimmen
	
 En_Capture;									// Timer 2 Capture interrupt enable, D-IF-A 
 NVIC_EnableIRQ(EINT2_IRQn);	// Enable EINT2 Detektionsfehler Interrupt	

 t_mess=t_mess_max;						// Timer Messzeit laden 	
 tmi=0;												// reset Messindex

 while (((bxi==rxi)||osActiveThread)&&(!comchange)) 	// Solange kein Zeichen von Terminal 
 {																										// Nur Abbruch bei Zeichenempfang im MainThread
	if ((connect&MQTT_LINK)&&(mqtt_com>1)) break;				// Abbruch bei MQTT Nachricht empfangen
	if (tmi==0) 
  {
   tbi=0;					// Bearbeitungsindex reset
   vsum=psum=0;		// Reset Summen
  }
	else if (tmi>tbi+1)	// Neuer Messwert?
  {
   psum+=tm.cIF[tbi+1]-tm.cIP[tbi+1];			// Phasensumme
   tm.IF[tbi]=tm.cIF[tbi+1]-tm.cIF[tbi];	// Periodendauer bilden
   vsum+=tm.IF[tbi++];										// Periodendauersumme         
  }
	
	if (tbi>=LenTm-1)				// Messpuffer gefüllt
  {
   NVIC_DisableIRQ(EINT2_IRQn);	// Disable EINT2 Detektionsfehler Interrupt 
	 Dis_CapCnt;									// Disable Timer 2 Capture Interrupt	
		
	 pwinkel=psum*360/vsum;		// Phasenwinkel berechnen
	 if (pwinkel>PHASENMINIMUM && pwinkel<PHASENMAXIMUM && tm.IF[LenTm/2]>0)
   {
    DirSort (LenTm-1);			// Sortiert Periodendauern
	  vfehler=50*(tm.IF[2*LenTm/3]-tm.IF[LenTm/3])/tm.IF[LenTm/2];
	  if (vfehler<=MAXFEHLER) 
	  {
	   speed10=v_const/tm.IF[LenTm/2];     	 
	   speed=speed10/10;	
	   break;
	  }  // end Maxfehler
   } // end Phasenwinkel ok   				
	 tmi=0; 
   NVIC_EnableIRQ(EINT2_IRQn);	// Enable EINT2 Detektionsfehler Interrupt	
	 En_Capture;									// Timer 2 Capture interrupt enable, D-IF-A 
	}	
	else if (tbi==(LenTm-1)/2)	// Puffer halb voll 
  { if (FIO1PIN&DIR) kommt=1;	// Richtung prüfen und merken
    else kommt=0;	
	} 
  
	if (t_mess==0) break; 						// Absolute Messzeitüberwachung
	if((int)(LenTm-(((t_mess+DPWR+DTXFEN+t_amp_en)*LenTm)/t_mess_max))>tmi)   
   break; // Dynamische Überwachung Anzahl gemessener Dopplerperioden
 }	 // end while
 
 Dis_CapCnt;									// Disable Timer 2 Capture Interrupt
 NVIC_DisableIRQ(EINT2_IRQn);	// Disable EINT2 Detektionsfehler Interrupt
 transceiver (aus);						// Transceiver ausschalten
 
 // if (speed>50) { putb_t ('w'); while(1); } // Watchdog reset
 
 if ((speed>=fp.vmm)&&(speed<=Defmaxspeed))	// Geschwindigkeit im Messbereich
 {
  if (!kommt) 						// Richtung in Geschwindigkeitswert berücksichtigen
  {
   speed=-speed;
   speed10=-speed10;
  }
  if (kommt||fp.bdir)				// Ankommend oder bidirektionale Erfassung
  {
   if (speichern) store(speed10,0);	// Geschwindigkeitswert im Flash speichern
   return (speed);									// Bei ankommender Fahrtrichtung Wert zur Anzeige zurück
  }
 }  
 return (0);				// Geschwindigkeit ausserhalb Messbereich nullen 
}	

void runcycle (void)				// Messzyklenbetrieb
{
 uchar cycle=0;							// Zyklenzähler (fp.mcycle/BASECYCLE) 	
 uchar acycle=0;						// Zähler Anzeigezyklen
 uchar scycle=0;						// Zähler Symbolanzeige
 uchar aswitch=0;						// Flag Schalter gesetzt	
 uint aspeed=0;							// Hilfsvariable angezeigte Geschwindigkeit
 uint asymbol=0;						// LED Anzeigesymbol, 0 keines anzuzeigen	
 uchar farbe=1;							// Hilfsvariable LED Farbe	
 uint anzeige=0;						// Numerische Geschwindigkeitswerte oder Symbolcodes, aktuelle und vorhergehende
 int speed=0, refresh=0;		// Messergebniss und Anzeigeauffrischung
 int cyclerest;							// Restzeit des 250ms Basiszyklus
	
 FIO1SET = EN_ANA;						// Analogversorgung/Transceiver ein
 get_dimmung ();							// Dimmfaktor bestimmen
 set_pwm(0,0);								// Externe und interne PWM setzen 	
 osDelay (50);								// warte bis Spannung ok

 if (fp.vspcam && (fp.ex12!=2) ) online=1;	// viaspeedcam und kein viasis PLUS dann sofort in den Online Messmodus
 																						// Beim viasis PLUS Aktivierung durch 'T' Zeitabfrage vom Matrix Contoller		
 else online=0;	
	
 while (1)	// forever
 {	
  ResetWDTm ();								// Watchdog Timer reset	 
	 
  if (!simulation)						// Kein Simulationsmodus?
  {
	 //timeupdate=0;						// timeupdate wird jetzt in Aufgabenzeit auf Null gesetzt	
		
	 if (minute || timeupdate)	// Minuteninterrupt von RTC oder Parameteränderung?
   {  
    if (minute) 							// Minuteninterrupt von RTC
		{ 	
		 minute=0;										// reset Minutenflag	
		 if (wait_min) wait_min--;		// Warteminuten GPS, Email, SMS Aktivität dekrementieren
		 if	(wait_mail) wait_mail--;	// Warteminuten Email Versand dekrementieren
		 if (wait_sms) wait_sms--;		// Warteminuten SMS Versand dekrementieren	
		 gpsfix++;										// Minutenzähler für GPS Positionsbestimmung erhöhen	
		 if (!transferzeit)	zeitumstellung();		// Prüfe und führe ggf. Sommer-/Winterzeit Umstellung durch	
		 else transferzeit--;										// Zähle 60 Minuten Prüfsperrzeit herunter
		 if ((PINMODE3&PULLDOWN)!=PULLDOWN) PINMODE3|=PULLDOWN;	// Bluetooth abgeschaltet -> einschalten
		 if (connect&BT_LINK) 							// Bluetooth Verbindung und Funkmodem?
		 {				
			if (!bt_time) 								// Verbindungszeit  Bluetooth (BT_TIMEOUT) abgelaufen?
			{			
			 putln(T_btdis);							// Text "Bluetooth getrennt ...
			 if (fp.btmodem==IF820) { bt_cmdmode(); osDelay(100); bt_release(); }	// IF820: SPP physisch trennen (CYSPP-Puls)
			 uart1_release(MUX_BT);			// Release UART1
			 connect&=~(UART1|BT_LINK);		// Verbindung virtuell trennen
			 PINMODE3&=~PULLDOWN;					// PULL-UP - Bluetooth Modul abschalten		
			}	
			else bt_time--;								// Verbindungszeitzähler Bluetooth herunter zählen 
		 }			 
		 if (mqtt_state) t_mqtt_status++;				// Erhöhe Minutenzähler für MQTT Status Versand
		 if (!MIN) fp.hcount++;					// Wenn MIN=0 Betriebsstundenzähler erhöhen	 
		 if (mqtt_md_simu&&(!osActiveThread)) md_simulation ((MIN&0x1F)+1, 0);		// Erzeuge Messdaten im Speicher		
		}  
    tasks=Aufgabenzeit();			// Prüfe ob Mess-/GSM- oder GPS-Einschaltzeit 		

    if (tasks&BOOTTASK) reboot();		// Starte CPU neu  	
	
	  if (tasks&GSMTASK) 					// GSM Einschaltzeit, offline Email, SMS oder MQTT-Datenversand?
    {		 	
		 if (!(connect&(UART0|MQTT_LINK|USB_LINK|BT_LINK|GSM_LINK)) 				// keine RS232, USB, BT, MQTT oder GSM Verbindung
			 || ((mqtt_debug|fp.vspcam) && (!(connect&(BT_LINK|MQTT_LINK|GSM_LINK)))))	// im MQTT Debug / speedcam mode und (noch) keine MQTT, BT oder GSM Verbindung
		 {  		
			//FIO0CLR = CSFLASH2;	
		  if (!wait_min)						// keine Warteminuten für GPS, Email, SMS Aktivität?						
		  {
			 if (simpinset) 									// Pin gesetzt?
			 {
				if (fp.vspcam) online=1; 											// Aktiviert wieder Online nach GSM-Registrierung/Schnittstellwechsel 
				if (smstosend&&!wait_sms&&!osActiveThread)		// SMS zu senden, keine SMS Wartezeit und kein laufender MQTT Connect?
				{	
         if (connect&MQTT_LINK) mqtt_state=CLOSE;	// Disconnect MQTT-Server und Verbindungsabbau					
				 else sendsms(0);													// Sende SMS			 
				} 
				else if (fp.servertyp==MQTT)				// MQTT Server?
				{ 
				 if (!(connect&MQTT_LINK))					// Nicht verbunden?
				 {	
					if (!(osActiveThread&GSM_REGISTRATION)&&(mqtt_state==DISCONNECT))	// Nicht während GSM Registrierung starten
					{
					 if (!mqtt_thread_id) mqtt_thread_id=osThreadCreate (osThread (MQTT_Connect), NULL);	// Start MQTT Verbindungsthread					 
					 osActiveThread|=MQTT_CONNECTION;	// MQTT Verbindungsthread aktiviert	
				   osSignalSet (mqtt_thread_id, 3); 	// Starte Thread MQTT Verbindung	
			    }	
				 }	
				}
				else if ((fp.servertyp==SMTP)&&(mailtosend&&!wait_mail))	// Mailserver und etwas zu senden?
				{	
				 clear_all();											// Led und Schalter aus
				 scycle=0;												// Wechselzähler reset
				 speed=0;													// reset speed	
				 if (mailtosend&(TESTMAIL|ALARMMAIL|DATAMAIL)) sendmail(6); 		// Email zu versenden?
				}		
			 } // end if SIM PIN gesetzt 		
       else 
			 {	 
			  if (!fp.pinerr) 										// 	Kein Pinnummer Fehler?
			  {	 				 					 	
				 if (!(osActiveThread&MQTT_CONNECTION))	// Bei aktivem MQTT Verbindungsaufbau nicht starten
				 {	
					if (osActiveThread&GSM_REGISTRATION) 																										// Ist Thread noch aktiv = nicht durchgelaufen?
					{ if (!osThreadTerminate(gsmp_thread_id)) gsmp_thread_id=0; }														// Beende Thread
					if (!gsmp_thread_id) gsmp_thread_id=osThreadCreate (osThread(GSM_Registration), NULL);	// Thread GSM Registrierung starten  				
					osActiveThread|=GSM_REGISTRATION;			// Registrierungs Thread ist gestartet	

				  if (mqtt_debug) putstr (T_gsmpower);		
				  osSignalSet (gsmp_thread_id, 2); 	// Starte Thread GSM Registrierung
			   }	
			  }	 // end if Pinnummer Fehler	
				else protocol (SIM_ERROR);						// Pinfehler protokollieren
			 }	
		  } // end if keine Warteminute					
     } // end if BT, MQTT, GSM nicht verbunden
		 else if ((connect&MQTT_LINK)&&(mqtt_state>=CONNECTED))		// MQTT Server und Status verbunden
		  test_mqtt_com_state	();																	// Prüfe MQTTS Nachrichtenstatus
		 else if ((!mqtt_debug&gsmpower)&&!osActiveThread) gsm_power(0);							// -> Modem ausschalten
		 //FIO0SET = CSFLASH2; 
    } // end if GSM Einschaltzeit oder Email
		else if (gsmpower&&!osActiveThread)						// Keine GSM Aufgabe oder Einschaltzeit und GSM Modem ein? 
		{
     if (connect&MQTT_LINK)
		 {		
		  if ((mqtt_state!=DATASEND)&&(mqtt_state!=DATACK))		// Offline Datenversand?	
			 mqtt_state=CLOSE;																	// Schließe MQTT Verbindung
		 }	 
	   else if (!gsmcall) gsm_power(0);							// Keine GSM Einwahlverbindung und kein MQTT Link	-> Modem ausschalten
		 if (fp.vspcam) online=1; 											// Aktiviert wieder Online nach Schnittstellwechsel	
		}
		
		if (tasks&GPSTASK) 								// Zeit für GPS Positionsbestimmung?
		 if (!wait_min&&!osActiveThread)	// keine Warteminuten für GPS, Email, SMS Aktivität?	
			get_geopos ();									// Ermittle GPS position 		 
   }	// end if minute oder timeupdate		
	} // end if kein Simulationsmodus 
 
	 
  if (!USB_configured() && (interfaces&USB_LINK))		// USB noch nicht konfiguriert aber registriert?
  {	
	 if (FIO1PIN&VBUS)					// USB Host angeschlossen?
	 {	
	  FIO1SET =	USB_PPWR;								// USB Device Versorgung aus
		if (!usbh_thread_id)							// Thread noch nicht aufgebaut?
		 usbh_thread_id=osThreadCreate (osThread (USB_HostService), NULL);	// Start USB Host Thread
		if (usbh_thread_id==NULL)	protocol(USB_ERROR);											// Start gescheitert protokollieren
		else osSignalSet (usbh_thread_id, 1); 										// Signalisiere Verbindung an USB Host thread
		osDelay (250);  
		if ((connect&MQTT_LINK)&& !mqtt_debug)  mqtt_state=CLOSE;		// Schließe ggf. MQTT Verbindung 
		if (fp.gps<2) wait_min=Def_wait;														// Setze 3 Warteminuten bei GPS Multiplex 
	 }	
	 else if (FIO0PIN&DPLUS) 		// USB Flash Disk? 
   {  
		Ledaus(); 																	// Led abschalten
		if (connect&MQTT_LINK) 
		{	
		 MQTT_fast_disconnect (0); 									// MQTT Serververbindung schließen
		 clear_comchange ();	 											// Verbindungsänderung GSM Modul abfangen	
		}	
	  USB_Client_service ();											// USB flash disc Bearbeitung endet mit Reset	 
	 }
  } // end USB (noch) nicht konfiguriert?			
	
	if (!osActiveThread)													// kein Verbindungsthread?
	{	
	 if (comchange || thread_comchange) 					// Änderung Kommunikationsstatus
	  communication_change ();										// Bearbeitung Schnittstellenwechsel	
  }	

	if (connect&MQTT_LINK)												// besteht MQTT Verbindung?
	{	
	 if (mqtt_fail) 															// Fehler Nachrichtenempfang	
	 {	
		MQTT_fast_disconnect (0); 	// MQTT Serververbindung schließen, Modem aus
		mqtt_con_err=50; 						// MQTT Fehler Nachrichtenempfang	
		wait_min=Def_wait; 
		if (mqtt_debug)
    {			
		 putstr(T_m_fail);													// MQTT fail Nachricht						
		 sendbuf ((char *) mqbf, 32);			
		 newline();	
		}	
	  wait_min=1;																	//	Verbinde nach 1 Minute wieder
	 }	
	 else if (mqtt_com>1)	eval_mqtt_message ();		// Nachrichtenempfang vom MQTTS?
	 else if (!mqtt_manual) mqtt_state_incr (); 	// Verbindungsstatus prüfen und Status/Daten ggf. senden
	}	
	
	if (mqtt_con_err)			// MQTT Verbindungsfehler protokollieren
	{
 	 if (fp.mqtt_pdbq)																																			// Erweiterte MQTT Debug Protokollmeldungen aktiviert?		
	  if (LastErrorEvent!=(MQTT_ERROR+mqtt_con_err)) protocol (MQTT_ERROR+mqtt_con_err);		// Protokolliere MQTT Verbindungsfehler
	 mqtt_con_err=0;		
	}	
	
	if (!osActiveThread)								// Keine Thread mit Zeichenbearbeitung aktiv?
	 if (bxi!=rxi) {										// Zeichen von Schnittstelle?
		mainmenu(rbuf[++bxi]); 						// Aufruf Hauptmenü/Zeichenbearbeitung	
		if (simulation) 									// Simulation?
		{ cycle=0;												// starte mit Messzyklus
		  if (simulation==2) 							// Simulationsende
			{ acycle=0; simulation=0; } } }	// reset Anzeigezyklus und reset Simulationszustand
	
	if (menrep)	mainmenu('0');					// Menüwiederholung
  		 	
	if ((fp.i2cdev&IC37_b) || simulation)	// Expander Messsteuerung installiert oder Simulation?
  {
	 cycletime=0;													// reset cycletime	
	 if (cycle==0) 												// Messzyklus? 
   {
	  get_dimmung ();											// Dimmfaktor bestimmen
		set_pwm(0,0);												// Externe und interne PWM setzen 
	  if (simulation) 										// Anzeige- und Schaltersimulation?
		{ refresh=vtst; speed10=10*vtst; }	// Einfache und zehnfache Geschwindigkeit zuweisen
	  else if ((tasks&MESSTASK)||online) 	// Messaufgabe oder Online Messmodus?	 	  	  
	   refresh=measure(1); 				// Geschwindigkeit messen	  			       
	  else refresh=0;							// Anzeige nicht auffrischen	 
   } // end Messzyklus	
	 else if (cycle==1)						// Anzeigeauffrischzyklus? 
   {
		if (refresh)								// Geschwindigkeit gemessen?
	  {
	   speed=refresh;							// Geschwindigkeit übernehmen 
		 if ((speed>0)&& (fp.led[pset])) 						// Geschwindigkeit > 0 gemessen und Anzeige aktiv?
	   {	
			aspeed=display_mode_speed (pset);					// Prüfe Schwellen und Modi der Led Anzeige	
			if (fp.swgrp) aswitch=setswitches(speed);	// Schalter setzen, wenn definiert	   
	    else aswitch=0; 
			if (fp.symbol) asymbol=ledsymbol(speed);	// LED Symbole definiert? Ja, Schaltschwellen prüfen
			else asymbol=0; 
			if (aspeed || asymbol || aswitch)					// Geschwindigkeits- oder Symbolanzeige, Schalter gesetzt?	 
			{		 
				acycle=1+fp.acycle/BASECYCLE;				// Led freigegeben? Ja, Anzeigezyklen setzen 		
				scycle=0;   				
			}	        							   			
		 } // end speed>0	 
		 if (online) // Online Messmodus
		 {
	    put_signed_float (speed10, 10, 0, fp.nk); // Geschwindigkeit mit Vorzeichen	   
	    newline();	   
	   }	
		}  // end refresh			
		refresh=0;		// reset Anzeige auffrischen
	 }	// end Anzeigeauffrischzyklus
	 
	 if (acycle)    					// Anzeige aktivieren?
   {
    acycle--;								// Anzeigezyklen dekrementieren	 
		if (aspeed) 
		{
		 anzeige=aspeed;														// Numerischen Wert übergeben
	   if (speed>0) farbe=farbe_numLED (speed);		// LED Farbe der numerischen Anzeige festlegen
		}
		else anzeige=0;
		
		scycle++;																		// Zähler Symbolanzeige
		if (scycle>2*fp.scycle/BASECYCLE) scycle=1;	// Wechselzyklus reset
		if (scycle>fp.scycle/BASECYCLE)							// Anzeigewechsel?
		 if (asymbol>0) anzeige=asymbol;						// Bei Symbolanzeige -> Symbolfont übergeben
		
		if (anzeige)	  // Geschwindigkeits- oder Symbolanzeige?
		{		 
			num_to_LED (anzeige,farbe);										// Anzeigecode nach LED Anzeigepuffer 			 
			if ((speed>=fp.vblk[pset])&&((acycle%2)==1))	// Geschwindigkeit >= Blinkschwelle 
			 Ledaus ();																		// Led abschalten
 	    else show_led(-1,0);													// Dimmung und Led Schaltregler einschalten
		} // end if anzeige
		
	  if (acycle==0) // Anzeigezyklus beendet?
	  {	
	   clear_all();																	// Led und Schalter aus
		 if (fp.pwm>=2) PINSEL7&=~PWM3;								// Plus oder Plus FT externe PWM von Pin trennen	
	   speed=0;						// reset speed	
	  }
   } // end Anzeigezyklus
#if (VSPCAM)	 
	 if (cycle && fp.vspcam)												// viaspeedcam angeschlossen
	 {
		if ((fp.vspcam==2) || ((fp.vspcam==1) && !(cycle&0x01))) 
		{	
		 if (online && measure(0)) // Online Messmodus
		 {
	    put_signed_float (speed10, 10, 0, fp.nk); // Geschwindigkeit mit Vorzeichen	   
	    newline();	   
	   }
	  } } // end if viaspeedcam angeschlossen
#endif	 
	 if (++cycle>=fp.mcycle/BASECYCLE) cycle=0;	// Messzykluszähler reset
		
	 //wait_uarts_empty();							// Warte bis alle Zeichen gesendet, wg. Interrupts und sleep
	 	 
	 if (!(mqtt_com>1) || simulation)					// überspringe bei MQTT-Nachricht, nie bei Simulation 
	 { 	
    if (osActiveThread)		 									// Verbindungsaufbau?
    {
		 cyclerest=BASECYCLE-cycletime;					// Verbleibende Zeit des Basiszeitzyklus
		 if (cyclerest>0) osDelay (cyclerest);	// Zeichenempfang unterbricht nicht Wartezeit
		}	
	  else if (refresh||acycle||connect||simpinset||gps_pending)	// Zyklus mit Ergebnis, Anzeige aktiv, Verbindung
	  {			 
	    cyclerest=BASECYCLE-cycletime;				// Verbleibende Zeit des Basiszeitzyklus	
	    if (cyclerest>0)											// Schleifen-/Messdauer kürzer als Basiszeit?
       getkey (cyclerest);									// Warte auf Zyklusende oder Zeichenempfang 	
    } 	
	  else if (cycle==1)											// Messzyklus ohne Ergebnis, Anzeige etc.
	  {         
		 InitWatchdog(fp.mcycle-cycletime);			// Setze Watchdog auf verbleibende Messzykluszeit		 	
		 DeepSleep ();													// Bringe CPU in Deep sleep 					 
		 cycle=0;									 							// Online Zyklenbetrieb wieder mit Messung anfangen
    }		
	  else getkey(BASECYCLE);	
	 } // end if not comchange	
  } //end if Messsteuerung installiert
 } // end while forever
}
