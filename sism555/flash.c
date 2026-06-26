//-----------------------------------------------------------------------------
//  FILE: flash.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für externen Flash Speicher 
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H2
//-----------------------------------------------------------------------------
//	COMPILATION: Thumb/Cortex M3
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   21.1.2015
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:	22.04.2015:	Hardware version H1 -> H2
//									13.07.2015: Hardware version H2 -> H3
//              	
//-----------------------------------------------------------------------------

#include "flash.h"
#include "ramcode.h"
#include "libtool.h"
#include "sio.h"
#include "rtc.h"
#include "i2cm.h"
#include "mqtt.h"
#include "cmsis_os.h"
#include "sictxt.h"
#include <string.h>
#include "dtc_codes.h"

void EINT0_IRQHandler (void)		// Interruptanforderung Power Down
{
 PINSEL4 &= ~(PWM6);											// Disable Anzeige	
 PCONP|=PCI2C0|PCSSP1;										// I2C0 und SSP1 Bus einschalten
 FIO1CLR = GSMPWR|USB_RPWR|PWRKEY|EN_ANA;	// GSM, USB und Analogversorgung (Transceiver,Verstärker) aus
 ResetWDT();															// watchdog timer reset	
 flbuf[0]=0x02; flbuf[1]=0; flbuf[2]=0;							// Puffer mit Output Register 2, Port 0 und 1 null füllen
 I2Cdrv->MasterTransmit(IC37>>1,flbuf,3,false);			// Puffer an IC37 senden -> 5V/3V3 Analogversorgung komplett aus	
 flash_wbuf1 ((uchar *)&fp.Kennung, PARAMETERPAGE, sizeof(fp));	// Parameter sichern
 flash_wbuf1 (vbuf, fp.md_page, fp.md_adr);					// aktuelle Seite mit Messdaten sichern  
 flash_wbuf1 (probuf, fp.pro_page, fp.pro_adr);			// aktuelle Protokollseite sichern 	
 while(1);								// Warte bis abgeschaltet oder Watchdog reset								 	
}

int InitSPIFlash (void)			// Initialisiere SPI und Flash
{

#if (SIMULATION==1)				
 uint retry=1;					// Anzahl Wiederholungen
#else	
 uint retry=8;					// Anzahl Wiederholungen															
#endif

	
 flashsize=0;					// Speichergröße in MB
 Init_SPI ();					// SPI (SSP1) Interface einschalten und initialisieren		
 P_fpar=&flbuf[0];		// Pointer auf unsigned char Arbeitspuffer
 flbuf[0]=0;					// reset Puffer	
 do
 {
  flash_command(FLASH_R_ID,1,2);		// Manufacturer and Device ID read
  if (flbuf[0]==0x1F) switch (flbuf[1]) // nach Device ID auswählen
  {
   case 0x24: flashsize=512;	// 512 Kilobyte, 2048 pages x 264 Byte
   			maxpage=2047;						
			  maxbyte=264;			
			  break;
   case 0x25: flashsize=1;		// 1 MegaByte, 4096 pages x 264 Byte
   			maxpage=4095;						
			  maxbyte=264;			
			  break;	
   case 0x26: flashsize=2;		// 2 MegaByte, 4096 pages x 528 Byte
	 		  maxpage=4095;						
			  maxbyte=528;			
			  break;	
   case 0x27: flashsize=4;      // 4 MegaByte, 8192 pages x 528 Byte
			  maxpage=8191;		
			  maxbyte=528;	
			  break; 	
   case 0x28: flashsize=8;      // 8 MegaByte 
			  flash_command(FLASH_R_STATUS,1,2);	// AT45DB642D - 1 Statusbyte; AT45DB641E - 2 unterschiedliche Statusbytes
		    if (flbuf[0]==flbuf[1])		// Dasselbe Statusbyte 2 mal gelesen - AT45DB642D mit 8192 pages x 1056 Byte
				{	
			   maxpage=8191;		
			   maxbyte=1056;	
				}
				else 											// Zwei unterschiedliche Bytes -> AT45DB641E - mit 32767 pages x 264 Byte
				{	
			   maxpage=32767;		
			   maxbyte=264;	
				}	
			  break;	
   default:   break;     		// unbekannte oder keine Flash Kennung	   
 			  //Protokoll (22);		// unbekannter Speicherchip
  }
  else osDelay(1);						// ~ 1 ms warten				
  if (--retry==0) break;  
 }
 while (!flashsize);	// bis Flash erkannt
   
 if (flashsize!=0)					// Bekannter Speicher 
 { 
  pagepk=FLASHKGV/maxbyte;	// Berechne Anzahl Seiten zur Sicherung 1k Block
  return (0);								// kein Fehler - Flash erfolgreich erkannt
 } 
 
 pagepk=1;								// kein Speicher, aber pagepk setzen wg. Nulldivision
 maxpage=0; 							// reset Anzahl Seiten und -größe
 maxbyte=0; 
 return(FLASH_ERROR);			// Fehler kein (bekannter) Flash
}	

