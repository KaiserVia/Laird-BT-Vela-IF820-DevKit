//-----------------------------------------------------------------------------
//  FILE: sio.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Serielle Ein-/Ausgabe Routinen für LPC1766
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

#include "cmsis_os.h"
#include "hard.h"
#include "flash.h"
#include "sio.h"
#include "sictxt.h"
#include "libtool.h"
#include "string.h"
#include "rl_usb.h"

int redirect_char_Out (int (*pchar_func)(int a)) // Umleitung Standard Ausgabe
{																									// Must be once called before 
																									// use of serial character I/O
	pchar = pchar_func;			// Output function selection			
 //getc = gchar_func;		// Input function selection
 return(0);
}

int putb(int ch)					// Sende Byte an UART/USB - Schnittstelle(n)
{
 int result=-1;
	
 if (connect&UART0) 						// Schnittstelle UART 0 - Terminal aktiviert  	
 { 
  while (!(U0->LSR & THRE)) {};	// Warte UART0 Transmitter FIFO leer? 
  U0->THR = ch;									// dann sofort senden	 
 } 
 if (connect&UART1) 						// Schnittstelle UART 1 - Terminal aktiviert  	
 {
	while (FIO2PIN & CTS)					// Wait for CTS LOW (modem ready to receive)
  {
	 if (WDTV<1000)								// Abbruch 1ms vor Watchdog reset
	 {
		ResetWDT();								
		return(-1);								
	 }		 
	}	
	while (!(U1->LSR & THRE)) 		// Warte UART1 Transmitter FIFO leer?
  {
	 if (WDTV<1000)								// Abbruch 1ms vor Watchdog reset
	 {
		ResetWDT();								
		return(-1);								
	 }		 
	}	
  U1->THR = ch;									// dann sofort senden 
 }	
 if (connect&UART2)	// Schnittstelle UART 2 - GPS Ausgabe  	
 {
	if (PCONP&PCUART2) 							// Schnittstelle eingeschaltet?
	{	
	 while (!(U2->LSR & THRE)) {};	// Warte UART2 Transmitter FIFO leer? 
   U2->THR = ch;									// dann sofort senden 
	}	 
 }  
 if (connect&USB_LINK)					// USB Host angeschlossen
  do result=USBD_CDC_ACM_PutChar (0, ch);		// Zeichen -> USB	  
		while (result!=ch);											// ggf. wiederholen
 return(1);
}

void putc(int c)			// send a character/byte to the current output device
{	
	if(c == '\n')				// do LF -> CR/LF translation
		pchar('\r');			// send CR
	pchar(c);					// send character
}

int putcbuf (int ch) { cbuf[cind++]=ch; return(1); }	// Zahl in Zeichenpuffer kopieren

/* int putbm10 (int ch)	// Byte an M10 Quecktel mit Zeitüberwachung senden
{	
 uchar retry=5;									// Gesamttimeout 5 * XMODEM_BLOCK_TIMEOUT	
 msdelay=XMODEM_BLOCK_TIMEOUT;	// Langer Timeout wg. GPRS Übertragungspausen	
	
 while (!(U1->LSR & THRE)) {		// Warte UART1 Transmitter FIFO leer? 
	if (!msdelay) 								// nach XMODEM_BLOCK_TIMEOUT
	{
	 if (retry--) { ResetWDT();	msdelay=XMODEM_BLOCK_TIMEOUT;	}	// Nachladen retry mal
	 else return(-1);							// Sonst Abbruch nach retry * XMODEM_BLOCK_TIMEOUT;												 
  } 		
 } // end warte Transmit FIFO leer	
 
 U1->THR = ch;									// danach sofort senden	
 return (1); 
} */

int getkey (uint wartezeit) 		// Warteschleife, Abbruch bei Zeichenempfang
{
 msdelay = wartezeit;						// Abwärtszähler laden
 
 while (bxi==rxi)	// Solange kein Zeichenempfang
 {	 
  if (wartezeit)
  {
   if  (!msdelay) return (-1); 			// Kein Zeichen in Wartezeit empfangen
   if (comchange) return (0);				// Abbruch Änderung Kommunikationsstatus
  }
  if ((connect&MQTT_LINK)&&(mqtt_com>1)) break;	
	osDelay(1);
 }
 return (1);		// Rückgabe Zeichen empfangen  
} 

uchar getkey1 (void)						// Rückgabe Vergleich Empfangspuffer Indizes UART1
{
 if (rxi==bxi) return(0);				// Kein Zeichenempfang, Rückgabe falsch
 else return (1);								// Zeichenempfang, Rückgabe wahr
}	

uchar getkey2 (void)						// Rückgabe Vergleich Empfangspuffer Indizes UART2
{
 if (rx2==bx2) return(0);				// Kein Zeichenempfang, Rückgabe falsch
 else return (1);								// Zeichenempfang, Rückgabe wahr
}

