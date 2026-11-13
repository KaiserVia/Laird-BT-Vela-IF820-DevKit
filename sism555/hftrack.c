//-----------------------------------------------------------------------------
//  FILE: hftrack.c		PROJECT: sism555
//-----------------------------------------------------------------------------
//  COMMENTS: Via-2-Radarempfaenger: Frequenzkanal, Werksabgleich und
//            Nachfuehrung der Sendefrequenz ueber das Digitalpoti IC32
//            (MCP4561, Wiper 0...256). 5.142, Anforderung HW-Kollege
//            (Mail 2026-11-13), Konzept Doku/HF_Kanal_Nachfuehrung.md.
//
//  KANAL:    10 Kanaele im Abstand von 5 MHz, Kanal k = 24155 + 5*k MHz
//            (24155...24200 MHz). Benachbarte Geraete sollen auf
//            verschiedenen Frequenzen senden, damit die Schwebung zwischen
//            ihren Sendesignalen ausserhalb des Doppler-Messbereichs liegt.
//            fp.hfkanal 0 = automatisch: letzte Ziffer der Seriennummer;
//            1...10 = Kanal 0...9 fest (Menue).
//
//  MESSUNG:  Ein Messpunkt schaltet den Transceiver wie im Betrieb nur kurz
//            ein (DPWR), verwirft eine Leermessung (erste Messung haeufig
//            falsch, Joachim 5.60) und nimmt den Median aus HFT_N
//            Messungen von mess_hf(). Einzelmessungen streuen um 2-3 MHz
//            nach unten (Board B, 5.141: 2 von 40), der Median filtert das.
//
//  NACHFUEHRUNG: geregelt wird direkt auf die gemessene Frequenz (keine
//            Temperatur auf dem Board). Erste Nachfuehrung HFT_FIRST_S nach
//            dem Einschalten, danach alle HFT_INTV_S, nur im Messzyklus vor
//            einer Messung (measure.c) und nur bei Via-2. Liegt die Frequenz
//            ausserhalb +/-HFT_TOL_KHZ um den Kanal, wird der Wiper um
//            -Abweichung/Steigung Stufen verstellt (hoechstens HFT_MAXSTEP,
//            bis HFT_TRIES mal). Die Steigung (kHz je Stufe, Vorzeichen
//            beliebig) wird beim Werksabgleich bestimmt bzw. bei Bedarf mit
//            einem Probeschritt gelernt. Endstellung einmal ins EEPROM des
//            Potis (bei ~35000 Schreibvorgaengen pro Jahr unkritisch), damit
//            sie einen Stromausfall der Analogversorgung uebersteht.
//            Die gemessene Frequenz geht in die Geschwindigkeitsberechnung
//            ein (set_measure_constants() ueber hf_tx_mhz()).
//
//  SLEEP:    Ohne Verkehr wacht die CPU per Watchdog-Reset (SLEEP_RES) aus
//            dem Deep Sleep auf. Zustand deshalb im __noinit-RAM mit
//            Kennung, Faelligkeit nach der RTC (laeuft durch).
//-----------------------------------------------------------------------------
//  CREATED :   2026-11-13 (5.142)
//-----------------------------------------------------------------------------

#include "hftrack.h"
#include "i2cm.h"
#include "sio.h"
#include "libtool.h"
#include "dtc_codes.h"

#define		HFT_MAGIC			0x48460142	// Kennung gueltiger __noinit-Daten

// MCP4561 Kommandos (Byte 1: Adresse<<4 | Kommando<<2 | D8), wie set_single_dpp()/f_characteristic()
#define		DPP_RD_VOL		0x0C				// Volatile Wiper 0 lesen
#define		DPP_WR_VOL		0x00				// Volatile Wiper 0 schreiben
#define		DPP_WR_NV			0x20				// Non-volatile Wiper 0 schreiben

