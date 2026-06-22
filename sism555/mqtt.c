//-----------------------------------------------------------------------------
//  FILE: mqtt.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für MQTT Client Kommunikation
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
//              	13.10.2017 File creation
//								
//-----------------------------------------------------------------------------

#include "sio.h"
#include "i2cm.h"
#include "rtc.h"
#include "gsmio.h"
#include "libtool.h"
#include "ramcode.h"
#include "flash.h"
#include "sictxt.h"
#include "mqtt.h"
#include "sicom.h"
#include <string.h>
#define DTCBASE 40000
#include "dtc.h"

text MQTTCONHEAD[10]	= {0x00,0x04,'M','Q','T','T',0x04,0xCE,0x02,0x58};
text MQTTDISCON[2]		= {0xE0,0x00};

void mqtt_remaining_len (uint length)		// Codiert und gibt MQTT Nachrichtenlänge aus
{
 uchar encodedbyte;	
 do																				// Bilde MQTT remaining length
 {
	encodedbyte = length%128;
  length/=128;
  if (length) encodedbyte|=0x80;
  putb(encodedbyte);	 										// Längenbyte senden	 
 } while (length);	
}	

ushort get_top_length (text *topic)		// Bestimme Länge des Gesamttopic Seriennummer/topic
{																			// Übergabe *topic - Zeiger aus subtopic Text
 ushort toplen=0;						// Hilfsvariable Berechnung remaining und topic length	
 text *s;										// Arbeitszeiger zur Stringbearbeitung
	
 s=topic;													// Arbeitszeiger auf Subtopic
 while (*s++) toplen++;						// Längenbestimmung subtopic
 s=fp.serno;											// Arbeitszeiger auf Seriennummer
 while (*s++) toplen++;						// Bestimme Länge Seriennummer	

 return (toplen);			// Rückgabe Zeichenkettenlänge							
}

int mqtt_message_length(uchar *Ptr)				// MQTT Nachrichtenlänge berechnen
{
 uint multiplier=1;
 uint length=0; 
 do 
 {
	length+=(*Ptr&127)*multiplier;  
	multiplier*=128;
  if (multiplier > 128*128*128) return (-1); 	 
 } while (*Ptr++&0x80);	 
 return (length);
}	

int wait_mqtt_connect (uint timeout)					// warte auf MQTTS Verbindungsbestätigung
{																							// Übergaben	timeout - Maximale Wartezeit auf Nachricht	
 int result=-1;																// Ergebnis
 uint ltime=mstimer+timeout;									// Wartezeitmarke 
	
 if (ltime<mstimer) { mstimer=0; ltime=timeout; }	// mstimer Überlaufkorrektur	
	
 rmi=0;																				// mqbf Index reset	

 do
 {	 
  if (bxi!=rxi)																// Zeichen in Emfangspuffer?
  {		
	 mqbf[rmi++]=rbuf[++bxi];												// Pufferzeichen umkopieren	
	 if ((rmi==1)&&(mqbf[0]!=MQTT_CONNACK)) break;	// Falsche Nachricht			
	 if ((rmi==2)&&(mqbf[1]!=2)) break;							// Falsche Länge		 
	 if (rmi>3) result=mqbf[3];											// Connect Ackknowledge ist vollständig	 
  }
	else ResetWDT();																// Watchdog Timer reset
 } while ((ltime>mstimer)&&(result<0));	 					// bis Wartezeitmarke Timer überläuft oder Ergebnis
 return (result);																	// Ergebnisrückgabe
}	

int send_mqtt_disconnect (void)			// MQTT disconnect senden
{	
 if (putb(MQTT_DISCONNECT)>0)				// disconnect code
  if (putb(0x00)>0) return (1);			// Nachrichtenlänge Null				
 return(-1);
}

int get_mqtt_package_handle (void)	// Suche freien Nachrichtensatz in mqttp structure
{
 uchar i=8;													// Laufvariable
 	
 while (i--) if (mqttp[i].mstyp==INVALID) return (i);	// Gib Index des leeren Nachrichtensatzes zurück
 return (-1);																					// kein leerer Nachrichtenslot
}

void note_mqtt_package (char handle, char typ)		// Packt Nachricht in Nachrichtenliste
{																				// Übergabe typ - Nachrichtentyp
 mqttp[handle].mstyp=typ;								// Nachrichtentyp Gerätestatus
 mqttp[handle].t_min=MIN;								// Minute Zeitstempel
 if (typ!=PINGREQ)											// Keine Paketnummer beim Ping
  mqttp[handle].mpid=PacketId++; 				// Paketnummer sichern und erhöhen
 mqttp[handle].t_ms=mstimer;						// Zur Anzeige/Messung der Antwortzeiten
 mqtt_mess_pend++;											// Anzahl vom MQTTS unquittierter Nachrichten + 1	
 t_mqtt_status=0;												// Jeder Publish setzt Minutenzähler Statusversand zurück
}	

int send_mqtt_connect(void)									// MQTT connect senden
{
 ushort strlen=0,i;
 ushort offset;	
		
 cind=0;																			// reset Index
 copy_to_cbuf((char *)&MQTTCONHEAD, 10);			// Variablen Header kopieren
 strlen=strlen_to_cbuf ((char *)fp.serno);		// Länge Seriennummer nach cbuf 
 copy_to_cbuf((char *)fp.serno,strlen);				// Seriennummer = ClientId nach cbuf	
 offset=cind;																	// cbuf Index merken
 cind+=2;																			// Platz für Länge last will topic	 
 str_to_cbuf(T_topbeg);												// Last will topic Teil 1	- Text vt/1.0/device/
 str_to_cbuf (fp.serno);											// Last will topic Teil 2	-	Seriennummer	
 str_to_cbuf (T_leave);												// Last will topic Teil 3 - Text /leave
 strlen=cind-2-offset;												// last will Länge berechnen
 cbuf[offset]=strlen>>8;											// MSB Länge last will topic
 cbuf[offset+1]=strlen;												// LSB Länge last will topic
 cbuf[cind++]=0; 															// MSB Länge last will message
 cbuf[cind++]=0; 															// LSB Länge last will message	
 strlen=strlen_to_cbuf ((char *)fp.serno);		// Länge Seriennummer als User name nach cbuf  
 copy_to_cbuf((char *)fp.serno,strlen);				// Seriennummer als user name nach cbuf
 strlen=strlen_to_cbuf ((char *)T_secret);	  // Länge Passwort nach cbuf
 copy_to_cbuf((char *)T_secret,strlen);				// Passwort nach cbuf
 putb(MQTT_CONNECT);													// Connect senden
 mqtt_remaining_len (cind);										// Berechnet und gibt MQTT Nachrichtenlänge aus	
 for (i=0;i<cind;i++) if (putb(cbuf[i])<0) return(-1);		// Nachricht senden	
 return(1);
}

/* void send_mqtt_topic (text *topic, ushort toplen)	// Topic mit führender Länge senden
{
 putb(toplen>>8);									// MSB topic length
 putb(toplen);										// LSB topic length
 putstr (topic);									// Sende topic		
} */

void topic_to_cbuf (char *topic_part, uchar len, uchar conftype)		// Erstellt kompletten Topic aus Teilstrings
{																											// Übergaben 	topic_part - Zeiger auf letzten Teilstring																											
																											//						len - Länge letzter Teilstring
																											//						conftype - 0/1/2 kein subtopic/configOut subtopic/configIn subtopic
 cind=2;																							// 2 Byte Platz für Topic length	
 copy_to_cbuf ((char *)T_topbeg, sizeof(T_topbeg)-1);	// Topic zusammenkopieren - 1. Teil	vt/1.0/device/
 str_to_cbuf (fp.serno);															// Seriennummer	- 2. Teil	
 if (conftype==1)	copy_to_cbuf (T_cfout, sizeof(T_cfout)-1);				// configOut subtopic?
 else if (conftype==2)	copy_to_cbuf (T_cfin, sizeof(T_cfin)-1);		// configIn subtopic?
 copy_to_cbuf (topic_part, len-1);										// kopiere variablen Text 3
 cbuf[0]=(cind-2)>>8;																	// MSB Topic length
 cbuf[1]=(cind-2);																		// LSB Topic length	
}

void mqtt_subscribe (void)					// Subscribe to topic /seriennummer/subtopic
{																		// Implizite Übergaben cind - topic length
																		//													-	cbuf[0], Beginn Topic string																		
 ushort i=0;		// Laufvariable
	
 concpy=connect;											// Schnittstellenzustand merken
 connect=UART1;												// an Modem senden

 putb(MQTT_SUBSCRIBE);								// MQTT message control senden
 mqtt_remaining_len (cind+3);					// Codiert und sendet remaining length
 putb (PacketId>>8);									// MSB package id senden
 putb (PacketId);											// LSB package id	senden
 while (i<cind) putb (cbuf[i++]);			// Ausgabe topic string
 putb(1);															// Sende QoS=1	
 
 connect=concpy;											// Schnittstellenzustand wieder herstellen			
}	

void send_cmd_subscribe (void)				// Sendet subscribe to command topic
{
 int handle=get_mqtt_package_handle();																						
	
 if (handle<0) return;									// Abbruch wenn Puffer Nachrichtenverfolgung voll

 if (mqtt_debug) {												// MQTT Debug Mode?
	newline ();
  put2str (T_sub, T_cmd);									// Melde Subscribe /command
  putstr (T_col);	}
	
 topic_to_cbuf (T_cmd, sizeof(T_cmd),0);	// Kompletten Topic in cbuf zusammenkopieren					
 mqtt_subscribe ();												// Sende subscribe to topic
 note_mqtt_package (handle, SUBCOM);			// Packt Nachricht in Nachrichtenliste
	
 if (mqtt_debug) putnumber (PacketId-1,0);									// MQTT Debug Mode? Ja, gib Packetnr aus	
	
}	

void send_mqtt_puback (ushort package)	// Ackknowledge to server publish
{
 concpy=connect;	
  /* if (mqtt_debug) {						// MQTT Debug Mode?
	newline ();
  putstr (T_m_pbackr);					// Text "Publish (pid)...
  putnumber (package,0);	} */	
 connect=UART1;									// an Modem senden
 putb(MQTT_PUBACK);							// MQTT control 
 putb(0x02);										// Remaining Length
 putb(package>>8);							// MSB packet Id
 putb(package);									// LSB packet Id	
 connect=concpy; 	
}	

int mqtt_publish (uchar flags)	// Publish to MQTT Server Topics
{																// Übergaben flags -	MQTT flags QoS, DUP, Retain				
																// Implizite Übergaben cbuf - Byte 0/1 MSB/LSB topic length
																//													-	Byte 2, Beginn Topic string
																//													- an Byte topic length steht MSB Package ID
																//		
																				
 ushort i=0;		// Laufvariable 			
	
 concpy=connect;			// Schnittstellenzustand merken
 connect=UART1;				// an Modem senden
	
 if (putb (MQTT_PUBLISH|flags)<0) return(-1);					// Publish control Byte senden	
 mqtt_remaining_len (cind);														// Codiert und sendet remaining length
 while (i<cind) if (putb (cbuf[i++])<0) return(-1);		// Ausgabe publish string		
	
 connect=concpy;	
 return (1);	
}

void mqtt_gpspos	(void)			// Packt letzte GPS Position aus Positionsliste in cbuf ab cind
{
 uint utd=fp.gp[fp.gi].utd;  												// Hilfsvariable auf Zeitwert GPS Position
	
 cbuf[cind++]=utd&0x1F;															// D
 cbuf[cind++]=(utd>>5)&0x0F;												// M
 cbuf[cind++]=(utd>>9)&0x7F;												// Y 	
 cbuf[cind++]=(utd>>16)&0x1F;												// h
 cbuf[cind++]=(utd>>21)&0x3F; 											// m
 cbuf[cind++]=(utd>>26)&0x3E; 											// s
 copy_to_cbuf ((char *)&fp.gp[fp.gi]+4, 8);					// lat, lon der letzten GPS Position in Puffer kopieren
}	

