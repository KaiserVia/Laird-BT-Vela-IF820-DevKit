//-----------------------------------------------------------------------------
//  FILE: Sictst.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Sammlung Testfunktionen
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


#include "i2cm.h"
#include "flash.h"
#include "ramcode.h"
#include "sio.h"
#include "rtc.h"
#include "libtool.h"
#include "USB_tools.h"
#include "sictst.h"
#include "sictxt.h"
#define DTCBASE 100000
#include "dtc.h"

uchar lookup_i2cdevices (uchar dadr)						// Prüft I2C Device List auf Adresseintrag
{																								// Übergabe Device Adresse: dadr
 uchar i;					// Laufvariable															
	
 for (i=1;i<Anz_I2C_Devices;i++)
   { 
	  if (dadr==i2cdev[i].adr) 
		{	
			putstr(i2cdev[i].label);			
			if (i2cdev[i].ic>0)							// IC Nummer existiert, Chip onboard?	
			{
				putstr(T_ic);	
				putnumber (i2cdev[i].ic,0);		// Ausgabe IC Nummer
			}	
			newline();
	    return(i); 											// I2C Adresse gefunden, Index zurück geben
	 }	
	}
 if (i==Anz_I2C_Devices) putln(i2cdev[0].label);	 
 return (0);		// I2C Adresse nicht gefunden, Index=0 mit default Werten
}	

void i2c_scan	(void)		// Check I2C bus for answering devices
{
 uchar dadr=0;
 uchar found=0;
	
 putstr(T_i2cresp);	
	
 do
 {
	if (write_i2c_dev (dadr,1,0)) 			// Byte senden Erfolg?
	{
	 putnumber(dadr,0x82);
	 putnumber(dadr,4);
	 putstr(T_dec);
   lookup_i2cdevices (dadr);					// Prüfe ob Adresse in Liste bekannter Devices 
	 found++;
	}  // end Erfolg
	dadr+=2;														// nächste Adresse
 }	while (dadr);	
 putnumber (found,2);
 putln(T_i2cfound); 
}

void set_dpp (uchar adr, uchar feature)	// Digitalpoti einstellen 
{											// Übergabe adr - I2C Adresse
											// 			feature - Zusatzfunktion
 int c=0;							// Hilfsvariable Zeicheneingabe
 uchar i;							// Laufvariable	
 ushort ch[4];				// Hilfsvariable I2C Wiper Kanäle Abfrageergebnis
 uint imps[4];				// Hilfsvariable für Kanal Impulse/s Ergebnisbildung	
 int result;					// Hilfsvariable I2C 
 uchar channel=0;			// DPP Kanalauswahl 1

 putmstr(L_dpp);			// +/- Text ausgeben

 if (feature==IC35)	putln(T_imp);		// Verstärkerabgleich? Ja, Text Imp/s hinter Kanälen ausgeben  				
 else newline();			// Sonstige DPP	Einstellungen				

 while (1)
 {  
	ResetWDT();	 																	// Watchdog Timer reset3
	for (i=0;i<4;i++)															// Volatile Wiper lesen
	{ 
   if (i<2) result=(i<<4);											// Wiper 1 und 2	
   else result=((i+4)<<4);											// Wiper 3 und 4
	 ch[i]=read_i2cdev (adr, result|0x0C, 2);			// Volatile Register 2 Byte lesen
	 if (is_CRLF(c))															// Return Eingabe?
	 {																						// Ja, Non-volatile Wiper schreiben
		result+=(2<<4);															// Offset non-volatile Wiper
    if (ch[i]>>8) result++;											// MSB 257 Tap Potis berücksichtigen		 
		osDelay(10);																// Warten erspart Statusabfrage 
    write_i2c_dev (adr,2,(ch[i]<<8U)|result);		// Änderungen schreiben	 		 		 
   }		 
	}	// end for 
	if (is_CRLF(c)) break;												// Return Eingabe? Ja, Ende
  
  if (feature==IC35)				  // Zusatzfunktion für Verstärkerabgleich?
  {
	 FIO1SET=MUX_A;							// MUX IC39, A=1, B=0, Auswahl D-IQA		
	 FIO1CLR=MUX_B;		
   cap20=cap30=cap31=0;				// reset Impulszähler
	 En_Count;									// Enable capture count interrupts Timer 2 und 3
   osDelay(125);						  // Messzeitfenster
	 imps[1]=8*cap30;						// Kanal Poti 0 - zur Zeit D-IQA
	 FIO1SET=MUX_B;							// MUX IC39, A=0, B=1, Auswahl D-IQB
	 FIO1CLR=MUX_A;	
	 cap30=0;
   osDelay(125);						  // Messzeitfenster
	 imps[2]=4*cap31;						// Kanal Poti 1 - zur Zeit D-IFB 	
	 imps[0]=4*cap20;						// Kanal Poti 2 - zur Zeit D-IFA		
	 imps[3]=8*cap30;						// Kanal Poti 3	- zur Zeit D-IQB
   delchar(44);						   	// alte Ausgabe löschen
  }
  else delchar(16); 
	for (i=0;i<4;i++)	putnumber(ch[i],4);	// Ausgabe Wiper Poti 0 bis 3

  if (feature==IC35)					// Zusatzfunktion für Verstärkerabgleich?
  {
	 for (i=0;i<4;i++) putnumber(imps[i],7);			// Ausgabe Pulszahl Kanal  	
   c=getbyte(100);															// Warte 0,1 s auf Eingabe
   bxi=rxi;				// Bearbeitungs- und Emfangs-Zeichenindizes gleichsetzen
  }
  else c=getc(TERMINAL_CHAR_TIMEOUT); 		// Warte timeout lang auf Eingabe
	
  if (c=='H') break;											// Abbruch ohne Schreiben		
  else if ((c>='1') && (c<='4')) 					// Kanalauswahl
	{	
		channel=(c&0x07)-1;										// Kanalauswahl maskieren
		if (channel>1) channel+=4;						// Obere 2 Kanäle	
	}	
  else if ((c=='-') || (c=='+'))					// Inkrement oder Dekrement?
  {
   if (c=='-') result=0x08;
   else result=0x04;
   write_i2c_dev (adr,1,(channel<<4)|result);		// Volatile Wiper inkrement/dekrement		
  } 															
 } // end while 
 
 FIO1CLR=MUX_B;			// unselect IC74 Verstärker IQ Signal
 Dis_CapCnt;				// Disable Capture Count  Interrupts Timer 2 und 3
 newline();
 newline();
}

