//-----------------------------------------------------------------------------
//  FILE: libtool.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Library Routinen	und diverse tools
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
//									13.07.2015: Hardware version H2 -> H3
//              	
//-----------------------------------------------------------------------------

#include "hard.h"
#include "sio.h"
#include "sicom.h"
#include "gsmio.h"
#include "gpsio.h"
#include "btio.h"
#include "rtc.h"
#include "i2cm.h"
#include "flash.h"
#include "ramcode.h"
#include "xmodem.h"
#include "sictxt.h"
#include "libtool.h"
#include "string.h"
#include "sictst.h"
#include "mqtt.h"
#include "dtc_codes.h"

const ushort crc_table[PAGE] = {
0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0};

bool isalpha (char x)					// prüft auf alphanumerische Zeichen 				
{
 if (x>=' ') if (x<='~') return (TRUE);
 return (FALSE);	 
}

bool isdigit (char x)					// prüft auf alphanumerische Zeichen 				
{
 if (x>='0') if (x<='9') return (TRUE);
 return (FALSE);	 
}

char to_upper (char c)				// character to upper case
{
 if ((c>=0x61)&&(c<=0x7A)) return (c-0x20);
	else return (c);
}	

uint atoi(char *str)					// Ascii String in Puffer to Integer Zahl
{
	uint num = 0;

	while(isdigit(*str))
	{
		num *= 10;
		num += (*str++ & 0x0F);
	}
	return (num);				// Rückgabe Zahl
}

void strcopy (text *s, char *t)	// Nullterminierte Zeichenkette kopieren
{																// 	Übergabe 	*s	-	Zeiger auf Zeichenkette Quelle
 while (*s) {*t=*s++; t++;}			//			 			*t - 	Zeiger auf Zeichenkette Ziel
 *t=0;		// Ziel Terminierung
}

void clean_cpy (text *s, char *t, uint8_t anz) 	// Nullterminierte Zeichenkette kopieren und restl. Zielarray löschen
{																								// 	Übergabe 	*s	-	Zeiger auf Zeichenkette Quelle
																								//			 			*t  - Zeiger auf Zeichenkette Ziel
																								//						anz - Länge Zielarray
 while (*s) {*t=*s++; t++; anz--;}							// Zeichen kopieren
 while (anz--) *t++=0;													// Terminieren und Restarray nullen
}	

void copy_to_cbuf (char *s, uint len)	// Zeichen nach cbuf kopieren
{																			// Übergabe *s	-	Zeiger auf Zeichenkette
 while (len--) { cbuf[cind++]=*s++; }
}

void str_to_cbuf (char *s)			// Nullterminierte Zeichenkette nach cbuf	
{																// Pufferindex Variable als inkrementierter Index	
 while (*s)	cbuf[cind++]=*s++;
}

ushort strlen_to_cbuf (char *s)				// Länge Zeichenkette ermitteln und nach cbuf
{																			// Übergabe *s	-	Zeiger auf Zeichenkette
 ushort len=0;								// Hilfsvariable Länge
 while (*s++) len++;					// Solange Zeichen nicht Null Länge inkrementieren
 cbuf[cind++]=len>>8;					// MSB schreiben
 cbuf[cind++]=len;						// LSB schreiben	
 return(len);									// Rückgabe Länge der Zeichenkette 	
}

/* void cstrcpy (text * s, char *t, uint len)	// Zeichenkette der Länge len kopieren
{																							// Übergabe *s	-	source pointer
																							// 					*t	-	target pointer
 while (len--) { *t=*s++; t++; }					
} */

uint16_t crcgen (uint8_t *Puffer, uint16_t anzahl, uint16_t oldcrc)	// Bilde CRC-16 mit oldcrc init
{                      																							// Übergabe: Puffer - Pufferadresse
																																		//			 			Anzahl - Anzahl Bytes 
																																		//						oldcrc - crc register init
// 						Crc-Parameter:   	Width 16 Bit, Polynom: 1021H
//						Register-Init: oldcrc Übergabeparameter
//						Reflection-In: false, Reflection-Out: false
//						XorOut: 0000, 	Check: 31C3h
                                       
  uint16_t newcrc=oldcrc;                  	// crc register init	   
  uint16_t ind;                          		// index in buffer         
	 
  for ( ind=0; ind < anzahl; ind++) 
  { 
   newcrc = (newcrc << 8) ^ crc_table[(newcrc>>8) ^ *(Puffer+ind)];
  }
 return (newcrc);                     // Rückgabe neuer CRC
}

uint16_t crc (uint8_t *Puffer, uint16_t anzahl)	{ return(crcgen (Puffer, anzahl, 0)); } // CRC-16 mit CRC Register init null
 
void DirSort (uchar n)			// Sortiert Periodendauern im Puffer 
{                              	// mit relativ konstanter Laufzeit 
 uchar k, i, minindex;
 uint min, swap;

 for (k=0; k<n-1;k++)
 {                            	// Minimum von T[1], ... , T[n-1] bestimmen 		
  min=tm.IF[k]; 
  minindex=k;
  for (i=k; i<n;i++)
   if (tm.IF[i]<min) 
   { min=tm.IF[i]; 
     minindex=i;
   }  
  swap=tm.IF[k]; 					// Minimum an Position k speichern 
  tm.IF[k]=tm.IF[minindex]; 
  tm.IF[minindex]=swap;
 }
}


int number_exists (int nummer, const ushort *liste, uchar len) // Prüfe Fehler/Ereignis Liste auf Nummerneintrag
{									  	// Übergabe	nummer - zu suchende Nummer
											//			liste	- Startadresse Liste
											//			len		- Listenlänge
 uchar i;							// Laufvariable
 if (!nummer) return(-1);									// Null unzulässig
 for (i=0;i<len;i++)											// Alle Einträge in Tabelle prüfen
  if (nummer==*(liste+i)) return (i); 		// Existiert, dann Rückgabe Tabellenelement
 return(-1);	// Eintrag existiert nicht
}

