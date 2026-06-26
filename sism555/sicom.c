//-----------------------------------------------------------------------------
//  FILE: sicom.c				PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Menüsteuerung, high-level I/O Routinen
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H3
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   27.01.2015
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//              	27.01.2015: File creation		
//								22.04.2015:	Hardware version H1 -> H2
//								14.07.2015:	Hardware version H3
//-----------------------------------------------------------------------------

#include "hard.h"
#include "i2cm.h"
#include "flash.h"
#include "sio.h"
#include "fio.h"
#include "btio.h"
#include "gsmio.h"
#include "gpsio.h"
#include "mqtt.h"
#include "libtool.h"
#include "rtc.h"
#include "xmodem.h"
#include "ramcode.h"
#include "sictst.h"
#include "sictxt.h"
#include "string.h"
#include "dtc_codes.h"



void firmware (void)		// Firmware Empfang, Prüfung und IAP Programmierung 
{	
 int blno;					// Hilfsvariable Blocknummer-/Fehlerrückgabe
 int fehler=0;			// Hilfsvariable Fehlerrückgabe
 int i;							// Laufvariable
 	
 Ledaus(); 					// Led abschalten
	
 if ((flashsize==0)||((flashsize>8)&&(flashsize<=128))) puterror (DTC_SI_NO_FLASH, -1); // Flash fehlt oder zu klein
	
 putln (T_fsend);											// Text "Send file with Xmodem (1k)...
 delete_protocol();										// Protokollzeiger im Parametersatz löschen
 delete_data();												// Messdatenzeiger im Parametersatz löschen	
	
 if ((blno=xmodem_receive(FIRMWARE_MAX_FILESIZE,0))< 0)		// Empfang 1k Xmodem Daten
  puterror (DTC_SI_XMODEM_ERR, blno);								// Xmodem Fehlernummer als Detail ausgeben
 else  
 {    
  if (file_is_firmware())				// Prüfe ob es sich um Firmware Datei handelt
  {   
   put2str(T_rdat,T_check);			   // Text "Datei empfangen. Checking Firmware ..."
 
   osDelay(50);	

   for (i=0;i<blno;i++)					// CRC eines jeden Datenblock im Flash prüfen
   {	
    flash_pcom((uchar *)cbuf,pagepk*i,FLASH_R_ARRAY,BLOCKSIZE+2);		// Atmel Flash Seite(n) mit CRC lesen   	 
    if ((fehler=crc((uchar *)cbuf, BLOCKSIZE+2)) != 0) break;				// CRC Überprüfung Flash Abbild	 	   	 
   }  

   if (!fehler) 	// kein CRC Fehler?
   {
    putln (T_ok);
    if (ja(T_repro)>0)								// Abfrage "Reprogrammieren...	 
    {     
		 if (connect&USB_LINK) { newline(); putln (T_usbdis);	osDelay (100);	} // Hinweis "Trenne USB				 		
		 iap_programming (blno);							// Reprogrammierung Erfolg -> System Reset		 	
     puterror(DTC_SI_IAP_PROG_ERR, -1); 		// Hier sollte der Programmzeiger nie ankommen -> IAP Programmierfehler
    }
   } // end if kein CRC Fehler
   else puterror (DTC_SI_CRC_ERR, -1);							// CRC Fehler ausgeben
  } // end if Firmware ist ok
  else  puterror (DTC_SI_NO_FIRMWARE, -1);	// Keine Firmware Fehler ausgeben
 }   
}

void identify (void)		// Ausgabe Geräteidentifikation
{
 if (fp.ex12==4) putstr(T_viatext);					// Text "Sistext" statt "viatext" - wird für altes viasis 3002 verwendet
 else 
 {	 
  putstr(T_ident); 							 								// Text "viasis3004"
  if (fp.ex12==2) putstr(&T_12mode[2][0]); 			// .. "Plus"
  else if (fp.ex12==3) putstr(&T_12mode[3][5]);	// .. "FT"	
 }	 
#if (VSPCAM)	
 if (fp.vspcam) 
 {	 
	if (fp.ex12>1) putc(' ');  			
	putstr (T_vsc);									// .. "VSC" viaspeedcam Option aktiviert
 }	 
#endif
 newline();	
}

void speed_setting_menu (void)				// Menü Optionen Geschwindigkeitsanzeige
{
 uchar item='5';						// 4 Menüpunkte initial - 5. Zurück
 newline();	
 put2str(T_vopt,T_dpkt);																									// Text "Optionen Geschwindigkeitsanzeige...
 putparameter (T_nk,0, (3<<16)|('1'<<8), &T_offon[fp.nk][0]);							// Parameter Nachkommastelle ausgeben	
 putparameter (T_vmin,fp.vmin[dset],(1<<16)|('2'<<8),&T_uall[fp.mph][0]);	// Ausgabe Mindestgeschwindigkeit
 putparameter (T_vmax,fp.vmax[dset],(1<<16)|('3'<<8),&T_uall[fp.mph][0]);	// Ausgabe Maximalgeschwindigkeit	
 putparameter (T_vblk,fp.vblk[dset],(1<<16)|('4'<<8),&T_uall[fp.mph][0]);	// Schwelle blinkende Anzeige	
 
 if (fp.farben==3) 				// Mischfarbe zulässig?
	 putparameter (T_col2,fp.vmix[dset],(1<<16)|(item++<<8),&T_uall[fp.mph][0]);	// Schwelle Farbumschaltung lesen
	 
 if (fp.farben>1)					// Mehr als eine Farbe definiert?
	 putparameter (T_vcolor,fp.vcol[dset],(1<<16)|(item++<<8),&T_uall[fp.mph][0]); // Schwelle Farbumschaltung lesen
 	
 select (item,1);				// Menüauswahl senden
}

void parametermenue	(void)	// Parametermenü
{
 putstr(T_optmenu);																								// Text Parametermenü
 putparameter (T_pset,dset+1,(1<<16)|('1'<<8),T_nil);							// Ausgabe Nummer des angezeigten Parametersatz
 put_menuitem (T_tmen, '2');																			// Menüpunkt "2. Zeitplanung .. ausgeben
 putparameter (T_bdir,0,(3<<16)|('3'<<8),&T_offon[fp.bdir][0]); 	// Ausgabe Status bidirektionale Erfassung
 put_menuitem (T_vopt, '4');																			// Optionen Geschwindigkeitsanzeige
 putparameter (T_brght,0,(3<<16)|('5'<<8),&T_brsel[fp.hoff][0]);	// Anzeigeoptimierung ausgeben	
 put_menuitem (T_ext, '6');																				// Menüpunkt "Schaltschwellen Erweiterungen.. ausgeben	
 putparameter (T_rsens,20*(fp.sense+1),(1<<16)|('7'<<8), T_perc);	// Parameter Radar sensitivity 0...4 für 20% - 100%	

 select ('8',1);				// Menüauswahl senden
}

void info_interfaces (void)		// Ausgabe bei poweron gefundener Interfaces
{
 put2str (T_inter,T_dpkt);							// Text "Schnittstellen"
	
 if (interfaces&BT_LINK) 								// Bluetooth Modem gefunden?
 { putstr(T_bt); 												// Text "Bluetooth..
	 if (fp.btmodem==Roving) putstr (T_rn4678);
	 else if (fp.btmodem==Laird) putstr (T_laird);
   if (interfaces&(USB_LINK|GSM_LINK))	// USB oder GSM installiert?
		 putstr(T_col);											// Trennzeichen anfügen
   else newline ();											// sonst neue Zeile
 }		
 if (interfaces&USB_LINK)								// USB vorhanden?
 {
  putstr(T_usb); 												// Text "USB	
	if (interfaces&GSM_LINK) 							// GSM installiert?
		putstr(T_col);											// Trennzeichen anfügen
  else newline ();											// sonst neue Zeile
 }
 if (interfaces&GSM_LINK) 							// GSM/GPRS gefunden?
 {	 
	 putstr(T_gsm+1);	 										// Text "GSM/GPRS
	 if ((fp.gsm==EG91)||(fp.gsm==UC15))
	 {
		putc('-');
    if	(fp.gsm==EG91) putstr	(T_lte);
		if	(fp.gsm==UC15) putstr	(T_umts);	
	 }
	 if (interfaces&GPS_LINK)							// GPS gefunden? 
		 put2str (T_col,T_gps);							// Trennzeichen und Text "GPS..
	 newline();		 												// Leerzeile
 }	 
}

void gsmmenu (void)		// GSM/Email Menü
{
 uchar i;												// Laufvariable
 char *Tp;	
	
 putstr(T_gsmmen);							// Text Parametermenü
 put_menuitem (T_tmen, '1');		// Menüpunkt "1. Zeitplanung .. ausgeben
 put_menuitem (T_pinno,'2');		// Menüpunkt "2. Pinnummer .. ausgeben
 putparameter (&T_service[2][0],0,(3<<16)|('3'<<8),&T_server[fp.servertyp][0]);	// Eingestellter Server
  for (i=0;i<=3-fp.servertyp;i++)	// 3 oder 4 Menüpunkte Konfiguration APN, SMS, Server, Email nur bei SMTP
 {
  put_menuitem (T_yconf,(i+4)|0x30);	// Text "Konfiguration "
	if (i==2) { putstr (&T_server[fp.servertyp][0]); putc ('-'); }	// Server SMTP oder MQTT 
  putstr(&T_service[i][0]);			// Texte "SMS ...  Server
 }
 if (fp.radnet==2) Tp=(char *) &T_auto;														// Automatische Netzauswahl
 else 
 {
	if ((fp.radnet==1)&(fp.gsm==EG91)) Tp= (char*)&T_nets[2][0]; 		// LTE
	else Tp= (char*)&T_nets[fp.radnet][0];													// GSM und UMTS
 } 	 
 putparameter (T_mfstd,0,(3<<16)|(((4+i++)|0x30)<<8),Tp);		// Automatische Netzauswahl ausgeben
 select((i+4)|0x30,1);					// Menüauswahl senden
}

void infomenu (uchar menu)	// Information als Menü oder Email Text ausgeben
{														// Übergabe: menu - 0/1 wenn Ausgabe für Email/Menü erfolgen soll 
 uchar akt_pset=0;					// Aktiver Parametersatz
 uchar val;	

 if (menu) newline();
 putln (T_infomenu);				// Text	"Information:.	
 putmstr(L_info);				
 putstr(fp.hwVersion);											// Hardware Revision	
 put2str(T_serialno,T_dpkt);								// Text "Seriennummer...	
 if (isalpha(*fp.serno))  putstr(fp.serno);	// Gültige Seriennummer existiert? Ja, ausgeben 
 newline();																	// Leerzeile
 if (menu) putparameter (T_betr,fp.hcount,(5<<16),T_h);	// Anzahl Betriebsstunden ausgeben	
 if (tasks&MESSTASK) akt_pset=pset+1;				// Abfragezeit ist Messzeit? Ja, task=Parametersatz+1  
 putparameter (T_parakt,akt_pset,(5<<16),T_nil);	// Aktiven Parametersatz oder Null ausgeben   
 if (isalpha(*fp.comment)) 
 {
  put2str(T_comment,T_dpkt);		// Text "Kommentar: ..
  put2str(fp.comment, T_LF);		// Kommentar/Messort
 }
 info_interfaces ();						// Art installierter Interfaces ausgeben
  
 if (menu)
 { 
  newline();			// Leerzeile
  put_menuitem (T_protocol, '1');	// Menütext "1. Protokoll...
  put_menuitem (T_comment, '2');	// Menütext "2. Anmerkung...
 }
 else if (fp.gps && fp.gpsanz)		// GPS Module und Position vorhanden?
 {
	put2str(T_gps,T_dpkt);
  send_gps_position (fp.gi);			// Aktuelle GPS Einzelposition ausgeben	 
 }	 
 
 send_date_time (0,'3'*menu);				// Datum ausgeben
 putstr(T_col);		 
 if (DOW<7) putstr(&T_tag[DOW][0]);	// Wochentag gültig? Ja, ausgeben
 send_date_time (1,'4'*menu);				// Zeit ausgeben
 if (menu) 
 {
	put_menuitem (T_tzone, '5');			// Menüpunkt "5. Zeitzone
	putstr(T_dpkt); 
	if (fp.utc<0) { val=-fp.utc; putc('-'); } // Zeitzone negativ Vorzeichen ausgeben
	else val=fp.utc;														
  putnumber (val,0);	 							// Zeitzone ausgeben
  	 
  putparameter (T_somwin,0,(3<<16)|('6'<<8),&T_zone[fp.s_w_zeit][0]); // Menütext "5. Sommer-/Winterzeit	
 }	 
}	

