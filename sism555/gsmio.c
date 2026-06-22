//-----------------------------------------------------------------------------
//  FILE: gsmio.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für Funkmodem und GPS Erweiterung
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H6
//-----------------------------------------------------------------------------
//  VERSION :  1.01
//-----------------------------------------------------------------------------
//  CREATED :   16.08.2012
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//              	16.08.2012 File creation
//								08.04.2015 Anpassung für LPC1766
//								22.04.2015 Hardware version H1 -> H2
//								06.05.2015 GPS L70 Routinen added
//								13.07.2015 Hardware version H2 -> H3
//								15.08.2017 Hardware version H6, GSMPWR, PWRKEY und GPS_PWR, M10 und UC15
//								17.05.2018 Hardware version H5 -> H6 EMG_OFF/RESET_N Bugfix
//-----------------------------------------------------------------------------

#include "gsmio.h"
#include "btio.h"
#include "i2cm.h"
#include "rtc.h"
#include "sio.h"
#include "sicom.h"
#include "sictxt.h"
#include "libtool.h"
#include "xmodem.h"
#include "flash.h"
#include "ramcode.h"
#include "string.h"
#include "mqtt.h"
#define DTCBASE 20000
#include "dtc.h"

const char T_month[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };		// Monate für Email Header Date

void Init_GSM_ch (uint baud)	// Uart1 initialisieren und GSM Kanal an MUX einstellen
{																// Übergabe baud - baud rate e. g. 115200
 uart1_request(MUX_GSM, baud);		// State machine: MUX + UART1 + BT suspend
}	

void send3plus	(void)			// Umschaltung Modem vom data in den command mode
{	
 uchar i;										// Laufvariable	
 osDelay(1000);							// laut M10/UC15 1s vorher ohne Zeichen
 for (i=0;i<3;i++) 					// Sende "+++" in innerhalb 1s im 50ms Abstand
 {		
	putc('+'); 
	osDelay(50); } 
 osDelay(950); 							// laut spec. 1s nachher ohne Zeichen
}

int wait_ok_time (uint timeout) { return(wait_message(T_OKNZ,timeout)); }	// Warte timeout lang auf OK Antwort
int wait_ok (void) { return(wait_ok_time(XMODEM_BLOCK_TIMEOUT)); }				// Warte 3 Sekunden auf OK Antwort

int gsm_power (uchar mode)		// GSM Modem ein-/ausschalten
{																		// Übergabe mode - 	Bit 0 - 0/1 = Funkmodem aus-/ein
																		//									Bit 1 - 0/1 = MUX GSM Kanal deselect/select
 uchar power_restart=0;			// Flag für Modem Neustart
 uchar retry=200;						// Wiederholungen	
 
 if (!mode)						// GSM abschalten, mode=0
 { 
  FIO1CLR = PWRKEY;				// GSM PWRKEY LOW  
  osDelay (25);
	if (!(FIO0PIN&PWR_GPS)) // Wenn GPS auch aus 
	 FIO1CLR = GSMPWR;			// Versorgung für GSM und GPS auch aus 
	gsmpower=0;							// GSM Power Status aus 
  gsmcall=0;							// Reset bestehende Dial-in Verbindung
  simpinset=0;						// Pinnummer ist weg	
	FIO0CLR=GSM_DTR;				// DTR LOW
	uart1_release(MUX_GSM);		// Release UART1, auto-restores BT if connected
	wait_min=3;
 }

 if (mode&0x01)	    			// GSM einschalten, mode 1,3
 {
  if ((fp.gsm==EG91)&&(FIO1PIN&GSMPWR))
  {		
	 FIO1CLR = GSMPWR;  	 		// Erst abschalten
	 osDelay(1000);						// 1s aus lassen
	}
  if (!(FIO1PIN&GSMPWR) || !(FIO1PIN&PWRKEY))	// Modem ist aus?
  {
   FIO1SET = PWRKEY|GSMPWR;	// M10 PWRKEY Signal HI und Versorgung ein
   FIO0CLR = GSM_DTR;				// DTR LO - Disable Modem Sleep 			
   osDelay (100);						// UC15 nur 0,03s Angabe
   FIO1CLR = PWRKEY;				// GSM PWRKEY Signal LOW
	 osDelay (500); 		
   FIO1SET = PWRKEY;				// GSM PWRKEY Signal wieder HI	
   power_restart=1;					// Modem wurde neu gestartet 		
	 gsmpower=1;			    		// GSM Power Status ein
	 simpinset=0;							// Pinnummer ist nach restart weg
   if (mode==1) return(1);		
  } // end Modem nicht eingeschaltet
 } // end GSM einschalten
 
 if (mode&0x02)								// Warte bis +CFUN=1 Modem Statusmeldung volle Funktion 
 {		
  Init_GSM_ch (GSM_BAUD);				// MUX GSM Kanal auswählen und UART1 auf 460800 Baud	 
	 
	if (FIO0PIN&GSM_DTR)				//  Modem Sleep enabled?
	 FIO0CLR=GSM_DTR;						// DTR  LO - Disable Modem Sleep	
		
	do
	{	
	 if (!(FIO2PIN&CTS)) break;	// CTS low?	
	 osDelay(50);								// Warte 50ms
	} while (--retry);
  if (!retry) return(-1); 		// CTS ging nicht low -> Abbruch	
	
	if (!power_restart) 						// Kein Modem Neustart?				
  {									
   concpy=connect;								// Kein Neustart Funktionsstatus aktiv abfragen
	 retry=16;											// Anzahl Abfragewiederholungen
   do	
   {	 
	  connect=UART1;																		// GSM Uart1 auswählen 
	  bxi=rxi;																					// Flush Empfangspuffer
	  putln(T_atcfun);																	// Abfrage Funktionsstatus
	  connect=concpy;																		// Uart Status wiederherstellen
	  if (wait_message(T_cfun,XMODEM_CHAR_TIMEOUT)>0)	
	  { 
	   wait_ok_time(XMODEM_CHAR_TIMEOUT); 							// Warte auf OK trailer	
	   return (1);																			// Ende bei Zeichenkettenvergleich positiv		 
	  }
	 } while (retry--);	 																// ggf. mehrfach abfragen
	 mqtt_con_err=43;																		// Debug Marke AT+CFUN? gescheitert setzen
	 return(-1);									// kein Erfolg				
  } 	
	else 			// Neustart - Einschalt URC abwarten
	{
	 if (wait_message(T_cfun,5*XMODEM_BLOCK_TIMEOUT)>0) // +CFUN: 1 sollte innerhalb 15s da sein		
	  return (1);																				// Ende bei +CFUN=1 Zeichenkettenvergleich positiv	
   mqtt_con_err=44;																		
	 return(-1);									// keine +CFUN=1 Einschaltmeldung erhalten 
  }	// end else kein Neustart 				
 }	// end mode & 2

 else { MUX_BT_Select; }				// MUX Bluetooth Leitungsauswahl	 mqtt_con_err=43;

 return (0);
}

void set_server_default(uchar servertyp)	// Default Parameter für Server setzen
{
 char *wp;																// Hilfszeiger zum Löschen der Textfelder
 wp=&fp.apn[0];														// auf Adresse APN Eintrag setzen
	
 do *wp++=0;  while (wp < &fp.smsno[0]); 	// Lösche alle fp Textfelder zwischen APN und SMSNO 
																					// Eintrag um Artefakte zu vermeiden	
	
 fp.mailmode=0;														// Kein Email Versand
 fp.mailalarm=0;													// keine Email-Alarm Nachrichten versenden
 mailtosend=0;														// reset Email Versandaufgaben 	
 	
	
 if (servertyp==SMTP)												
 {
	fp.temail=0;							// Versandzeit
	fp.ewtag=0;								// Sonntag
	fp.emtag=1;								// 1.	
	strcopy ((char *)T_dmsrv, fp.server);		// Default SMTP Server Name kopieren
	fp.port=Def_smtpport;										// default SMTP Port
	strcopy (T_dehost, fp.ehost);						// Default email host kopieren
	strcopy (T_deuser, fp.euser);						// Default email user kopieren
	strcopy (T_dpass, fp.epass);						// Default email Passwort kopieren
	strcopy (T_dmfrom, fp.emfrom);					// Default email from kopieren	 
	fp.mqtt_packetsize=0;										// Delete MQTT Packetgröße  
 }	 
 else if (servertyp==MQTT)
 {
	clean_cpy (T_mqip, fp.server,sizeof(fp.server));	// Default MQTT Server Name kopieren und Feld bereinigen
	fp.port=Def_mqttport;															// default MQTT Port
  clean_cpy (fp.serno, fp.euser,sizeof(fp.euser));	// Default MQTT user kopieren und Feld bereinigen
  clean_cpy (T_mqpass, fp.epass,sizeof(fp.epass));	// Default MQTT Passwort kopieren
  fp.mqtt_packetsize=DEF_MQTT_PACKET_SIZE;					// Default MQTT Packetgröße
  fp.opmode=INTERVAL;																// Default Messdaten Packetversand		 
	MQTT_index=0;																			// reset MQTT Messdaten Sendeindex
  MQTT_send=(uint)((fp.md_page<<16)+fp.md_adr);			// Sendezeiger auf aktuellen Messdatenzeiger setzen											 
 }	 
 fp.servertyp=servertyp;									// Lokalen Servertyp nach Parameterblock
}

