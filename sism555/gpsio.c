//-----------------------------------------------------------------------------
//  FILE: gpsio.c			PROJECT: vario
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für Quectel L70 GPS Modul und EG91EX
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version J2
//-----------------------------------------------------------------------------
//  VERSION :  1.01
//-----------------------------------------------------------------------------
//  CREATED :   23.07.2021
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//              	23.07.2021 File creation
//-----------------------------------------------------------------------------
//	Unterstuetzt unterschiedliche GPS Varianten
// 	Hardware						fp.gps		GPS_out			GPS_in 			Anmerkungen
//	UC15 + L70					1					UART1				UART1				Umschaltung auf MUX, keine gleichzeitige Kommunikation mit GSM oder Bluetooth, U
//	UC15/EG91 + L70			2					UART2				UART2				Uneingeschränkte gleichzeitige Kommunikation mit GSM oder Bluetooth Modul auf UART1
// 	EG91EX							3					UART2				UART1				Die GPS-Messung muss auf UART1 gestartet und gestoppt werden, ging nicht in Produktion
//  EG91EX + LC76F			4					UART2				UART2				
//  EG91EX + LC76G			5					UART2				UART2				Unterstützt ab viasis 5.55 - 01/2026 nur fp.gps=4 wird noch mit unterstützt -> GPS und BT oder LTE gleichzeitig nutzbar

#include "gpsio.h"
#include "sio.h"
#include "sictxt.h"
#include "libtool.h"
#include "gsmio.h"
#include "mqtt.h"
#include "flash.h"

int gps_power (uchar mode)			// Schalte GPS L70/LC76F Modem - aus/ein/restart 
{																// Übergabe: mode - 0/1/2 power off/on/restart
																// Rückgaben	-1/0/1	- Fehler / Modem ein/aus
	
 if (mode>2) return(-1);				// Fehler Parameterübergabe
 else if (!mode)								// Ausschalten
 {
	FIO0CLR=PWR_GPS;
  if (!gsmpower) FIO1CLR = GSMPWR;	// 3V6 Versorgung auch aus
	Init_UART2 (0); 									// UART2 abschalten 
  return (0);	 
 }	 
 else if (mode==1)				// Einschalten
 {
	FIO1SET=GSMPWR;					// 3V6 Versorgung an
 	FIO0SET=PWR_GPS;				// L70 an 
	return(1); 
 }
 else											// Mit Restart Einschalten 
 {	 
	if ((FIO1PIN & GSMPWR) && (FIO0PIN & PWR_GPS) && (mode==2)) 		// GSM und GPS Versorgung ist an und Restart
  {		
	 FIO0CLR=PWR_GPS;				// L70 erst abschalten
	 osDelay(500);					// Warte 500 ms
  }
	else osDelay(5);				// Warte 5 ms
	FIO1SET=GSMPWR;					// 3V6 Versorgung an
 	FIO0SET=PWR_GPS;				// L70 an
	return(1);
 }	
}

void gps_command (text *s, uchar output) 		// Prüfsumme bilden und GPS Kommando senden
{																							// Übergabe *s - Zeiger auf Kommando mit * Abschluß
																							// 					output - 0 UART 1	keine Terminalausgabe
																							//									 1 UART 1	mit Terminalausgabe		
																							//									 2 UART 2 keine Terminalausgabe
																							//									 3 UART 2	mit Terminalausgabe
																							//									 4 UART 2 mit Stringheader in cbuf keine Terminalausgabe
																							//									 5 UART 2 mit Stringheader in cbuf mit Terminalausgabe
 uchar save_con=connect;						// Sicherung Schnittstellenzustand
	
 if (output<4)																// Zu sendender String s wird vollständig übergeben
 {	 
  redirect_char_Out(putcbuf);									// Zeichenausgabe in cbuf leiten
  cind=0;																			// reset Index	
 }	 
 else output-=2;															// Ausgabeoffset für Stringanteil des Aufrufers wieder abziehen
 putstr(s);																		// Nachricht an Puffer ausgeben ohne Prüfsumme
 putnumber(nmea_checksum (cbuf+1),0x82);			// xor Prüfsumme bilden und an Puffer ausgeben
 cind--;																			// 'H' Kennzeichnung der Hexadez.-Ausgabe überschreiben		
 putc('\0');																	// String null terminieren	
 redirect_char_Out(putb);											// Zeichenausgabe an UART(s) leiten	
 if (output==0) connect=UART1;								// sende nur an GPS Modul über UART1
 else if (output==1) connect|=UART1;					// sende an GPS Modul über UART1 und an Terminals
 else if (output==2) connect=UART2;						// sende nur an GPS Modul über UART2
 else if (output==3) connect|=UART2;					// sende an GPS Modul über UART2 und an Terminals	
 putln(cbuf);																// GPS Kommando ausgeben	
 connect=save_con;														// Schnittstellenzustand wieder herstellen	
}

