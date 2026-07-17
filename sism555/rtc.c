//-----------------------------------------------------------------------------
//  FILE: rtc.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für interne LPC1766 Echtzeituhr
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H2
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   06.11.2014
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:	22.04.2015:	Hardware version H1 -> H2
//              	
//-----------------------------------------------------------------------------

#include "rtc.h"
#include "hard.h"
#include "i2cm.h"
#include "sio.h"
#include "sictst.h"
#include "sicom.h"
#include "libtool.h"
#include "flash.h"
#include "mqtt.h"
#include "sictxt.h"

//-----------------------------------------------------------------------------
// Zentrale Zeitsynchronisation - alle Quellen (Backend/GPS/NITZ/manuell) hier durch
//-----------------------------------------------------------------------------
uchar g_timesrc    = SRC_NONE;   // aktuelle Zeitquelle (Anzeige + Rang)
uint  g_timesync_h = 0;          // fp.hcount beim letzten erfolgreichen Abgleich

static const uchar src_rank[5] = { 0, 2, 1, 2, 3 };  // NONE,MANUAL,LTE,GPS,MQTT

static int tage_im_monat (int m, int y)   // Tage im Monat inkl. Schaltjahr
{
 static const int d[13]={0,31,28,31,30,31,30,31,31,30,31,30,31};
 if (m==2 && ((y%4==0 && y%100!=0)||y%400==0)) return (29);
 if (m<1||m>12) return (30);
 return (d[m]);
}

void RTC_IRQHandler (void)						// RTC-Interrupt jede Sekunde oder Minute
{
	if (CIIR&IMSEC)	tsec=T1TC;					// Sekundeninterrupt, 1 MHz Timer 2 auslesen
	else if (CIIR&IMMIN) minute++;	 		// Minutenzähler inkrementieren
  while (ILR) ILR|=ILR;								// Reset Interrupt Location Register 
}

void InitRTC (void)				// Initialisierung der Echtzeituhr
{
 PCONP|= PCRTC;												// Interne Uhr (nochmal) einschalten
 if (RTC_INIT_CODE != PARAKENNUNG)		// Uhr bereits initialisiert? 
 {										
  RCCR=0;															// Reset and halt clock 1 Hz oscillator
  { /* Default = Build-Datum/-zeit (statt fix 2015): ungestellte Uhr bleibt plausibel */
   const char *bd=__DATE__;                 /* "Mmm dd yyyy" */
   const char *bt=__TIME__;                 /* "hh:mm:ss"     */
   static const char mon[]="JanFebMarAprMayJunJulAugSepOctNovDec";
   static const int  dowt[12]={0,3,2,5,0,3,5,1,4,6,2,4};   /* Sakamoto */
   int m,d,y,yy;
   for (m=0;m<12;m++) if (bd[0]==mon[m*3]&&bd[1]==mon[m*3+1]&&bd[2]==mon[m*3+2]) break;
   m++;                                      /* 1..12 */
   d=(bd[4]==' '?0:bd[4]-'0')*10+(bd[5]-'0');
   y=(bd[7]-'0')*1000+(bd[8]-'0')*100+(bd[9]-'0')*10+(bd[10]-'0');
   HOUR=(bt[0]-'0')*10+(bt[1]-'0'); MIN=(bt[3]-'0')*10+(bt[4]-'0'); SEC=(bt[6]-'0')*10+(bt[7]-'0');
   DOM=d; MONTH=m; YEAR=y;
   yy=y; if (m<3) yy--;
   DOW=(yy + yy/4 - yy/100 + yy/400 + dowt[m-1] + d) % 7;  /* 0=Sonntag */
  }
  RCCR|=(CLKEN|CCALEN);								// Clock enable, disable Calibration
	RTC_INIT_CODE = PARAKENNUNG;				// Parameter Kennung speichern 
 }  
 ILR|=ILR;														// Evtl. Anstehende Interrupts löschen
 CIIR=IMMIN;													// Minuteninterrupt freigeben
 
 NVIC_SetPriority(RTC_IRQn, 8);				// Priorität RTC Interrupt
 NVIC_EnableIRQ(RTC_IRQn);						// Enable RTC Interrupt	
}