void init_gsm (void)	// GSM Modem initialisieren 
{
 int i=5;	   						// Laufvariable
 uchar uartsave=connect;
 uchar modem;	

 if (connect&GSM_LINK)	return;		// Abbruch wenn GSM bereits aktiv		
	
 fp.gsm=0;								// Reset GSM installiert 
 gsm_power (1);						// GSM Modem einschalten
 Init_GSM_ch (115200);		// MUX GSM Kanal auswählen und UART1 konfigurieren
	
 osDelay(3);							// warte auf Umschaltung		
	 
 connect|=UART1;					// an Modem senden
 do												// Prüfe 115200, 9600, 307200, 460800
 {  
  putln(T_at);															 
  if (wait_ok()>0) break;							// Abbruch wenn Antwort "AT ... OK"    
	if (i==4) 	Init_UART1 (9600);			// 1. Fehlschlag: teste mit 9600 Baud
	if (i==3) 	Init_UART1 (307200);	// 2. Fehlschlag: teste mit 307200 Baud
	if (i==2) 	Init_UART1 (460800);	// 3. Fehlschlag: teste mit 460800 Baud
 } while (i--);												// Maximal i+1 mal senden
  
 if (i>=0) i=3;								// wenn Kommunikation erfolgreich
	
 while (i>0)										// Maximal 3 mal Werksreset bzw. Abfrage ID senden
 {  
  put2str(T_at,T_def);					// Reset auf Werkseinstellung "AT&F" 
  if (wait_ok()>0) 							// Weiter wenn Antwort "AT&F ... OK"
	{
	 cind=0; 
	 putln(T_ati);								// Abfrage Produkt Identification
	 if (wait_ok()>0) break;				      
	}	 
	i--; 
 }
 connect&=~UART1;							// Nur an Terminal(s)	senden
	
  if (i)												// "OK" Antworten etc. empfangen 
  {
   putln(cbuf);									// Antwort bis "OK" ausgeben
	 if (compare(T_uc15,&cbuf[0],0,cind)>0) modem=15;					// Prüfe Puffer auf Produkt ID UC15
   else if (compare(T_eg91,&cbuf[0],0,cind)>0) modem=91;		// dto. EG91
	 else modem=1;
   osDelay (2);			
   connect|=UART1;							// auch wieder an GSM Modem senden
	 putln(T_cfg);								// Gemeinsame Konfiguration alle Modems senden
	 if ((wait_ok_time(XMODEM_BLOCK_TIMEOUT)>0) && ((FIO2PIN&CTS)==0)) 	// ok, kein Timeout und CTS/RTS aktiviert?	
	 {	 
	  putln(T_cfuc15);						// Konfiguration senden		
    if (wait_ok_time(XMODEM_BLOCK_TIMEOUT)>0) 	// warte auf ok oder Timeout
		{
		 putln(T_cfsto);						// Baudrate umstellen und Konfiguration speichern		
		 if (wait_ok_time(2*XMODEM_BLOCK_TIMEOUT)>0)	// ok und kein Timeout?	
     {													// Default Verbindungsparameter GSM/GPRS/Email setzen	
      fp.apn[0]=0;										// GPRS APN reset
      fp.user[0]=0;										// GPRS USER reset
      fp.pass[0]=0;										// GPRS PASSwort reset
      fp.smsalarm=0;									// keine SMS Alarm Nachrichten versenden
      fp.roaming=0;										// keine roaming Verbindung zulassen
		  fp.gsmtage=0x3E;								// Modem alle Wochentage ein
      fp.tgsmein=8*60;								// 08:00 Uhr ein
      fp.tgsmaus=18*60;								// 18:00 Uhr aus
      fp.simpin=0;										// 0 - SIM Pinnummer Abfrage ausgeschaltet
      fp.pinerr=0;										// 0 - Kein Fehler Pinabfrage aufgetreten
		  fp.smssc[0]=0;									// reset service center number
		  fp.smsno[0]=0;									// reset sms call number
			fp.radnet=2;										// Funknetz default Automatisch
		  gsmcall=0;											// reset Einwahlverbindung
      interfaces|=GSM_LINK;						// zu installierten Interfaces hinzufügen
      fp.gsm=modem; 									// in Parameterblock registrieren
			set_server_default(SMTP);				// Default Parameter für SMTP Server setzen
    } // end if ok auf Baudrate und Konfiguration speichern   
   } // end if ok auf modemspezifische Konfiguration
  } // end if ok auf gesendete gemeinsame Konfiguration
 } 
 uart1_release(MUX_GSM);					// Release UART1, auto-restores BT
 connect=uartsave;									// Schnittstellenzustand wieder herstellen
  
 if (!fp.gsm) 											// Fehler?
 {	 
	dtcerr(E_gsm);							// Fehlermeldung ausgeben
	newline(); 
  gsm_power(0);											// GSM aus und deselektieren
 }	 
 else putln(cbuf);									// Letzte Antwort ausgeben
 
 osDelay (10);											// Warte auf geänderten Schnittstellenzustand
 clear_comchange ();								// Bereinige Schnittstellenwechsel																					
}


int test_gsm (uchar poweron)	// Prüfe ob GSM/GPRS Modem antwortet
{															// Übergabe poweron - 1 bei Systemstart
 int result;
 uchar uartsave=connect;
 uchar retry=1;	
	
 do
 {	 
  result=gsm_power (3);						// GSM Modem ggf. einschalten
  if (result>=0)			
  { 
   msdelay=3000;	 
   while (FIO2PIN&CTS) if (!msdelay) break;		// Geht CTS in 3s low?
	 if (msdelay) 	
   {	
	  connect=UART1;																// an Modem senden
    putln (T_at);																	// Text "AT"
    result=wait_ok_time (XMODEM_CHAR_TIMEOUT); 		// auf AT Antwort "OK" empfangen?            
    if (poweron) FIO0CLR = CSAF;									// MUX GSM Kanal deselect beim Einschalten
    connect=uartsave;															// Uart Status wiederherstellen
   } // end if msdelay	 
	 if (result>=0) break;
  } // end if result >0
 } while (retry--);															// Eine Wiederholung
	
 if (result<0)											// Misserfolg?
 {
  gsm_power (0);	 									// Abschalten	
	if (poweron) puterror(GSM_ERROR,-1);	// beim Einschalten Fehler melden und protokollieren
	else  
	{
	 dtcerr(E_gsm);						// Fehlermeldung ausgeben
	 newline();	
  }		
 } // end if result

 return(result);
}

int modem_start (void)									// Einschaltsequenz Funkmodem
{
 int result=1;

 if (!gsmpower)																// Modem ist aus? 
 {	
	result=gsm_power (3); 											// Einschalten
  if (result>0) 															// Powered und +CFUN Meldung
  {			 	
	 result=wait_line(3*XMODEM_BLOCK_TIMEOUT);		// Warte auf nächste Modemzeile		 
	 if (result>0)			
	 { 		 
		result=compare(T_cpin,cbuf,0,cind);				// +CPIN erhalten?
	  if ((result>0))														// CPIN Startnachricht erhalten?
	  {					 															
		 if (compare(T_ready, cbuf, 7,cind) > 0)	result = 2;				// Enhält die Zeile "READY"?	
		 else if (compare(T_simpin, cbuf,7,cind) > 0) result = 3;		// Enhält die Zeile "SIM PIN"?
		 else result = 4;																						// Anderes Ergebnis			 	      
	  }	
   }		
  }		
 }  // end Modem ist aus
 else result = gsm_power (2);							// Modem ist bereits an, select MUX GSM Kanal, disable sleep und frage CFUN ab	

 return (result);
}	

void atcpin (ushort set, uchar manual)	// Setze Pinnummer oder prüfe Status
{																				// Übergabe	set			- 0/1 Pinnummer prüfen/setzen
																				//			 		manual 	- 0/1 nach Zeitplan automatisch/manuell setzen	 
 int result;														// Ergebnis Abfrage
	
 concpy=connect;
	
 result=modem_start ();									// Einschaltsequenz Funkmodem	
	
 if (result > 1) {																		// Rückmeldung von Modem?							
	connect=concpy;				
	if (!set) 																					// PIN nicht setzen?
	{	
	 if (manual)  																			// Terminalaufruf?
	 { 
    putstrCR(cbuf); 																	// Puffer der Einschaltesequenz senden
    newline(); 		
   }		 
	 if (result==2)  simpinset=1; 											// Ready im Puffer -> Pinnummer ist gesetzt
	 if (result>=3)  gsm_power(0);											// SIM PIN, SIM NOT INSERTED, BUSY, ... -> Modem wieder aus 
	 wait_message (T_pbdone,5*XMODEM_BLOCK_TIMEOUT); 		// Warte Ende Einschalt URCs
	 return;																						// und Ende
  } // end if PIN nicht setzen
 } // end if Rückmeldung von Modem? 
 
 /* else if (result<0)																		// keine Rückmeldung GSM/UMTS/LTE Modem Fehler
 {  
	connect=concpy; 
  puterror (GSM_ERROR,-1); 
  gsm_power(0); 
  return; }		
 
  putnumber (result,0); newline();
  osDelay (200); */
																						// Modem war bereits an (result=1) oder PIN muss gesetzt werden				
 connect=UART1;															// GSM Uart1 auswählen  
 putln(T_cmee);															// sende +CMEE=2
 rxi=bxi;
 result=wait_ok_time(XMODEM_BLOCK_TIMEOUT);	// warte auf OK Antwort
 
 if (result<0) 
 {	 
	connect=concpy;  
	if (manual) { 
		putln(cbuf);									// Modem letzte Status Meldung ausgeben 
	  newline();
	}	
	mqtt_con_err=40;
	gsm_power(0); 	
	return;	
 }
 	  
 put2str(T_at,T_cpin);						// sendet "AT+CPIN"
 if (set)													// Pinnummer setzen
 {
   putc('=');				
   putnumber(fp.simpin,0xC4);				// Pinnummer senden
 }																								   
 else putc('?');									// Pinnummer abfragen
 
 bxi=rxi;													// bisherige Meldungen löschen
 simpinset=0;											// Status Pinnummer nicht gesetzt
 newline();
 connect=concpy;									// Uart Status wiederherstellen
 result=wait_message (T_ready, 2*XMODEM_BLOCK_TIMEOUT); // Ready im Puffer?	 
 
 if (result>0) 										// Ok und +cpin: ready erhalten?
 {  
   simpinset=1;										// Status Pinnummer gesetzt
   fp.pinerr=0;										// kein Pinfehler aufgetreten	 
 } 
 else if (compare((char *)T_wpass,cbuf,0,cind)>0) // +CME Error: incorrect password?
 {
  if (!fp.pinerr && !osActiveThread)	// Pinfehler noch nicht protokolliert?
	 protocol (SIM_ERROR);							 	
  fp.pinerr=1; 												// Falsche Pinnummer
 }

 if (manual) putln(cbuf);					// Modem Status Meldung ausgeben
 
 if (!simpinset) 		// Sim Pin nicht gesetzt?
 {	 
	 gsm_power(0);		// Schalte Modem ab, wenn Pinnummer nicht gesetzt 
	 mqtt_con_err=41;
	 bxi=rxi;					// Modem Ausschaltmeldungen löschen
 } 
 else 							// Pinnummer gesetzt nach Einschaltung?
 {
  if (set) wait_message (T_pbdone,XMODEM_BLOCK_TIMEOUT); 	// Ja, warte Ende Einschalt URCs 
	else wait_message (T_OKNZ,XMODEM_BLOCK_TIMEOUT); 				// Warte auf Ok
 }	 
}

void GSM_Registration (void const *argument)					// Modem Einschaltung und Registrierung im Funknetz
{
 while (1)																	
 {	 
  osSignalWait (2,osWaitForever);						// Warte auf Signal von MainThread
	atcpin(fp.simpin,0);											// Modem einschalten und Pin ggf. setzen 
	osActiveThread&=~GSM_REGISTRATION;				// Registrierungs Thread inaktiv 
	osSignalClear(gsmp_thread_id,2); 					// Reset GSM_registration thread Startsignal
 }	 
}