void num_to_LED (uint wert, int farbe)	// Numerischen Wert in 7 Segment Anzeigesteuerwert wandeln
{															// Übergaben:	farbe - 1/2/3 Grund/Warn oder Mischfarbe  	
															//						wert - numerischer Anzeigewert (< 500) oder Fontnummer
															// Nummernkreise wert: 	0..199 Numerische Ziffern in Grund oder Warfarbe
															//											200..399 Numerische Ziffern mit Dezimalstelle
															//											400..499 Symbolziffern im roten Kreis
															//											500 .. Sondersymbole
 const char seg7[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};	// 7 Segment Zifferncodes

 uint zahl=wert;
 uint code7=0;
 uchar i=5,digit;				// Hilfsvariablen
 
 farbcode=0;					// Versorgungen power enable 1 bis 5
 
 if (wert<400)																		// Ziffern Grund-/Warnfarbe oder Mischfarbe
 { 	 			
	if (farbe&0x02) farbcode=COL2;									// Warnfarbe
	if (farbe&0x01) farbcode|=COL1;									// Grundfarbe
 }	 
 else																							// Kreissymbole
 {	
	if (PINSEL4&PWM6)																// Anzeige ist an?
   if (ledbuf[0]&COL1)														// bisherige Anzeige mit Grundfarbe (grün)?
	 { 	
		 PINSEL4&=~PWM6; 															// Regler PWM aus
		 NVIC_DisableIRQ(PWM1_IRQn);									// Disable PWM1 Interrupt
		 if ((ledbuf[0]&(COL1|COL2))==(COL1|COL2))		// zweifarbig?
			discharge=DISCHRG; 													// Setze discharge Bit
		 else 
		 {
			ledbuf[0]|=DISCHRG;			 
			Init_SPI();																	// SPI initialisieren	
			while(i--)	sendspi	(ledbuf[i]);						// Daten über SPI versenden
      FIO2SET = LE_TRB;														// Latch Schiebedaten 74HC595 
			while ((FIO2PIN & LE_TRB) == 0);						// Warte bis gesetzt 
			FIO2CLR = LE_TRB;														// Latch reset -> LED Treiber MBI5039 Datenübernahme			 
			PCONP&=~PCSSP1;	 														// SSP1 power disable
		  PINSEL0&=~(MOSI|SCK|MISO);									// Deselect SSP1 Pinfunktionen 
		 }	
	   osDelay(50); 	  													// Schaltregler aus 50 ms warten
		 PWMMCR&=~PWMMR0I;													// Lösche Interrupt bei vorausgehender Mischfarbdarstellung
		 NVIC_EnableIRQ(PWM1_IRQn);									// Ensable PWM1 Interrupt
		 discharge=0; 															// Reset discharge Bit	
	 } // end vorherige Anzeige mit Grundfarbe
	 
  if (wert<500) farbcode= COL3 | COL4;						// weiße Ziffernsymbole, roter Kreis
  else if (wert<504)															// Sonderzeichen -  Kreissymbole
  {
	 if (wert<502) farbcode= COL4;									// übrige rote Kreissymbole
	 else if (wert<504)	farbcode=COL5; 							// grüne Kreissymbole
	 if (wert&0x01) code7=0x0FC00000;								// Smileys		
	 else code7=0xF3C00000;  												// Haken und Ausrufezeichen
  }
 }  
 
 if (wert<504) ledbuf[0]=farbcode;							// Anzeigefarbe und Clusterspg.-bit setzen
 else ledbuf[0]=0;						// illegaler Wert abschalten
 
 if (wert<500)								// Keine Sondersymbole
 {	 
	for (i=0;i<2;i++)						// Einer und Zehnerstelle
	{
	 digit=zahl%10;									// Ziffer
	 code7|=(seg7[digit]<<(7*i)); 	// Zifferncode
   zahl/=10;											// Letzte Ziffer entfernen
	 if ((farbcode&(COL1|COL2))!=0)		// Grund- oder Warnfarbe
	 {	
		switch (digit)								// Ziffernergänzungen	
		{	 
		 case 0: {										// Ziffer = 0
							if (i)							// Zehnerstelle			 
							{							
							 if ((zahl&0x01)||((zahl>=2)&&(zahl<4))) code7|=(1<<29); // bei Wert>100 oder DP ergänzen
							 else code7&=~(0x7F<<7); // sonst führende Null auf Zehnerstelle entfernen		
							}	
							else code7|=(1<<28);		// Einerstelle ergänzen
							break; } 
		case 1: 
		//case 4:	 													// 4 mit langem, rechteckigen, rechten Strich - in Grundfarbe nicht schön				 
		case 7:	 
							{												// Ziffer = 7
							if (i) code7|=(3<<19);	// Zehnerstelle
							else code7|=(3<<17);		// Einerstelle
							break; }
		 					
		}	 
	 } // end if Grund- oder Warnfarbe
   else if ((i==1) && (digit==0)) code7&=~(0x7F<<7); // Symbolziffern führende Null auf Zehnerstelle entfernen	 
	} // end for  
  if (zahl&0x01) code7|=(0x20C0<<8);	// Hunderter Stelle? Ja -> Hund. Segmente mit Ergänzung
	if (zahl>=2) 
	{	
	 if (zahl<4) code7|=(1<<16);				// Dezimalpunkt
	 else code7|=(0x0F<<22);						// Symbolziffern 400-499 mit rotem Kreis	
	}		
 }	// end kein Sonderzeichen 
 
 memcpy(ledbuf+1,&code7,sizeof(code7));	// 7 Segment Steuercode nach Anzeigepuffer kopieren
}	

void get_dimmung (void)		// Umgebungshelligkeit messen und Dimmfaktor bestimmen
{
 uchar spot_dim;										// Dimmung für LED Spot					 
 uint bright=get_brightness ();			// Umgebungshelligkeit messen
 uint sensor=(200000+bright*bright-bright*bright*bright/1515-525*bright)/1000; 
 dimm+=(sensor>>2)-(dimm>>2);				// Expo. Glättung
 if (fp.ledspot)			   						// LED Warnleuchte?
 {
  spot_dim=(uint)(dimm*15/200);	// Dimmfaktor 1...200 auf TCA6507 Dimmung 1...15 umrechnen
  if (spot_dim==0) spot_dim=1;	// kleinste Dimmstufe
  write_i2c_dev (ICLED,2,(spot_dim<<12)|(spot_dim<<8)|0x18);	// Max. Intensität setzen
 }     
}

int show_led (int time, uchar hell)		// LED Anzeige bis Zeitablauf oder Eingabe
{								 											// Übergaben:	time	-	Dauer in ms
																			//						hell	-	Helligkeit 0 = variabel oder 1...200 * 1/2%
 // Hinweis: Inhalt der LED Anzeigepuffer und Anzeigefarbe muss vor Aufruf festgelegt sein												
	 
 if (hell!=0xff) set_pwm (hell,hell);		// Setze externe und interne LED PWM
 shift_to_LED (T_nil);									// Anzeigepuffer ausgeben	und Versorgung ein
 
 if (time>=0) 										// time>=0 warten bis zeicheneingabe oder Zeitabbruch
 {
	bxi=rxi;												// reset evtl. vorhandene Pufferzeichen  
	while (bxi==rxi)								// Warte auf Zeichen
  {
	 ResetWDT(); 										// Watchdog reset	
	 osDelay(1);										// Task Wechsel	zulassen
	 if (time) 
	 {
    time--;		 
		if (!time) break;							// Zeitabbruch
	 }	 	
	 else if (comchange) break;			// wenn time=0 Abbruch bei Schnittstellenwechsel
  } 		
  if (bxi!=rxi) return(rbuf[++bxi]);	// Zeichen zurück
  return(-1);
 }
 return (0);		// time <0 Anzeige starten und nicht warten
}

int show_led_off (int time, uchar hell) 	// LED Anzeige bis Zeitablauf oder Eingabe
{													  							// Übergabe siehe show_led
																					// Funktion wie show_led aber Anzeige wird im
 int c;											  						// Anschluss abgeschaltet
 c=show_led (time, hell);		
 Ledaus();					// Anzeige abschalten
 return (c);
}