int getbyte (uint wartezeit)		// Warte auf Zeichen von UART(s)
{
 msdelay = wartezeit;						// Abwärtszähler laden
		 
	while (bxi==rxi)								// Warte auf Zeichen
	{
	 if (wartezeit) 						    // Wartezeit definiert?
	 {	 
	  if  (!msdelay) return (-1); 			// Abbruch nach Wartezeit
		else if (comchange) return (-1);	// Abbruch Änderung Kommunikationsstatus 
	 }	 
	 else if (!msdelay)							// ohne Wartezeit
	 {
		ResetWDT();										// Watchdog reset
	  msdelay=XMODEM_BLOCK_TIMEOUT;	// reset alle 3 Sekunden
	 }		 
	 osThreadYield(); 
  }
	
 return (rbuf[++bxi]);				// Bearbeitungsindex erhöhen und Rückgabe
}

int waitcms (uint wartezeit, char c)	// Warte ms lang auf Zeichen
{																			// Übergaben: wartezeit - in ms	
																			//				c	-	Zeichen, 0 alle Zeichen
 msdelay=wartezeit;				
 while (msdelay)
 {
  if (bxi!=rxi) 					   	// Zeichen empfangen?
  {
   bxi++;
   if (!c) return(TRUE);								// Alle zeichen 
   else if (c==rbuf[rxi]) return(TRUE);	// Spezifisches Zeichen   
  }
  if (comchange) return(-1);		// Wechsel Schnittstellenstatus
 } 	
 return(-1);									 
}


int getc (uint timeout)				// Warte auf Eingaben von UART(s)
{
 ResetWDT();									// Watchdog reset	
 //comchange=0;								// ignoriere vorhergehenden Schnittstellenwechsel
 bxi=rxi;											// reset evtl. vorhandene Pufferzeichen
 return (getbyte(timeout));		// Rückgabe Zeichen oder Fehler (<=0)				
}


void newline (void) { putc('\n'); }													// neue Zeile 
void space (int anz) { while (anz--) putc(' '); } 					// Anzahl Leerzeichen ausgeben
void delchar (uchar anzahl) { while(anzahl--) putc(BS); } 	// Ausgabe Anzahl Delete Character
void itemno (uchar i) { putc(i); putc ('.'); putc (' '); }	// Menünummer ausgeben

int is_CRLF (int c) // Prüft ob Zeichen Carriage return oder Line Feed
{					   				// Übergabe: c - Zeichen
 if (c==CR) return (1);
 if (c==LF) return (1);
 return (0);	// weder noch
}

text * sprachauswahl (text *s)	// Setzt Zeiger auf gewählte Ausgabesprache
{																// Durchsuche String bis '\f' der gewählten Sprachnummer
 uchar i = fp.sprache;			// Laufvariable Sprachnummer

 if (i>ANZSPRACHEN-1) i=0;	// Ungültiger Zeiger

 if (*s=='\0') return(s);		// Leere Zeichenkette -> Abbruch
 else if (*s=='\f')					// Mehrsprachiger Text?
 {
  while (i) { do s++; while (*s!='\f'); i--; }	// Durchsuche String bis '\f' der gewählten Sprachnummer
  s++; 											// Zeiger auf Anfang Teilzeichenkette der gewählten Sprache
 }	 	
 return (s);								// Rückgabe Zeiger auf gewählte Sprache
}

void putstr(text * s)			// Stringausgabe für null terminierte Strings
{													// Übergabe Zeiger auf String, Rückgabe Anzahl Ausgabezeichen
	
 s=sprachauswahl(s);			// Setzt Zeiger auf Ausgabesprache	
	
 while (*s)								// Ausgabe bis Nullterminierung
 {
  if (*s=='\f') break;		// nächste Teilzeichenkette (=Sprache) erreicht
  putc(*s++);							// Zeichen- oder Teilzeichenkette ausgeben
 }
}

void putstrCR(text * s)		// Gibt String bis CR oder Terminierung aus
{
 while (*s)								// Ausgabe bis Nullterminierung
 {
	putc(*s++);							// Zeichen- oder Teilzeichenkette ausgeben 
	if (*s==CR) break;  		// Beende bei Zeilenende
 }	 
}

uchar putxchr (text *s, uchar anz)		// Stringausgabe - Anzahl Zeichen
{																			// Übergaben: 	*s - Zeiger auf String
																			// 							anz - Anzahl Ausgabezeichen
 uchar send=0;												// Anzahl Ausgabezeichen

 s=sprachauswahl(s);									// Setzt Zeiger auf Ausgabesprache	

 while (send<anz)											// Anzahl Zeichen ausgeben
 {
  if ((*s=='\f')|(*s=='\0')) break;		// nächste Teilzeichenkette (=Sprache) erreicht
  putc(*s++);													// Zeichen- oder Teilzeichenkette ausgeben 
	send++; 
 }
 
 return (send);					 	// Rückgabe Anzahl ausgegebender Zeichen
}


void put2str(text * s1, text *s2) 	// Zwei Textstrings ausgeben
{ putstr (s1); putstr (s2);	}

void putmstr (text * const strings[]) // Ausgabe Textstringfolge
{								// Übergabe strings - Zeigerarray auf Strings
 uchar ind=0;
 text *s;
 while (1) 
 { s=strings[ind++];		// Zeichenkette zuweisen, Arrayindex erhöhen 
   if (*s=='\0') break;		// Abbruch bei Leerzeichenkette
   putstr (s++); 			// Ausgabe Zeichenkette
 }
}

