//-----------------------------------------------------------------------------
//  FILE: xmodem.c					PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Xmodem Routinen für serielle Kommunikation
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
//-----------------------------------------------------------------------------
//  VERSION :  1.00
//-----------------------------------------------------------------------------
//  CREATED :   25.02.2015
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//              		25.02.2015: File creation
//									13.07.2015: Hardware version H2 -> H3
//-----------------------------------------------------------------------------

#include "sio.h"
#include "fio.h"
#include "rtc.h"
#include "flash.h"
#include "ramcode.h"
#include "libtool.h"
#include "xmodem.h"
#include "sictxt.h"
#include <string.h>

void xmodemInFlush(int timeout)	// Warte bis alle Zeichen empfangen
{	
	while(getbyte(timeout) >= 0)
	 /* FIO1CLR=CTS */ ;			// Empfang wieder zulassen XXX
}

void xmodem_Cancel (void)	// Verbindungsabbruch
{
 xmodemInFlush(XMODEM_BLOCK_TIMEOUT);		// Warte bis alle Zeichen eingetroffen
 putc(CAN);															// Sende 3 x 'Cancel'
 putc(CAN);
 putc(CAN);
}

void addcrctoblock (uchar *Puffer, ushort blocklen) // Fügt CRC an Datenblock in Puffer an
{
 ushort ebcrc;						// Neue CRC Prüfsumme Block 1
 ebcrc=crc(Puffer,blocklen);		// Neuen CRC bilden
 *(Puffer+blocklen)=ebcrc>>8;	  	// auf CRC Position hinter Datenblock speichern
 *(Puffer+blocklen+1)=ebcrc;	  	// auf CRC Position hinter Datenblock speichern
}