int test_registration (void)	// Prüfe ob Modem eingebucht ist
{
 uchar retry=16;						// Anzahl Abfragewiederholungen
 uchar i;										// Laufvariable für iccid Kopie	
 uchar status=0;
 int pos;	

 if (!simpinset)	    			// Pin noch nicht gesetzt?
 {
	if (!fp.pinerr) 		  		// kein Pinfehler?
	atcpin(fp.simpin,0);			// -> Modem einschalten und Pin ggf. setzen
 }
										  
 if (simpinset)							// Pin gesetzt?
 { 
  if (gsm_power (2)>0) 	    // ggf. MUX Kanal auf GSM/GPRS setzen			
  {		
   do
   {
    connect=UART1;						// GSM Uart1 auswählen
    bxi=rxi;									// Flush Empfangspuffer
		if (fp.gsm==EG91) putln (T_cregeg91);		// EG91 Netzwerk Registrierungsstatus abfragen
		else putln (T_creguc15);
    connect=concpy;					// Uart Status wiederherstellen		 
    if (wait_ok_time(XMODEM_BLOCK_TIMEOUT)>0)		 
    {	 
		 pos=compare(T_creg,cbuf,0,cind);														// Position +CREG suchen
     if (pos>=0)
     {			 
		  if (cbuf[3+pos]==',') status = cbuf[4+pos];
		  if ((status =='1')||((status=='5')&&fp.roaming)) break;
		  pos=compare(T_cgreg,cbuf,pos,cind);												// Position +CGREG suchen
			if (cbuf[3+pos]==',') status = cbuf[4+pos];
		  if ((status =='1')||((status=='5')&&fp.roaming)) break;
      if ((pos>=0) && (fp.gsm==EG91))														
			{
			 pos=compare(T_cereg,cbuf,pos,cind);											// EG91 auch +CEREG suchen
			 if (cbuf[3+pos]==',') status = cbuf[4+pos];
		   if ((status =='1')||((status=='5')&&fp.roaming)) break;	
      } 				
		 }	 
		}  	// end wait_ok 			   	  					   	  
    osDelay(5000);						// warte 5s zwischen Abfragen
   } while (retry--);	 				// ggf. mehrfach abfragen
	 
	 if ((status =='1')||((status=='5')&&fp.roaming)) // Status eingebucht oder "roaming"? 
	 {	
		connect=UART1;						// GSM Uart1 auswählen	
		putln(T_csq);							// Signalstärke abfragen	
		bxi=rxi;									// Puffer leeren		
		if (wait_message(T_csq+3,XMODEM_CHAR_TIMEOUT)>0)	
		{								 
		 if (wait_ok()>0) 
		 fp.csq=atoi(&cbuf[2]); 	// Signalstärke sichern
    } 	
		putln(T_qccid);						// SIM ICCID mit "AT+QCCID"	Kommando abfragen
		bxi=rxi;									// Flush Empfangspuffer
		if (wait_message(T_qccid+3,XMODEM_CHAR_TIMEOUT)>0)
		{
		 if (wait_ok()>0)										// Warte auf Ok
		 {	
			i=2;												// Führendes CRLF überspringen
      while (cbuf[i]!='\r') i++;	// Stringabschluss suchen
			cbuf[i]='\0';								// String terminieren
			clean_cpy(&cbuf[2],iccid,sizeof(iccid));	// String kopieren
		 }	
		}
		else iccid[0]='\0';			
		connect=concpy;						// Uart Status wiederherstellen
 		mqtt_con_err=0;						// reset Verbindungsfehler	
		return (status);					// Ja, Status zurück 				   	  	
	 }
	 else mqtt_con_err=42;			// Keine Registrierung	
	} // end if gsm_power > 0
	else simpinset=0;					// reset pin status
 }
 connect=concpy;						// Uart Status wiederherstellen
 return(-1);
}

void send_local_ip (void)			// Lokale IP Adresse ausgeben
{
 uchar i,send=0;							// Lauf und Hilfsvariable
 uchar uartsave=connect;
	
 putstr(T_act);														// Modem AT+QIACT? senden
 putc('?');	
 newline();																// Kommandoabschluss
 connect&=~UART1;													// nur an Terminal senden 
 if (wait_ok()<0) putln (cbuf);						// Warte auf ok, bei Fehler sende Pufferinhalt
 else
 {
	putstr(T_ip);														// Text "+IP: senden	
	for (i=0;i<cind;i++) {									// Puffer durchsuchen
	 if (cbuf[i]=='\"') {										// bis Anführungsstriche
		if (send) break; 
		 else {send=1;	i++; } }	 
	 if (send) putc(cbuf[i]);								// Zeichen zwischen Anführungsstrichen senden 
  } // end for
	 newline();
	} 			 
 connect=uartsave;												// Ausgabezustand wiederherstellen
}

int set_gprs_config (uchar mode)	// GPRS Konfiguration senden und testen
{																	// Übergabe mode	-	Bit0 - 0/1 für ohne/mit Terminalausgabe
																	//									Bit1 - 0/1 Deaktiviere GPRS beim Beenden		
 uchar state=1;									// Laufvariable
 uint timeout;									// Zeitabbruch
 uchar uartsave=connect; 				// Schnittstellenstatus sichern										
 
 if (test_registration ()>0)				// Prüfe ob Modem eingebucht ist
 {
  if (mode&0x01) connect|=UART1;	// Ausgabe der Kommandos an Terminal
  else connect=UART1; 
		
  for (state=2;state<5;state++)
  {
   switch (state)
   {
    case 2:	put2str(T_at,T_qicsgp);						// Kommando at+qicsgp=1,			  
						putc('1');												// 1 = IPV4 Kontext
						putc(',');										
						putqstr(fp.apn);									// Zugriff, User und Passwort setzen
						putc(',');					
						putqstr(fp.user);
						putc(',');
						putqstr(fp.pass);
						newline();						
						timeout=3*XMODEM_BLOCK_TIMEOUT;		// Aktivierung dauert länger	
						break;						
		case 3:	osDelay (50);											// Warte 50 ms
						bxi=rxi;													// Empfangspuffer flush 
						putln(T_act15);										// Aktiviere Verbindung
						timeout=20*XMODEM_BLOCK_TIMEOUT;	// auf Antwort GPRS Server warten			
						break;	
	  case 4: if (mode&0x01) send_local_ip ();	// bei Initialisierung/Terminalausgabe lokale IP an Terminal ausgeben						  		
						if (mode&0x02)
						{
						 connect=UART1;		 	
						 putstr(T_qideact);					    		// Deaktiviere IP Kontext
						 putstr("=1");		// bei UC15 Kontextnummer (immer 1, nur eine Verbindung) angeben
						}
		        else putstr(T_at);									// Dummy AT senden				
						newline();	
						break;															// AT Kommando abschließen   						
   } // end switch state
   if (wait_ok_time(timeout)<0) break;	// kein Ok -> Abbruch	
  } // end for 
 } // end if test_registration
 
 connect=uartsave;				// Ausgabezustand wiederherstellen
 if (state<5) 						// Antwortfehler, Schleifenende nicht erreicht?
 {
  putnumber (state,0);
  newline();
  return(-state);					// Abbruch vorzeitig, Fehler
 }
 else return(1);
}

void putcbuf_line (uchar offset)	// Puffer terminieren und eine Zeile an Schnittstelle senden
{																	// Übergabe offset - Zeichenoffset in cbuf 
 uchar uartsave=connect;		// Schnittstellen sichern
	
 connect&=~UART1;						// Nicht an Modem senden
 cbuf[cind]=0;							// String in Puffer Null terminieren
 cind=offset;
	
 do	
 {	  											// Puffer senden
  if (cbuf[cind]==0) break;	// Null Terminierung?
  putb(cbuf[cind]);	}				// Ausgabe mit putb zur Vermeidung CR->CRLF Erweiterung
 while (cbuf[cind++]!=LF);	// bis Zeilenende
 			
 connect=uartsave;						// Ausgaben wieder an Schnittstellen
}

bool email_messdaten (void)			// Erweiterten Email Header ausgeben
{								   
 uint anzmwerte=messdaten(2);		// Anzahl Messdaten und Batteriespg. berechnen und in Mail schreiben
 putmstr(L_mbnd);								// MIME Trenner, Boundary und Content type
 putstr(T_mattach);							// Text "application/octet-stream\nContent-Disposition: attachment; filename=...
 fp.fileno++;										// inkrementiere fortlaufende Dateinummer
 make_filename ();							// Erstellt VTF Dateiname in cbuf
 redirect_char_Out(putb);				// Zeichenausgabe wieder an UART(s) leiten
 putln(cbuf);										// Dateinamen ausgeben
 putln(T_mencode);							// Text "Content-transfer-encoding: base64\n" - wichtig Leerzeile hiernach
 if (send_vtf_file (anzmwerte, 0))	// VTF Datei senden erfolgreich?
 {	 
  put2str(T_LF,T_ddash);				// MIME Trenner
  putstr(T_mbound);							// Boundary String
  putln(T_ddash);								// Close multipart message
	return (true); 
 }	 
 return(false);
} 