/* void send_mqtt_ping (void)				// MQTT Ping senden - wird durch Status überflüssig
{
 int handle=get_mqtt_package_handle();																						
	
 if (handle<0) return;								// Abbruch wenn Puffer Nachrichtenverfolgung voll
	
 concpy=connect;	
 connect=UART1;													// an Modem senden
 putb(MQTT_PINGREQ);
 putb(0x00);														// Nachrichtenlänge Null	
 connect=concpy;	
 	
 note_mqtt_package (handle, PINGREQ);		// Packt Nachricht in Nachrichtenliste
}	*/

void packetid_to_cbuf (void)	// Publish Topic length und Packet Id nach Puffer
{
															// Package ID einfügen
 cbuf[cind++]=PacketId>>8;		// MSB Package Identifier
 cbuf[cind++]=PacketId;				// LSB Package Identifier	
}

void send_join	(void)				// Baue join Nachricht in cbuf auf und sende diese
{
 uchar i;																							// Laufvariable
 uint16_t crcval;																			// Hilfsvariable zur crc Parameter Ermittlung
 int result;																					// Hilfsergebnis	
 int handle=get_mqtt_package_handle();								// Nachrichtenhandle														
	
 if (handle<0) return;																// Abbruch wenn Nachrichtenpuffer voll
	
 if (mqtt_debug) {																		// MQTT Debug Mode?
	newline ();
  put2str (T_pub, T_join); }													// Melde Publish /join
	
 if ((fp.ex12==2)||(fp.ex12==4))											// viasis PLUS und viatext Textseiten und Bitmap Prüfsumme
 {
	result=plus_com (2);																// Prüfsumme Textseiten und Bitmaps ermitteln
	if (result<0)																				// Kommunikation mit Matrixcontroller nicht erfolgreich?
	{
	 mqtt_state=LEAVE;																	// Disconnect from Server	
	 return;	 
  }	
	mcrc.plus=result;																		// viasis Plus Textseiten und Sonderzeichen Prüfsumme sichern
 }	
 else mcrc.plus=0;																		// Kein PLUS oder viatext
 
 if (mqtt_debug) {																		// MQTT Debug Mode?
  putstr (T_col);
  putnumber (PacketId,0);	}														// Gib Packetnr aus	

 topic_to_cbuf (T_join, sizeof(T_join),0);						// Erstellt kompletten Topic aus Teilstrings	
 packetid_to_cbuf ();																	// Packet Id nach Puffer	
																							// Join Nachrichteninhalt	
 if (fp.ex12==2) cbuf[cind++]=typ_VIASISPLUS;					// signtype VIASISPLUS
 else if (fp.ex12==4) cbuf[cind++]=typ_VIATEXT;				// signtype VIATEXT
 else cbuf[cind++]=typ_VIASIS; 												// signtype VIASIS
 clean_cpy(T_version,&cbuf[cind],10);									// Firmware Version nullterminiert kopieren	
 cind+=10;																						// cind auf Feld Hardware Version, siehe buscom.h
 clean_cpy(fp.hwVersion,&cbuf[cind],12);							// Hardware Version nullterminiert kopieren
 cind+=12;																						// cind	auf Datum/Zeit Feld
 read_date_time (DATE_HEX, &cbuf[cind]);							// ASCII Datum DDMMYY in Puffer
 cind+=3;																							// cbuf index erhöhen
 read_date_time (TIME_HEX, &cbuf[cind]);							// ASCII Zeit hhmmss nach Puffer 
 cind+=3;																							// cbuf index erhöhen
 cbuf[cind++]=fp.utc;																	// UTC Zeitzonenoffset nach Puffer
																											// CRC Bildung über konstante Teile des Parameterblocks 	
 crcval=crc ((uchar *)&fp.Kennung,(uint)&fp.pro_start_page-(uint)&fp.Kennung);	// CRC viasis Parameterblock Bereich 1
 crcval=crcgen((uchar *)&fp.mph,(uint)&fp.u_usb-(uint)&fp.mph,crcval);					// Bereich 2
 crcval=crcgen((uchar *)&fp.btmodem,(uint)&fp.gpsanz-(uint)&fp.btmodem,crcval); // Bereich 3
 crcval=crcgen((uchar *)&fp.pwmex,(uint)&fp.csq-(uint)&fp.pwmex,crcval);				// Bereich 4
 crcval=crcgen((uchar *)&fp.pKennung,(uint)&fp.gp-(uint)&fp.pKennung,crcval);		// Bereich 5	
 mcrc.param=crcval; 		 															// Parameter Prüfsumme sichern
 cbuf[cind++]=crcval>>8;															// MSB 16 Bit CRC
 cbuf[cind++]=crcval;																	// LSB 16 Bit CRC
 cbuf[cind++]=0;																			// Symbol Bitmap Prüfsumme nur beim Vario
 cbuf[cind++]=0;																			// Symbol Bitmap Prüfsumme nur beim Vario
 mcrc.bitmap=0;																				// Vario Prüfsumme hier immer 0 
 cbuf[cind++]=mcrc.plus>>8;														// MSB 16 Bit CRC Plus Textseiten und Bitmaps	
 cbuf[cind++]=mcrc.plus;															// LSB 16 Bit CRC		
 
 if (fp.gps && fp.gpsanz)															// GPS Modul und Position vorhanden?
  mqtt_gpspos	();																			// Letzte GPS Position aus Positionsliste nach cbuf ab cind
 else for (i=0;i<14;i++) cbuf[cind++]=0; 							// Keine Position oder kein GPS
 
 if (iccid[0])																				// SIM Karte ICCID Nummer ermittelt?
	for (i=0;i<sizeof(iccid);i++) cbuf[cind++]=iccid[i]; 	// Ja, in Puffer kopieren und mitsenden
 
 if (mqtt_publish (QoS1)<0)														// Ausgabe der join Nachricht
 { MQTT_fast_disconnect(0); return; }									// Abbruch wenn Versand scheitert
 note_mqtt_package (handle, JOIN);										// Packt Nachricht in Nachrichtenliste
 
 if (mqtt_debug) {												// MQTT Debug Mode?
  putstr (T_col);
  putnumber (mcrc.param,0x80);	
  putstr (T_col);
  putnumber (mcrc.bitmap,0x80);
  putstr (T_col);
  putnumber (mcrc.plus,0x80); }
}

int send_mqtt_status (void)				// Sendet Statusmeldung 
{
 uchar i;	
 uchar pset=0;															// Eingestellter Parametersatz																		
	
 int handle=get_mqtt_package_handle();			// Hilfsvariable																							
 if (handle<0) return(0);										// Abbruch wenn Puffer zur Nachrichtenverfolgung voll
	
 if (mqtt_debug) {													// MQTT Debug Mode?
	newline ();
  put2str (T_pub, T_mstate);								// Melde Publish /status
  putstr (T_col);	}
	
 topic_to_cbuf (T_mstate, sizeof(T_mstate),0);						// Erstellt kompletten Topic aus Teilstrings	
 packetid_to_cbuf ();																			// Packet Id nach Puffer	
																		// Inhalt Status message
 cbuf[cind++]=fp.u_in>>8;																	// MSB U_in	
 cbuf[cind++]=fp.u_in;																		// LSB U_in
 cbuf[cind++]=fp.csq;																			// CSQ Signalstärke
 cbuf[cind++]=fp.hcount>>24;															// MSB Betriebsstunden	
 cbuf[cind++]=fp.hcount>>16;	
 cbuf[cind++]=fp.hcount>>8;	
 cbuf[cind++]=fp.hcount;																	// LSB Betriebsstunden
	
 if (fp.gps && fp.gpsanz)															// GPS Modul und Position vorhanden?
  mqtt_gpspos	();																			// Letzte GPS Position aus Positionsliste nach cbuf ab cind
 else for (i=0;i<14;i++) cbuf[cind++]=0; 							// Keine Position oder kein GPS	
 
 if (fp.turnsw)	pset=get_turnswitch();								// Drehschalter aktiviert? Ja, Position lesen  															
 cbuf[cind++]=pset;				 														// Eingestellten Parametersatz in Statusmeldung einfügen
	
 if (mqtt_publish (QoS1)<0) return(-1);								// Ausgabe des status	topic
 note_mqtt_package (handle, STATUS);			// Packt Nachricht in Nachrichtenliste
	
 if (mqtt_debug) putnumber (PacketId-1,0);						// Gib Packetnr aus	
 return(1);
}	

int send_mqtt_par (void)				// Sendet Parameterblock an MQTTS
{
 uchar * Ptr;	
 ushort i=0;
 uchar concpy=connect;
 int handle=get_mqtt_package_handle();			// Hilfsvariable				
	
 if (handle<0) return(0);										// Abbruch wenn Puffer zur Nachrichtenverfolgung voll	
	
 if (mqtt_debug) {													// MQTT Debug Mode?
	newline ();
  put2str (T_pub, T_tpar);									// Melde Publish /configOut/parameter
  putstr (T_col);	}

 topic_to_cbuf (T_tpar, sizeof(T_tpar), 1);		// Erstellt kompletten Topic aus Teilstrings		
 packetid_to_cbuf ();													// Packet Id nach Puffer	

 concpy=connect;			// Schnittstellenzustand merken
 connect=UART1;				// an Modem senden
	
 if (putb (MQTT_PUBLISH|QoS1)<0) return(-1);				// Publish control Byte senden	
 mqtt_remaining_len (cind+2048);										// Codiert und sendet remaining length

 while (i<cind) if (putb(cbuf[i++])<0) return(-1);	// Publish Header ausgeben
 i=2*BLOCKSIZE;	
 Ptr=&fp.Kennung;																		// Zeiger	auf Parameterblock
 while (i--) if (putb(*Ptr++)<0) return(-1);				// und ausgeben	
 
 note_mqtt_package (handle, PARAMTR);		// Packt Nachricht in Nachrichtenliste
  
 connect=concpy;	
 if (mqtt_debug) putnumber (PacketId-1,0);	// Gib Packetnr im Debug aus	
	
 return(1);	
}	

int send_mqtt_plus (void)							// Sendet viasis Plus Textseiten und Bitmaps
{
 ushort i=0;																// Laufvariable	
 uchar concpy=connect;											// Schnittstellenstatus sichern
 int handle=get_mqtt_package_handle();			// Hilfsvariable für Kontrolle Paketversand	
 int 	rescrc;
	
 if (handle<0) return(0);										// Abbruch wenn Puffer zur Nachrichtenverfolgung voll
	
 if (mqtt_debug) {													// MQTT Debug Mode?
  newline ();
  put2str (T_pub, T_tplus);									// Melde Publish /configOut/plus
  putstr (T_col);	}

 topic_to_cbuf (T_tplus, sizeof(T_tplus), 1);		// Erstellt kompletten Topic aus Teilstrings		
 packetid_to_cbuf ();														// Packet Id nach Puffer

 concpy=connect;			// Schnittstellenzustand merken
 connect=UART1;				// an Modem senden
	
 if (putb (MQTT_PUBLISH|QoS1)<0) return(-1);		// Publish control Byte senden	
 mqtt_remaining_len (cind+3*BLOCKSIZE);					// Codiert und sendet remaining length	
 
 while (i<cind) if (putb (cbuf[i++])<0) return(-1);		// Publish Header ausgeben	
 
 rescrc=plus_com (2);														// Textseiten und Bitmap Payload -> cbuf
 if (rescrc>0) mcrc.plus=rescrc;								// Neuer gesamt CRC über Alles (Text und Bitmaps) 
 for (i=0;i<3*BLOCKSIZE;i++) if (putb (cbuf[i])<0) return(-1); 		// Puffer senden
 
 note_mqtt_package (handle, PARAPLS);						// Packt Nachricht in Nachrichtenliste
  
 connect=concpy;	
 if (mqtt_debug) putnumber (PacketId-1,0);			// Gib Packetnr im Debug aus 
 return(1);	
}	