void LED_188 (void)		// 188 LED power on, Batterie- und Echtzeituhr Prüfung
{
 uchar col;				// Laufvariable
 uint i;
 uint wert=0;
 uint dauer=500;		// Anzeigedauer	insgesamt 8 x dauer

 if (fp.farben<2) dauer=1000;	// Anzeigedauer bei einfarbiger Anzeige verdoppeln

 for (col=1;col<3;col++) 			// Beide LED Farben anzeigen
 {
  num_to_LED(188,col);			  // 188 nach Anzeigepuffer
  if (col>fp.farben) break;
  for (i=1;i<=128;i*=2)				// 2, 4, 8, 16, 32, .. Tastverhältnis in 1/2% Schritten 
	 {
    show_led (dauer,i);				// Tastverhältnis Dimmung von 1/2% bis 64%	   	
	 }	 
  if (col==1)
  {
	 show_led (-1,RES_PWM);				// Tastverhältnis 100%	
   wert=test_battery (0)/100;		// Batterietest	ohne Meldung
	 osDelay (dauer);	   
  }
  else show_led (dauer,RES_PWM);	// Tastverhältnis 100%   
	Ledaus();												// Anzeige abschalten 
	set_pwm (1,1);									// auf kleinstes Tastverhältnis zurück
  osDelay (dauer);
 } 
 
 wert+=fp.uoff;									// Batteriespannungsoffset addieren
 num_to_LED(wert+200,1);				// Kommastelle hinzufügen
 show_led  (-1,2*fp.hell[0]);		// Wert an Led-Schieberegister
 test_rtc (1);									// Test des Echtzeituhr Oszillators */
}

uint ledsymbol (uchar speed)	// Prüft Anzeigeschaltschwellen der LED Anzeigesymbole
{								// Übergabe:	speed - gemessene Geschwindigkeit
 uchar i;						// Laufvariable Symbole

 for (i=0;i<fp.symbol;i++)		// Schaltschwellen aller definierten Symbole prüfen
 {
  if (speed>=fp.vsym[pset][2*i])  	// Geschwindigkeit >= Einschaltschwelle
   if (speed<fp.vsym[pset][2*i+1])	// Geschwindigkeit < Ausschaltschwelle
    return (fp.symfont[i]);			// Rückgabe Symbolfont  					
 }
 return (0);	// kein Symbol anzuzeigen
}

int transceiver (uchar mode)	// Transceiver Ein-/Ausschaltung
{									// Übergabe mode 	= 0	Transceiver aus
									//					= 1 Transceiver und Verstärker ein
									// 					= 2 Zyklischer Messbetrieb
									//					>= 3 Testmodus - wartet jede Stufe
	
 ushort analog=(fp.txrcon&~(GAIN_INH))|(ANT_EN2|ANT_EN1|HF_EN);	// MUX Verstärkungsreduzierung
 									
 if (mode)		// Einschalten?
 {
	FIO1CLR=LED1;					// Status LED ein 
  if (mode>1)
  { 
   if (mode==3) {putparameter(T_ic37, analog&~(GAIN_A|GAIN_B), (0x05<<16)|0x84, T_nil); getc (0); }   
   analog|=HF_EN|AMP_EN|GAIN_A|GAIN_B|BW_A;		// Transceiver disabled ein, Verstärkung minimal
   write_i2c_dev (IC37,3,(analog<<8)|0x02);  			
   if (mode==3) {putparameter(T_ic37, analog, (0x05<<16)|0x84, T_nil); getc (0); } 
   else getkey(DPWR);
   analog&=~(ANT_EN2|ANT_EN1);								// Transceiver enable	
   write_i2c_dev (IC37,3,(analog<<8)|0x02);
   if (mode==3) {putparameter(T_ic37, analog, (0x05<<16)|0x84, T_nil); getc (0); } 
   else getkey(DTXFEN);
  }  // end if zykl. Betrieb oder Testmodus
  analog&=~(ANT_EN2|ANT_EN1|GAIN_A|GAIN_B);			// Transceiver enable und Minimal-Verstärkung reset
  analog|=HF_EN|AMP_EN|AMP_BW|BW_A|(fp.txrcon&(GAIN_INH|GAIN_A|GAIN_B));	// 5V/3V3 ein, volle Bandbreite, eingestellte Verstärkung übernehmen 0x05E0
  if (!write_i2c_dev (IC37,3,(analog<<8)|0x02)) return (-1);	   // Kein Erfolg Fehlerabbruch   
  if (mode==3) {putparameter(T_ic37, analog, (0x05<<16)|0x84, T_nil); getc (0); } 
  else getkey(t_amp_en);
 } // end if mode 
 else 
 {	 
	FIO1SET=LED1;					// Status LED aus 
	if (!write_i2c_dev (IC37,3,((analog&~(GAIN_A|GAIN_B))<<8)|0x02)) return (-1); // Transceiver Ausschalten   	
 }	 
  	   		
 return (1);	// Ergebnis ok
}

void set_measure_constants	(void)	// Konstanten für Messung anhand Parametereinstellung festlegen
{
 IFConst=fp.TxF;												// Transceiver Sendefrequenz 24000 ... 24250 MHz laden
 IFConst*=100000;
 IFConst/=IFDIV;													// 100 fache Dopplerkonstante (~4470 Hz/kmh)
 v_const=(100ul*CCLK)/(TDIV*IFConst/10);	// Konstante Geschwindigkeitsberechnung abhängig von Capture Zählerfrequenz
 
 if (fp.mph) 		// kmh in mph Umrechnung
 {
  IFConst=1000*IFConst/MILE;   
  v_const=1000*v_const/MILE;
 }

 v_const=v_const*fp.vcor/100;		// Geschwindigkeitskorrekturfaktor berücksichtigen
 t_mess_max=DPWR+DTXFEN+t_amp_en+(100000*(LenTm+2)/(fp.vmm*IFConst));		// Max. Messzeit abhängig von kleinster Messgeschwindigkeit	  
}

uint display_mode_speed (uchar psatz)	// Prüfe Schwellen der Modi der Led Anzeige
{										// Übergaben psatz - zu verwendender Parametersatz

 uchar speed=speed10/10;		// Messgeschwindigkeit oder simulierte Geschwindigkeit
 uint aspeed=0;					// Anzuzeigender Geschwindigkeitswert (für Fontdekodierung)	

 if (speed>=fp.vmin[psatz] && speed<=fp.vmax[psatz] && (fp.vmin[psatz]!=fp.vmax[psatz])) // LED Ausgabe?
 {
  aspeed=speed;		// Anzeige ist gemessener Geschwindigkeitswert
  if (fp.dmode)		// Differentieller Anzeigemodus
  {		 
   if (speed>=(fp.vlim[psatz]+fp.voff))	// Tempolimit und Anzeigeoffset überschritten?
    aspeed=speed-fp.vlim[psatz];   		// Differenzgeschwindigkeit bilden
   else aspeed=0;		 		 
  }
  else 				// Absoluter Anzeigemodus
  {
   if (fp.vlim[psatz]!=Defmaxspeed+1)				// Tempolimit Anzeige aktiv?
   aspeed=fp.vlim[psatz];			 							// Anzeige ist Tempolimit 
   else if (fp.nk&&(speed10<=Defmaxspeed))	// Nachkommastelle und < 19.9
   aspeed=speed10+200;											// Anzeige ist Kommawert 	 
  }		
 } // end LED Ausgabe, vmin, vmax
 return (aspeed);
}

int farbe_numLED (uchar speed)		// Prüfe Schwellen für LED Farben der numerischen Anzeige
{																	// Modfizierte Sondersoftware Anzeig Grundfarbe und Mischfarbe getauscht
 uchar farbe=1;						// Anzeigefarbe ist zunächst Grundfarbe
	
 if (fp.farben>=2)						// 2. Anzeigefarbe definiert
 {	  
  if (fp.farben==3)					 // Mischfarbe zugelassen
   if (speed>=fp.vmix[pset]) farbe=3;	// Geschwindigkeit >= Schwelle Mischfarbe -> Grundfarbe anzeigen
  if (speed>=fp.vcol[pset]) farbe=2;	// Geschwindigkeit >= Schwelle Farbe 2
 }
 return (farbe);
}