int xmodem_receive (uint filesize, int pageoffset)	// Empfang 1k XMODEM Daten und transfer ins Flash
{												// Übergaben:  	filesize - Max. Dateigröße in Byte
												// 			 	pageoffset - Flash Seitenoffset, neg. keine Flash Speicherung
												// Rückgabe Anzahl Datenblöcke oder Fehlernummer
 uchar retry=XMODEM_RETRY_LIMIT;				// Anzahl Übertragungswiederholungen
 uchar response='C';										// Starte Übertragung mit CRC enabled
 uchar blkno=0;													// Anzahl gelesener 1 k Blöcke
 uint recbytes=0;												// Anzahl empfangener Bytes   
 ushort blocklen;												// Blockgröße 													
 int c;										// Laufvariable	und Zeichen von Schnittstelle
 
 while(retry)							// Nochmal Wiederholen?
 {
  putc(response);						// Starte Übertragung/Packet

  if( (c = getc(XMODEM_BLOCK_TIMEOUT)) >= 0)	// Warte auf erstes Blockbyte
  {
   switch (c)														// Erstes Byte auswerten
   {
    case SOH: if (blkno) blocklen=128;	// start of header eigentlich unzulässig bei 1k xmodem										
																				// aber wg. HyperTerminal Bug als Abschlussblock zulassen
							else											// 128 Byte XMODEM erster Block = Startversuch
							{
								xmodemInFlush(XMODEM_CHAR_TIMEOUT);				// Puffer löschen
								xmodem_Cancel();				// Verbindung abbrechen
								return (XMODEM_ERROR_PACKET_TYPE);
							}
							break;
    case STX:	blocklen=BLOCKSIZE;				// normaler 1024 Byte Datenblock bei 1k xmodem
							break;					 
		case EOT:	putc(ACK);													// Acknowledge, normales Übertragungsende
							xmodemInFlush(XMODEM_CHAR_TIMEOUT);	// Warte auf EOT Wiederholungen				
							return (blkno);											// Rückgabe Anzahl Blocks
		case CAN:	if((c = getbyte(XMODEM_CHAR_TIMEOUT)) == CAN)
							{					   												// Abbruch durch Gegenstelle				  
								xmodemInFlush(XMODEM_CHAR_TIMEOUT/5);  		
								putc(ACK);
								return (XMODEM_ERROR_REMOTECANCEL);
							}
		default:	xmodemInFlush(XMODEM_CHAR_TIMEOUT);	// Empfangspuffer löschen 				 
							response = NAK;
							if (--retry) continue; 
							xmodem_Cancel();					// Verbindung abbrechen
							return (XMODEM_ERROR_PACKET_TYPE);
   } // end switch 
  }	 
  else 			// Timeout - kein Zeichenempfang
  {    
	 if (--retry) continue; 
   return (XMODEM_ERROR_TIMEOUT); 	// Abbruch nach max. Anzahl Wiederholungen
  }	
  
  if (uart_read(blocklen+4,XMODEM_BLOCK_TIMEOUT,0)>0)	 // Ganzer Block eingelesen?
  {										
   P_fpar=(uchar *)&cbuf;									// Pointer auf erstes Byte in SRAM Parameter Puffer									
   if((*P_fpar == (uchar)~(*(P_fpar+1))) 	// Blocknummern korrekt empfangen?
    &&	(crc(P_fpar+2, blocklen+2) == 0))	// und CRC Prüfsumme korrekt?
   {
    if(*P_fpar == blkno+1)								// Richtige(n) Block(-nr.) empfangen?
    {	    
     recbytes += blocklen;								// Empfangene Bytes aufaddieren
		 if (recbytes>filesize)								// Maximal Anzahle Bytes empfangen ?	
  	 { 
   	  xmodem_Cancel ();										// Verbindungsabbruch
      return (XMODEM_ERROR_FILE_TOO_LONG);	// Fehlerrückgabe
  	 }
		 else 					   										// gesendete Datei nicht zu lang
		 {	 	
			P_fpar+=2;													// Zeiger hinter Blocknummer plazieren
			if (blocklen==128)									// Verkürzter 128 Byte SOH Abschlussblock?
			{
			 cind-=2;																			// cind auf Prüfsumme
       while (cind<BLOCKSIZE+2) cbuf[cind++]=STRZ;	// Block mit STRZ füllen			
			 addcrctoblock (P_fpar, BLOCKSIZE);						// CRC an 1024 Byte Position neu berechnen
			}
			if (pageoffset>=0)									// Seitenoffset >= 0?
			flash_pcom(P_fpar,(blkno*pagepk)+pageoffset,FLASH_ERASE_WRITE_1,BLOCKSIZE+2);	// Flash Seite(n) je nach Seitengröße beschreiben	 	 
	   }
		 blkno++;   													// nächster Block
		 retry = XMODEM_RETRY_LIMIT;					// reset Wiederholungen
		 response = ACK;											// Acknowledge antworten
		 osDelay (5);
		 continue;														// nächsten Block anfordern
    } 
    else if(*P_fpar == blkno) 						// Block wiederholt?
    {
		 response = ACK;											// Acknowledge antworten
		 continue;														// nächsten Block anfordern
    }
    else		// nicht mehr synchron -> Abbruch
    {
		 xmodem_Cancel ();										// Verbindungsabbruch
		 return (XMODEM_ERROR_OUTOFSYNC);
    }  
   }
   else	// Packet oder CRC-Fehler
   {		
    if (--retry==0) 
		{	
		 xmodem_Cancel ();					// Verbindungsabbruch	
		 return (XMODEM_ERROR_CRC);	// Prüfsumme falsch
		}	
    xmodemInFlush(XMODEM_CHAR_TIMEOUT/5);
    response = NAK;
		osDelay (5); 
    continue;
   }
  }
  else	 			// Timeout bei zuwenig Zeichen
  {	
   retry--;	   
   xmodemInFlush(XMODEM_BLOCK_TIMEOUT);
   response = NAK;		// Wiederholanforderung senden   
  }    
 } // end while retry

 // Max. Anzahl Wiederholungen überschritten
 xmodem_Cancel ();		// Verbindungsabbruch
 return (XMODEM_ERROR_RETRYEXCEED);
}