void read_date_time (uchar typ, char * Pc)// Zeit oder Datum in Übergabe-Puffer schreiben
{ 												// Übergaben: 	typ - TIME, DATE_SHORT, DATE_LONG
													// Übergabe 	Pc	- Zeiger auf Puffer
 uchar i,l;								// Laufvariable	
 ushort wert;							// Hilfvariable ausmakierter Zeit/Datumswert
 uint rtcreg;							// Register Echtzeituhr					
 char del;								// Delimiter
 uchar digits=2;					// Anzahl Ausgabestellen
 const char *pmask = &TimeDateMask[0];	// Pointer auf Array mit Maskeninformation 
 		
 if ((typ==TIME)||(typ==TIME_HEX)||(typ==TIME_NODEL)) 		// Zeit lesen
 { 
  rtcreg=CTIME0; 	// LPC1766 Zeitregister lesen
  del=TIME_DEL; 	// Delimiter festlegen
  i=16;
 }	  
 else 		// Datum lesen
 { 
  rtcreg=CTIME1;	// LPC1766 Datumsregister lesen 
  del=DATE_DEL; 	// Delimiter festlegen
  pmask+=3; 
  i=0; 
 }	

 if (Pc==cbuf) cind=0; 		// Zeiger auf cbuf Pufferanfang? Ja, reset Pufferindex
 
 for (l=0;l<3;l++)						// 3 Zeit/Datumselemente senden/in Puffer schreiben 
 {  	
  wert=(rtcreg>>i)&mask(*pmask++);					// Registerwert
  if ((l==2) && ((typ==DATE_HEX)||(typ==DATE_NODEL))) wert%=100;	// Bei Datum HEX oder NODEL kurzer Jahreswert
  if (typ>=TIME_HEX) *Pc++=wert;						// Hexwerte nach Puffer  
  else
  {												// ASCII Ziffern in Puffer
   if ((l==2) && (typ==DATE_LONG)) digits=4;	// Langes Datum 4 Ziffern Jahreswert
   putnumber(wert,digits|0xC0);  				// Zeit/Datumselement maskieren und senden/in Puffer schreiben
   if ((l<2) && (typ!=DATE_NODEL) && (typ!=TIME_NODEL)) putc(del);	// Delimiter senden/in Puffer schreiben 	
  }
  if ((typ==TIME)||(typ==TIME_HEX)||(typ==TIME_NODEL)) i-=8; else i+=8;	// Offset für Bitposition Zeit oder Datum
 }
}

void send_date_time (uchar mode, uchar menu)	// RTC Datum, Zeit oder beides ausgeben
{ 										// Übergabe mode 	- 0/1/2 für Datums-/Zeitausgabe oder beides
											//					menu  - Menüpunktausgabe, 0 - keine
											// Achtung cind offset auf cbuf Ausgabepuffer wirksam!!!
 text *tp;												// Zeiger Ausgabetexte
 int loop=mode;										// Schleifenhilfsvariable
	
 do
 { 	 
  if (loop&0x01) tp=T_time;				// Zeiger auf Zeittext
	else tp=T_date;									// Zeiger auf Datumtext

  if (mode<2) newline();					// neue Zeile
	else if (loop<2) putstr(T_col);	// Komma und Leerzeichen zwischen Datum und Zeit 
  if (menu) itemno (menu);				// Ausgabe Menüpunkt
	putstr(tp);											// Datum oder Zeittext mit Doppelpunkt ausgeben
	putstr(T_dpkt);	
	if (loop&0x01) read_date_time (TIME, cbuf+cind);	// Zeit ausgeben 
	else read_date_time (DATE_LONG, cbuf+cind);				// Datum ausgeben
 }	 
 while (--loop>0);							
}

uchar Zeit_falsch (char *line)   // Test der Eingabe von Stunde, Minute und Sekunde  
{
   if (atoi(line)>23) return (1);   // Stunde > 23 -> Fehler      
 
   if(atoi(line+3)>59) return (1);  // Minute > 59 -> Fehler      

   if (*(line+5)=='\0') return (0); // verkürzte Zeiteingabe      

   if(atoi(line+6)>59) return (1);  // Sekunde > 59 -> Fehler     
   
   return (0);                      // Alles ok 
}

