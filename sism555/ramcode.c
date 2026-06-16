//-----------------------------------------------------------------------------
//  FILE: ramcode.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für Ausführung im RAM 
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

void Init_SPI (void)					// SPI (SSP1) Interface einschalten und initialisieren
{
 PCONP|=PCSSP1;								// SSP1 power enable
 SSP1->CPSR=SSPCLKDIV;				// SSP Clock Teiler muss gerade sein zwischen 2...254
 SSP1->CR0=SSP8;							// SSP mit Master, CPOL=0, CPHA=0, 8 Bit
 SSP1->CR1=(1<<SSE);					// SSP Enable
 PINSEL0|=(MOSI|SCK|MISO);		// SSP1 - SCK1,MISO1,MOSI1	aktivieren
}

void Clear_SPI (void)					// Disable SPI
{
 PCONP&=~PCSSP1;	 						// SSP1 power disable
 PINSEL0&=~(MOSI|SCK|MISO);		// Deselect SSP1 Pinfunktionen 	
}	

void flash_adress (uint bytadr, uint seite)	// Schreibt dataFlash Byte und Page address
{																						// in Arbeitspuffer
 if (maxbyte==1056)			    		// AT45DB642
 {
  flbuf[1]=(uchar)(seite>>5); 	// page address upper 3 bit
  flbuf[2]=(uchar)(seite<<3); 	// page address lower 5 bit
 }
 else if (maxbyte==528)		    	// AT45DB321 und AT45DB161
 {
  flbuf[1]=(uchar)(seite>>6); 	// page address upper 2 bit
  flbuf[2]=(uchar)(seite<<2); 	// page address lower 6 bit
 }
 else	// maxbyte==264				AT45DB081, AT45DB041, AT45DB021 und AT45DB011 
 {
  flbuf[1]=(uchar)(seite>>7); 	// page address upper bit
  flbuf[2]=(uchar)(seite<<1); 	// page address lower 7 bit
 }
 flbuf[2]|=(uchar)(bytadr>>8); 	// bytadr upper 1/2/3 bit
 flbuf[3]=(uchar)bytadr;		// bytadr lower 8 Bit                                          	
}

uchar flash_ready (uint check)	// Warte bis flash status ready oder return status
{																// Übergabe check - 0 warte bis Flash Status ready oder max. 100 ms
 volatile ushort status=0;			//					1 return status
 uint rdelay;										// Antwortzeit in us

#if (SIMULATION==1)
	uint retry=1;
#else
 	uint retry=500;
#endif
	
 if (check) retry=10;						// beim prüfen nicht mehr als 10 Versuche = 2 ms	

 Init_SPI ();										// SPI initialisieren und verbinden
 
 while (SSP1->SR&RNE) status=SSP1->DR;		// Flush Empfangsfifo leeren 

 FIO4SET = CSFLASH1;		 				// CS reset, stoppe ggf. laufendes spi transmit
 	  
 while (retry--)								// wiederhole bis zu 100 mal
 { 
  rdelay=T1TC;														// Timer 1 Stand laden
  FIO4CLR = CSFLASH1;											// CS low, start spi transmit 
  while (SSP1->SR&BSY)										// warte SSP Sende FIFO leer
	 if ((T1TC-rdelay) >= 100) break;				// Kein Transfer in 100 us -> Abbruch sofort
  SSP1->DR=0xD7;													// Opcode Flash Statusregister in FIFO	  
  while (SSP1->SR&BSY)										// warte bis gesendet
   if ((T1TC-rdelay) >= 100) break;				// Kein Transfer in 100 us -> Abbruch sofort
  if (SSP1->SR&RNE) status=SSP1->DR;			// Dummy read Byte 
	
	rdelay=T1TC;														// Timer 3 Stand laden							
  SSP1->DR=0x00;													// Dummy Byte senden
  while (SSP1->SR&BSY) 										// warte bis gesendet
   if ((T1TC-rdelay) >= 100) break;	// Kein Transfer in 100 us -> Abbruch sofort
  if (SSP1->SR&RNE) 
  {
   status=SSP1->DR;							 					// Status empfangen?
   if ((status&FLASH_RDY)||(check>0)) break; // Flash Status ready oder nur prüfen 		
  }
  FIO4SET =CSFLASH1;			 								// CS reset, stoppe ggf. laufendes spi transmit
  while ((uint)(T1TC-rdelay)<100);				// sonst warte 100 us							
 }  	
 
 FIO4SET =CSFLASH1;			 	// CS reset, stoppe ggf. laufendes spi transmit
 return (status);				// Return Status code
}

