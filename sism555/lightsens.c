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
//  ERKENNUNG (5.s2): Der VEML7700 ist das Merkmal fuer Mainboard V2. Die
//            Version steht in fp.radar Bit 3:2 (BOARD_xxx in hard.h) und wird
//            wie bei BT/LTE/GPS nur beim Hochfahren geprueft (board_detect(),
//            nicht nach Deep-Sleep- oder Update-Reset): passt der gespeicherte
//            Stand (ein I2C-Zugriff), geht es sofort weiter; sonst voller Test
//            (Konfiguration schreiben und zuruecklesen, bis LS_DETECT_TRY mal),
//            bei einem Wechsel Info I17006 und Neustart des Sensorzustands.
//-----------------------------------------------------------------------------
//  CREATED :   2026-11-12 (5.s1)
//-----------------------------------------------------------------------------

#include "lightsens.h"
#include "i2cm.h"
#include "sio.h"
#include "dtc_codes.h"
#include "rtc.h"

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

// Ueberstehen den Deep-Sleep-Reset (SLEEP_RES), gueltig nur mit ls_magic==LS_MAGIC
static uint ls_magic		__noinit;
static uint ls_clux			__noinit;		// letzter gueltiger Messwert in 1/100 lx
static uint ls_calls		__noinit;		// Aufrufe von lightsens_dimm() seit der letzten Messung
static uchar ls_gain		__noinit;		// Index Verstaerkung
static uchar ls_valid		__noinit;		// 1 = ls_clux gueltig
static uint ls_sig_h		__noinit;		// 5.s2: fp.hcount beim letzten Signal (Lichtsensor lebt)
static uchar ls_err			__noinit;		// 5.s2: V2 Messversuche in Folge ohne Antwort
static uchar ls_bad			__noinit;		// 5.s2: 1 = V2 VEML7700 defekt (W17007 gemeldet)
static uchar ls_stuck		__noinit;		// 5.s2: 1 = W17008 gemeldet
static uchar ls_clk			__noinit;		// 5.s2: 1 = Uhr seit dem Kaltstart gestellt (g_timesrc geht bei jedem Deep-Sleep-Reset verloren)
static uchar ls_noclk		__noinit;		// 5.s2: 1 = W17009 gemeldet
static uchar ls_mode		__noinit;		// 5.s2: aktuelle Regelung LS_MODE_xxx

static void ls_reset_state (void)	// Sensorzustand neu beginnen (Kaltstart, ungueltige Daten, Mainboard gewechselt)
{
 ls_magic=LS_MAGIC;
 ls_gain=1;																					// Start mit x1
 ls_valid=0;
 ls_calls=LS_EVERY;																	// sofort messen, sobald die Anzeige dunkel ist
 ls_sig_h=fp.hcount;																// 48-h-Frist beginnt neu
 ls_err=0; ls_bad=0; ls_stuck=0;
 ls_clk=0; ls_noclk=0; ls_mode=LS_MODE_SENSOR;
}

static void ls_state_ok (void)		// __noinit-Daten gueltig? Sonst neu beginnen
{
 if ((ls_magic!=LS_MAGIC) || (ls_gain>=LS_GAINS)) ls_reset_state();
}

static void ls_signal (bool seen)	// 5.s2: Signal gesehen? Sonst nach LS_STUCK_H Betriebsstunden einmal W17008
{
 if (seen) { ls_sig_h=fp.hcount; ls_stuck=0; return; }
 if (!ls_stuck && ((uint)(fp.hcount-ls_sig_h)>=LS_STUCK_H))
 {
  ls_stuck=1;
  puterror (DTC_MA_LS_NOSIGNAL, BOARD_TYP ? BOARD_TYP>>2 : 3);	// Wert 1 = V1 (Fotodiode), 2 = V2 (VEML7700), 3 = unbekannt
																		// (nie 0: puterror_dtc() verwirft Wert 0 ohne Ausgabe/Protokoll)
 }
}

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
 ls_signal ((r>=LS_RAW_SIG) && !((r>=65535) && (ls_gain==LS_GAINS-1)));	// 5.s2: 0 oder Anschlag bei x1/8 = kein Signal
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

static bool ls_verify (void)			// Voller Test: ALS_CONF (Shutdown) schreiben und zuruecklesen
{																	// true nur, wenn der Baustein auf 0x10 genau den VEML7700-Wert liefert
 int r;
 uchar t;

 for (t=0; t<LS_DETECT_TRY; t++)
 {
  if (t) osDelay (5);
  if (!ls_write_conf(false)) continue;										// kein ACK
  r=read_i2cdev (VEML7700, VEML_CONF, 2);
  if (r<0) continue;
  r=((r&0xFF)<<8)|((r>>8)&0xFF);													// LSB zuerst
  if (r==(int)(ls_gcode[ls_gain]|VEML_SD)) return (true);	// Verstaerkung, 100 ms, Shutdown wie geschrieben
 }
 return (false);
}