void putln(text * s)	// prints a null-terminated line 
{
	if (!s) return;			// return on invalid pointer
	putstr(s);				// print the string until a null-terminator
	newline();				// print CRLF	
}

void putln_ifexist (text * s) // Stringausgabe - wenn Null Text "nicht gesetzt"
{
 if (isalpha(*s)) putln(s);		// Wenn nicht Null - ausgeben
 else putln(T_notcon);				// Text "Nicht gesetzt
}

void putqstr (text *qs)	{ putc('"');	putstr(qs); putc('"');	} // Sende Zeichenfolge in Hochkommas

void putnumber(uint n, uchar format) 	// Formatierte Ganzzahlausgabe
{										// Übergabe:	n		- Ausgabewert
										// 				format 	- Bits D0..D5 Anzahl digits 0..32
										//						  		D0..D5 = 0 -> variable Länge			
			 							//	D7	D6		Ausgabe
										//	0	0		dezimal führende Leerzeichen
										//	1	1		dezimal führende Nullen
										//	1	0		hexadezimal führende Nullen
										//	0	1		binär führende Nullen	
																					
 char *p, buf[33];						// Pointer auf und Ausgabepuffer max. 32 digits
 uint x=n;	
 uchar numDigits = format&0x3F;
 uchar padchar='0';	
 uchar base=10;				
 uchar count;   

 switch (format&0xC0)
 {
  case 0x00:	padchar=' '; 			// führende Leerzeichen
  case 0xC0:	break;						// dezimal
  case 0x80:	base=16;	break;	// hexadezimal
  case 0x40:	base=2;		
 }
	 
 if (numDigits) count = (numDigits-1);		// setup string buffer
 else count = sizeof (buf);
 
 p = buf + sizeof (buf);
 *--p = '\0';
	
 *--p = hexchar(x%base); 	// force calculation of first digit to prevent zero printing 
 x /= base;	
	
 while(count--)			// calculate remaining digits
 {
  if(x != 0)
  {
   *--p = hexchar(x%base);	// calculate next digit   
   x /= base;
  }
  else if (numDigits) *--p = padchar;		// no more digits left, pad out to desired length
  else break;
 }
 //if (base==16) {putb ('0'); putb('x');}
 while(*p) putc(*p++); 						// print the string right-justified

 if (base==2) putc('b');	 				// indiziere Binärzahl
 else if (base==16) putc('h');		// indiziere Hexadezimalzahl
}

void put_uint32_big_end	(uint wert) 	// Ausgabe uint32 big endianness
{																			// MSB wird zuerst gesendet
 uchar *Ptr, i=4;
 Ptr=((uchar *) &wert) + 3;
 while (i--) putb(*Ptr--);
}	

uchar getstr (char *buf, uchar n, uchar typ) // Zeichenkette einlesen
{											// *buf 	- Zeiger auf Zielpuffer												
											// n	-	max. Anzahl gelesener Zeichen
											// typ	-	a/h/d/'.' = alphanumeric, hex, decimal, float
  char *Pb;						// Arbeitszeiger
  uchar i = 0, l;			// Pufferindex und Kopie
  int c;							// Eingabezeichen
  Pb=buf;							// Pufferadresse kopieren

  do  {
    c = getc (TERMINAL_CHAR_TIMEOUT); 	// read character within timeout
    if (c <= 0) { if (i) *Pb = 0; return (0); } // Timeout oder Schnittstelle entfernt? 
    if (c == '\r')  c='\n';  			// process carriage return
    if (c == BS  ||  c == DEL)  {    	// process delete and backspace
     if (i)      {
      i--; Pb--;                       	// decrement count                
      putc (BS);                   		// echo backspace                 
      putc (' ');
      putc (BS);  }
    }
    else {
     switch(typ) {
			case 'a':
              if (isalpha(c))       	// ignore all except alphanumeric characters  
							{      
							 if ((c!='?')&&(c!=':'))  // ? und : wg. Menüsteuerung ausschließen
								{
								 putc (*Pb++ = c);       		// echo and store character       
								 if (c=='\\') *(Pb-1)='\f';	// Erweiterung mehrsprachiger Text
                 i++;                     	// and count                      
								}
							}
              break;
      /* case 'h':	
              if (isxdigit(c))  {     	// ignore all except hexadecimal numbers        
              putc (*Pb++ = c);       	// echo and store character       
              i++;            }         // and count                      
              break; */
      case 'd':
              if (isdigit(c))  {     	// ignore all except decimal numbers             
              putc (*Pb++ = c);       	// echo and store character       
              i++;			   }
              break;           
      default:
              if (isdigit(c) || ( c == typ ))  {   	// ignore all except digits and typ signs    
              putc (*Pb++ = c);               			// echo and store character         
              i++;
              break;           }
         }
    }
  if (c=='\n') break;		// Abbruch bei Return Eingabe
 }
 while (i < n);      					// check limit and line feed
 l=i;
 do *Pb++ = 0; while (l++ < n);  	// String termination and clear buffer
 return (i);											// Rückgabe Anzahl gelesener Zeichen
}