void make_filename (void)		// Erstellt VTF Dateiname in cbuf
{
 cind=0;
 redirect_char_Out(putcbuf);				// Zeichenausgabe in Puffer cbuf leiten
 putstr(T_sc);											// Beginn Dateiname "SC_" nach Puffer 
 if (isalpha(*fp.serno)) 						// Seriennummer definiert? 
	{ putstr(fp.serno); putc('_'); } 	// Seriennummer des Geräts ausgeben, Trennzeichen
 read_date_time (DATE_NODEL, cbuf+cind);	// Datum TTMMJJ nach Puffer
 putc('_');																// Trennzeichen Dateinamenserweiterung
 putnumber (fp.fileno,0xC3);							// Dateinummer 3 stellig ausgeben
 putc('.');																// Trennzeichen Datenamenserweiterung
 putstr(T_vtf);														// Erweiterung "vtf"
 putc('\0');															// Dateinamen abschliessen
}

int compare (text *v, text *buf, ushort start, ushort stop) // Suche Zeichenkette in Puffer
{ 								   				// Übergaben:	v	-	Zeiger auf Null terminierte Suchzeichenkette
														//				buf	-	Zeiger auf Puffer oder Zeichenkette
														//				start - Startoffset Suche
														//				stop  - Stopoffset Suche
 uchar len=0;   			// Länge übergebene Zeichenkette
 uchar iden=0;      	// Anzahl Uebereinstimmungen
 text *p;							// Hilfszeiger

 p=v;
 while (*p++ != '\0') len++;							// Länge Zeichenkette
 p=v;
 while (start!=stop)           						// bis Empfangs- und Bearbeitungszeiger gleich
 {
  if (*(buf+start++)==*p) {iden++; p++; } // Zeichen gleich
  else { iden=0; p=v; }										// Zeichen ungleich		
  if (iden==len) return (start);					// identische Zeichenkette gefunden
 }
 return (-1);                    					// kein Erfolg
}

bool file_is_firmware (void)		// Prüfen ob empfangene Datei Firmware
{	  
 uint *P_val;						// Pointer auf Interrupt Vektortabelle in Firmware
 uint sum=0;						// Aufaddierte Prüfsumme Vektortabelle
 uchar i;								// Laufvariable 
 	
 flash_pcom((uchar *)cbuf,0,FLASH_R_ARRAY,BLOCKSIZE);	// Erste Seite mit Firmware adressieren und Seite(n) lesen	
 P_val=(uint *)cbuf; 																	// auf Beginn Datenblock/Vektortabelle

#if (NOCRPCHECK == 0)	
 switch (*(P_val+CRP_key/4))													// Code Read Protection (CRP) Schlüssel prüfen
 {				
	 case CRP2:	 
	 case CRP3:				
										break;														// CRP 2 und 3 immer zulässig	 
	 case CRPNONE:
	 case	CRP1:
#if (DEBUGGING == 1)
										break;														// nur zulässig bei Debug Versionen
#endif	 
	 default:					return (false);										// CRP1, nicht definierter oder falscher Schlüssel unzulässig
 }	 
#endif 
 for (i=7;i<=10;i++) if (*(P_val+i)!=0) 							// prüfe ob reserved code words 7 bis 10 null
  return (false); 																		// Fehlerrückgabe
 
 for (i=0;i<7;i++) sum+=*(P_val+i);										// Prüfsumme addieren 
 *(P_val+7)-=sum;																			// Prüfsumme an Vektoradresse 0x0000001C schreiben
 
 addcrctoblock ((uchar *)cbuf, BLOCKSIZE); 						// Fügt neuen CRC an Datenblock in Puffer an
 flash_pcom((uchar *)cbuf,0,FLASH_ERASE_WRITE_1,BLOCKSIZE+2);	// Erste Seite mit Firmware adressieren und Seite(n) schreiben
 return(true);													// Ergebnis wahr bei Erfolg
}

void clear_comchange (void)		// Bereinige Schnittstellenwechsel z. B. nach Modemausschaltung
{
 int state;	
 if (comchange) 										// Ein-/Ausschaltung verursachte Schnittstellenwechsel?
 {
  state=get_com_status();  					// Status serielle Schnittstellen aktualisieren	 
	if (state>=0)														// Baustein gelesen?
  {		
	 if (!osActiveThread) comstate=state;		// Kein Verbindungsthread aktiv - Status aktualisieren
	 else thread_comchange=1;								// comchange merken 	
	}	
 }	 
 comchange=0;												// reset Flag Schnittstellenwechsel 
}	

void modem_terminal (uchar mode)	// Eingabeschnittstelle für Modem Kommunikation
{																	// Übergabe mode - 2 UART2 Empfang an Terminal senden / 46 RN4678 installiert
 int c=0;												// Eingabezeichen	
	
 bxi=rxi;												// Empfangsindizes UART1 gleich setzen
 bx2=rx2;												// Empfangsindizes UART2 gleich setzen
 cind=0;
	
 if (mode==46) { connect|=MQTT_LINK;		// UART1 Empfang in pbuf leiten
 bpi=rpi; }															// pbuf Pufferindizes gleichsetzen 	
	
 do
 {
  ResetWDT();
  if (bxi!=rxi) 					// Zeichenempfang?
  { 
   c=rbuf[++bxi];							// Empfangs- in Arbeitspuffer kopieren
   cbuf[cind++]=c;						// Empfangs- in Arbeitspuffer kopieren, Klein- in Großbuchstaben wandeln	 
   cbuf[cind]=0;							// Zeichenkettenabschluss		
   if (is_CRLF(c)) 
   { 
    if (cbuf[0]=='!')
	  { connect|=UART1; putstr (&cbuf[1]); putb(LF); }
	  else
		{
     if ((cbuf[0]=='A')|(cbuf[0]=='a'))  connect|=UART1;							// AT Modem-Kommando?
		 if ((cbuf[0]=='$')& (mode==2)) connect|=UART2;	// GPS $ Kommando
		 else connect|=UART1;	
     putstr (cbuf);
   	 if (mode==2) putb(LF);	
		}
		cind=0; 
		connect&=~(UART1|UART2);
   } 	 
   if (c==STRW)   														// Abbruch Transparente Verbindung
   {
    connect|=UART1;
	  putstr (T_plus);
	  connect&=~UART1;
   }
  }   
	else if (mode==2)				// UART 2 Empfang an Terminal senden
	{
	 if (rx2!=bx2) putc(gbuf[bx2++]);	// Sende Empfangszeichen an Terminal
	}		
	if (mode==46)
	{
	 if (bpi!=rpi) putb(pbuf[bpi++]);	// UART1 Zeichenempfang ausgeben
  }		
 } while (c!=STRZ);
 
 if (mode==46) connect&=~MQTT_LINK;	// UART1 Empfang wieder auf rbuf
}	

