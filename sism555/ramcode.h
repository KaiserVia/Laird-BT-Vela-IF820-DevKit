/* Header Datei für ramcode.h, Projekt MPX */

#ifndef RAMCODE_H_
#define RAMCODE_H_

#include "hard.h"


#define CMD_SUCCESS			0						// Erfolgreiche IAP Kommandoausführung
#define IAP_RETRY_LIMIT	8						// Max. Anzahl Wiederholungen beim Firmware Update
#define IAP_LOCATION 		0x1FFF1FF1	// LPC1766 IAP Programminterface

typedef void (*IAP)(unsigned int [],unsigned int[]);	// Funktionszeiger IAP Progammierinterface

/* Funktionen extern bereit stellen */
//extern IAP 	iap_entry;															// Funktionszeiger IAP Programmierschnittstelle
extern void Init_SPI (void);													// SPI (SSP1) einschalten und initialisieren 
extern void Clear_SPI (void);													// Disable SPI
extern uchar flash_ready (uint check);								// Warte bis flash status ready oder return status
extern int flash_command (uint opcode, uint adrbytes, uint txbytes);	// Flash data transfer
extern void flash_adress (uint bytadr, uint seite);		// Schreibt dataFlash Byte und Page address
extern void iap_programming (uint blocks);						// Programmiert LPC7166 Programmspeicher

#endif /*RAMCODE_H_*/