uchar getline (char *buf, uchar n, uchar typ) 	// Zeile einlesen
{																// Übergaben: siehe getstr()
 uchar i=getstr (buf, n, typ); 	// Zeichenkette einlesen
 newline();											// neue Zeile
 return (i);										// Rückgabe Anzahl gelesener Zeichen
}

void readline(char *Ppar, uchar length, char type) 	// Lies Zeile, übernehme bei Eingabe
{												 														// Übergaben: 	length - Pufferlänge 
																										//							type siehe getstr()
 int dummy;																// Hilfsvariable
 uchar pass=0;
 if (type=='*') {pass=1; type='a'; }			// Bei Passwort 
 dummy=getstr(cbuf,length-1,type);				// Zeichenkette lesen
 if (!dummy) 
 {
  if (!pass) putstr (Ppar);								// Bisherigen String ausgeben
  else putstr(T_stars);										// Passwortdummy senden
 }
 else if ((dummy==1)&&(cbuf[0]==' ')) *Ppar=0;			// String löschen
 else if (dummy>0) memcpy(Ppar,cbuf,length);			// Eingabe? Ja, mit Terminierung übernehmen
 newline();
}

int getmnumber (text * pk, uint min, uint max, uint multiple)	// Eingabeaufforderung und Variable einlesen
{												// Übergabe min - kleinster zulässiger Wert
												//			max - größter zulässiger Wert											
												//			pk - Pointer auf nullterm. Zeichenkette
												//			multiple - Eingabe muss Vielfaches von multiple sein
 uint anz=0;		// max. Anzahl digits einzulesen
 uint dummy=0;		
 uint ziffern=max;	// Hilfsvariable
 
 while (ziffern) { ziffern/=10; anz++; }	// Anzahl digits berechnen	   
 
 do
 {
  putstr (pk);							// Text Eingabeaufforderung
  if (*pk!='?') putc('=');				
  ziffern=getline(cbuf,anz,'d');		// Wert einlesen, Ergebnis 0 bei Keine Eingabe, Timeout
  if (ziffern) dummy=atoi(cbuf);		// ASCII in Puffer in HEX Wert wandeln
  else return(-1);									// Abbruch
 } while (dummy<min || dummy>max || (multiple && (dummy%multiple)));	// Bis zulässiger Wert eingegeben
 return (dummy);						// Rückgabe Eingabe
}

int getnumber (text * pk, uint min, uint max) 	// Eingabeaufforderung und Variable einlesen
{ return(getmnumber (pk, min, max, 0)); }		// Reduziert Übergabeaufwand, da KGV meist unbenutzt

int getuchar (text *pk, uchar min, uchar max, uchar* parameter)	// Unsigned char Parameter einlesen
{																								// Übergaben * parameter - Zeiger auf Parameterwert
 int wert;																			//			 andere siehe getnumber	()
 wert=getnumber (pk, min, max);									// Wert einlesen
 if (wert<0) return (-1);												// Kein Erfolg -> Abbruch
 *parameter=wert;																// Variablen zuweisen
 return (1); // Rückgabe Erfolg
}

void getsignedchar (text *p1, text *p2, int8_t min, int8_t max, int8_t* par)	// int8t einlesen
{																								// Übergaben *p1	- Erster Textteil Parameter
 uchar anz=0;																		//					 *p2	- Zweiter Textteil Parameter
 uchar wert;																		//				min,max - Zulässige kleinste/größte Eingabe
 int8_t sval;																		//						par - Zeiger auf zu lesenden Parameter
 uchar sign;	
 char *Ptr;	
	
 do {
	sign=0; 
	newline();  
	put2str (p1,p2);															// Parametertext ausgeben
	putstr (T_ist);
  anz=getstr(cbuf,3,'-');												// 3 Zeichen - Ziffern und '-' lesen					 
  if (anz)																			// Gültige Eingabe?
  {
	 Ptr=cbuf;
   if (*Ptr=='-')	{ Ptr++; sign=1; }						// Negatives Vorzeichen
   wert=atoi(Ptr);
   if (sign && (anz>1) && wert && (wert<=128))  // Negativer Wert im Bereich -1 ... -128?
   { sval=-wert;
		 if ((sval>=min)&&(sval<=max)) { *par=sval;  break;}			 // Wert zuweisen und Ende
	 }
   if (!sign && (wert<=127))										// Positiver Wert im Bereich 0...127	
   { sval=wert;
     if ((sval>=min)&&(sval<=max)) { *par=sval;  break;}			 // Wert zuweisen und Ende
	 }		 
  }						
 } while(anz);	// Solange (gültige) Eingabe	
}	

void put_menuitem (text *s, char item)	// Menüpunkt ausgeben
{																				// Übergaben	s	-	Zeiger auf Menüpunkttext
																				//				item - 	Menüpunktnummer
 newline();									// Neue Zeile
 itemno(item);							// Menüpunktnummer
 putstr(s);									// Menüpunkttext
}