void modem_com (void)						// Direkte Kommunikation mit Modems
{																
 int c=0, cc=0;									// Eingabezeichen
 uint baud=9600;								// Einzustellende Baudrate
 //uchar uart2=0;									// Uart2 Empfang aktivieren	
 uchar mode=0;									// 2=Uart2 Empfang aktivieren, 46=RN4678 installiert	
 int result;	
	
 InitWatchdog(LONG_WD_32);					// langes Watchdog Interval auf 32 Sekunden setzen
 if (connect&UART1) { put2str(T_LF,T_noacc);	return; }	// kein Zugriff über GSM,BT
 concpy=connect;
	
 c=putmenu(T_sbaud,'5');						// Baudrate Abfrage Menü
 if ((c<'1')||(c>'4')) return;			// Abbruch ohne Auswahl
 if (c=='2') baud=115200;  
 else if (c=='3') baud=307200; 
 else if (c=='4') baud=460800;
 
 c=putmenu(T_smodem,'4');						// BT, GSM Auswahl Menü
 if ((c<'1')||(c>'3')) return;			// Abbruch wenn Auswahl nicht GSM, BT  oder GPS
 
 if (c=='1')
 {
  cc=putmenu(T_blue,'3');	 					// Bluetooth Typ auswählen
	if ((cc<'1')||(cc>'2')) return; 
	if (cc=='2') mode=46;							// RN4678 ausgewählt 
	Init_BT_ch (baud);								// Konfiguriere uart1 und setze MUX Kanal auf BT modem												
 }
 else if (c=='2')												// GSM/UMTS/LTE Modem ausgewählt? 
 {
	putln (T_gsmpower);								// Text "Power GSM/UMTS/LTE modem. Wait... 
	if (fp.gsm==0) result = gsm_power (1);	// Kein Modem konfiguriert, dann nur einschalten
	else result = modem_start ();						// Modem konfiguriert, dann Startmeldungen abwarten
	 
	if (result > 0)										// Antwort von Modem?
  {
   if (fp.gsm==0)
   {
		Init_GSM_ch (baud);					// MUX GSM Kanal auswählen und UART1 konfigurieren 
	  if (FIO0PIN&GSM_DTR)				// Modem Sleep enabled?
	  FIO0CLR=GSM_DTR;						// DTR  LO - Disable Modem Sleep 
   }			
	 //clear_comchange ();							// Bereinige Schnittstellenwechsel
	 if (result > 1) wait_message (T_pbdone,5*XMODEM_BLOCK_TIMEOUT); 		// Modem power up -> Warte Ende Einschalt URCs	
	 putstr(T_gsm+1);						
   putc(' ');
   putln(&T_offon[1][0]);						// Einschaltung melden 
	}	
	else 
	{	
	 connect=concpy;										
	 dtcerr(DTC_LIB_GSM_ERR);						// GSM-Fehler in libtool
	 newline();	
	 return;	
	}		 
 }	 
 else if (c=='3')  							// GPS Auswahl
 { 
	gps_power(2);
	Init_UART2 (baud);						// UART2 auf gewählte Baud rate konfigurieren
	mode=2;												// Leere Puffer
 }
 
 if (comchange) clear_comchange();		// Einschaltung von GSM/GPS Versorgung kann scheinbaren Schnittstellenwechsel verursachen
 osDelay (1);	
 
 concpy=connect;					// aktive Ein-Ausgabeschnittstellen merken	
 modem_terminal (mode);	// Eingabeschnittstelle für Modem Kommunikation	 
 if (c=='3') 							// Auswahl war GPS?
  gps_power(0);						// GPS abschalten und deselektieren 	 
 uart1_release(uart1_owner);	// Release UART1 regardless of current owner
 connect=concpy;					// Ausgabe Schnittstellen wieder herstellen
 FIO0SET=GSM_DTR;					// DTR IN HI - Enable Modem Sleep
 osDelay (100);
 newline();
}

void init_turnsw (void)	// Drehschalter Parametersatzwahl initialisieren
{																
 char c;															// Eingabezeichen
 
 fp.turnsw=0x07; 										// Drehschalter installiert sichern
 put2str(T_turn,T_dpkt);							// Text "Turn switch:_"
 putc(SPACE);												// Leerzeichen senden
 do
 {
	delchar (1);												// Lösche letzte Ausgabe
  putnumber (get_turnswitch(),0x01);	// IC54 P0 auslesen	und wandeln	
	c=getc(200);												// warte 200 ms auf Zeichen			
	comchange=0;												// Schalteränderungen löschen
 } while (!is_CRLF(c));							// Ende bei Return Eingabe
 newline(); 	 
}	

void set_default_port (void)	// Ändert default I2C Expander Porteinstellung
{									 	
 ushort * def_port;						// Zeiger auf default Porteinstellung	
 uchar i,l;										// Laufvariable Schalter und Portpinnummer

 fp.defic58=Def_port_ic58;		// Default Voreinstellung laden

 for (i=0;i<NOSW;i++)
 {
  if (fp.swexp[i]==1) def_port=&fp.defic58; // Zeiger auf Expander IC58 Defaulteinstellung
  else continue;														// Kein gültiger oder physikalischer Schalter

  l=fp.swport[i];											// Portpinnummer
  if (l<16) *def_port&=~(1<<l); 			// Default Portmaske ändern
  else *def_port|=(1<<(l-16));
 }
}

void set_exp_switch (uchar switchno)	// Setzt definierte Schalter über I2C Expander
{																			// Übergabe Schalternummer 0 .. 11
 ushort def_port;						// Default Porteinstellung
 uint port;									// Porteinstellung
 uchar devadr;							// I2C Device Adresse
 uchar l;										// Portpinnummer

 if (fp.swexp[switchno]==1) { def_port=fp.defic58; devadr=IC58; }	// IC58 Expander 		
 else if (fp.swexp[switchno]==2) { def_port=0; devadr=ICLED; }			// LED Spot
 else return;
	 
 l=fp.swport[switchno];								// Portpinnummer
 if (l<16) port=def_port|(1<<l); 			// Schalter nicht invertiert, setzen
 else port=def_port&(~(1<<(l-16)));		// Schalter invertiert, rücksetzen

 if (fp.swexp[switchno]==1) write_i2c_dev (devadr,3,((port<<8)|0x02));	// Ausgabe an I2C Expander 
 else if (fp.swexp[switchno]==2)
 {	 
  if (fp.i2cdev&ICLED_b)	 // externer Led Spot
	{	
	 l=port&0x07;													// Korrespondierende Schalterbits ausmaskieren	
   if (l) write_i2c_dev (ICLED,4,((l<<24)|(l<<16)|(l<<8)|0x10));	// Led ansteuern
  }
  else puterror(DTC_LIB_TCA6507, -1);			// Led spot Treiber Fehler ausgeben
 }
 else put2str (T_err, T_notcon);
 
 newline();
}

uchar nmea_checksum (text *s)		// XOR Prüfsumme GPS NMEA Nachricht bis * Abschlusszeichen
{																// Übergabe: Zeiger auf erstes Zeichen hinter $
 uchar checksum=*s++;

 while ((*s!='*')&&(*s)) checksum^=*s++;	// Bilde Prüfsumme bis * oder Null Terminierung
 return (checksum);											
}

uint colonpos(text *s, uchar anz)		// Ermittle Byteposition hinter anz.ter Kommastelle in Zeichenkette
{																		// Übergaben *s - Zeiger auf null terminierten Puffer oder String
																		//					anz - Anzahl Kommas
 uint pos=0;									// Anzahl geprüfter Bytes
	
 if (!anz) return(0);					// nulltes Komma ist illegal				
 while (*s) 									// Nullterminierung? 
 { 
  if (*s++==',') anz--;				// Zeichen ist Komma? Ja, dekrementiere Kommaanzahl
   pos++;											// Inkrementiere Pufferzeiger und geprüfte Byteposition	
  if (!anz) return(pos); 			// verlangte Kommazahl erreicht, Rückgabe Byteposition hinter Kommastelle
 }
 return (0);									// Kein Komma bis Nullterminierung			
}

