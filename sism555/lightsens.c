//-----------------------------------------------------------------------------
//  FILE: lightsens.c		PROJECT: sism555
//-----------------------------------------------------------------------------
//  COMMENTS: Umgebungslichtsensor VEML7700 (Vishay) auf Mainboard V2 (IC1),
//            I2C0 (P0.27 SDA / P0.28 SCL), feste 7-Bit-Adresse 0x10.
//            Liefert den Dimmfaktor fuer die LED-Anzeige (dimm, 1...200).
//            Ohne Sensor (Mainboard V1) gibt lightsens_dimm() -1 zurueck,
//            get_dimmung() misst dann wie bisher ueber die Fotodiode SFH213.
//
//  MESSABLAUF: Die Anzeige-LEDs leuchten auf den Sensor (Aussparung in der
//            Frontscheibe). Gemessen wird deshalb nur, solange die Anzeige
//            dunkel ist, und nur bei jedem LS_EVERY-ten Aufruf von
//            get_dimmung() (einmal je Messzyklus, Standard 1,5 s):
//            Sensor einschalten, LS_WAIT ms warten (blockierend), ALS-Register
//            lesen, Sensor wieder abschalten. Die Anzeige wird nur vom
//            Hauptthread geschaltet, waehrend der Wartezeit bleibt sie dunkel.
//            Blockierend statt verteilt auf zwei Aufrufe, weil die CPU ohne
//            Verkehr nach jedem Messzyklus in den Deep Sleep geht und per
//            Watchdog-Reset (SLEEP_RES) aufwacht - eine angefangene Messung
//            ginge dabei verloren. Messwert, Verstaerkung und Aufrufzaehler
//            liegen deshalb im __noinit-RAM und ueberstehen diesen Reset.
//
//  MESSBEREICH: Integrationszeit fest 100 ms, Verstaerkung automatisch
//            (x2, x1, x1/4, x1/8). Uebersteuert (>= LS_RAW_HIGH) -> eine Stufe
//            unempfindlicher und beim naechsten Aufruf neu messen; zu klein
//            (< LS_RAW_LOW) -> beim naechsten Mal eine Stufe empfindlicher.
//            Aufloesung lt. aktuellem Datenblatt/Application Note (0,0042 lx
//            bei x2/800 ms); aeltere Datenblattstaende nennen 0,0036 lx. Der
//            Unterschied (ca. 15 %) ist fuer die Dimmung ohne Bedeutung. Die
//            Linearitaetskorrektur fuer > 1000 lx ist nicht umgesetzt, die
//            Anzeige ist dort ohnehin nahezu voll hell.
//-----------------------------------------------------------------------------
//  CREATED :   2026-11-12 (5.s1)
//-----------------------------------------------------------------------------

#include "lightsens.h"
#include "i2cm.h"

// VEML7700 Register (Kommandocodes)
#define		VEML_CONF			0x00			// ALS_CONF_0: Verstaerkung, Integrationszeit, Shutdown
#define		VEML_ALS			0x04			// ALS-Messwert, 16 Bit, LSB zuerst
#define		VEML_SD				0x0001		// ALS_SD: 1 = Shutdown
																				// ALS_IT (Bit 9:6) = 0000 -> 100 ms, Persistenz und Interrupt aus

#define		LS_GAINS			4
#define		LS_RAW_HIGH		60000			// ab hier Bereich wechseln (Uebersteuerung bei 65535)
#define		LS_RAW_LOW		1500			// darunter empfindlicher messen
#define		LS_MAGIC			0x4C530138	// Kennung gueltiger __noinit-Daten

static const ushort ls_gcode[LS_GAINS] = {0x0800, 0x0000, 0x1800, 0x1000};	// ALS_GAIN (Bit 12:11): x2, x1, x1/4, x1/8
static const ushort ls_res[LS_GAINS]   = {336, 672, 2688, 5376};						// Aufloesung bei 100 ms in 1/10 mlx je Zaehlwert
static text * const ls_gtext[LS_GAINS] = {"x2", "x1", "x1/4", "x1/8"};

enum { LS_UNKNOWN=0, LS_ABSENT, LS_PRESENT };	// Sensor nach jedem Reset neu pruefen

static uchar ls_state=LS_UNKNOWN;

// Ueberstehen den Deep-Sleep-Reset (SLEEP_RES), gueltig nur mit ls_magic==LS_MAGIC
static uint ls_magic		__noinit;
static uint ls_clux			__noinit;		// letzter gueltiger Messwert in 1/100 lx
static uint ls_calls		__noinit;		// Aufrufe von lightsens_dimm() seit der letzten Messung
static uchar ls_gain		__noinit;		// Index Verstaerkung
static uchar ls_valid		__noinit;		// 1 = ls_clux gueltig

static bool ls_write_conf (bool on)	// ALS_CONF schreiben: Verstaerkung, 100 ms, an/aus
{
 uint conf=ls_gcode[ls_gain]|(on ? 0 : VEML_SD);
 return (write_i2c_dev (VEML7700, 3, (conf<<8)|VEML_CONF));	// Kommando, LSB, MSB
}