void select (char bis, uchar zuruck)  	// Sendet "Ihre Auswahl .." an Terminal 
{                          							// Übergabeparameter:	bis - ergänzt "Ihre Ausw..." 
																				//						zuruck = 1 ergänzt MP zurueck  
 
 if (zuruck) put_menuitem (T_zuruck,bis); // Menüpunkt ". zurück" senden
 newline();																// Leerzeile 
 putstr(T_ausw);													// "Ihre Auswahl 1 bis" senden
 putc(bis);        												// letzte Auswahlnummer
 putc('?');   														// Abschluss
}

int putmenu (text *s, char bis)	// Menüabfrage
{
 int c;

 putstr(s); 											// Menutext
 select (bis,1);									// Auswahl
 c=getc(TERMINAL_CHAR_TIMEOUT);		// Warte auf Zeichen max. 30 Sekunden
 if (isdigit(c)) putc(c);	  			// Zeichenecho
 else if (c=='R') { c=-1; }				// return
 else if (c=='H') { menu=0; c=-1; }	// Hauptmenüaufruf
  
 newline();	newline();					// Neue Zeilen

 return(c);			// Eingabe zurück geben
}

int ja (text *s)				// Prüft Eingabe ob Ja oder Nein
{ 	                    // da JA, NEIN länderspezifisch siehe Definition
												// Übergaben: *s -Fragetext
 char c;	  						// Eingabe
 int result; 
 
 newline();							// Neue Zeile
 put2str (s,T_jn);			// Fragetext und (j/n)? senden
 
 c=getc(TERMINAL_CHAR_TIMEOUT);		// Warte 30s auf Antwort

 if (c!='C') c|=0x20; 				// -> Kleinschreibung
 else return (c);					// Einleitung Modemdialog     

 if ((c == T_ja[fp.sprache]) | (c == 'y')) result=1;  	// 'Yes' 
 else if (c == T_nein[fp.sprache]) result=-1;			// 'Nein'
 else result=0; 
 if (result) putc (c);              					// echo     
 newline();
 return(result);
}

void puterrstr_dtc (uchar ln, uint dtc) { putc(' '); putstr(T_err); putstr("DTC"); putnumber(dtc,0); if (ln) newline(); } // Text "Fehler " formatiert ausgeben

void dctext (text *msg, uint dtc)   // "Fehler DTCxxx <msg>" + Log, ohne Zeilenumbruch
{
 putstr(T_err); putstr("DTC"); putnumber(dtc,0); putc(' '); putstr(msg);
 if (flashsize) protocol (dtc);
}

int puterror_dtc (int errorno, int errorval, uint dtc)		// Fehlermeldung und Nummer ausgeben
{																						// Übergaben 	errorno - Fehlernummer in Fehlerliste
																						// 						errorval - Rückgabewert -1/0 = kein Ausgabewert/kein Fehler						
 int i;												// Index für Fehlermeldungsliste

 if (errorval!=0)																	// Fehler?
 {
  newline();																			// neue Zeile
  putstr(T_err);																	// Fehlermeldung
  putstr("DTC"); putnumber(dtc,0);                       /* eindeutiger Marker */
  if (errorno<0) errorno=200-errorno;							// Neg. Fehlermeldungen 
  i=number_exists (errorno, errno, MAXERRORTEXT); // Alle Einträge in Fehlertabelle prüfen
  if (i>=0) { putc(' '); putstr(errtxt[i]); }										// Fehlertextausgabe
  /* kein Hex-Fallback noetig: DTC zeigt den Code */										// Sonst Fehlernummer in Hexausgabe	
	if (errorval!=-1)
  {		
	 putc(' ');
   putnumber (errorval,5);												// Ggf. Fehler Rückgabewert	ausgeben								 
	}	
  newline();	
 
  if (flashsize) protocol (dtc);							// Falls Flash vorhanden, Fehler dort protokollieren
 }	
 
 return (errorval);																// Fehlerwert durchreichen	
}

void putparameter (text *Ps, uint wert, uint format, text *Pu)	// Parameterwerte formatiert ausgeben
{												// Übergaben	*Ps	-	Zeiger auf Parameterlabel 
												//				wert - Numerischer Ausgabewert
												//				*Pu	-	Zeiger auf nachgestellten Textstring		
												//				format 	- 1. Byte (LSB) Zahlenformat wert
												//					   		- 2. Byte vorangestellte Menüpunktnummer
												//					   		- 3. Byte Bit 0	- ": "
												//								 Bit 1  - keinen Wert ausgeben											
												//								 Bit 2  - Zeilenabschluss senden
												
 uchar smenu=(uchar)(format>>8);					// Menüpunkt maskieren
 							
 if (smenu) put_menuitem (Ps, smenu);				// Ggf. Menüpunkt ausgeben
 else putstr (Ps);													// vorangestellten Textstring ausgeben
 if ((format>>16)&0x01) putstr (T_dpkt);		// Doppelpunkt
 if (!((format>>17)&0x01))									// Wert ausgeben?
  putnumber (wert,(uchar)format);						// Wert formatiert ausgeben
 if (Pu!=T_nil) { putc(' '); putstr (Pu);}	// nachgestellten Textstring ausgeben 
 if (format>>18) newline();									// Zeilenabschluss senden
}