uchar sendmail (uchar mode)			// Email versenden und konfigurieren
{										// Übergabe mode 	Bit 0	-	0/1 ohne/mit Terminalausgabe
										//								Bit 1	-	0/1 Verbindung halten/schließen
										//								Bit 2	-	0/1 keine/eine Email senden																			
										

 uint timeout=XMODEM_BLOCK_TIMEOUT;		// Antworttimeout
 ushort response=0;										// Numerischer ASCII (SMTP-Server-)Antwortcode 
 uchar state=10;											// Zustandsvariable, beginne erst bei 10 wg. GPRS states
 uchar failure=0;											// Fehler aufgetreten
 uchar testmail=0;										// Hilfsvariable 1 wenn Testmail		
 text *P_resp;												// Zeiger auf Modem - Antwortstring
 uchar uartsave=connect;							// Schnittstellenstatus sichern		
 uchar utc;														// Hilfsvariable für Zeitzone		
	
 InitWatchdog(LONG_WD_32);							// langes Watchdog Interval auf 32 Sekunden setzen
 
 if ((mode&0x04)&&(connect&(UART0|USB_LINK)))		// Mail zu senden und Terminal verbunden
 {
  put2str(T_LF, T_txmail);								// Ausgabe Text "Sende Email  
  putln(T_dot3);													// Ausgabe "..."
 }
 	
 if (mode&0x01) connect|=UART1;						// Ausgabe auch an angeschlossenes Terminal?
 else connect=UART1;											// Ausgabe nur an Modem	
  	
 do																				// Zustandsfolgeschleife
 {
  P_resp=T_nil;														// Reset Antwortstring
  response=0;															// Reset Antwortcode
	ResetWDT();															// Watchdog reset
 	bxi=rxi;																// flush buffer 	 
  
  if (state==10) 													// GPRS Verbindung aktivieren
  { if (set_gprs_config(0)< 0)						// GPRS Verbindung erfolgreich? 
	 { failure=1; 													// Nein, Fehlerabbruch
	   state=38;
	 } 
  }	 
  else if (state==11) 										// Lokale IP an Terminal ausgeben
  { if (mode&0x01) send_local_ip (); 			// nur bei Terminalausgabe
		state++;															// weiter mit state 13
  }
  else if (state==13)											// TCP Verbindung zum Mailserver öffnen
  {   
   putstr(T_qiopen);											// Kommando AT+QIOPEN= senden
	 putstr(T_tcp);													// Kontext "TCP" senden
   putqstr(fp.server);										// IP Adresse oder URL in Quotes senden
   putc(',');
   putnumber(fp.port,0);									// Portnummer angeben 
	 putln(",0,2");													// Local Port automatic, transparent access mode	
   timeout=3*XMODEM_BLOCK_TIMEOUT;	    	// 9 Sekunden Timeout für TCP Verbindungsaufbau
   P_resp=T_connect;											// Antwortet mit CONNECT 
	 state++; 															// weiter mit state	15 
  } 					
  else if (state==15)	  									// Prüfe Restmeldung Mailserver nach Connect
  {	  	
   if (mode&0x01) 												// Bei Terminalausgabe
   { connect=uartsave&~UART1; 						// nicht an GSM modem
     putstr(&cbuf[2]);										// Ausgabe CONNECT	Status an Terminal
	   connect|=UART1; } 	   
   response=220;													// Serverantwortcode 220					
  }
  else if (state==16)		    							// Sende Host Domain Name
  {
	 if (mode&0x04) connect=UART1;					// Bei Mailversand Terminalausgabe stoppen	
   if (fp.euser[0]==0) 										// Kein Euser?
   {
    putstr(T_Helo); 											// Sende "HELO ", nur SMTP
		state+=3;															// ubergehe 3 Folgezustände (AUTH LOGIN)
   }
   else putstr(T_Ehlo);										// sonst sende "EHLO ", ESMTP
	 putln(fp.ehost);												// Sende "hostdomain.de"	  
   timeout=3*XMODEM_BLOCK_TIMEOUT;				// Wegen extra langer ESMTP Antwortsequenz
   response=250;													// Serverantwortcode 250	
  }
  else if (state==17)		    							// Sende AUTH LOGIN
  {
   putstr(T_login);												// AUTH LOGIN
	 connect=uartsave&~UART1;
   putc(' ');															// Nur an Terminal	
	 connect=UART1;
   newline ();														// Nur an UART1
   connect=uartsave;		
   response=334;													// Serverantwortcode 334
  }
  else if (state==18)											// Sende EUSER
  {
	 connect=UART1;	
   putstr_b64 (fp.euser, 0);							// Account Username Base64 kodiert senden	 	
	 newline();
	 if (mode&0x01) connect|=uartsave;				// Bei Terminalausgabe	
	 response=334;													// Serverantwortcode 334
  }
  else if (state==19)											// Sende EPASS
  {
	 connect=UART1;	
   putstr_b64 (fp.epass, 0);							// Account Passwort Base64 kodiert senden
   newline();
	 if (mode&0x01) connect|=uartsave;			// Bei Terminalausgabe	
   response=235;													// Serverantwortcode 235
  }
  else if (state==20)											// Sende MAIL FROM
  {
   if (mode&0x01) connect|=uartsave;				// Ausgabe auch an angeschlossenes Terminal?
   put2str(T_mailf, fp.emfrom); 					// Text und MAIL FROM Einstellung senden
   putln(T_cb);							 
   response=250;													// Serverantwortcode 250
   if (mode<4)	state=33;		 							// Keine Mail zu senden -> QUIT Serververbindung schließen
  }
  else if (state==21)											// Sende RCPT - Mail to
  {
   put2str(T_mrcp,fp.emto);								// Empfängeradresse senden
   putln(T_cb);
   response=250;													// Serverantwortcode 
  }
  else if (state==22)											// Sende RCPT - Mail copy 
  {
   if (isalpha(fp.emcopy[0]))							// Mail copy to gesetzt?
   {
    put2str(T_mrcp,fp.emcopy);						// Empfängeradresse senden
    putln(T_cb);
    response=250;													// Serverantwortcode 
   }
  }
  else if (state==23)											// Mailserver Daten ankündigen
  {
   putln(T_mdata);												// SMTP DATA senden
   response=354;													// Serverantwortcode 
  }	
  	
  else if (state==24)											// --- Mailheader aufbauen
  {	
   connect=UART1;													// Nicht an Terminal senden
   if (isalpha(fp.emto[0]))		   					// Empfängeradresse gesetzt?
   {							
    putstr(T_hfrom);											// Headertext "From: "
    putln(fp.emfrom);											// Senderadresse
    putstr(T_hto);												// Headertext "To: "
    putln(fp.emto);												// Empfängeradresse
    if (isalpha(fp.emcopy[0]))						// Mail copy to gesetzt?
    {
     putstr(T_hcc);												// Headertext "Cc: "
		 putln(fp.emcopy);										// Mail copy to Empfängeradresse senden
    }		
    		
		putstr(T_edate);											// Text "Date: "
		read_date_time (DATE_HEX,cbuf);				// Datum nach Puffer
		putnumber (cbuf[0],0xC0);							// Tag
		putc(' ');	
		putstr (&T_month[cbuf[1]-1][0]);			// Monat Kurzname
		putstr (" 20");												// Langes Jahresdateum
		putnumber (cbuf[2],0xC2);							// Jahresdatum
		putc(' ');
		read_date_time (TIME,cbuf);						// Zeitausgabe
		putc(' ');	
		if (fp.utc<0) { putc('-');	utc=-fp.utc; }	// Offset Zeitzone negativ
		else { putc('+'); utc=fp.utc; }							// Offset Zeitzone positiv
		putnumber (utc,0xC2);									// Zeitzone
		putln ("00");													// Offset nur ganze Stunden
		putstr(T_mid);												// Text "Message-Id: "
		read_date_time (DATE_NODEL, &cbuf[cind]); // Datum TTMMJJ in puffer	
		putc('_');															  // Trennzeichen zwischen Datum und Zeit
		read_date_time (TIME_NODEL, &cbuf[cind]); // Zeit hhmmss in puffer
		putc('_'); 														    // Trennzeichen zwischen Zeit und Seriennummer
		putstr(fp.serno);						          // Text "Seriennummer: ausgeben
		putc('@');
		putstr(fp.ehost);											// domain name ausgeben = Message-Id String Ende
		putln(">");														// Close character message-id and CRLF
		
    putstr(T_hsubj);											// Headertext "Subject: "
    putstr (T_viasis);										// Text "VIASIS-"
    put2str(fp.serno, T_dpkt);						// "Seriennummer: ausgeben	
   }
   else state=33;				    							// Weiter mit QUIT Serververbindung
  }
  else if (state==25)											// Headertext "Subject: " je nach Emailtyp vervollständigen 
  {
   if (mailtosend&TESTMAIL) putln(T_status);  	// Testemail Headertext "Subject: " + Text "Status"  				  
   else if (mailtosend&DATAMAIL) 
   {
    putln(T_mdaten);											// Datenmail Headertext "Subject: " + Text "Messdaten"
		putmstr(L_mime);											// Text "MIME-Version: 1.0\nContent-type: multipart/mixed; boundary=
		putln(T_mbound);											// Boundary String
		putmstr(L_mbnd);											// MIME Trenner, Boundary und Content type
		putstr(T_mtplain);										// Text "text/plain; charset=iso-8859-1
   }
   else	if (mailtosend&ALARMMAIL) 
   {
    if (mailtosend&SYSERROR) putstr(T_syserr);	// Alarmmail Headertext "Subject: " + "Fehler"
    else if (mailtosend&(MEM95|FULMEM|LOWBAT)) 				
		{
		 putstr(T_status); 													// Alarmmail Headertext "Subject: " + Text "Status"
		 putc(' ');
		 if (mailtosend&(MEM95|FULMEM)) putstr(T_memfull);		// Meldung "Speicher voll "
		 if (mailtosend&LOWBAT) putstr(T_batlow);							// Meldung "Batteriespannung < 11V"	 
		}
		newline();
   }
   connect=UART1;																// Nicht an Terminal senden 
  }
  else if (state==26)														// --- Mailbody
  {   
   newline();																		// Leerzeile Header zum Mailbody
	 state=32; 																		// Weiter mit Email abschließen	
   if (mailtosend&TESTMAIL) putln(T_mailset);		// Testemail Text "Email Versand eingerichtet
   else 
   {
    infomenu(0);																// Allgemeine Statusinformation
    if (mailtosend&DATAMAIL) 
		{	
		 if (!email_messdaten())							// Messdaten Email senden nicht erfolgreich?
		 { state=38; failure=1; }							// Verbindung beenden, Modem CTS hi und timeout
		}	
    else if (mailtosend&ALARMMAIL) 							// Alarmmail 
    {
     messdaten(2);												// Anzahl Messdaten und Batteriespg. berechnen und in Mail schreiben
		 if (mailtosend&(SYSERROR)) 					// Systemfehler, dann Protokoll senden
		 put_protocol (0);										// die letzten 8 Zeilen Protokoll ausgeben													
    }   
   } // end else Testmail
  }
  else if (state==33)											// Email abschließen
  {
   putln(T_mend);													// CRLF "." CRLF
   if (mode&0x01) connect=uartsave|UART1;		// Ausgabe auch an angeschlossenes Terminal?
   timeout=10*XMODEM_BLOCK_TIMEOUT;
   response=250;			    								// Serverantwortcode   
  }
  else if (state==34)											// QUIT Serververbindung schließen
  {  
	 connect=UART1;													// keine Terminalausgabe mehr	
	 if (fp.gsm==EG91) send3plus	();				// Umschaltung Modem vom data in den command mode		 
   FIO0SET=GSM_DTR;												// DTR HI - switch from data to command mode  
	 timeout=3*XMODEM_BLOCK_TIMEOUT;	
   P_resp=T_OKNZ;													// Erwarte OK Antwort 		 	 
	 state=36;															// Weiter mit state 37
  }
  else if (state==37)											// Modemverbindung zum SMTP-Server schließen
  {
	 FIO0CLR=GSM_DTR;												// DTR sofort rücksetzen, sonst UC15 -> Sleep	
	 timeout=5*XMODEM_BLOCK_TIMEOUT;	
	 connect=UART1;													// keine Terminalausgabe mehr			
 	 putstr (T_qiclose); 										// AT+QICLOSE senden
	 putln("=1,2"); 												// ConnectID und socket timeout 2s senden
   P_resp=T_OKNZ;	   											// Erwarte OK Antwort		   	
  }
  else if (state==38)											// GPRS Verbindung deaktivieren
  {   
   putstr(T_qideact);											// Deaktiviere IP Kontext
	 putln("=1");														// Ergänze Kontext ID	
	 timeout=5*XMODEM_BLOCK_TIMEOUT;	
   P_resp=T_OKNZ;	   											// Erwarte OK Antwort
  }

  if (P_resp!=T_nil)											// Modemantwort erwartet?
  { if (wait_message(P_resp,timeout)<0) failure=1; }	// falsche Antwort -> Fehler markieren    
  else if (response)											// Serverantwort erwartet?
  {   
   uart_read(512,timeout,0);							// Lies Antwort
   if (cind>0)														// Antwort erhalten?
   { if (mode&0x01) putcbuf_line (0); 		// Pufferzeile an Terminal ausgeben
	   if (atoi(cbuf)!=response) failure=1;	// Antwortcode falsch
   }
   else failure=1;												// keine Antwort    
  } // end if response

  if (failure)
  {   
   if (mode&0x01) 
   {
    connect=uartsave&~UART1;								// Nur an Terminal ausgeben wenn aktiv
    putnumber (state,0);
    newline();
		connect|=UART1;		  									// Ausgabe wieder an Modem
   }
   break;	 
  }	// end failure
  else if (state==33)											// Kein Fehler und Nachricht vom Server akzeptiert
  {
   if (mailtosend&TESTMAIL) 							// Testnachricht?
	 {	 
		mailtosend&=~TESTMAIL;								// Testnachricht Versandaufgabe löschen
		testmail=1;														// Testmail Flag setzen
	 }	 
   else if (mailtosend&DATAMAIL)					// Messdaten Email 
   {
    delete_data();												// Messdaten im Speicher löschen
		mailtosend&=~DATAMAIL;								// Messdaten Versandaufgabe löschen
   }
   else if (mailtosend&ALARMMAIL)					// Status/Fehler Email Versandaufgabe löschen
   {
    mailtosend&=~(ALARMMAIL|SYSERROR|FULMEM|MEM95|LOWBAT);	// Fehler Versandaufgabe löschen
   }
  } // end else if state=33 Nachricht vom Server quittiert
 } while (state++<38);	// end do Zustandsfolgeschleife	
 
 connect=uartsave;									// Verbindungszustand wieder herstellen
 wait_mail=3;											// zuerst immer 3 Minuten mit nächster Email warten
 
 if ((mode&0x04)&&(connect&(UART0|USB_LINK)))		// Mail senden und Terminal verbunden
  put2str(T_LF,&T_service[3][0]); 							// Text "Email ..

 if (failure)
 {
	if ((mode&0x04)&&(connect&(UART0|USB_LINK))) puterrstr(1);	// Terminalausgabe Text "Fehler"
	gsm_power(0);																								// Modem bei Fehler aus	 
  if (!testmail)									// Fehlerhafte Test-Emails ignorieren
	 {
	  if (mailfail<20) mailfail++; 	// Fehlerzähler erhöhen, bis 20 Fehler 3 Minuten Wiederholverzögerung
    else wait_mail=60;						// nach 20 Sendeversuchen, 60 Minuten Wiederholverzögerung
	 } 
	return(0);
 }	 
 else if ((mode&0x04)&&(connect&(UART0|USB_LINK))) putln (T_ok); // Terminalausgabe Text "ok"	
 
 FIO0SET=GSM_DTR;									// DTR HI -> enable Sleep 
 mailfail=0;											// erfolgreicher Versand, reset Fehlerzähler
  
 clear_comchange ();							// Bereinige scheinbaren Schnittstellenwechsel
 
 return(state);										// Letzten gültigen Betriebsstatus zurück
}