uint flash_pcom (uchar * Pdata, uint page, uint opcode, int size)	// Adaptiert Seitenzugriff für unterschiedliche Flash Seitengrößen
{													// Übergaben 	Pdata	- Zeiger auf Sende-/Empfangsdaten
													//				page 	- Flash Seitenadresse
													//				opcode 	- Flash Kommandocode
													// 				size	- Anzahl Schreib/-Lesebytes												
 uint bytes;					// Anzahl Schreiblesebytes
 uchar adrbytes=4;		// Anzahl Kommando-, Adress- und Dummybytes

 if (opcode==FLASH_R_MAIN || opcode==FLASH_R_ARRAY) adrbytes=8;

 if (maxbyte>0)				// Flash vorhanden?
 {
  if ((opcode==FLASH_ERASE_WRITE_1) && (size>maxbyte)) bytes=maxbyte;	// Bytes zu schreiben maximal Puffergröße
   else bytes=size; 
  P_fpar=Pdata;							// Übergabe Datenzeiger
  flash_adress (0, page++);				// Seite im Atmel Flash adressieren
  flash_command (opcode,adrbytes,bytes);		// Kommando senden

  if (opcode==FLASH_ERASE_WRITE_1)		// Schreiben durch Atmel SRAM Puffer 1
  {
   size-=maxbyte;						// Seitengröße abziehen
  
   while (size>0)						// noch Bytes zu schreiben
   {
    P_fpar+=maxbyte;					// Seitengröße zu Datenzeiger addieren
    if (size>maxbyte) bytes=maxbyte;	// Anzahl Bytes > Seitengröße
    else bytes=size;   						
    flash_adress (0, page++);				// Seite im Atmel Flash adressieren
    flash_command (opcode,adrbytes,bytes);	// Seite schreiben oder lesen
    size-=maxbyte;							// Seitengröße	
   } 
  }
 }
 return (page);			// Rückgabe letzte geschriebene/gelesene Seite + 1
}

