/* Header Datei für libtool.c */
#ifndef LIBTOOL_H_
#define LIBTOOL_H_

#include "hard.h"
#include <stdbool.h>

//#define		bool			uchar

extern bool isalpha (char x);										// prüft auf alphanumerische Zeichen
extern bool isdigit (char x);										// prüft auf alphanumerische Zeichen
extern char to_upper (char c);									// character to upper case
extern uint atoi (char *str);										// Ascii String to Integer Zahl
extern void strcopy (text *s, char *t);					// Nullterminierte Zeichenkette kopieren
extern void clean_cpy (text *s, char *t, uint8_t anz); 	// Nullterminierte Zeichenkette kopieren und restl. Zielarray löschen
extern void str_to_cbuf (char *s);							// Nullterminierte Zeichenkette nach cbuf
extern void copy_to_cbuf (char *s, uint len);		// Zeichenkette der Länge len nach cbuf kopieren
extern ushort strlen_to_cbuf (char *s);				  // Länge Zeichenkette ermitteln und nach cbuf
extern uint16_t crc (uint8_t *Puffer, uint16_t anzahl);	// Bilde CRC-16 über Anzahl Pufferbytes mit null init
extern uint16_t crcgen (uint8_t *Puffer, uint16_t anzahl, uint16_t oldcrc);	// Bilde CRC-16 mit oldcrc init
extern void DirSort (uchar n);									// Sortiert Periodendauern im Messpuffer
extern int number_exists (int nummer, const ushort *liste, uchar len); // Prüfe Fehler/Ereignis Liste auf Nummerneintrag
extern void num_to_LED (uint wert, int farbe);	// Numerischen Wert in 7 Segment Anzeigesteuerwert wandeln
extern void get_dimmung (void);									// Umgebungshelligkeit messen und Dimmfaktor bestimmen
extern int show_led (int time, uchar hell);			// LED Anzeige bis Zeitablauf oder Eingabe
extern int show_led_off (int time, uchar hell); // Dto. aber LED werden nach Ablauf ausgeschalten
extern void LED_188 (void);											// 188 LED power on, Batterie- und Echtzeituhr Prüfung
extern uint ledsymbol (uchar speed);						// Prüft Anzeigeschaltschwellen der LED Anzeigesymbole
extern int transceiver (uchar mode);						// Transceiver Ein-/Ausschaltung
extern void set_measure_constants(void);				// Konstanten für Messung anhand Parametereinstellung festlegen
extern uint display_mode_speed (uchar psatz);		// Prüfe Schwellen der Modi der Led Anzeige
extern int farbe_numLED (uchar speed);					// Prüfe Schwellen für LED Farben der numerischen Anzeige
extern void make_filename (void);								// Erstellt VTF Dateiname in cbuf
extern int compare (text *v, text *buf, ushort start, ushort stop); // Suche Zeichenkette in Puffer
extern bool file_is_firmware (void);						// Prüfen ob empfangene Datei Firmware
extern void modem_com (void);										// Direkte Kommunikation mit Modems
extern void init_turnsw (void);									// Drehschalter für Parametersatzwahl initialisieren und testen
extern void set_default_port (void);						// Ändert default I2C Expander Porteinstellung
extern void set_exp_switch (uchar switchno);		// Setzt definierte Schalter über I2C Expander
extern uchar nmea_checksum (text *s);						// XOR Prüfsumme GPS NMEA Nachricht bis * Abschlusszeichen
extern void clear_comchange (void);							// Bereinige Schnittstellenwechsel z. B. nach Modemausschaltung
extern uint colonpos(text *s, uchar anz);				// Ermittle Byteposition hinter anz.ter Kommastelle in Zeichenkette
extern uchar no_at (text *s);										// Sucht @ Zeichen in Zeichenkette
extern void Helligkeit (void);									// Einstellung der LED Dimmung
extern int plus_com (uchar mode);								// UART1 Kommunikation mit Matrix Controller Platine
extern void md_simulation (uint anzahl, uchar terminal);		// Erzeuge Messdaten im Speicher
extern void communication_change (void);				// Bearbeitung Schnittstellenwechsel, Änderung der Kommunikation
extern void reboot (void);											// Startet CPU neu

#endif /*LIBTOOL_H_*/