int test_gps (uchar sendmessages)		// Prüfe ob GPS Modul Einschaltnachrichten sendet
{																		// Übergabe: sendmessages - 0/1 für Nachrichten nicht senden/senden
																		// Es werden nur noch die GPS Typen 4 (LC76F) und 5 (LC76G) unterstützt
 int result=-1, zeile;
 uint baud=115200;
 uchar runs, ii=0;
	
 if (fp.gps==0)	runs=2;				// GPS Modemtyp noch unbestimmt, 2 Durchläufe bei 115200 und 9600 Baud erforderlich
 else { runs=1; fp.gps=0; }		// GPS Modemtyp war bereits bestimmt -> nur 1 Durchlauf bei 115200
 
 while (runs--)	// 2 Durchläufe wenn unbestimmt, LC76G und LC76F bei 115200, danach LC76F bei 9600 Baud
 { 
	result=-1; 
	 
	FIO0CLR = CSFLASH2; 
	 
	gps_power (2);									// GPS mit restart einschalten  
	
	Init_UART2(baud);								// Konfiguriere und aktiviere Uart2
	
	bx2=rx2;												// Leere Puffer
	msdelay=5000;										// Timeout erste Systemnachricht
	FIO0SET = CSFLASH2; 
	do
	{
	 if (bx2!=rx2) 									// Zeichenempfang Uart2
	 { if (gbuf[bx2]== '$') break;	// $ Zeichen -> Beenden
		 else bx2++; 									// Sonst nur Bearbeitungszeiger erhöhen
	 }	 
	 ResetWDT();	
	} while (msdelay);							// bis Timeout	
	
	if (msdelay>0) 						// Kein Timeout ('$' Zeichen bei 115200 empfangen)?
	{		 
	 FIO0CLR = CSFLASH2;	
	 while (1)								// Warte auf Nachrichtenzeilen
   {	
    zeile=wait_message_2(T_CRLF,500,UART2);			// Warte 500ms auf Zeile von GPS Modul		 
	  if (zeile<0) break;													// Keine Zeile von GPS Modul erhalten -> Abbruch
	  if (zeile>0)																// Zeile hat Inhalt/Länge? 	
		{	
		 if (result<0) 														  // Einschalttext in Zeile noch nicht gefunden?	
		 {
			if (compare(T_gntxt,cbuf,0,sizeof(T_gntxt))>0) fp.gps=4;  														// Suche "GNTXT", wenn gefunden LC76F Typ 4
			else if (baud==115200) if (compare(T_pqtmver,cbuf,0,sizeof(T_pqtmver))>0) fp.gps=5;  	// Suche "PQTMVER", wenn gefunden LC76G Typ 5			
			if (fp.gps) result=1; 	
		 }	 		 
	   if (result>0) 															// Einschalttext gefunden ausgeben und gib empfangene 5 Folgenachrichten aus
	   { 	 
		  ii++; 																			// Nachrichtenzähler erhöhen
		  if (sendmessages) putstr (cbuf); 			
		  if (ii>=5) break;  								// Anzahl 5 gewünschte Folgenachrichten erreicht? Ja, -> Ende
	   } 				
	  } // end if Zeile hat Inhalt
   }	// end while warte auf Nachrichten	 	 
  } // end if kein Timeout ($ Zeichen erhalten)
	FIO0SET = CSFLASH2; 
	if (fp.gps) break;  														// Modultyp gefunden dann beende runs while
	if (sendmessages) osDelay(2);										// warte 2 ms bis letztes Byte sicher gesendet
	
	baud=9600;				// auf LC76F gps_typ 4 niedrige default Baud rate umstellen
 } 
 
 if (fp.gps==0)	// Keine Meldung und Modul nicht gefunden
 {
	 if (!sendmessages)								// Keine Ausgabe im Power On
	  puterror(GPS_ERROR, -1);				// Kein Erfolg, Fehler im Protokoll melden	 
 	 gps_power(0);										// Abschalten	
 } 
 return(result);										
}