int list_selection	(text *titel, text *liste, uchar *Parameter, uchar items, uchar size)	// Listenauswahl
{								// Übergaben:	*titel	-		Zeiger auf Listenüberschrift
								//						*liste	- 	Zeiger auf Listenanfang
								//						items	-			Anzahl Listeneinträge 1..9, bit7=1 langer Abfragetext "Inre Auswahl etc."
								//						Parameter - für Ergebniseintrag
								//						size	-			Arrayelementgröße
 uchar i;																// Laufvariable
 uchar offset=0;			   								// Offset auf Listentexteinträge
 uint timeout=TERMINAL_CHAR_TIMEOUT;
 int dummy=0;														// Hilfsvariable für Eingabe
 int result=-1;													// Abfrageergebnis
 uchar lprompt=items&0xC0;							// Hilfsvariable für Abfragewiederholung

 items&=0x0F;								// Anzahl Listeneinträge ausmaskieren

 putstr(titel);							// "Titeltext
 for (i=0;i<items;i++)			// für alle Listenpunkte 
 {
  put_menuitem(liste+offset,(i+1)|0x30);	// Menüpunkt als Listeneintrag ausgeben
  offset+=size;
 }

 do
 {	
	if (lprompt==0xC0) select((items+1)|0x30,1);  				// Menüauswahltext	  
  else if (lprompt==0x80) select(items|0x30,0);  		// Menüauswahltext
  else { newline(); putc('?'); }					// Neue Zeile und Ausgabe '?'
  
  dummy=getc(timeout);	// Zeichen einlesen 
  if (dummy>0)  				// kein Timeout 
  {
   if (isdigit(dummy)) 	// Ziffer?
   {
    putc(dummy);						// Echo '0'...'9'
    dummy&=0x0F;						// ausmaskieren
    if ((dummy>=1)&&(dummy<=items))
    {
     *Parameter=dummy-1;	// Ergebnis Parameter zuweisen
		 result=1;	 
		 break; 
    } 
		else if ((lprompt==0xC0)&(dummy==items+1)) break;		// Auswahl zurück 
   } // end ziffer
  }
  if (dummy=='H') { menu=0; break; } 	// Abbruch ins Hauptmenu
  if (is_CRLF (dummy)) break;					// Abbruch Auswahl
 } while (dummy>=0);									// end while
 newline(); newline();								// Zwei Zeilen vor
 return (result);											// Erfolg
}

void sendbuf(text * buf, ushort lastbyte)	// Puffer im Hexformat ausgeben
{											// Übergaben 	*buf 		- Zeiger auf Puffer
 											//				lastbyte 	- Anzahl Bytes zu senden
 ushort i;
  
  for (i=0;i<lastbyte;i++)
  {
   if (!(i%320) && i) getc(TERMINAL_CHAR_TIMEOUT);
   if (!(i%16))
   {
    newline();
    putnumber (i,0x85);
    putc (':');
   }
   putnumber(*buf++,0x82);
   putc(' ');
  }
}

void put_signed_float (int zahl, ushort teiler, uchar vor, uchar nach) // Zahl mit Vorzeichen und Nachkommastelle ausgeben
{										// Übergabe zahl - auszugebende Zahl	
 										//					Teiler - Teilerfaktor
 										// 					vor - Vorkommastellen
										//					nach - Nachkommastelle
										// Achtung: bei Teiler=60 - Minutenzahl in Zeitausgabe hh:mm
 uint ganz, rest;
 char c;

 if (zahl<0) { zahl=-zahl; putc('-'); }	// neg. Vorzeichen
 ganz=zahl/teiler;						// Ganzzahlanteil
 rest=zahl%teiler;						// Rest aus Ganzzahldivision
 putnumber (ganz,vor);				// Vorkommastelle(n)
 if (nach) {									// Nachkommastellen?
  if (teiler==60) putc(':'); 	// Zeitausgabe? Ja, Trennzeichen ausgeben
  else putc (',');					  // sonst Komma ausgeben
 }
 if (teiler==60) teiler=100;			
 while (nach--)							// Nachkommastellen berechnen
 { 
  rest*=10;								
  c=rest/teiler;						// Nachkommastellen berechnen
  putc (c|0x30);						// Ausgabe Nachkommastelle
  rest-=c*teiler;						// Restwert neu bilden
 }
}

void putbat (uint wert)				// Batteriespannung formatiert ausgeben
{															// Übergabe wert - Batteriespannung in 10 mV Einheiten
 put2str(T_batt,T_dpkt);							// Text "Batteriespannung...
 put_signed_float (wert, 100, 2, 2);	// Spannungsausgabe mit 2 Vor- und 2 Nachkommastelle
 putln(T_Volt);	 											// Volt Einheit
}