int sendblock (uchar typ, uchar blno, uchar get_C)	// Datenblock in cbuf ausgeben
{			 							// Übergabe typ - SOH oder STX
 										// 			blno - fortlaufende 8 Bit Blocknummer
 										//			getC - 1/0 warte auf Sendeaufforderung 'C'
 int c;																	// Zeichenempfang
 uint i;																// Laufvariable Pufferindex
 uint blen=128;													// Blocklänge
 uchar retry=XMODEM_RETRY_LIMIT/2;			// Anzahl Übertragungswiederholungen
 int error=XMODEM_ERROR_OUTOFSYNC;			// Max. Antwortzeit überschritten

 if (typ==STX) blen=BLOCKSIZE;					// bei STX Blocklänge = 1024
 addcrctoblock ((uchar *)&cbuf, blen); 	// Fügt CRC an Block

 do
 { 
  putb(typ);														// Ausgabe Blocktyp
  putb(blno);														// Blocknummer
  putb(~blno);													// Einer-Komplement Blocknummer
  for (i=0;i<blen+2;i++) 
	{ 
	 putb(cbuf[i]);												// Block + CRC senden
	 if (connect&UART1)										// Modem angeschlossen?
    if ((FIO0PIN&(CSAF|CSBF))==0)				// BT selected	
     if (i%260==256)	osDelay(3);				// 3 ms Pause wg. Laird BT Modul	 
  } 
  c=getbyte(2*XMODEM_BLOCK_TIMEOUT);		// warte auf Antwort
  if (c==ACK) 													// Ackknowledge empfangen
  {
   if (get_C)
   {
    c=getbyte(XMODEM_BLOCK_TIMEOUT);		// warte auf Antwort
    if (c=='C') error=0;								// Erfolg
   } else error=0;											// Erfolg
  }
  else if (c==CAN) return(XMODEM_ERROR_REMOTECANCEL); 	// Verbindungsabbruch oder -änderung
  else if (c<=0) return(XMODEM_ERROR_TIMEOUT);					// Max. Antwortzeit überschritten   
 } while (--retry && error);														// Wiederhole retry mal

 if (retry) return(1);									// Übertragung ok 
 if (error) return(error); 							// Fehlerrückgabe
 return (XMODEM_ERROR_RETRYEXCEED);			// Max. Anzahl Wiederholungen überschritten
}

int send_b64(uchar nullbytes)		// Sendet vier 6 Bit Zeichen aus shiftb64 
{																// Übergabe: nullbytes - Anzahl angefügter Null Füllbytes
 uchar sbcnt=24;								// Laufvariable Anzahl 6 Bit Zeichen
 uchar sixbit;									// 6 Bit Zeichen

 do			// Vier 6 Bit Zeichen zu codieren und auszugeben
 {
  sbcnt-=6;
  sixbit=(shiftb64>>sbcnt)&0x3F;					// 6 Bit Base64 Zeichen bilden
  if (sixbit < 26)			sixbit+=65;				// 'A-Z'
  else if (sixbit < 52)		sixbit+=71;			// 'a-z'
  else if (sixbit < 62)		sixbit-=4;			// '0-9'
  else if (sixbit == 62)	sixbit= '+';		// '+'
  else sixbit = '/';											// '/'
  if (sbcnt<6*nullbytes) sixbit='=';			// '="
  if (putb (sixbit)<0) return(-1);
  if (b64crlf++>=75)	// nach 76 Ausgabezeichen Leerzeile einfügen 
  { if (putb(CR)<0) return(-1);
  	if (putb(LF)<0) return(-1); 
  	b64crlf=0; 
  }	  									
 } while (sbcnt);
 shiftcnt=0;	// Reset Bytezähler	
 return(1);				
}

int putstr_b64 (char *p, int len)	// Base 64 Kodierung Zeichenkette oder einer/mehrere Pufferinhalte
{									// Übergaben: 	*p	- Zeiger auf String/Puffer
									//							len - wenn > 0 Anzahl Zeichen in Puffer
									//									- wenn < 0 Pufferabschluss zu senden
									//									- wenn = 0 Null terminierten String senden
									// Externe Variable: shiftb64 - 3 Byte bzw. 4 x 6 Bit Schieberegister
									//					 shiftcnt - Schiebefortschritt letzter Puffer		  		
									//					 b64crlf  - CRLF Zähler, Base64 nach 57 Bytes 
 
 uint bcnt=0;								// Bytezähler 
 uchar nullbytes=0;					// Anzahl eingefügter Nullbytes 0..2
  
 if (!len) 														// Nullterminierte Zeichenkette?
 {
  if (!*p) return (-1); 							// Erstes Zeichen null - Abruch 
  while (*(p+bcnt)!=0) bcnt++;	 			// Länge Zeichenkette ermitteln
 }
 else if (len<0) bcnt=0;							// Pufferabschluss zu senden?
 else bcnt=len;												// Anzahl Bytes Pufferinhalt

 while (bcnt--)												// Alle Zeichen aus Puffer oder Zeichenkette bearbeiten
 {
  if (shiftcnt++ < 3)									// Übertrage bis zu 3 Bytes in Schiebevariable
  {   
   shiftb64<<=8;											// 8 Bit Linksschieben, LSB löschen
   shiftb64|=*p++;										// Bytes aus Zeichenkette oder Puffer hinzufügen    
  }
  if (shiftcnt==3) 										// 3 Byte im Register - sende Base64 Zeichen
   if (send_b64	(0)<0) return(GPRS_ERROR_TIMEOUT);			
 } // end while bcnt

 if (len<=0)													// Pufferabschluß oder Zeichenkette
 {
  if ((shiftcnt>0)&&(shiftcnt<3))			// Letztes Byte Tripple unvollständig?
  {
   while (shiftcnt++<3) 							// bis 3 Bytes vollständig	
   { shiftb64<<=8; 										// Nullbytes
     nullbytes++; }										// Anzahl angefügter Nullbytes zählen
   if (send_b64	(nullbytes)<0) return(GPRS_ERROR_TIMEOUT);	//Sende 3 Base64 Zeichen
  }
  shiftb64=0;													// Reset nach Zeichenkette oder Pufferabschluß 
  shiftcnt=0; 		 	
  b64crlf=0;
 }
 return (1);
}