void werkeinstellung (uchar gesamt)		// Ausgabe der Werksparameter
{							 												// Übergabe gesamt - 1/0 für alle/kurze Parameterauswahl
 // uchar i;				// Laufvariable

 putmstr(L_conf);															// Text "Configuration ...Serial number: ...
 if (isalpha(*fp.serno))  putln(fp.serno);		// Gültige Seriennummer existiert? Ja, ausgeben
 else newline();	
 if (gesamt) putparameter(T_hwrev, 0, (7<<16),fp.hwVersion); 		// Harware Revision ausgeben		
 putstr(T_sprache+1);									// Text "Language: ...
 putln(&T_spritm[fp.sprache][0]);			// Gewählte Programmsprache
 if (gesamt) putparameter(T_sende,fp.TxF,(1<<16),T_MHz); 	// Parameter Sendefrequenz
 putstr(T_dimm);											// Dimm/PWM Modus für LED
 putln(&T_dimsel[fp.pwm][0]);
 if (gesamt) 
 {
	putparameter(T_battoff,fp.uoff,(1<<16),T_LF);		// Batteriespannungsoffset 
  putparameter(T_parsets,fp.psets,(1<<16),T_LF);	// Anzahl Parametersätze
	putparameter(T_ledcol,fp.ledcode,(1<<16),T_LF);  // Farbcode Led Farben 
 }
 putparameter(T_colors,fp.farben,(1<<16),T_nil);								// Anzahl Led Farben
 if (fp.ledspot) put2str(T_col,T_spot);					// Text ", Led Spot" 
 put2str(T_LF,T_12V);														// Text "12V extern..
 putln(&T_12mode[fp.ex12][0]);									// Text "aus/ein/Plus/Plus FT" 
 if (fp.usb)															// USB installiert?
 {
  putstr(T_usb); 													// Text "USB
  if (fp.gsm|fp.btmodem) putstr(T_col);		// GSM installiert, dann Trennzeichen
  else newline ();												// sonst neue Zeile
 }				
 if (fp.btmodem) 																// Bluetooth Modem installiert?
 { 
  putstr(T_bt); 													// Text "Bluetooth..
	if (fp.btmodem==Roving) putstr (T_rn4678);
	else if (fp.btmodem==Laird) putstr (T_laird); 
  if (fp.gsm) putstr(T_col);							// USB oder GSM installiert, dann Trennzeichen
  else newline ();												// sonst neue Zeile
 }
 if (fp.gsm) 
 {	 
	if (fp.gsm==EG91) putstr(T_eg91);
	else if (fp.gsm==UC15) putstr(T_uc15);
	else putstr(T_gsm+1);										// GSM gefunden? Ja -> Text "GSM
	if (fp.gps) put2str(T_col,T_gps);				// GPS installiert, Ja -> Trennzeichen und Text "GPS"
	newline (); 														// Neue Zeile
 }	 
 if (fp.turnsw) putln(T_turn); 						// Drehschalter? Ja -> Text "Turn switch
#if (VSPCAM)	 
 if (fp.vspcam)														// viaspeedcam Option
 {
	putstr(T_spcam); 
	putln(&T_vspcam[fp.vspcam][0]);					// Text "500ms/250ms" 
 }	 
#endif
}

void addplus_display_pages (void)			// Schaltergruppe Anzeigeseiten für Viasis Plus hinzufügen			
{
 uchar i;															// Laufvariable
	
 memcpy(&fp.swgrname[0][0], &T_swgrp[0][0], SWGLEN);						// Gruppentext "Anzeigeseiten..." kopieren
 for (i=0;i<4;i++)																							// Schalterausgänge für 4 Anzeigseiten 
 {
	memcpy (&fp.swname[i][0],&T_swtyp[0][0], SWLEN);							//  Name Schaltausgang Anzeigeseite kopieren 	
  fp.swno[i]=i+1;										// Schalternummer									
	fp.swexp[i]=1;										// Expander	
	fp.swport[i]=24+i;								// Port 8...11 invertiert	
 }	// end 4 Schaltausgänge definieren	
 fp.nosw[0]=4;											// 4 Anzeigeseiten in 1. Schaltergruppe konfiguriert
 fp.swgrp=1;												// eine Schaltergruppe konfiguriert
 set_default_port ();								// Ändert default I2C Expander Porteinstellung
 reset_switches();									// Reset alle Schalter XXX
} 

void Werkskonfiguration (void)			// Gerätekonfiguration einlesen
{

 int dummy;														// Hilfsvariable	


 InitWatchdog(LONG_WD_32);						// langes Watchdog Interval auf 32 Sekunden setzen	
 werkeinstellung(1);									// Alle Werkparameter ausgeben

 if (ja(T_cconf)<=0) return;					// Abfrage "Change configuration (y/n)?"
 
 put2str(T_serial,T_ist);										// Text "Seriennummer = ...
 dummy=getline(cbuf,SERNO_LEN,'a');					// bis zu 8 alphanumerische Zeichen lesen
 if (dummy==1) fp.serno[0]='\0';						// Seriennummer löschen 
 else if (dummy==SERNO_LEN) 								// 8 Zeichen gelesen?
  memcpy(fp.serno,cbuf,SERNO_LEN+1);				// Ja, mit Terminierung übernehmen
 put2str(T_hwrev,T_ist);										// Text "Hardware:...
 dummy=getline(cbuf,sizeof(fp.hwVersion)-1,'a');							// alphanumerische Zeichen lesen
 if (dummy>1) memcpy(fp.hwVersion,cbuf,sizeof(fp.hwVersion));	// Zeichen übernehmen
 list_selection(T_sprache, &T_spritm[0][0], &fp.sprache, ANZSPRACHEN, sizeof(T_spritm[0]));	// Listenauswahl Anzeigesprache
 dummy=getnumber(T_sende, Min_TXF, Max_TXF);		// Sendefrequenz einlesen
 if (dummy >0) fp.TxF=dummy;					
 if (getuchar(T_battoff,0,10,&fp.uoff)<0) newline();												// Batteriespannungsoffset einlesen
 list_selection(T_dimm, &T_dimsel[0][0], &fp.pwm, 3, sizeof(T_dimsel[0]));	// Listenauswahl PWM modi
 getuchar(T_parsets,1,MAX_PARSET,&fp.psets);																// Anzahl Parametersätze einlesen 
 getuchar(T_ledcol,1,0x1F,&fp.ledcode);																			// LED Farbkodierung abfragen
 getuchar(T_colors,1,MAXFARBEN,&fp.farben);																	// Anzahl LED Farben einlesen
 switch (ja(T_spot))		// Led spot
 {
  case 1:  fp.ledspot=test_ledspot(); break;	// Prüfe ob LED Spot Steuerchip erreichbar
  case -1: fp.ledspot=0; break;								// LED Spot deaktivieren
  default: newline();
 }

 dummy=fp.ex12;																// Einstellung sichern	
 list_selection(T_12V, &T_12mode[0][0], &fp.ex12, 5, sizeof(T_12mode[0]));	// Listenauswahl 12V extern
 
 if (dummy^fp.ex12)													// Einstellung geändert?
  if ((fp.ex12==2) || (fp.ex12==4))						// Einstellung auf Viasis Plus oder Viatext geändert
  {	 
	 addplus_display_pages ();										// Schaltergruppe Anzeigeseiten für Viasis Plus hinzufügen
	 if (fp.ex12==4) { fp.symgr=0; fp.symbol=0; }	// Keine Symbole mehr
  }

 switch (ja(T_usb))																			// USB (J/N)?
 {
  case 1: fp.usb=TRUE; interfaces|=USB_LINK; break;			// USB registrieren
  case -1:	{ fp.usb=FALSE;	interfaces&=~USB_LINK; }		// USB deaktivieren
 }
 
 switch (ja(T_bt))			// Bluetooth (J/N)?
 {  
  case 1: init_bluetooth(); break;											// Bluetooth konfigurieren
  case -1: { if (fp.btmodem==IF820) { bt_cmdmode(); osDelay(300); bt_release(); }  // IF820: aktive SPP-Verbindung trennen
             fp.btmodem=0; interfaces&=~BT_LINK; connect&=~(UART1|BT_LINK); }       // Modem deinstallieren
 }
 
 switch (ja(T_gsm+1))																		// GSM/GPRS (J/N)?
 {
  case 1: init_gsm(); break;														// GSM/GPRS konfigurieren
  case -1:	{ fp.gsm=0; fp.gps=0; gsm_power(0); interfaces&=~(GSM_LINK|GPS_LINK); }		// GSM und GPS deinstallieren
 }
 
 if (fp.gsm)																// GSM/GPRS vorhanden?
  switch (ja(T_gps))												// GPS (J/N)? 	 
	{
	 case 1: init_gps(); break;								// GPS abfragen/testen										
   case -1:																	// GPS deinstallieren
   { gps_power(0); 
		 fp.gps=0;
	   interfaces&=~GPS_LINK; 
	 }				
	}
 
 switch (ja(T_turn))										// Drehschalter?
 {
  case 1: init_turnsw(); break;					// Drehschalter prüfen und Positionsausgabe										
  case -1: 	fp.turnsw=0;  break;				// Drehschalter deinstalliert
	default: newline(); 
 }	 
#if (VSPCAM)
 list_selection(T_spcam,&T_vspcam[0][0],&fp.vspcam, 3, sizeof(T_vspcam[0]));	 	// viaspeedcam Optionen
#endif 
 
 if (!parameter_to_progmem())						// Schreibt Sicherungskopie der Parameter in LPC1766 Programmspeicher
    puterror(DTC_SI_PARAM_INIT, -1); // Initialisierungsfehler
}

void upload_file (void)								// Server Zertifikat über Flash ins Modem laden
{	
 int blno=0;									// Anzahl 1k Datenblöcke
 int fehler=0;								// Hilfsvariable Prüfsummenfehler	
 int filecrc=0;								// CRC16 Prüfsumme der Datei bilden		
 uint filesize=BLOCKSIZE;			// Dateigrösse in Bytes
 uchar i;											// Laufvariable
 char fname[13];							// Puffer Dateiname	8.3 + '\0' = 13 Zeichen

	
 putln (T_gsmpower);												// Text "Power GSM/UMTS modem. Wait...											
 if (modem_start () > 0)										// Funkmodem antwortet?
 {
	uart1_release(MUX_GSM);									// Release UART1, restore BT if connected
  clear_comchange ();	 										// Wirkung Modemeinschaltung beseitigen
	putln (T_fsend);												// Text "Send file with Xmodem (1k)...
  delete_data();													// Messdatenzeiger im Parametersatz löschen
	 
	if ((blno=xmodem_receive(FIRMWARE_MAX_FILESIZE,MEASUREPAGE))<0)	// Xmodem Empfang
	 puterror (DTC_SI_XMODEM_ERR, blno);																						// Xmodem Fehlernummer als Detail ausgeben
  else		// Empfang ok 
  {
	 put2str(T_rdat,T_check);			   // Text "File received. Checking checksums ..."
	 osDelay(50);
	 for (i=0;i<blno;i++)						// CRC eines jeden Datenblock im Flash prüfen
   {	
    flash_pcom((uchar *)cbuf,pagepk*i,FLASH_R_ARRAY,BLOCKSIZE+2);		// Atmel Flash Seite(n) mit CRC lesen   	 
    if ((fehler=crc((uchar *)cbuf, BLOCKSIZE+2)) != 0) break;				// CRC Überprüfung Flash Abbild	 	   	 
   }
   if (!fehler) 	// kein CRC Fehler?
   {	 
    putln (T_ok);
		while (filesize--) if (cbuf[filesize]!=STRZ) break;					// Bytes letzter Block aufsummieren
		filecrc=crcgen((uchar *)cbuf,filesize+1,filecrc); 					// Datei CRC über Bytes des letzten CRC Bilden  
    filesize+=(blno-1)*BLOCKSIZE+1;		 													// Bytes übrige Blöcke addieren
		newline(); 
		putstr (T_fsize);
    putnumber (filesize,0);										// Dateigröße ausgeben									 
		putstr (T_crc);
		putnumber (filecrc,0x80);									// CRC16 der geladenen Datei ausgeben
   	newline();	  
		putstr (T_fname); 
		readline (fname,sizeof(fname),'a');				// Dateiname einlesen
		putln(T_wait);														// Inform user: uploading, terminal paused
		uart1_request(MUX_GSM, GSM_BAUD);				// Switch to GSM for file transfer
		fehler = file_to_uc15(filesize, fname);	// Schreibe Datei in UC15 Speicher
		uart1_release(MUX_GSM);								// Restore BT after transfer
		if (fehler > 0)													// Upload result
		 putln(T_csucc);												// Success message (visible on BT)
		else
		 puterror(DTC_SI_GSM_UPLOAD_FAIL, -1);								// Upload error
   }
   else puterror (DTC_SI_CRC_ERR, -1);							// CRC Fehler ausgeben
  }		
 }	// Modem nicht gestartet	
 else
 {
	dtcerr(DTC_SI_GSM_UPLOAD_FAIL);								// GSM Upload Fehler
	newline();	
 }
 clear_comchange ();			// Bereinige scheinbaren Schnittstellenwechsel
	
 newline();	
}

void compmenu(void)		// Testmenü für Bauteile etc.
{
 int c=0;				// Hilfsvariable Menüauswahl

 do
 {
  c=putmenu(T_comp,'8');			// Bauteil Testmenü senden, auf Auswahl warten
  switch (c)									// Auswahl auswerten
  {
	 case '1': write_flash (); break;			// Flash Seiten mit virtuellen Daten beschreiben   	
   case '2': read_flash (); break;			// Flash Seiten lesen
	 case '3': LED_font_test ();  break;	// LED Anzeigetest Ziffern	
	 case '4': LED_check ();  break;			// Schneller LED Test 7-Segment und Symbole
	 case '5': i2c_scan	(); break;				// I2C Device Scan
	 case '6': test_i2c_device (); break;	// Test I2C Devices
 	 case '7': upload_file ();	break;		// Upload certificate	
	}
 } while ((c>0)&&(c!='8'));	// Abbruch
}

void symbol_switches_info (void)	// Ausgabe der Schalter und Symbol Konfiguration
{
 uchar i;										// Laufvariable
 uchar sg,sw,sym=0;					// Hilfsvariable Listenauswahl Schaltgruppe, Schalter, Symbole

 if (fp.symgr || fp.swgrp) 				// Led Symbol- oder Schaltgruppen definiert
 {
  putln(T_conf);									// Text "Configuration: ..    
  for (sg=0;sg<fp.symgr;sg++)			// Für jede definierte Symbolgruppe 
  {
   putstr (T_symled);			    		// Text "Led Symbole...
   putstr(&fp.symgrname[sg][0]);	// Bezeichner jeder Symbolgruppe ausgeben
   if (fp.nosym[sg])							// Symbol in Gruppe definiert?	
    for (i=sym;i<sym+fp.nosym[sg];i++) // für jedes Symbol in Gruppe
    { 
     putstr(T_col);								// Trennstring
     putstr(T_symb);							// Text "Symbol.."
     putstr(&fp.symname[i][0]); 	// Symbolname
    }
   sym+=fp.nosym[sg];							// Symbole der letzten Gruppe addieren
   newline();
  }
  if (fp.symbol) 			// Symbole definiert
   putparameter (T_tsym, fp.scycle, (5<<16), T_ms);	// Parameter Symbolwechselzeit ausgeben
 
  for (sg=0;sg<fp.swgrp;sg++)								// Für jede definierte Schaltgruppe 
  {
   putstr(&fp.swgrname[sg][0]);									// Bezeichner jeder Schaltergruppe ausgeben
   if (fp.nosw[sg])															// Schalter in Gruppe definiert?	
    for (sw=4*sg;sw<4*sg+fp.nosw[sg];sw++) 			// für jeden Schalter in Gruppe
   { 
    putstr(T_col);															// Trennstring
    putstr(&fp.swname[sw][0]); 									// Schaltername
    if (fp.swno[sw]) putnumber(fp.swno[sw],2); 	// Schalternummer
   }
   newline();
  }
  newline();
 }
}