// Warnungen W17010 (Wert), je Einschaltvorgang nur einmal je Grund
#define		HFT_W_MESS		1						// keine gueltige Frequenzmessung oder IC32 antwortet nicht
#define		HFT_W_FLACH		2						// Kennlinie flach / Steigung unplausibel
#define		HFT_W_ANSCHLAG	3					// Wiper am Anschlag, Kanalfrequenz nicht erreichbar
#define		HFT_W_ZIEL		4						// Kanalfrequenz nach HFT_TRIES Korrekturen nicht erreicht

// Ueberstehen den Deep-Sleep-Reset (SLEEP_RES), gueltig nur mit hft_magic==HFT_MAGIC
static uint hft_magic		__noinit;
static uint hft_due			__noinit;			// RTC-Sekunde der naechsten Nachfuehrung
static uint hft_last		__noinit;			// RTC-Sekunde der letzten Pruefung (Erkennung Jahreswechsel/Uhr gestellt)
static int  hft_slope		__noinit;			// Steigung in kHz je Wiper-Stufe, 0 = unbekannt
static uint hft_mhz			__noinit;			// zuletzt gemessene Sendefrequenz in MHz, 0 = unbekannt
static uchar hft_logged	__noinit;			// bereits protokollierte Warnungen (Bit = Grund)

static uint rtc_s (void)									// RTC-Zeit in Sekunden ab Jahresbeginn
{
 return ((uint)LPC_RTC->DOY*86400u + (uint)HOUR*3600u + (uint)MIN*60u + (uint)SEC);	// HOUR/MIN/SEC: RTC-Register (alilpc.h)
}

static void hft_reset (void)							// Zustand neu, erste Nachfuehrung nach HFT_FIRST_S
{
 hft_magic=HFT_MAGIC;
 hft_last=rtc_s();
 hft_due=hft_last+HFT_FIRST_S;
 hft_slope=0;
 hft_mhz=0;
 hft_logged=0;
}

static void hft_warn (uchar grund)				// W17010 einmal je Grund und Einschaltvorgang
{
 if (hft_logged&(1<<grund)) return;
 hft_logged|=(1<<grund);
 puterror (DTC_MA_HF_TRACK, grund);
}

static int iabs (int x) { return (x<0) ? -x : x; }

uchar hf_kanal (void)										// wirksamer Kanal 0...9
{
 int i;
 if ((fp.hfkanal>=1) && (fp.hfkanal<=HF_KANAELE)) return (fp.hfkanal-1);	// fest eingestellt
 for (i=sizeof(fp.serno)-1; i>=0; i--)				// automatisch: letzte Ziffer der Seriennummer
  if ((fp.serno[i]>='0') && (fp.serno[i]<='9')) return ((fp.serno[i]-'0')%HF_KANAELE);
 return (0);
}

uchar hf_kanal_auto (void)
{
 return (!((fp.hfkanal>=1) && (fp.hfkanal<=HF_KANAELE)));
}

uint hf_ziel_mhz (void)
{
 return (HF_KANAL_MHZ(hf_kanal()));
}

uint re_time (void)											// wirksame RE/-Zeit in ms
{
 if ((fp.re_ms>=RE_MS_MIN) && (fp.re_ms<=RE_MS_MAX)) return (fp.re_ms);		// fest eingestellt
 return ((fp.vmm<5) ? RE_MS_LOW : RE_MS_DEF);								// automatisch nach kleinster Messgeschwindigkeit
}

uint t_on_delay (void)									// Wartezeit Einschalten -> Messbeginn (wie transceiver(2))
{
 if (IS_VIA2) return (re_time());
 return (DPWR+DTXFEN_T+t_amp_en);
}

uint hf_tx_mhz (void)
{
 return ((hft_magic==HFT_MAGIC) ? hft_mhz : 0);
}

static int wiper_read (void)							// Volatile Wiper IC32, -1 = Fehler
{
 int r=read_i2cdev (IC32, DPP_RD_VOL, 2);
 if (r<0) return (-1);
 return (r&0x1FF);
}