int set_gps_messages (uchar protocols)	// LC76G default aktive Nachrichtentypen deaktivieren		
{																				// Übergabe protocols - bit0=GGA, bit1= GLL, bit2=GSA, ... bit5=VTG siehe PAIR062 Nachrichtennummern
 uchar i;																	
 for (i=0;i<=5;i++)			// Alle 5 default aktiven Nachrichtentypen ggf. auf Null setzen
 {
  if (!(protocols&0x01))								// Nachrichtentyp deaktivieren wenn Bitposition Null
  {		
	 cind=0;															// Init cbuf Pufferindex 
	 redirect_char_Out(putcbuf);					// Redirect Zeichenausgabe 		
	 putstr(T_pair62);										// Kommando nach cbuf	
	 putc (i|0x30);												// Protokollnummer 1=GLL, ...5=VTG	 
	 gps_command (T_null,4);							// Restliches Kommando senden  
	 bx2=rx2;															// Leere Puffer 
	 if (wait_message_2(T_CRLF,500,UART2)<=0) break;						// Keine Zeile von UART2 in 0,5s -> Ende
   if (compare(T_p62ack,cbuf,0,sizeof(T_p62ack))<=0)	break;	// Kein Ack auf PAIR062 erhalten -> Ende	
	} // end if protocols	
	protocols>>=1;
 }	// end for	 	
 
 if (i>5) return(1);							// Schleife vollständig abgearbeitet 
 return (-1);					 						
}	
 