void test_i2c_device (void)				// Test I2C Devices
{																	// Extern: i2cdev structure mit Device Parametern
 int result;											// Hilfsvariable I2C Abfrageergebnis
 int adr;													// Hilfsvariable Deviceadresse
 uchar i, typ, reg;								// Laufvariable, Hilfsvariable Device Typ und Register
 uchar idl;												// Hilfsvariable Index Deviceliste mit Zusatzinfos	
 
 adr=getnumber ((char *)T_devadr , 1, 255);		// I2C Adresse einlesen
 newline();		
 if (adr>0)
 {	 
  if (write_i2c_dev (adr, 1, 0))		// Prüfe ob Device antwortet		
	{
	 putstr(T_i2cdev);								// Text "I2C Device " 	
	 idl=lookup_i2cdevices (adr);			// Prüfe ob Adresse in Liste bekannter Devices	
	 typ=i2cdev[idl].typ;	
   if (typ==DPP) transceiver(ein);	// Transceiver einschalten		
   while (1)
   {
    put2str(T_i2creg,T_i2cval);
		newline();  
    for (i=0;i<i2cdev[idl].reg;i++) 			
    {	
		 if (typ==DPP) result=read_i2cdev (adr, (i<<4)|0x0C, 2);	// Registerwert (2 Byte) aus DPP lesen			
	   else result=read_i2cdev (adr, i, 1);											// Byteregister aus I2C Expander lesen
     if (result>=0)
     {
	    putnumber (i,0);
	    putc('\t');
			putc('\t'); 
      putnumber (result,i2cdev[idl].format);
	    newline();
     } 
	   else break;	
    } // end for 
    if (result>=0)					 								// Erfolgreich ausgelesen
    { 
		 if ((typ==EXPD)||(typ==DPP))						// I2C Expander oder DPP
		 {		
			if (ja(T_change)<=0) return;	 				// Change? Nein, Abbruch
			reg=getnumber(T_i2creg+1,0,0xFF);			// Register lesen	
			result=getnumber(T_value,0,0x100);		// Wert für Register lesen 
			if (typ==DPP) reg<<=4;								// Registeradresse ins upper nibble schieben
	    if (!write_i2c_dev (adr,2,((result<<8)|reg))) break;	// Einzelregister beschreiben, bei Fehler Abbruch 	     
		 } // end if typ 		
     else return;														// Ende kleiner Expander PCA9554 		
    } // end if (result)
   } // end while  
  } // end Schreiberfolg  
	if (typ==DPP)	puterror (I2C_DPP_ERROR, -1);			// DPP Fehlermeldung ausgeben
	else puterror (I2C_DEVICE_ERROR, -1);						// EXPANDER Fehlermeldung ausgeben
 } // end if adr 
}