uchar no_at (text *s)					// Sucht @ Zeichen in Zeichenkette
{															// Übergabe *s - Zeiger auf Zeichenkette
 if (!*s++) return (0);				// Nullstring 
	
 while (*s)										// Ab 2. Zeichen 
 {
	if (*s++=='@') return(0);  	// at gefunden
 }	 
 return (1);									// kein @ Zeichen ab 2. Stelle im String		
}	

void Helligkeit (void)			// Einstellung der LED Dimmung
{
 int c;								// Zeichen von Terminal
 uchar i;  							// Laufvariable Farben
 uchar hell;						// Helligkeitswert
 uchar max=MAXTAST+1;		// Schleifenendwert	
 uchar maxwert=100;			// Einstellbarer Maximalwert Helligkeit/Farbbalance
 uchar *fpp;						// Zeiger auf Helligkeit/Tastverhältnis im Parameterblock	

 putstr(T_bright);		// Text "Brightness
 	
 for (i=0;i<max;i++)								// für alle Anzeigefarben	mit eigenem Tastverhältnis
 {
  newline();
	if (i!=MAXTAST)										
	{		
   putstr(T_color+2);							// Text "Color
   putnumber(i+1,1);							// Farbnummer  
	}	
	else 
	{
	 putstr(T_expwm);								// Externe PWM
	 if (fp.ex12==3) 	 							// Festtext für Plus FT einschalten XXX für Plus fehlt
	 {
		write_i2c_dev (IC58,3,(((fp.defic58|(0x0F<<1))<<8)|0x02));	// Alle Texte ein - Ausgabe an Expander IC58 
		FIO0SET = EX12_2; 						// 12V extern Versorgung einschalten
	 }		
  }		
  put2str(T_inprz,T_dpkt);				// Text "in %: "
  
  while (1) 
  {
   if (i<MAXTAST) { hell=fp.hell[i]; fpp=&fp.hell[i]; } 
	 else { hell=fp.pwmex; fpp=&fp.pwmex; }  				
   putnumber(hell,3);													// Helligkeitswert ausgeben
	 if (i<2) num_to_LED (hell,i+1);						// Grund-/Warnfarbe Wert in LED Anzeigewert wandeln
   else if (i==2) num_to_LED (400+hell,0);		// Symbolziffern weiß im roten Kreis
   else if (i==3) num_to_LED (503,0);					// Grüner Smiley		
	 else if (i==MAXTAST) num_to_LED (hell,1);	// Tastverhältnis
	 
   if (i!=MAXTAST) c=show_led (0,2*hell-1);		// LED Puffer anzeigen und Eingabe abwarten   
	 else 																			// Abgleich externe LED		
	 {	 
		set_pwm (2*fp.hell[0]-1,2*fp.pwmex-1);		// Setze externe und interne LED PWM  
		shift_to_LED (T_nil);											// Anzeigepuffer ausgeben	und Versorgung ein 
		
		c=getc(0);																// Warte auf Eingabe 
	 }	 
   if (is_CRLF(c)) break;										// Abbruch
   if (c=='+') { if (hell<maxwert) *fpp=hell+1; }	
   if (c=='-') { if (hell>1) *fpp=hell-1; }
   if (c==ESC) { if ((fp.ex12==2)||(fp.ex12==4)) plus_com(0); }			// ins Matrixmenü 
   delchar(3);								// Letzte Ausgabe löschen      
  } // end while
 } // end für jede Farbe

 clear_all ();									// Led Abschaltung
 newline();
}

int plus_com (uchar mode)		// Kommunikation mit Matrix Controller Platine
{														// Übergabe mode - 0/1/2 für transparent/xmodem upload/xmodem download
 int result=-1;			// Hilfsvariable Ergebnis
 uint ic58port;			// Expander Portzustand	
 bool Term_state;		// Status Viasis - Matrix Schnittstelle  	

 concpy=connect;																		// Schnittstellenzustand merken
 clear_all();																				// Led samt Regler und Schalter aus	
 PINSEL7 |= PWM3;																		// Externen PWM Ausgang wieder ein, sonst funktionieren Matrix Anzeigetestfunktionen bei BT, USB Verbindung nicht		
 Term_state=read_i2cdev (IC54, 0, 1) & RS232_LINK;	// Bestehende RS232 Verbindung zu Matrix (Invalid) ausmaskieren
 ic58port=read_i2cdev (IC58, 2, 3);									// Zustand Ausgänge IC58 lesen, Achtung Register 3 wg. Byteswap!	 		
	
 if (write_i2c_dev (IC58,3,(((ic58port&~0x8000)<<8)|0x02))) // Kommunikationsanforderung an Matrix gesendet?
 {
	msdelay=XMODEM_CHAR_TIMEOUT;		// Max. Wartezeit Verbindungsaufbau Matrix controller

  if (Term_state==0)							// Matrix controller war aus?
   while (!comchange)							// Warte bis Matrix controller RS232 ein
    if (msdelay==0) break;				// Wartezeit abgelaufen, dann Abbruch 
	 
	if (msdelay)			    	// Matrix controller hat sich gemeldet, oder war bereits an?
  {
	 osDelay(5);
   clear_comchange ();		// Schnittstellenwechsel reset
   connect=UART0;					// Ausgabe nur an Matrix umleiten
   putb(ESC);	
	 if (mode==0) 	    		// Transparente Kommunikation UART1 mit Matrix Controller
   {
		plustrans=1;					// Direkte Kommunikation 
		bpi=rpi;							// Reset Indizes Plus Buffer 
		bxi=rxi;							// Reset Indizes "Terminal" Buffer 
		msdelay=XMODEM_BLOCK_TIMEOUT;				// 3 s Intervall für Watchdog reset
		while (!comchange)		// Solange Kommunikation nicht beendet
    {
		 if (bpi!=rpi) 				// Zeichen von Matrix?
		 {
			connect=concpy&~UART0;	// Matrix RS232	ausblenden
			putb(pbuf[++bpi]);			// Zeichen an USB oder ext. UART1 Schnittstellen senden
		 }		
     if (bxi!=rxi)						// Zeichen von externer Schnittstelle/Terminal
		 {
			connect=UART0;					// nur an Matrix 
			putb(rbuf[++bxi]);			// an Matrix senden
			if (rbuf_full) 					// Empfangspuffer voll gelaufen? (RTS high, Empfang angehalten)
       if (bxi==rxi)					// Wieder Platz in Ringpuffer?
			 {	 
				rbuf_full=0; 					// Flag Puffer voll reset 
				NVIC_EnableIRQ(UART1_IRQn);		// UART1 Interrupt wieder zulassen, Zeichen abholen
			 }	 
		 }
     if (!msdelay)
		 {
			ResetWDT();											// Watchdog reset
			msdelay=XMODEM_BLOCK_TIMEOUT;		// 3 s Intervall 
		 }		
		}	// end while !comchange	
		plustrans=0;																				// Direkte Kommunikation über pbuf beenden
		result=1;
   } 	
	 else 
	 {	 
		if (mode==1) { result=plus_upload_old ();				// Upload von Textnachrichten und Sonderzeichen   
		 if (waitcms (XMODEM_CHAR_TIMEOUT, '?')>=0)			// Hauptmenü empfangen?
		 {	
		  putb('R');																		// Viamatrix Hauptmenü beenden
		  waitcms (XMODEM_CHAR_TIMEOUT, ' ');						// Warte auf Trennzeichen
		  waitcms (XMODEM_CHAR_TIMEOUT, LF);						// Warte auf Line feed von "Taste <ESC> .... 
		 } }
																										// Neue schnelle Schnittstellen für MQTT Betrieb
    else if (mode==2) result=plus_download ();			// Download von Textnachrichten und Sonderzeichen			
		else if (mode==3) result=plus_upload ();				// Download von Textnachrichten und Sonderzeichen 
	 }	 
  }	// end if msdelay - keine timeout	
 } // end if I2C Antwort	 
 write_i2c_dev (IC58,3,((ic58port|0x8000)<<8)|0x02); 	// Kommunikation sis3003 schließen
 if (Term_state==0) osDelay (10);											// Warte bis Matrix RS232 aus
 clear_comchange ();																	// Wechsel Schnittstellenstatus reset
 connect=concpy;																			// Schnittstellen wiederherstellen	
 if (result<0)
 {
  if (result==-1) puterror (DTC_LIB_HW_ERR,-1);			// Hardware Fehler eintragen
	else if (result==MATRIX_PARAM_ERROR);							// Nichts machen, neg. Rückgabeergebnis führt zu Parameter Init error  
	else puterror (DTC_LIB_COMM_TIMEOUT, result);		// Kommunikationsfehler eintragen
 } 
 return(result);	
}

