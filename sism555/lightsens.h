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
#define		LS_DETECT_TRY	3				// 5.s2: voller Test der Mainboard-Version, Versuche

// 5.s2: Ueberwachung des Lichtsensors (V1 Fotodiode, V2 VEML7700)
#define		LS_ERR_MAX		5				// V2: so viele Messversuche in Folge ohne Antwort -> W17007 (ca. 1 min)
#define		LS_STUCK_H		48				// so viele Betriebsstunden ohne Signal -> W17008
#define		LS_RAW_SIG		3				// V2: Rohwert ab dem ein Signal vorliegt (x2: ca. 0,1 lx)
#define		LS_ADC_SIG_LO	2				// V1: A/D-Wert (0...1023) der Fotodiode, ueber dem ein Signal vorliegt (0 = sehr hell)
#define		LS_ADC_SIG_HI	1020			// V1: A/D-Wert, ab dem der Eingang als am Anschlag gilt (dunkel); auf V2 ohne Fotodiode
																	// gemessen 1021...1023 (Board 26SC9983, 2026-11-13)

// 5.s2: Entscheidungsbaum Anzeigehelligkeit (Anwendervorgabe 2026-11-13)
//   Sensor geht                          -> nach dem Sensor regeln
//   Sensor geht nicht, Uhr gestellt      -> nach Uhrzeit: tags LS_DIM_TAG, nachts LS_DIM_NACHT
//   Sensor geht nicht, Uhr nicht gestellt -> dauerhaft LS_DIM_NOCLK
// "Sensor geht nicht" = V2 VEML7700 antwortet nicht (W17007) oder V1/V2 48 h ohne Signal (W17008).
#define		LS_DIM_TAG		(RES_PWM*80/100)	// 80 %
#define		LS_DIM_NACHT	(RES_PWM*40/100)	// 40 %
#define		LS_DIM_NOCLK	(RES_PWM*50/100)	// 50 %
#define		LS_TAG_VON		7				// Tag von 7:00 (Ortszeit der Geraeteuhr) ...
#define		LS_TAG_BIS		19			// ... bis 18:59

enum { LS_MODE_SENSOR=0, LS_MODE_UHR, LS_MODE_FEST };

extern int lightsens_dimm (void);			// Dimmfaktor aus dem VEML7700 bzw. Ersatz (Uhrzeit/fest), -1 = V1 -> Fotodiode
extern uint lightsens_fallback (void);	// 5.s2: Ersatz-Dimmfaktor nach Uhrzeit oder fest (W17009 ohne gestellte Uhr)
extern uchar lightsens_mode (void);		// 5.s2: aktuelle Regelung LS_MODE_xxx
extern text * lightsens_modetext (void);	// 5.s2: aktuelle Regelung als Text
extern void board_detect (void);			// 5.s2: Mainboard V1/V2 beim Hochfahren pruefen (aus InitI2C_Devices), Ergebnis in fp.radar Bit 3:2
extern bool lightsens_present (void);	// VEML7700 vorhanden? (gespeicherte Mainboard-Version V2 und I2C-Bus bereit)
extern bool lightsens_diode_check (uint adc);	// 5.s2: V1 Fotodiode ueberwachen (A/D-Wert aus get_brightness()), false = ohne Signal (W17008)
extern int lightsens_nosig_h (void);					// 5.s2: Betriebsstunden seit dem letzten Signal, -1 = unbekannt
extern bool lightsens_defekt (void);					// 5.s2: V2 und VEML7700 antwortet nicht (W17007)
extern text * board_text (void);			// 5.s2: gespeicherte Mainboard-Version als Text ("V1", "V2", "unbekannt")
extern int lightsens_measure (uint *raw, uchar *gain);	// Messung fuer das Testmenue (blockiert 150...600 ms), Rueckgabe 1/100 lx oder -1
extern text * lightsens_gaintext (uchar gain);					// Verstaerkung als Text ("x2", "x1", "x1/4", "x1/8")
extern uint lightsens_lux2dimm (uint clux);							// Kennlinie: 1/100 lx -> Dimmfaktor LS_DIM_MIN...LS_DIM_MAX

#endif /* LIGHTSENS_H_ */