void show_ledsymbols (void)	// Led Symbole anzeigen, siehe auch cfont.h
{
 char items=0;						// Anzahl Listeneinträge
 ushort wert;							// Hilfsvariable LED Fontauswahl 
 uchar i;									// Laufvariable
 int result=0;

 putstr(T_showsym);										// Text "Show symbol.."
 for (i=0;i<fp.symbol;i++)	
 {
  put_menuitem (T_symb, (i+1)|0x30); 	// Menüpunkt "1. Symbol.." usw.
  putstr(&fp.symname[i][0]); 					// Symbolname
 }
 items=fp.symbol|0x30;
 select (items,0);										// Auswahl
 result=getc(TERMINAL_CHAR_TIMEOUT);	// Warte auf Zeichen max. 30 Sekunden  
 
 if ((result>'0')&&(result<=items)) 		// Gültige Eingabe?
 {
  newline();
  wert=fp.symfont[(result&0x0F)-1];
  putln(T_ende);											// Text Ende mit Return
  num_to_LED (wert,1);								// Font Wert nach LED Anzeigepuffer
  get_dimmung ();											// Dimmfaktor bestimmen
  show_led_off (TERMINAL_CHAR_TIMEOUT, 0);	// LED Ausgabe Symbol für maximal 30 Sekunden
 }
}

uchar get_symbols (uchar first, uchar last)	// Symbole einlesen
{																						// Übergabe:	first - Index des ersten zu lesenden Symbols
																						//						last	-	Index des letzten zu lesenden Symbols
 uchar i;																		// Laufvariable	
 for (i=first;i<last;i++)														// Für jedes Symbol des Indexbereichs
 {
	putstr(T_symname);																	// Eingabeaufforderung "Symbol name: "
	if (!getline (&fp.symname[i][0], SYLEN-1,'a')) break;	// Symbolname einlesen, Abbruch wenn Länge null	   	  
	fp.symfont[i]=getnumber (T_font, 400, 503);					// Fontnummer des Symbols lesen
 }
 return(i);				// Rückgabe Anzahl eingelesener Symbole 
}	

void symbol_redefine (void)			// Kundenschnittstelle Symbol Redefinition
{
 int result=0;
 uchar i;												
 uchar symgr, sanz=0;			
	
 if (list_selection (T_symgrn, &fp.symgrname[0][0], &symgr, 0xC0|fp.symgr, sizeof(fp.symgrname[0]))<0) return;
	
 put2str(T_newgrp,T_ist);
 result=getline(cbuf,SYGLEN-1,'a');		// Abfrage neuer Name Symbolgruppe	
 if (result<0) return; 
 else if (result>0) memcpy(&fp.symgrname[symgr][0],cbuf,SYGLEN);	// Eingabe ok, kopieren
 for (i=0;i<symgr;i++) sanz+=fp.nosym[i];				// Index erstes Symbol der Gruppe bestimmen
 result=get_symbols (sanz, sanz+fp.nosym[i]);		// Symbole einlesen	 
 if (result!=sanz+fp.nosym[i]) 	// Symbole unvollständig gelesen
  puterror (DTC_SI_PARAM_DEF_ERR, -1);	 
	
 newline();	
	
}	

void symbol_setup (void)				// Symboldefinition
{
 uchar i;												// Laufvariable	
 uchar sg;											// Symbolgruppe Laufvariable
	
 fp.symbol=0;											// Reset Anzahl definierter Symbole
 
   for (sg=0;sg<fp.symgr;sg++)		// für alle definierten Symbolgruppen 	 
   {
    putstr (T_symled);			    	// Text "Led Symbole...
    putln(&fp.symgrname[sg][0]);	// Bezeichner jeder Symbolgruppe ausgeben

    if (getuchar (T_nosym, 1, 4, &fp.nosym[sg])>0)				// Anzahl Symbole in der Gruppe abfragen
		{
		 i=get_symbols (fp.symbol, fp.symbol+fp.nosym[sg]);		// Symbole einlesen			 
		 if (i!=(fp.symbol+fp.nosym[sg])) break;	// Nicht erfolgreich gelesen -> Abbruch
		 else fp.symbol+=fp.nosym[sg];						// Eingelesene Symbole addieren 
		} else break;
		newline();
   }  // end for Symbolgruppen
   if (sg!=fp.symgr) 
	 {	 
		fp.symbol=0; 	// Fehler beim Einlesen -> Reset Symbole
		puterror (DTC_SI_PARAM_DEF_ERR, -1);
	 }	 
}	

void set_symbols_switches(void)	// Setup LED Symbole und Schalter
{
 int c,cc;					// Hilfsvariable Menüauswahl und 
 uchar sg,sw;				// Hilfsvariable Listenauswahl Schaltgruppe, Schalter, Symbole 
 uchar i,l;					// Laufvariable

 do
 {
  symbol_switches_info ();				// Ausgabe der Schalter und Symbol Konfiguration
  putstr(T_symsw);
  putc(':');
  putln(T_mswsym);								// Menü Schalter und Symbole senden 
	itemno('7');										// Menüpunkt für Symbolwechselzeit	 
  c=putmenu(T_tsym,'8');					// Symbolwechselzeit senden, auf Auswahl warten

	if (c=='1')																					// Symbolgruppen definieren
  {
   getuchar (T_nosymgr, 0, NOSYGR, &fp.symgr);				// Anzahl installierter Symbolgruppen abfragen
   for (i=0;i<fp.symgr;i++)														// Für jede Symbolgruppe
   {
    if (list_selection (T_tsymgr, &T_symgr1[0][0], &sg, 0x83, sizeof(T_symgr1[0]))<0)	// Listenauswahl?
		{
		 putstr(T_tsymgr);																// Eingabeaufforderung "Led symbol group name: "
		 if (!getline (&fp.symgrname[i][0], SYGLEN,'a')) 	// Bezeichner der Gruppe einlesen
			{ fp.symgr=0; break; }													// falls Timeout, reset Schaltgruppen und Abbruch
		}
		else memcpy(&fp.symgrname[i][0], &T_symgr1[sg][0], SYGLEN); // Listenauswahl kopieren	
   }
  } // end c=='1'
	
	else if ((c=='2') && fp.symgr)	symbol_setup ();		// Led Symbole definieren, wenn Gruppen definiert
	
	else if (c=='3' && fp.symbol) show_ledsymbols();		// Led Symbole anzeigen

	else if (c=='4')															//  Schaltergruppen definieren
	{	
	 if (ja(T_cconf)>0) 											 					// Abfrage "Change configuration (y/n)?"
   {		 
    if (getuchar (T_noswgrp, 0, NOSWGR, &fp.swgrp)>0) 	// Anzahl installierter Schaltergruppen abfragen
	  {	 
	   for (i=0;i<NOSW;i++) fp.swexp[i]=0;								// Reset alle Schalter Expander Einträge	 
     for (i=0;i<fp.swgrp;i++)														// Für jede Schaltergruppe
     {
      if (list_selection (T_tswgrp, &T_swgrp[0][0], &sg, 0x85, sizeof(T_swgrp[0]))<0)	// Listenauswahl?
		  {
		   putstr(T_tswgrp);																// Eingabeaufforderung "Switch group name: "
		   if (!getline (&fp.swgrname[i][0], SWGLEN,'a')) 	// Bezeichner der Gruppe einlesen
		   { fp.swgrp=0; break; }														// falls Timout, reset Schaltgruppen und Abbruch
		  }
		  else memcpy (&fp.swgrname[i][0], &T_swgrp[sg][0], SWGLEN);		// Listenauswahl kopieren	
     }
    }
   }
  } // end Schaltergruppen definieren
   
  else if (((c=='5')||(c=='6')) && fp.swgrp)					// Schaltergruppen definieren oder Schalter testen
  {
   if (list_selection (T_tswgrp, &fp.swgrname[0][0], &sg, fp.swgrp, sizeof(fp.swgrname[0]))>0)	// Listenauswahl Name Schaltgruppe? 
   {
		if (c=='5')	// Schaltergruppen definieren
		{
		 for (i=4*sg;i<4*sg+4;i++) fp.swexp[i]=0;			// Reset alle Schalter Expander Einträge in Gruppe
		 fp.nosw[sg]=0;																// Reset Anzahl Schalter
     if (getuchar (T_nosw, 1, 4, &fp.nosw[sg])>0)	// Anzahl Schalter in der Gruppe abfragen
			for (i=4*sg;i<4*sg+fp.nosw[sg];i++)					// Für jeden Schalter der Gruppe
			{
			 if (list_selection (T_tsw, &T_swtyp[0][0], &sw, 0x84, sizeof(T_swtyp[0]))<0)	// Listenauswahl Name Schalter?
			 {																							// Keine Eingabe
				putstr(T_tsw);																// Eingabeaufforderung "Switch name: "
				if (!getline (&fp.swname[i][0], SWLEN,'a')) 	// Schaltername einlesen
	      break; 																				// falls Timout, reset Schalter in Gruppe und Abbruch
			 }
			 else memcpy (&fp.swname[i][0], &T_swtyp[sw][0], SWLEN);						// Listenauswahl kopieren 
			 if (getuchar(T_tswno,0,9,&fp.swno[i])<0) fp.swno[i]=0;							// Optionale Schalternummer einlesen
			 if (getuchar (T_expno,1,2,&fp.swexp[i])<0) fp.nosw[sg]=0;					// Expander einlesen, reset Gruppe bei Timeout
			 else if (getuchar (T_portno,0,31,&fp.swport[i])<0) fp.nosw[sg]=0;	// Expander Port einlesen, reset Gruppe bei Timeout
			 else // wenn Schalterdefinition ok
			 {		// Prüfe auf Expander oder LED Spot device
	      if (((fp.swexp[i]==1)&& !(fp.i2cdev&IC58_b)) || ((fp.swexp[i]==2) && ((fp.i2cdev&ICLED_b)==0)))
				{
				 puterror(DTC_SI_I2C_EXP_ERR, -1);  // IC58 Expander Fehler oder fehlt
				 fp.nosw[sg]=0;	// Reset Anzahl Schalter in Gruppe
				 fp.swexp[i]=0;	// Reset Schalter Expander Eintrag
				 break;
				}	// end Expander Fehler   
			 }	// end Schalter ok  
			} // end if jeder Schalter
		 set_default_port ();			// Ändert default I2C Expander Porteinstellung
		 reset_switches();				// Reset alle Schalter XXX
		} // end if Schaltergruppen definieren
		else // Schalter testen
		{
		 while(1)
		 {
			putstr(T_tsw);	
			for (i=0;i<fp.nosw[sg];i++)										// Für jeden Schalter der Gruppe
			{
			 l=4*sg+i;																		// Index alle Schalter
			 put_menuitem (&fp.swname[l][0], (i+1)|0x30);	// Menüpunkt ausgeben
			 putc(' ');
			 if (fp.swno[l]) putnumber(fp.swno[l],0);			// Schalternummmer ggf. ausgeben
			}
			i=(fp.nosw[sg]+1)|0x30;
			cc=putmenu(T_nil,i);													// Ausgabe Auswahl
			if (cc>'0' && cc<i)														// Gültige Eingabe
			{
			 i=4*sg+(cc&0x0F)-1;			// Menüindex dekrementieren -> Schalterindex der Gruppe berechnen
			 putparameter (T_expno, fp.swexp[i], 0, T_LF);
			 putparameter (T_portno, fp.swport[i], 0, T_LF);
			 num_to_LED (188,1);			// Anzeigecode 188 nach LED Anzeigepuffer
			 show_led(-1,0);					// 188 ausgeben
			 if (fp.ex12==3) FIO0SET = EX12_2;	// 12V extern für Viasis Plus FT einschalten	   	
			 set_exp_switch (i);			// Setzt definierten Schalter
			 if (fp.ledspot) write_i2c_dev (ICLED,2,0xFF18);				// Max. Intensität setzen  
			 wait_return ();				// Warte auf Return Eingabe
			 if (fp.ex12==3) FIO0CLR = EX12_2;	// 12V extern für Viasis Plus FT abschalten
			 clear_all();						// Led samt Schalter aus	  
			}
			else break; // Abbruch
		 } 	
		} // end else Schalter testen
   }  // Listenauswahl Schaltergruppe
  }	// end if Schaltergruppen definieren oder Schalter testen
	
	else if (c=='7' && fp.symbol)						// Symbolanzeigedauer setzen, wenn Symbole definiert
	{
   cc=getmnumber(T_tsym, BASECYCLE, MAX_LED_HOLD, BASECYCLE);	// Symbolwechselzeit einlesen
	 if (cc>0) fp.scycle=cc;
  }
	
 } while ((c>0)&&(c!='8'));		// Abbruch 
 newline();	// Zeilenvorschub
}

void transmenu (void) 	// Transceiver & Verstärker Menü
{
 int c=0;								// Hilfsvariable Menüauswahl

 do
 {
  newline();
	test_battery(0);							// HF-/Analog-Spannungsversorgung messen  
  putln(T_trx);									// Transceiver & Verstärkermenü senden	
  putparameter (T_vtg, fp.u_hf, 5<<16, T_mVolt);		// Versorgung vor Menü ausgeben
  c=putmenu(T_tmenu,'3');				// Restmenü senden, auf Auswahl warten
  switch (c)										// Auswahl auswerten
  {
   case '1': amplifier_adjust (); break;		// Abgleich Verstärker/Transceiver 	
   case '2': transceiver_test (); 	break;	// Test Verstärker/Transceiver mit gedimmtem Display
  } 
 } while ((c>0)&&(c!='3'));	// Abbruch 

 newline();	// Zeilenvorschub
}