void md_simulation (uint anzahl, uchar terminal)		// Erzeuge Messdaten im Speicher
{																										// Übergaben: Anzahl - Anzahl Messdaten
																										//						terminal - bei 1 Terminalausgabe
 uint start=1;	
 int16_t wert, ii=1000;
 int8_t vz=1;	
 uchar dd=1, mm=6, yy=18, hh=0, min=0, ss=0;
 uint page=fp.md_page;	
 uchar * vptr;
 uchar memful=0;	
	
 if (terminal) putparameter (T_anzmw, start,(1<<16)|0x06, T_nil);	// Initialwert für Bildschirmausgabe
 if (messdaten(0))					// Bereits Messdaten im Speicher? 
 {
	if (fp.md_adr==0)	// Messdatenzeiger steht auf Seitenanfang -> vbuf ist leer
  {									// -> letzte Messdatenseite auslesen
	 if (page==0) page=PROTOCOLPAGE-1;				// Messdatenseite ist erste Seite -> letzte Seite Protokollseite -1
	 else page--;	
   flash_pcom (vbuf, page, FLASH_R_MAIN, VSATZLEN*MD_PER_PG);  // Letzte Seite lesen	
   vptr=&vbuf[VSATZLEN*MD_PER_PG];					// Zeiger hinter letzten Messdatensatz der Seite		
	}		
  else vptr=&vbuf[fp.md_adr]; // auf aktuellen Satzanfang
	ss=*(--vptr); min=*(--vptr); hh=*(--vptr);		// Zeit lesen 
	yy=*(--vptr); mm=*(--vptr); dd=*(--vptr);			// Datum lesen
 } // end if messdaten vorhanden	 
 
 do 
  {
	 vptr=&vbuf[fp.md_adr];								// Zeiger auf Beginn Messdatensatz	
		
	 ss+=1+(T1TC&0x0F);										// Zeitwert erhöhen
	 if (ss>59) 
	 { ss-=60; min++; if (min>59) 
		 { min-=60; hh++; if (hh>23) 
			{ hh-=24; dd++; if (dd>28)				// wg. Februar
			 { dd-=28; mm++; if (mm>12)	
				{ mm-=12; yy++;} } } } }				// Soweit sollte es nicht kommen	
		
	 if ((start%10)==0) vz	= -vz;				// Toggle Vorzeichen
	 wert=vz*ii--;	
   if (ii <= 10*fp.vmm) 	// Wenn ii < kleinste messbare v
	 {	 
		 ii=1000;							// wieder bei 100 beginnen
		 ResetWDT();
   }		 
	 
	 *vptr++=wert>>8;							// Geschwindigkeit in Puffer kopieren
	 *vptr++=wert;
	 *vptr++=0;										// Entfernung in Puffer kopieren
   *vptr++=0;	
	 
	 if (terminal)								// Nur im Terminalbetrieb historisches Datum verwenden
	 { 
	  *vptr++=dd;										// Datum
	  *vptr++=mm;
	  *vptr++=yy;							
	  *vptr++=hh;										// Uhrzeit
	  *vptr++=min;
	  *vptr++=ss;
	 }
   else
   {
		read_date_time (DATE_HEX, (char *)&vbuf[fp.md_adr+4]);	// Datum in Puffer schreiben
    read_date_time (TIME_HEX, (char *)&vbuf[fp.md_adr+7]);	// Zeit in Puffer schreiben
    vptr+=5;
		*vptr++=start;							// Unterschiedliche Sekundenwerte für cloud erzeugen
   }		 
	 if (flashsize!=0) 									// Flash vorhanden?   
	  if (mdata_to_flash ()) memful=1;		// Schreibe Messdaten in Flash  
	 	 								
	 if (terminal && ((start%100==0)||(start==anzahl))) // Bei Terminalausgabe, jedem hundertsten bzw. beim letzten Wert
   {delchar(6); 																			// Ausgabe löschen	
    putnumber (start,6);															// Index ausgeben
	  if (is_CRLF(getbyte(10))) break;									// Abbruch bei Return Eingabe 
   }			 
  } while (start++<anzahl); 													// Bis Übergabe Anzahl erreicht
	
 if (memful) protocol(MEMFULL);						// Speicher voll -> Protokollmeldung erzeugen
}	