void wait_return (void)				// Warte auf Return Eingabe 
{									// Achtung: Wird nicht durch Terminalwechsel abgebrochen
 char c;

 putln(T_ende);								// Ausgabe Ende mit Return
 while (1)
 {
  if (bxi!=rxi)  							// Indizeänderung, Zeichen in Puffer?
  {
   bxi++; 										// Bearbeitungsindex erhöhen
   c=rbuf[rxi];
   if (is_CRLF (c)) break; 			// Zeichen ist Return Eingabe -> Ende
  }
  if (!msdelay)
  {
   ResetWDT();						
   msdelay=XMODEM_BLOCK_TIMEOUT;
  }
	osDelay(5);							// Warte 5 Millisekunden
 }
}

void put_schwellwert (text *Ps, text *Parr, uchar *Pw, uchar ind)	// Ein-/Aus-Schwellwerte ausgeben
{														// Übergaben	*Ps	-	Zeiger auf Bezeichner, z. B. Zeichen
														//				*Pb	-	Zeiger in Elementarray, z. B. <30>, <50> ...  
														//				*Pw - Zeiger auf numerischen Ausgabewert										
 uchar i;				// Laufvariable					//				ind - Menüindex
 for (i=0;i<=1;i++)
 {
  put_menuitem (Ps, (2*ind+1+i)|0x30);	// Menüpunkt ausgeben
  putc(' ');
  putstr (Parr);												// Symbol- / Zeichenbezeichner ausgeben
  putc(' ');
  putstr(&T_offon[1-i][0]);							// Text ein/aus
  putstr(T_dpkt);
  putnumber (*Pw,0);										// erst Einschaltwert, im 2. Durchlauf Ausschaltwert ausgeben
  Pw++;
  putc(' ');
  putstr(&T_uall[fp.mph][0]);						// Einheit ausgeben 
 }
}

void put_date_time (char *Pbuf)		// Datum und Zeit aus Puffereintrag ausgeben
{																	// Übergabe *Pbuf - Zeiger auf Datum/Zeit Werte
 uchar i;
 char delim='.';			// Datums-Trennzeichen

 for (i=0;i<6;i++)
 {
  if (i%3==0) putc(';');		
  else putc(delim);			// Trennzeichen
  putnumber(*Pbuf++,0xC2);	// Datum-/Zeitwert 2 stellig
  if (i>2) delim=':';		// Zeittrennzeichen
 }
}

int uart_read (uint anz, uint st_out, ushort ind)	// Anzahl Bytes von UART nach Arbeitspuffer cbuf lesen
{																									// Übergabe anz 	- Anzahl Bytes zu lesen
																									// 					st_out	- Wartezeit bis erstes Zeichen
																									// 					ind 	- Pufferstartindex
 uint stoptime=mstimer+st_out;		// Lange Wartezeit bis Übertragungsbeginn
 uint chartimeout=st_out/6;
	
 cind=ind;	
 
 while (cind<(anz+ind)) 									// Bis alle Zeichen in Puffer
 {
  if (stoptime==mstimer) return(-1);			// Abbruch bei Timeout
  if (bxi!=rxi) 													// Zeichenempfang?
  {   
   cbuf[cind++]=rbuf[++bxi];							// Empfangs- in Arbeitspuffer kopieren   
   stoptime=mstimer+chartimeout;					// Kurze Wartezeit zwischen Zeichen   
  }
  else    osThreadYield();								// Empfang wieder zulassen XXX   
 } // end warte auf Zeichen
 cbuf[cind]=0;														// Zeichenkettenabschluss             
 return (cind);
}

void puttage(uchar eintag)	// Ausgabe der Wochentagskürzel 
{														// Übergabe eintag - aufzulistende Einschalttage
 text *s ;									// Textzeiger
 uchar i,l,k;								// Laufvariable Sprachnummer, Wochentag, Zeichenkürzel
 uchar wkaus=0;							// Flag für bereits Kürzel ausgegeben

 for (l=0;l<=6;l++)					// Jeden Einschalttag prüfen und ggf. ausgeben
 {  
  if (eintag&(1<<l))				// Tagesflag gesetzt
  {
   if (wkaus>0) putc(',');	// Kommatrennung  
   s=&T_tag[l][0];					// Zeiger auf Tagestext
   if (*s=='\f')						// Mehrsprachiger Text?
   {
    i= fp.sprache;
    while (i) { do s++; while (*s!='\f'); i--; }	// Durchsuche String bis '\f' der gewählten Sprachnummer
    s++; 										// Zeiger auf Anfang Teilzeichenkette der gewählten Sprache
   }
   for (k=0;k<WTAG_KURZ;k++) putc(*s++);	// Wochentagskürzel ausgeben
   wkaus++;	
  }	 // end Tagesflag gesetzt
 }	// end für jeden Einschalttag
}

void get_weekday (uchar *wparam)		// Wochentage einlesen
{										// Übergabe	wparam - Zeiger auf Wochentags-Parameter
 uchar cres;							// Hilfsvariable für Eingabe

 if(list_selection(T_wtag, &T_tag[0][0], &cres, 0x87, sizeof(T_tag[0]))>0)	// Listenauswahl Wochentag
 {
  cres=(1<<cres);							// Tagesflag bilden	    
  if (*wparam&cres) *wparam&=~cres;			// Tagesflag gesetzt -> löschen
    else *wparam|=cres;						// wenn nicht dann setzen
 }
}