void init_gps (void)							// Initialisiere und teste GPS LC76 Modul
{
 int result=-1;	
 uchar uartsave=connect;					// Schnittstellenzustand sichern
	
 result=test_gps (1);									// GPS LC76F/LC76G Modul sendet Einschaltnachrichten?
 if (comchange) clear_comchange();		// Einschaltung von GSM/GPS Versorgung kann scheinbaren Schnittstellenwechsel verursachen	
 	 
 if (result>0) 													// LC76F/LC76G Start-Nachricht(en) empfangen?
 { 
	result=-1;
	
	if (fp.gps==4)		// LC76F an uart 2
	{	
	 osDelay (300);												// Warte bis NMEA Nachrichten empfangen		
	 gps_command (T_C147,2);							// LC76 Baudrate auf 115200 setzen, wird permanent gespeichert	
	 osDelay (150);	
	 Init_UART2 (115200);									// Uart2 auf 115200 konfigurieren 			 	
	 gps_command (T_C242A,2);							// PGKC Kommando 242, GGA Ausgabe festlegen
   bx2=rx2;															// Leere Puffer			
	 if (wait_message_2(T_CRLF,250,UART2)>0)	// Warte auf Antwort an UART2
   { 
		if (compare(T_pgkc,cbuf,0,sizeof(T_pgkc))>=0)		 		// Prüfe ob PMTK Ackknowledge
	  {	
		 putstr(cbuf); 
		 if (wait_message_2(T_CRLF,1200,UART2)>0)	// Warte auf erste GGA Nachricht 
		 {	
			putstr(cbuf);													// an Terminal ausgeben			
		  result=1;															// GPS Modul erfolgreich installiert 
		 } 	
		} 	// end if Ack Nachricht empfangen	
   } 	// end if Nachricht empfangen		 
	}	// end if LC76F	
	else if (fp.gps==5)		// LC76G an uart 2
	{
	 osDelay (100);												// Warte bis erster NMEA Nachrichtensatz empfangen	
	 if (set_gps_messages (1<<0)	> 0) 		// LC76G Alle aktiven Nachrichten ausser GGA auf Null setzen, bei Erfolg	
	  if (wait_message_2(T_CRLF,1200,UART2)>0)	// Warte auf erste GGA Nachricht 
		{ 
		 putstr(cbuf);													// an Terminal ausgeben			
		 result=1;															// GPS Modul erfolgreich installiert 
		}	 
	} // end if LC76G an uart 2	
  gps_power (0);														// GPS wieder abschalten
	Init_UART2(0);
 } //end if result Startnachrichten	
 
 if (result<=0) {fp.gps=0; put2str(T_err,E_gps);	}// GPS Fehlermeldung ausgeben
 else		// Erfolg
 {
	fp.gpsintv=Def_gpsintv;							// Setze default GPS Positionsfix Zeitinterval
 	gpsintv=fp.gpsintv;	
	fp.gpsanz=0;												// Null GPS Positionsbestimmungen
	fp.gi=0;														// Index Positionsbestimmungen auf Null	
	gpsfix=60*gpsintv;	 								// Bestimme Zeitpunkt Positionsermittlung 
	interfaces|=GPS_LINK;								// zu installierten Interfaces hinzufügen
  putstr (T_gpstyp);									// Text "GPS Typ...
  putnumber (fp.gps,0);								// Typnummer
  newline();	 
 }	  
 osDelay (10);												// Warte auf geänderten Schnittstellenzustand
 if (comchange) clear_comchange ();		// Bereinige Schnittstellenwechsel
 connect=uartsave;										// Schnittstellenzustand wiederherstellen
}

void send_gps_raw (void)						// Ausgabe von GPS Rohdaten GLL, RMC und GGA
{
 int result=-1;	
 char c='\0';												// Eingabezeichen
 uchar empfang=0;	
	
 result=test_gps (0);	
 if (comchange) clear_comchange();		// Einschaltung von GSM/GPS Versorgung kann scheinbaren Schnittstellenwechsel verursachen		
	 	
 if (result>0)												// GPS // L70 GPS Typ 1 und 2 oder LC76F Typ 4 meldet sich ?
 {		
  result=-1; 
	putln (T_ctrlz);	newline(); 				// Text "Ende mit CTRL-Z"
 	if (fp.gps==4)
	{ 
   osDelay (300);											// Warte bis NMEA Nachrichten empfangen	
	 gps_command (T_C242B,2);						// PGKC Kommando 242 - GLL, RMC und GGA Ausgabe festlegen
   bx2=rx2;														// Leere Puffer		
	 if (wait_message_2(T_CRLF,250,UART2)>0)	// Warte auf Antwort an UART2
   {	if (compare(T_pgkc,cbuf,0,sizeof(T_pgkc))>=0)	result=1;	} 		// Prüfe ob PMTK Ackknowledge
	}
	else if (fp.gps==5)	
	{
	 osDelay (100);												// Warte bis erster NMEA Nachrichtensatz empfangen		
	 result = set_gps_messages ((1<<4)|(1<<1)|(1<<0));		// GLL, RMC und GGA Ausgabe mit PAIR062 Kommandos festlegen
	}		
 }	
 
 if (result>0) 
 {	
  bx2=rx2;							// Pufferempfang reset
	 
  while (!comchange)	 	// Solange kein Schnittstellenwechsel
	{
	 if (getkey2()) { c=gbuf[++bx2]; empfang++; }  	// Zeichen UART2 einlesen
   if (getkey1()) { c=rbuf[++bxi]; empfang++; }		// Zeichen UART0, UART1 einlesen
	 if (c==STRZ) break;														// Ende mit CTRL-Z
   if (empfang) { putc(c); empfang--; }	 					// Zeichen an Terminal	
   ResetWDT();																		// Watchdog reset	 
	}
	Init_UART2 (0);										// bzw. UART2 deinitialisieren
 }	
 else put2str(T_err,E_gps);			// GPS Fehlermeldung ausgeben 
 gps_power(0);									// L70/LC76F abschalten und deselektieren 
 gps_pending=0;									// GPS Positionsbestimmung unterbrochen
 putln (T_LF);
}