static bool wiper_write (int w, bool nv)	// Volatile (und ggf. non-volatile) Wiper IC32 setzen
{
 bool ok;
 if (w<0) w=0;
 if (w>HFT_WMAX) w=HFT_WMAX;
 ok=write_i2c_dev (IC32, 2, ((w&0xFF)<<8)|(DPP_WR_VOL|(w>>8)));
 if (nv && ok)
 {
  ok=write_i2c_dev (IC32, 2, ((w&0xFF)<<8)|(DPP_WR_NV|(w>>8)));
  osDelay(10);															// EEPROM-Schreibzeit MCP4561 max. 5 ms
 }
 return (ok);
}

uint hf_measure_khz (void)								// Messpunkt, Rueckgabe Frequenz in kHz, 0 = Fehler
{
 uint t[HFT_N], r, x;
 uchar n=0, i, j;
 unsigned long long f;

 if (!IS_VIA2) return (0);
 if (transceiver (ein)<0) { transceiver (aus); return (0); }
 osDelay(DPWR);															// Einschwingen wie in f_characteristic()
 mess_hf ();																// Leermessung, erste Messung haeufig falsch
 for (i=0; i<HFT_N; i++)
 {
  r=mess_hf ();
  if (r) t[n++]=r;
 }
 transceiver (aus);
 if (n<(HFT_N+1)/2) return (0);								// zu wenige gueltige Messungen
 for (i=1; i<n; i++)													// sortieren (n <= 7)
  for (j=i; j && (t[j-1]>t[j]); j--) { x=t[j]; t[j]=t[j-1]; t[j-1]=x; }
 f=((unsigned long long)HF_const*1000ull)/t[n/2];		// Median, Frequenz in kHz
 if ((f<HFT_FMIN_KHZ) || (f>HFT_FMAX_KHZ)) return (0);
 return ((uint)f);
}

static void put_mhz (int khz)							// kHz als MHz mit einer Nachkommastelle
{
 if (khz<0) { putc('-'); khz=-khz; }
 putnumber ((uint)khz/1000, 0); putc('.'); putnumber (((uint)khz%1000)/100, 0);
 putstr (" MHz");
}

static void put_step (int w, int khz, int ziel)	// eine Zeile "Stufe www  ffff.f MHz  (+d.d)"
{
 putstr ("Stufe "); putnumber ((uint)w, 3); putstr ("  ");
 if (khz) { put_mhz (khz); putstr ("  ("); if (khz>=ziel) putc('+'); put_mhz (khz-ziel); putc(')'); }
 else putstr ("Messfehler");
 newline ();
}

static int measure_at (int w, int ziel, uchar verbose)	// Wiper (volatile) setzen, einschwingen lassen, messen
{																		// Nach einem Potisprung laeuft die Frequenz langsam nach (Board B, 5.142:
 int f, fv=0;												// Bisektion mit 5 ms Wartezeit mass noch die alte Frequenz). Deshalb messen,
 uchar n;														// bis zwei Messpunkte hoechstens HFT_STABLE_KHZ auseinander liegen.
 if (!wiper_write (w, false)) return (0);
 osDelay(HFT_SETTLE);
 for (n=1; n<=HFT_STABLE_N; n++)
 {
  ResetWDT ();
  f=(int)hf_measure_khz ();
  if (!f) break;
  if (fv && (iabs(f-fv)<=HFT_STABLE_KHZ)) break;
  fv=f;
  osDelay(HFT_SETTLE);
 }
 if (verbose) { put_step (w, f, ziel); putstr ("   ("); putnumber (n, 0); putstr (" Messpunkte)"); newline (); }
 return (f);
}