int sendsms (uchar ausgabe)	// SMS Service konfigurieren und SMS versenden
{																			// Übergabe ausgabe 0/1 - Terminalausgabe
 uchar state=0;												// Laufvariable
 uchar failure=0;											// Fehler aufgetreten
 uint timeout=XMODEM_BLOCK_TIMEOUT/2;	// Zeitabbruch
 text *P_resp;												// Zeiger auf Modem - Antwortstring
 uchar uartsave=connect;								// Schnittstellenstatus sichern		
	
 if (connect&(UART0|USB_LINK))								// RS232 oder USB Terminal verbunden?
 {
  put2str(T_LF, T_txsms);											// Ausgabe Text "Sende SMS  
  putln(T_dot3);															// Ausgabe "..."
 }

 if (test_registration ()>0)									// Modem eingebucht?
 {	 
  if (ausgabe) connect|=UART1;								// Ausgabe der Kommandos an Terminal
  else connect=UART1; 

  do																					// Zustandsfolgeschleife
  {
   P_resp=T_nil;															// Reset Antwortstring
	 bxi=rxi;																		// flush buffer	
	 osDelay(5);			

	 if (!state)																// SMS Service Center Nummer  abfragen/setzen
   {
    
    putstr(T_csca);														// Sende AT+CSCA
    if (!isalpha(fp.smssc[0])) putstr(T_rqst);	// Keine SMS Service Center Nummer eingetragen, dann abfragen
    else
    {
     putc('=');		
		 putqstr(fp.smssc);												// Service Center Nummer in quotes senden
     newline();
    }
    P_resp=T_OKNZ;	   													// Erwarte OK Antwort
   }
   else if (state==1)													// SMS Zeichensatz festlegen
   {
    putln(T_cmgf);															// Sende AT+CMGF=1
    P_resp=T_OKNZ;	   													// Erwarte OK Antwort
   }
   else if (state==2)													// SMS Parameter festlegen
   {
    putln(T_csmp);															// Sende AT+CSMP=17,167,0,241
    P_resp=T_OKNZ;	   													// Erwarte OK Antwort
   }
   else if (state==3)													// SMS status reports festlegen
   {
    putln(T_cnmi);															// Sende AT+CNMI=2,1,0,0,0
    P_resp=T_OKNZ;	   													// Erwarte OK Antwort
   }
   else if (state==4)													// SMS Rufnummer senden
   {   
    if (isdigit(fp.smsno[0]))
    {
     putstr(T_cmgs);    
		 putqstr(fp.smsno);												// Sende SMS Nummer
		 newline();																// Zeilenabschluss
		 P_resp=T_cbbl;	   												// Erwarte > Antwort
    }
    else failure=1;   													// Keine Nummer eingetragen -> Fehlerabbruch
   }
   else if (state==5)														// SMS Text senden
   {
    connect&=~(UART0|USB_LINK);									// Nachfolgendes nicht an Terminal
    putstr (T_name);														// Ausgabe Text Viasis 3003M oder OEM Bezeichner
    putln(fp.serno); 				 										// Ausgabe Seriennummer
    put2str(T_comment,T_dpkt);									// Text "Kommentar:
    putln_ifexist(fp.comment);									// wenn gesetzt Kommentar - Standort	ausgeben
    if (smstosend&TESTSMS) putln(T_smsset);			// SMS Versand ist eingerichtet		
    if (smstosend&LOWBAT) putln(T_batlow);			// Batteriespannung niedrig
    if (smstosend&(MEM95|FULMEM)) putln(T_memfull);	// Speicher voll
    if (smstosend&SYSERROR) putln(T_syserr);		// Gerätefehler
    osDelay(10);
    putb(STRZ);																	// Nachrichtenabschluss  
    connect=uartsave;															// Ausgabeschnittstellen wieder herstellen 
    timeout=5*XMODEM_BLOCK_TIMEOUT;
    P_resp=T_OKNZ;	   													// Erwarte OK Antwort
   }

   if (P_resp!=T_nil)													// Modemantwort erwartet?
   {
    if (wait_message(P_resp, timeout)<0)
    { 
     failure=1; 																// falsche Antwort -> Fehler markieren
		 connect=uartsave;
		 if (ausgabe) sendbuf(cbuf, 64);
    }
   }

   if (failure) break;
  } while (state++ < 5); // Ende Zustandsfolgeschleife

  connect=uartsave;														// Schnittstellenstatus wiederherstellen
  if (connect&(UART0|USB_LINK))								// RS232 oder USB Terminal 
  {
   put2str(T_LF, &T_service[1][0]);						// Ausgabe Text "SMS
   if (failure) puterrstr(1);									// Text "Fehler"
   else putln (T_ok);													// Text "Ok"
  }
 } else failure=1; 
 
 wait_sms=3;											// immer 3 Minuten mit nächster SMS warten

 if (failure) 
 { 
	gsm_power(0);										// Modem bei Fehler aus	  
	if (!(smstosend&TESTSMS))				// Fehlerhafte Test-SMS ignorieren
	{
	  if (smsfail<20) smsfail++; 		// Fehlerzähler erhöhen, 3 Minuten Wiederholverzögerung
    else wait_sms=60;							// nach 20 Sendeversuchen, 60 Minuten Wiederholverzögerung
	} 	 
	return(-1);
 } // end failure
 else FIO0SET=GSM_DTR;					// DTR HI enable Sleep
 
 smstosend=0;				// Nachricht(en) wurden versandt -> löschen
 smsfail=0;					// reset SMS Versandfehler Zähler
 return(state);  		// Rückgabe letzter Zustand
}

void get_email_config (void)		// Email Konfiguration einlesen
{
 int result;

 putstr (T_mto);
 putln_ifexist (fp.emto);													// Mail to ausgeben
 putstr (T_mcopy);
 putln_ifexist (fp.emcopy);												// Mail copy ausgeben
 putparameter (T_emalarm,0,3<<16,&T_offon[fp.mailalarm][0]); // Ausgabe SMS Alarm: ein/aus

 result=ja(T_change);															// Abfrage "Ändern ...?" 
 
 if (result>0)																		// Ergebnis ja
 { 
	do 
  {		
   put2str (T_LF,T_mto);
   readline(fp.emto, sizeof(fp.emto), 'a');				// Mail to einlesen 		
	}	while (no_at(fp.emto));							// kein @ Zeichen im String an Zeichenposition > 1?
	
	do 
  {
   putstr (T_mcopy);
   readline(fp.emcopy, sizeof(fp.emcopy), 'a');								// Mail copy einlesen	
	} while (no_at(fp.emcopy));						// kein @ Zeichen im String an Zeichenposition > 1?	
	
  list_selection(T_emalarm, &T_offon[0][0], &fp.mailalarm,0x82, sizeof(T_offon[0]));	// Email Alarm j/n Abfrage
 }

 if (*fp.emto)																	// Email Adresse existiert?
	if (ja(T_mnow)>0)																// Email senden?
  {
   if (result>0) mailtosend|=TESTMAIL;						// Test Email versenden setzen
   else mailtosend|=DATAMAIL;											// Daten Email versenden setzen
   if (!(connect&(UART1|MQTT_LINK)))							// keine bestehende BT, MQTT oder GSM Verbindung auf UART1
   { 
    sendmail(7);																	// Email sofort versenden
    mailtosend&=~TESTMAIL;												// Test Email nicht dauerhaft versuchen zu senden
   }
  }
}

void get_sms_config (void)	// SMS Konfiguration einlesen
{
 int result;

 putstr (T_smssc);
 putln_ifexist (fp.smssc);												// SMS Service center
 putstr (T_smsno);
 putln_ifexist (fp.smsno);												// SMS Rufnummer
 putparameter (T_smsalarm,0,3<<16,&T_offon[fp.smsalarm][0]); // Ausgabe SMS Alarm: ein/aus

 result=ja(T_change);															// Abfrage "Ändern ...?"

 if (result>0)																		// wenn Ergebnis "Ja"
 {
  put2str (T_LF,T_smssc);
  readline(fp.smssc, sizeof(fp.smssc), 'a');			// SMS Service center Nummer einlesen
  putstr (T_smsno);
  readline(fp.smsno, sizeof(fp.smsno), ' ');			// SMS Rufnummer einlesen
  list_selection(T_smsalarm, &T_offon[0][0], &fp.smsalarm,0x82, sizeof(T_offon[0]));
 }
 
 if (*fp.smssc && *fp.smsno)											// Service und SMS Nummern eingegeben?
  if (ja(T_snow)>0) 															// SMS senden?
  {
   smstosend|=TESTSMS;														// Test SMS versenden setzen
   if (!(connect&(UART1|MQTT_LINK)))							// keine bestehende BT, MQTT oder GSM Verbindung auf UART1
   {
    sendsms(1);																		// Sofortversand
    smstosend&=~TESTSMS;													// Test SMS versenden wieder löschen
   }
  }
}