void communication_change (void)					// Bearbeitung Schnittstellenwechsel, Änderung der Kommunikation oder Hexschalter
{																					// wird über I2C Expander gemeldet
 uchar waitdelay = 250;										// Default Wartezeit bis zum Versand der Startnachricht
 uchar comstate_neu;	
 int state=get_com_status ();  						// Status I2C Expander IC54 Eingänge lesen
 uint32_t baud =BT_BAUD;										// Default BT Baudrate	
	
 if (state>=0)														// IC54 erfolgreich gelesen? 	
 {
  comstate_neu=state;	 
	
	if ((comstate_neu^comstate)&BT_LINK&interfaces) // Bluetooth und Änderung Bluetoothverbindung?
  {
   if ((comstate_neu&BT_LINK)==0) 	// Bluetooth neu verbunden? - BT_LINK ist low aktiv
   {      	 
		if (connect&MQTT_LINK) 					// Besteht MQTT Verbindung?
		{	
     comchange=0;														// reset Schnittstellenwechsel
		 MQTT_fast_disconnect (1); 							// MQTT Serververbindung besteht? Schließe Verbindung mit Abmeldung sofort
		 if (comchange) state=get_com_status(); // Verbindungsänderung GSM Modul abfangen
     if (state>=0) comstate_neu=state; 			
    }
		else osDelay(200);
		
		if (Init_BT_ch (baud))					// Konfiguriere uart1 und setze MUX Kanal auf BT modem, bereit?
		{	
     startmessage=1; 				  
		 if (fp.gps<2)gps_pending=0;						// Laufende GPS Positionsbestimmung bei Multiplex unterbrechen
		 connect|=(UART1|BT_LINK);							// Daten an UART1 senden
     bt_time=BT_TIMEOUT;					  				// Max. Bluetooth Verbindungszeit	ohne Empfang	
		 if (fp.btmodem==Roving || fp.btmodem==IF820) bt_pininit=1;	// Pinabfrage erforderlich (RN4678 + IF820)
		 
#if (VSPCAM) 				 
		 if (fp.vspcam) online=0;				// Online Messmodus reset		
#endif
		}
	 }		// end if BT neu verbunden														
	 else 														// Bluetooth Verbindung beendet
	 {	
		connect&=~(UART1|BT_LINK);			// Daten nicht mehr an UART1 senden
		bt_pininit=0;										// Reset Bluetooth Pin Initialisierung falls aktiv
#if (VSPCAM)			
		if (fp.vspcam && (connect&UART0)) online=1; 	// viaspeedcam und RS232 Verbindung -> online Messmodus
#endif				
	 }	
	 comchange=0;											// Änderung Kommunikationsstatus bearbeitet
	 thread_comchange=0;
  } // end BT connect/disconnect
	 
	if ((comstate_neu^comstate)&GSM_LINK&interfaces) // GSM vorhanden, Änderung GSM-Verbindung?
  {		
   if (gsmpower && (fp.servertyp==SMTP))						// Modem eingeschaltet und kein MQTT-Verbindungsabbruch?
	 {
		 if ((comstate_neu&GSM_LINK)==0)								// Incoming call? GSM_LINK ist low aktiv
		 {
		  gsm_power(2);	  															// Uart1 und Mux Kanal konfigurieren  
			wait_message(T_conn, 2*XMODEM_BLOCK_TIMEOUT);	// CONNECT "9600" Nachricht abwarten, kann aber auch schon durch sein
			connect|=(UART1|GSM_LINK);										// Daten an UART1 senden
#if (VSPCAM)				 
			if (fp.vspcam) online=0;											// Online Messmodus reset  
#endif				 
			startmessage=1;																// Einschaltmeldung senden
			gsmcall=1;																		// Datenverbindung besteht
			if (fp.gps<2) gps_pending=0;									// Laufende GPS Positionsbestimmung bei Multiplex unterbrechen	
		 }
		 else if (gsmcall)															// Einwahlverbindung besteht?  
		 {
			if (wait_message(T_noca, XMODEM_BLOCK_TIMEOUT)<0)	// NO CARRIER Meldung abfangen
			 sendbuf(cbuf,cind);															// Bei Fehler - echo Puffer
			gsmcall=0;																				// Datenverbindung besteht nicht mehr
			FIO0SET=GSM_DTR;																	// DTR HI - Enable Modem Sleep
			connect&=~(UART1|GSM_LINK);												// Ausgabeverbindung löschen
#if (VSPCAM)			
			if (fp.vspcam && (connect&UART0)) online=1; 	// viaspeedcam und RS232 Verbindung -> online Messmodus
#endif				
	  } // end if gsmcall 		 
	 } // end if Modem ein
	 comchange=0;																			// Änderung Kommunikationsstatus bearbeitet
	 thread_comchange=0;
  } // end GSM connect/disconnect and power on/off

	if ((comstate_neu^comstate)&RS232_LINK) 	// Änderung RS232 Terminal?
  {		 
   if (comstate_neu&RS232_LINK) 						// RS232 Terminal angeschlossen?
	 {
    startmessage=1;												// Einschaltmeldung senden
	  connect|=UART0;												// Daten an UART0 senden
		wait_min=3;														// 3 Warteminuten für GPS-Positionsfix  
		if (connect&MQTT_LINK) 								// MQTT Serververbindung besteht? 
		 if (!mqtt_debug) mqtt_state=CLOSE;		// Kein MQTT Debug, dann Verbindung schließen
#if (VSPCAM)			
		if (fp.vspcam) online=1;								// viaspeedcam online Modus starten
#endif					
   }
   else 
	 {	 
		connect&=~UART0;			// kein Terminal (mehr) angeschlossen, Daten nicht an UART0 senden
		mqtt_debug=0;					// Beende MQTT Debug Mode
	 }	 
	 comchange=0;								// Änderung Kommunikationsstatus bearbeitet
	 thread_comchange=0;
  } //end Terminal Änderung
	 
	if (comchange||thread_comchange)			// keine Datenschnittstelle geändert -> Drehschalter-Änderung
  {
	 if (fp.turnsw)							// Drehschalter aktiviert?
	 { 
		osDelay(20);							// warte bis alle Kontakte umgeschaltet	
		get_turnswitch();					 
		timeupdate=1;							// Drehschalter neu lesen, legt aktiven Parametersatz fest 	  
	 }	
	 comchange=0;		 						// Änderung bearbeitet
   thread_comchange=0;	 
	} 
	 
	if (startmessage)		// Einschaltmeldung zu senden
  {    
	 startmessage=0;
	 osDelay(waitdelay);				// Warte bis Verbindung stabil	
	 putmstr(L_start);					// Einschaltmeldung an Terminal senden	
	 if (bt_pininit) putstr (T_pin);  // Bluetooth Pinnummer Abfrage senden bei RN4674	 
	 else
	 {	 
		putstr (T_main);					// Haupmenü an Terminal senden
    select ('6',0);						// Menüauswahl senden
	 }	 
	 bxi=rxi;										// Evtl. Zeichen vor Verbindung löschen	
	 menu=0;										// reset Menüauswahl	
#if (VSPCAM)		 
	 if (!fp.vspcam) 	// viaspeedcam deaktiviert?
#endif			
	 online=0;					// Online Messmodus reset
	 if (fp.gps<2) wait_min=Def_wait;				// Setze 3 Warteminuten bei GPS Multiplex
  } // end if startmessage 
	concpy=connect;							// Kopie aktualisieren
	comstate=comstate_neu;	
 } // end if IC54 erfolgreich gelesen	
}

void reboot (void)							// Erzeuge Watchdog reset und starte CPU neu
{
 i_reset=BOOT_RES;													// Setze Resetursache 
 Ledaus();																					// Disable Anzeige
 if (connect&MQTT_LINK) MQTT_fast_disconnect(1); 		// MQTT Verbindung sofort schließen	
 PCONP|=PCI2C0|PCSSP1;															// I2C0 und SSP1 Bus einschalten
 FIO1CLR = GSMPWR|USB_RPWR|PWRKEY|EN_ANA;						// GSM, USB und Analogversorgung (Transceiver,Verstärker) aus
 flbuf[0]=0x02; flbuf[1]=0; flbuf[2]=0;							// Puffer mit Output Register 2, Port 0 und 1 null füllen
 I2Cdrv->MasterTransmit(IC37>>1,flbuf,3,false);			// Puffer an IC37 senden -> 5V/3V3 Analogversorgung komplett aus	
 protocol(REBOOT);																	// Reboot protokollieren	
 flash_wbuf1 ((uchar *)&fp.Kennung, PARAMETERPAGE, sizeof(fp));	// Parameter sichern
 flash_wbuf1 (vbuf, fp.md_page, fp.md_adr);					// aktuelle Seite mit Messdaten sichern  
 flash_wbuf1 (probuf, fp.pro_page, fp.pro_adr);			// aktuelle Protokollseite sichern 	
 WDTC=0; 									//
 WDMOD=WDEN | WDRESET;			// Enable Watchdog	 
 WDFEED=0xAA;								// Timed access
 WDFEED=0x55;	
 while(1);								// Warte bis abgeschaltet oder Watchdog reset								 		
}