void werkmenue (void) 	// Werksmenü
{
 volatile int c;				// Hilfsvariable Menüauswahl

 newline();
 do
 {	 
	InitWatchdog(LONG_WD_32);					// langes Watchdog Interval auf 32 Sekunden setzen 
  werkeinstellung (0);							// Ausgabe ausgewählter Werksparameter
	
  put_menuitem (T_cconf,'1');				// Menüpunkt "1. Change configuration"
  put_menuitem (T_symsw,'2');				// Menüpunkt "2. LED symbols and switches"
  put_menuitem (T_bright,'3');			// Menüpunkt "3. LED brightness"
  put_menuitem (T_trx,'4');					// Menüpunkt "4. Amplifier & Transceiver"
  c=putmenu(T_kmenu,'8');						// Werksmenü senden, auf Auswahl warten
	 
	switch (c)								// Auswahl auswerten
  {
	 case '1': Werkskonfiguration (); break;	// Geräte-Konfiguration einlesen
	 case '2': set_symbols_switches(); break;	// Setup LED Symbole und Schalter	
	 case '3': Helligkeit(); break;						// Helligkeitseinstellung	
	 case '4': transmenu (); break;						// Transceiver & Verstärker Menü			 			    	
	 case '5': putstr(T_eraseall);						// Warnhinweis
						 if (ja((char *)T_cont)>0)	  	// Fortfahren?
						  if (InitParameter(1)==1) 			// Werkinititialisierung?
								putmstr(L_winit);						// Ja, Meldung
						 break;	
   case '6': compmenu();		break;					// Testmenü für Bauteile etc.
	 case '7': modem_com();		break;					// Befehlsmodus Modems
	 default:	break;				 
  } // end switch 		
 } while ((c>0)&&(c!='8'));	// Abbruch nach Auswahl	 	 
 newline();	// Zeilenvorschub
}

void specialmenu (void)			// Sondermenue
{
 int c;				// Hilfsvariable Menüauswahl
 int result;
 uchar items;

 do
 {	
  items='8';	 
  newline();	
  putstr (T_smenu);				// Text "Sondermenü ...
  putparameter (T_mcyc, fp.mcycle, (1<<16)|('1'<<8), T_ms);							// Parameter Messzyklus ausgeben
  putparameter (T_acyc, fp.acycle, (1<<16)|('2'<<8), T_ms);							// Parameter Anzeigezyklus ausgeben
	putparameter (T_units,0, (3<<16)|('3'<<8), &T_uall[fp.mph][0]);				// Messeinheit ausgeben 
  putparameter (T_vmmin,fp.vmm, (1<<16)|('4'<<8), &T_uall[fp.mph][0]); 	// Parameter kleinste gemessene Geschwindigkeit  
  putparameter (T_vcor,fp.vcor, (1<<16)|('5'<<8), T_perc);							// Parameter Korrekturfaktor ausgeben
  putparameter (T_vlim,fp.vlim[dset],(1<<16)|('6'<<8), &T_uall[fp.mph][0]);	// Parameter Tempolimit  	
  putparameter (T_dismod,0,(3<<16)|('7'<<8),&T_dmodes[fp.dmode][0]);		// Text "Display mode..
  if (fp.dmode)	 	// Anzeigemodus differenz?
  {
   putparameter (T_voff,fp.voff, (1<<16)|('8'<<8), &T_uall[fp.mph][0]);		// Parameter Anzeigeoffset
	 items++;	
  }

  c=putmenu(T_nil,items);   
  switch (c)	
  {
   case '1':		// Messzyklus einlesen
   {
    result=getmnumber(T_mcyc, MIN_MESSCYCLE, MAX_MESSCYCLE, BASECYCLE);	// Messzykluszeit einlesen
    if (result>0) fp.mcycle=result;
    break;
   }	
   case '2':		// Anzeigezyklus einlesen
						 {
						 result=getmnumber(T_acyc, fp.mcycle, MAX_LED_HOLD, BASECYCLE);	// Anzeigehaltezeit einlesen
						 if (result>0) fp.acycle=result;
						 break;
						 }
	 case '3': list_selection(T_units, &T_uall[0][0], &fp.mph, 0x82, sizeof(T_uall[0]));		// Listenauswahl Messeinheit 
						 break;					 
   case '4': getuchar(T_vmmin, Defminspeed, Defmaxspeed, &fp.vmm);	// kleinste gemessene Geschwindigkeit einlesen    
 						 if (fp.vmm < 5) 														// Kleinste Messgeschwindigkeit < 5 
						 {	 
							fp.txrcon|=BW_B;													// Verstärker untere Grenzfrequenz auf 1,4 km/h reduzieren
							t_amp_en=20;															// Messverzögerung Übersteuerungszeit Verstärker verlängern					 
						 }	 
						 else 																			// Kleinste Messgeschwindigkeit >= 5 
						 {	 
							fp.txrcon&=~(BW_B);								  			// Verstärker untere Grenzfrequenz auf 8,1 km/h nach oben setzen
							t_amp_en=10;															// Messverzögerung Übersteuerungszeit Verstärker verkürzen	 
						 }	 
						 break;   
   case '5': getuchar(T_vcor, Def_vcor, MAX_VCOR, &fp.vcor);				// Geschwindigkeitskorrekturfaktor
						 break;
   case '6': getuchar(T_vlim,Defminspeed,Defmaxspeed+1,&fp.vlim[dset]);	// Tempolimit
						 break;							 
	 case '7': list_selection(T_dismod, &T_dmodes[0][0], &fp.dmode, 0x82, sizeof(T_dmodes[0]));	// Listenauswahl Anzeigemodi					
						 break;		
   case '8': if (fp.dmode)	 	// Anzeigemodus differenz?						 
      			  getuchar(T_voff,0,Defmaxspeed-fp.vlim[dset],&fp.voff);	// Anzeigeoffset einlesen
  } // end switch   	  
 } while ((c>=0)&&(c!=items));	// Abbruch
 
 newline();	// Zeilenvorschub
}

int thrmenu (void)			// Menü Schaltschwellen Erweiterungen
{
 uchar item=0;														// Anzahl Menüpunkte
 uchar i;																	// Laufvariable Schaltergruppen

 if (fp.symgr||fp.swgrp)									// Symbole oder Schalter?
 {
  putstr(T_ext);   												// Menütitel "Schaltschwellen ...
  putc(':');
 }
 else putstr(T_ninst);										// Nein, Text "Nicht installiert...

 for (i=0;i<fp.symgr;i++)									// Für jede definierte Symbolgruppe
 {
   put_menuitem(T_symled,(++item|0x30));	// Menüpunkt mit Namen der Symbolgruppe
   putstr(&fp.symgrname[i][0]);
 }

 for (i=0;i<fp.swgrp;i++)									// Für jede definierte Schaltergruppe
   put_menuitem(&fp.swgrname[i][0],(++item|0x30));		// Menüpunkt mit Namen der Schaltgruppe
 
 if (fp.symgr) put_menuitem(T_symdef,(++item|0x30));	// Menüpunkt Symbol Neudefinition

 return(item);				// Rückgabe Anzahl Erweiterungen
}

void switchmenu (int grpno)		// Menü Ein-/Ausschaltschwellen Schaltergruppen
{								// Übergabe	grpno - Schaltgruppennummer-1 (0..2)
 uchar i;						// Laufvariable
 int grpoff=4*grpno;			// Hilfsvariable Gruppenoffset
 int c=0;						// Menüauswahlzeichen
 uchar anzsw=fp.nosw[grpno];	// Anzahl Schalter
 int items=(2*anzsw)|0x30;		// Anzahl Menüpunkte

 do
 { 
  putstr(&fp.swgrname[grpno][0]);	// Bezeichner der Schaltergruppe ausgeben
  cbuf[1]='\0';						// Null Terminierung
  for (i=0;i<anzsw;i++)				// Für jeden Schalter
  {
   if (fp.swno[grpoff+i]) cbuf[0]=fp.swno[grpoff+i]|0x30; 	// Ggf. Schalternummer nach cbuf
   else cbuf[0]=0;					// Leerstring
   put_schwellwert (&fp.swname[grpoff+i][0],cbuf,&fp.vswi[dset][2*(grpoff+i)], i);	// Schaltschwellen ausgeben
  }
  c=putmenu(T_nil,items+1);			// Menüabschluss senden und max. 30 Sekunden auf Auswahl warten
  if (c>='1' && c<=items)			// Gültige Eingabe
  {
   getuchar(T_thr,Defminspeed,Defmaxspeed+1,&fp.vswi[dset][2*grpoff+(c&0x0F)-1]);	// Schwellwert einlesen	   
   putc('\n');
  }
 } while ((c>0)&&(c!=items+1));		// Abbruch
}

void symbolmenu (int grp)	// Schaltschwellenmenü für Anzeigesymbole
{							// Übergabe	grp - Symbolgruppe-1 (0..2)
								
 int c=0;			// Zeichen Menüauswahl
 uchar i;			// Laufvariable 
 uchar off=0;		// Hilfsvariable offset Speicherindex Symbol
 int mitem=(2*fp.nosym[grp])|0x30;			// Anzahl Menüpunkte

 for (i=0; i<grp;i++) off+=fp.nosym[i];	// Offset Speicherindex Symbol ermitteln

 do
 { 
  putstr (T_symled);			    		// Text "Led Symbole...
  putstr(&fp.symgrname[grp][0]);	// Bezeichner der Symbolgruppe ausgeben
  putc(':');

  for (i=0;i<fp.nosym[grp];i++)	// Für alle Symbole im Menü
	 put_schwellwert (T_symb,&fp.symname[off+i][0],&fp.vsym[dset][2*(i+off)], i);	// Schwellwerte ausgeben
  c=putmenu(T_nil,mitem+1);			// Menüabschluss senden und max. 30 Sekunden auf Auswahl warten
	if (c>='1' && c<=mitem)				// Gültige Eingabe
	{
	 getuchar(T_thr,Defminspeed,Defmaxspeed+1,&fp.vsym[dset][2*off+(c&0x0F)-1]);	// Schwellwert einlesen	
	 putc('\n');
	}
 } while ((c>0)&&(c!=(mitem+1)));		// Abbruch
}

void get_thresholds (int auswahl)		// Einlesen von Geschwindigkeitsschwellen für Symbole und Schalter
{																		// Übergabe auswahl - Menüauswahl
 uchar i;				// Laufvariable
 int vgl=1;			// Vergleichswert
 
 newline();			// Zeilenvorschub

 for (i=0;i<NOSYGR;i++)			// Symbolgruppe 1 bis 3
  if (fp.symgr>=i+1)
   if (auswahl==vgl++)	{ symbolmenu(i); return; }		// Symbol Schwellwertmenü(s)

 for (i=0;i<NOSWGR;i++)			// Schaltgruppe 1 bis 3
  if (fp.swgrp>=i+1)
   if (auswahl==vgl++)	{ switchmenu(i); return; }		// Schalter Schwellwertmenü		

 if (fp.symgr)						// Symbolgruppe(n) existiert?
  if (auswahl==vgl++)	
   symbol_redefine();			// Symbolgruppe neu definieren	

 if (auswahl==vgl) menu>>=4;	// Menü Return
}

void put_ascii_data (void)				// ASCII Messdaten ausgeben
{
 char c;														// Hilfsvariable Zeicheneingabe
 uchar zeile=1;											// Laufvariable Ausgabezeilen
 ushort mdseite=fp.md_start_page;		// Laufvariable Messdatenseite
 ushort mdadr;											// Laufvariable Messdatensätze
 ushort lastadr;										// letzte zu lesende Seitenadresse 

 while (1)		// Alle Seiten lesen
  {
   mdadr=0;
   if (mdseite!=fp.md_page) 				// zu lesende ungleich aktueller Seite
   {
   	flash_pcom ((uchar *)cbuf, mdseite, FLASH_R_MAIN, maxbyte);		// Flashseite lesen
		lastadr=VSATZLEN*MD_PER_PG; 		// letzte zu lesende Datensatzadresse
   }
   else 
   {
    memcpy (cbuf,(text*)&vbuf[0], fp.md_adr);		// Daten nach cbuf kopieren
		lastadr=fp.md_adr;													// letzte zu lesende Datensatzadresse
   }
   while (mdadr<lastadr)	// Alle Datensätze lesen
   {
    put_signed_float (((short)(cbuf[mdadr]<<8)+cbuf[mdadr+1]), 10, 0, fp.nk); // Geschwindigkeit mit Vorzeichen
		put_date_time (&cbuf[mdadr+4]);					// Datum und Zeit aus Puffereintrag ausgeben
   	newline();				// Leerzeile

		mdadr+=VSATZLEN;		// Datensatzlänge addieren
		if (zeile++%20==0)
    {
     putln(T_weret);
		 c=getc(30000);
		 if (!is_CRLF(c)) return;
    }
   }
   if ((mdseite==fp.md_page)&&(mdadr==fp.md_adr)) break; // aktueller Messdatenzeiger erreicht?
   mdseite++;									   	// Nächste Messdatenseite
   if (mdseite==PROTOCOLPAGE) mdseite=MEASUREPAGE;	// wenn Messdaten=Protokollseite -> Überlauf
  } // end forever
}

