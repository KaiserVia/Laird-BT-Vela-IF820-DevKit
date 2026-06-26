/* Header Datei für sio.c, Projekt LPC1766 Test */

#ifndef SIO_H_
#define SIO_H_

#include "hard.h"

// Zeichen-Definitionen
#define STX		0x02	   	// start of text
#define ETX		0x03	   	// end of text
#define ESC   0x1B
#define BS		0x08
#define DEL   0x7F  
#define CR    0x0D
#define LF    0x0A
#define SPACE	0x20
#define STRC	0x03	// Strg 'C'
#define STRF	0x06	// Strg 'F'
#define STRG	0x07	// Strg 'G'
#define STRT  0x14	// Strg 'T'
#define STRU 	0x15	// Strg 'U'
#define STRW 	0x17	// Strg 'W'
#define STRZ	0x1A	// Strg 'Z'
					
#define MODEM_CHAR_TIMEOUT					100				

#define hexchar(x)	HexChars[(x)&0x0F]		
static const char HexChars[] = "0123456789ABCDEF";					// Hexadecimal conversion table

extern int redirect_char_Out (int (*pchar_func)(int a)); 	// Umleitung Standard Ausgabe
extern int putb(int ch);																		// Sende Byte an UART - Schnittstelle(n)
//extern void putb_t(int ch);																// Sende Byte an UART0/USB Terminal - Schnittstelle(n)
extern void putc(int c);																		// Zeichen an aktuelle Ausgabeeinheit senden
extern int putcbuf (int ch); 																// Ausgabe Zeichen in Zeichenpuffer
extern int putbm10 (int ch);																// Byte an M10 Quecktel mit Bytewiederholung bei RTS Stop senden
extern void putstr(text *s);																// Ausgabe null-terminierte Zeichenkette
extern void putstrCR(text * s);															// Gibt String bis CR oder Terminierung aus
extern uchar putxchr (text *s, uchar anz);									// Stringausgabe - Anzahl Zeichen
extern void put2str(text * s1, text *s2); 									// Zwei Textstrings ausgeben
extern void putmstr (text * const strings[]); 							// Ausgabe Textstringfolge
extern void putln(text * s);																// Ausgabe null-terminierte Zeile
extern void putqstr (text *qs);															// Sende Zeichenfolge in Hochkommas
extern void putln_ifexist (text * s); 											// Stringausgabe - wenn Null Text "nicht gesetzt"
extern void newline (void); 																// neue Zeile
extern void delchar (uchar anzahl);  												// Ausgabe Anzahl Delete Character
extern void space (int anz);																// Ausgabe Anzahl Leerzeichen
extern int is_CRLF (int c); 																// Prüft ob Zeichen Carriage return oder Line Feed
extern void putnumber(uint n, uchar format); 								// Formatierte Ganzzahlausgabe
extern void put_uint32_big_end	(uint wert); 								// Ausgabe uint32 big endianness
extern void itemno (uchar i); 															// Menünummer ausgeben
extern void select (char bis, uchar zuruck);  							// Sendet "Ihre Auswahl .." an Terminal
extern void put_menuitem (text *s, char item);							// Menüpunkt ausgeben
extern int getkey (uint wartezeit); 												// Warteschleife, Abbruch bei Zeichenempfang
extern uchar getkey1 (void);																// Rückgabe Vergleich Empfangspuffer Indizes UART1
extern uchar getkey2 (void);																// Rückgabe Vergleich Empfangspuffer Indizes UART2
extern int getbyte (uint wartezeit);												// Warte auf Zeichen von UART(s)
extern int getc (uint timeout);															// Lösche Puffer und warte auf Eingaben von UART(s)
extern uchar getline (char *buf, uchar n, uchar typ); 			// Zeile einlesen
extern void readline(char *Ppar, uchar length, char type);  // Lies Zeile, übernehme bei Eingabe
extern int putmenu (text *s, char bis);											// Menüabfrage
extern int getnumber (text * pk, uint min, uint max); 			// Eingabeaufforderung und Variable einlesen
extern void getsignedchar (text *p1, text *p2, int8_t min, int8_t max, int8_t* par);	// signed int8t einlesen
extern int getuchar (text *pk, uchar min, uchar max, uchar* parameter);	// Unsigned char Parameter einlesen
extern int ja (text *s);																		// Prüft Eingabe ob Ja oder Nein
extern void puterrstr_dtc (uchar ln, uint dtc);     /* Fehler <code> <subsystem> + Log */
extern int  puterror_dtc (uint dtc, int errorval);  /* Fehlermeldung ausgeben, Wert zurueck */
extern void dctext (uint dtc);                      /* dtcerr(code) - Fehler + Log */
extern const char *dtc_text (uint code, char *ptyp);/* Kurztext+Typ aus Diagnose-Tabelle, 0=unbekannt */
extern void put_dtc (uint code);                    /* einheitliche Ausgabe F<code> <Kurztext> */
extern void putparameter (text *s, uint wert, uint format, text *u);	// Parameterwerte formatiert ausgeben
extern int list_selection	(text *titel, text *liste, uchar *Parameter, uchar items, uchar size);	// Listenauswahl
extern void sendbuf(text * buf, ushort lastbyte);						// Puffer im Hexformat ausgeben
extern void putbat (uint wert);															// Batteriespannung formatiert ausgeben
extern void wait_return (void);															// Warte auf Return Eingabe
extern int getmnumber (text * pk, uint min, uint max, uint multiple);	// Eingabeaufforderung und Variable einlesen
extern void put_schwellwert (text *Ps, text *Parr, uchar *wert, uchar ind);	// Ein-/Aus-Schwellwerte ausgeben
extern void put_date_time (char *Pbuf);											// Datum und Zeit aus Puffereintrag ausgeben
extern int uart_read (uint anz, uint st_out, ushort ind);		// Anzahl Bytes von UART nach Arbeitspuffer cbuf lesen
extern void put_signed_float (int zahl, ushort teiler, uchar vor, uchar nach); // Zahl mit Vorzeichen und Nachkommastelle ausgeben
extern void puttage(uchar eintag);													// Ausgabe der Wochentagskürzel 
extern void get_weekday (uchar *wparam);										// Wochentage einlesen
extern int wait_message (text *Pt, uint timeout);						// Warte auf Nachricht in rbuf 
extern int wait_message_2 (text *Pt, uint timeout, uchar uart);		// Warte auf Nachricht in rbuf oder gbuf
extern int waitcms (uint wartezeit, char c);								// Warte ms lang auf Zeichen
extern int wait_line (uint timeout);												// Warte auf Zeile von Uart mit Timeout

#endif /* SIO_H_ */
