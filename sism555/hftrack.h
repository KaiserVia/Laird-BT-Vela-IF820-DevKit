/* Header Datei fuer hftrack.c - Via-2 Frequenzkanal, Abgleich und Nachfuehrung der Sendefrequenz (5.142) */

#ifndef HFTRACK_H_
#define HFTRACK_H_

#include "hard.h"

// Messung
#define		HFT_N				7					// Frequenzmessungen je Messpunkt (Median), davor eine Leermessung
#define		HFT_FMIN_KHZ	24100000		// plausibler Messbereich in kHz (Band 24150...24200 MHz plus Rand)
#define		HFT_FMAX_KHZ	24300000		// Board B: Stufe 256 = 24270 MHz, muss messbar sein

// Regelung
#define		HFT_TOL_KHZ		1500			// Totband: innerhalb +/-1,5 MHz um die Kanalfrequenz nicht nachstellen
#define		HFT_FIRST_S		120				// erste Nachfuehrung 2 min nach dem Einschalten
#define		HFT_INTV_S		900				// danach alle 15 min
#define		HFT_MAXSTEP		16				// Nachfuehrung: hoechstens so viele Poti-Stufen je Korrektur (Kennlinie oben bis 2,5 MHz/Stufe)
#define		HFT_SETTLE		50				// ms nach einem Potisprung, dann messen bis zwei Messpunkte uebereinstimmen
#define		HFT_STABLE_KHZ	300			// zwei Messpunkte gelten als eingeschwungen, wenn sie hoechstens so weit auseinander liegen
#define		HFT_STABLE_N	8				// hoechstens so viele Messpunkte beim Einschwingen
#define		HFT_BAND_LO		(Min_TXF_VIA2*1000)	// Nachfuehrung/Abgleich bleiben im Band 24150...24200 MHz
#define		HFT_BAND_HI		(Max_TXF_VIA2*1000)
#define		HFT_TRIES			3					// Nachfuehrung: hoechstens so viele Korrekturen je Durchgang
#define		HFT_LEARN			8					// Stufen fuer das Lernen der Steigung (kHz je Stufe)
#define		HFT_SLOPE_MIN	20				// kHz je Stufe: darunter gilt die Kennlinie als flach (unplausibel)
#define		HFT_WMAX			256				// MCP4561: Wiper 0...256

extern uchar hf_kanal (void);								// wirksamer Kanal 0...9 (fp.hfkanal oder aus der Seriennummer)
extern uchar hf_kanal_auto (void);						// true, wenn der Kanal aus der Seriennummer kommt
extern uint hf_ziel_mhz (void);							// Zielfrequenz des wirksamen Kanals in MHz
extern uint re_time (void);									// wirksame RE/-Zeit in ms (fp.re_ms oder automatisch)
extern uint t_on_delay (void);							// Wartezeit vom Einschalten bis zur Messung in ms (Messzeitberechnung)
extern uint hf_tx_mhz (void);								// zuletzt gemessene Sendefrequenz in MHz, 0 = unbekannt
extern uint hf_measure_khz (void);					// Messpunkt: Transceiver ein, Median aus HFT_N Messungen, aus; 0 = Fehler
extern void hf_track_init (uchar poweron);		// aus main(): bei Einschalten Zustand neu, erste Nachfuehrung nach HFT_FIRST_S
extern void hf_track_poll (void);						// aus dem Messzyklus: Nachfuehrung, wenn faellig
extern int hf_track_now (uchar verbose);			// Nachfuehrung sofort ausfuehren, Rueckgabe Abweichung in kHz oder Fehler <0
extern int hf_adjust (void);								// Werksabgleich auf die Kanalfrequenz (Menue), Rueckgabe Frequenz in MHz oder <0
extern void hf_info (void);									// Kanal, Zielfrequenz, Messung, RE/-Zeit, Messzeit ausgeben (Menue)

#endif /* HFTRACK_H_ */