int flash_command (uint opcode, uint adrbytes, uint txbytes)	// Flash data transfer
{                              	// Übergabe opcode LSB: dataflash operation code	
																// Achtung: opcode MSB > 0 bei Lesevorgang							
																// 			txbytes: Transferbytes schreiben/lesen
																//			adrbytes: Anzahl = Opcode+Adressbytes+Dummybytes
																// P_fpar muss vor Aufruf auf Datenbytes zu lesen/schreiben zeigen
 uchar *sbp=&flbuf[0];		// Flash Adress- und Dummybytes immer in flbuf[] 
 int erg=-1;							// Rückgabeergebnis
 uchar result;
	
 NVIC_DisableIRQ(PWM1_IRQn);						// Disable PWM1 Interrupt	

 result=flash_ready(0);					
 
 if ((result&FLASH_RDY)&&(!(result&0x03)))	// Bereit Status?
 {
  *sbp=opcode;								// Opcode in Puffer schreiben
  FIO4CLR =CSFLASH1;					// CS low, start spi transmit 

  adrbytes+=txbytes;					// Summe Bytes zu übertragen
  while (adrbytes)						// Zeichen zu übertragen? 
  {
   while (SSP1->SR&BSY);			// solange bis SSP Sende FIFO nicht voll
   SSP1->DR=*sbp;							// Sendebyte in FIFO
   while ((SSP1->SR&BSY)||(SSP1->SR&!RNE));	// warte bis gesendet und empfangen
   if (opcode>0x00FF) *sbp=SSP1->DR;				// Empfangsbyte in Puffer schreiben
   else result=SSP1->DR;										// flush Empfangsbyte
   sbp++;																		// inkr. Pufferpointer  
   if (--adrbytes==txbytes) sbp=P_fpar;			// Adress-/Dummybytes gesendet?  
  }
  erg=1;			// Erfolg
 }  
 FIO4SET =CSFLASH1;						// CS reset, stop spi transmit
 Clear_SPI ();								// Disable SPI
 NVIC_EnableIRQ(PWM1_IRQn);		// Enable PWM1 Interrupt 
 
 return (erg);
}

int sector_adress (int kblock)		// Bestimmung der Sektoradresse
{
 uint sector;
 if (kblock<=64) sector=kblock/4;		// Sektornummer unterhalb 64k, Sektorgröße 4k
 else sector=15+((kblock/32)-1);		// Sektornummer oberhalb 64k, Sektorgröße 32k
 return (sector);
}

