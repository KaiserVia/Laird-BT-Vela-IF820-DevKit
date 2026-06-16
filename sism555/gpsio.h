/* Header Datei für gpsio.c, Projekt vario */

#ifndef GPSIO_H_
#define GPSIO_H_

#include "hard.h"

// L70 GPS Textkonstanten
//#define T_pmtk			"$PMTK"
//#define T_CGGA			"$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*"
//#define T_CRMC			"$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*"
//#define T_C251			"$PMTK251,115200*"
//#define T_C300			"$PMTK300,250,0,0,0,0*"


// L76F GPS Textkonstanten
#define T_gntxt			"GNTXT"
#define T_C147			"$PGKC147,115200*"
#define T_C242A			"$PGKC242,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*"		// nur GGA Ausgabe
#define T_C242B			"$PGKC242,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*"		// GLL, RMC und GGA Ausgabe
#define T_C242C			"$PGKC242,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*"		// nur RMC Ausgabe
#define T_C101			"$PGKC101,250*"
#define T_pgkc			"$PGKC001"

// LC76G GPS Textkonstanten
#define T_pqtmver		"PQTMVER"																							// Startmeldung
#define T_pair62		"$PAIR062,"																						// Kommando 062 Protokoll aktivieren/deactivieren
#define T_null			",0*"
#define T_p62ack		"$PAIR001,062,0*3F"																		// Kommando 62 ACK
#define T_gngga			"$GNGGA"																							// GGA Datensatz erhalten?
#define T_grmc			"$GNRMC"

// EG91EX GPS AT Commands
//#define T_gpsport		"AT+QGPSCFG=\"outport\",\"uartdebug\""
//#define T_gpsnmea		"AT+QGPSCFG=\"gpsnmeatype\","
//#define T_gpsodp		"AT+QGPSCFG=\"odpcontrol\",2"
//#define T_gpson			"AT+QGPS=1,30,50,0,1"
//#define T_gpsoff		"AT+QGPSEND"
//#define T_$G				"$G"
//#define T_gpsgga		"AT+QGPSCFG=\"gpsnmeatype\",1"
//#define T_restart		"AT+CFUN=1,1"

// Routinen extern bereit stellen
extern int gps_power (uchar mode);				// Schalte GPS L70/LC76F Modem - aus/ein/restart
extern void gps_command (text *s, uchar output);	// Prüfsumme bilden und GPS Kommando senden
extern void init_gps (void);							// Initialisiere und teste GPS L70 Modul
extern int test_gps (uchar sendmessages);	// Prüfe ob GPS Modul Einschaltnachrichten sendet
extern void send_gps_raw (void);					// Ausgabe von GPS Rohdaten
extern void send_gps_GGA	(void);					// Formatierte Ausgabe des GPS NMEA GGA Datensatzes
extern void get_geopos (void);						// Ermittle GPS position
extern void send_gps_data (void);					// Ausgabe im Parameterblock gespeicherter Positionsdaten
extern void send_gps_position (uchar ind);							// GPS Einzelposition ausgeben

#endif // GPSIO_H_
