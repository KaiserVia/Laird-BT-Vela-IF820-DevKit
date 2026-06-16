/* Header Datei für i2cm.c */

#ifndef I2CM_H_
#define I2CM_H_

#include "hard.h"
#include "Driver_I2C.h"

// Typ Aliase 
#define		EXPD	1
#define		DPP		2
#define		EXPS	3
#define		RTC		4

#define Anz_I2C_Devices	7

// I2C Device Adressen
#define		IC35		0x58				// IC35 MCP4441 quad DPP Microchip für Verstärkereinstellung
#define		IC37		0x42				// IC37 PCA9555 Expander für Messkontrolle
#define		IC54		0x46				// IC54	PCA9554 Expander für Drehschalter und Interface Link
#define		IC58		0x44				// IC58	PCA9555 Expander für Mikromatch SV (Viasis Plus)
#define		ICLED		0x8A				// TCA6507 LED und Relais Treiber (extern)

// Bitflags Parameter devices in fp.i2cdev
#define		I2CB0			(1<<0)	// I2C Bus 0	
#define		IC37_b		(1<<1)	// IC37 PCA9555 Expander für Messkontrolle
#define		IC58_b		(1<<2)	// IC58	PCA9555 Expander für Relaisansteuerung
#define		IC54_b		(1<<3)	// IC54	PCA9555 Expander für Schnittstellen Links, Hex-Schalter etc.
#define		IC35_b		(1<<5)	// IC35 MCP4441 quad DPP Microchip für Verstärkereinstellung
#define		ICLED_b		(1<<6)	// TCA6507 LED und Relais Treiber (extern)


// Structures, Funktionszeiger, etc.
typedef struct ViasisI2cDevices {
	char const adr;								// I2C Adresse
	char const ic;								// IC Nummer
	char const typ;								// DPP, EXP single oder dual
	char const reg;								// Registeranzahl
	char const format;						// Ausgabeformat
	char const label[11];					// Bezeichner IC, z. B. PCA9555 usw.
}
i2cdevices;

extern const i2cdevices i2cdev[Anz_I2C_Devices];

// CMSIS Structures, Funktionszeiger, etc.
extern ARM_DRIVER_I2C * I2Cdrv;
extern ARM_DRIVER_I2C Driver_I2C0;							// Definition I2C0 Struktur
//extern ARM_DRIVER_SPI Driver_SPI1;							// Definition SPI1 Struktur	

// Funktionen extern bereit stellen
extern int Init_I2C	(void);																			// Initialisiere CMSIS I2C Treiber
extern bool write_i2c_dev (uchar dadr, uchar anz, uint data); 	// Anzahl Bytes an I2C Device senden
extern int read_i2cdev (uchar dadr, uchar adrcom, uchar anz);		// I2C Device mit repeated start auslesen
extern int InitI2C_Devices (void);															// I2C Bausteine initialisieren
extern int get_com_status (void);  						// Status IC54 Datenschnittstellen und Hexschalter einlesen
extern uchar get_turnswitch (void);							// Status Drehschalter lesen und Position dekodieren
extern void reset_switches (void);							// Setzt I2C Expander zurück
extern uchar setswitches(uchar speed);					// Schalter entsprechend Schaltschwellen setzen
extern void clear_all	(void);										// Numerische Anzeige und ggf. Zusatzanzeigen abschalten


#endif /* I2CM_H_ */

