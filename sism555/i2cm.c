//-----------------------------------------------------------------------------
//  FILE: cmper.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS: Verlinkt Routinen der alten viasis 3003 Firmware
//						mit CMSIS 2.02 Treiberroutinen. 	
//						Betrifft folgende Periperie:	I2C, SPI
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
#include "flash.h"
#include "string.h"
#include "i2cm.h"
#include "sio.h"
#include "sictxt.h"


const i2cdevices i2cdev[Anz_I2C_Devices]  = {	{0, 0, 0, 16, 0x80, "Unknown" },
																			{0x42, 37, EXPD, 8, 0x48, "PCA9555"},
																			{0x44, 58, EXPD, 8, 0x48, "PCA9555"},
																			{0x46, 54, EXPS, 4, 0x48, "PCA9554"},
																			{0x58, 35, DPP, 11, 0x80, "MCP4461"},
																			{0x8A, 0, 0, 16, 0x80, "TCA6507"},
																			{0x64, 61, RTC, 16, 0x80, "PTRTC"}};


ARM_DRIVER_I2C * I2Cdrv = &Driver_I2C0;	// Zeiger auf I2C0 Struktur

int Init_I2C	(void)													// Initialisiere CMSIS I2C Treiber
{
 int result;
	
 fp.i2cdev&=~I2CB0;																// I2C Bus bereit Bit löschen
 result=I2Cdrv->Initialize (NULL);								// Kein Callback 
 result|=I2Cdrv->PowerControl (ARM_POWER_FULL);		// Fehler in Library - I2Cdrv->PowerControl setzt PCLK_I2C0 auf CCLK
																									// Daher muss PCLK_I2C0 auf CCLK in system_lpc17xx gesetzt sein
 result|=I2Cdrv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_NORMAL); // sonst ändert sich die I2C Baudrate um Faktor 4
																									// wg. ARM_I2C_BUS_SPEED_NORMAL=250 kHz Daher Driver_I2C.h und I2C_LPC17xx.c modifiziert	
 NVIC_SetPriority(I2C0_IRQn,4);										// I2C0 Interrupt Priorität erhöhen	wg. I2C Zugriff bei Power Down
 return (result);	
}

bool write_i2c_dev (uchar dadr, uchar anz, uint data) // Anzahl Bytes an I2C Device senden
{																				// Übergabe i2cadr	- Deviceadress
																				// 			anz		- Anzahl Bytes zu senden
																				//			data	- max. 4 Bytes in 32 Bit gepackt
 uint rdelay;																				// Antwortzeit in us	
		
 memcpy(flbuf, &data, sizeof(data));								// Data in Puffer kopieren														
 I2Cdrv->MasterTransmit(dadr>>1,flbuf,anz,false);		// Schreibt Anzahl Bytes an Slave device
 rdelay=T1TC+200U*anz;															// Timer 1 Timeout Stand berechnen	
 while (I2Cdrv->GetStatus().busy)										// warte bis wieder bereit	
  if (T1TC > rdelay) break;													// Maximal 100 us auf Byte transfer warten
 if (!(I2Cdrv->GetStatus().busy))										// I2C bereit?
  if (I2Cdrv->GetDataCount () == anz) 
		return (true); // Anzahl Bytes gelesen? Ja,Rückgabe Erfolg
 I2Cdrv->Control (ARM_I2C_ABORT_TRANSFER,true);			// Abbruch Transfer
	
 return (false);		// Misserfolg
}	