int page_test (uint write)	// Löscht oder beschreibt Flash Seiten und vergleicht mit Puffer 2
{														// Übergabe: 	write 0/1	= löschen/scheiben 
 uint pcount=0;																			// Zählvariable für Flash-Seiten 
 uchar fillbyte=0xFF;
 char * Tp=(char *)T_erase;													// Zeiger auf Text Schreiben/Löschen 
 											   
 if (write) { fillbyte=0; Tp=(char *)T_write; }			// Schreiben
 put2str(Tp,T_page);																// Text "Schreibe oder Lösche Seite:

 flash_fill_buffer (fillbyte);											// Beschreibt Flash Buffer 1 mit Füllbyte
 
 putnumber(0,5);           													// Wegen Zeichenposition
 do       
 {  
  flash_adress(0,pcount);														// Setze Seitenadresse
  if (write) flash_command(FLASH_WE_BUF1_MAIN,4,0);	// Schreibe Buffer 1 in main page
  else flash_command(FLASH_ERASE_MAIN,4,0);					// Lösche Page
  if (pcount%8==0)           												// Jede 8 Seite
  {  delchar(5);        														// vorherige Ausgabe löschen
	 putnumber(pcount,5);      												// Seitenzahl ausgeben
	 ResetWDT();																			// Watchdog reset
  }   
  flash_command(FLASH_C_BUF1,4,0);									// Vergleiche Seite mit Buffer 1     
  if ((flash_ready(0)&0xC0)!=0x80) break;  					// Abbruch bei Unterschied oder nicht bereit
  if (bxi!=rxi) break;
 } while (++pcount<maxpage);												// Alle Seiten prüfen 

 delchar(5);        																// vorherige Ausgabe löschen
 putnumber(pcount+1,5);															// Letzte erfolgreiche Seite ausgeben
 if (bxi==rxi)																			// Kein Abbruch
 {
  if (pcount==maxpage) putstr(T_ok); 								// Bis zur letzten Seite? -> Text "ok
  else puterrstr(0); 																// Nein, Text " Fehler

 }
 else bxi++;																				// Zeichen bearbeitet
 Clear_SPI ();																			// Disable SPI
 
 return (pcount);  // Rückgabe letzte erfolgreich verglichene Flash Seite
}

void flash_test (void)			// Test des Flash Speichers
{
 newline();
	
 clear_all();				// Led und Schalter aus

 if (InitSPIFlash () == 0) 	  // Bekannter Flash?
 {
  putnumber(flashsize,0);						// Flashgröße ausgeben
  if (flashsize<128) putstr(T_MB);	// Text "MB"
  else putstr(T_KB); 								// Text "KB"    
 }
 else putstr(T_kein);								// Text "Kein ...
 putln(T_flash);	

 if (!flashsize) { newline(); return; }		// Abbruch wenn kein Speicher

 putstr(T_eraseall);
 if (ja((char *)T_cont)>0)	  	// Fortfahren?
 {
  if (page_test(0) == maxpage)						// Flash Löschtest erfolgreich?
   page_test(1);													// dann auch Flash Schreibtest machen 
  flash_erase (fp.pro_page, PROTBLOCKS*pagepk);			// Protokollseiten im Flash löschen
  flash_erase (PARAMETERPAGE, PARAMBLOCKS*pagepk);	// Parameterseiten im Flash löschen
  delete_protocol();						// Protokoll im Speicher löschen  
  delete_data();							// Messdaten im Speicher löschen    
 }
 putstr (T_2LF);		// Zwei Zeilen vor
}

