/* Header Datei für xmodem.c, Projekt sis3003m */

#ifndef XMODEM_H
#define XMODEM_H

#include "hard.h"

// xmodem control characters
#define SOH			0x01	   	// start of header
#define EOT			0x04	   	// end of transmission
#define ACK			0x06	   	// ackknowledge
#define NAK			0x15	   	// not ackknowledge
#define CAN			0x18	   	// cancel
#define CPMEOF	0x20			// Alias cpm end of file

#define XMODEM_RETRY_LIMIT			16						// Max. Wiederholungen

#define XMODEM_ERROR_REMOTECANCEL 	-1		
#define XMODEM_ERROR_RETRYEXCEED  	-2	
#define XMODEM_ERROR_OUTOFSYNC	  	-3	
#define XMODEM_ERROR_FILE_TOO_LONG 	-4	
#define XMODEM_ERROR_PACKET_TYPE		-5
#define XMODEM_ERROR_TIMEOUT				-6
#define XMODEM_ERROR_CRC						-8
#define GPRS_ERROR_TIMEOUT					-10
#define USB_WRITE_ERROR							-11
#define MATRIX_PARAM_ERROR					-12

// Funktionen extern bereit stellen
extern int xmodem_receive (uint filesize, int pageoffset);	// Empfang 1k XMODEM Daten und transfer ins Flash
extern int sendblock (uchar typ, uchar blno, uchar get_C);	// Datenblock in cbuf per ymodem ausgeben
extern int btransfer (uchar modem, uchar blno);							// 1k Blockausgabe
extern int modem_send_pro (uchar modem, uchar blno);				// 1k (X)modem oder base64 Ausgabe Protokolldaten
extern int modem_send_par (uchar modem, uchar blno);				// 2*1k Ausgabe Parameter
extern void modem_eot (uchar cancel);												// xmodem transfer beenden
extern void addcrctoblock (uchar *Puffer, ushort blocklen); // Fügt CRC an Datenblock in Puffer an
extern void ymodem_close (void);														// Ymodem transfer beenden
extern int putstr_b64 (char *p, int len);				// Base 64 Kodierung Zeichenkette oder einer/mehrere Pufferinhalte
extern int plus_upload (void);									// Upload von Matrix Textnachrichten/Sonderzeichen aus Puffer
extern int plus_upload_old (void);							// Upload von Textnachrichten und Sonderzeichen in Matrix Controller
extern int plus_download (void); 								// Download von Textnachrichten/Sonderzeichen in den Flash

#endif /* define XMODEM_H */