bool send_vtf_file (uint anzwerte, uchar ziel)	// VTF Datei senden	
{									// Übergaben:	anzwerte - Anzahl Messwerte im Speicher
									//						ziel - 0 Base64 codierte Direktausgabe
									//									 1 Y-Modem Ausgabe
									//									 2 USB flash disc Ausgabe
 int result=1;
 uchar blno=0;					// Blocknummer 8 Bit
 uint seite;						// Laufvariable Seiten
 uint pglen;						// Hilfsvariable Seitenlänge
 uint ind=0;						// Bytezähler Datenausgabe			
 
 ResetWDT();						// Watchdog reset
 
 if (ziel>0)						// Ymodem oder USB Speichermedium
 {
  fp.fileno++;														// inkrementiere fortlaufende Dateinummer
  make_filename ();												// Erstellt VTF Dateiname in cbuf 
 }
 
 if (ziel==1)						// Ymodem 
 {	 
  putnumber ((VTF_LEN+PARA_LEN+PROT_LEN+10*anzwerte),0);	// Dateilänge in Byte senden
  while (cind<128) putc(0);								// Restlichen Header mit Nullen füllen
  redirect_char_Out(putb);								// Zeichenausgabe wieder an UART(s) leiten
  result=sendblock (SOH, blno++,1);				// Headerblock in cbuf ausgeben
 }
 else if (ziel==2)												// USB-Speicherstick
	result=open_vtf_file (); 								// Erstelle VTF Datei 

 if (result>0) 														// Ymodem Übertragung ok, Direktausgabe oder USB vtf Datei offen?
 {																				// VTF Dateiheader aufbauen
  cind=BLOCKSIZE; do cbuf[--cind]=0x20; while (cind);	// cbuf mit Leerzeichen initialisieren								
  redirect_char_Out(putcbuf);							// Zeichenausgabe in Puffer cbuf leiten
  putln(T_viafile);												// Dateiinfo
  putstr(T_filefor);											// Dateiformat
  cind=54; newline();											// an Position 54 CRLF
  read_date_time (DATE_SHORT, cbuf+cind);	// Auslesedatum 
  newline();
  read_date_time (TIME,cbuf+cind);				// Auslesezeit								  
  newline();
  putnumber(fp.fileno,0x08);							// Dateinummer, 8 stellig, führende Leerzeichen
  newline();
  putnumber(anzwerte,0x08);								// Anzahl Messwerte, 8 stellig, führende Leerzeichen
  newline();
  putnumber(anzahl_protokoll_ereignisse(),0x08);	// Anzahl Protokollereignisse
  newline();
  putnumber(PARA_LEN,0x08);								// Länge Parameter
  newline();
  putstr(fp.serno);												// Seriennummer
  cind=132; newline();
  putstr(fp.comment);											// Kommentar/Messort
  cind=214; newline();
  redirect_char_Out(putb);								// Zeichenausgabe wieder an UART(s) leiten
  result=btransfer (ziel, blno++);				// VTF Headerblock in cbuf ausgeben

  if (result>0) 													// Übertragung ok?
  {																				// Parameterblock senden										
   result=modem_send_par (ziel, blno);		// Ausgabe 2k Parameter
   
   if (result>0) 													// Übertragung ok?
   {
    blno=result;													// Blocknummer aktualisieren
		result=modem_send_pro (ziel, blno);		// Protokoll 2k senden

		if (result>0) 												// Übertragung ok?
    {
		 blno=result;													// Blocknummer aktualisieren
     seite=fp.md_start_page;							// Messdaten senden
     pglen=MD_PER_PG*VSATZLEN;						// Anzahl Datensätze pro Seite     
     cind=0; ind=0;												// Reset Pufferzeiger und Flash Seite Byteindex
     while (1)
     { 
      if (seite==fp.md_page)							// Seite ist aktuelle Seite? 
      {	   	   
       if ((cind+fp.md_adr)<=BLOCKSIZE)		// Pufferindex + aktuelle Messdaten <= 1k
        copy_to_cbuf ((char *)vbuf, fp.md_adr);			// aktuelle Messdaten nach cbuf
       else // > 1k			
       {
				ind=BLOCKSIZE-cind;								// Platz in Puffer
        copy_to_cbuf ((char *)vbuf, ind);	// Puffer füllen		
				result=btransfer (ziel, blno++);	// Messdaten in cbuf ausgeben
        if (result<0) break;	
        cind=0;														// Reset Pufferzeiger										
				copy_to_cbuf ((char *)&vbuf[ind],fp.md_adr-ind);	// Restliche aktuelle Messdaten
       }
			 if (cind)													// wenn Messdaten vorhanden
        while (cind<BLOCKSIZE) cbuf[cind++]=CPMEOF;		// Messdatenblock auf 1k füllen
       ind=pglen;	// nächstes Seitenlesen überspringen		
      }
      else // bereits im Flash gesicherte Seite
      {   
       if ((cind+pglen)>=BLOCKSIZE) ind=BLOCKSIZE-cind;	// max. 1k einlesen
       else ind=pglen;						    

       P_fpar=(uchar *)&cbuf[cind];				// Zeiger auf Puffer
       flash_adress(0,seite);							// Messdatenseite addressieren
       flash_command (FLASH_R_MAIN, 8, ind);	// Seite nach cbuf lesen
       cind+=ind;								// Pufferzeiger erhöhen    
      }	// end aktuelle/gesicherte Seite

      if (cind>=BLOCKSIZE) 								// 1k Blockgröße erreicht?
      {
			 result=btransfer (ziel, blno++);	// Messdaten in cbuf ausgeben
       if (result<0) break;								// Abbruch bei Fehler	
       cind=0;														// Reset Pufferzeiger
       if (ind<pglen)											// weniger als eine Seite gelesen
       {																	// Seitenrest einlesen
        P_fpar=(uchar *)&cbuf[cind];			// Zeiger auf Puffer
				flash_adress(ind,seite);					// Messdatenseite ab ind Byte addressieren
				if ((pglen-ind)>=BLOCKSIZE)				// Seitenrest > 1k
				{
				 flash_command (FLASH_R_MAIN, 8, BLOCKSIZE);	// Seitenrest nach cbuf lesen
				 result=btransfer (ziel, blno++);						// Messdaten in cbuf ausgeben
       	 if (result<0) break;							// Abbruch bei Fehler	
         cind=0;													// Reset Pufferzeiger
				 ind+=BLOCKSIZE;
				 flash_adress(ind,seite);					// Messdatenseite ab ind Byte addressieren
				}    
        flash_command (FLASH_R_MAIN, 8, pglen-ind);	// Seitenrest nach cbuf lesen
        cind=pglen-ind;										// neuer Pufferindex 	
       } // end if Seitenrest
      } // end if 1k Blockgröße erreicht
      if (seite==fp.md_page) 							// bis aktuelle Messdatenseite gesendet?
      {
			 if (ziel==0) putstr_b64 (cbuf, -1);		// Base64 Pufferausgabe beenden	
       else if (ziel==1) ymodem_close ();			// ymodem Übertragung beenden			 
       return (true);													// Rückgabe alles ok
      }
      if ((seite+1)==PROTOCOLPAGE) seite=MEASUREPAGE;	// Überlauf
      else seite++;		// nächste Flash Seite lesen
     } // end while (1) alle Messdaten(seiten) ausgeben  	
		} // end if result>0
   } // end if result>0
  } // end if result>0
 } // end if result>0
 puterror (DTC_SI_COMM_ERR, result);	  // Fehlermeldung
 return (false);		// Ende bei Fehler
}

uint messdaten (uchar ausgabe)		// Anzahl Messdaten berechnen und ggf. ausgeben
{																	// Übergabe: ausgabe - 0 keine Ausgabe, Rückgabe absolute Anzahl
																	//										 1 keine Ausgabe, Rückgabe in Prozent
																	//										 2 Ausgabe Anzahl und Vbat
																	//										 3 Terminalausgabe
 char c;																// Hilfsvariable Zeicheneingabe 
 uint anz_max, anz_proz=0;							// Hilfsvariable - Anzahl	Messwerte, max. Anzahl und % voll 
 int anzmw=fp.md_page-fp.md_start_page;	// Anzahl beschriebener Flashseiten	
 
 if (anzmw<0) anzmw+=PROTOCOLPAGE;	// Max. Anzahl Messdatenseiten addieren
 anzmw*=MD_PER_PG;									// Anzahl Seitendatensätze (26,52,103)
 anzmw+=fp.md_adr/VSATZLEN;					// Datensätze aktuelle Seite addieren 

 if (!ausgabe) return(anzmw);				// Rückgabe Anzahl Messwerte
	
 if (ausgabe==2) newline();		    	// Email-Ausgabe
 if (ausgabe>1)					    
 {
  put2str(T_anzmw,T_ist); 					// Text "Anzahl Messwerte = ...
  putnumber(anzmw,0);								// Anzahl ausgeben
 }

 if (ausgabe==3)				    				// Terminalausgabe?
 {  
  c=ja(T_daus);																// Abfrage Datenausgabe
  if (c==1) put_ascii_data ();								// ASCII Messdaten ausgeben	
  else if (c=='C') 
	{	
	 send_vtf_file (anzmw, 1);				// VTF Datei per ymodem senden 
	 osDelay(100);										// warte 100 ms Ymodem Ende
	}	
  if (ja(T_delete)>0) delete_data();					// Abfrage Daten löschen? Ja -> Messdaten im Speicher löschen
 } // end ausgabe
 else 															// keine Ausgabe an Terminal
 {
  if (ausgabe==2) putstr(T_col);									// Kommatrenner
  if (anzmw)																			// Anzahl Messwerte nicht null?
  {
   anz_max=PROTOCOLPAGE*MD_PER_PG;								// Maximale Anzahl Messwerte berechnen
   anz_proz=(anzmw*100/anz_max)+1; 								// Speicher voll in %     
  }
  if (ausgabe==1) return (anz_proz);							// Rückgabe Anzahl Messwerte in %
  putparameter(T_memfull,anz_proz,5<<16, T_perc);	// Formatiert mit Text ausgeben
  putbat(fp.u_in/10);															// Ausgabe Batteriespannung
 }
 return(anzmw);																		// Rückgabe Anzahl Messwerte 
} 

void put_protocol (uchar ausgabe)	// ASCII Protokoll ausgeben
{																	// Übergabe:	ausgabe - 0/1 für Email- / Terminalausgabe
 char c=0;												// Hilfsvariable Zeicheneingabe
 int result=0;										// Ergebnisvariable für Tabellensuche
 int number;											// Hilfsvariable zur Ereignis-/Fehlernummerermittlung
 int anzpro;											// Hilfsvariable Berechnung Protokollsätze
 ushort prtseite=fp.pro_page;			// Laufvariable Protokollseite
 ushort byteadress=fp.pro_adr;		// Laufvariable Protokollsätze
 uchar zeile=1;										// Laufvariable Ausgabezeilen

 if (!ausgabe) newline();
 put2str(T_anzpr,T_ist);	   						// Text "Anzahl Protokolldaten = ...
 anzpro=anzahl_protokoll_ereignisse();	// Berechnet Anzahl der Protokollereignisse
 putnumber(anzpro,0);										// Anzahl ausgeben
	
 if (!anzpro) return;										// Abbruch keine Daten	
											
 if (ausgabe) c=ja(T_daus);							// Abfrage Datenausgabe				   
 else { c=1; newline(); }

 if (c==1) 
 {  
  if (byteadress) memcpy (cbuf, probuf, byteadress);	// aktuelle Protokollseite nach cbuf

  while (1)	// Protokolleinträge rückwärts lesen, d.h. letzte Einträge zuerst ausgeben
  {
	 c=CR;													// Voreinstellung wg. Doppelschleifenausgang
   while (byteadress)							// solange Einträge in Seite
   {
    byteadress-=PSATZLEN;					// Einen Protokolleintrag zurück
    number=(cbuf[byteadress]<<8)+cbuf[byteadress+1];
    {                                       // Code -> Typ + Klartext aus Diagnose-Tabelle (alt+neu)
     char tp; const char *t = dtc_text(number, &tp);
     if (t) { putc(tp); putnumber(number,0); putc(' '); putstr(t); }
     else putnumber(number,0);              // unbekannter Code -> nur Nummer
    }
    put_date_time (&cbuf[byteadress+2]);	// Datum und Zeit aus Puffereintrag ausgeben
    newline();
		if (zeile>8) if (!ausgabe) break;
		if (zeile++%20==0)
		{
		 putln(T_weret);
		 c=getc(30000);
		 if (is_CRLF(c)) continue;
    }
   } // end while Byteadress
   if (!is_CRLF(c)) break;
   if (prtseite==fp.pro_start_page) break;  
   else if (prtseite==PROTOCOLPAGE) prtseite=PROTOCOLENDPG; 		// Überlauf
   else prtseite--;																							// Seite dekrementieren 
   byteadress=BLOCKSIZE/pagepk;																	// auf  Protokollseitenende+1
   flash_pcom((uchar *)cbuf, prtseite, FLASH_R_MAIN, maxbyte);	// nächste Protokollseite lesen
  } // end while (1)
 } 
 else if (c=='C')							// 1k (X)modemausgabe Aufforderung?
 {
  result=modem_send_pro(1,1);			// 1k (X)modemausgabe Protokoll XXX
  modem_eot (1);									// Xmodem Datentransfer beenden
  if (result<0) 									// 1k (X)modemausgabe Protokoll	ok?
   puterror (DTC_SI_COMM_ERR, result);					// Fehler ausgeben
 }
 if (ausgabe) 
  if (ja(T_delete)>0) 	// Abfrage Daten löschen?
   delete_protocol();		// Protokoll im Speicher löschen 
}

void zeitplan (void)			// Zeiteinstellungen
{
 put2str(T_tmen,T_dpkt);												// Text Menü Zeitplanung
 put_menuitem(T_led,'1');												// Menüpunkt "1. LED Anzeige: ...
 putstr(&T_offon[fp.led[dset]][0]);							// Text ein/aus  
 put_menuitem(T_ontage,'2');										// Menüpunkt "2. Einschalttage: ...
 puttage(fp.eintage[dset]);											// Einschalttage ausgeben
 put_menuitem(T_tein,'3');											// Menüpunkt "3. Einschaltzeit: ...
 put_signed_float (fp.tein[dset], 60, 0xC2, 2);	// Einschaltzeit ausgeben
 put_menuitem(T_taus,'4');											// Menüpunkt "4. Ausschaltzeit: ..
 put_signed_float (fp.taus[dset], 60, 0xC2, 2);	// Ausschaltzeit ausgeben
}