void test_rtc (uchar silent)		// Vergleichstest Echtzeituhr- und Haupt- Oszillator
{ 
 uint time1,time2;								// Zeitwerte
 int delta;												// Hilfvariable Berechnung Zeitabweichung
 bool	neg = false, fehler=false;	// Negative Abweichung und Fehler	
	
 if (!silent)
 {
  send_date_time (0,0);						// Datum ausgeben
  send_date_time (1,0);						// Zeit ausgeben
 }
	
	// while (getkey(1)>0)
 NVIC_DisableIRQ(RTC_IRQn);						// Disable RTC Interrupt	
 CIIR=IMSEC;													// Sekundeninterrupt freigeben	
 ILR|=ILR;														// Evtl. anstehende Interrupts löschen
 msdelay=1100;												// Abbruch nach spätestens 1,1s		
 tsec=0;															// Reset Sekundenzeitmarke	
 NVIC_EnableIRQ(RTC_IRQn);						// Enable RTC Interrupt
 while (tsec==0) 
 {	 
	 osDelay(10);												// Prüfe etwa alle 10 ms auf Änderung
	 if (!msdelay) break;								// RTC arbeitet nicht Abbruch	
 }
 if (msdelay)
 {	 
  time1=tsec;
  tsec=0;
  while (tsec==0) osDelay(10);					// Prüfe etwa alle 10 ms auf Änderung
  time2=tsec;
  NVIC_DisableIRQ(RTC_IRQn);						// Disable RTC Interrupt	
  CIIR=IMMIN;													// Minuteninterrupt wieder freigeben	
  ILR|=ILR;														// Evtl. anstehende Interrupts löschen 	
  NVIC_EnableIRQ(RTC_IRQn);						// Enable RTC Interrupt	
  delta=1000000U-(time2-time1);				// Zeitabweichung in ppm
  if (delta < 0)  											// RTC Osz. langsamer als Hauptoszillator
  { neg=true;	
	  delta=-delta; }						 	
 }	else fehler=true;									// RTC arbeitet nicht
 
 if (!silent) putstr (T_rtc);					// Text "Echtzeituhr...
 
 if ((delta>MAXPPM) || (fehler!=0)) 				// Abweichung > 50us oder kein Sekundenwechsel
 {
  if (!silent) 					// keine stille Ausgabe
  {
   puterrstr(0);				// Text "Fehler"
   if (!fehler)					// wenn Sekundenwechsel aber Abweichung zu groß
   {   
		if (neg) putc('-');			// Negative Abweichung, RTC langsamer 
    putnumber (delta,0);		// Abweichung in ppm (=µs)
    putstr (T_ppm);
   }
  }
  puterror (RTC_ERROR, -1);	// RTC Fehler ausgeben bzw. protokollieren
 }
 else if (!silent) putln (T_ok); 	// Erfolg 
 
 newline();
}

void write_flash (void)		// Flash Seiten mit virtuellen Daten beschreiben
{
 uchar c;
 uint anzahl;
 int16_t wert;	
 
 c=getnumber(T_flash_w_d,1,2);				// Auswahl Messdaten oder Protokoll
 if (c==1)	// Messdaten
 {
  anzahl=getnumber(T_anzmw,1,0xFFFFFFFF);		// Anzahl Messwerte einlesen
	md_simulation (anzahl, 1);								// Erzeuge Messdaten im Speicher 
 }
 else if (c==2)	// Protokoll
 {
  anzahl=getnumber(T_anzpr,1,1000);		// Anzahl Protokollsätze einlesen
  wert=0;								   
  while (wert++<anzahl) protocol(wert);
 }
 newline(); 		
}

uint test_battery (uchar mode)	// Batterietest
{																// Übergabe mode - 	0/1	ohne/mit Spannungsmeldung															
													
 uint wert;											// Hilfsvariable Wert Batteriespannung 
 uchar i=0;
 ushort * Psh;
 	
 Psh=&fp.u_usb;									// Zeiger auf ersten Spannungsmesswert im Parameterblock 
 if (mode) clear_all();		// Led und Schalter aus	
 do
 {	 
	FIO1CLR=MUX_A|MUX_B;	 
  FIO1SET=(i<<MUX_B_b);											// IC36 MUX auf Spannungskanäle schalten
 
  osDelay(1);	 															// Warte 1 ms
  wert=get_ad(2);														// Batteriespannung CPU AD2 Kanal wandeln

  if (i==1) wert=(wert*SPGMUL12)/SPGDIV12;	// Batteriespannung in mV berechnen
	else wert = (wert*SPGMUL5)/SPGDIV5;				// Spannungsumrechnung in mV bei 2:1 Vorteiler
		
  *Psh++=wert;	 
 }	 
 while (i++<2);	 								// 3 Spannungen -> 3 Durchläufe 
  
 if (mode&0x01)															// Spannungsmeldung?
 {
  putbat(fp.u_in/10);												// Spannungswert formatiert ausgeben
  newline();																// Leerzeile anfügen
 }

 return (fp.u_in);	// Rückgabe Eingangsspannung in mV
}