void send_gps_GGA	(void)	// Formatierte Ausgabe des GPS NMEA GGA Datensatzes
{
 int result=-1;
 uchar pos;	
 uint val;
 uint satz=0;	
 char c=0;	
	
 result=test_gps (0);
 if (comchange) clear_comchange();		// Einschaltung von GSM/GPS Versorgung kann scheinbaren Schnittstellenwechsel verursachen		
	
 if (result>0)									// GPS L70/LC76F meldet sich
 {
	if  (fp.gps==4)											// GPS LC76F?
	{
	 osDelay (300);											// Warte bis NMEA Nachrichten empfangen	
	 gps_command (T_C242A,2);						// PGKC Kommando 242, GGA Ausgabe festlegen
   bx2=rx2;														// Leere Puffer		
	 if (wait_message_2(T_CRLF,250,UART2)>0)	// Warte auf Antwort an UART2
    {	if (compare(T_pgkc,cbuf,0,sizeof(T_pgkc))>=0)	result=1;	} 		// Prüfe ob PMTK Ackknowledge	
  }	
  else if (fp.gps==5)	{									// GPS LC76G?	
	  osDelay (100);											// Warte bis erster NMEA Nachrichtensatz empfangen		
	 result = set_gps_messages (1<<0);	} // Nur GGA Ausgabe mit PAIR062 Kommando festlegen		
 }	
 
 if (result>0)												// Nur noch fp.gps Typ 4 oder 5
 {	
  putln (T_ctrlz);										// Text "Ende mit CTRL-Z"   	 
	while ((c!=STRZ) & (comchange==0))					// Warte auf CTRL-Z Abbruch oder Schnittstellenwechsel
  {		
	 bx2=rx2;																		// Leere Puffer	
	 if (wait_message_2 (T_CRLF,1500,UART2)>0)	// Warte auf GGA Satz	an Uart2
	 {
		if (compare(T_gngga,cbuf,0,sizeof(T_gngga))>=0)	// Satz beginnt mit $GNGGA										
    {		 
		 if (satz++%20==0) putln (T_gga);				// Tabellenüberschrift
		 pos=colonpos(cbuf,7);									// Finde Satellitenanzahl hinter 7. Komma	
     val=atoi(cbuf+pos);										// Zu Nummer wandeln
		 putnumber (val,2);											// zweistellig ausgeben
		 putc(',');
		 if (val>0)															// wenigstens 1 Satellit	
		 {
			pos=colonpos(cbuf,1);									// Ermittle Zeitposition
			putxchr (cbuf+pos, 6);								// UTC 6 stellig mit Komma ausgeben
			putc(',');
			val=cbuf[colonpos(cbuf,6)];						// Werte Positionsfix aus
			if ((val=='1')||(val=='2'))						// GPS oder DGPS fix
			{
			 pos=colonpos(cbuf,2);						// Ermittle Latitude Position	
			 if (fp.gps>=3) putxchr (cbuf+pos, 29);	// EG91EXGA/LC76F Ausgabe Latitude/Longitude
			 else putxchr (cbuf+pos, 25);						// L70 Ausgabe Latitude/Longitude
			 val=colonpos(cbuf,9);						// Suche Höhenposition												
			 pos=colonpos(cbuf,8);						// Ermittle Position Präzision - HDOP
			 putxchr (cbuf+pos,val-pos);			// Ausgabe HDOP	
			}		
		 } // end if >= 1 Satellit		 
		 newline();
		} // end if Satzbeginn $    	 
	 } // end if wait_message_ > 0	
	 if (bxi!=rxi) c=rbuf[++bxi];				// Zeichen von Uart 0/1
	}	// end while ()
	Init_UART2 (0);										// bzw. UART2 deinitialisieren
 }		

 if (result<0) put2str(T_err,E_gps);	// GPS Fehlermeldung ausgeben 
 gps_power(0);												// LC76 abschalten und deselektieren
 gps_pending=0;												// GPS Positionsbestimmung unterbrochen
 putln (T_LF);
}	