void board_detect (void)				// 5.s2: Mainboard V1/V2 beim Hochfahren pruefen (I2C-Bus muss bereit sein)
{
 uchar alt=BOARD_TYP, neu;

 ls_state_ok();																					// Kaltstart oder ungueltige Daten

 if ((alt==BOARD_V2) && ls_write_conf(false)) { ls_err=0; ls_bad=0; return; }	// schnell: Sensor antwortet wie gespeichert
 if ((alt==BOARD_V1) && !ls_write_conf(false)) return;	// schnell: weiterhin kein Sensor
 neu=ls_verify() ? BOARD_V2 : BOARD_V1;									// unbekannt oder Abweichung: voller Test
 if ((alt==BOARD_V2) && (neu==BOARD_V1))								// gespeichert V2, Sensor antwortet nicht: Das Mainboard (mit dem
 {																											// Parameter-Flash) ist dasselbe geblieben, also VEML7700 defekt
  if (!ls_bad) puterror (DTC_MA_LS_DEFEKT, 1);					// W17007 Wert 1 = beim Hochfahren (nicht 0, siehe ls_signal)
  ls_bad=1; ls_err=LS_ERR_MAX;
  return;																								// bleibt V2, feste Helligkeit LS_DIM_DEFEKT
 }
 if (neu==alt) return;																	// Einzelfehler beim schnellen Zugriff, Stand bestaetigt
 if (alt!=BOARD_UNBEKANNT) puterror (DTC_MA_BOARD_TYPE, neu>>2);	// I17006 Wechsel melden (1 = V1, 2 = V2)
 fp.radar=(fp.radar&~BOARD_MASK)|neu;										// merken (wird mit dem Parameterblock gesichert)
 ls_reset_state();																			// Sensorzustand neu beginnen
}

text * board_text (void)
{
 return (BOARD_TYP==BOARD_V2 ? "V2" : BOARD_TYP==BOARD_V1 ? "V1" : "unbekannt");
}

bool lightsens_diode_check (uint adc)	// 5.s2: V1 Fotodiode ueberwachen: 0 (offen) oder Anschlag = kein Signal
{																				// false = 48 h ohne Signal (W17008) -> Ersatzregelung
 ls_state_ok();
 ls_signal ((adc>LS_ADC_SIG_LO) && (adc<LS_ADC_SIG_HI));
 if (!ls_stuck) ls_mode=LS_MODE_SENSOR;
 return (!ls_stuck);
}

uint lightsens_fallback (void)		// 5.s2: Ersatz-Dimmfaktor, wenn der Sensor nicht geht
{
 uchar h;

 ls_state_ok();
 if (g_timesrc!=SRC_NONE) ls_clk=1;				// Uhr wurde seit dem Kaltstart gestellt (Backend, GPS, Netz, manuell)
 if (!ls_clk)
 {
  if (!ls_noclk) { ls_noclk=1; puterror (DTC_MA_LS_NOCLK, -1); }	// W17009 einmal je Kaltstart
  ls_mode=LS_MODE_FEST;
  return (LS_DIM_NOCLK);										// Uhr nicht gestellt: dauerhaft 50 %
 }
 ls_mode=LS_MODE_UHR;
 h=HOUR;																		// Ortszeit der Geraeteuhr
 return (((h>=LS_TAG_VON) && (h<LS_TAG_BIS)) ? LS_DIM_TAG : LS_DIM_NACHT);
}

uchar lightsens_mode (void)
{
 return ((ls_magic==LS_MAGIC) ? ls_mode : LS_MODE_SENSOR);
}

text * lightsens_modetext (void)
{
 switch (lightsens_mode())
 {
  case LS_MODE_UHR:  return ("Ersatz nach Uhrzeit");
  case LS_MODE_FEST: return ("Ersatz fest 50 % (Uhr nicht gestellt)");
  default:           return ("Sensor");
 }
}

int lightsens_nosig_h (void)		// Betriebsstunden seit dem letzten Signal, -1 = unbekannt
{
 if (ls_magic!=LS_MAGIC) return (-1);
 return ((int)(fp.hcount-ls_sig_h));
}

bool lightsens_defekt (void)		// V2 und VEML7700 antwortet nicht (W17007)
{
 return (IS_BOARD_V2 && (ls_magic==LS_MAGIC) && ls_bad);
}

bool lightsens_present (void)		// VEML7700 vorhanden? Nach der gespeicherten Mainboard-Version, ohne I2C-Zugriff
{
 ls_state_ok();																					// z. B. erster Aufruf ohne board_detect()
 return (IS_BOARD_V2 && (fp.i2cdev&I2CB0));
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
  if (clux==-1)															// naechster Versuch nach LS_EVERY Aufrufen)
  {
   if (ls_err<LS_ERR_MAX) ls_err++;
   if ((ls_err>=LS_ERR_MAX) && !ls_bad) { ls_bad=1; puterror (DTC_MA_LS_DEFEKT, 2); }	// W17007 Wert 2 = im Betrieb
  }
  else { ls_err=0; ls_bad=0; }							// Sensor antwortet (wieder)
 }
 if (ls_bad || ls_stuck) return ((int)lightsens_fallback());	// 5.s2: Sensor geht nicht -> Uhrzeit oder fest
 ls_mode=LS_MODE_SENSOR;
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