void licht_test (void)		// Test des Lichtsensors
{

 putln(T_ende);							// Text "Ende mit Return
 put2str(T_licht,T_ist);
 clear_all();								// Led und Schalter aus
	
 while (1)					
 {
	ResetWDT ();											// Watchdog Timer reset  
  putnumber (get_brightness(),4); 	// Lichtsensor auslesen
  if (getkey(500)>=0) break;				// Abbruch bei Zeichenempfang/Schnittstellenwechsel		
  delchar(4);   
 }

 newline(); newline();							// Zwei Zeilen vor
}

void LED_check (void)								// Schneller LED Test 7-Segment und Symbole					
{
 uint sfont;	
 char c;	
	
 putln (T_ende);
	
 num_to_LED (388, 3);												// 18.8 zweifarbig nach LED Anzeigepuffer	 	
 c=show_led_off (0, 50);										// LED Anzeigepuffer ausgeben	
 if (is_CRLF(c)) return;										// Ggf. Abbruch bei Return  
 
 num_to_LED (111, 3);												// 111 zweifarbig nach LED Anzeigepuffer	 	
 c=show_led_off (0, 50);										// LED Anzeigepuffer ausgeben	
 if (is_CRLF(c)) return;										// Ggf. Abbruch bei Return 	
 
 num_to_LED (300, 3);												// 10.0 zweifarbig nach LED Anzeigepuffer	 	
 c=show_led_off (0, 50);										// LED Anzeigepuffer ausgeben	
 if (is_CRLF(c)) return;										// Ggf. Abbruch bei Return 

 
 num_to_LED (488, 0);												// 88 Kreissymbol nach LED Anzeigepuffer
 c=show_led_off (0, 50);										// LED Anzeigepuffer ausgeben	
 if (is_CRLF(c)) return;										// Ggf. Abbruch bei Return 
 
 for (sfont=500;sfont<=504;sfont++)
 {
	num_to_LED (sfont, 0);										// Kreissymbole nach LED Anzeigepuffer
  c=show_led_off (0, 50);										// LED Anzeigepuffer ausgeben	
  if (is_CRLF(c)) return;										// Ggf. Abbruch bei Return	 
 }
}	

void LED_font_test (void)		// Anzeigetest
{
 char c=0;
 int farbe=0, val, dim;
 uint vstart, vend, time;
 uchar fstart, fend;

 val=getnumber(T_numb,0,503);											// Zahl oder Symbol abfragen, 0 für alle 
 if (val<400) farbe=getnumber(T_color+2,1,3); 		// Farbe abfragen bei Zahl
 dim = getnumber(T_duty, 1, 200);									// LED Dimmung einlesen

 if (dim<0) dim=0;																// Keine Eingabe -> variable Helligkeit	

 putln(T_ende);

 if (val >= 0) { vstart=val; vend=val; time=0; }	// Einzelzifferausgabe
 else { vstart=0; vend=503; time=1000; }					// Alle Zahlen bis 499 und Symbole 500 bis 503				
 
 if (farbe >= 0) { fstart=farbe; fend=farbe; }			// Eine Farbe ausgeben
 else { fstart=1; fend=fp.farben; time=1000;}				// Alle definierten Farben
  
 for (val=vstart;val<=vend;val++)
 {  
  if (!dim) get_dimmung ();											// Dimmfaktor bestimmen							 
  //delchar(4);
  //putnumber (dimm,4);
  for (farbe=fstart;farbe<=fend; farbe++)				// Alle eingestellten Farben ausgeben 
  {	 
	 num_to_LED (val, farbe);									// Wert nach LED Anzeigepuffer	 	
	 c=show_led (time, dim);									// LED Anzeigepuffer ausgeben	
   if (is_CRLF(c)) break;										// Ggf. Abbruch bei Return							
  } 
	if (is_CRLF(c)) break;										// Ggf. Abbruch bei Return
 }
 Ledaus ();											// Led Abschaltung
 newline(); newline();					// Zwei Zeilen vor
}