uchar Datum_falsch (char *line) // Teste Eingabe von Tag, Monat und Jahr
{
   uchar Tag,Monat;
   uint Jahr;
   
   Tag = atoi (line);         // Ascii nach Hex Tag   
   Monat = atoi (line+3);     // Ascii nach Hex Monat 
   Jahr = atoi (line+6);      // Ascii nach Hex Jahr  
   
   if (Jahr<100) Jahr+=2000;	// Kurzes Jahresformat erweitern

   if (Jahr < 2011 || Jahr > 2099) return (1); 	// Jahr von 2011 bis 2099 ok 

   if (Tag < 1 || Tag > 31) return (1);     // Tage von 1 bis 31 gültig       

   if (Monat<1||Monat>12) return (1);       // Monate von 1 bis 12 gültig     

   if((Monat==4 || Monat==6 || Monat==9 || Monat==11) &&
       Tag > 30) return (1);               // Monate mit nur 30 Tagen 

   if (Monat==2) {                         // Monat Februar                  
    if (Jahr%4 && Tag > 28) return (1);    // kein Schaltjahr -> 28 Tage     
    else if (Tag > 29) return (1); }       // im Schaltjahr 29 Tage            

   return (0);
}

int dayofweek(int dd, int mm, int yyyy)              	// Gibt Wochentag zurück
																											// Übergaben dd/mm/yyyy	- Tag, Monat, Jahr unbedingt vierstellig
{ 
  int s=0, mtag[13]={0,31,28,31,30,31,30,31,31,30,31,30,31};    // Anzahl der Tage pro Monat
  if ((yyyy % 4==0 && yyyy % 100!=0) || yyyy % 400==0) s=1;     // Schaltjahrescheck
  if (yyyy<1583 || yyyy>9999) return(-1);                       // Jahr auf sinnvollen Bereich einschränken
  if (mm<1 || mm>12) return(0);                                 // Monat auf Gültigkeit prüfen
  if (dd<1 || dd>mtag[mm]+s*(mm==2)) return(0);                 // Tag auf Gültigkeit prüfen
  if (mm < 3) { mm += 13; yyyy--; } else  mm++;                 // Jahresanfang auf März verschieben
  s = dd + 26*mm/10 + yyyy + yyyy/4 - yyyy/100 + yyyy/400 + 6;  // eigentliche Berechnung
		// Rückgabe:
		// 1  = Sonntag, 2 = Montag, ... , 7 = Samstag
		// 0  = Datum ungültig z.B. 29.2.2007, 32.12.2007
		// -1 = Jahr kleiner als 1583 oder größer als 9999
  return((s % 7) + 1);                                            // Ergebnis anpassen
}

int get_date_time (text *Pp, text *Pf, uchar len) 	// Datum oder Zeiteingabe vom Terminal prüfen
{                                           	  
 uchar ergebnis;		// Hilfsvariable Anzahl gelesener Zeichen
 uchar delimiter='.';	// Zeit-/Datumstrennzeichen
 
 if (len<10) delimiter=':';	// Zeittrennzeichen
                     	    
 do 	// Solange bis gültige Eingabe oder CR
 {
  putstr (Pp);
	putstr (Pf); 
  putstr(T_ist);
  ergebnis = getline (cbuf,len,delimiter);	// Zeit-/Datum einlesen
  newline();
  if (ergebnis==len)
  {
   if (len==10) { if (!Datum_falsch (cbuf)) return (1); }	// Datum
   else { if (!Zeit_falsch (cbuf)) return (1); }			// Zeit    
  }       
  if (!ergebnis) return (0);                // 0 = Eingabe Return           
 }                                          // 1 = korrekte Zeit/ Datum 
 while (1);
}

void set_time (char *tline)			// Setzt Zeit mit Pufferwerten
{
 uchar nsec=0;

 if (*(tline+5)!='\0') nsec=atoi(tline+6); 	// Sekundenwert lange Zeiteingabe
 
 RCCR=0;										// Reset and halt clock 
 HOUR=atoi(tline);					// Stundenregister setzen
 MIN=atoi(tline+3);					// Minutenregister setzen
 SEC=nsec;									// Sekundenregister setzen
 RCCR|=(CLKEN|CCALEN);			// Clock enable, no calibration
}

void set_date (char *dline)			// Setzt Datum mit Pufferwerten
{
 uint year;

 RCCR=0;										// Reset and halt clock 
 DOM=atoi(dline);						// Tagesregister setzen 
 MONTH=atoi(dline+3);				// Monatsregister setzen
 year=atoi(dline+6);				// Jahreswert lesen
 if (year<100) year+=2000;	// Ggf. kurzes in langes Jahresdatum wandeln
 YEAR=year;									// Jahresregister setzen
 DOW=dayofweek(DOM, MONTH, YEAR)-1;  // Berechnet und setzt Wochentag
 RCCR|=(CLKEN|CCALEN);			// Clock enable, no calibration	
}