void iap_programming (uint blocks)			// Programmiert LPC7166 Programmspeicher
{																				// Übergabe: blocks - Anzahl 1k Block zu schreiben
 int i;													// Laufvariable
 char retry=IAP_RETRY_LIMIT;		// Maximale Anzahl Wiederholungen
 uchar startsector, endsector;	// Hilfsvariable für Sektorenindizes 
 IAP iap_entry;									//  Zeiger auf IAP Funktionsaufruf
	
 __disable_irq ();							// IRQ Interrupts sperren
 WDTC=LONG_WD_32*(RCCLK/4/1000);	// Timerwert für 32s watchdog timeout berechnen
 WDMOD=WDEN | WDRESET;						// enable Watchdog and Reset
 WDFEED=0xAA;											// Timed access watchdog reset
 WDFEED=0x55;
	
 iap_entry=(IAP) IAP_LOCATION;	// Funktionszeiger setzen	
 P_fpar=(uchar *)cbuf;					// Pointer SRAM Parameter Puffer

 while (retry--)										// Bis zu 8 Versuche
 {
	for (i=0;i<=1;i++)										// Beim Löschen Sektor 7 auslassen, daher 2 Durchläufe
  {	    
	 if (i)																// 2. Durchlauf 9 bis letzter benötigter Sektor löschen
   {		
    startsector=8;											// Startsektor 2. Durchlauf	 
		endsector=sector_adress (blocks-1);	// Endsektor bestimmen
	 }
   else	{ startsector=0; endsector=6; }	// 1. Durchlauf Sektor 0 bis 6 löschen
	 
   iap_data[0]=50;											// Prepare Sector for Erase
   iap_data[1]=startsector;							// Startsector
   iap_data[2]=endsector;								// Endsector
   iap_entry (iap_data, iap_data);			// IAP Funktionsaufruf  
   iap_data[0]=52;											// Erase Sectors
   iap_data[1]=startsector;							// Startsector
   iap_data[2]=endsector;								// Endsector
   iap_data[3]=CCLK/1000;								// CPU Clock in KHz
   iap_entry (iap_data, iap_data);			// IAP Funktionsaufruf
   if (iap_data[0]!=CMD_SUCCESS) break;	// verlasse for
	 iap_data[0]=53;											// Blank check Sectors
   iap_data[1]=startsector;							// Startsector
   iap_data[2]=endsector;								// Endsector
   iap_entry (iap_data, iap_data);			// IAP Funktionsaufruf
	 WDFEED=0xAA;													// Timed access watchdog reset
   WDFEED=0x55; 
	 if (iap_data[0]!=CMD_SUCCESS) break; // verlasse for
	}	// end for
	if (iap_data[0]!=CMD_SUCCESS) continue;					// Löschvorgang kein Erfolg -> Wiederholen
 
  for (i=0;i<blocks;i++)													// Alle übertragenen Programmblöcke schreiben
  { 
	 startsector=sector_adress (i);									// Sektoradresse bestimmen
	 if (startsector!=7)														// Sektor 7 nicht beschreiben	
   {	
    flash_adress(0,pagepk*i);											// Seitenadresse setzen
    flash_command (FLASH_R_ARRAY, 8, BLOCKSIZE);	// Atmel Flash Seite(n) lesen    
    iap_data[0]=50;																// Prepare Sector for RAM copy   
    iap_data[1]=startsector;											// Startsector
    iap_data[2]=startsector;											// hier gleich Endsector
    iap_entry (iap_data, iap_data);								// IAP Funktionsaufruf
    iap_data[0]=51;																// Copy RAM to ProgrammFlash
    iap_data[1]=i*1024;														// Zieladresse im ProgrammFlash
    iap_data[2]=(uint)P_fpar;											// Quelladresse RAM Puffer
    iap_data[3]=BLOCKSIZE;												// Anzahl Bytes zu schreiben
    iap_data[4]=CCLK/1000;												// CPU Clock in KHz
    iap_entry (iap_data, iap_data);								// IAP Funktionsaufruf
    if (iap_data[0]!=CMD_SUCCESS) break;	
   }	// end nicht Sektor 7	 
	 WDFEED=0xAA;																	// Timed access watchdog reset
   WDFEED=0x55;	
  }	// end for 
  if (iap_data[0]==CMD_SUCCESS) break;		
 } // end while

 if (retry>0)						// Erfolg?
 {  
	i_reset=FIRM_UP_RES;			// Resetursache Firmware update	
	WDTC=0; 									//
  WDMOD=WDEN | WDRESET;			// Enable Watchdog	 
	WDFEED=0xAA;							// Timed access
  WDFEED=0x55;
  while (1);								// warte auf Watchdog reset		
 }
 else __enable_irq ();	// sonst IRQ Interrupts wieder freigeben
}