int ledtest1 (void)		// Led Testprogramm/Messedemo Teil Helligkeit
{
 uchar color;  							// Laufvariable Farben
 uint hstufe=1;							// Laufvariable Helligkeitsstufen
 uint hwert;								// Hilfsvariable Helligkeitsberechnung
 uint anzwert=399;	
	
 Ledaus();								// Erstmal alles (wieder) aus
 osDelay(500);						// 1/2s warten
	
 if (!comchange) putparameter (T_hell, 0, 3, T_perc);	// Ausgabe "Helligkeit:   0 % 

 reset_switches ();											// Setzt I2C Expander zurück
 for (color=1;color<=fp.farben;color++)	// Für Grund- und Warnfarbe
 {
	if (color >2) anzwert=199;
	num_to_LED (anzwert,color);								// 19.9 in Puffer 
  hstufe=1;
  while (1) 	// Schleife für Helligkeitsstufen
  {
   if (!comchange)
	 {		
    delchar (5);   
    putnumber (hstufe,3);				// Helligkeitswert ausgeben
    putstr (T_perc);							// Prozent senden
	 }	 
   hwert=(adj_hell()*hstufe)/50;
   if (!hwert) hwert=1;
   if (is_CRLF(show_led (Def_mcycle/2, hwert))) return (-1);	// 750ms LED Farbe anzeigen, Abbruch bei Eingabetaste
   hstufe*=2;   
   if (hstufe==128) hstufe=100;			// Nicht über 100% ausgeben
   if (hstufe>=200) break;					// Abbruchbedingung
  }   
  if (is_CRLF(getc(Def_mcycle))) return (-1); // Eingabetaste
	set_pwm(1,1);
	Ledaus();
	osDelay(500);
 }	// end für jede Farbe
 
 if (!comchange) newline();	// Leerzeile
 return (1);
}

int ledtest2 (void)		// Led Testprogramm/Messedemo Teil Blinken
{
 uchar color, i;  		// Laufvariable Farben und Blinkzyklen

 for (color=1;color<=fp.farben;color++)	// Für jede eingestellte Farbe
  {
	 num_to_LED (124,color);								// 124 in Puffer	
   for (i=1;i<=Def_mcycle/BASECYCLE;i++)
   {
    if (is_CRLF(show_led_off (Def_mcycle/6, 2*adj_hell()))) return(-1);	// Blinken ein und dann aus
    if (is_CRLF(getc(Def_mcycle/6))) return(-1); // Abbruch bei Eingabe
   }
  }	// end jede Farbe blinkend
 return (1);
}

int ledtest3 (void)		// Led Testprogramm/Messedemo Teil Auf-/Abwärtszählen
{
 uchar color;  						// Laufvariable Farben
 uchar i;							// Laufvariable 
 uint symbol;						// LED Anzeigesymbol, 0 keines anzuzeigen
 int step=1;						// Änderungsschritte	 
 
 for (i=fp.vmin[pset];i>=fp.vmin[pset];i+=step)	// zwischen vmin und vmax erst rauf und dann wieder runter
 {   
   if (fp.swgrp) setswitches(i);											// Schalter definiert? Ja, prüfe und setze Schalter
   color=1;
   if (i>=fp.vmix[pset] && fp.farben==3) color=3;			// Mischfarbschwelle überschritten
   if (i>=fp.vcol[pset] && fp.farben>1) color=2;			// Farbschwelle 2. Anzeigefarbe überschritten
	 num_to_LED (i,color);															// Numerischen Wert in Puffer
   if (is_CRLF(show_led (Def_mcycle/4,2*adj_hell()))) return(-1);	// Wert anzeigen
   symbol=ledsymbol(i);					// LED Symbole definiert? Ja, Schaltschwellen prüfen
   if (symbol)
   {
		num_to_LED (symbol,1);																							// Symbol nach Puffer
	  if (is_CRLF(show_led (Def_mcycle/4,2*adj_hell()))) return(-1);	// Symbol anzeigen
   }
   else if (i>=fp.vblk[pset])																// Blinkschwelle überschritten? 
   {
    Ledaus();																								// Ledaus wenn Blinkschwelle überschritten
    if (is_CRLF(getc(Def_mcycle/4))) return(-1);						// Messzyklus/4 Auszeit warten			
   }
   else if (is_CRLF(show_led (Def_mcycle/4,2*adj_hell()))) return(-1);	// Wert wieder anzeigen
   if (i==fp.vmax[pset]) step=-step;		// Zählrichtung wechseln
 } // end for
 return(1);
}

