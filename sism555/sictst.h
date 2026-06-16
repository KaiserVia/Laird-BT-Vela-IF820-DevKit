/* Header Datei für sictst.c, Projekt LPC1766 Test */

#ifndef SICTST_H_
#define SICTST_H_

#include "hard.h"

typedef int (*funcp)(void);											// Prototyp Funktionszeiger

extern void i2c_scan (void);										// Check I2C bus for ackknowledging devices
extern void set_dpp (uchar adr, uchar feature);	// Digitalpoti einstellen
extern void test_i2c_device (void);							// Test PCA9555 Expander und MCP4662 DPPs
extern void flash_test (void);									// Test Atmel Flash Speicher
extern void test_rtc (uchar silent);						// Vergleichstest Echtzeituhr- und Haupt- Oszillator
extern void write_flash (void);									// Flash Seiten mit virtuellen Daten beschreiben
extern void licht_test (void);									// Test des Lichtsensors
extern void ledtest (void);											// Led Testprogramm/Messedemo
extern void LED_font_test (void);								// Anzeigetest für Fontnummer
extern void LED_check (void);										// Schneller LED Test 7-Segment und Symbole
extern uint test_battery (uchar mode);					// Batterietest
extern void amplifier_adjust (void); 						// Transceiver und Verstärkerabgleich
extern void transceiver_test (void);						// Test Transceiver/Verstärker Ansteuerung				
extern bool test_ledspot(void);									// Prüfe Steuerchip für externe LED Spotlampe

#endif /*SICTST_H_*/