static int ls_read_raw (void)			// ALS-Register lesen, -1 bei Fehler
{
 int r=read_i2cdev (VEML7700, VEML_ALS, 2);
 if (r<0) return (-1);
 return (((r&0xFF)<<8)|((r>>8)&0xFF));	// read_i2cdev setzt MSB-zuerst zusammen, VEML7700 sendet LSB zuerst
}

static int ls_measure_once (uint *raw, uchar *gain)	// Eine Messung (blockiert LS_WAIT ms)
{																										// Rueckgabe 1/100 lx, -1 Lesefehler, -2 uebersteuert
 int r;
 uint clux;

 if (!ls_write_conf(true)) return (-1);
 osDelay (LS_WAIT);
 r=ls_read_raw();
 ls_write_conf(false);
 if (r<0) return (-1);
 *raw=(uint)r;
 *gain=ls_gain;
 if ((r>=LS_RAW_HIGH) && (ls_gain<LS_GAINS-1)) { ls_gain++; return (-2); }	// uebersteuert -> unempfindlicher
 clux=(uint)r*ls_res[ls_gain]/100;				// 1/10 mlx -> 1/100 lx (max. 65535*5376 passt in 32 Bit)
 if ((r<LS_RAW_LOW) && ls_gain) ls_gain--;	// naechste Messung empfindlicher
 ls_clux=clux;
 ls_valid=1;
 return ((int)clux);
}

static bool ls_dark (void)				// Anzeige-LEDs aus?
{
 return (!(PINSEL4&PWM6) && (FIO2PIN&PWM_TRB));	// PWM getrennt und Treiber-OE\ HIGH (siehe Ledaus, main.c Einschaltanzeige)
}

static uint ls_log2q8 (uint x)		// log2(x) mit 8 Nachkommabits (x >= 1), Mantisse linear genaehert, monoton
{
 uint e=31-__CLZ(x);
 uint f=(e>=8) ? (x>>(e-8)) : (x<<(8-e));
 return ((e<<8)|(f&0xFF));
}

uint lightsens_lux2dimm (uint clux)	// Kennlinie: logarithmisch zwischen LS_LUX_MIN und LS_LUX_MAX
{
 uint lo=ls_log2q8(LS_LUX_MIN*100);
 uint hi=ls_log2q8(LS_LUX_MAX*100);
 if (clux<=LS_LUX_MIN*100) return (LS_DIM_MIN);
 if (clux>=LS_LUX_MAX*100) return (LS_DIM_MAX);
 return (LS_DIM_MIN+(LS_DIM_MAX-LS_DIM_MIN)*(ls_log2q8(clux)-lo)/(hi-lo));
}

text * lightsens_gaintext (uchar gain)
{
 return ((gain<LS_GAINS) ? ls_gtext[gain] : "?");
}

bool lightsens_present (void)		// VEML7700 vorhanden? Erster Aufruf nach Reset prueft (Sensor wird dabei abgeschaltet)
{
 if (ls_state==LS_UNKNOWN)
 {
  if ((ls_magic!=LS_MAGIC) || (ls_gain>=LS_GAINS))	// Kaltstart oder ungueltige Daten
  {
   ls_magic=LS_MAGIC;
   ls_gain=1;																				// Start mit x1
   ls_valid=0;
   ls_calls=LS_EVERY;															// sofort messen, sobald die Anzeige dunkel ist
  }
  if ((fp.i2cdev&I2CB0) && ls_write_conf(false)) ls_state=LS_PRESENT;	// antwortet -> Mainboard V2
  else ls_state=LS_ABSENT;																						// kein ACK -> V1, Fotodiode
 }
 return (ls_state==LS_PRESENT);
}

int lightsens_dimm (void)				// Dimmfaktor aus dem VEML7700, -1 = kein Sensor
{
 uint raw;
 uchar gain;
 int clux;

 if (!lightsens_present()) return (-1);

 if (ls_calls<LS_EVERY) ls_calls++;
 if (ls_dark() && (ls_calls>=LS_EVERY))			// Anzeige dunkel und Messung faellig?
 {
  clux=ls_measure_once(&raw, &gain);
  if (clux==-2) ls_calls=LS_EVERY;					// uebersteuert -> beim naechsten Aufruf neu messen
  else ls_calls=0;													// gemessen oder Lesefehler (letzten Wert behalten,
 }																						// naechster Versuch nach LS_EVERY Aufrufen)
 if (!ls_valid) return ((int)dimm);					// noch kein Messwert: Dimmfaktor halten
 return ((int)lightsens_lux2dimm(ls_clux));
}

int lightsens_measure (uint *raw, uchar *gain)	// Messung fuer das Testmenue (Anzeige muss aus sein)
{
 int clux=-2;
 uchar tries=LS_GAINS;

 if (!lightsens_present()) return (-1);
 while ((clux==-2) && tries--)				// bei Uebersteuerung mit kleinerer Verstaerkung wiederholen
  clux=ls_measure_once(raw, gain);
 if (clux>=0) ls_calls=0;
 return ((clux>=0) ? clux : -1);
}