void ledtest (void)		// Led Testprogramm/Messedemo
{
 const funcp ledfunc[3]={ledtest1,ledtest2,ledtest3};  	// Funktionstabelle
 uchar i=0;				// Laufvariable

 putstr(T_ende);		// Text "Ende mit Return
 
 while (1)
 {    
	if (fp.turnsw && get_turnswitch())			// Drehschalter aktiviert und Position nicht off	 
   pset=get_turnswitch()-1;								// Parametersatz aus Schalterposition festlegen  
  if (ledfunc[i++]()<0) break;						// Led Testfunktion aufrufen
  if (is_CRLF(getc(Def_mcycle))) break; 	// Eingabetaste  
  if (i>2) i=0;						// reset Laufvariable
  if (!USB_configured() && (interfaces&USB_LINK))		// USB nicht (mehr) konfiguriert aber registriert?	
	 if (FIO1PIN&VBUS)																// USB Host angeschlossen?
	 {	
		osSignalSet (usbh_thread_id, 1); 			// Signalisiere Verbindung an USB Host thread		 
		osDelay (250);												// Warte wegen Enumeration
    wait_min=Def_wait;										// Setze 3 Warteminuten für nächste GPS, Email, SMS Aktivität		 		 
	 }	  
 }
 clear_all();									// Led und Schalter aus
 get_dimmung ();							// Dimmfaktor neu anpassen
 if (fp.turnsw) timeupdate=1;	// Nachprüfen ob Drehschalter Änderung 
 newline();										// Leerzeile 
 newline();										// Leerzeile
}

void transceiver_test (void)			// Test Transceiver/Verstärker Ansteuerung				
{
 uchar helligkeit;

 putstr(T_bright);								// Text "Brightness
 getuchar (T_inprz, 0, 100, &helligkeit);
 if (helligkeit>0) 
 {
  num_to_LED (188,1);							// 188 in LED Anzeigewert wandeln
  show_led (-1, 2*helligkeit);		// Anzeige in Schieberegister, Helligkeit setzen
 }
 transceiver(3); 									// Transceiver im Testmode einschalten
 bxi=rxi;													// Fortschrittszeichen ggf. löschen
 Ledaus();												// Led komplett abschalten
 transceiver(aus); 								// Transceiver ausschalten
}

void amplifier_adjust (void) // Transceiver und Verstärkerabgleich
{	
 fp.txrcon&=~(GAIN_INH|GAIN_A|GAIN_B);			// Verstärkung löschen
 fp.txrcon|=sense_tab[4];										// 100% Verstärkung zuweisen
 if (transceiver(ein)>0)										// Transceiver und Verstärker ein
 	set_dpp (IC35,IC35);											// Digitalpoti einstellen
 else puterror (I2C_DEVICE_ERROR, -1);			// IC34 I2C Expander Fehler
 fp.txrcon&=~(GAIN_INH|GAIN_A|GAIN_B);			// Verstärkung löschen
 fp.txrcon|=sense_tab[fp.sense];						// eingestellte Verstärkung zuweisen
 transceiver(aus);													// Transceiver und Verstärker aus
}

bool test_ledspot(void)	// Prüfe Steuerchip für externe LED Spotlampe
{
 fp.i2cdev&=~ICLED_b;				// LED Spot I2C Device erstmal austragen bzw. löschen
 FIO0SET=EX12_1;						// 12V Lastversorgung 1 ein
 if (write_i2c_dev(ICLED,3,((0x60<<16)|(0x20<<8)|0x16))) 						// TCA6507 vorhanden?
  {
   write_i2c_dev (ICLED,4,((0x00<<24)|(0x00<<16)|(0x10<<8)|0x13));	// Blinkrhytmus setzen
   write_i2c_dev (ICLED,4,((0x07<<24)|(0x07<<16)|(0x07<<8)|0x10));	// Led ansteuern
   getc(TERMINAL_CHAR_TIMEOUT);																			// Maximal 30 Sekunden warten
   write_i2c_dev (ICLED,4,0x10);	// Led Ausgange reset	
   fp.i2cdev|=ICLED_b;						// LED Spot Device eintragen
   return(TRUE);   
  }
 FIO0CLR=EX12_1;							// 12V Lastversorgung 1 aus
 puterror(TCA6507_ERROR, -1);	// Kein Erfolg, Fehler melden
 return (FALSE); 							// LED Steuerchip nicht ansprechbar
}