void get_geopos (void)		// Ermittle GPS position
{
 uchar pos;								// Hilfsvariable Datenposition im Puffer
 uchar retry=0;						// Anzahl Nachrichtenempfänge	
 int val;									// Hilfsvariable Wert Datenfelder
 uint utd, lat, lng;			// Hilfsvariable zur lokalen Ermittlung der GPS-Daten		
   
 if (!gps_pending) 										// GPS Positionsbestimmung ist noch nicht gestartet?
 { 
	if (test_gps(0) > 0) 								// GPS meldet sich ?
  {	  	
   if (fp.gps==4)												// LC76F GPS Typ 4?
	 {
	  osDelay (300);											// Warte bis erste NMEA Nachrichten empfangen	
	  gps_command (T_C242C,2);						// PGKC Kommando 242, RMC Ausgabe festlegen
		bx2=rx2;														// Leere Puffer		
		if (wait_message_2(T_CRLF,250,UART2)>0)	// Warte auf Antwort an UART2
    {	if (compare(T_pgkc,cbuf,0,sizeof(T_pgkc))>=0)	gps_pending=1;	} 		// Wenn PMTK Ackknowledge GPS Positionsbestimmung	
   } // end if LC76F GPS Typ 4	
	 else if (fp.gps==5)									// LC76G GPS Typ 5?
	 {
		osDelay (100);																		// Warte bis erste NMEA Nachrichten empfangen 
		if (set_gps_messages (1<<4) > 0) gps_pending=1; 	// Nur GGA Ausgabe mit PAIR062 Kommando festlegen 	 
   }		 
	 if (!gps_pending) gps_power(0);			// GPS Positionsbestimmung nicht initiert dann GPS power off
  }
 } // end if GPS Positionsbestimmung noch nicht gestartet	 
 else																			// Positionsbestimmung ist bereits gestartet
 {	
	Init_UART2 (115200); 										// LC76F oder LC76G mit 115200 Baud an UART2
	do																
  {				
	 if (wait_message_2 (T_CRLF,1500,UART2)>0) // Warte auf Positionsnachricht
	 {
	  if (compare(T_grmc,cbuf,0,sizeof(T_grmc))>=0)	// $ Zeichen und RMC Nachrichtenanfang im Puffer?
		{
		 //if (connect&UART0) putln (cbuf);		// XXX
		 pos=colonpos(cbuf,12);								// Position Positionsfixstatus im RMC Satz	
		 val=cbuf[pos];												// Werte Positionsfixstatus aus	
		 if ((val=='A')||(val=='D'))					// GPS oder DGPS fix?
		 {		
			val=cbuf[colonpos(cbuf,2)];					// Werte Feld Zeit/Datum gültig aus
			if (val=='A')												// Zeit/Datum ist gültig?
			{		// Statt der Satelliten Zeitdaten werden RTC Zeitdaten verwendet, wg. Match von GPS und Messdaten
			 val=cbuf[colonpos(cbuf,4)];				// Latitude N/S Feld lesen
			 if ((val=='N')||(val=='S'))				// Latitude N/S Feld korrekt?
			 {
				if (val=='N') lat=1;							// Nord in Latitude Variable codieren
			  else lat=0;												// dto. Süd
			  lat<<=15;													// 15 Minutenmantissebit links 
				pos=colonpos(cbuf,3)+5;						// Position Latitude Minutenmantisse
				if (fp.gps>2) cbuf[pos+4]=SPACE;	// LC76F liefert 6 statt 4 Minutenmantisse Digits, daher terminieren	
				lat+=atoi(cbuf+pos);							// Mantisse addieren
				lat<<=6;													// 6 Minutenbit links
				pos-=3;														// auf Minuten Zehner positionieren
				lat+=atoi(cbuf+pos);							// Minuten addieren
				lat<<=7;													// 7 Gradbit links
				cbuf[pos]=SPACE;									// Endterminierung Grad
				pos-=2;														// auf Grad Zehner positionieren
				lat+=atoi(cbuf+pos);							// Gradwert addieren	
				val=cbuf[colonpos(cbuf,6)];				// Longitude E/W Feld lesen
        if ((val=='W')||(val=='E'))				// Longitude E/W Feld korrekt?	 
				{
				 if (val=='E') lng=1;							// Ost in Longitude Variable codieren
				 else lng=0;											// dto. West
				 lng<<=15;												// 15 Minutenmantissebit links	
				 pos=colonpos(cbuf,5)+6;					// Position Longitude Minutenmantisse
				 if (fp.gps>2) cbuf[pos+4]=SPACE;// LC76F liefert 6 statt 4 Minutenmantisse Digits, daher terminieren	
				 lng+=atoi(cbuf+pos);							// Mantisse addieren
				 lng<<=6;													// 6 Minutenbit links
				 pos-=3;													// auf Minuten Zehner positionieren
				 lng+=atoi(cbuf+pos);							// Minuten addieren
				 lng<<=8;													// 8 Gradbit links
				 cbuf[pos]=SPACE;									// Endterminierung Grad
				 pos-=3;													// auf Grad Hunderter positionieren
				 lng+=atoi(cbuf+pos);							// Grad addieren	
					
				 utd=SEC/2;												// RTC Halber Sekundenwert
				 utd=utd<<6;											// 6 Minutenbit links schieben			
				 utd+=MIN;												// RTC Minutenwert addieren
				 utd=utd<<5;											// 5 Stundenbit links schieben		
				 utd+=HOUR;												// RTC Stundenwert addieren
				 utd=utd<<7;											// 7 Jahresbit links schieben
				 utd+=(YEAR%2000);								// kurzen RTC Jahreswert addieren
				 utd=utd<<4;											// 4 Monatsbit links schieben			
				 utd+=MONTH;											// RTC Monatswert addieren	
				 utd=utd<<5;											// 5 Tagesbit links schieben
				 utd+=DOM;												// RTC Tageswert addieren	
				 
				 fp.gi++;																				// Inkrementiere Positionsdatenindex		
				 if (!fp.gpsanz || (fp.gi>GPS_POS-1)) fp.gi=0;	// Noch keine Daten oder Index overflow? Ja, reset			 
				 fp.gp[fp.gi].utd=utd;									// Zeitwert in Parameterblock sichern
				 fp.gp[fp.gi].lat=lat;									// Latitude in Parameterblock sichern
				 fp.gp[fp.gi].lng=lng;									// Longitude in Parameterblock sichern
				 
				 if (fp.gpsanz<GPS_POS) fp.gpsanz++;		// Anzahl Positionswerte max. 25
			 
				 gps_pending=0;												 	// GPS Aufgabe erledigt, Positionsbestimmung beendet 
				 gpsfix=0;															// Reset Minutenzähler für Positionsfix
				 break;
				}	 // end if E/W  korrekt	
			 }	// end if N/S korrekt	
			}	// end if Zeit/Datum gültig und RMC vollständig
		 } // end if Positionsfix gültig
		 else if (fp.gps>2) retry++;				// nur 1 Nachricht abwarten wg. langer Wiederholzeit
		 if (gps_pending) 									// Position noch nicht erfolgreich bestimmt
	   {	 
			if (gps_pending++ > GPS_MAX_LIM) 	// Spätestens nach 60 Minuten abschalten
			{	 
			 gpsintv=0;												// Positionsbestimmung abschalten		
			 gps_pending=0;										// GPS Aufgabe nicht in 60 Minuten erledigt -> beenden
			 protocol(GPS_TIME_ERROR);				// Fehler protokollieren			 	
			}	 
		 }	// if end gps_pending
		} // end if Nachricht erwischt?		
   } // end if Nachricht innerhalb Timeout
   else  { gps_pending=0;  break;	}			// Keine Nachrichtensatz -> Abbruch		 
  } while (++retry<2); 	// end while nach 2 Nachrichtenempfängen	
 } // end else Positionsbestimmung ist bereits gestartet	

 Init_UART2 (0);										// bzw. UART2 deinitialisieren 
 
 if (!gps_pending)		// Position bereits bestimmt, Zeitgrenze erreicht oder Start GPS Positionsbestimmung nicht erfolgreich?
  gps_power(0);				// GPS abschalten und deselektieren
 
}