static void clock_logsrc (uchar src)  // Quelle vermerken + Info-Ereignis NUR bei Wechsel
{
 if (g_timesrc==src) return;
 switch (src)
 {
  case SRC_MQTT:   protocol(EV_TIME_MQTT);   break;
  case SRC_GPS:    protocol(EV_TIME_GPS);    break;
  case SRC_LTE:    protocol(EV_TIME_LTE);    break;
  case SRC_MANUAL: protocol(EV_TIME_MANUAL); break;
  default: break;
 }
 g_timesrc=src;
}

uchar clock_set (uchar dd, uchar mm, uint yyyy, uchar hh, uchar mi, uchar ss,
                 int8_t utc_off, uchar src, uchar flags)  // 1=uebernommen, 0=verworfen
{
 int h, wd;

 // 1. Validierung - offensichtlich falsche Zeiten verwerfen
 if (yyyy<2024 || yyyy>2099) return (0);
 if (hh>=24 || mi>=60 || ss>=60) return (0);
 if (dayofweek(dd,mm,yyyy) <= 0) return (0);            // 0/-1 = ungueltiges Datum

 // 2. Auto-Zeit abgeschaltet? Nur Hintergrundquellen blocken (manuell + Force laufen)
 if (fp.autotime_off==1 && src!=SRC_MANUAL && !(flags&CLK_FORCE)) return (0);

 // 3. Rang-/Drift-Pruefung (ausser bei manuellem Force)
 if (!(flags&CLK_FORCE) && g_timesrc!=SRC_NONE)
  if (src_rank[src] < src_rank[g_timesrc])                 // schwaechere Quelle
   if ((uint)(fp.hcount - g_timesync_h) < RESYNC_STUNDEN)  // letzter Abgleich noch frisch
    return (0);

 // 4. UTC -> Lokalzeit (fp.utc enthaelt bereits den DST-Versatz)
 if (flags & CLK_FROM_UTC)
 {
  h = (int)hh + fp.utc;
  while (h<0)   { h+=24; if(--dd<1)  { if(--mm<1){mm=12;yyyy--;} dd=tage_im_monat(mm,yyyy); } }
  while (h>=24) { h-=24; if(++dd>tage_im_monat(mm,yyyy)){ dd=1; if(++mm>12){mm=1;yyyy++;} } }
  hh = (uchar)h;
 }

 // 5. Quelle liefert autoritativen UTC-Offset
 if (flags & CLK_SET_UTC) fp.utc = utc_off;

 // 6. Schon korrekt (gleiches Datum, <=3 s Abweichung)? -> nur Buchfuehrung
 {
  long cur = (long)HOUR*3600 + MIN*60 + SEC;
  long nw  = (long)hh*3600   + mi*60  + ss;
  long df  = nw-cur; if (df<0) df=-df;
  if (YEAR==yyyy && MONTH==mm && DOM==dd && df<=3)
  { clock_logsrc(src); g_timesync_h=fp.hcount; return (1); }
 }

 // 7. RTC setzen
 RCCR=0;
 SEC=ss; MIN=mi; HOUR=hh;
 DOM=dd; MONTH=mm; YEAR=yyyy;
 wd=dayofweek(dd,mm,yyyy); DOW=wd-1;
 RCCR|=(CLKEN|CCALEN);

 // 8. Buchfuehrung + Info-Ereignis (nur bei Quellenwechsel)
 clock_logsrc(src);
 g_timesync_h=fp.hcount;
 timeupdate=1;        // zeitliche Aufgaben nach Zeitaenderung neu pruefen
 return (1);
}

const char * time_source_text (void)   // Klartext der aktuellen Zeitquelle
{
 switch (g_timesrc)
 {
  case SRC_MQTT:   return ("Backend");
  case SRC_GPS:    return ("GPS");
  case SRC_LTE:    return ("Netz");
  case SRC_MANUAL: return ("manuell");
  default:         return ("ungestellt");
 }
}

void clock_mark_manual (void)   // manuelle Datums-/Zeiteingabe als Quelle vermerken
{
 if (g_timesrc!=SRC_MANUAL) { protocol(EV_TIME_MANUAL); g_timesrc=SRC_MANUAL; }
 g_timesync_h=fp.hcount;
}