int hf_track_now (uchar verbose)						// Nachfuehrung, Rueckgabe Abweichung kHz; Fehler -1000000
{
 int w, w0, wn, f, f2, err, steps, s;
 int ziel=(int)hf_ziel_mhz()*1000;
 uchar k;

 if (!IS_VIA2) return (-1000000);
 if (hft_magic!=HFT_MAGIC) hft_reset ();
 w=w0=wiper_read ();
 f=(int)hf_measure_khz ();
 if ((w<0) || !f) { hft_warn (HFT_W_MESS); if (verbose) putln ("Messfehler"); return (-1000000); }
 if (verbose) put_step (w, f, ziel);
 hft_mhz=(uint)(f+500)/1000;

 for (k=0; k<HFT_TRIES; k++)
 {
  err=f-ziel;
  if (iabs(err)<=HFT_TOL_KHZ) break;											// im Totband: fertig
  if (hft_slope==0)																	// Steigung unbekannt: Probeschritt
  {
   wn=(w<=HFT_WMAX/2) ? w+HFT_LEARN : w-HFT_LEARN;
   f2=measure_at (wn, ziel, verbose);
   if (!f2) { hft_warn (HFT_W_MESS); wiper_write (w, false); return (-1000000); }
   if ((f2<HFT_BAND_LO) || (f2>HFT_BAND_HI)) { hft_warn (HFT_W_ANSCHLAG); wiper_write (w, false); return (-1000000); }
   s=(f2-f)/(wn-w);
   if (iabs(s)<HFT_SLOPE_MIN) { hft_warn (HFT_W_FLACH); wiper_write (w, false); return (-1000000); }
   hft_slope=s;
   w=wn; f=f2;
   continue;
  }
  steps=(-err)/hft_slope;															// Stufen = -Abweichung / Steigung, gerundet
  s=(-err)-steps*hft_slope;
  if (2*iabs(s)>=iabs(hft_slope)) steps+=(((-err)>0)==(hft_slope>0)) ? 1 : -1;
  if (steps>HFT_MAXSTEP) steps=HFT_MAXSTEP;
  if (steps<-HFT_MAXSTEP) steps=-HFT_MAXSTEP;
  if (steps==0) steps=((-err>0)==(hft_slope>0)) ? 1 : -1;
  wn=w+steps;
  if (wn<0) wn=0;
  if (wn>HFT_WMAX) wn=HFT_WMAX;
  if (wn==w) { hft_warn (HFT_W_ANSCHLAG); break; }
  f2=measure_at (wn, ziel, verbose);
  if (!f2) { hft_warn (HFT_W_MESS); wiper_write (w, false); return (-1000000); }
  if ((f2<HFT_BAND_LO) || (f2>HFT_BAND_HI)) { hft_warn (HFT_W_ANSCHLAG); wiper_write (w, false); break; }	// nie aus dem Band
  if (iabs(wn-w)>=4)																// Steigung nachlernen
  {
   s=(f2-f)/(wn-w);
   if (((s>0)==(hft_slope>0)) && (iabs(s)>=HFT_SLOPE_MIN)) hft_slope=(hft_slope+s)/2;
  }
  w=wn; f=f2;
  hft_mhz=(uint)(f+500)/1000;
 }
 err=f-ziel;
 if (iabs(err)>HFT_TOL_KHZ) hft_warn ((w==0||w==HFT_WMAX) ? HFT_W_ANSCHLAG : HFT_W_ZIEL);
 if (w!=w0) wiper_write (w, true);										// Endstellung dauerhaft
 set_measure_constants ();
 return (err);
}

void hf_track_init (uchar poweron)
{
 if (poweron || (hft_magic!=HFT_MAGIC)) hft_reset ();
}

void hf_track_poll (void)									// aus dem Messzyklus
{
 uint now;
 if (!IS_VIA2) return;
 if (hft_magic!=HFT_MAGIC) hft_reset ();
 now=rtc_s ();
 if ((now<hft_last) || (hft_due>now+HFT_INTV_S+HFT_FIRST_S)) hft_due=now;	// Jahreswechsel oder Uhr gestellt
 hft_last=now;
 if (now<hft_due) return;
 hft_due=now+HFT_INTV_S;															// auch bei Fehler erst im naechsten Intervall wieder
 hf_track_now (false);
}