void protocol (short event)		// Protokolleintrag ins Flash sichern
{								// Übergabe event - Ereignisnummer
 
 probuf[fp.pro_adr]=event>>8;			// Ereignis in Puffer kopieren
 probuf[fp.pro_adr+1]=event;
 read_date_time (DATE_HEX, (char *)&probuf[fp.pro_adr+2]);	// Datum in Puffer schreiben
 read_date_time (TIME_HEX, (char *)&probuf[fp.pro_adr+5]);	// Zeit in Puffer schreiben  

 fp.pro_adr+=PSATZLEN;												// Byteadresse um Protokollsatzlänge erhöhen
 if (fp.pro_adr>=BLOCKSIZE/pagepk)						// Seite bzw. pagepk*256 Bytes voll?
 { 
  fp.pro_adr=0;																// Reset Byteadresse
  if (flashsize!=0)														// Flash vorhanden
   flash_wbuf1 (probuf,fp.pro_page,maxbyte);	// Protokollseite speichern   															  
  if (fp.pro_page>=PROTOCOLENDPG) 						// Protokoll Seitenzeiger am Pufferende?
   fp.pro_page=PROTOCOLPAGE;									// Protokoll Seitenzeiger auf Pufferanfang setzen
  else fp.pro_page++;  												// sonst inkrementieren
  if ((fp.pro_page)==fp.pro_start_page) 			// Protokollzeiger überschreibt Protokollanfang?
  {
   if (fp.pro_start_page>=PROTOCOLENDPG)			// Protokollstartseite	am Pufferende?
    fp.pro_start_page=PROTOCOLPAGE;						// Protokollstartseite auf Pufferanfang
   else fp.pro_start_page++;									// Protokollstartseite inkrementieren
  }
  flash_erase (fp.pro_page, 1);	// neue aktuelle Protokollseite löschen
 }	 // end Seite voll
 
 if (event!=LastErrorEvent)										// Fehler ungleich letztem Fehlerereignis?
 {
	if (((event >=220)&&(event<=540))|| (event==WATCHDOG_RESET))	// Systemfehler?
  {																							// Letzte Systemfehler haben Priorität und überschreiben evtl. vorhandene Alarme
	 mailtosend|=(ALARMMAIL | SYSERROR);					// Fehlerstatus und Email Versand sichern
	 if (fp.smsalarm) smstosend|=SYSERROR;				// SMS Fehlermeldung zu senden	
  }		
	else if (!(mailtosend&ALARMMAIL))							// Noch kein aktiver Alarm? 
	{
	 if (event==BATTERYLOW)												// Unterspannung?
	 {	 
	  mailtosend|=(ALARMMAIL | LOWBAT);						// Low Battery Email Fehlermeldung senden
	  if (fp.smsalarm) smstosend|=LOWBAT;					// SMS Batterieunterspannung senden 	
	 }	 
	 else if (event==FULMEM)
	 {	 
	  mailtosend|=(ALARMMAIL | FULMEM);						// Speicher voll Flag für Alarm Mail setzen
		if (fp.smsalarm) smstosend|=FULMEM;					// Speicher voll Flag für Alarm SMS setzen
	 }	 
  }		
	LastErrorEvent=event;												
 }	 
 
 if ((connect&MQTT_LINK)&&(mqtt_state==STATUSSEND))
	if (event!=FULMEM)													// Speicher voll nicht melden
  {		
	 LastEvent=event;														// Ereignis übergeben
   mqtt_state=EVENT;	 												// MQTT Status setzen
	}		 
}

void flash_fill_buffer (uint fbyte)	// Beschreibt Atmel Flash Buffer mit fbyte
{                                   // Übergabe: 	fbyte = Füllbyte
 ushort anz;						// Laufvariable und Anzahl zu sendender Bytes													
 
 flash_ready (0);  					// Warte auf Flash bereit
 
 FIO4CLR =CSFLASH1;					// CS low, start spi transmit

 while (SSP1->SR&!TFE);				// warte bis leer
 SSP1->DR=FLASH_W_BUF1;				// dataFlash Opcode Buffer 1 schreiben
    
 for (anz=0; anz<maxbyte+3; anz++)
 {  
  while (SSP1->SR&!TNF);				// warte bis nicht voll	
  if (anz<3) SSP1->DR=0;				// Pufferbyteadresse=0 (3 Bytes) und				 
  else SSP1->DR=fbyte;					// danach Füllbytes schicken
  while (SSP1->SR&BSY);					// warte bis bereit
 } // end for anz 

 FIO4SET =CSFLASH1;						// CS flash reset, stop spi transmit
 Clear_SPI ();								// Disable SPI
}

void flash_erase (uint page, uint anzahl)	// Eine oder mehrere Flash Seiten im Hauptspeicher löschen
{											// Übergaben 	page 	- Flash Seitenadresse											
											// 				anzahl	- Anzahl Seiten	zu löschen
 P_fpar=&flbuf[0];						// Pointer auf Arbeitspuffer
 while (anzahl--)	// Anzahl Seiten löschen
 {
  flash_adress (0, page++);				// Seite im Atmel Flash adressieren
  flash_command (FLASH_ERASE_MAIN,4,0);	// Puffer 1 nach gelöschter Seite schreiben  
 }
}