void get_apn_config (void)												// APN Konfiguration einlesen
{
 int result;
 
 putstr(T_apn);
 putln_ifexist (fp.apn);
 putstr(T_user);
 putln_ifexist (fp.user);
 putstr(T_pass);
 putln_ifexist (fp.pass);
 putparameter (T_roam,0,3<<16,&T_offon[fp.roaming][0]); // Ausgabe ROAMING: ein/aus

 if (ja(T_change)>0)																		// Abfrage "Ändern ...?"
 {     
  put2str(T_LF,T_apn); 
  readline(fp.apn,sizeof(fp.apn),'a');	 								// APN einlesen 
  putstr(T_user);
  readline(fp.user,sizeof(fp.user),'a');  							// Username einlesen
  putstr(T_pass);
  readline(fp.pass,sizeof(fp.pass),'a');								// Passwort einlesen
  list_selection(T_roam, &T_offon[0][0], &fp.roaming,0x82, sizeof(T_offon[0])); // Auswahl Roaming
  if (!(connect&UART1))																	// kein BT oder GSM?
  {
   put2str(T_LF,T_yconf); 
   putln(T_dot3); 
   result=set_gprs_config(3);														// GPRS Verbindung mit Ausgabe aufbauen
   put2str(T_LF,&T_service[0][0]);  										// Text "GPRS
   if (result<0) puterrstr(1); 													// Text "Fehler"
   else putln (T_ok);
  }
 } // end ändern	  
}

void get_server_config (void)											// Einlesen der Server Konfiguration
{
 int result;
 char *Port, *Server;

 if (fp.servertyp==SMTP) { Server=T_smtpsrv; Port=T_smtpport; }
 else { Server=T_mqttsrv; Port=T_mqttport; }  

 putstr (Server);																	// Server Text ausgeben
 putln_ifexist (fp.server);												// Server URL oder IP ausgeben
 putparameter (Port,fp.port,1<<16,T_LF);					// Port ausgeben 
 if (fp.servertyp==SMTP)													// Nur SMTP / Email
 {	
  putstr (T_ehost);																// Email Hostdomain ausgeben
  putln_ifexist (fp.ehost);
	putstr (T_euser);																// Email User prompt ausgeben 
 }
 else putstr (T_user);														// MQTT User prompt ausgeben
 putln_ifexist (fp.euser);	
 if (fp.servertyp==SMTP) putstr (T_epass);
 else putstr (T_pass);
 if (isalpha(*fp.epass)) putln(T_stars);					// Wenn nicht Null - ausgeben
 else putln(T_notcon);														// Text "Nicht gesetzt
 if (fp.servertyp==SMTP)													// Nur SMTP / Email
 {
  putstr (T_mfrom);																// Email mailfrom ausgeben
  putln_ifexist (fp.emfrom);	
 }
 
 if (connect&MQTT_LINK)														// MQTT Server ist bereits verbunden
 {
	putln (T_cmqtt); 																// Verbindungsmeldung	  
	if (ja(T_discon)>0) 
	{	
	 Con_MQTT_server(18);														// MQTT Serververbindung schließen   
	 wait_min=Def_wait;															// Warte 3 Minuten mit reconnect
	}	
 }	 
 else if (ja(T_change)>0)															// Abfrage "Ändern ...?"
 { 
  put2str (T_LF,Server);
  readline(fp.server,sizeof(fp.server),'a');	 		// Server einlesen
  result=getnumber (Port,1,0xFFFF);								// Port einlesen
  if (result>=0) fp.port=result;									// Übernehmen wenn > Null
	else putparameter (Port,fp.port,5<<16,T_nil);
	if (fp.servertyp==SMTP)
  {  
   putstr (T_ehost);							
   readline(fp.ehost,sizeof(fp.ehost),'a'); 			// Email Host einlesen
	 putstr (T_euser);	
  }
	else putstr (T_user);
	
  readline(fp.euser,sizeof(fp.euser),'a'); 				// Email User einlesen
  if (fp.servertyp==SMTP) putstr (T_epass);
	else putstr (T_pass);
	
  readline(fp.epass,sizeof(fp.epass),'*'); 				// Email Passwort einlesen
	if (fp.servertyp==SMTP)													// Server SMTP oder MQTT?	
  {
   putstr (T_mfrom);							
   readline(fp.emfrom,sizeof(fp.emfrom),'a');	 		// Email Mailfrom  einlesen
   if (!(connect&UART1))													// kein BT oder GSM?
   {
    put2str(T_LF,T_yconf); 
    putln(T_dot3);  
    result=sendmail(3);														// SMTP Konfigurieren
    put2str(T_LF,&T_service[2][0]); 							// Text "Server ..
    if (result<28) puterrstr(1);									// Text "Fehler"
    else putln (T_ok);
	 } // end if connect	 
  } // end if smtp
	else if (!(connect&UART1))											// keine aktive BT oder GSM Verbindung?																		
	{		
	 result=Con_MQTT_server(0);											// MQTT Server verbinden/testen
	 put2str(T_LF,&T_service[2][0]); 								// Text "Server ..	
	 if (result>0) putln (T_ok);													
	 else { puterrstr(0);	putnumber (-result,0); newline(); }	// Text "Fehler" 			 
  }
 }			
}

void change_radionet (uchar net)		// Funknetzeinstellung GSM / UMTS / LTE ändern
{																		// Übergabe gewünschtes Netzwerk 1/2/3 für GSM/UMTS(LTE)/Automatisch

 int result=-1;	
 uchar uartsave=connect;
 uchar netcode=0;	
	
 if (!(connect&UART1))							// wenn keine GSM oder BT Verbindung
 {	
	if (test_gsm(0)>=0)								// Modem antwortet und Netzauswahl korrekt?
	{
	 connect=UART1;										// an Modem senden	
	 putstr(T_nsel);									// Modem Kommando AT+QCFG="NWSCANMODE","
	 if (net==3) netcode=0;						// Automatische Funknetzwahl
   else if ((fp.gsm==EG91) && (net==2))	netcode=3;// Netzauswahl 2 (=LTE) bei EG91
   else netcode=net;								// Netzauswahl 1/2/3 - GSM/UMTS bei UC15 und GSM/LTE bei EG91	 
	 putc(netcode|0x30);							// Einstellung an Modem senden, wird sofort umgestellt und dauerhaft gespeichert		
	 newline();												// Zeilenabschluss
   result=wait_ok_time(XMODEM_BLOCK_TIMEOUT);
	 connect=uartsave;	
	 if (result>0)	fp.radnet=net-1;	// Einstellung speichern
	 FIO0SET=GSM_DTR;									// DTR HI - Release Modem Sleep control	
   MUX_BT_Select;										// MUX Bluetooth Leitungsauswahl	
	}	
 }
 else put2str(T_LF,T_noacc);				// kein Zugriff über GSM oder BT 
}		
	

int file_to_uc15(uint filesize, char *Pfname)				// File upload UC15 Funkmodem
{																										// Übergabe filesize - Länge Datei in Bytes
																										// 					Pfname - Zeiger auf Dateinamen		
 int result=-1;	
 uint filehandle;
 ushort page=MEASUREPAGE;							// Flash page	
 ushort blockbytes;										// Anzahl Blockbytes zu schreiben	
 ushort i;	
 uchar constate=connect;	
	
 connect|=UART1;						// Modem und Terminal
 putstr (T_fopen1);					// Command open ufs file for writing part 1				
 putqstr (Pfname);					// Send filename
 putln(T_fopen2);						// Command part2
 result=wait_message(T_fresp,XMODEM_CHAR_TIMEOUT);	// Warte auf Antwort
 if (result>0)		//		QFOPEN?
 {
	wait_line(XMODEM_CHAR_TIMEOUT); // Warte auf file handle
	filehandle=atoi(cbuf); 
	wait_ok_time(XMODEM_CHAR_TIMEOUT);  
	putstr(T_fwrite);								// AT+QFWRITE senden
  putnumber (filehandle,0);				// file handle senden
  putc(',');
  putnumber (filesize,0);					// Dateigrösse senden
  newline();
  result=wait_message(T_connect,XMODEM_CHAR_TIMEOUT);						// Warte auf "CONNECT" Antwort	 
	if (result>0) 
	{	
	 connect=UART1;																								// nur an Modem	*/
	 while (filesize>0)
	 {
		if (filesize<BLOCKSIZE)	blockbytes=filesize;								// weniger als 1k noch zu senden
		else blockbytes=BLOCKSIZE; 
		flash_pcom((uchar *)&cbuf,page,FLASH_R_ARRAY,blockbytes);		// Anzahl blockbytes nach cbuf lesen
		filesize-=blockbytes;																				
    i=0; while (blockbytes--) putb(cbuf[i++]);									// Blockbytes an Modem senden
		page+=pagepk; 																							// Flash Seiten hochzählen		 																			
	 }		 		
	 wait_ok_time(XMODEM_CHAR_TIMEOUT);					// Warte auf ok
	 putstr(T_fclose);													// AT+QFCLOSE, Datei schließen
   putnumber (filehandle,0);									// Handle senden
	 newline();																	// Kommando abschließen
	 result=wait_ok_time(XMODEM_CHAR_TIMEOUT);	// Warte auf ok
	}	
 }	
 connect=constate;						// Ausgabe Schnittstellen reset  
 
 return (result);	 
}