void send_gps_position (uchar ind)	// GPS Einzelposition ausgeben
{
 uint utd, lat, lng;	// Hilfsvariable für Zeit/Datum, geo. Breite und Länge
	
 utd=fp.gp[ind].utd;													// Zeit/Datum aus Parameterblock
 putnumber (utd&0x01F,0xC2); putc('.');								// Datum, Tag
 putnumber ((utd>>5)&0x0F,0xC2); putc('.');
 putnumber ((utd>>9)&0x7F,0xC2); putstr(T_col);
 putnumber	((utd>>16)&0x1F,0xC2); putc(':');					// Zeit, Stunde
 putnumber	((utd>>21)&0x3F,0xC2); putc(':'); 
 putnumber	(((utd>>26)&0x3E),0xC2); putstr(T_col);
		 
 lat=fp.gp[ind].lat;													// geo. Breite aus Parameterblock
 putnumber (lat&0x7F,0xC2); putc('°');
 putnumber ((lat>>7)&0x3F,0xC2); putc('.');
 putnumber ((lat>>13)&0x7FFF,0xC4); putc('\''); putc(',');
 if (lat&(1<<28)) putc('N');
 else putc('S');
 putstr(T_col);
			
 lng=fp.gp[ind].lng;													// geo. Länge aus Parameterblock
 putnumber (lng&0xFF,0xC3); putc('°');
 putnumber ((lng>>8)&0x3F,0xC2); putc('.');	
 putnumber ((lng>>14)&0x7FFF,0xC4); putc('\''); putc(',');
 if (lng&(1<<29)) putc('E');
 else putc('W');		
 newline();			
}

void send_gps_data (void)				// Ausgabe im Parameterblock gespeicherter Positionsdaten
{
 uchar i;							// Hilfsvariable	
 int si;							// Satzindex	
	
 put2str(T_gpsanz,T_ist); 			// Text "Anzahl GPS Positionen = ...
 putnumber(fp.gpsanz,0);				// Anzahl ausgeben
	
 if (fp.gpsanz) 								// Positionsdaten	vorhanden
 {	 
  if (ja(T_daus)>0)							// Abfrage Datenausgabe	 
	{
	 si=fp.gi;										// Index letzter Positionsfix
	 i=fp.gpsanz;	
   while (i)										// Alle Datensätze ausgeben	
	 {
		send_gps_position (si);			// GPS Einzelposition ausgeben 		
		if (--si<0) si=GPS_POS-1;		// Position Index underrun
		i--; 												// Dekrementiere Anzahl
	 } 		
  } // end if Abfrage Datenausgabe
	
	if (ja(T_delete)>0) 				// Abfrage GPS Daten löschen?	
	 fp.gpsanz=0;						 	// Daten löschen
	
 } // end if Positionsdaten	vorhanden	
 newline(); 	
}