uint flash_wbuf1 (uchar * Pdata, uint page, int size)	// Atmel Flash schreiben durch Puffer 1 auf gelöschte Seiten
{														// Übergaben 	Pdata	- Zeiger auf Sendedaten
  													//				page 	- Flash Seitenadresse											
														// 				size	- Anzahl Schreibbytes
 uint bytes;				// Anzahl Schreibbytes
	
 if (flashsize)							// Flash vorhanden?
 {
  P_fpar=Pdata;							// Übergabe Datenzeiger
  while (size>0)
  {
   if (size>maxbyte) bytes=maxbyte;				// Bytes zu schreiben maximal Puffergröße
   else bytes=size;
   flash_adress (0, 0);										// Byteadress Atmel Flash Puffer adressieren
   flash_command (FLASH_W_BUF1,4,bytes);	// Nach Puffer 1 schreiben
   flash_adress (0, page++);							// Seite im Atmel Flash adressieren
   flash_command (FLASH_W_BUF1_MAIN,4,0);	// Puffer 1 nach gelöschter Seite schreiben 
   P_fpar+=maxbyte;												// Seitengröße zu Datenzeiger addieren
   size-=maxbyte;													// Seitengröße
  }	// end while (size>0)
 } // end Flash vorhanden
 return(page); 	// Rückgabe letzte geschriebene Seite + 1
}

uint werkparameter (void)		// Setzt Parameter auf default Werkszustand
{
 uchar i;							// Laufvariable
 char *wp;						// Hilfszeiger zur Blocklöschung	

 fp.Kennung=FLASHKENNUNG;					// Kennung Werkinitialisierung
 if (fp.pKennung!=PARAKENNUNG) 		// Erstmalige Werkinitialisierung
	 fp.hcount=0;		 								// Betriebsstundenzähler auf Null
 fp.pKennung=PARAKENNUNG;					// Kennung Parameterblock
 clean_cpy (T_version, fp.pVersion, sizeof(fp.pVersion));				// Neue Programmversion nach Parameterblock 
 clean_cpy (T_def_hw_rev,fp.hwVersion,sizeof(fp.hwVersion));		// Default Hardware Revision setzen
 fp.i2cdev=0;											// Reset installierte I2C Baugruppen   
 fp.sprache=Def_sprache;					// Programmsprache 
 fp.psets=Def_paraset;						// Default Anzahl Parametersätze
 for (i=0;i<MAXTAST;i++) 					// Für alle definierbaren Tastverhältnisse
  fp.hell[i]=Def_hell;						// Helligkeit = Tastverhältniss einstellen
 fp.pwmex=Def_hell;								// Externes PWM Signal Tastverhältniss einstellen		
 fp.hoff=1;												// Helligkeitseinstellung Kunde  
 fp.farben= Def_farben;						// Anzahl LED Farben
 fp.mph=Def_mph;									// Einheit Geschwindigkeiten
 fp.uoff=Def_U_offset;						// Offset Batteriespannungsanzeige
 fp.pwm=Def_pwm;									// Default PWM Modus
 fp.s_w_zeit=0;										// Sommer-/Winterzeitumstellung aus
 fp.sz=0;													// Winterzeit
 fp.TxF=Def_TxF;									// Sendefrequenz
 fp.vspcam=Def_vspcam;						// viaspeedcam Modus setzen	
 fp.dmode=0;											// Anzeigemodus absolut
 fp.voff=0;												// Anzeigeoffset differentielle Anzeige
 fp.ex12=1;												// 12V extern Bereitstellung ein
 fp.vmm=Def_vmin;									// Default kleinste angezeigte Geschwindigkeit
 fp.symbol=8;											// 8 LED Anzeigesymbole
 fp.swgrp=0;											// Keine Schaltergruppen
 fp.symgr=3;											// 3 Symbolgruppen
 fp.defic58=Def_port_ic58;				// Porteinstellung IC58 Expander
 fp.defic68=0;										// Porteinstellung alter IC68 Expander, zur Zeit ungenutzt
 for (i=0;i<NOSWGR;i++) fp.nosw[i]=0;		// Reset Anzahl Schalter in Schaltgruppe
 wp=&fp.swgrname[0][0];
 do *wp++=0; while (wp < (char*)&fp.swgrp);				// Lösche Textfelder Schaltergruppen	 
 for (i=0;i<NOSW;i++) fp.swexp[i]=0;							// Reset alle Schalter Expander Einträge
 wp=&fp.symgrname[0][0];
 do *wp++=0; while (wp < (char*)&fp.symfont[0]);	// Lösche Textfelder Symbole
 for (i=0;i<3;i++) strcopy (&T_symgr1[i][0],&fp.symgrname[i][0]);	// Setze Led Anzeigesymbol Bezeichner Gruppe 1 und 2
 fp.nosym[0]=3;										// Drei Symbole in Gruppe 1	
 fp.nosym[1]=3;										// Drei Symbole in Gruppe 2
 fp.nosym[2]=2;										// Zwei Smiley Symbole in Gruppe 3
 for (i=0;i<fp.symbol;i++)				// definiere Symbole
 {
  strcopy (&T_defsym[i][0],&fp.symname[i][0]); 	//  Definiere Default Symbol(-namen)
  fp.symfont[i]=Def_sym_font[i];								// zugehörige Fonts eintragen
 }
 fp.fileno=0;											// Reset fortlaufende Dateinummer
 fp.btmodem=0;										// Reset installiertes Bluetooth Modem
 fp.gsm=0;												// Reset installiertes GSM/GPRS Modem
 fp.gps=0;												// Reset installiertes GPS Modul
 fp.usb=0;												// Reset installierte USB Schnittstelle(n)
 fp.ledspot=0;										// Reset installierte LED Blinklampe
 fp.sense=4;											// Sensitivity auf 100%
 fp.txrcon=Def_txrcon;						// Verstärker und Transceiver - Schaltregler auf 3V3 und ein   
 fp.turnsw=0;											// kein externer Drehschalter Parametersatzauswahl
 fp.gpsintv=Def_gpsintv;					// Setze default GPS Positionsfix Zeitinterval
 fp.gpsanz=0;											// Null GPS Positionsbestimmungen
 fp.gi=0;													// Index Positionsbestimmungen auf Null
 fp.radnet=2;											// Funknetz default automatische Auswahl
 fp.ledcode=0;										// Bestückte LED Farben
 fp.utc=1;												// Default UTC Zeitzone utc+1 (D,F,IT,...) 
 fp.mqtt_pdbq=0;									// Erweiterte Protokollierung insbesondere der MQTT Aktivität
 return (1);
}