void modem_eot (uchar cancel)			// XModem transfer beenden
{
 int c;					   				// Zeichenempfang
 uchar repeat=5;			   			// Anzahl Wiederholungen
 do
 {
  putb(EOT);               			// Übertragungsende schicken
  c=getbyte(XMODEM_BLOCK_TIMEOUT);	// warte auf Antwort
  if (c<0) break;					// Abbruch bei Timeout
 } while ((repeat--!=0)&&(c!=ACK));

 if (!repeat && cancel) putb(CAN);			// Cancel senden 
}

void ymodem_close (void)	// Ymodem transfer beenden
{
 int c;										// Zeichenempfang

 modem_eot (0);						// XModem transfer beenden

 c=getbyte(XMODEM_BLOCK_TIMEOUT);	// warte auf Antwort	
 if (c=='C')											// Sendeaufforderung nächste Batchdatei
 {
  cind=0;
  while (cind<128) cbuf[cind++]=0;	// Protokollblock auf 1k füllen
  sendblock (SOH,0,0);							// Nullblock für Verbindungsende senden
 }
 else putb(CAN);			// Cancel senden 
}

int btransfer (uchar ziel, uchar blno)		// 1k Blocktransfer
{											// Übergabe	modem - 0/1 base64 codierte Direkt-/Xmodem- Ausgabe
											//  				blno -  Blocknummer 8 Bit
 int result=-1;	
 ResetWDT();																				// Watchdog reset
 	
 switch (ziel)																			// nach Ausgabeziel verteilen
 {
	 case 0:	result=putstr_b64 (cbuf, BLOCKSIZE);		// 1k Block in cbuf base64 codiert versenden
					  break;
	 case 1:	result=sendblock (STX, blno,0);					// 1k Block in cbuf mit 1k Xmodem Protokoll ausgeben
						break;	
	 case 2:	result=write_block();										// 1k Block in cbuf in USB Speicherdatei schreiben
 }	 
 return (result);	// hier solte der Programmzeiger nicht ankommen -> Misserfolg
}