int wait_message (text *Pt, uint timeout)		// Warte auf Nachricht
{																						// Übergaben 	*Pt 	- Zeiger auf erwarteten Text
																						// 				timeout	- Timeout bis erstes Zeichen empfangen								
 ushort anzahl=0, start=0;					// Hilfsvariable Länge Zeichenkette und Suchoffset in cbuf
 uint ltime=mstimer+timeout;				// Wartezeitmarke 
	
 if (ltime<mstimer) { mstimer=0; ltime=timeout; }	// mstimer Überlaufkorrektur

 cind=0;														// reset cbuf Index
 //bxi=rxi;													// Indizes Empfangspuffer gleichsetzen	
 while (*(Pt+anzahl)!=0) anzahl++;	// Ermittle Textlänge
 
 do
 {
  if (bxi!=rxi)										// Zeichenempfang?
  { 
   ltime=mstimer+timeout/2;				// Wartezeit zwischen Zeichen verkürzen
   cbuf[cind++]=rbuf[++bxi];			// Empfangs- in Arbeitspuffer kopieren
   if (cind>=start+anzahl) 				// Anzahl neue Zeichen > Länge Zeichenkette
   {
    cbuf[cind]=0;									// Zeichenkettenabschluss
    if (compare((char *)Pt,cbuf,start, start+anzahl)>0) // Zeichenkettenvergleich positiv
	  return (cind); 								// Erfolg
	  else start++;	   							// Suchoffset inkrementieren
   }   
  }	 // Ende neues Zeichen
	else 
	{ 
	 ResetWDT();															// Setze Watchdog zurück
   //if (osActiveThread) clear_comchange();		// Thread aktiv - Ignoriere Schnittstellenwechsel 
	 osThreadYield(); 												// gib Taskwechsel frei
	}	 
 } while (ltime>mstimer);				// Abbruch bei Timeout
 return (-1);										// kein Erfolg
}

int wait_message_2 (text *Pt, uint timeout, uchar uart)		// Warte auf Nachricht in UART Puffern
{																													// Übergaben 	*Pt 	- Zeiger auf erwarteten Text
																													// 				timeout	- Timeout bis erstes Zeichen empfangen
																													//				uart		- Empfangsschnittstelle
 ushort anzahl=0, start=0;							// Hilfsvariable Länge Zeichenkette und Suchoffset in cbuf
 uchar (*f_ptr) (void) = getkey1;				// Zeiger auf UART1 Funktion
 if (uart==UART2) f_ptr = getkey2;			// Zeiger auf UART2 Funktion
	
 cind=0;														// reset cbuf Index
 while (*(Pt+anzahl)!=0) anzahl++;	// Ermittle Textlänge
 msdelay=timeout;										// Lange Wartezeit bis Übertragungsbeginn
 do
 {
  if (f_ptr())										// Zeichenempfang?
  { 
   msdelay=timeout/2;							// Kurze Wartezeit zwischen Zeichen
   if (uart==UART2) cbuf[cind++]=gbuf[++bx2];	// Empfangs- in Arbeitspuffer kopieren
   else cbuf[cind++]=rbuf[++bxi];							
   if (cind>=start+anzahl) 				// Anzahl neue Zeichen > Länge Zeichenkette
   {
    cbuf[cind]=0;									// Zeichenkettenabschluss
    if (compare((char *)Pt,cbuf,start, start+anzahl)>0) // Zeichenkettenvergleich positiv
	  return (cind); 								// Erfolg
	  else start++;	   							// Suchoffset inkrementieren
   }   
  }	 // Ende neues Zeichen
	else ResetWDT();							// Setze Watchdog zurück 
 } while (msdelay>0);	 					// Abbruch bei Timeout
 return (-1);										// kein Erfolg
}

int wait_line (uint timeout)		// Warte auf Zeile von Uart
{
 char c;
 uchar CRLF_header=1;	
 uint ltime=mstimer+timeout;			// Wartezeitmarke 
	
 if (ltime<mstimer) { mstimer=0; ltime=timeout; }	// mstimer Überlaufkorrektur
	
 cind=0;														// reset cbuf Index
	
 do
 {
	if (bxi!=rxi)										// Zeichenempfang?
  {
	 c=rbuf[++bxi];									// Zeichen lesen
   if ((c!=LF)&&(c!=CR))					// kein line feed oder carriage return? 
   {
		cbuf[cind++]=c;	
		CRLF_header=0;										
   }		 
	 else if (CRLF_header==0)
   {		 
		 if (c==LF) 
			 { cbuf[cind++]=0 ;					// Zeichenkettenabschluss
				 return (cind);	}					// Rückgabe Zeichenanzahl
	 }	 
  }
  else 
  { ResetWDT();	osThreadYield(); }	// Setze Watchdog zurück und gib Taskwechsel frei	
 } while (ltime>mstimer);	 		// bis Wartezeitmarke Timer überläuft	
 
 return(-1);
}

 