void default_parameter (void)		// Setze überschreibbare Parameter auf Voreinstellung
{
  uchar i,l;										// Laufvariable

  fp.nk=0;												// Nachkommastellenausgabe  
  fp.bdir=0;											// Bidirektionale Messung      
  fp.mcycle=Def_mcycle;						// Messzykluszeit einstellen
  fp.acycle=2*Def_mcycle;					// Anzeigehaltezeit einstellen
  fp.scycle=3*BASECYCLE;					// Basiszyklus * 3
  fp.vcor=Def_vcor;								// Default Geschwindigkeitskorrekturfaktor laden  
  fp.pro_start_page=PROTOCOLPAGE;	// Protokollstartseite im Flash  
  fp.pro_page=PROTOCOLPAGE;				// Zeiger auf aktuelle Protokollseite im Flash
  fp.pro_adr=0;										// Reset Adresszeiger auf nächsten Protokolleintrag im Flash
  fp.md_start_page=MEASUREPAGE;		// Messdaten Startzeiger auf Messdatenstartseite im Flash
  fp.md_page=MEASUREPAGE;					// Messdaten Zeiger auf Messdatenstartseite im Flash
  fp.md_adr=0;										// Reset Messdaten Adresszeiger  
  fp.comment[0]=0;								// Kommentare reset  
  for (i=0;i<MAX_PARSET;i++)			// für jeden Parametersatz
  {
   fp.led[i]=1;										// LED Anzeige aktiviert
   fp.eintage[i]=0;								// Einschalttage
   fp.tein[i]=0;									// Einschaltzeit
   fp.taus[i]=0;									// Ausschaltzeit
   fp.vmin[i]=fp.vmm;							// kleinste angezeigte Geschwindigkeit
   fp.vmax[i]=Defmaxspeed;				// größte angezeigte Geschwindigkeit
   fp.vblk[i]=Defmaxspeed+1;			// Schwelle blinkende Anzeige aus
   fp.vcol[i]=Defmaxspeed+1;			// Schwelle Farbumschaltung aus
   fp.vmix[i]=Defmaxspeed+1;			// Schwelle Mischfarbe aus  
   fp.vlim[i]=Defmaxspeed+1;			// Tempolimit aus
   for (l=0;l<2*NOSYM;l++) fp.vsym[i][l]=Defmaxspeed+1;		// reset Ein-/Ausschaltschwellen Verkehrssymbole 30, 50,...
   for (l=0;l<(2*NOSW);l++) fp.vswi[i][l]=Defmaxspeed+1;	// reset Ein-/Ausschaltschwellen Schalter   
  } // end for each parameter set
  fp.eintage[0]=0x7F;							// Einschalttage im ersten Parametersatz ein
  MQTT_send=fp.md_page<<16;				// Setze MQTT Versandzeiger in GPREG2 auf Messdatenzeiger
  MQTT_index=0;										// Reset MQTT Sendeindex in GPREG3
  md_pg_adr=fp.md_page<<16;				// Setze Kopie Messdatenzeiger in GPREG4 auf Messdatenzeiger
}