int send_leave (void)							// Publish zum /leave topic, message null Länge
{
 int handle=get_mqtt_package_handle();			// Hilfsvariable				
	
 if (handle<0) return(-1);									// Abbruch wenn Puffer zur Nachrichtenverfolgung voll	
	
 if (mqtt_debug) {													// MQTT Debug Mode?
	newline ();
  put2str (T_pub, T_leave);									// Melde Publish /leave
  putstr (T_col);	}	
	
 topic_to_cbuf (T_leave, sizeof(T_leave), 0);	// Erstellt kompletten Topic aus Teilstrings	
 packetid_to_cbuf ();												// Packet Id nach Puffer
	
 if (mqtt_publish(QoS1) <0) return(-1);			// Ausgabe des leave topic	
	
 note_mqtt_package (handle, BYE);						// Packt Nachricht in Nachrichtenliste
	
 if (mqtt_debug) putnumber (PacketId-1,0);	// Gib Packetnr aus		
	
 return(1);	
}	

int send_gettime (void)						 					// Publish zum /time topic, Antwort auf GET_TIME Kommando
{
 int handle=get_mqtt_package_handle();			// Hilfsvariable				
	
 if (handle<0) return(-1);									// Abbruch wenn Puffer zur Nachrichtenverfolgung voll	
	
 if (mqtt_debug) {													// MQTT Debug Mode?
	newline ();
  put2str (T_pub, T_ttime);									// Melde Publish /time
  putstr (T_col);	}	
	
 topic_to_cbuf (T_ttime, sizeof(T_ttime), 0);	// Erstellt kompletten Topic aus Teilstrings	
 packetid_to_cbuf ();												// Packet Id nach Puffer
	 
 read_date_time (DATE_HEX, &cbuf[cind]);		// ASCII Datum DDMMYY in Puffer
 cind+=3;																		// cbuf index erhöhen
 read_date_time (TIME_HEX, &cbuf[cind]);		// ASCII Zeit hhmmss nach Puffer 
 cind+=3;																		// cbuf index erhöhen
 cbuf[cind++]=fp.utc;												// UTC Zeitzonenoffset nach Puffer	
	
 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des /time	topic
 note_mqtt_package (handle, TINFO);					// Packt Nachricht in Nachrichtenliste
	
 if (mqtt_debug) putnumber (PacketId-1,0);						// Gib Packetnr aus	
 return(1); 	 
}	

int send_debug_info (uchar debuglevel)			// Dient zum Senden von Debuginformationen auf den cloud topic /+/debug
{																						// Übergabe debuglevel - 0 ... 255
	
 uint seite;						// Laufvariable Flash Seiten
 uint ind=0;						// Bytezähler Datenausgabe	
	
 int handle=get_mqtt_package_handle();			// Hilfsvariable				
 if (handle<0) return (0);									// Abbruch wenn Puffer zur Nachrichtenverfolgung voll	
	
 if (mqtt_debug) {													// MQTT Debug Mode?
	newline ();
  put2str (T_pub, T_debug);									// Melde Publish /+/debug
  putstr (T_col);	}	
 
 topic_to_cbuf (T_debug, sizeof(T_debug), 0);		// Erstellt kompletten Topic aus Teilstrings		
 packetid_to_cbuf ();														// Packet Id nach Puffer	
 	
 switch (debuglevel)
 {	 
	case 0:
	{
   fp.mqtt_pdbq=0;														// Erweiterte Protokollierung abschalten		
   redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
	 putstr (T_debug+1);												// Kopiert "debug" nach cbuf
	 putc(' ');
   putstr (&T_12mode[0][0]);									// Text "off"	
   redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug modes off	
	 break;	
	}
	case 1:
  {
	 fp.mqtt_pdbq=1;														// Erweiterte Protokollierung einschalten		
   redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
	 putstr (T_debug+1);												// Kopiert "debug" nach cbuf
	 putc(' ');
	 putstr (T_name);														// Gerätename wg. Ident OEM Varianten	
   putstr (&T_12mode[1][0]);									// Text "on"	
   redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug modes ein
	 break;	
  }			
	case 2:																			// Status erweiterte Protokollierung
	{	 
   redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
	 putstr (T_debug+1);												// Kopiert "debug" nach cbuf
	 putc(' ');
   putstr (&T_12mode[fp.mqtt_pdbq][0]);				// Text "on"	oder off
   redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug modes ein
	 break;	
  }
	case 3:																			// Protokolldaten senden
  {
   seite=fp.pro_start_page;										// Protokollstartseite
   while (seite!=fp.pro_page) 								// bis aktuelle Protokollseite erreicht
   {
    P_fpar=(uchar *)&cbuf[cind];							// Zeiger auf Puffer
    flash_adress(0,seite++);									// Protokollseite addressieren und post-inkrement Seite
    if (seite>PROTOCOLENDPG) seite=PROTOCOLPAGE;				// Überlauf
    flash_command (FLASH_R_MAIN, 8, BLOCKSIZE/pagepk);	// Seite nach cbuf lesen
    cind+=BLOCKSIZE/pagepk;										// Pufferzeiger erhöhen
    ind+=BLOCKSIZE/pagepk;										// Bytezähler     
   } // end while 
   if (fp.pro_adr!=0) 
	 { copy_to_cbuf ((char *)probuf, fp.pro_adr);	// aktuelle Protokollseite nach cbuf
     ind+=fp.pro_adr;	}													// ind um aktuelle Protokollseite erhöhen		 
   while (ind++<PROT_LEN) cbuf[cind++]=0;				// Weniger als 2k in cbuf -> auffüllen
   if (mqtt_publish (QoS1)<0) return(-1);				// Ausgabe des debug topic level 1	 
	 break;
	}	
	case 4:																			// Bericht Tasks und Verbindungen, GSM, MQTT und GPS Status senden
	{
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
   radiostatus (); 														// Aufruf verdeckte Radiostatus Menüfunktion		
	 redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Sende debug topic level 4
	 break;	
	}	
	case 5:																			// Restart GPS Messung
  {
	 gpsintv=fp.gpsintv;												// gpsintv wieder auf Intervalleinstellung setzen
	 gpsfix=60*gpsintv;													// Triggere GPS Messung Start in Folgeminute	
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
	 putln(T_gpsres);														// Text "Restart Positionbestimmung	..
	 redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 5	
	 break;	
	}		
	case 6:
	{
	 fp.gpsintv=Def_gpsintv;										// Setze default GPS Positionsfix Zeitinterval
 	 gpsintv=fp.gpsintv;	
	 fp.gpsanz=0;																// Null GPS Positionsbestimmungen
	 fp.gi=0;																		// Index Positionsbestimmungen auf Null	
	 gpsfix=60*gpsintv;	 												// Bestimme Zeitpunkt Positionsermittlung	
	 fp.gps=4;																	// LC76 GPS Modul aktivierer
	 interfaces|=GPS_LINK;											// GPS Interface Registrieren	
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
	 putln(T_gpsact);														// Text "GPS L70 aktiviert...
	 redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 6	
	 break;	
	}		
	case 7:																			// GPS deaktivieren
	{
	 fp.gps=0;																	// Kein GPS Modul
	 interfaces&=~GPS_LINK;											// GPS Interface de-registrieren
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten	
	 putln(T_gpsdact);													// Text "GPS off
   redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 7		
   break;		
	}	
	case 8:																			// Vario 16 Symbole wiederherstellen
	{
   redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten		
	 putln(T_noacc);														// Text "kein Zugriff" - da kein Vario
	 redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 8	
	 break;	
  }		
	case 9:
	{
	 strcopy ((char *)T_dmobile, fp.apn);				// APN auf "datamobile" ändern
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten		
	 putln(T_apnset);														// Text "APN datamobile ...
	 redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 9	
	 break;	
	}
	case 10:																		// Force LEAVE
	{
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten	
	 putln(T_leave+1);													// Text "leave"
	 redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 10
   mqtt_state=CLOSE;													// Verbindung schließen
   wait_min=1;																// Eine Warteminute vor neuer Verbindung
	 break;	
  }	
	case 11:																		// Test des Publish auf GET_TIME Command
	{
   send_gettime();																
	 break;
	}	
	case 15:																		// Lösche Messdatenspeicher
  {
	 fp.md_start_page=MEASUREPAGE;		// Seitenzeiger auf Messdatenbeginn im Flash
   fp.md_page=MEASUREPAGE;					// Zeiger auf aktuelle Messdatenseite im Flash
   fp.md_adr=0;											// Reset Messdaten Adresszeiger	
	 MQTT_send=fp.md_page<<16;				// Setze MQTT Versandzeiger in GPREG2 auf Messdatenzeiger
	 MQTT_index=0;										// Reset MQTT Sendeindex in GPREG3
	 md_pg_adr=fp.md_page<<16;				// Setze Kopie Messdatenzeiger in GPREG4 auf Messdatenzeiger	
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten	
	 putln(T_memreset);													// Text "Memory Reset"
   redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 10		
	 break;	
	}	
	case 16:
	{
	 fp.utc=1;																	// Zeitzone UTC+1
   fp.s_w_zeit=2;															// UTC+1 Zeitumstellung		
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten	
	 putln(T_daylight);													// Text "Daylight time change activated"
   redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level 10	
	 mqtt_state=CLOSE;													// Verbindung schließen
   wait_min=1;																// Eine Warteminute vor neuer Verbindung	
	 break;	
  }		
	case 20:																		// Sende viasis Plus Matrix Nachrichten und Bitmaps
  {
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
	 putstr (T_debug+1);												// Kopiert "debug" nach cbuf
	 putc(' ');     	
	 if (fp.ex12==2) { putstr (&T_12mode[2][0]);
		 mqtt_param_get|=PLUSDATA; } 							// Plus Texte und Bitmaps senden und Bit 7 Versand läuft setzen
	 redirect_char_Out (putb);									// Standardausgabe
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug modes ein
	 break;		
	} 	
	case 199:																		// Deaktiviere USB
	{
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
   fp.usb=0;
   interfaces&=~(USB_LINK);
	 putln(T_usbdeact);													// Text "USB deaktiviert ...	 
	 redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level	
	 mqtt_state=CLOSE;													// Verbindung schließen
   wait_min=1;																// Eine Warteminute vor neuer Verbindung
   break;		
  }
	case 200:
	{
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
   fp.gsm=0; 
	 fp.gps=0;		
	 interfaces&=~(GSM_LINK|GPS_LINK);	
   putln(T_gsmdact);													// Text "GSM + GPS off ...
	 redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level
   mqtt_state=CLOSE;													// Verbindung schließen		
	 break;	
	}
	case 201:
	{
	 redirect_char_Out (putcbuf);								// Ausgabe in cbuf umleiten
   putln(T_smtpserv);													// Text "GSM + GPS off ...
	 redirect_char_Out (putb);									// Standardausgabe	
	 if (mqtt_publish (QoS1)<0) return(-1);			// Ausgabe des debug topic level
   mqtt_state=CLOSE;													// Verbindung schließen		
	 set_server_default(SMTP);									// Default Parameter für SMTP Server setzen	
	 break;	
	}
	default:																		// Unbekannter Debuglevel
	{ 	
	 str_to_cbuf (T_debug+1);										// Kopiert String nach cbuf
	 cbuf[cind++]=debuglevel;										// Nur Debuglevel zurückgeben
	 if (mqtt_publish (QoS1)<0) return(-1);			// Sende Antwort auf unbekannte debug topic level 
	}		
 }	
 note_mqtt_package (handle, DBGINFO);					// Packt Nachricht in Nachrichtenliste
 if (mqtt_debug) putnumber (PacketId-1,0);		// Gib Packetnr aus 
 return(1);
}	

int mqtt_md_bytes_to_send (void)		// Ermittelt die Anzahl zu sendender Messdatenbytes
{
 ushort s_page=MQTT_send>>16;				// letzte gesendete Seite ausmaskieren
 ushort s_adr=MQTT_send;						// letzte gesendete Seitenadresse ausmaskieren
 int anzmd;													// Anzahl Messdaten
  	
 anzmd=fp.md_page-s_page;						// Differenz Messdatenseite zu Sendeseite
 if (anzmd<0) anzmd+=PROTOCOLPAGE;	// Negativ -> Überlauf -> Max. Anzahl Messdatenseiten addieren	
 anzmd*=MD_PER_PG;									// Anzahl Seitendatensätze mit Messdatensätze/Seite (26,52,103)	multiplizieren
 anzmd=(anzmd*VSATZLEN+fp.md_adr-s_adr);	// Anzahl Werte auf nicht vollen Seiten 
 
 return (anzmd);			// Rückgabe Anzahl Messwerte zu senden in Bytes	
}