void date_time (void)	// ASCII Zeit und Datumsstring lesen oder ausgeben
{
 int c;				// Empfangszeichen
  
 c=getc(MODEM_CHAR_TIMEOUT);		// warte auf Antwort

 if (c<0)		// Timeout?
 {
	newline(); 
  read_date_time (TIME, cbuf);				// Zeit in Puffer schreiben   
  putc(';');													// Trennzeichen
  read_date_time (DATE_SHORT, cbuf);	// Datum in Puffer schreiben   
  putc(';');													// Trennzeichen
  putc(DOW|0x30);				// Wochentag senden
 }
 else // Kein Timeout, weitere Zeichen empfangen
 {
  cind=0;
  do								   // Zeit-/Datumsinformation empfangen
  {
   cbuf[cind++]=c;						// Empfangszeichen in Puffer 
   c=getc(MODEM_CHAR_TIMEOUT);			// Zeichen empfangen
  } while ((cind<=17)&&(c>=0));			// bis Zeit-/Datumsinformation vollständig oder Timeout
  if (cind==18)							// Zeit-/Datumsinformation vollständig?
  {
   if ((c>='0') && (c<='6'))	// Wochentag prüfen 
   {  
	  DOW=c&0x07;																// Wochentag in LPC1766 setzen
	  if (!Zeit_falsch (cbuf))	// Zeit prüfen	 
	  {
	   set_time(cbuf);					// Zeit setzen
     if (!Datum_falsch(cbuf+9)) set_date(cbuf+9);	// Datum prüfen	und setzen     
    }
   }
  } // end Zeit-/Datumsinformation vollständig
 }	
 newline();	// Neue Zeile
}

uchar test_sommerzeit (uchar setzen) // Prüfe auf Sommerzeit oder Winterzeit bei Datums und Zeiteingabe,
{                              		 // korrigiere Sommer/Winterzeitbit bei automatischer Zeitumstellung
                               		 // Übergabe setzen=0 nur prüfen, setzen=1 Sommer/Winterzeitflag umstellen 
 uchar sommerzeit=0;
 uchar monat=MONTH,	wtag=DOW, tag=DOM, stunde=HOUR; // Zeitwerte
 
 if (fp.s_w_zeit)								// Sommer-/Winterzeitumstellung aktiviert?
 {	 
  if ((monat>=4)&&(monat<=9)) sommerzeit=1; 	// Sommerzeitbit Apr bis Sep setzen
  else if ((monat==3)||(monat==10))					// März und Oktober
  { if (tag-wtag<25) {        								// letzter Sonntag im Monat nicht erreicht
     if (monat==10) sommerzeit=1; }					// Oktober Sommerzeit 			
    else 						// letzter Sonntag im Monat erreicht oder überschritten
    { if (wtag==0) {													// Heute letzter Sonntag?            
       if (monat==3)	{												// März
        if (stunde>=fp.s_w_zeit) sommerzeit=1; } 	// ab 1:00 UTC, letzter So im Mrz Sommerzeit
       else {																			// Oktober
        if (stunde<=fp.s_w_zeit) sommerzeit=1; } 	// bis 1:59 UTC, letzter So im Okt Sommerzeit
      } 
      else 					// letzter Sonntag überschritten
      { if (monat==3) sommerzeit=1; }				// März -> Sommerzeit  	 
    }     
  } // Ende März und Oktober
 } // end if Sommer-/Winterzeit aktiv	
  
 if (setzen)                  // Setzen und Sommer-/Winterzeitumstellung gesetzt
 { if (sommerzeit) fp.sz=1;									// Sommerzeit setzen
   else fp.sz=0; }            							// dto. löschen */
 return (sommerzeit);  	   							                               
}