int parameter_to_progmem (void)	// Schreibt Sicherungskopie des Parameterblocks in LPC1766 Programmspeicher
{
 uchar const command[5] = {50, 52, 53, 50, 51}; // Liste auszuführender IAP Kommandos
 uchar i=0;																			// Laufvariable 
 IAP iap_entry;																	//  Zeiger auf IAP Funktionsaufruf
	
 ResetWDT();												// Reset Watchdog
 iap_entry=(IAP) IAP_LOCATION;			// Funktionszeiger setzen
 __disable_irq();										// Interrupts sperren
 do
 {
  iap_data[0]=command[i++];					// Kommando und Parameterübergabe in Array
  if (i>=5) break;									// Ende wenn 5. Kommando 51 geladen
  iap_data[1]=7;										// Startsector
  iap_data[2]=7;										// Endsector
  iap_data[3]=CCLK/1000;						// Crystal in KHz
  iap_entry (iap_data, iap_data);		// IAP Funktionsaufruf
 } while (iap_data[0]==CMD_SUCCESS);

 if (i==5)	// Alle Kommandos mit Erfolg abgearbeitet
 {
  iap_data[1]=7*4*1024;							// Zieladresse Sektor 7 im ProgrammFlash
  iap_data[2]=(uint)&fp.Kennung;		// Quelladresse RAM
  iap_data[3]=4096;									// Anzahl Bytes zu schreiben
  iap_data[4]=CCLK/1000;						// Crystal in KHz
  iap_entry (iap_data, iap_data);		// IAP Funktionsaufruf    
 }

 __enable_irq();										// Interrupts wieder zulassen
 
 if (iap_data[0]==CMD_SUCCESS) return (1);	// Erfolg
 else return(0); 	   				// oder auch nicht
}