int send_mqtt_data (int less)							// Sendet MQTT DataMessage
{																					// Übergabe less - Sende weniger als 5k Messwerte
 ushort i;																// Laufvariable
 ushort seite, adr;												// Laufzeiger	Versand
 uint pglen;															// Hilfsvariable Seitenlänge	
 int handle;															// Hilfsvariable Pakethandle	
 int bytes_to_send;												// Hilfsvariable Anzahl zu sendender Messwerte
	
 if (((MQTT_send>>16)==fp.md_page)&&(MQTT_send==fp.md_adr))	return(0); // Sendezeiger = Messdatenzeiger dann Ende	
	
 bytes_to_send=mqtt_md_bytes_to_send();				// Anzahl zu sendender Messdatenbytes berechnen

 if (bytes_to_send<=0) return(0);							// Keine Daten bei stündlichem Aufruf oder zuviele -> Ende
 if (bytes_to_send%VSATZLEN) return(0);				// Unvollständiger Datensatz -> Abbruch	
	
 handle=get_mqtt_package_handle();	
 if (handle<0) return(0);											// Abbruch wenn Puffer zur Nachrichtenverfolgung voll

 if (fp.opmode && (bytes_to_send < MQTT_PKG*BLOCKSIZE) && !less) return(0); 	
	
 if (bytes_to_send > MQTT_PKG*BLOCKSIZE) 			// Paketgrößenlimit überschritten?
	 bytes_to_send = MQTT_PKG*BLOCKSIZE;				// Nur maximale Paketgröße versenden
 
 topic_to_cbuf (T_data, sizeof(T_data), 0);		// Erstellt kompletten Topic aus Teilstrings
 packetid_to_cbuf ();													// Packet Id nach Puffer
 
 concpy=connect;															// Schnittstellenzustand merken
 connect=UART1;																// an Modem senden
	
 if (putb (MQTT_PUBLISH|QoS1)<0) return(-1);		// Publish control Byte senden	
 mqtt_remaining_len (cind+2*4+bytes_to_send);	// Codiert und sendet remaining length, 2*4 Bytes Index und Sendezeiger
 for (i=0;i<cind;i++) if (putb (cbuf[i])<0) return(-1);					// Publish Header ausgeben
 put_uint32_big_end	(MQTT_index); 						// Ausgabe MQTT_index uint32 big endianness 
 put_uint32_big_end	(MQTT_send);							// Ausgabe MQTT Messdaten-Sendezeiger uint32 big endianness
 
 mqtt_pbd.bytes_send=bytes_to_send;						// Anzahl gesendeter Messdatensätze sichern
 seite=MQTT_send>>16;													// Laufzeiger mit letzter Sendeseite laden	
 adr=MQTT_send;																// Laufzeiger mit letzter Sendedaresse laden
 pglen=MD_PER_PG*VSATZLEN;										// Anzahl Datensätze pro Flash Seite
 cind=0;																			// reset Arbeitspufferindex 
 	
 do		// alle Messdaten senden 
 { 
	if (seite!=fp.md_page)		// Sendeseite ist nicht aktuelle Messdatenseite?
  {	 
	 P_fpar=(uchar *)cbuf;										// Zeiger auf Arbeitspuffer
   flash_adress (0,seite);									// Messdatenseite addressieren
   flash_command (FLASH_R_MAIN, 8, pglen);	// Flash Seite nach cbuf lesen		
	 while (adr<pglen)												// bis Seitenende
	 {
    if (putb (cbuf[adr++])<0) return(-1);		// Puffer Geschwindigkeitsdaten senden, Abbruch bei Verbindungsabbruch		 				 		 
		if (--bytes_to_send==0) break;					// Anzahl Sendebytes erreicht -> Ende
   }		
   if ((bytes_to_send==0)&&(adr!=pglen)) break;				// Anzahl Sendebytes erreicht -> Ende	 
	 if ((seite+1)==PROTOCOLPAGE) seite=MEASUREPAGE;		// Überlauf
   else seite++;														// nächste Flash Seite lesen	
	 adr=0;																		// auf erste Adresse in nächster Seite rückstellen
	}
	else 											// im Puffer gesicherte Messdatensätze
  {	
	 while (adr<fp.md_adr) {
		 
		 if (putb (vbuf[adr++])<0) return(-1);	// Puffer Geschwindigkeitsdaten senden, Abbruch bei Verbindungsabbruch	
		 if (--bytes_to_send==0) break;					// Anzahl Sendebytes erreicht -> Ende
	 }	 						
	}	// end else		
 } while (bytes_to_send);	// end do alle Messdaten 
 
 mqtt_pbd.send_page=seite;									// letzte gesendete Seite sichern
 mqtt_pbd.send_adr=adr;											// letzte gesendete Adresse sichern
 
 note_mqtt_package (handle, MDATA);					// Packt Nachricht in Nachrichtenliste
 
 connect=concpy;	

 if (mqtt_debug) {												// MQTT Debug Mode? 
  newline ();
  put2str (T_pub, T_data);								// Melde Publish T_data
  putstr (T_col);
  putnumber (mqtt_pbd.bytes_send,0);
  putstr (T_col);
  putnumber (PacketId-1,0);								// Gib Packetnr aus 
 }
 return(1);																// Messdaten gesendet
}	

void mqtt_unsubscribe (void)				// Unsubscribe from topic 
{																		// Implizite Übergaben cind - topic length
																		//													-	cbuf[0], Beginn Topic string																		
 ushort i=0;		// Laufvariable
	
 concpy=connect;											// Schnittstellenzustand merken
 connect=UART1;												// an Modem senden

 putb(MQTT_UNSUBSCRIBE);							// MQTT message control senden
 mqtt_remaining_len (cind+2);					// Codiert und sendet remaining length
 putb (PacketId>>8);									// MSB package id senden
 putb (PacketId);											// LSB package id	senden
 while (i<cind) putb (cbuf[i++]);			// Ausgabe topic string
 
 connect=concpy;											// Schnittstellenzustand wieder herstellen			
}

void send_topic_subscribe (uchar type, uchar unsub)		// Sendet (un-)subscribe to topic  vt/1.0/device/<SERIENNUMMER>/type
{																												// Übergabe type - 1/2/4/16 für configIn/parameter /bitmaps /plus subtopic type oder nur /cert
																												//					unsub - 0/1 für subscribe/unsubscribe			
 char *ptr;	
	
 int handle=get_mqtt_package_handle(); 	
 if (handle<0) return;									// Abbruch wenn Puffer Nachrichtenverfolgung voll
 
 if (type==PARBLOCK) 			{ ptr=T_tpar;
	 topic_to_cbuf (T_tpar, sizeof(T_tpar),2);	}		// Topic mit /configIn/parameter in cbuf zusammenkopieren	
 else if (type==SYMBLOCK) { ptr=T_tbit;
	 topic_to_cbuf (T_tbit, sizeof(T_tbit),2);	}		// Topic mit /configIn/bitmap in cbuf zusammenkopieren
 else if (type==PLUSDATA) { ptr=T_tplus;
	 topic_to_cbuf (T_tplus, sizeof(T_tplus),2); }	// Topic mit /configIn/plus in cbuf zusammenkopieren
 else if (type==CERT) 		{ ptr=T_tcert;
 	 topic_to_cbuf (T_tcert, sizeof(T_tcert),0); }	// Topic mit /cert in cbuf zusammenkopieren
 else return;																			// Unknown 
	 
 if (!unsub) 
 { mqtt_subscribe ();											// Sende subscribe to topic  
   note_mqtt_package (handle, SUBCOM);		// Packt Nachricht in Nachrichtenliste	 
 }
 else
 {
	mqtt_unsubscribe ();										// Unsubscribe from topic	 
	note_mqtt_package (handle, UNSUB);			// Packt Nachricht in Nachrichtenliste 
 }	 

 if (mqtt_debug) {												// MQTT Debug Mode? 
  newline ();
  if (!unsub) putstr (T_sub);							// Melde Subscribe 
  else putstr (T_unsub);									// Melde Unsubscribe  
  putstr (ptr);														// Melde Topic parameter/bitmaps/plus
  putstr (T_col);	
  putnumber (PacketId-1,0);								// Gib Packetnr aus
 }	 
}	

void send_mqtt_event (void)						// Sendet letzten Protokolleintrag auf /event Topic
{
 int handle=get_mqtt_package_handle();			// Hilfsvariable				
	
 if (handle<0) return;											// Abbruch wenn Puffer zur Nachrichtenverfolgung voll	
	
 if (mqtt_debug) {													// MQTT Debug Mode?	
  newline ();
  put2str (T_pub, T_event);									// Melde Publish /event
  putstr (T_col);	
  putnumber (LastEvent,0);
  putstr (T_col);
  putnumber (PacketId,0);	}									// Gib Packetnr aus	
	
 topic_to_cbuf (T_event, sizeof(T_event), 0);	// Erstellt kompletten Topic aus Teilstrings	
 packetid_to_cbuf ();													// Packet Id nach Puffer
	
 cbuf[cind++]=0;														// XXX wg. uint_t32 in buscom.h
 cbuf[cind++]=0;														// XXX wg. uint_t32 in buscom.h
 cbuf[cind++]=LastEvent>>8;									// MSB LastEvent
 cbuf[cind++]=LastEvent;									 	// LSB LastEvent
	
 read_date_time (DATE_HEX, &cbuf[cind]);		// ASCII Datum DDMMYY in Puffer
 cind+=3;																		// cbuf index erhöhen
 read_date_time (TIME_HEX, &cbuf[cind]);		// ASCII Zeit hhmmss nach Puffer 
 cind+=3;																		// cbuf index erhöhen	
	
 mqtt_publish (QoS1);												// Ausgabe des T_event topic	
 note_mqtt_package (handle, EVNT);					// Packt Nachricht in Nachrichtenliste
	
}	

int find_packetId (ushort package)		// Suche PackageId in Nachrichtenverfolgung
{
 uchar i=8;
 while (i--) 																	// Durchsuchen
	 if (mqttp[i].mstyp!=INVALID)								// Gültige Nachricht
		if (mqttp[i].mpid==package) return (i);		// Gefunden und zurückgeben
 return (-1);																	// Nicht gefunden	 
}	