void zeitumstellung (void)		// Prüfe und führe Sommer-Winterzeit Umstellung durch
{
 uchar stunde;
 uchar old_sz=fp.sz;			// Sommerzeitflag sichern

 stunde=HOUR; 						// Stundenwert aus interner LP2132 RTC

 if (fp.s_w_zeit)					// Zeitumstellung eingeschalten?
 {	 
  if (test_sommerzeit(0)!=fp.sz) 	// Prüfe ob Zeit umzustellen ist?
  {
   if (fp.sz)						// Sommerzeit auf Winterzeit umstellen?
   {
    if (stunde!=0) 					// Uhrzeit nicht in der ersten Stunde zurück stellen
    { stunde--; fp.sz=0; protocol(WINTERZEIT);	// Uhr eine Stunde zurück stellen, Winterzeit eintragen
			fp.utc--; transferzeit=60; }							// Zeitzonenoffset-1 und setze Prüfsperrzeit 60 Minuten
   }   
   else 								// Winterzeit auf Sommerzeit umstellen
   {
    if (stunde<23)    					// Zeit vor 23 Uhr, sonst verschieben
    { stunde++; fp.sz=1; protocol(SOMMERZEIT);  // Uhr eine Stunde vor stellen, Sommerzeit eintragen
		  fp.utc++; }				// Zeitzonenoffset+1
   }
   if (old_sz!=fp.sz) HOUR=stunde;	// Sommerzeitflag geändert
  }
 }	else fp.sz=0;					// Umstellung abgeschaltet, reset evtl. gesetztes Sommerzeitflag
}

uchar Zeitvergleich (uchar fptag, ushort fpein, ushort fpaus, ushort zeitwert, uchar wtag)
{	   												// Prüft ob Ein-/Ausschaltzeit und Tage aktuelle Zeit ist
 int result;								// Übergaben	fptag		-	Zeiger auf Einschalttage
 uchar task=0;							//				fpein/fpaus	-	Zeiger auf Ein-/Ausschaltzeit
														//				zeitwert		- aktueller Minutenzeitwert
														//				wtage				-	aktueller Wochentag

 if (fptag&(1<<wtag)) 					// aktueller Tag ist Messtag?
 {
  result=fpaus-fpein;					// Differenz Auszeit - Einschaltzeit 
  if (!result) return(1);				// Einschaltzeit=Ausschaltzeit   
  if (result>0)							// Ausschaltzeit > Einschaltzeit
  {
   if (zeitwert-fpein>=0) task=1;		// Aktuelle Zeit >= Einschaltzeit
   else task=0;							// Aktuelle Zeit < Einschaltzeit
   if (zeitwert-fpaus>=0) task=0;		// Aktuelle Zeit >= Ausschaltzeit
  }
  else 									// Einschaltzeit > Ausschaltzeit
  {
   if (zeitwert-fpaus>=0) task=0; 		// Aktuelle Zeit >= Ausschaltzeit
   else task=1;							// Aktuelle Zeit < Ausschaltzeit
   if (zeitwert-fpein>=0) task=1;		// Aktuelle Zeit >= Einschaltzeit
  }
 }
 return (task);	  	// Rückgabe 1 wenn Aufgabe zu erledigen
}  

uint minutenwert (char *buf)			 // Pufferzeit hh:mm in Minuten umrechnen
{ return(atoi(cbuf)*60+atoi(cbuf+3)); }	 // Übergabe buf	-	Zeiger auf Puffer