int InitParameter (uchar werkinit)	// Flash Parameterinitialisierung
{									// Übergabe werkinit=0 Einschaltinitialisierung, Werkinitialisierung nur bei falscher Kennung
									//					werkinit=1 Werkinitialisierung 			
									//					werkinit=2 Reset auf Standardparameter (Schaltschwellen, Zeitpläne, etc.)
 uint init=0;								// Rückgabe 1 bei Werkinitialisierung
 uchar retry=4;							// 3+1 Versuche 

 if ((!werkinit) && flashsize)	    // Einschaltinitialisierung und Flash vorhanden?
 {
	do					// Parameter aus Atmel Flash lesen
  {
   if (retry>1) flash_pcom(&fp.Kennung,PARAMETERPAGE,FLASH_R_ARRAY,sizeof(fp)); // Aus externem Flash lesen
	 else																													
   {		 
		memcpy ((char *)&fp.Kennung, &fp_cp_area[0], sizeof(fp)); 	// Im letzten Versuch Parameterkopie aus LPC Flash lesen
																																// Messdatenzeiger der LPC Parameterkopie sind eventuell veraltet, daher
		fp.md_page=md_pg_adr>>16;																		// NV GPREG Kopie des Messdatenzeigers verwenden 
		fp.md_adr=md_pg_adr;																				// Adressteil kopieren
		fp.md_start_page=fp.md_page;																// Sichtbare Messdaten löschen  
	 }	 
   if ((fp.Kennung==FLASHKENNUNG)&&(fp.pKennung==PARAKENNUNG)) break;		// Parameter Kennungen in Ordnung?
	 osDelay(100);
  } while (--retry);	// Bis zu 3 Versuche	
 } 
 
 if (!retry || (werkinit>0))	// Werkinitialisierung, Parameter reset oder falsche Kennung?
 {											
  if (!retry || (werkinit==1)) init=werkparameter ();	// Bei Kennung falsch oder Werkinitialiserung setze Werte der Werkinitialisierung   
	default_parameter ();																// Setze überschreibbare Parameter auf Voreinstellung 	
	if (werkinit==2) protocol(DEF_PARAMETER);	// Reset auf Default Parameter protokollieren 	
 } // end if Werkinitialisierung, Parameter reset oder falsche Kennung
 
 if (!parameter_to_progmem())						// Schreibt Sicherungskopie der Parameter in LPC1766 Programmspeicher
  puterror(DTC_FL_PARAM_INIT, -1);		// Parameter-Initialisierung fehlgeschlagen

 if (flashsize!=0)							// Flash vorhanden?
 {
	if (init) protocol(WERK_INIT);			// Werkinitialisierung ins Protokoll  
  else if (!werkinit) 								// Einschaltinitialisierung?
  {      
   flash_pcom (probuf,fp.pro_page,FLASH_R_ARRAY,FLASHKGV);				// Protokollseite lesen      
   flash_pcom (vbuf,fp.md_page,FLASH_R_ARRAY,FLASHKGV);						// Aktuelle Messdatenseite lesen
  }
  if (sizeof(fp)>(PARA_LEN)) puterror(PARAMETER_INIT_ERROR, -1); 	// Initialisierungsfehler Parameterarray zu groß 
 } 
 else puterror (DTC_FL_WRITE_FAIL, -1);	// Flash Fehler ausgeben und protokollieren
	
 fp.flash=flashsize;									// Inst. Speichergröße für Parameterblockausgabe bereit stellen  
 
 return (init);
}

void delete_data(void)			// Messdatenzeiger im Speicher löschen
{ 
 fp.md_start_page=fp.md_page;	// Aktuellen Messdatenseite reset auf Startseite
 fp.md_adr=0; 								// Reset Messdatenzeiger Adressteil
 MQTT_send=fp.md_page<<16;		// Reset MQTT Sendezeiger auf Messdatenzeiger
 md_pg_adr=fp.md_page<<16;		// Messdatenzeiger gleich setzen bzw. Adressanteil nullen
 mailtosend&=~(FULMEM|MEM95);	// reset Speicher voll Flags Email Versand
 smstosend&=~(FULMEM|MEM95);	// reset Speicher voll Flags SMS Versand
}

void delete_protocol(void)			// Protokollzeiger im Speicher löschen
{ fp.pro_start_page=fp.pro_page; fp.pro_adr=0; }	// Reset Zeiger

void read_flash (void)		// Terminalausgabe Flash Seite im Hexformat
{
 int pageno;								// Flashseite
 uint lenbytes=maxbyte;
 text *P_buf;

 redirect_char_Out(putb);		// Zeichenausgabe an UART(s) leiten
 do
 {
  pageno=getnumber(T_fpge,0,maxpage+6);	// Lese Seite 0..8191, Puffer 1/2 =8192/3, >= 8194-8197 LPC Flash
  if (pageno<0) break;
  else if (pageno>=maxpage+3) lenbytes=512;
 
  P_fpar=(uchar *)&cbuf;							// Pointer auf Pufferbeginn
  P_buf=cbuf;
  if (pageno<=maxpage)								// Seite im Hauptspeicher nach Flash SRAM Puffer 2 lesen
  {  
   flash_adress (0, pageno);					// Seite im Flash adressieren
   flash_command (FLASH_R_MAIN,8,maxbyte);	// Flash Seite direkt lesen
  }
  else																// SRAM Puffer des Flash lesen
  {
   flash_adress (0, 0);								// Byte 0 adressieren
   if (pageno==(maxpage+1))
    flash_command (FLASH_R_BUF1, 4, maxbyte);	// Atmel Flash SRAM Puffer 1 lesen
   else if (pageno==(maxpage+2))
    flash_command (FLASH_R_BUF2, 4, maxbyte);	// Atmel Flash SRAM Puffer 2 lesen
   else
   {
    P_buf=fp_cp_area;									// Zeiger auf Kopie Parameterblock im LPC1766 	
		P_buf+=512*(pageno-(maxpage+3));	// Seitenoffset addieren
   } 
  }  	
  sendbuf(P_buf, lenbytes);				// Puffer im Hexformat ausgeben

 } while (1);
}