int read_i2cdev (uchar dadr, uchar adrcom, uchar anz)		// I2C Device mit repeated start auslesen
{																			// Übergabe i2cadr 	- I2C Deviceadresse
																			// 					adrcom 	- Adresse, Kommando
																			//					anz		- Anzahl bytes zu lesen
 int result=0;		
 uchar *Pbuf;		
 uint rdelay;													// Antwortzeit in us	
	
 flbuf[0]=adrcom;																		// Kommando oder interne Adresse in Puffer
 if (anz>FBUFSIZE) return(-1);											// Abbruch
 I2Cdrv->MasterTransmit(dadr>>1,flbuf,1,true);			// Sendet Kommando an Slave device
 rdelay=T1TC+200;																		// Timer 1 Stand + 100 µs laden	
 while (I2Cdrv->GetStatus().busy)										// warte bis wieder bereit	 
	if ((T1TC > rdelay)&& I2Cdrv->GetStatus().busy)		// Busy nochmal nachhaken wg. Multi-tasking	
		{ result=-1; break; } 													// Timeout Byte transfer Zeit überschritten Abbruch  	 
 
 if (result>=0)																			// Kein Abbruch?
 {
  I2Cdrv->MasterReceive(dadr>>1,flbuf,anz,false);		// Anzahl bytes in Puffer einlesen
  rdelay=T1TC+200U*anz;															// Timer 1 Stand + 100 us / Byte laden 
  while (I2Cdrv->GetStatus().busy)									// warte bis wieder bereit
	 if ((T1TC > rdelay)&&I2Cdrv->GetStatus().busy)		// Busy nochmal nachhaken wg. Multi-tasking
		{ result=-1; break; } 					// Timeout Byte transfer überschritten Abbruch 
  if ((result>=0) && (I2Cdrv->GetDataCount () == anz))		// Erfolg?
  {
   Pbuf=(uchar *)&result;	 													// Bytezeiger auf result Variable			
	 if (anz<=3) while (anz--) *Pbuf++=flbuf[anz]; 		// bis zu 3 Pufferbytes nach result
	 return(result);		// Rückgabe Pufferbytes 
  }	
 } // end if result>=0, Deviceadresse	
 
 I2Cdrv->Control(ARM_I2C_ABORT_TRANSFER,true);			// Abbruch Transfer
 return(-1);																				// Rückgabe Misserfolg
}

int InitI2C_Devices (void)		// I2C Bausteine initialisieren
{
 int result=1;
	
 FIO1SET = EN_ANA;						// Analogversorgung/Transceiver ein	
 osDelay (50);								// warte bis Spannung ok
	
 if (write_i2c_dev (IC37,3,((0x0100<<8)|0x02)))						// IC37 PCA9555 Analog-/Transceiversteuerung
 {																												// 5V/3V3 Versorgung einschalten
  write_i2c_dev (IC37,3,((0x0100<<8)|0x06));							// IC37 Port Konfiguration
  fp.i2cdev|=IC37_b;		// IC37 Initialisierung eintragen  
 }
 else return (-1);			// Misserfolg, IC37 zwingend notwendig für Analogsteuerung
 
 if (write_i2c_dev (IC58,3,((0<<16)|(0<<8)|0x06)))				// IC58 PCA9555 Ports alles Ausgänge
 {	 
	if ((fp.ex12==2)||(fp.ex12==4)) fp.defic58|=0x8000;			// Plus/Viatext evtl. gesetztes sis3003 Kommunikationsflag reset  
  if (write_i2c_dev (IC58,3,((fp.defic58<<8)|0x02)))			// IC58 default Ports setzen
   fp.i2cdev|=IC58_b;																			// IC58 Initialisierung eintragen
	else if (fp.ex12>=2) return (-1);												// Bei Plus, Viatext oder FT zwingend erforderlich
 }	
	
 if (write_i2c_dev (IC54,2,(0xFF<<8)|0x03)) 							// PCA9554 HEX-Switch und COM Interface		
	 fp.i2cdev|=IC54_b;																			// IC54 Initialisierung eintragen 
 else return (-1);																				// Misserfolg, IC54 zwingend notwendig 
 
 if (read_i2cdev (IC35,0x5C,1)>=0)					// Status Register DPP MCP4441 lesen
	 fp.i2cdev|=IC35_b;												// IC35 Anwesenheit eintragen  
 else return (-1);													// Misserfolg, IC35 zwingend notwendig
 
 if (fp.ledspot>0)													// Led Spotlampe aktiviert?
 {
	if (write_i2c_dev (ICLED,3,((0x60<<16)|(0x20<<8)|0x16))) 					// TCA6507 vorhanden?
  {
   write_i2c_dev (ICLED,4,((0x00<<24)|(0x00<<16)|(0x10<<8)|0x13));	// Blinkrhytmus setzen
   fp.i2cdev|=ICLED_b;																							// LED Spot Device eintragen
  }
  else result=-1;														// Misserfolg, TCA zwingend notwendig
 } // end Led Spotlampe 

 return (result);	// Rückgabe Initialiserungsergebnis
}

int get_com_status (void)  		// Status I2C Expander IC54 Eingänge lesen
{ 
 int state=read_i2cdev (IC54, 0x00, 1);		// IC54 Register 0 Inputs auslesen
 if (state>=0) return (state);						// Rückgabe Erfolg 
 protocol (I2C_DEVICE_ERROR);							// Fehler protokollieren	
 return	(-1);															// Rückgabe Fehler	
}	