void eval_mqtt_message (void)		// Werte MQTT Server Nachrichten aus
{																// Übergaben keine																		
 ushort toplen;						// Topic Stringlänge
 ushort package;					// MQTT Package Id
 uchar mstyp;							// MQTT Nachrichtentyp	
 uchar i=8, l=0;					// Laufvariable		
 ushort *Pcrc,rescrc;			// CRC Prüfsummen Zeiger und Ergebnis Hilfsvariable
 int res, theday=-1;			// Ergebnis Hilfsvariable
 uchar settyp;						// typ des SET topics	
 char url[80];						// URL firmware Update
 int filecrc;							// Checksumme firmware update		
	
 switch (mqbf[0]&0xF0)		// Control Byte, upper nibble auswerten
  {
		case MQTT_PINGRESP:		while (i--) if (mqttp[i].mstyp==PINGREQ)	// Liste Nachrichten durchsuchen
													{	
													 if (mqtt_debug) {												// MQTT Debug Mode?
													  putstr(T_m_rxpgack);										// Text " PINGACK ....
													  putnumber (mstimer-mqttp[i].t_ms,0); }	// Antwortzeit Request
													 mqttp[i].mstyp=INVALID;									// Lösche Nachricht
													 if (mqtt_mess_pend) mqtt_mess_pend--;		// Laufenden Nachrichten dekr.			
													 break;	
													}		
													break;		
		case MQTT_PUBACK:			package=(mqbf[2]<<8)|mqbf[3];							// Packet ID lesen
													if (mqtt_debug) {													// MQTT Debug Mode?
													 putstr(T_m_rxpback);											// Text	"\n PUBACK (pid,time,typ):"									
													 putnumber (package,0);	}									// Packet ID ausgeben														
													if ((res=find_packetId (package))>=0)			// Suche PackageId in Nachrichtenverfolgungsliste	
													{
													 if (mqtt_debug) {												// MQTT Debug Mode?	
													  putstr (T_col);	 
													  putnumber (mstimer-mqttp[res].t_ms,0);	// Antwortzeit
													  putstr (T_col);
													  putnumber (mqttp[res].mstyp,0);	}				// Nachrichtentyp			
													 mstyp=mqttp[res].mstyp;									// Registerzugriff
													 switch (mqttp[res].mstyp)								// Nachrichtentyp auswerten
													 {
														case JOIN:		if (mqtt_state==SUBCOMMAND) 				// JOIN Ack in state SUBCOMMAND?
																						mqtt_state=JOINED;								// Ja, neuer state ist JOINED 
																					break;
														case PARAPLS: mqtt_param_get&=~(PPEND|PLUSDATA);	// Plus Datenanforderung erledigt
																					break;
														case PARAMTR: mqtt_param_get&=~(PPEND|PARBLOCK);	// Parameter Anforderung erledigt
																					break;
														case PARASYM:	mqtt_param_get&=~(PPEND|SYMBLOCK);	// Vario Bitmap Anforderung erledigt
																					break;															// Sollte beim viasis gar nicht auftauchen
														case STATUS: 	if (mqtt_state==JOINED)							// STATUS Ack in state JOINED 
																						mqtt_state=STATUSSEND;						// Ja, neuer state ist STATUSSEND
																					break;
														case EVNT:		mqtt_state=STATUSSEND;							// Event bearbeitet, neuer state ist STATUSSEND 
																					LastEvent=0;												// Reset letzten Event
																					break;
														case MDATA:																				// DataMessage Ack
																					MQTT_index+=(mqtt_pbd.bytes_send/VSATZLEN);		// Sendeindex erhöhen
																					MQTT_send=(uint)(mqtt_pbd.send_page<<16);			// Flash Seite Sendezeiger sichern
																					MQTT_send+=mqtt_pbd.send_adr;									// Flash Adresse Sendezeiger sichern	
																					if (mqtt_state!=DATACK) break;		// Online Versand im mqtt_state=STATUS_SEND
																																						// Offline manual Versand mqtt_state=DATACK
																					if ((fp.mqtt_packetsize)&&(mqtt_md_bytes_to_send()>fp.mqtt_packetsize*BLOCKSIZE)) // Noch Messdaten zu senden?
																					{ 
																					 mqtt_state=DATASEND;							// Sende weiter Daten
																					 break;														// Schließe MQTT Verbindung nicht, kein LEAVE s. u.	
																					}															  
																					
														case BYE:																				// Leave Message Ack, Disconnect
														default:			mqtt_state=LEAVE;									// unbekannte Nachricht												
																					case DBGINFO: 										// DEBUG Command, Keine Statusänderung oder Aktion 
																					case TINFO:												// GET_TIME 
																					break;														// 																					
													 }																	
													 mqttp[res].mstyp=INVALID;													// Nachricht bearbeitet	
													 if (mqtt_mess_pend) mqtt_mess_pend--;							// Laufenden Nachrichten dekr.	
													}
													break;
		case MQTT_PUBLISH:		toplen=(mqbf[mqtt_head]<<8) + mqbf[mqtt_head+1];	// Topic length													
													i=mqtt_head+toplen+2;															// Payload (QoS1/QoS2=0) oder Package Identifier Offset
													if (mqbf[0]&(QoS1|QoS2))													// QoS>0?
													{														 	
													 package=(mqbf[i]<<8) + mqbf[i+1];								// Package Id
													 i+=2;																						// Payload beginnt 2 Byte später, hinter PackageId
													}	
													// i zeigt auf erstes Publish Payload Byte in mqbf[]
													//send_mqtt_puback (package);											// Sende ackknowledge to server publish
													if (mqtt_debug) {																	// MQTT Debug Mode?
													 putstr(T_m_rxpub);																// Text "PUBLISH....													
													 putxchr((char*)&mqbf[mqtt_head+2],toplen);				// Topic Ausgabe
													 putstr(T_col);														
													 if (mqbf[0]&(QoS1|QoS2))	putnumber (package,0);	// QoS>0? Ja, Pid ausgeben												
													 else putstr (T_m_none);													// Text "None"
													 if (mqtt_fail) puterror(MQTT_ERROR, -1);
													 else putstr(T_col); }														
													if (compare(T_cmd, (char *)mqbf, mqtt_head+2,mqtt_head+toplen+2)>0)	// Publish to /command?
													{		
													 res=1;	
													 if (mqtt_debug) { putnumber(mqbf[i],0);	putstr(T_col); } 					// Command Byte im Debug ausgeben		
													 switch (mqbf[i])									// command byte auswerten
													 {
													  case 0:													// TIME command erhalten
													  {
														 res=-1;	
														 if (mqtt_debug) { newline(); send_date_time (2,0); }			// MQTT Debug Mode? Ja, altes Datum/Uhrzeit ausgeben
														 theday=dayofweek(mqbf[i+1], mqbf[i+2], 2000 + mqbf[i+3]);	// Ermittelt Wochentag und prüft Datum
														 if (theday>0) 																							// Datum ok?
														 {	 
														  if ((mqbf[i+4]<24) && (mqbf[i+5]<60)	&& (mqbf[i+6]<60)) // Zeitwerte ok?
														  {	
															 if ((mqbf[i+7]>244) || (mqbf[i+7]<15)) // Wert UTC Offset -11 ... +14 korrekt?
														   {	
														    RCCR=0;										// Reset and halt clock
														    DOM=mqbf[i+1];						// DD -> RTC
														    MONTH=mqbf[i+2];					// MM
														    YEAR=2000+mqbf[i+3];			// YYYY Langes Jahresformat 
														    HOUR=mqbf[i+4];						// hh
														    MIN=mqbf[i+5];						// mm
														    SEC=mqbf[i+6];						// ss
														    DOW=theday-1;							// Wochentag setzen, Index anpassen
														    RCCR|=(CLKEN|CCALEN);			// Clock enable, no calibration		
														    fp.utc=(int8_t)mqbf[i+7];	// UTC offset übernehmen
														    if (mqtt_debug) { newline(); send_date_time (2,0); }	// MQTT Debug Mode? Ja, neue Datum/Uhrzeit ausgeben 													 
																res=1; 
														   } // end if Zeitzone korrekt
														  } // end if Zeitwerte ok
													   } // end if Wochentag und Datum ok    
														 if (res<0)												// Nicht Alles ok?
														 {
														  protocol (PARAMETER_DEF_ERROR);	// protokollieren
														  mqtt_state=LEAVE;								// Disconnect from Server
														 }	
														 break; 																 
													 }
													 case 1:													// DATA command erhalten
													 {																// Neuen MQTT Sendeindex/-zeiger einlesen
														MQTT_index=(mqbf[i+1]<<24)+(mqbf[i+2]<<16)+(mqbf[i+3]<<8)+mqbf[i+4]; 
														MQTT_send=(mqbf[i+5]<<24)+(mqbf[i+6]<<16)+(mqbf[i+7]<<8)+mqbf[i+8]; 
														break; 
													 }														
													 case 2:													// GET Parameter command erhalten
													 {	
														Pcrc=&mcrc.param;								// Zeiger auf Parameter Prüfsumme	
														for (i+=1; i<mqtt_len; i+=2)		// Prüfsummen ausgeben
														{
														 rescrc=(mqbf[i]<<8)+mqbf[i+1];	// CRC aus GET
														 if (mqtt_debug)								// MQTT Debug Mode?
														 {
															newline(); 
															putnumber (1<<l,0);
															putstr(T_col); 
															putnumber (*Pcrc,0x84);
															putstr(T_col); 
														  putnumber (rescrc,0x84);
														  putstr(T_col);															  																				
														 }
														 if (rescrc!=*Pcrc) mqtt_param_get|=1<<l;							// Ungleiche Prüfsumme notieren
														 Pcrc++;					// Zeiger auf nächste Geräteprüfsumme
														 l++;							// Nächster Prüfsummentyp	
														}
														if (mqtt_debug) putnumber (mqtt_param_get,0x80); 						// MQTT Debug Mode? Ja, resultierende GET Flags ausgeben
														if ((fp.ex12!=2)&&(fp.ex12!=4)) mqtt_param_get&=~PLUSDATA;	// Kein PLUS oder viatext -> liefert keine Plusdaten
														mqtt_param_get&=~SYMBLOCK;																	// Kein VARIO -> liefert keine Symbolbitmaps														
														break;
													 }	   
													 case 3:													// SET Parameter command erhalten
													 { 
														mcrc.param=(mqbf[i+2]<<8)+mqbf[i+3];		// Parameterblock CRC sichern
														mcrc.bitmap=(mqbf[i+4]<<8)+mqbf[i+5];		// Bitmapdaten CRC sichern entfällt beim viasis
														mcrc.plus=(mqbf[i+6]<<8)+mqbf[i+7]; 		// Plusdaten CRC sichern  
														if (mqtt_debug) 
														{	
														 putnumber (mqbf[i+1],0x80);						// MQTT Debug Mode? Ja, empfangene SET Flags ausgeben
														 putstr(T_col);	
														 putnumber (mcrc.param,0x80);						// Ausgabe Parameterblock CRC
														 putstr(T_col);	
														 putnumber (mcrc.bitmap,0x80);					// Ausgabe Bitmapdaten CRC	
														 putstr(T_col);	
														 putnumber (mcrc.plus,0x80);						// Ausgabe Plusdaten CRC	
													  } 	
														if (mqbf[i+1]&(PARBLOCK|PLUSDATA)) 										// Parameter/Plus Texte... abzuholen?	 
														 mqtt_set|=mqbf[i+1]&(PARBLOCK|PLUSDATA|PPEND); 			// Maskierte Übernahme	 
														break;
													 }
													 case 4:														// Opmode command erhalten
													 {
														fp.opmode=mqbf[i+1];					
														break; 
													 }	
													 case 5:													// Firmware command erhalten
													 {
														memcpy (url,(void *)&mqbf[i+1],sizeof(url));						// Download URL sichern
														filecrc=(mqbf[i+81]<<8) + mqbf[i+82]; 									// Dateiprüfsumme sichern
														send_mqtt_puback (package);															// Sende ackknowledge to server publish
														if (mqtt_state>=JOINED) send_leave ();									// Publish zum /leave topic																											
														concpy=connect;		  																		// Schnittstellen wg. Debug mode sichern														
														if (Download_firmware(url,1)>0)													// Firmware ins Modem laden erfolgreich?
														{																 	
														 res=firmware_to_flash (filecrc);												// Transferiert Firmware Datei vom Modem in Flash Speicher, prüft CRC
														 if (res>0)																							// Datenblocks und CRC überprüft, res >0 enthält Blockanzahl
														 {
                              if (file_is_firmware())																// Prüfe auf Firmware Datei und trage CRP ein	
															{	
															 if (mqtt_debug) putln (T_fw_prog);	
															 fp.md_page=MEASUREPAGE;															// Setzt zusammen mit delete_data alle Zeiger zurück	
															 delete_data();																				// Messdatenzeiger im Speicher löschen	
															 iap_programming (res);																// Reprogrammierung Erfolg -> System Reset		 	
														  } 
														 }	 
														}
														puterror(IAP_PROGRAMM_ERROR, -1);		// Hier sollte der Programmzeiger nicht ankommen -> IAP Programmierfehler
														if (mqtt_debug)
														{
														 connect=UART0; 	
														 newline();	
														 putstr (T_url);	
														 putln(url);	
														 putstr ("CRC=");
														 putnumber (filecrc,0x80);
														 newline();		
														 mqtt_debug=0;	
														}		
														res=-1;
														break; 
													 }			
													 case 6:														// CERT_SAVE Command empfangen
													 {
														memcpy (certname,(void *)&mqbf[i+1],sizeof(certname));	// Zertifikatsnamen kopieren
														if (mqtt_debug) putstr (certname); 											// Im Debug Mode ausgeben
														l=i+1+sizeof(certname);																	// Offset zur Prüfsumme 
													  certcrc=(mqbf[l]<<8) + mqbf[l+1];												// Prüfsumme Zertifikat sichern
														mqtt_set|=CERT;																					// Flag für Zertifikat abholen setzen	
														break; 
													 }														 
													 case 7:														// CERT ACTIVATE Command empfangen	
													 {
														res=activate_cert ();										// Aktiviert im Funkmodem vorhandene neue TLS Zertifikate 
														if (!gsmpower) mqtt_state=DISCONNECT; 	// Aktivierung oder Wiederaufnahme der Verbindung gescheitert
														else if (res>0) mqtt_state=CLOSE;				// Erfolg beende Verbindung -> wird nach 1 Minute mit neuen Certs wieder aufgebaut 
														break; 
													 }
													 case 8:														// Command send debug information empfangen	
													 {
														send_mqtt_puback (package);				// Sende ackknowledge to server publish
														send_debug_info (mqbf[i+1]);			// Sende Testinfo	
														res=0;														// Paket Acknowledge nur einmal senden
														break;		
													 }	
													 case 9:														// GET_TIME command erhalten
													 {		
														send_gettime ();				 					// Publish zum /time topic, Antwort auf GET_TIME Kommando 
														break; 
													 }	 
													 default: 													// Unbekanntes Kommando	
														//res=-1; 												// Fehler - auskommentiert -> Publish quittieren und Kommando ignorieren
														if (mqtt_debug) 									// MQTT Debug Mode? 
														 for (; i<mqtt_len; i++)					// Kommandosequenz ausgeben
														 {
														  putnumber (mqbf[i],0x82);
														  putstr(T_col);			
														 }	
														break;
													 }	// end switch command byte
													 if (mqbf[0]&(QoS1|QoS2))						// QoS>0?
													  if (res>0) 
															send_mqtt_puback (package);			// Sende ackknowledge to server publish	
												  } // end command topic
													else if ((res=compare(T_cfin, (char *)mqbf, mqtt_head+2, mqtt_head+toplen+2))>0)
													{																					// Publish to Topic /configIn?
													 if (mqtt_debug) {																				// MQTT Debug Mode?	
													  putnumber (mqtt_len,0);																	// Nachrichtenlänge ausgeben
													  putstr(T_col); }		
													 if (compare(T_tpar, (char *)mqbf, res, mqtt_head+toplen+2)>0) //	Publish to Topic /parameter?
													 {	 			
														settyp=PARBLOCK;	
														res=-1;	
														if ((uint)(mqtt_len-mqtt_head-toplen-4) == sizeof(fp))	// Datenlänge korrekt?
														{		
														 rescrc=crc(&mqbf[i],(uint)&fp.pro_start_page-(uint)&fp.Kennung);																// CRC viasis Parameterblock Bereich 1
														 rescrc=crcgen(&mqbf[i+(uint)&fp.mph-(uint)&fp.Kennung],(uint)&fp.u_usb-(uint)&fp.mph,rescrc);	// CRC viasis Parameterblock Bereich 2
														 rescrc=crcgen(&mqbf[i+(uint)&fp.btmodem-(uint)&fp.Kennung],(uint)&fp.gpsanz-(uint)&fp.btmodem,rescrc); 	// 	Parameterblock Bereich 3
														 rescrc=crcgen(&mqbf[i+(uint)&fp.pwmex-(uint)&fp.Kennung],(uint)&fp.csq-(uint)&fp.pwmex,rescrc);					// 	Parameterblock Bereich 4
														 rescrc=crcgen(&mqbf[i+(uint)&fp.pKennung-(uint)&fp.Kennung],(uint)&fp.gp-(uint)&fp.pKennung,rescrc);			// 	Parameterblock Bereich 5	
														 if (mcrc.param==rescrc) 																// Korrekte Parameter-Prüfsumme?
														 {																											// Parameter übernehmen
															memcpy((char *)&fp.pKennung,&mqbf[BLOCKSIZE+mqtt_head+toplen+4],(uint)&fp.gp - (uint)&fp.pKennung);																											
															res=1;																	// Erfolg
														 } 
													  } // end if Datenlänge korrekt
													 } // end if publish to /parameter
													 else if (compare(T_tplus, (char *)mqbf, res, mqtt_head+toplen+2)>0) //	Publish to Topic /plus?
													 {
														settyp=PLUSDATA; 
														res=-1; 
														rescrc=crc(&mqbf[mqtt_head+toplen+4],3*BLOCKSIZE); 	// CRC Prüfsumme des Plusdaten bilden
														if (rescrc==mcrc.plus)															// CRC Prüfsumme stimmt?
														 res=plus_com (3);												// Transferiere beides an Matrixcontroller	
														if (mqtt_debug) osDelay(10);							// Warte bis Matrix Controller Unsubscribe wieder ausgeben kann		
													 }		
													 if (res>0) 																// Parameter erfolgreich übernommen
													 {
														send_mqtt_puback (package);								// Sende ackknowledge to server publish
														send_topic_subscribe (settyp, 1);					// Unsubscribe from configIn/settyp topic
														mqtt_set&=~(settyp|PPEND);								// Abholung erledigt  
													 }		
													 if (!(mqtt_set&(PARBLOCK|PLUSDATA)))	// Alle configIn topics /parameter und /plus gelesen
													 {	 
													  if (res<0) protocol(PARAMETER_INIT_ERROR);	// Protokolliere Parameter nicht übernommen													 
													  else protocol(PARAMETER_INIT); 							// Protokolliere Parameter übernommen
													 }
													} // end if configIn publish
													else if ((res=compare(T_tcert, (char *)mqbf, mqtt_head+2, mqtt_head+toplen+2))>0)	// Publish to Topic /cert?
													{																 	
													 certsize=mqtt_len-(mqtt_head+toplen+4);		// Länge der Zertifikatsdatei errechnen													
													 if (certcrc==crc(&mqbf[i],certsize))				// CRC Prüfsumme über Zertifikatsdatei korrekt?
													 {	 
														// Upload cert in UC15 														
														ResetWDT ();
														res=cert_to_uc15(i);											// Zertifikat als _filename.ext ins UC15 schreiben
														if (gsmpower) 														// Modem wg. Fehlern nicht aus?
														{	
														 send_mqtt_puback (package);							// Sende ackknowledge to server publish
														 send_topic_subscribe (CERT, 1);					// Unsubscribe from /cert topic 
														 if (res>0)																// Zertifikat erfolgreich geschrieben? 	
														 {
														  mqtt_set&=~(CERT|PPEND);								// Abholung erledigt  
													    protocol(CERT_SAVED); 									// Protokolliere Zertifikat übernommen	
														 }		 
													  } // end if gsmpower		
														else mqtt_state=DISCONNECT;								// Verbindung abgebrochen	
													 }	// end if CRC Prüfsumme ok
													} // end if topic /cert
													break;
		case MQTT_SUBACK:			package=(mqbf[2]<<8) + mqbf[3];							// PackageId lesen
													mqtt_com=0;																	// Nachricht bearbeitet -> Nächste lesen ...
													if (mqtt_debug) {														// MQTT Debug Mode?
													 putstr(T_m_rxsback);												// Text "\n SUBACK (pid,time,typ,pay): "														
													 putnumber (package,0); }
													if ((res=find_packetId (package))>=0)	// Suche PackageId in Nachrichtenverfolgungsliste	
													{
													 mstyp=mqttp[res].mstyp;										// Registerzugriff auf Nachrichtentyp	
													 if (mqtt_debug) {													// MQTT Debug Mode?	
													  putstr(T_col);
													  putnumber (mstimer-mqttp[res].t_ms,0);		// Antwortzeit
													  putstr (T_col);
													  putnumber (mstyp,0);											// Nachrichtentyp			
													  putstr (T_col);
													  putnumber (mqbf[4],0x82);									// Return Code ausgeben
													 }		
													 if ((mqtt_state==CONNECTED)&&(mstyp==SUBCOM)) 		
														 mqtt_state=SUBCOMMAND; 									// Status erhöhen
													 mqttp[res].mstyp=INVALID;									// Nachricht bearbeitet	
													 if (mqtt_mess_pend) mqtt_mess_pend--;			// Laufenden Nachrichten dekr.	
													}															
													break;
		case MQTT_UNSUBACK:	  package=(mqbf[2]<<8)|mqbf[3];								// PackageId lesen
													if (mqtt_debug) {														// MQTT Debug Mode?													
													 putstr(T_m_rxusbk);												// Text "\n UNSUBACK (pid):"						
													 putnumber (package,0);		}									// Packet ID ausgeben
													if ((res=find_packetId (package))>=0)				// Suche PackageId in Nachrichtenverfolgungsliste	
													{
													 mqttp[res].mstyp=INVALID;													// Nachricht bearbeitet
													 if (mqtt_mess_pend) mqtt_mess_pend--;							// Laufenden Nachrichten dekr.
													}		
													break;
	 default:								if (compare(T_noca,(char *)mqbf,0, rmi)>0) 	// Zeichenkettenvergleich positiv
													{
													 if (mqtt_debug) putstr(T_conclose);				// MQTT Debug Mode?	Ja, Text "\nMQTT server closed connection"
													 else mqtt_con_err = 51;										// irregulärer Verbindungsabbruch				 
													} 
													else if (mqtt_debug) sendbuf((char *)mqbf, rmi);	// im Debug Mode Puffer ausgeben
													break;
	} // end switch		
	mqtt_com=0;										// Nachricht bearbeitet	
}	