int modem_send_pro (uchar modem, uchar blno)	// Protokollausgabe 
{												// Übergaben: 	modem - 0/1 base64 codierte Direkt-/Xmodem- Ausgabe
												//  			blno -  Blocknummer 8 Bit
 int result;
 uint seite;						// Laufvariable Seiten
 uint ind=0;						// Bytezähler Datenausgabe

 seite=fp.pro_start_page;				// Protokolldaten senden
 cind=0;												// Reset Pufferzeiger
 while (seite!=fp.pro_page) 		// bis aktuelle Protokollseite erreicht
 {
  P_fpar=(uchar *)&cbuf[cind];	// Zeiger auf Puffer
  flash_adress(0,seite++);			// Protokollseite addressieren und post-inkrement Seite
  if (seite>PROTOCOLENDPG) seite=PROTOCOLPAGE;				// Überlauf
  flash_command (FLASH_R_MAIN, 8, BLOCKSIZE/pagepk);	// Seite nach cbuf lesen
  cind+=BLOCKSIZE/pagepk;								// Pufferzeiger erhöhen
  ind+=BLOCKSIZE/pagepk;								// Bytezähler  
  if (cind>=BLOCKSIZE) 									// 1k Blockgröße erreicht?
  {
   result=btransfer (modem, blno++);			// Protokollblock in cbuf ausgeben
   if (result<0) return(result); 				// Abbruch bei Fehler
   cind=0;															// Reset Pufferzeiger
  }  
 } // end while

 if (fp.pro_adr!=0) copy_to_cbuf ((char *)probuf, fp.pro_adr);	// aktuelle Protokollseite nach cbuf 
 while (ind<PROT_LEN)																						// Weniger als 2k gesendet
 {	 
  while (cind<BLOCKSIZE) cbuf[cind++]=0;												// Protokollblock auf 1k füllen 
  result=btransfer (modem, blno++);															// Protokollblock in cbuf ausgeben
  if (result<0) return(result); 																// Abbruch bei Fehler
	ind+=cind;																										// Addiere Pufferindex
	cind=0; 																											// reset Pufferindex
 }
 
 return (blno);
}

int modem_send_par (uchar modem, uchar blno)	// 2*1k Ausgabe Parameter
{										// Übergaben: 		modem - 0/1 base64 codierte Direkt-/Xmodem- Ausgabe
										//  							blno -  Blocknummer 8 Bit
 int result;
 int bytewr=0, bytetowr=0;			// Anzahl geschriebener und zu schreibender Bytes

 do
 {
  cind=0;
  if (sizeof(fp)-bytewr<BLOCKSIZE) bytetowr=sizeof(fp)-bytewr; 	// Bytes zu schreiben < 1k?
  else bytetowr=BLOCKSIZE;									   									// sonst ist 1k zu schreiben
  copy_to_cbuf ((char *)(&fp.Kennung+bytewr), bytetowr);				// Kopiere 1k Parameter nach Puffer
  while (cind<BLOCKSIZE) cbuf[cind++]=0;		// Parameterblock ggf. auf 1k füllen 
  result=btransfer (modem, blno++);					// Parameterblock in cbuf ausgeben
  if (result<0) return(result); 						// Abbruch bei Fehler
  bytewr+=BLOCKSIZE;												// Anzahl geschriebener Bytes + BLOCKSIZE
 } while (bytewr<sizeof(fp));								// Bis Parameter vollständig ausgegeben
 
 return (blno);	// Rückgabe Blocknummer
}

int plus_upload_old (void)	// Upload von Textnachrichten und Sonderzeichen in Matrix Controller
{
 uint seite=FREEPAGE+2*pagepk;	// Variable für Flash Seitennummer
 uchar blno=1;					// Xmodem Blocknummer	
 uchar bitmap=0;				// 0 bei Textnachrichten, 1 bei Bitmaps

 do
 {
  if (blno==1) if (waitcms (XMODEM_CHAR_TIMEOUT, '?')<0) break; 	// Hauptmenü Matrix empfangen?
  flash_pcom ((uchar *)cbuf, seite, FLASH_R_ARRAY, BLOCKSIZE+2); 	// Block mit CRC nach cbuf laden
  seite+=pagepk;					// Seitenindex erhöhen
  if (blno==1) 
  { 
   if (!bitmap) putb(STRU);			// 1k Xmodem Text upload einleiten 
   else	putb(STRW);							// 1k Xmodem Bitmap upload einleiten    
  }
  if (blno==1) if (waitcms (XMODEM_CHAR_TIMEOUT, 'C')<0) break; 	// "Datei mit Xmodem..." Hinweis empfangen?
  if (sendblock(STX, blno++,0)<0) break;	// Erfolgreiche Ausgabe des Datenblocks in cbuf?
  if (!bitmap || blno==3) 
  {
   modem_eot (1);									// Xmodem transfer beenden
   if (waitcms (XMODEM_CHAR_TIMEOUT, '!')<0) break; // Keine Upload Quittung empfangen?
  }
  if (!bitmap) { blno=1; bitmap=1; }	// Von Textnachrichten auf Bitmaps wechseln  
 }
 while (blno<3);

 if (blno<3) return (XMODEM_ERROR_TIMEOUT);		// Übertragene Blockanzahl zu klein, Timeout
  	
 return(1); 
}

