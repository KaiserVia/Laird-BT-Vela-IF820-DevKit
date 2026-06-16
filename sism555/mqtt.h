/* Header Datei für mqtt.c, PROJECT: sis3000M */

// Aliase für compiler
#include "hard.h"	

#define MQTT_CONNECT			0x10
#define MQTT_CONNACK			0x20
#define MQTT_DISCONNECT		0xE0
#define MQTT_PINGREQ			0xC0
#define MQTT_PINGRESP			0xD0
#define MQTT_PUBLISH			0x30
#define MQTT_PUBACK				0x40
#define MQTT_SUBSCRIBE		0x82
#define MQTT_SUBACK				0x90
#define MQTT_UNSUBSCRIBE  0xA2	
#define MQTT_UNSUBACK			0xB0

#define RETAIN						(1<<0)
#define DUP								(1<<0)
#define QoS0							(0<<0)
#define	QoS1							(1<<1)
#define QoS2							(1<<2)

#define MQTT_STATUS_MINUTES		4				// MQTT STATUS alle Minuten senden
#define MQTT_MESSAGE_TIMEOUT	2				// MQTT Nachrichtentimeout
#define DEF_MQTT_PACKET_SIZE	5				// 

// Definitionen Nachrichtentypen zur Packetnachverfolgung
#define INVALID						0			// Ungültig = Löscht Nachricht in Handle Liste
#define STATUS						1			// Statusnachricht
#define PARAMTR						2			// Parameternachricht
#define PARASYM						3			// Vario Symboldatennachricht
#define PARAPLS						4			// PLUS Textseiten und Bitmapnachricht
#define	MDATA							5			// Messdatennachricht	
#define JOIN							6			// Join Nachricht
#define PINGREQ						7			// Ping Request	
#define SUBCOM						8			// Subscribe 
#define UNSUB							9			// Unsubscribe
#define EVNT							10		// Event Nachricht
#define BYE								11		// Leave Nachricht
#define DBGINFO						12		// Debug Nachricht
#define TINFO							13		// Zeit Nachricht

// Definition Kommunikationstatus
#define DISCONNECT				0			// DISCONNECT Zustand, Verbindung geschlossen
#define CONNECTED					1			// Just connected with MQTTS
#define DATASEND					2			// Messdaten zu senden (offline) 
#define DATACK						3			// Warte auf Puback Daten
#define SUBCOMMAND				4			// Subscribed to command
#define JOINED						5			// Join published
#define STATUSSEND				6			// Status published
#define EVENT							7			// Event ist zu publizieren
#define CLOSE							8			// Publiziere to /leave
#define LEAVE							9			// Close Connection

// Definitionen Messdatenversand
#define REALTIME					0			// Messdaten sofort senden
#define INTERVAL					1			// Messdaten Paketversand

// Bitflags	Parameter Änderung Übergabe
#define PARBLOCK					(1<<0)	// Parameterblock geändert
#define SYMBLOCK					(1<<1)	// Symbole (VARIO) geändert
#define PLUSDATA					(1<<2)	// PLUS Textseiten oder Bitmaps geändert
#define CERT							(1<<4)	// Zertifikat geändert
#define PPEND							(1<<7)	// Laufender Parameterversand

// Signtypes vgl. buscom.h für MQTT Join
#define	typ_VARIO				0
#define typ_VARIOPLUS		1
#define typ_VIASIS			2
#define typ_VIASISPLUS	3
#define typ_VIATEXT			4

// Routinen extern bereit stellen
//extern void mqtt_reception (void);								// Zeichenbearbeitung MQTT
//extern int send_mqtt_connect(void);							// MQTT connect senden
extern int send_mqtt_disconnect (void);						// MQTT disconnect senden
//extern void close_mqtt (uchar concpy);					// Schließt MQTT Verbindung
extern void send_mqtt_ping(void);									// MQTT Ping senden
extern void eval_mqtt_message (void);							// Werte MQTT Server Nachrichten aus
//extern void mqtt_subscribe (text *topic);					//	Subscribe to subtopic seriennummer/topic
//extern void send_mqtt_unsubscribe (text *topic);	// Unsubscribe von MQTT Server Topic
extern int Con_MQTT_server(uchar still);					// MQTT Server verbinden oder Verbindung prüfen
extern void MQTT_Connect (void const *argument);	// MQTT Verbindungsaufbau in eigenem Thread
extern void send_join	(void);											// Baue join Nachricht in cbuf auf
extern int send_mqtt_status (void);								// Baut Statusmeldung in cbuf auf und sendet diese
extern int send_mqtt_par (void);									// Sendet Parameterblock an MQTTS
extern void test_mqtt_com_state	(void);						// Prüfe MQTTS Kommunikationsstatus
extern void mqtt_state_incr (void);								// Published/Subscribed bis im SUBCOMMAND Status
extern void send_cmd_subscribe (void);						// Sendet subscribe to command topic
extern int send_leave (void);										// Publish zum /leave topic
extern int mqtt_md_bytes_to_send (void);					// Ermittelt die Anzahl zu sendender Messdatenbytes
extern int send_mqtt_data(int less);							// Sendet MQTT DataMessage
extern void get_mqtt_ind (void);									// Änderung MQTT Sendeindex und Speicherzeiger der Messdaten und der Versandminute
extern void MQTT_fast_disconnect (uchar leave);		// Schneller Verbindungsabbruch mit Modemabschaltung
