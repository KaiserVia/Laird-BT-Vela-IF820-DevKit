/* Header Datei für sicom.c, Projekt sis3000M */

#ifndef SICOM_H_
#define SICOM_H_

#include "hard.h"

/* Funktionen extern bereit stellen */

extern void mainmenu (char c);													// Zeichenbearbeitung RS232 Terminalschnittstelle 
extern uint messdaten (uchar ausgabe);									// Anzahl Messdaten berechnen und ausgeben
extern bool send_vtf_file (uint anzwerte, uchar ziel);	// VTF Datei senden	
extern int Set_parameter (uint blocks);									// Im flash abgelegte Parameterblocks prüfen und einlesen
extern void infomenu (uchar menu);											// Information als Menü oder Email Text ausgeben
extern void put_protocol (uchar ausgabe);								// Protokoll ausgeben
extern void radiostatus (void); 												// Bericht Tasks und Verbindungen, GSM, MQTT und GPS Status ausgeben

#endif /* SICOM_H_ */