void gsm_zeit (void)			// Zeiteinstellungen Modem/Email
{
 uchar items='5';																// Fünf Menüpunkte initial
 putmstr(L_gsm);																// Text Menü Zeitplanung GSM/Email
 put_menuitem(T_ontage,'1');										// Menüpunkt "1. Einschalttage: ...
 puttage(fp.gsmtage);														// Einschalttage ausgeben
 put_menuitem(T_tein,'2');											// Menüpunkt "2. Einschaltzeit: ...
 put_signed_float (fp.tgsmein, 60, 0xC2, 2);		// Einschaltzeit ausgeben
 put_menuitem(T_taus,'3');											// Menüpunkt "3. Ausschaltzeit: ..
 put_signed_float (fp.tgsmaus, 60, 0xC2, 2);		// Ausschaltzeit ausgeben
 if (fp.servertyp==SMTP)												// SMTP server?
 {	 	
  put_menuitem(T_vemail,'4');										// Menüpunkt "4. Email Versand: ..
  if (fp.mailmode>4) fp.mailmode=0;							// Nicht initialisierten Parameterblock, bei updates abfangen
  putstr(&T_mailcyc[fp.mailmode][0]); 						// Ausgabe Versandmodus
  if (fp.mailmode>=2)														// Versand täglich, wöchentlich, monatlich
  {
   newline();																		// Neue Zeile
   itemno (items++);															// Menüpunkt "5.
   put2str(T_time,T_dpkt);												// Menüpunkt Zeit: ..
   put_signed_float (fp.temail, 60, 0xC2, 2);		// Versandzeit ausgeben
   if (fp.mailmode==3)														// Versand wöchentlich?
   {
    if (fp.ewtag>6) fp.ewtag=0;									// Nicht initialisierten Parameterblock, bei updates abfangen
    put_menuitem(T_wtag,items++);								// Menüpunkt "6. Wochentag: ..
    putstr (&T_tag[fp.ewtag][0]);								// Versand Wochentag ausgeben
   }
   if (fp.mailmode==4)														// Versand monatlich?
   {
    if (fp.emtag>28) fp.emtag=1;	    						// Nicht initialisierten Parameterblock, bei updates abfangen
    putparameter (T_mtag,fp.emtag,(1<<16)|(items++<<8),T_nil);  // Menüpunkt "6. Tag des Monats: ..
   }
  }
 }	// end if SMTP server
 else items--;																	// Kein Email Versand bei MQTT	
 select(items,1);																// Menüauswahl senden 
}

int Set_parameter (uint blocks)		// Im flash abgelegte Parameterblocks prüfen und einlesen
{																	// Übergabe blocks - Anzahl Parameterblocks
 int result=blocks;									
 uint offset;				
 	
 if (blocks==2 || (blocks==5 && ((fp.ex12==2)||(fp.ex12==4))))		// 2k oder 5k(nur Plus) Xmodem Datenblöcke empfangen?
 { 
  flash_pcom ((uchar *)&cbuf, FREEPAGE+pagepk, FLASH_R_ARRAY, BLOCKSIZE); // Zweiten 1k Block nach cbuf    
  if (cbuf[0]==fp.pKennung)	// Datenblock mit Parameterkennung? 
  {
   if (fp.serno[0]==0)				// noch nicht initialisiertes viasis?
	 {
    memcpy ((char *)&fp.pKennung, &cbuf[0], sizeof(fp)-BLOCKSIZE);		// fp_Länge-1k variable Parameter kopieren
	  flash_pcom ((uchar *)cbuf, FREEPAGE, FLASH_R_ARRAY, BLOCKSIZE); 	// Ersten 1k Block nach cbuf laden
	  memcpy ((char *)&fp.Kennung, &cbuf[0], BLOCKSIZE);								// 1k feste Parameter kopieren
	 }
	 else											// Seriennummer im viasis bereits gesetzt
	 {
		flash_pcom ((uchar *)cbuf, FREEPAGE, FLASH_R_ARRAY, BLOCKSIZE); 	// Ersten 1k Block zurück nach cbuf
		offset=(uint)&fp.serno-(uint)&fp.Kennung;
		if ((cbuf[offset]==0)||																	// Seriennummer in Parameterdatei leer? Oder
		(compare (fp.serno, cbuf,offset,offset+SERNO_LEN)>0))		// Seriennummervergleich positiv?
		{
		 flash_pcom ((uchar *)cbuf, FREEPAGE+pagepk, FLASH_R_ARRAY, BLOCKSIZE); 	// Zweiten 1k Block nach cbuf
		 memcpy ((char *)&fp.pKennung, &cbuf[0],(uint)&fp.gp - (uint)&fp.pKennung);	// 1k variable Parameter ohne GPS Datenfeld kopieren
		} else result=DTC_SI_PARAM_INIT;	// Parameter Initialisierungsfehler
	 }
	 if ((result==2)||(result==5)) 					// Gültige Anzahl Blöcke übertragen, bisher kein Fehler
	 {
    result=parameter_to_progmem();		 		// Schreibt Sicherungskopie der Parameter in LPC1766 Programmspeicher
	  if (!result) puterror(DTC_SI_PARAM_INIT, -1);	// Initialisierungsfehler
		else result=blocks; 															// Blockanzahl zurück nach result	
	 } // end gültige Blockanzahl	
	 if ((result==5)&&((fp.ex12==2)||(fp.ex12==4)))			// Viasis Plus / Viatext - Datenblock, Textnachrichten und Sonderzeichen	 
	  if (plus_com(1)<0) result=DTC_SI_PARAM_INIT;		// Upload nicht erfolgreich?
  }
  else result=DTC_SI_PARAM_INIT;								// Parameter Initialisierungsfehler
 }	 // end if 2k oder 5k (Plus, Viatext) empfangen
 else if (result>0) result=DTC_SI_PARAM_INIT;	// sonst allgemeinen Initialisierungsfehler ausgeben
  //else flash_pcom((uchar *)cbuf,FREEPAGE,FLASH_ERASE_WRITE_1,BLOCKSIZE+2);  
 dset=0;

 return(result);
}

void Parameter_transfer(uint timeout)	// Parameter down-/upload mit 1k xmodem
{																			// Übergabe timeout - Wartezeit auf Zeichen in ms 
 int result=0;
 int c=getbyte(timeout);							// warte auf weiteres Zeichen	
 
 if (c=='C')					// Parameterdownload
 {  
  result=modem_send_par (1,1);	// Parameter senden
  modem_eot (1);								// XModem transfer beenden
 }
 else if (c==-1)	// keine Antwort -> Parameter upload
 {
  result=xmodem_receive(5*(BLOCKSIZE+2),FREEPAGE);	// Xmodem Empfang	
	result=Set_parameter (result);										// Parameter prüfen und setzen				 
 }  

 if (comstate&RS232_LINK) 					// Terminal angeschlossen?
    osDelay(XMODEM_CHAR_TIMEOUT);		// warte bis Terminal bereit

 if (result==2 || ((result==5) && ((fp.ex12==2)||(fp.ex12==4)))) 	// 2k oder 5k bei Plus/Viatext empfangen?
 {
  set_default_port ();						// Ändert default I2C Expander Porteinstellung
  reset_switches();								// Reset alle Schalter
  timeupdate=1;										// Neuberechnung Parameter 
  newline();
  putln (T_parainit);							// Meldung "Parameter initialisation"..
  protocol(PARAMETER_INIT);				// protokollieren
 }
 else if (!((result==3)&&(c=='C'))) puterror (result, -1);  // Wenn nicht zwei Blöcke gesendet, Fehler ausgeben
 newline();
}

void radiostatus (void) 		// Bericht Tasks und Verbindungen, GSM, MQTT und GPS Status ausgeben
{
 uchar cres=0;
 uchar bt_time_cpy=bt_time;												// Notwendig da bt_time mit den ersten Versand zurück gesetzt wird.						
	
 putstr (T_report);	putnumber (tasks,0); putstr(T_rp_conn); putnumber (connect,0x80); putstr(T_rp_thread); putnumber (osActiveThread,0x80); 
 putstr (";IC54="); putc (' '); putnumber (read_i2cdev (IC54, 0, 1),0x80); // Verbindungen, Tasks, Threadstatus kommt immer
 if (fp.vspcam) { putstr ("; Online:"); putc (online|0x30); }	 
 if (fp.btmodem && ((connect&0x82)==0x82)) { putstr ("; BT Minuten: "); putnumber (bt_time_cpy,0); }
 newline();
 
 if (fp.gsm>0)							// GSM/UMTS Funkmodem installiert?
 {	
  putstr (T_rp_gsm); if (FIO1PIN&GSMPWR) cres=1; else cres=0; putstr (T_rp_sup); putc(cres|0x30);  // Platinenversorgung
	putstr (T_rp_gpwr); putnumber (gsmpower,0);			// GSM Modul ein/aus						 
	putstr (T_rp_csq); putnumber (fp.csq,0); 				// GSM/UMTS Stärke Empfangssignal
  putstr (T_rp_sim); putnumber (simpinset,0);			// Pinnummernstatus
	putstr (T_rp_wait); putnumber(wait_min,0);			// Warteminuten GSM/GPS Aktivität 
	newline();	 
  if (fp.servertyp==SMTP)															// Email Versandbetrieb?
	{		
	 putstr (T_rp_mail);	putnumber (mailtosend,0x80);	// Email Versandaufgaben
   putstr(T_rp_wmail); putnumber (wait_mail,0);				// Email Wartezeit
	 putc(';');		
	}	
	putstr (T_rp_sms);	putnumber (smstosend,0x80);			// SMS Versandaufgabe
  putstr(T_rp_wsms); 	putnumber (wait_sms,0);   			// SMS Wartezeit
  newline();
	if (fp.servertyp==MQTT) 														// MQTT Serverbetrieb?
	{
	 putstr (T_rp_mcom); putnumber (mqtt_com,0);				// MQTT Nachrichtenstatus
	 putstr (T_rp_fail); putc(mqtt_fail|0x30);					// MQTT Fehlerstatus
	 putstr (T_rp_pend);	putnumber (mqtt_mess_pend,0);	// MQTT Versandstatus - laufende, unquittierte Meldungen
   putstr (T_rp_tstat); putnumber (t_mqtt_status,0);	// Zeit MQTT Statusmeldung Versand > 5 = senden	
	 putstr (T_rp_pid); putnumber (PacketId,0);	 				// Zähler PacketId 
	 putstr (T_rp_mstat); putnumber (mqtt_state,0);			// MQTT Status (Connected, Joined, ...)
	 putstr (T_rp_man); putnumber (mqtt_manual,0);			// Manueller Betrieb bzw. Messdatenversand bei mehr als 5k Messdaten	
	 newline();
	 putstr (T_rp_mind); putnumber (MQTT_index,0);				// MQTT Messdaten Sendeindex
	 putstr (T_rp_mpg); putnumber (MQTT_send>>16,0);			// MQTT Sendedaten Flash Seitenzeiger
	 putstr (T_rp_madr); putnumber (MQTT_send&0xFFFF,0); 	// MQTT Sendedaten Flash Adresszeiger
	 putstr (T_rp_mdpg); putnumber (fp.md_page,0);				// Flash Seitenzeiger Messdaten
	 putstr (T_rp_mdadr); putnumber (fp.md_adr,0);				// Flash Adresszeiger Messdaten
	 putstr (T_rp_spg); putnumber (fp.md_start_page,0);		// Flash Messdaten Startseite
	 /*	 if (connect&MQTT_LINK)
   {		 
		if (mqtt_com)								// XXX Ausgabe Nachrichtenempfang
    {
     newline();		
	   for (cres=0;cres<16; cres++) { putc(' '); putnumber(mqbf[cres],0x82); }
     putstr(" rmi="); putnumber (rmi,0);	 
	  }
	  if (mqtt_fail) { newline(); sendbuf((char *)mqbf,rmi);	} 
	 }			 */
	 newline(); 
  }  // end if MQTT Betrieb
 }	// end if GSM installiert
 
 if (fp.gps)			// GPS Modul aktiv?
 {	 
  if (FIO0PIN&PWR_GPS) cres=1;  else cres=0; putstr (T_rp_gpspw);	putc(cres|0x30);	// GPS Modul powered
  putstr(T_rp_gpstyp); putnumber (fp.gps,0); 			// GPS Verbindungstyp 1=Multiplexed UART1, 2=UART2
  putstr(T_rp_gpspnd); putnumber (gps_pending,0);	// Laufende Positionsbestimmung
  putstr(T_rp_gpsfix); putnumber (gpsfix,0);			// Positionsbestimmungszeit[min] > Bestimmungsintervall[min] Bestimmung starten
	putstr(T_rp_anz);	putnumber (fp.gpsanz,0);			// Anzahl gespeicherter GPS Positionen 
	putstr(T_rp_gpsint); putnumber (gpsintv,0);			// GPS Positionsbestimmungs-Intervall 
  newline();
 }
 newline();	 
}

void debug_n_test (void)	// Debug and Testmenü
{
 char c;		
 uint i;	
	
 InitWatchdog(LONG_WD_32);					// langes Watchdog Interval auf 32 Sekunden setzen 

 putstr(T_mqtt1);	
 putparameter (T_mq_sim,0,(0x03<<16)|('3'<<8), &T_offon[mqtt_md_simu][0]); 	// Menüpunkt Datensimulation mit Ein/Aus Zustand
 putparameter (T_mq_dbg,0,(0x03<<16)|('4'<<8), &T_offon[mqtt_debug][0]); 		// Menüpunkt MQTT Debug mit Ein/Aus Zustand
 c=putmenu(T_mqtt2,'9');
	
 switch	(c)
  {
	 case '1':  	gpsfix=60*fp.gpsintv+1; break;						// Starte GPS Bestimmung
	 case '2': 	  i=test_registration();										// Prüfe auf Registrierung -> Nebenprodukt neuer CSQ Wert
								if (i==1) putln (T_regd);									// Eingebucht
								else if (i==5) putln (T_roam); 						// Roaming
								else putln (T_nreg);											// Nicht registriert
								break;
	 case '3': 		if (mqtt_md_simu) mqtt_md_simu=0;					// MQTT Messdatensimulation 0/1 - ein/aus
								else mqtt_md_simu=1;								
								break;								
	 case	'4':		wait_min=0;																// XXX keine Wartezeit
								if (mqtt_debug) mqtt_debug=0;							// MQTT Debugging 0/1 - ein/aus
								else mqtt_debug=1;								
								break;	
	 case '5':		get_mqtt_ind(); break;										// MQTT Speicherindizes etc.
	 case '6':		putnumber (plus_com(2),0); newline();
								getc(0);
		            for (i=0;i<3*BLOCKSIZE;i++) { putnumber(cbuf[i],0x82); putb(' '); osDelay(5); }// Puffer ausgeben
								break;				 	 									
   case '7':		test_registration();											// Ermittle ICCID
								putstr(T_simiccid);												// Text "SIM ICCID= "
								putln(iccid);															// ICCID Nummer ausgeben																								 
								break;		
	 case '8':	  Con_MQTT_server(17);											// Abmelden und disconnect MQTTS
								wait_min=Def_wait;												// Warte 3 Minuten bis neu verbinden nach Zeitplan
								break;											
   default:	break;				 
  } // end switch 		
}	