int Con_MQTT_server(uchar still)					// MQTT Server verbinden oder Verbindung prüfen
{ 																				// Übergabe still - 0 Testbetrieb mit Ausgabe, Verbindung schließen
																					//					still - 1 Verbindungsaufbau ohne Detailausgabe, nicht schließen
																					//					still - 17 MQTTS disconnect, dann still schließen	
																					//					still - 18 Server Verbindung mit Ausgabe schließen
																					//					still - 19 Server Verbindung still schließen
	
 uint timeout=XMODEM_BLOCK_TIMEOUT;				// Antworttimeout
 uchar state=8;														// Zustandsvariable, beginne erst bei 8 wg. GPRS states
 //uchar failure=0;													// Fehler aufgetreten
 text *P_resp;														// Zeiger auf Modem - Antwortstring	
 int result;															// Ergebnis MQTT Connect
 uchar i;	
	
 InitWatchdog(LONG_WD_32);								// langes Watchdog Interval auf 32 Sekunden setzen
 concpy=connect;													 
	
 if (still<16)														// Verbindung aufbauen?
 {	 
	if (!online) 														// Keine Ausgabe im viaspeedcam Online Modus
	{	
	 if (still) newline();									// Leerzeile 
   putstr(T_conmqtt);											// Ausgabe Text "Verbinde mit MQTT Server  
   putln(T_dot3);													// Ausgabe "..."
	}	
 } else state=still;											// Verbindung schließen 
 still&=0x01;															// Ausgabeflag maskieren
 
 if (!still) connect|=UART1;							// Ausgabe auch an angeschlossene Terminal(s)
 else connect=UART1;											// Nur Modemausgabe

 do																				// Zustandsfolgeschleife
 {
  P_resp=T_nil;														// Reset Antwortstring
	ResetWDT();															// Watchdog reset 
  
  if (state==8) {														// Prüfe GPRS Server 
   if (!isalpha(fp.server[0])) return(-state); 	// Fehler Servereintrag nicht gesetzt/gelöscht		
	}	
	
	else if (state==9) {											// GPRS Verbindung aktivieren
	 connect=concpy|UART1;	
   result=set_gprs_config(0);										
	 if (result < 0) return(result);	 			// GPRS Verbindung nicht erfolgreich? -> Fehlerabbruch
	 if (still) connect=UART1; }
	
	else if (state==10) {
    if (!still) send_local_ip (); 				// bei Ausgabe, sende IP an Terminal
	  /*if (fp.gsm==EG91)											// TCP/IP keepalive nur bei EG91, UC15 kennt das Kommando nicht
		{		
		 putln(T_cfgalive);										// Setze TCP keepalive
	   P_resp=T_OKNZ;												// Erwarte "OK" Antwort
		}	*/
	}	
	
  else if (state==11)											// Prüfe eingestellten Server Port
	{
	 switch (fp.port)
	 {
		 case	1883:	break;										// Unverschlüsselt
     case 8883: putln(T_sslcfg1);					// Konfiguriere Verschlüsselung und Server Zertifikat am UC15
								P_resp=T_OKNZ;						// Erwarte "OK" Antwort
								break;
		 default:		return(-state); 					// Kein zulässiger Porteintrag, Fehlerabbruch 
	 }		 // end switch
	}		
  else if (state==12)											// Konfiguriere Client Zertifikat und Schlüssel
	{
	 if (fp.port==8883)
   {
		putstr(T_sslcfg2);										// Konfiguriere Client Zertifikat
		putstr(fp.serno);					
		putstr(T_sslcfg3);										// Konfiguriere Client private key
		putstr(fp.serno);
		putln(T_sslcfg4);
		P_resp=T_OKNZ;												// Erwarte "OK" Antwort 
   } 		 
  } 
	else if (state==13)											// MQTT Serververbindung öffnen
	{
	 if (fp.port==1883)										// Unverschlüsselt 
	 {	 
		putstr(T_qiopen);										// Kommando "AT+QIOPEN=1,1," senden
		putqstr(T_tcp);											// Kontext "TCP" senden
		putc(',');	
	 }	 
   else putstr(T_sslopen); 							// Kommando "AT+QSSLOPEN=1,1,1," senden	  
	 putqstr(fp.server);										// Server IP Adresse oder URL in Quotes senden
   putc(',');
   putnumber(fp.port,0);									// Portnummer ausgeben
	 if (fp.port==1883) putstr(",0");			// Default local Port
	 putln(",2");													// Transparent access	   
	 timeout=6*XMODEM_BLOCK_TIMEOUT;				// Warte bis zu 18s	
   P_resp=T_connect;											// Erwarte "CONNECT" Antwort
  }		
	else if (state==14)											// Server TCP Connected
	{
	 if (comchange)													// Connect verursacht Schnittstellenwechsel	
   {
    thread_comchange=1;										// comchange merken		 
		comchange=0;													// Änderung löschen		
   } 		 	
	 if (!still)														// Terminalausgabe?
   { 		 
	  connect=concpy&~UART1; 								// nicht an GSM modem
	  putstr(T_tcp);												// Kontext "TCP" senden
    putc(' ');																		
    putstr(&cbuf[2]); 										// Ausgabe CONNECT	Status an Terminal 
	 }	 
   connect=UART1;													// nur an GSM modem
	 if (send_mqtt_connect()<0) return(-state);	// Sende MQTT connect, bei Fehler Abbruch
   bxi=rxi;																// leere Puffer	 
	}	
	else if (state==15)											// Antwort im Puffer auswerten
	{
	 concpy&=~UART1; 									
	 connect=concpy;												// nicht an GSM modem
	 result=wait_mqtt_connect(timeout);	    // MQTTS Verbindungsantwort
	 if (result==0)													// MQTTS Verbindung bestätigt?
   {
		if (mqtt_debug || !still) 						// MQTT Debug Mode oder Verbindungstest? 
		 putstr (T_cmqtt); 													// Verbindungsmeldung	senden
		mqtt_com=0;																	// Reset Nachrichtenempfang
    for (i=0;i<8;i++) mqttp[i].mstyp=INVALID;		// Reset Packetnachverfolgung 
		mqtt_mess_pend=0;														// Reset Anzahl MQTTS unquittierte Nachrichten
		mqtt_fail=0;																// MQTT Fehler reset		
    mqtt_set=0;																	// MQTT Parameter/Bitmaps/Plus/Certs abzuholen Status auf Null
		mqtt_param_get=0;														// MQTT Parameter/Bitmaps/Plus zu senden Status auf Null
		bpi=rpi;																		// Initialisiere Emfangsringpuffer				 
    if (still)
    {			
			concpy|=MQTT_LINK;										// MQTT Server Verbindung besteht
			state=23;															// Ende mit aktiver MQTT Server Verbindung 
		}	
		else state++;														// weiter mit state 17
   }		 
	 else					// MQTTS Verbindung nicht bestätigt
	 {
		if (mqtt_debug || !still) 						// MQTT Debug Mode oder Verbindungstest? 
		{	
		 putstr (T_conref);									// Text "Connection refused" 
		 if (result==1) putln (T_wprot);			// Falsches Protokoll
	   else if (result==2) putln (T_wclient);		// ClientID not accepted
	   else if (result==3) putln (T_nomqtt);		// MQTT service not available
	   else if (result==4) putln (T_baduser);		// Falscher user name oder Passwort
	   else if (result==5) putln (T_noauth);		// Not authorized
		 else putln (T_unknown); 
		  // if (mqtt_debug) sendbuf ((char *)mqbf,rmi);			
      // newline (); 		 
		}	
		mqtt_con_err=30+result;								// MQTT-Verbindungsfehler, Protokollierung im Hauptthread
		state=20;															// MQTT disconnect nicht erforderlich -> state 21 UC15 command mode folgt 
	 }	// end MQTT Verbindung unbestätigt  
	} 
	else if (state==17)											// MQTT Serververbindung schließen
	{
	 mqtt_state=DISCONNECT;									// MQTT Kommunikation beendet 
	 mqtt_manual=0;													// reset manueller Betrieb	
	 connect=UART1;													// Ausgabe nur an Modem  
   if (send_mqtt_disconnect () < 0) return(-state);		// Sende disconnect an MQTTS
   mqtt_com=0;														// Reset MQTT Nachrichtenempfang
	 		
   state++;																// weiter mit 19			
  }		
	else if (state==18);										// Dummy für Verbindung schließen mit Ausgabe
  else if (state==19)											// MQTT disconnect melden und MQTT_LINK austragen
  {	
   concpy&=~MQTT_LINK;										// MQTT Server Verbindung geschlossen		
   connect=concpy&~UART1;									// Nicht an UC 15 senden
   if (mqtt_debug || !still) 							// MQTT Debug Mode oder Test?		
	 {	 
	  newline();	
    putln (T_discon);											// Disconnect ggf. an Terminal
	 }	 
	 timeout=XMODEM_BLOCK_TIMEOUT;					// Warte bis zu 3s
	 P_resp=T_noca;													// auf "NO CARRIER"
   state=21;															// Server hat GPRS Verbindung geschlossen -> state 22 folgt		
	}		
	else if (state==21)											// Umschalten in command mode
	{
 	 if (fp.gsm==EG91) send3plus	();				// Umschaltung Modem vom data in den command mode 	
	 FIO0SET=GSM_DTR;												// DTR HI -> UC15 switch DATA to COMMAND Mode		
	 if (!still&mqtt_debug) connect=concpy|UART1;		// Ausgabe auch an angeschlossene Terminal(s)
   else connect=UART1;														// Nur Modemausgabe	
   concpy&=~MQTT_LINK;										// MQTT Server Verbindung geschlossen 	 
	 mqtt_state=DISCONNECT;									// MQTT Kommunikation beendet	
   P_resp=T_OKNZ;
   timeout=XMODEM_BLOCK_TIMEOUT;					// Warte bis zu 3s		
	}
	else if (state==22)											// TCP Verbindung schließen
	{	
	 FIO0CLR=GSM_DTR;												// DTR sofort rücksetzen, sonst UC15 -> Sleep	
	 if (!still) connect=concpy|UART1;			// Ausgabe auch an angeschlossene Terminal(s)
   else connect=UART1;										// Nur Modemausgabe	
	 if (fp.port==1883) putstr (T_qiclose); // unverschlüsselt AT+QICLOSE senden
	 else putstr (T_sslclose);							// verschlüsselte Verbindung AT+QSSLCLOSE 		
	 putln("=1,2"); 												// ConnectID und socket timeout 2s senden
	 P_resp=T_OKNZ;
   timeout=3*XMODEM_BLOCK_TIMEOUT;				// Warte bis zu 9s	
	}
	else if (state==23)											// APN Verbindung schließen
	{
	 if (!still) connect=concpy|UART1;			// Ausgabe auch an angeschlossene Terminal(s)
   else connect=UART1;										// Nur Modemausgabe	
	 putstr(T_qideact);											// Deaktiviere IP Kontext
   putln("=1");														// Ergänze Kontext ID
	 mqtt_com=0;														// Reset MQTT Nachrichtenempfang 2. Mal		
	 P_resp=T_OKNZ;
   timeout=5*XMODEM_BLOCK_TIMEOUT;				// Warte bis zu 15 s	
	}	
	
	if (P_resp!=T_nil)											// Modemantwort erwartet?
  { if (wait_message(P_resp,timeout)<0) return(-state); // falsche Antwort -> Fehler markieren
	  /* if (!still && (state>=20)) { 						// Ausgabe auch an angeschlossene Terminal(s)
		 connect=concpy&~UART1;									// nicht an GSM modem
     putstr(&cbuf[2]);     			
	   connect=concpy|UART1; } */
	}	
	
	osDelay(10); 											
 } while (state++ < 23);								// end do Zustandsfolgeschleife
	
 connect=concpy;												// Schnittstellenzustand wiederherstellen

 return(state);
}