int plus_upload (void)		// Upload von Matrix Textnachrichten/Sonderzeichen aus Puffer		
{
 uchar blno=1;						
 uchar bitmap=0;		
 ushort offset=43;				// Startoffset der Textseiten in mqbf
	
 osDelay(2);							// 2 ms warten	
 putb('U');								// Upload starten

 do
 {		 
	if (blno==1) 														// erster Block?
	{	
   if (getc(XMODEM_CHAR_TIMEOUT)!='C') return (XMODEM_ERROR_TIMEOUT);	// Keine Sendeaufforderung erhalten -> Abbruch
  }
  memcpy (&cbuf[0],&mqbf[offset],BLOCKSIZE);													// Textseiten/Bitmaps in Sendepuffer kopieren
  if (sendblock(STX,blno++,0)<0) return(XMODEM_ERROR_RETRYEXCEED);		// Kein Xmodem transfer Erfolg -> Abbruch	
	offset+=BLOCKSIZE;																		
	if (!bitmap) { bitmap=1; blno=1; }										// Umschalten von Textseite auf Bitmaps
 } while (blno<=2);	
 
 if (waitcms (XMODEM_CHAR_TIMEOUT, ACK) >0) return(1);	// Ackknowledge Empfang -> Texte und Bitmaps erfolgreich gesetzt
 return(MATRIX_PARAM_ERROR); 														// Misserfolg
}	

int plus_download (void) 	// Download von Matrix Textnachrichten/Sonderzeichen in Puffer 
{													// mit Rückgabe der CRC Prüfsumme 
 uchar blno=1;						// Blocknummer
 uchar bitmap=0;
 int c;	
 char blk, iblk;	
 ushort i;	
 int crcall=0;						// Gesamt CRC über Alles (Text und Bitmaps)
 uint offset=0;	
 uchar retry=10;
	
 osDelay(2);							// 2 ms warten	
 putb('D');								// Download starten 	
 osDelay(30); 						// 30 ms warten
 
 do
 {	
  if (blno==1) putb ('C');							// ersten Block anfordern													

	if( (c = getc(20)) >= 0)	// Warte 20ms auf erstes Blockbyte
  {
   switch (c)														// Erstes Byte auswerten
	 {
		case STX:		blk=getbyte(XMODEM_CHAR_TIMEOUT);			// Blocknummer
								iblk=getbyte(XMODEM_CHAR_TIMEOUT);		// inverse Blocknummer
								if (uart_read(BLOCKSIZE+2,XMODEM_BLOCK_TIMEOUT,offset)>0)	 	// 1k Block mit CRC eingelesen?
								{
								 if (blk==(uchar)~iblk) 																		// Blocknummern korrekt empfangen?
								 {	
									if (crc((uchar *)&cbuf[offset], BLOCKSIZE+2)==0)					// CRC Block-Prüfsumme korrekt
									{					 	 											 
									 offset+=BLOCKSIZE;																				// Offset auf nächstes 1k											 
									 putb (ACK);		// Sende Ackknowledge	
									 blno++;		
									 break;	
									} 
								 }
							  }	
								putb (CAN);		// Abbruch senden 
								return (-1);	// Fehler
		case EOT:	 putc(ACK);								// Acknowledge senden, normales Übertragungsende
							 osDelay(2);							// 2 ms warten	
							 if (!bitmap)	{ bitmap=1; blno=1; osDelay(50); retry=5; break;	}		// Text	empfangen, weiter mit Bitmaps
							 else if (blno==2)																				// Nur einen Bitmapblock empfangen?
							 {
								for (i=offset;i<=3*BLOCKSIZE;i++) cbuf[i]=0x20;					// Block mit Leerzeichen generieren		 																 
							 }
							 crcall=crc((uchar *)&cbuf, 3*BLOCKSIZE);								// Gesamt CRC generieren
							 return(crcall); // Empfang erfolgreich
		default:	if (retry--) continue;					 
							else return(XMODEM_ERROR_RETRYEXCEED); 
   }		 
	}	
	else if ((blno==1)&&retry--) continue;	// Kein Zeichen, erster Block, retry mal wiederholen 		
	else break;															
 } while (blno<=3);				// 2 Bitmap + 1 Text Block + EOT lesen
 putb (CAN);							// Abbruch senden
 return(XMODEM_ERROR_TIMEOUT);	
}	