uint anzahl_protokoll_ereignisse(void)		// Berechnet Anzahl der Protokollereignisse
{
 int anzpro;
 anzpro=fp.pro_page-fp.pro_start_page;	// Anzahl Protokollseiten
 if (anzpro<0) anzpro+=(1+PROTOCOLENDPG-PROTOCOLPAGE);
 anzpro*=BLOCKSIZE/pagepk/PSATZLEN;		
 anzpro+=fp.pro_adr/PSATZLEN;		    // Anzahl Protokollsätze
 return (anzpro);
}

uchar mdata_to_flash (void)						// Schreibt Messdaten in Flash, wenn Puffer voll
{
 uchar memfull=0;
	
 if (!(fp.md_page-fp.md_start_page)&&!fp.md_adr) protocol(MESSBEGINN);	
	
 fp.md_adr+=VSATZLEN;												// Byteadresse um Messdatensatzlänge erhöhen
 if (fp.md_adr>=BLOCKSIZE/pagepk)						// Flashseite bzw. pagepk*256 Bytes voll?
 {   																				// Gespeichert werden 260,520,1030 Bytes/page
  flash_wbuf1 (vbuf,fp.md_page,maxbyte);		// Messdatenseite speichern   
  if ((fp.md_page+1)>=PROTOCOLPAGE) fp.md_page=MEASUREPAGE;	// Messdaten Seitenzeiger am Pufferende?
  else fp.md_page++;												// Messdatenseite inkrementieren
  if (fp.md_page==fp.md_start_page) 				// Seitenzeiger Messdaten überschreibt Messdatenanfang?
  {
	 memfull=1;																// Speicher voll	
   if ((fp.md_start_page+1)>=PROTOCOLPAGE)	// Messdatenstartseite am Pufferende?
    fp.md_start_page=MEASUREPAGE;						// Messdatenstartseite auf Pufferanfang
   else fp.md_start_page++;									// Messdatenstartseite inkrementieren
  }   
  fp.md_adr=0;									// Reset Byteadresse
  flash_erase (fp.md_page, 1);	// aktuelle Messdatenseite löschen
 }	 // end Seite voll	
 md_pg_adr=(fp.md_page<<16)+fp.md_adr;			// Kopie Messdatenzeiger in GPREG4 sichern
 return (memfull);						// 1 wenn Daten im Messdaten Ringspeicher überschreiben 
}	

void store (uint speed, uint distance)	// Speicherung Geschwindigkeitsdaten im Flash
{
 vbuf[fp.md_adr]=speed>>8;							// Geschwindigkeit in Puffer kopieren
 vbuf[fp.md_adr+1]=speed;
 vbuf[fp.md_adr+2]=distance>>8;					// Entfernung in Puffer kopieren
 vbuf[fp.md_adr+3]=distance;
 read_date_time (DATE_HEX, (char *)&vbuf[fp.md_adr+4]);	// Datum in Puffer schreiben
 read_date_time (TIME_HEX, (char *)&vbuf[fp.md_adr+7]);	// Zeit in Puffer schreiben 

 if (flashsize!=0)											// Flash vorhanden?
 {
	if (mdata_to_flash ())								// Schreibe Messdaten in Flash
	 if (!((interfaces&GSM_LINK)&&(fp.servertyp==MQTT)))	// Im MQTT Serverbetrieb nicht protokollieren
	  protocol(MEMFULL);																	// Protokollmeldung Speicher voll, alte Messdaten werden überschrieben	 	
 } // end if Flash vorhanden 	 
}

/* uint mdata_per_page	(void)		// Anzahl Messwerte pro Flash Seite berechnen
{ return (((BLOCKSIZE/pagepk)/VSATZLEN)+1); } */