void MQTT_Connect (void const *argument)		// MQTT Verbindungsaufbau und Zeichenbearbeitung in eigenem Thread
{
 char c;	
 int result;	
		
 while (1)																	
 {	 
  osSignalWait (3,osWaitForever);								// Warte auf Signal von MainThread
	 
 	result=Con_MQTT_server(1);						// Verbindung aufbauen 
	if (result<0) 												// Verbindungsaufbau gescheitert?
	{
	 connect=concpy;											// Nach Abbruch Schnittstellenstatus wiederherstellen	
	 FIO1CLR = PWRKEY;										// GSM PWRKEY LOW   
	 osDelay (25);
	 if (!(FIO0PIN&PWR_GPS)) 							// Wenn GPS auch aus ->
	  FIO1CLR = GSMPWR;										// gemeinsame Versorgung für GSM und GPS aus 
	 gsmpower=0;													// GSM Power Status aus 
   simpinset=0;													// Pinnummer Status ist ungültig	
	 FIO0CLR=GSM_DTR;											// DTR LOW
	 uart1_release(MUX_GSM);								// Release UART1, auto-restores BT if connected	
	 mqtt_con_err=-result;								// Fehlerprotokollierung im Hauptthread			
	 wait_min=Def_wait;										// Verbinden erfolglos? Ja, warte 3 Minuten bis nächster Verbindungsaufbau
	}		
	else																					// MQTT Nachrichtenempfang im Thread 
	{
	 osActiveThread&=~MQTT_CONNECTION;						// MQTT Verbindungsaufbau erledigt
   if (mqtt_manual) 														// kein automatischer MQTT Betrieb
	 {	
	  mqtt_state=DATASEND;												// Nur Daten senden, kein Subscribe /command und kein Join
	  mqtt_manual=0;															// reset manueller MQTT Betrieb  	
	 }	  
	 else mqtt_state=CONNECTED;										// Erfolgreich, MQTT Status auf Verbunden
   while (mqtt_state)														// Solange nicht Null also Disconnected
   {
		if (bpi!=rpi)																// Zeichen in Ringpuffer?
    {
		 if (!mqtt_com) 							// Keine Nachricht begonnen
		 { 									 
			rmi=0;											// Reset Pufferindex
			mqtt_len=0;									// Nachrichtenlänge reset
			mqtt_com=1;									// MQTT Nachrichtenempfang gestartet
			//mqtt_fail=0;
		 } // end noch keine Nachricht begonnen	
		 if (mqtt_com<2)							// keine gültige Nachricht in mqbf?
		 {
			while (bpi!=rpi)											// Bearbeite Puffer, solange pbuf Indizes ungleich
			{
			 mqbf[rmi]=pbuf[++bpi];								// Zeichen umkopieren
			 if (rmi<MQBFSIZE-1) rmi++;		
			 if (rmi==1)													// 1. Zeichen 								
			 {
				c=mqbf[0];													// Control Byte 
				c=c>>4;															// Upper Nibble maskieren									 
				if ((c<2)||(c==8)||(c==10)||(c==12)||(c>13))	// Ungültiges Control Byte?
				mqtt_fail=1;												// MQTT Fehlerflag setzen
				break;		
			 }		 
			 else if ((rmi>1) && (rmi<5) && !mqtt_len)	// 2. bis 4. Zeichen und Länge noch unbestimmt
			 {
				if (!(mqbf[rmi-1]&0x80))								// Längenbyte ohne Folgebyte?
				{
				 uint multiplier=1;
				 uchar i=1; 
				 do 																		// Ja, Paketlänge ermitteln
				 {
					mqtt_len+=(mqbf[i]&127)*multiplier;
					multiplier*=128;
					if (!(mqbf[i]&0x80)) break;						// Kein Folgebyte - Abbruch		
				 } while (i++<=2);											// Maximal 2 Längenbytes auswerten, Nachricht zu groß
				 mqtt_head=i+1;													// Länge fixed header (incl.length remaining length bytes)
				 mqtt_len+=mqtt_head;										// Nachrichtenlänge gesamt
				 if (mqtt_len>MQBFSIZE-1) mqtt_fail=1;	// Nachricht zu lang?	Ja, MQTT Fehlerflag setzen	
				} // end if kein Folgebyte			
			 } // end rmi<5 und Länge unbestimmt
			 if (mqtt_len && (rmi==mqtt_len))					// Länge bestimmt und vollständig empfangen?
			 { 	 
				mqtt_com++;															// Nachrichtenstatus auf vollständig erhöhen
				break;																	// Nachricht umkopiert -> Ende  
			 } // end if Nachricht ist vollständig	 
			 if (rbuf_full)															// Empfangspuffer voll?
			 {
				rbuf_full=0;															// Puffer voll Flag reset
				NVIC_EnableIRQ(UART1_IRQn);								// Starte Pufferempfang neu - Enable UART1 Interrupt 
       }				 
			} // end while Indizes ungleich 
     } // end if noch keine gültige Nachricht	
    } // end if Zeichenempfang			
		else osDelay(2);
		if (mqtt_fail) break; 
   } // end while mqtt_state != Disconnected		 
	} 
	osActiveThread&=~MQTT_CONNECTION;							// MQTT Verbindungsaufbau erledigt
	osSignalClear (mqtt_thread_id,3); 	// Reset MQTT Connect thread Startsignal
 }	// end forever 	
}	