int cert_to_uc15 (uchar offset)							// MQTT Zertikats-File upload ins UC15 Funkmodem
{																						// Übergabe: offset - Offset im MQTT Empfangspuffer zum 1. Zertifikatsbyte
 int result=-1;	
 uint filehandle;
 uchar constate=connect;										// Ausgabe-Schnittstallen Status sichern	
 ushort i=offset; 	
	
 connect=(connect&~MQTT_LINK)|UART1;				// Empfang auf von mqbf auf rbuf umschalten  
  if (fp.gsm==EG91) send3plus();							// EG91 vorläufig "+++" senden 	
 FIO0SET=GSM_DTR;														// DTR HI -> Modem switch DATA to COMMAND Mode	
 result=wait_ok_time(XMODEM_CHAR_TIMEOUT); 
 FIO0CLR=GSM_DTR;														// DTR sofort rücksetzen, sonst UC -> Sleep
 if (result>0) 															// UC15 Data->Command Mode, OK auf Umschaltung?
 {																					 
	connect=UART1;														// Nur Modemausgabe
	putstr (T_fopen1);												// Command open ufs file for writing part 1
 	putb ('"'); 	 
  putb ('_');	 															// Underline vor Dateinamen einfügen	
  putstr (certname);												// Zertikats-Filename
	putb ('"');  
  putln(T_fopen2);													// Command part2
  result=wait_message(T_fresp,3*XMODEM_CHAR_TIMEOUT);	// Warte auf Antwort
  if (result>0)															// Ergebnis QFOPEN?
  {
	 wait_line(XMODEM_CHAR_TIMEOUT); // Warte auf file handle
	 filehandle=atoi(cbuf); 
	 wait_ok_time(XMODEM_CHAR_TIMEOUT);  	
	 putstr(T_fwrite);								// AT+QFWRITE senden
   putnumber (filehandle,0);				// file handle senden
   putc(',');
   putnumber (certsize,0);					// Dateigrösse senden
   newline();
   result=wait_message(T_connect,XMODEM_CHAR_TIMEOUT);				// Warte auf "CONNECT" Antwort	 
	 if (result>0) 
	 {		
	  while (certsize-->0)	putb (mqbf[i++]);	// Zertifikat an UC15 senden
	  wait_ok_time(XMODEM_CHAR_TIMEOUT);			// Warte auf ok - Zertifikat vom UC15 eingelesen 
	 }	
	 putstr(T_fclose);												// AT+QFCLOSE, Datei schließen
   putnumber (filehandle,0);								// Handle senden
	 newline();																// Kommando abschließen
	 if (wait_ok_time(XMODEM_BLOCK_TIMEOUT)>0)	// Warte auf ok - Datei ist zu 	
   {		 
	  putln (T_ctsw);													// Kontextumschaltung Command -> Data Mode
    if (wait_message(T_connect,6*XMODEM_BLOCK_TIMEOUT)>0)	// Erfolg, CONNECT ?
		{
		 for (i=0;i<3;i++) if (certs[i][0]==0) { memcpy(&certs[i][0], &certname[0], sizeof(certname)); break; }	// Zertifikat in Liste eintragen	
		 if (i==3) while (i) certs[--i][0]=0;	 						// Kein freier Namensplatz - alle Löschen
		 else if (mqtt_debug)
		 {			
			connect=constate&~(UART1|MQTT_LINK);						// Nur an RS232/USB Terminal
			putstr (cbuf);																	// Reconnect ausgeben 
			putstr (&certs[i][0]);													// Zertifikatsname aus Liste
			putstr (T_m_write);															// Text " written...
		 } // end if mqtt_debug	
		 connect=constate;
		 return (1);															// Erfolgreich beendet
		} // Kein 	
	 } 
  } // end if QFOPEN - Datei offen
 } // end if Umschaltung Data -> Command Mode erfolgreich 

 gsm_power(0);													// Sonst Modem aus
 connect=constate&~(UART1|MQTT_LINK);		// Ausgabe Schnittstellen reset
 return (-1); 								// Zertifikat schreiben oder Wiederaufnahme der Verbindung gescheitert
}

int activate_cert (void)										// Aktiviert im Funkmodem vorhandene neue TLS Zertifikate
{
 int result;	
 uchar constate=connect;										// Ausgabe-Schnittstallen Status sichern	
 uchar i;
	
 for (i=0;i<3;i++) if (certs[i][0]) break;	// Prüfe ob Zertifikatsnamen gespeichert	

 if (i<3)																		// Wenigstens 1 Zertifikatsnamen gefunden?
 {	 
  connect=UART1;														// Empfang auf von mqbf auf rbuf umschalten  
  if (fp.gsm==EG91) send3plus();						// Vorläufig mit defektem DTR Prototyp 	 
  FIO0SET=GSM_DTR;													// DTR HI -> UC15 switch DATA to COMMAND Mode 	
  result=wait_ok_time(XMODEM_CHAR_TIMEOUT); // Warte auf Umschalt-OK des Command mode
  FIO0CLR=GSM_DTR;													// DTR sofort rücksetzen, sonst Modem -> Sleep
  if (result>0) 														// UC15 Data->Command Mode, OK auf Umschaltung?	
  {
	 for (i=0;i<3;i++)				// Bis zu 3 Zertifikatsdateien aktivieren
   {
		result=1; 
		if (certs[i][0]) 				// Zertname existiert?
		{
		 putstr(T_fmove);				// File move command part1
     putc('"');							// File name start quotation
		 putc('_');							// Source name starts with underline
		 putstr(&certs[i][0]);	// Source name
		 putc('"');							// File name stop quotation
     putc(',');							// Delimiter
		 putc('"');							// File name start quotation		
		 putstr(&certs[i][0]);	// Destination name	
		 putc('"');							// File name stop quotation 	
		 putln(T_fmove2);				// Endstring mit Copy=0 und Overwrite=1 Flags gesetzt -> Overwrite existing file, delete source
     result=wait_ok_time(XMODEM_CHAR_TIMEOUT); // Warte auf Kommando Antwort OK
		 if (result<0) break;	
		 if (mqtt_debug)
     {			
			connect=constate&~(UART1|MQTT_LINK);				// Nur an RS232/USB Terminal	
			newline();																	// Neue Zeile	
			putstr (&certs[i][0]);											// Zertname
      putstr (T_m_activ);									 				// Text " active..."
			while (!(U0->LSR&THRE));										// Daten an Terminal gesendet 
			connect=UART1;	 														// Zurück auf Modemverbindung
		 }	 
		 certs[i][0]=0;					// Zertifikatsnamen löschen, da bearbeitet 	 
		}	// end if Zert existiert		
   } // end for jedes Zert
	 
	 if (result>0) // Wenigstens ein Zertifikat erfolgreich umbenannt?
	 {
		putln (T_ctsw);													// Kontextumschaltung Command -> Data Mode
    if (wait_message(T_connect,6*XMODEM_BLOCK_TIMEOUT)>0)	// Erfolg, CONNECT ?
		{
		 connect=constate;	
	   return (1);			
		}	
   }		 
  }	// end if Command Mode 
 } // end if Zert gefunden	
 else return(0);												// kein Zertifikatsname gespeichert
 gsm_power(0);													// Sonst Modem aus
 connect=constate&~(UART1|MQTT_LINK);		// Ausgabe Schnittstellen reset
 return(-1);									// Fehler beim Umbenennen des Zertifikats oder Wiederaufnahme der Verbindung
}

int Download_firmware	(const char *url, uchar mqtt_connect)		// Verbinde mit HTTP Server vt.gonicus.de  und lade Firmware herunter
{																					// Übergabe mqtt_connect 0/1, 1=wenn noch MQTT Server-Verbindung besteht
																					// Übergabe s - Zeiger auf URL String
																					// Rückgabe Erfolg/Misserfolg 1/-1 
 uint timeout=XMODEM_BLOCK_TIMEOUT;				// Antworttimeout
 uchar state=9;														// Zustandsvariable, beginne erst bei 9 wg. GPRS states
 uchar failure=0;													// Fehler aufgetreten
 text *P_resp;														// Zeiger auf Modem - Antwortstring	
 int pos;																	// Position im Puffer und Ergebnis Hilfsvariable
 int filesize;														// Dateilänge
 char *s;																	// Hilfszeiger	 

 s=(char *)url;														// Zeiger auf URL String
 if (mqtt_connect) state=10;							// Verbindung zum MQTT-Server muss noch geschlossen werden 	

 InitWatchdog(LONG_WD_32);								// langes Watchdog Interval auf 32 Sekunden setzen
 concpy=connect;													
	
 connect=UART1;														// Ausgabe an Modem				
	
 do																				// Zustandsfolgeschleife
 {
  P_resp=T_nil;														// Reset Antwortstring
	bxi=rxi;																// flush buffer 
	ResetWDT();															// Watchdog reset 
  
  if (state==9) {													// GPRS Verbindung aktivieren
	 connect=concpy|UART1;										
	 if (set_gprs_config(0)< 0) failure=1;	// APN GPRS Verbindung nicht erfolgreich? -> Fehlerabbruch
	 else send_local_ip (); 								// sende IP an Terminal 	
	 state+=2;															// Trennen der MQTT Serververbindung überspringen	
  }
	else if (state==10)											// Modem Data in Command Mode umschalten  
  {
   connect=UART1;													// keine Terminalausgabe mehr
	 send_mqtt_disconnect();	
	 mqtt_com=0;														// Reset MQTT Nachrichtenempfang
	 mqtt_manual=0;													// reset manueller Betrieb
	 mqtt_state=DISCONNECT;									// MQTT Status Kommunikation beendet	
	 concpy&=~MQTT_LINK;										// MQTT Server Verbindung geschlossen		
	 	   		
    if (mqtt_debug) 											// MQTT Debug Mode oder Test?		
	  {	 
		 connect=concpy&~UART1;								// Nicht an Modem senden 
	   newline();	
     putln (T_discon);										// Disconnect ggf. an Terminal 
	  }	
	  connect=UART1;

	  if (fp.gsm==EG91) P_resp=T_noca;			// EG91 warte auf "No Carrier"
		else
		{	
		 FIO0SET=GSM_DTR;											// DTR HI -> Modem switch DATA to COMMAND Mode	
     P_resp=T_OKNZ;	   										// Erwarte OK Antwort bei DTR Umschaltung
		}		
  }

  else if (state==11)											// TCP/IP Verbindung zum MQTT-Server schließen
  {
	 FIO0CLR=GSM_DTR;												// DTR sofort rücksetzen, sonst UC15 -> Sleep	
	 timeout=5*XMODEM_BLOCK_TIMEOUT;	
	 connect=UART1;													// keine Terminalausgabe mehr			
 	 putstr (T_sslclose);										// verschlüsselte Verbindung AT+QSSLCLOSE 
	 putstr("=1,2"); 												// UC15, connectID und socket timeout 2s senden
	 newline();															// Kommandozeilenabschluss senden	
   P_resp=T_OKNZ;	   											// Erwarte OK Antwort		   	
  }
	else if (state==12) {										// Kontext für https Firmware Download Server erstellen
	 if (mqtt_debug) 
	 {
		connect=concpy;  
		newline();														// Leerzeile 
		putstr(T_confws);											// Ausgabe Text "Verbinde mit Firmware Server  
		putln(T_dot3);												// Ausgabe "..."	
		connect=concpy|UART1;	
	 }	 
   putstr(T_htctxid);											// Konfiguriere http context id
	 putc('1');															// = 1
	 putstr(T_htrsphd);											// Konfiguriere http response header
	 putc('0');															// = 0
	 putstr(T_htrqhd);											// Konfiguriere http request header
	 putc('0');															// = 0	
	 putln(T_htsslctx);											// Konfiguriere SSL context id
	 timeout=XMODEM_BLOCK_TIMEOUT;	
	 P_resp=T_OKNZ;		 											// Erwarte "OK" Antwort		
  }
	
  else if (state==13)	{										// Konfiguriere Verschlüsselung und Server Zertifikat am UC15
	 putln(T_sslcfg0);		
   state++;																// Ohne Zertifikate weiter mit 15			
	 P_resp=T_OKNZ;	}	 											// Erwarte "OK" Antwort	
	
	else if (state==14)	{										// Konfiguriere Client Zertifikat und Schlüssel
		putstr(T_sslcfg2);										// Konfiguriere Client Zertifikat
		putstr(fp.serno);					
		putstr(T_sslcfg3);										// Konfiguriere Client private key
		putstr(fp.serno);											
		putln(T_sslcfg4);
		P_resp=T_OKNZ;	}											// Erwarte "OK" Antwort  		 
    
	else if (state==15)	  {									//    
	 putstr(T_htcfg2); 											// Kommando	"AT+QHTTPURL=" senden	
	 pos=0;	
   while (*s++) pos++;										// URL Stringlänge ermitteln							
	 putnumber (pos,0);											// URL Stringlänge senden
   putc(',');		
   putln("80");														// 80 Sekunden	 	
	 P_resp=T_connect;											// UC15 antwortet sofort mit CONNECT  
  }
	
	else if (state==16)	{									
	 putln(url);														// URL senden	
	 P_resp=T_OKNZ;	}												// Erwarte "OK" Antwort
	
	else if (state==17)	{										// GET request Kommando senden
	 putstr(T_htget);												// Kommando	"AT+QHTTPGET=" senden   		
	 putln("120");													// Sekunden maximum response Zeit
   P_resp=T_OKNZ;		 
  }
	
	else if (state==18)	{										// GET Ergebnismeldung auswerten
	 if (wait_line (20*XMODEM_BLOCK_TIMEOUT)>0)		// Download bzw. Ergebnis Timout?
   {
		putcbuf_line (0); 
		newline(); 
		 pos=compare ("GET: ",cbuf,0,cind); 				// prüfe ob "+QHTTPGET: " Teilstring Antwort?
		if (pos<0) failure=1;
		else
		{
		 if (cbuf[pos]!='0') failure=2;							// Nicht Null Fehlerabbruch
		 else
     {
			if (atoi (cbuf+pos+2)!=200)	failure=3;		// Serverantwort nicht 200 = ok? 
			else filesize = atoi (cbuf+pos+6);				// Content length = Dateilänge lesen
      if (filesize>FIRMWARE_MAX_FILESIZE) 			// Firmware Datei zu lang
			{
			 protocol(XMODEM_FILE_TOO_LONG);					// Fehler protokollieren
       failure=4;				
			}		
		 }		
		}	
	 } else failure=1;											// Download timeout 
	} // end state 18
	
	else if (state==19)	{										// Speichere Firmware Datei "firmware.bin" im UC15 Flash 
	  putstr(T_htrdfile);										// Kommando	"AT+QHTTPREADFILE=" senden
		putqstr(T_fwfile);										// Firmware Dateiname, z. B. firmware.bin
		putln (",80");												// 80 Sekunden http session closure time
    timeout=40*XMODEM_BLOCK_TIMEOUT;			// 120 Sekunden Timeout
		P_resp=T_OKNZ;												// Erwarte "OK" Antwort
  }
	
	else if (state==20)	{												// Prüfe AT+QHTTPREADFILE Ergebnis
	 if (wait_line (20*XMODEM_BLOCK_TIMEOUT)>0)
	 {	 
		putcbuf_line (0);
    newline();	 
	  pos=compare ("FILE: ",cbuf,0,cind);			// Antwort enthält +QHTTPREADFILE: Teilstring?
	  if (pos<0) failure=5;
	  else if (cbuf[pos]!='0') failure=6;			// Nicht Null Fehlerabbruch
	 }	 
	 else failure=7;
	} // end state 20
 
  else if (state==21)												// Deaktiviere PDP Kontext						
  {	 
   putstr(T_qideact);												// "AT+QIDEACT"
   putstr("=1");														// Ergänze Kontext ID	
   newline();
	 P_resp=T_OKNZ;
   timeout=5*XMODEM_BLOCK_TIMEOUT;					// Warte bis zu 15 s	
  }	 
	
	if (P_resp!=T_nil)											// Modemantwort erwartet?
  { if (wait_message(P_resp,timeout)<0) failure=1; 	// falsche Antwort -> Fehler markieren
	  if (state>=14)  {											// ab State 14
		 if (mqtt_debug)											// Ausgabe auch an angeschlossene Terminal(s)	
			 putcbuf_line (2); 	}								// Pufferzeile an Terminal ausgeben
	}	
	
  if (failure) break;											// Fehler -> Abbruch		
	
	osDelay(10); 											
 } while (state++ < 21);								// end while Zustandsfolgeschleife
 
 connect=concpy;												// Schnittstellenzustand wiederherstellen
 
 if (failure)								// Fehler aufgetreten
 { 
	if (mqtt_debug)
  {		
   newline(); 
   putnumber(failure,0);
   putc(' ');
	 putnumber(state,0); 
	}	
  gsm_power(0);													// Modem bei Fehler aus	
	return (-1); 
 }
 
 return (1);
}