uchar get_turnswitch (void)		// Status Drehschalter lesen und Position dekodieren
{
 uchar wert;
 wert=(~get_com_status())&0x07;
 if (wert && (wert<=fp.psets+1)) return (wert-1);	// Schalterposition!=6 und < Anzahl Parametersätze
 return (0);	// Schalterposition 
}

void reset_switches (void)		// Setzt I2C Expander zurück
{	
 uchar retry=16;	
 if (fp.i2cdev&IC58_b) write_i2c_dev (IC58,3,((fp.defic58<<8)|0x02));	// Default - Ausgabe an I2C Expander
 if (fp.ex12==3) FIO0CLR = EX12_2;																		// Plus FT, 12V extern abschalten
 if (fp.i2cdev&ICLED_b) while (write_i2c_dev (ICLED,4,0x10)==false) 	// reset Led Spot
	if (!retry--) break; 																								// Abbruch nach 16 Versuchen
 PINSEL7 &= ~PWM3;																										// Trenne externen PWM Ausgang erstmal ab
}

uchar setswitches(uchar speed)		// Schalter entsprechend Schaltschwellen setzen
{																	// Übergabe speed - gemessender Geschwindigkeitswert
 uchar i;													// Laufvariable Schalter
 uchar switchset=0;								// Flag Schalter gesetzt
 uint portic58, portspot=0;	
 uchar retry=16;	

 /*  if (fp.ex12==2)	// Viasis Plus?
 { // Unterbrechung evtl. gesetzter Textseitenschalter, für Synchronisation blinkender Anzeigen
  if (pagesync++>3)	// erst nach 4 Anzeigen Synchronisations-Unterbrechung
  {
   write_i2c_dev (IC68,3,(((portic68|(fp.defic68&0x000F))<<8)|0x02)); // reset Textseiten P0.0 - P0.3
   waitus (100); // 100 µs Unterbrechung
   pagesync=0;
  }
 } */
 
 portic58=fp.defic58;			// IC58 Portmaske übergeben

 for (i=0;i<NOSW;i++)			// Schaltschwellen aller Schalter prüfen	
 {
  if (fp.swexp[i])				// gültiger Schalter
  {
   if (speed>=fp.vswi[pset][2*i])											// Geschwindigkeit >= Einschaltschwelle	des Schalters?
	 {	 
    if (fp.swexp[i]==1) portic58|=(1<<fp.swport[i]);  // Portpin in Maske setzen (0..15 normal, 16..31 invertierend)
 		if (fp.swexp[i]==2) portspot|=(1<<fp.swport[i]);	// Pin 0..2 in Led Spot Maske setzen
   }
   if (speed>=fp.vswi[pset][2*i+1])	// Geschwindigkeit >= Ausschaltschwelle	des Schalters?
	 {	 
    if (fp.swexp[i]==1) portic58&=~(1<<fp.swport[i]); // Portpin in Maske rücksetzen (0..15 normal, 16..31 invertierend)
		if (fp.swexp[i]==2) portspot&=~(1<<fp.swport[i]); // Pin 0..2 in Led Spot Maske löschen	
	 }	 
  }	// end if gültiger Schalter
 } // end alle Schalter prüfen

 if (fp.i2cdev&IC58_b)					// Expander IC58 vorhanden?
 {
  portic58&=~(portic58>>16);										// Invertierende Ports berücksichtigen
  write_i2c_dev (IC58,3,((portic58<<8)|0x02));	// Ausgabe an Expander IC68
  if (fp.defic58!=portic58) 
	{	
	 if ((fp.ex12==3)&&(portic58&0x0F))	FIO0SET = EX12_2; 	// Viasis Plus FT 12V extern Versorgung einschalten
	 switchset=1;				// Schalter wurden gesetzt	 				
	}	
 }

 if (fp.i2cdev&ICLED_b)					// Externer LED Spot - TCA6507
 {	 
	if (portspot&0x07)						// Schalterbits gesetzt?
  {	
	 while(write_i2c_dev (ICLED,4,((portspot<<24)|(portspot<<16)|(portspot<<8)|0x10))==false)	// Led ansteuern
		if (retry--) break;																		// Abbruch nach 8 Versuchen
	 if (retry) switchset=1;																// Schalter wurden gesetzt	
	} // end if Schalterbits	
  else while (write_i2c_dev (ICLED,4,0x10)==false)				// reset Led Spot
   if (retry--) break;																		// Abbruch nach 8 Versuchen		
 } 	
 
 return (switchset);
}

void clear_all	(void)	// Numerische Anzeige und ggf. Zusatzanzeigen abschalten
{
 Ledaus();						// Alle Led ausschalten
 reset_switches();		// Reset alle Schalter
}