int hf_adjust (void)											// Werksabgleich, Rueckgabe Frequenz MHz oder <0
{
 int ziel=(int)hf_ziel_mhz()*1000;
 int f0, f1, fl, fh, lo, hi, mid, f, w;
 bool steigend;

 if (!IS_VIA2) return (-1);
 if (hft_magic!=HFT_MAGIC) hft_reset ();
 putstr ("Kanal "); putnumber (hf_kanal()+1, 0); putstr (", Ziel "); put_mhz (ziel); newline ();
 f0=measure_at (0, ziel, true);								// Richtung aus Stufe 0 und Mitte (Stufe 256 liegt auf Board B mit
 f1=measure_at (HFT_WMAX/2, ziel, true);					// 24270 MHz ausserhalb des Bands, wird nicht angefahren)
 if (!f0 || !f1) { hft_warn (HFT_W_MESS); putln ("Messfehler, Abbruch"); return (-1); }
 if (iabs(f1-f0)<HFT_SLOPE_MIN*(HFT_WMAX/2)/4) { hft_warn (HFT_W_FLACH); putln ("Kennlinie flach, Abbruch"); return (-2); }
 steigend=(f1>f0);
 if ((f1<ziel)==steigend) { lo=HFT_WMAX/2; hi=HFT_WMAX; fl=f1; fh=0x7FFFFFFF; }	// Ziel in der oberen Haelfte
 else { lo=0; hi=HFT_WMAX/2; fl=f0; fh=f1; }
 while (hi-lo>1)																// Bisektion: lo liegt unter, hi ueber dem Ziel (bei steigend)
 {
  ResetWDT ();
  mid=(lo+hi)/2;
  f=measure_at (mid, ziel, true);
  if (!f) { hft_warn (HFT_W_MESS); putln ("Messfehler, Abbruch"); return (-1); }
  if ((f<ziel)==steigend) { lo=mid; fl=f; } else { hi=mid; fh=f; }
 }
 w=(iabs(fl-ziel)<=iabs(fh-ziel)) ? lo : hi;			// (fh=0x7FFFFFFF: hi nie gemessen -> lo)
 f=measure_at (w, ziel, true);										// Kontrolle in der Endstellung
 if (!f) { hft_warn (HFT_W_MESS); putln ("Messfehler, Abbruch"); return (-1); }
 if ((f<HFT_BAND_LO) || (f>HFT_BAND_HI)) { hft_warn (HFT_W_ANSCHLAG); putln ("ausserhalb 24150...24200 MHz, nicht gespeichert"); return (-3); }
 wiper_write (w, true);													// dauerhaft
 hft_mhz=(uint)(f+500)/1000;
 fp.TxF=(ushort)hft_mhz;													// Sendefrequenz-Parameter (wird beim Abschalten gesichert)
 set_measure_constants ();
 if (iabs(f-ziel)>HFT_TOL_KHZ) { hft_warn ((w==0||w==HFT_WMAX) ? HFT_W_ANSCHLAG : HFT_W_ZIEL); putln ("Kanalfrequenz nicht erreicht"); }
 else puterror (DTC_MA_HF_ADJUST, hft_mhz);					// I17011 Abgleich protokollieren
 putstr ("Abgleich: Stufe "); putnumber ((uint)w, 0); putstr (", "); put_mhz (f); newline ();
 return ((int)hft_mhz);
}

void hf_info (void)												// Statusausgabe im Via-2-Transceivermenue
{
 int w=wiper_read ();
 int f=(int)hf_measure_khz ();
 int ziel=(int)hf_ziel_mhz()*1000;

 if (f) hft_mhz=(uint)(f+500)/1000;
 set_measure_constants ();
 putstr ("Kanal "); putnumber (hf_kanal()+1, 0); putstr (hf_kanal_auto() ? " (automatisch)" : " (fest)");
 putstr (", Ziel "); put_mhz (ziel); newline ();
 putstr ("Frequenz "); if (f) { put_mhz (f); putstr ("  ("); if (f>=ziel) putc('+'); put_mhz (f-ziel); putc(')'); } else putstr ("Messfehler");
 putstr (", Stufe "); if (w>=0) putnumber ((uint)w, 0); else putc('?'); newline ();
 putstr ("RE/-Zeit "); putnumber (re_time(), 0); putstr (" ms"); putstr ((fp.re_ms>=RE_MS_MIN && fp.re_ms<=RE_MS_MAX) ? " (fest)" : " (automatisch)");
 putstr (", max. Messzeit "); putnumber (t_mess_max, 0); putstr (" ms"); newline ();
}
