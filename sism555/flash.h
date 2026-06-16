/* Header Datei für flash.c, Projekt MPX */

#ifndef FLASH_H_
#define FLASH_H_

#include "hard.h"

/* 	Information Anzahl Protokollseiten u. max. Anzahl Protokolleinträge
 	Aktuelle Protokollseite muss für Datenrettung immer gelöscht sein (fp.pro_page)
	Gleichzeitig werden im SRAM (probuf) bis zu (Seitengröße/Protokollsatzlänge)-1 Einträge gehalten

 	Typ		Größe	Seiten	Seitengröße	Seiten		Protokoll-	Max. Anzahl	   Max. Anzahl
										/1k Block	seiten		Protokollsätze Messwerte
AT45DB642	8 MB	8192	1024/1056	1			2			2*128-1= 255   842952
AT45DB321	4 MB	8192	512/528		2			3			3x64-1 = 191   425125
AT45DB161	2 MB	4096	512/528		2			3			3x64-1 = 191   212160
AT45DB081	1 MB	4096	256/264		4			5			5x32-1 = 159   105664
.....						
AT45DB041	512KB	2048	256/264		4			5			5x32-1 = 159	52415

AT45DB011, AT45DB021 unzulässig, da zu klein beim Firmware update		
*/

// Atmel Flash Opcodes
#define FLASH_ERASE_MAIN		0x81	// Erase Main Memory Page 
#define FLASH_ERASE_WRITE_1	0x82	// Write to Main Memory through Buffer 1 with Erase	
#define FLASH_W_BUF1				0x84	// Data to Flash Buffer 1
//#define FLASH_W_BUF2			0x87	// Data to Flash Buffer 2
#define FLASH_W_BUF1_MAIN		0x88	// Write Buffer 1 to Main Page without Erase
//#define FLASH_W_BUF2_MAIN	0x89	// Write Buffer 2 to Main Page without Erase
#define FLASH_WE_BUF1_MAIN	0x83	// Write Buffer 1 to Main Page with Erase
#define FLASH_R_MAIN			0x10D2	// Read Main Memory page 
#define FLASH_R_BUF1			0x10D1	// Read Buffer 1
#define FLASH_R_BUF2			0x10D3	// Read Buffer 2
#define FLASH_R_ARRAY			0x10E8	// Continuous Array Read
#define FLASH_R_ID				0x109F	// Manufacturer and Device ID read
#define FLASH_R_STATUS		0x10D7	// Lese Statusregister
#define FLASH_C_BUF1			0x60		// Compare Page to Buffer 1


//Bitpositionen, Definitionen SSP Control Register
#define DSS			0							// Register Datenlänge 
#define FRF			4							// SPI Protokoll
#define CPOL		6							// SPI Polarity 
#define CPHA		7							// SPI Phasenmode
#define SCR			8							// SPI Clock Teiler
#define SSE			1							// SPI enable
#define Bit8		0x0007				// Konfiguration mit Datenlänge 8 Bit
#define Bit16		0x000F				// Konfiguration mit Datenlänge 16 Bit
#define SSP8	  (Bit8<<DSS)		// 8 Bit mode
#define SSP16		(Bit16<<DSS)	// 16 Bit mode

// SPI Bus Takt und Konstanten
#define SSPCLK				3000000									// SSP Clock 3 MHZ
#define PCLK_SSP1			24000000								// Peripherieclock SSP1 		
#define SSPCLKDIV			2*(PCLK_SSP1/SSPCLK/2)	// Bereich 2...254, gerade Ganzahl

// Status Bit Atmel data Flash
#define FLASH_RDY	(1<<7)	// Flash busy bit

/* Funktionen extern bereit stellen */
extern void EINT0_IRQHandler (void);								// Interruptanforderung Power Down -> Daten im Flash sichern
extern void flash_fill_buffer (uint fbyte);					// Beschreibt Atmel Flash Buffer 1 mit fbyte
extern int InitSPIFlash (void);											// Initialisiere SPI und Flash
extern void flash_erase (uint page, uint anzahl);		// Eine oder mehrere Flash Seiten im Hauptspeicher löschen
extern uint flash_wbuf1 (uchar * Pdata, uint page, int size);	// Atmel Flash schreiben durch Puffer 1 auf gelöschte Seiten
extern int InitParameter (uchar werkinit);					// Flash Parameterinitialisierung
extern void delete_data(void);											// Messdaten im Speicher löschen
extern void protocol (short event);									// Protokolleintrag ins Flash sichern
extern void delete_protocol(void);									// Protokoll im Speicher löschen
extern uint anzahl_protokoll_ereignisse(void);			// Berechnet Anzahl der Protokollereignisse
extern void store (uint speed, uint distance);			// Geschwindigkeitsdaten im Flash sichern
extern void read_flash (void);											// Terminalausgabe Flash Seite im Hexformat
extern uint flash_pcom (uchar * Pdata, uint page, uint opcode, int size);	// Adaptiert Zugriff für unterschiedliche Flash Seitengrößen
extern int parameter_to_progmem (void);							// Schreibt Sicherungskopie des Parameterblocks in LPC1766 Programmspeicher
extern uchar mdata_to_flash (void);									// Schreibt Messdaten in Flash, Rückgabe 1 wenn Puffer voll

#endif /*FLASH_H_*/
