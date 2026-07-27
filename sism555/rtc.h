/* Header Datei für rtc.c, Projekt sis3000M	*/

#ifndef RTC_H_
#define RTC_H_

#include "hard.h"

// Definitionen Echtzeituhr Formate Zeitangaben
#define TIME				0			// hh:mm:ss
#define DATE_SHORT	1			// TT.MM.JJ
#define TIME_NODEL  2			// hhmmss
#define DATE_LONG		3		  // TT.MM.JJJJ
#define DATE_NODEL	4			// TTMMJJ
#define TIME_HEX		5
#define DATE_HEX		6
#define TIME_DEL		':'				// Delimiter Zeitformat, z. B. hh:mm:ss 
#define DATE_DEL		'.'				// Delimiter Datumsformat, z. B. tt.mm.jj

// Makros
#define		mask(n)	(uint)((1<<(n+1))-1)	// Maske n bits rechtsbündig

static const char TimeDateMask[] =  {5,6,6,5,4,12};


// Zeitsynchronisation - Quellen-IDs (Identitaet + Anzeige)
#define SRC_NONE    0
#define SRC_MANUAL  1
#define SRC_LTE     2
#define SRC_GPS     3
#define SRC_MQTT    4
// clock_set() Flags
#define CLK_FROM_UTC   0x01   // Eingabe ist UTC -> +fp.utc auf Lokalzeit
#define CLK_SET_UTC    0x02   // Quelle liefert UTC-Offset -> fp.utc setzen
#define CLK_FORCE      0x04   // manuell ausgeloest: Rang/Drift/Auto-Aus ignorieren
#define RESYNC_STUNDEN 12     // schwaechere Quelle darf erst nach 12 h nachziehen
// Info-Ereigniscodes Zeitsync (System 17, Typ I)
#define EV_TIME_MQTT   17050
#define EV_TIME_GPS    17051
#define EV_TIME_LTE    17052
#define EV_TIME_MANUAL 17053
/* Funktionen extern bereit stellen */
extern void RTC_IRQHandler (void);													// RTC-Interrupt jede Sekunde
extern void InitRTC (void);																	// Initialisierung der Echtzeituhrvoid 
extern void read_date_time (uchar typ, char * Pc);					// Zeit oder Datum in Übergabe-Puffer schreiben
extern void send_date_time (uchar stime, uchar menu);				// RTC Datum oder Zeit ausgeben
extern int dayofweek(int dd, int mm, int yyyy);             // Gibt Wochentag zurück, Achtung Index 1...7
extern int get_date_time (text *Pp, text *Pf, uchar len); 	// Datum oder Zeiteingabe vom Terminal prüfen
extern void set_time (char *tline);													// Setzt Zeit mit Pufferwerten
extern void set_date (char *dline);													// Setzt LPC1766 Datum mit Pufferwerten
extern void date_time (void);																// ASCII Zeit und Datumsstring lesen oder ausgeben
extern uchar test_sommerzeit (uchar setzen); 								// Prüfe auf Sommerzeit oder Winterzeit
extern void zeitumstellung (void);													// Prüfe und führe Sommer-Winterzeit Umstellung durch
extern uint minutenwert (char *buf);												// Pufferzeit hh:mm in Minuten umrechnen
extern uchar Aufgabenzeit (void);							// Prüfe ob aktuelle Zeit und Tag Mess- oder GSM Einschaltzeit ist

extern uchar clock_set (uchar dd, uchar mm, uint yyyy, uchar hh, uchar mi, uchar ss,
                        int8_t utc_off, uchar src, uchar flags); // zentrale Zeitsync
extern uchar g_timesrc;                                     // aktuelle Zeitquelle
extern uint  g_timesync_h;                                  // fp.hcount letzter Abgleich
extern const char *time_source_text (void);                // Klartext Zeitquelle
extern void clock_mark_manual (void);                       // manuelle Eingabe vermerken
#endif /*RTC_H_*/