void test_mqtt_com_state	(void)	// Prüfe MQTTS Kommunikationstimeouts und sende Gerätestatus/-daten
{
 int i=8;														// Laufvariable	
 uchar tmin=MIN;										// Minuten aus RTC
 uchar dt;													// Minutendifferenz	
 int res=0;													// Ergebnis	
	
 if (mqtt_debug) putc('M');								// Terminal Minutenausgabe im MQTT Debug Mode
	
 while (i--) if (mqttp[i].mstyp!=INVALID)	// Prüfe alle laufenden Nachrichten
 {
	if (tmin >= mqttp[i].t_min) dt=tmin-mqttp[i].t_min;	// MIN > Minutenzeitstempel
  else dt=60+tmin-mqttp[i].t_min; 	 									// MIN < Minutenzeitstempel
  if (dt>=MQTT_MESSAGE_TIMEOUT) { res=-1; mqtt_con_err=45; break; }		// Nachrichten nach 2 Minuten unbeantwortet
 }	 
 
 if (res>=0) 
 {
	if (mqtt_state==STATUSSEND)													// Angemeldet am MQTT?
  {	 																																						 
   if (poweron_min==MIN) { res=send_mqtt_data(1); if (res<0) mqtt_con_err=46;}	// Stündlicher MQTT Versand Messdaten
	 if ((res==0) && (t_mqtt_status>=MQTT_STATUS_MINUTES))  											// Keine Daten versendet, dann sende Statusmeldung
		{ res=send_mqtt_status (); if (res<0) mqtt_con_err=47;} 
  } 
 }
 if (res<0) MQTT_fast_disconnect(0);																							// Fehler? Ja, disconnect from MQTTS ohne Abmeldung  																									  
}	

void test_for_send (void)																		// Prüfe auf erforderlichen Parameterversand,/-empfang oder Zertifikatsempfang
{	
 int res=0;																									// Ergebnisvariable Verbindungsabbruch	
	
 if (mqtt_param_get	&& mqtt_param_get<PPEND)								// Parameter/Bitmaps/Plus zu senden
 {																													//	und kein laufender Versand?
	if (mqtt_param_get&PARBLOCK)  																	// Parameter zu versenden?
		{ res=send_mqtt_par (); if (res>0) mqtt_param_get|=PPEND; }		// Parameter senden und Bit 7 Versand läuft setzen
	else if (mqtt_param_get&PLUSDATA) 															// Plus Parameter zu versenden?
		{ res=send_mqtt_plus (); if (res>0)	mqtt_param_get|=PPEND; } 	// Plus Texte und Bitmaps senden und Bit 7 Versand läuft setzen	
	else mqtt_param_get&=(PARBLOCK|PLUSDATA);												// Alles andere (VARIO?) nur bereinigen	
 }
 else if (mqtt_set && mqtt_set<PPEND)												// Parameter/Bitmaps/Plus/Certs abzuholen
 {																													//	und keine laufende Abholung?		
	if (mqtt_set&PARBLOCK)  																	// Parameter abzuholen?
	{ send_topic_subscribe (PARBLOCK,0);	mqtt_set|=PPEND; }	// Subscribe to /parameter und Bit 7 Abholung läuft setzen				
  else if (mqtt_set&PLUSDATA) 															// Plus Texte/Bitmaps abzuholen?
  { send_topic_subscribe (PLUSDATA,0);	mqtt_set|=PPEND; }	// Subscribe to /plus	und Bit 7 Abholung läuft setzen					
  else if (mqtt_set&CERT) 																	// Neues Zertifikat abzuholen?	
	{ send_topic_subscribe (CERT,0); mqtt_set|=PPEND; }				// Subscribe to /cert und Bit 7 Abholung läuft setzen
	else mqtt_set&=(PARBLOCK|PLUSDATA|CERT);									// Alles andere (VARIO?) nur bereinigen
 }	 
 else	if (mqtt_set<PPEND)	res=send_mqtt_data(0);						// Keine Parameter Initialisierung im Gang? Ja, sende Messdaten wenn Paketgröße erreicht 
 
 if (res<0)	{ MQTT_fast_disconnect(0); mqtt_con_err = 48; }	//  Versand gescheitert? Ja, disconnect from MQTTS ohne Abmeldung 
} 

void mqtt_state_incr (void)			// Status Publishes/Subscribes 
{	
 int i=0;	
 if (!mqtt_mess_pend) switch (mqtt_state)								// Verzweigt je nach aktuellem Verbindungsstatus
 {
	 case CONNECTED:	protocol (MQTT_CONNECTED);					// Verbindung protokollieren
										send_cmd_subscribe ();							// Sendet subscribe to command topic
										break; 			
	 case DATASEND:		if ((SEC%10)==0)										// Datenblöcke im 10s Abstand versenden
										{
										 i=send_mqtt_data(0);								// Messdaten offline versenden
										 if (i>0) mqtt_state=DATACK;				// Erfolg? Ja, Messdaten publish ackknowledge erwartet
										 else if (i==0) mqtt_state=LEAVE;		// Keine Daten versandt - Verbindung beenden		
										 else { MQTT_fast_disconnect (0); 	// Schließe Verbindung ohne Abmeldung sofort
											mqtt_con_err = 49; } 
										}	
										break; 	  	  	 
	 case SUBCOMMAND:	send_join	(); 					break;			// Sendet join Nachricht 
   case JOINED:			send_mqtt_status ();		break;			// Sendet Statusmeldung
	 case STATUSSEND:	test_for_send ();				break;			// Prüft ob Parameter/Messdaten zu senden
	 case EVENT:			send_mqtt_event ();			break;			// Sende letzten Protokolleintrag
	 case CLOSE:			send_leave ();					break;			// Publish zum /leave topic, message null Länge
	 case LEAVE: 			Con_MQTT_server(17);		break;			// Reguläres Ende MQTTS Verbindung	 
	 default:					Con_MQTT_server(19);		break;			// Unbekannter State -> Ende ohne MQTTS Disconnect Abmeldung
 } 	 
}

void get_mqtt_ind (void)				// Änderung MQTT Sendeindex und Speicherzeiger der Messdaten und der Versandminute
{
 int dummy;																								// Hilfsvariable für Eingabe	
	
 newline();
 putparameter (T_m_ind,	MQTT_index,1<<16,T_col);							// Ausgabe MQTT Sendeindex des letzten Messdatensatzes
 dummy=getnumber(T_m_ind,0,0x7fffffff);												// Abfrage Sendeindex				
 if (dummy>=0) MQTT_index=dummy;															// Übernahme bei Eingabe
 putparameter (T_m_spg,	MQTT_send>>16,1<<16,T_col);						// Ausgabe Speicherzeiger letzte Flash Sendeseite
 dummy=getnumber(T_m_spg,0,8191);  														// Sendeadresse letzte an cloud gesendete Messdatenseite		
 if (dummy>=0) MQTT_send=(dummy<<16)|(MQTT_send&0xFFFF);			// Übernahme bei Eingabe
 putparameter (T_m_sadr,	MQTT_send&0x0000FFFF,1<<16,T_col);	// Ausgabe Speicherzeiger letzte Flash Sendeadresse
 dummy=getnumber(T_m_sadr,0,1056);  													// Abfrage Sendeadresse
 if (dummy>=0) MQTT_send=(MQTT_send&0xFFFF0000)|dummy; 				// Übernahme bei Eingabe
 putparameter (T_m_smin, poweron_min,1<<16,T_col);						// Ausgabe zufällige Versandminute
 dummy=getnumber(T_m_smin,0,59);															// Abfrage Versandminute										
 if (dummy>=0) poweron_min=dummy;															// Übernahme bei Eingabe
 putparameter (T_md_start,fp.md_start_page,1<<16,T_col);			// Änderung der Messdaten Seiten-Startzeigers
 dummy=getnumber(T_md_start,0,PROTOCOLPAGE-1);								// Abfrage
 if (dummy>=0) fp.md_start_page=dummy;												// Übernahme bei gültiger Eingabe
}	

void MQTT_fast_disconnect (uchar leave)	// Schneller Verbindungsabbruch mit Modemabschaltung
{																					// Übergabe leave - 1 Abmeldedialog	wenn möglich
 uchar concpy=connect;
	
 if (FIO0PIN&GSM_DTR) FIO0CLR=GSM_DTR;		// DTR HI? - Ja DTR -> LOW, Disable Modem Sleep
 osDelay(5);															// Warte bis bereit
 	
 if (!mqtt_fail && leave && ((FIO2PIN&CTS)==0))		// kein Empfangsfehler und Modem bereit?
 {	 
	if (mqtt_state>=JOINED) send_leave ();					// Publish zum /leave topic	
  osDelay(10);																		// 10 ms Pause	
  connect=UART1;																	// Nur Modem	
  send_mqtt_disconnect ();												// Sende disconnect an MQTTS
  connect=concpy;	
  osDelay(800);														// Pause, Nachrichten müssen vor Modemabschaltung gesendet sein	
 }	 
 connect&=~(MQTT_LINK|UART1);							// Mqtt Empfang beenden
 mqtt_state=DISCONNECT;										// MQTT Kommunikation beendet
 mqtt_mess_pend=0;												// Reset Anzahl MQTTS unquittierte Nachrichten
 gsm_power(0);														// Modem ohne Kontextabschluss hart abschalten	
 osDelay(50);															// 50 ms Pause	
 mqtt_com=0;															// reset Nachrichtenempfang
 mqtt_fail=0;															// Reset MQTT Empfangsfehler
 wait_min=3;															// 3 Warteminuten bis nächste Verbindung aufgebaut wird
}	







	