uchar Aufgabenzeit (void)	// Prüfe ob aktuelle Zeit und Tag Zeit für Messung oder GSM aktiv ist
{																									
 uchar wtag=DOW, mtag=DOM, stunde=HOUR, lmin=MIN;	// Zeitwerte	aus interner Uhr laden
 ushort zeitm=stunde*60+lmin;											// Uhrzeit als Minutenwert
 uchar satz;																			// Laufvariable für Parametersätze	
 uchar aufgabe=0;																	// Nicht null wenn Messzeit, GSM oder GPS einzuschalten
	
 if (fp.turnsw && get_turnswitch())		// Drehschalter aktiviert und Position nicht off
 {
  pset=get_turnswitch()-1;				// Parametersatz aus Schalterposition festlegen	
  aufgabe=0x01;										// und ständige Messung
 }
 else															// Sonst alle definierten Parametersätze auf Messzeit prüfen
 {
  for (satz=0;satz<fp.psets;satz++)	// Wenn aktuelle Zeit Messzeit eines Parametersatzes, dann Schleifenende	  	
   if (Zeitvergleich (fp.eintage[satz], fp.tein[satz], fp.taus[satz], zeitm, wtag)) break;						
  if (satz<fp.psets) { pset=satz; aufgabe=MESSTASK; }	// Aktuelle Zeit war Messzeit eines Parametersatzes
 }	 
 
 if (test_battery(0)<LOWBATLIMIT) 								// Unterspannung Batterie?
 {	 
  if (LastErrorEvent!=BATTERYLOW)									// Noch nicht festgestellt?
	 protocol (BATTERYLOW);													// Protokollieren und ggf. Mail und SMS-Versand
 }	
 else if ((timeupdate==0)&&((stunde==3)&&(lmin==15))) aufgabe|=BOOTTASK;		// Reboot jede Nacht 3:15
 //else if ((timeupdate==0)&&((lmin==0)|(lmin==15)|(lmin==30)|(lmin==45))) aufgabe|=BOOTTASK;		// Test Reboot jede 15 Minuten
 
 timeupdate=0;																		// reset Flag Zeitaufgabe
 
 if (interfaces&GSM_LINK)										// GSM installiert und erkannt
 { 
	if (fp.servertyp==SMTP) 									// viasis mit Email Versand?
  {
	 if (LastErrorEvent!=MEMFULL95) 						// Letztes Fehlerereignis nicht Speicher 95% Meldung
    if (messdaten(1) >= 95)										// Speicher 95% voll?
    {
     if (fp.mailmode==1) mailtosend|=DATAMAIL;			// Email Mode Speicher voll Versand?
     else if (!(mailtosend&MEM95))									// Speicher 95% voll noch nicht vermerkt?
     {																							// wenn kein automatischer Versand
      mailtosend|=(ALARMMAIL|MEM95);					// Email Fehlermeldung senden
		  if (fp.smsalarm) smstosend|=MEM95;			// SMS Speicher 95% voll senden
		  LastErrorEvent=MEMFULL95;								// Einmalige Nachricht aber kein Protokolleintrag
     }
    } 
   if (fp.mailmode>=2)								// Täglicher, wöchentlich und monatlicher Versand
   {
    if (fp.temail==zeitm)													// Sendezeit Daten erreicht?
    { 
   	 if (fp.mailmode==2) mailtosend|=DATAMAIL;			// Täglicher Versand?
		 else if (fp.mailmode==3) 											// Wochenversand?
		 { if (wtag==fp.ewtag) mailtosend|=DATAMAIL; }	// Wochentag ist Versandtag?
		 else if (fp.mailmode==4)												// Monatsversand?
		 { if (mtag==fp.emtag) mailtosend|=DATAMAIL; }	// Monatstag ist Versandtag?
    } // end Sendezeit erreicht
   }	// end Tages-, Wochen- und Monatsversand
	
   if (!fp.mailalarm) mailtosend&=~(ALARMMAIL|FULMEM|MEM95|LOWBAT);	// Email-Alarm aus? Versand löschen und Fehler löschen
   if (!fp.smsalarm) smstosend=0;																		// SMS-Alarm aus? Ja, SMS Versand und Fehler löschen
	 if ((mailtosend&(ALARMMAIL|TESTMAIL|DATAMAIL)) || smstosend)			// Email oder SMS-Versand
		aufgabe|=GSMTASK; 														// GSM Modem einschalten
  }	// end if SMTP Mailversand
	else if (!(connect&MQTT_LINK))		// MQTT Server Betrieb und offline
	{	
	 if (!fp.smsalarm) smstosend=0;																		// SMS-Alarm aus? Ja, SMS Versand und Fehler löschen
	 else if (smstosend) aufgabe|=GSMTASK; 														// GSM Modem ist einzuschalten 		
	 if ((fp.mqtt_packetsize)&&(mqtt_md_bytes_to_send()>fp.mqtt_packetsize*BLOCKSIZE)) // Messdaten zu senden?
	 { aufgabe|=GSMTASK; 													  // GSM Modem ist einzuschalten
		 mqtt_manual=1;																// ohne automatischen mqtt_state inkrement
   }	 	 
  } 
	
  if (Zeitvergleich (fp.gsmtage,fp.tgsmein,fp.tgsmaus,zeitm, wtag)) 					// GSM-Einschaltzeit?
	 aufgabe|=GSMTASK; 																													// GSM Modem ist einzuschalten 
 } // end GSM installiert
 
 if (interfaces&GPS_LINK)													// GPS Modul registriert?
  if (gpsintv)	 																	// Positionsbestimmung aktiviert?
 	 if (gpsfix/60>=gpsintv) aufgabe|=GPSTASK; 			// Zeit für GPS Positionsbestimmung? 

 return (aufgabe);		// Rückgabe Bit0=1 wenn Messzeit, Bit1=1 wenn GSM Einschaltzeit, Bit2=1 wenn GPS Positionsbestimmung	
}	







