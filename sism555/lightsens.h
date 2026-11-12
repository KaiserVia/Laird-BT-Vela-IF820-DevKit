/* Header Datei fuer lightsens.c - Umgebungslichtsensor VEML7700 (Mainboard V2, IC1) */

#ifndef LIGHTSENS_H_
#define LIGHTSENS_H_

#include "hard.h"
#include "i2cm.h"

// I2C-Adresse in der 8-Bit-Schreibweise von i2cm.c (7-Bit-Adresse 0x10, fest)
#define		VEML7700			0x20

// Kennlinie Lux -> Dimmfaktor (logarithmisch zwischen den Endpunkten, darunter/darueber begrenzt)
#define		LS_LUX_MIN		1					// bis zu dieser Helligkeit (lx) minimale Anzeigehelligkeit
#define		LS_LUX_MAX		10000			// ab dieser Helligkeit (lx) volle Anzeigehelligkeit
#define		LS_DIM_MIN		3					// Dimmfaktor bei LS_LUX_MIN (wie bisherige Fotodioden-Kennlinie bei Dunkelheit)
#define		LS_DIM_MAX		RES_PWM		// Dimmfaktor bei LS_LUX_MAX (200 = 100 %)

// Messablauf
#define		LS_EVERY			8					// nur bei jedem 8. Aufruf von get_dimmung() messen (je Messzyklus einer, ca. 4...12 s)
#define		LS_WAIT				150				// Wartezeit nach dem Einschalten bis zum Auslesen (ms), 100 ms Integrationszeit + Reserve

extern int lightsens_dimm (void);			// Dimmfaktor aus dem VEML7700, -1 = kein Sensor (V1 -> Fotodiode)
extern bool lightsens_present (void);	// VEML7700 vorhanden? (prueft beim ersten Aufruf)
extern int lightsens_measure (uint *raw, uchar *gain);	// Messung fuer das Testmenue (blockiert 150...600 ms), Rueckgabe 1/100 lx oder -1
extern text * lightsens_gaintext (uchar gain);					// Verstaerkung als Text ("x2", "x1", "x1/4", "x1/8")
extern uint lightsens_lux2dimm (uint clux);							// Kennlinie: 1/100 lx -> Dimmfaktor LS_DIM_MIN...LS_DIM_MAX

#endif /* LIGHTSENS_H_ */