int firmware_to_flash (int filecrc)		// Transfer Firmware vom Funkmodem in den Flash
{																			// Prüfe Datei CRC auf Übereinstimmung					
 int pos;															// Hilfsvariable Zeichenposition im String
 int filehandle=-1;										// Handle für geöffnete Firmware Datei	
 uint filesize=0,fbytetoread=0;				// Größe der Firmware Datei / Anzahl Dateidaten zu lesen
 int bytes_to_read=0;									// Anzahl Bytes zu lesen
 int bytes_read=0;										// Anzahl Bytes gelesen
 int flashcrc=0;											// CRC Prüfsumme der gespeicherten Datei im Flash
 uint blno=0;													// Anzahl gelesene 1k Blöcke
 uchar retry=3;												// Anzahl Block Lese-Wiederholversuche			
 uchar concpy=connect;	
	
 connect=UART1;												// an Modem senden
	
 putln (T_at);												// Text "AT"
 if (wait_ok_time (XMODEM_CHAR_TIMEOUT)<0)	return(-1); 		// Modem bereit, auf AT Antwort "OK" empfangen?
	
 if (mqtt_debug) { connect=concpy; putln (T_cp_fw); }		// Text "Copy firmware..

 connect|=UART1;												// Command Ausgabe an Modem		
	
 putstr (T_fopen1);											// AT command Datei öffnen	AT+QFOPEN=
 putqstr(T_fwfile);											// Firmware Dateiname, z. B. firmware.bin
 putln (",2");													// Zum Lesen öffnen
 if (wait_ok_time (XMODEM_BLOCK_TIMEOUT)>0)			// Warte auf OK Abschluss
 {
  pos=compare(T_fresp,cbuf,0,cind);			// Suche +QFOPEN Antwort	
  if (pos>0)														// +QFOPEN gefunden
  {		
	 filehandle=atoi(cbuf+pos);						// Dateihandle merken 	
   putcbuf_line (2);										// Antwort an Terminal

   putstr (T_qflst);										// Kommando Datei listen "AT+QFLST="	
   putqstr(T_fwfile);										// Firmware Dateiname, z. B. firmware.bin	
   newline();														// Befehlsabschluss
	 wait_ok_time (XMODEM_CHAR_TIMEOUT);	// Warte auf OK Zeile
   pos=compare(",",cbuf,0,cind);				// Suche nach Kommastelle	 
   putcbuf_line (2);											
   if (pos>0) 													// Komma gefunden			
	 {	
	  filesize=atoi(cbuf+pos);						// Dateilänge lesen		
		if (filesize)												// Keine Null Byte lange Datei
		{		
		 fbytetoread=filesize;							// kopieren 	
		 putstr (T_qseek);									// Kommando Dateizeiger "AT+QFSEEK=
		 putnumber(filehandle,0); 					
		 putln (",0,0"); 										// auf Dateianfang	
		 if (wait_ok_time (XMODEM_BLOCK_TIMEOUT)>=0) do		// Datei Zeiger positioniert, dann Daten Blöcke lesen
		 {		
			if (fbytetoread<BLOCKSIZE) bytes_to_read=fbytetoread;	// Dateirest < 1k zu lesen?
			else bytes_to_read=BLOCKSIZE;			// 1024 Bytes zu lesen		  
			putstr (T_fread);									// Kommando Datei lesen "AT+QFREAD=
			putnumber(filehandle,0);					// Dateihandle als ASCII senden
			putc(','); 												// Trennzeichen
			putnumber(bytes_to_read,0);				// Anzahl Bytes zu lesen
			newline ();												// Kommandoabschluss				 
			wait_line (XMODEM_CHAR_TIMEOUT);	// Warte auf CONNECT und Anzahl Lesebytes Zeile
			bytes_read=uart_read(bytes_to_read, XMODEM_CHAR_TIMEOUT,0); 
			if(bytes_read==bytes_to_read)	 		// Anzahl bytes_to_read vom Modem empfangen
			 flash_pcom((uchar *)cbuf,(blno*pagepk),FLASH_ERASE_WRITE_1,bytes_read);	// Pufferdaten nach Flash schreiben
			else if (retry--)									// Noch Lese-Wiederholversuche
			{
			 connect=UART1;	
			 putstr (T_qseek);								// Kommando Dateizeiger "AT+QFSEEK= positionieren
			 putnumber(filehandle,0); 	
			 putc(',');
			 putnumber (blno*BLOCKSIZE,0);		// Dateioffset	
       putln(",0");											// vom Datei Anfangsposition
			 if (wait_ok_time (XMODEM_BLOCK_TIMEOUT)<0) break; // kein Erfolg -> Abbruch	
			 continue;												// Selben Block nochmal lesen	
			}		
			else break;
			retry=3;													// Block mit Erfolg gelesen Wiederholzähler reinit		
			wait_line (5);										// Warte auf OK	Abschluss des Leseblocks	
			connect&=~UART1;
			if (!blno++) { putstr ("Write"); putstr (T_block); } 
			else
			{	
			 connect=concpy;
			 delchar(4);	
			 putnumber(blno,4);	
			}	
			fbytetoread-=bytes_read;					//
			connect=UART1;										// Nur Protokoll-Ausgabe des ersten Blocks
		 } while (fbytetoread);							// end do block copy while		 
	  } // end if filesize > 0
   } // end if filesize pos>0 
	} // end if +QFOPEN pos>0 
 } //end if QFOPEN OK	
 
 connect=concpy;
 newline ();													// Neue Zeile
 connect|=UART1;
 putstr (T_fclose);										// Datei schließen
 putnumber(filehandle,0);						  // Dateihandle als ASCII senden 
 newline ();													// Kommandoabschluss
 if (wait_ok_time(XMODEM_BLOCK_TIMEOUT) > 0)	// OK, Datei zu?
 {	
	if (mqtt_debug) { connect=concpy; putln(T_fw_crc); connect|=UART1; } 
	fbytetoread=filesize;	blno=0;				// Init do variables for crc check	 
	do 
	{	 
	 flash_pcom((uchar *)cbuf,pagepk*blno,FLASH_R_ARRAY,BLOCKSIZE);			// Atmel Flash Seite(n) mit CRC lesen
	 if (fbytetoread>BLOCKSIZE) bytes_to_read=BLOCKSIZE;								// Anzahl Flash Daten zu lesen > 1k
   else bytes_to_read=fbytetoread; 																				
	 flashcrc=crcgen((uchar *)cbuf,bytes_to_read,flashcrc);							// CRC über gelesene Flash Daten bilden
	 fbytetoread-=bytes_to_read;																				// Anzahl restlicher Lesedaten berechnen
	 if (!blno++) { putstr ("Read"); putstr (T_block); }								// Meldung gelesener Datenblöcke
	 else
	 {	
		connect=concpy;
		delchar(4);	
		putnumber(blno,4);	
	 }	
	}		
	while (fbytetoread);								// Solange Daten zu lesen		
 } 

 connect=concpy;
 
 putstr (T_crc);
 putnumber (flashcrc,0x80);
 
 if(!filecrc) return (blno);						// CRC=0 übergeben - Testfunktion
 if (flashcrc==filecrc) return (blno);	// Prüfsummen stimmen überein
 
 putln (" Error");
 puterror (CRC_ERROR,-1);							// Fehler protokollieren
  
 return(-1);											
}