void reset_bt_pin (void) { newline(); putstr(T_pin); menu=0; bt_pininit=1; }			// Pinauswertung

void radionet_menu (void)														// Funknetzmenü
{
 putparameter (T_mfstd,0,(0x03<<16),T_nil);									// Menütitel
 putparameter (&T_nets[0][0],0,(0x02<<16)|('1'<<8), T_nil); 	// Ausgabe GSM/GPRS
 if (fp.gsm==UC15)	
  putparameter (&T_nets[1][0],0,(0x02<<16)|('2'<<8), T_nil); 	// UC15 Ausgabe UMTS/HSPA
 if (fp.gsm==EG91) 	
	putparameter (&T_nets[2][0],0,(0x02<<16)|('2'<<8), T_nil); 	// EG91 Ausgabe LTE 
 putparameter (T_auto,0,(0x02<<16)|('3'<<8), T_nil);					// Auswahl "Automatisch"
 putparameter (T_sstr, fp.csq,(0x01<<16)|('4'<<8), T_nil);		// Signalstärke ausgeben
 select ('5',1);		// Menüauswahl	 
}	

void mainmenu (char c)		// Zeichenbearbeitung Terminalschnittstelle(n) 
{
 uchar mensel=1;				// Hilfsvariable für Menüauswahl
 uchar cres=0;					// Hilfsvariable für Eingabe
 int result;						// Hilfsvariable für Menüsteuerung
	
 if (bt_pininit==0)							// Keine Bluetooth Pinabfrage aktiviert?
 {	 
 if (c=='H') { menu=0; online=0; menrep=1; newline(); }		// Menüreset und Hauptmenüausgabe
 if (c==ESC) if ((fp.ex12==2)||(fp.ex12==4)) { plus_com (0);	menrep=1; }	// Kommunikation Matrix controller 
 if (c==STRF) { radiostatus (); return;}									// Funkmodem und GPS Status ausgeben
 //if (c==STRT) { putstr("Reboot"); reboot(); }
	
 if (!online && !simulation)											// Kein Online-Messmodus oder Simulation
 { 	
	if (c=='V') { identify(); return;}												// Ausgabe Geräteidentifikation
  if (c=='R') { menu>>=4; menrep=1; newline(); }						// eine Menüebene aufwärts;	
  if (c=='T') { date_time();																// Zeitinformation senden oder laden 
								if (fp.vspcam && (fp.ex12==2) ) online=1;		// Im viaspeedcam Mode aktiviere Online Messmodus
								return; }																		// erst nach Zeitabfrage durch den Matrix Controller	
	
  if (menu==0x0003)									// Versteckte Funktionen Testmenü
  {
	 if ((c==STRW)||(c==STRU)) clear_all ();									// Anzeige abschalten, Schalter reset
   if (c==STRW) { werkmenue(); menrep=1; } 									// Werkseinstellung	
	 //if (c==STRZ) test_registration ();												// GSM Registrierung prüfen
   if (c==STRU) { putln(T_firmup); firmware(); menrep=1;  }	// Firmware update
   if (c=='X') { InitParameter(2); return;}									// Reset Parameter auf Werkeinstellung
	}
	
	if (menu==0x0004) 								// Versteckte Funktionen Parametermenü
  {
	 if (c==STRW) { Parameter_transfer(100);  menrep=1; }		// Parameter Xmodem up-/download
   if (c=='D') 	{ Parameter_transfer(30000);  menrep=1; }	// Parameter Xmodem up-/download	
	 if (c==STRU) { specialmenu(); menrep=1; }			// Sondermenü
   if (c=='X') { InitParameter(2); return;}				// Reset Parameter auf Werkeinstellung
  }	
	
	if (c==STRG) {
	 if ((menu==0x0052)||((menu==0x053))||((menu==0x0521))) { debug_n_test (); return; } }	// GSM/GPS/MQTT Test und Kommandomenü	
	
  if (!menrep)  					// Kein Menüwechsel
  {
   if (isdigit(c))  			// Nur Menüauswahl 1 bis 9 zulassen   
   {	
    putc (c);								// Zeichenecho     
    c&=0x0F;    	      	  // Maske 
    menu=(menu<<4)|c;   		// Maskierte Menüauswahl hinzufügen    
    newline(); newline();		// Zwei Zeilen vor       
   }  
   else mensel=0; 				// nachfolgende Menüauswahl ignorieren 
  }
  else menrep=0; 				// Anforderung Menüausgabe löschen
 } // end kein online messen	
 
 if (mensel) switch (menu)							// Menüverteiler
 {
  case 0x0000:       			// Hauptmenüausgabe
  {
	 putstr(T_main);				// Hauptmenüausgabe
   select ('6',0);				// Menüauswahl senden
   mensel=0; 							// Menüauswahl löschen und wiederholen
   break;
  }
	case 0x0001:						// Datenausgabe
  {
	 Ledaus ();							// Led abschalten	 
	 messdaten (3);					// Datenausgabe aufrufen 	
	 break;	
	}	
	case 0x0002:						// Online Messung
  {	 		
	 if (!online)			    
   {
		online=1;						// Online Datenausgabe aktivieren	
		putln(T_ende);			// Ausgabe Ende mit Return
		mensel=0; 					// Menüauswahl löschen und wiederholen
   }
   else if (is_CRLF(c) || c=='R') 
		 online=0;  				// Abbruch "Online Messmodus"
   else return;					// Andere Zeichen ohne Menüwiederholung ignorieren
   break;
  } 	
  case 0x0003:					// Testmenü
  {
   putstr(T_estmenu);			// Testmenüausgabe
   select ('7',1);				// Menüauswahl senden
   mensel=0; 							// Menüauswahl löschen und wiederholen
   break;
  }
	case 0x0031:					// Anzeigetest
  {
   ledtest ();					// Led Testprogramm/Messedemo
   break;
  }
	case 0x0032:					// Flash Speichertest
  {
   flash_test ();				// Test des Flash Speichers
   break;
  }
	case 0x0033:					// Test Echtzeituhr
  { 
   test_rtc (0);				// Test des Echtzeituhr Oszillators
   break;
  }
	case 0x0034:					// Test Echtzeituhr
  { 
   test_battery (1);		// Batteriespannungsmessung mit Spannungsmeldung
   break;
  }	
	case 0x0035:					// Anzeige und Schaltersimulation
  { 
	 if (!simulation)			// Keine Simulation	
	 {
		vtst=getnumber (T_sim, fp.vmm, Defmaxspeed);	// Testgeschwindigkeit einlesen 
		if (vtst>0)
		{	
		 if (fp.psets>1)		// Mehr als ein Parametersatz einstellbar?
		 {
			result=getnumber (T_pset, 1, fp.psets); // Parametersatz abfragen
			if (result>0) pset=result-1;						// Eingabe zuweisen
		 } else pset=0;														// Nur ein Satz vorhanden	
		 simulation=1;			// Simulationsbetrieb aktivieren	
		 putln(T_ende);			// Ausgabe Ende mit Return
		 mensel=0; 					// Menüauswahl löschen und wiederholen
	  } 
		else newline();			// Keine Eingabe -> neue Zeile
	 } // end (noch) keine Simulation
	 else if (is_CRLF(c) || c=='R' || c=='H')				//	Eingabe CR, 'H' oder 'R' während Simulation) 
	 {	 
		simulation=2;  			// Ende "Simulationsmodus"
		clear_all(); 				// Anzeige und Schalter reset
		timeupdate=1;				// korrekten Parametersatz etc. wieder einstellen
	 }	 
   else return;					// Andere Zeichen ohne Menüwiederholung ignorieren
   break;		 	 
	}	
	case 0x0036:					// Lichtsensor Test
  {
   licht_test ();				// Test des Lichtsensors
   break;
  }
	case 0x0004:
  {		
   parametermenue();				// Parametermenüausgabe          
   mensel=0;						
   break;
	}	
	case 0x0041: 				   	 //  Angezeigten Parametersatz einlesen
  {
   if (fp.psets>1){if (getuchar (T_pset, 1, fp.psets, &dset)>=0) dset--; } // Mehr als ein Satz definierbar
   break;   
  }
  case 0x0042: 				   	//  Zeitplan Parametersatz einlesen 
  {
   zeitplan ();															// Menü Zeiteinstellungen
   select('5',1);														// Menüauswahl senden
   mensel=0;	
   timeupdate=1;														// setze Minutenflag für Neubestimmung der Mess(zeit)aufgaben
   break;
  }
	case 0x0421:					// Led Anzeige ein/aus
  {
   list_selection(T_led, &T_offon[0][0], &fp.led[dset],0x82, sizeof(T_offon[0]));	// Listenauswahl Anzeigemodi    
   break;
  } 
  case 0x0422:					// Auswahl Einschalttage
  {
   get_weekday(&fp.eintage[dset]);					// Wochentage für Messung einlesen
   break;
  } 
  case 0x0423:					// Einschaltzeit
  {
   if (get_date_time (T_tein, T_fstime, 5)) // Einschaltzeit korrekt eingegeben?
    fp.tein[dset]=minutenwert(cbuf);				// Einschaltzeit in Minuten umrechnen   
   break;
  }
  case 0x0424:					// Ausschaltzeit
  {
   if (get_date_time (T_taus, T_fstime, 5)) // Ausschaltzeit korrekt eingegeben?
    fp.taus[dset]=minutenwert(cbuf);				// Ausschaltzeit in Minuten umrechnen
   break;
  }
	case 0x0043: 				   	//  Bidirektionale Erfassung einlesen
  {
   list_selection(T_bdir, &T_offon[0][0], &fp.bdir,0x82, sizeof(T_offon[0]));	// Listenauswahl Anzeigemodi
   break;
  }
	case 0x0044:							// Menü Optionen Geschwindigkeitsanzeige
	{	
	 if (fp.ex12!=4)					// Kein Viatext?
	 {	 											// Nur bei viasis mit numerischer Anzeige
		speed_setting_menu ();						
    mensel=0;		
	 }
   else putln(T_noacc);			// kein Zugriff bei Viatext 
	 break;
  }
	case 0x0441: 				   		// Nachkommastelle ändern
  {
	 list_selection(T_nk,&T_offon[0][0],&fp.nk, 0x82, sizeof(T_offon[0]));	 	// Listenauswahl Datenspeicherung	
	 break;	
	}
  case 0x0442: 				   	// Mindestgeschwindigkeit einlesen
  {
   getuchar (T_vmin, fp.vmm, fp.vmax[dset], &fp.vmin[dset]);
   break;
  }
  case 0x0443: 				   // Maximalgeschwindigkeit einlesen
  {
   getuchar (T_vmax, fp.vmin[dset], Defmaxspeed, &fp.vmax[dset]);	
   break;  
  }
	case 0x0444:					// Schwelle blinkende Anzeige	
  {
   getuchar (T_vblk, Defminspeed, Defmaxspeed+1, &fp.vblk[dset]);	// dto. einlesen
   break;
  }
	case 0x0445:							// Mischfarbschwellwert wenn erlaubt
	{
	 if (fp.farben==3)  																	// Mischfarbschwellwert erlaubt? 
		getuchar (T_col2, fp.vmin[dset], Defmaxspeed+1, &fp.vmix[dset]);	// Schwelle einlesen	
	 else if (fp.farben>1)																// sonst Schwelle Farbumschaltung?
		getuchar (T_vcolor, Defminspeed, Defmaxspeed+1, &fp.vcol[dset]);	// dto. einlesen 
	 else menu>>=4;																				// auch nicht Return Parameter Menü
	 break;
  } 																				
	case 0x0446:							// Schwelle Farbumschaltung	
	{	
	 if (fp.farben==3) 			 															// Mischfarbschwelle erlaubt?	 
    getuchar (T_vcolor, Defminspeed, Defmaxspeed+1, &fp.vcol[dset]);	// Farbumschaltung hier einlesen   			
	 else menu>>=4;																											// Return Parameter Menü
   break;
  }		
	case 0x0045:							// 	Anzeigeoptimierung
  {	
	 list_selection(T_brght,&T_brsel[0][0],&fp.hoff, 0x83, sizeof(T_brsel[0]));	// Listenauswahl Optimierungen zeigen	 
	 break;	
	}
	case 0x0046: 				   		// Schaltschwellen Erweiterungen
  {
   result=thrmenu();								// Erweiterungsmenü
   if (result>0) 
   {
    select(++result|0x30,1);				// Menüauswahl senden
    mensel=0;
   }	
   break; 	 
  }	
	case 0x0461: 					        
  case 0x0462: // Verteiler LED Symbole und/oder Schalter Schwellwerte	   
  case 0x0463:
  case 0x0464:
  case 0x0465:
  case 0x0466:
  case 0x0467:
	case 0x0468:
  case 0x0469:		
  {
   get_thresholds (menu&0x0F);  		// Ausgabe Schwellwertmenüs
   break; 
  } 
  case 0x0047:											// Radar Empfindlichkeit
	{	
	 result=getmnumber(T_rsens,20,100,20);		
	 if (result>0) 
	 {
		fp.sense=(result/20)-1;									// 20% bis 100% in Stufe 0...4 umrechnen
		fp.txrcon&=~(GAIN_INH|GAIN_A|GAIN_B);		// Kanalauswahl löschen
		fp.txrcon|=sense_tab[fp.sense];					// Kanalauswahl zuweisen
	 }
	 break;	
	}  
	case 0x0005:					// Radio Modem Menü
  { 
   putstr(T_modem);									// Modemmenüausgabe
   cres='3';												// zur Menüsteuerung	
	 if (interfaces&GPS_LINK)					// GPS Modem eingetragen?
   {	
	  put_menuitem (T_mgps,'3');			// Menüpunkt "3. GPS ..." ausgeben
		cres++;
   }		 
   select(cres,1);									// Menüauswahl senden
   mensel=0;	
   break; 
  }
	case 0x0051:					// Bluetoothmenü
  {
   if (interfaces&BT_LINK)					// Modem initialisiert und eingetragen
   {    
    if (connect&BT_LINK)						// Aktuell aktive BT Verbindung?
		 putln(T_noacc);								// kein Zugriff bei aktivem BT 
		else   
		{	           		
     putstr(T_btmenu);							// Bluetooth Menüausgabe
		 cres='3';											// zur Menüsteuerung
     if (fp.btmodem==Laird)					// Laird?
		 { 			
			put_menuitem (T_btname,'3');	// Gerätename nur bei Laird einstellbar
			cres++;												// Ein Menüpunkt mehr 
		 }	 
		 select(cres,1);								// Menüauswahl senden
     mensel=0;	
		} 
   }
   else putln(T_ninst); 						// Text "Nicht installiert..
   break;
  }
	case 0x0511:					// Bluetooth Information
  {
   send_bt_info ();									// Bluetooth Informationen ausgeben 
   break;
  }	
	case 0x0512:					// Bluetooth Pinnummer
  {
   set_bt_pin ();										// Setze Pinnummer
   break;											   
  }
	case 0x0513:					// Bluetooth Gerätename
  {
	 if (fp.btmodem==Laird)						// Laird?
    set_bt_name ();									// Setze Bluetooth device (friendly) name
	 else menu>>=4;		 
   break;
  }
	case 0x0052:					// GSM/GPRS Menü
  {
   if (interfaces&GSM_LINK)					// Modem initialisiert und eingetragen
   {	
		gsmmenu();			
		mensel=0;    	
   }
   else putln(T_ninst); 						// Text "Nicht installiert..
   break;
  }
	case 0x0521:					// Modem Zeitplan
  {
   gsm_zeit();											// Menü mit Einstellungen aufbauen   
   mensel=0;	
   timeupdate=1;										// setze Minutenflag für neue Bestimmung der GSM(zeit)aufgaben				
   break;
  }
	case 0x5211: 					// GSM Einschalttage definieren
  {
   get_weekday(&fp.gsmtage);				// Wochentage für GSM Einschaltung lesen   
   break;
  }
  case 0x5212:					// GSM Einschaltzeit einlesen
  {
   if (get_date_time (T_tein, T_fstime, 5)) // Einschaltzeit korrekt eingegeben?
		if (fp.tgsmaus!=minutenwert(cbuf))			// Einschalt- ungleich Ausschaltzeit
     fp.tgsmein=minutenwert(cbuf);					// Einschaltzeit in Minuten umrechnen   
   break;
  }
	case 0x5213:
  {
   if (get_date_time (T_taus, T_fstime, 5)) // Ausschaltzeit korrekt eingegeben?
		if (fp.tgsmein!=minutenwert(cbuf)) 			// Einschalt- ungleich Ausschaltzeit 
     fp.tgsmaus=minutenwert(cbuf);					// Ausschaltzeit in Minuten umrechnen
   break;
  }
  case 0x5214:					// Email Versandmodus
  {
	 if (fp.servertyp==SMTP)										// SMTP Server?
    list_selection(T_vemail, &T_mailcyc[0][0], &fp.mailmode,0x85, sizeof(T_mailcyc[0]));
	 else menu>>=4;															// sonst GSM Menü return
   break;
  }
	case 0x5215:					// Email Versandzeit
  {
   if ((fp.servertyp==SMTP)&&(fp.mailmode>=2))	// Versandzeit erforderlich
   {
    if (get_date_time (T_time, T_fstime, 5)) 	// Versandzeit korrekt eingegeben?
		 fp.temail=minutenwert(cbuf);							// in Minuten umrechnen und sichern
   }
   else	menu>>=4;															// sonst GSM Menü return
   break;
  }
  case 0x5216:					// Email Versand Wochen- oder Monatstag
  {
	 if (fp.servertyp==SMTP)
	 {
    if (fp.mailmode==3)												// Versand wöchentlich
    {
     list_selection(T_wtag, &T_tag[0][0], &fp.ewtag, 0x87, sizeof(T_tag[0]));	// Listenauswahl Wochentag
    }
    else if (fp.mailmode==4)	 								// Versand monatlich
    {
     getuchar (T_mtag, 1, 28, &fp.emtag);			// Tag des Monats (1..28) einlesen 
		 newline();
    }
		else	menu>>=4;														// täglicher Versand GSM Menü return
		break;
	 } 	
   else	menu>>=4;															// sonst GSM Menü return
   break;
  }
	case 0x0522:					// Pinnummer setzen/rücksetzen
  {
   result=getnumber(T_pinno,0,9999);					// Pinnummer einlesen
   newline();
   if (result>=0)															// Zulässige Pinnummer erhalten? 
   {
    fp.simpin=result;		  										// Ja, Pinnummer -> Parameterblock
		fp.pinerr=0;															// reset Pinfehler, Pinnummer wird wieder gesetzt
   }
   if (!(connect&(UART1|MQTT_LINK)))		  		// wenn keine GSM, MQTT oder BT Verbindung
   {
    if ((result>0) || ((gsmpower==0)&&(fp.simpin!=0))) atcpin (1, 1); // Pinnummer setzen
		else  atcpin (0, 1);											// Pinnummer Status nur prüfen und ausgeben
    if (gsmpower) FIO0SET=GSM_DTR;						// Enable Modem Sleep		 
   }
   else put2str(T_LF,T_noacc);								// kein Zugriff über GSM oder BT
   break;
  }
	case 0x0523:						// Servertyp SMTP oder MQTT auswählen					
  {
	 if (!(connect&(UART1|MQTT_LINK)))		// Nur wenn keine GSM oder BT Verbindung
   {	
    result=list_selection(&T_service[2][0],&T_server[0][0],&cres,0xC2,sizeof(T_server[0]));
	  if (result>0)
	  {
		 if (fp.servertyp!=cres)					// Servertyp geändert?
     {
		  fp.servertyp=cres;							// Änderung übernehmen
      set_server_default(cres);				// Default Parameter für Server setzen 	
		 }		
    }		 
	 }
   else put2str(T_LF,T_noacc);								// kein Zugriff über GSM, BT bzw. bei MQTT Verbindung	
   break;
  }
  case 0x0524:						// Konfiguration APN
  {
   get_apn_config();						
   break;
  }
	case 0x0525:						// SMS Konfiguration einlesen und testen
  {
   get_sms_config ();
   break;
  }
  case 0x0526:						// Konfiguration SMTP-Serverdaten
  {
   get_server_config();
   break;
  }
  case 0x0527:						// Email Konfiguration einlesen und testen
  {	
	 if (fp.servertyp==SMTP)
	 {	 
		get_email_config ();					
    break;
	 }			 	 	 
  }  
	case 0x0528:						// UC15 Funknetzauswahl, sonst Return
	{
	 if ((fp.servertyp==MQTT)&&(menu==0x0528)) menu>>=4;			// Auswahl löschen
	 else
	 {	 
    if (!(connect&UART1))							// wenn keine GSM oder BT Verbindung	
		{	
		 radionet_menu ();								// Funknetzmenü	
		 mensel=0; 												// Menüauswahl löschen und wiederholen	
		}	
    else put2str(T_LF,T_noacc);				// kein Zugriff über GSM oder BT
	 }	
   break;	 
  }	
  case 0x5271:						// Funknetzeinstellung ändern 		
	case 0x5272:
	case 0x5273:	
	case 0x5281:								
	case 0x5282:
	case 0x5283:	change_radionet (menu&0x03);				
								break;
  case 0x5274:						// Ermittelt Signalstärke neu
	case 0x5284:	test_registration ();		
								break;
	case 0x0053:						// GPS Menü
	{
	 if (interfaces&GPS_LINK)						// GPS Interface installiert
   {
		putstr(T_gpsmen);									// GPS Menü
		putparameter (T_tposfix,0,(3<<16)|('4'<<8),T_nil);	// Ausgabe Position fix interval
		if (gpsintv)
		{
		 putnumber (gpsintv,0);
		 putstr(T_h);	
    }	
    else putstr (&T_offon[0][0]);		
		select('5',1);										// Menüauswahl senden
		mensel=0;			
   }
   else menu>>=4;											// Nicht installiert, Auswahl löschen	 
	 break;	
  }		
	case 0x0531:					// Ausgabe gespeicherter GPS Daten
	{	
	 send_gps_data ();									// Ausgabe im Parameterblock gespeicherter Positionsdaten
	 break;	
	}	
	case 0x0532:					// GPS GGA formatierte Datenausgabe
	{
	 if ((connect&UART1)&&(fp.gps==1)) putln(T_noacc);	// kein Zugriff über GSM oder BT bei GPS über MUX UART1	
	 else send_gps_GGA	();															// Formatierte Ausgabe des GPS NMEA GGA Datensatzes	
	 break;	
	}
	case 0x0533:					// GPS NMEA Rohdatenausgabe
	{
	 if ((connect&UART1) &&(fp.gps==1)) putln(T_noacc);	// kein Zugriff über GSM oder BT bei GPS über MUX UART1
	 else send_gps_raw ();															// GPS Rohdatenausgabe
	 break;	
	}
  case 0x0534:					// GPS Interval Positionsbestimmung
	{
	 getuchar (T_tposfix, 0, 255, &fp.gpsintv);	// Zeitintervall GPS Positionsbestimmung lesen
	 gpsintv=fp.gpsintv;	
	 break;		
	}
	case 0x0006:					// Informationsmenü
  {   
   infomenu(1);																// Menüaufruf
   select('7',1);															// Menüauswahl senden
   mensel=0;	
   break;  
  }
	case 0x0061: { put_protocol(1); break;	}		// Protokollausgabe
	case 0x0062:					// Anmerkungen einlesen
  {
   put2str(T_comment,T_ist);									// Text "Anmerkungen =...
   readline(fp.comment,80,'a');								// bis zu 80 Zeichen, alphanumerisch
   break;
  }
	case 0x0063: 				  // Datum einlesen
  {
   if (get_date_time (T_date, T_fdate, 10)) 	// Datum korrekt eingegeben?
   {
    set_date(cbuf);														// wenn Ergebnis korrekt, dann Datum setzen
    test_sommerzeit (1);											// Prüfe ob aktuelle Zeit Sommerzeit und setze ggf. Flag		 
   }   	 
   timeupdate=1;															// Zeitänderung, ggf. neue Aufgaben
   break;   
  }
  case 0x0064: 				   			//  Uhrzeit einlesen
  {   
   if (get_date_time (T_time, T_ftime, 8)) 	 // Zeit korrekt eingegeben?
   {
    set_time(cbuf);  			// wenn Ergebnis korrekt, dann Zeit setzen				
	  test_sommerzeit (1);	// Prüfe ob aktuelle Zeit Sommerzeit und setze ggf. Flag
   }
   timeupdate=1;					// Zeitänderung, ggf. neue Aufgaben
   break;
  }
	case 0x0065:						// UTC Zeitzone einlesen
	 getsignedchar (T_tzone, T_utcr, -11, 12, &fp.utc);	// signed int8t einlesen
	 newline();
   break;
	case 0x0066:								// Sommerzeit/Winterzeit Umstellung
  {
   list_selection(T_somwin, &T_zone[0][0], &fp.s_w_zeit,0x84, sizeof(T_zone[0]));	// Listenauswahl Anzeigemodi
   test_sommerzeit (1);		// Prüfe ob aktuelle Zeit Sommerzeit und setze ggf. Flag
   timeupdate=1;					// Zeitänderung, ggf. neue Aufgaben
   break;
  }		
	case 0x0037:						// Testmenü Return 
  case 0x0048:						// Parametermenü Return 
  case 0x0054:						// Modemmenü Return  	   
  case 0x0067:						// Infomenü Return 
  case 0x0425:						// Zeitplan Return
	case 0x0447:						// Geschindigkeitsanzeigeoptionen Return	
  case 0x0514:						// Bluetooth Menü Return
	case 0x5217:						// GSM Zeitplan Return	
  case 0x5275:						// Funknetzmenü MQTT Return
	case 0x5285:						// Funknetzmenü SMTP Return	
	case 0x0529:						// Funkmodem Menü return	
	case 0x0535:						// GPS Menü Return
  default:		
  {
   menu>>=4;						// Auswahl löschen
   break;
  }
 } // end if mensel switch menu 

 if (mensel)	// Menü 1. Stufe aufwärts?
 {
  menrep=1;					// Menü wiederholen
  menu>>=4;					// Auswahl löschen
 }
 } // end if bt_pininit == 0
 else // bt_pininit > 0 Bluetooth Pinnummerabfrage aktiviert
 {
	if (c=='H') reset_bt_pin ();
  else 
	{	
	 putc('*');	
	 if (isdigit(c))
   {	
    menu*=10;					// bisherige Ziffern * 10
    menu+=(c&0x0F);		// neue Ziffer addieren 
	 }	
	 bt_pininit++;	

	 if (fp.btpin==menu) // Erfolg?
	 {	
	  putstr (T_main);					// Hauptmenü senden
		select ('6',0);						// Menüauswahl senden		
		bt_pininit=0;							// Reset Pinnabfrage
		menu=0; 
	 }	 
	 else if (bt_pininit>6)	reset_bt_pin ();		// Zuviele (falsche) Eingaben 
  }		
 }  // end if bt_pininit > 0 
}	
