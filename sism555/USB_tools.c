//-----------------------------------------------------------------------------
//  FILE: USB_tools.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  USB Routinen für LPC1766
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   06.11.2014
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//									13.07.2015: Hardware version H2 -> H3
//              	
//-----------------------------------------------------------------------------


#include "rl_usb.h"
#include "rl_fs.h"
#include <Driver_USART.h>
#include "hard.h"
#include "sio.h"
#include "rtc.h"
#include "fio.h"
#include "flash.h"
#include "sicom.h"
#include "sictxt.h"
#include "libtool.h"
#include "ramcode.h"


CDC_LINE_CODING LineCoding;									// VCP Übertragungsparameter

void USB_HostService (void const *argument)	// Initialisiert und startet ggf. USB Host Service
{
 int c;	
 
 while (1)
 {	 
  osSignalWait (1,osWaitForever);							// Warte auf Signalereignis 
	
  if (puterror (USB_ERROR, USBD_Initialize (0))==0)     	// USB Device 0 Initialization
	 {
    if (puterror (USB_ERROR, USBD_Connect (0))==0)       	// USB Device 0 Connect
    {		
		 connect|=USB_LINK;												// USB-Verbindung hergestellt, ab hier sendet putb()
		 concpy=connect;													// Kopie sichern		
#if (VSPCAM)
 		 if (fp.vspcam) online=0;									// viaspeedcam online Modus beenden		
#endif			
		 //putstr(T_usbcon);
     while (USBD_Configured (0)==0) osDelay(10);
		 putstr(T_usbcf);		 			
		 while (FIO1PIN&VBUS)										// Solange Host-Versorgung angeschlossen
		 {
			
			c=USBD_CDC_ACM_DataAvailable(0); 												// Daten verfügbar?
			if (c) {																								// Bytes vorhanden
				while (c--)
				{		
				 if (bxi!=rxi+1) rbuf[++rxi] = USBD_CDC_ACM_GetChar (0); 	// Byte(s) lesen		
				 else osThreadYield();			 															// Pufferüberlauf? -> Taskwechsel
			  }	}	
      else osThreadYield();																		// kein(e) Zeichen Taskwechsel 
		 } // end while VBUS
		 connect&=~USB_LINK;		 								// ab hier sendet putb() nicht mehr
		 concpy=connect;												// Kopie sichern
		 if (puterror (USB_ERROR, USBD_Disconnect(0))==0) putstr(T_usbdcon);
#if (VSPCAM)		 
		 if (fp.vspcam && (connect&UART0)) online=1; 	// viaspeedcam und RS232 Verbindung -> online Messmodus
#endif		 
    } // end if USB connect
		puterror (USB_ERROR, USBD_Uninitialize (0));	// Return Code bei Fehler ausgeben
		PINMODE3 |= (NOPULL<<28);											// Disable Pullup P1.30
   } 		   	 
	 FIO1CLR =	USB_PPWR;										// USB Device Versorgung ein
	 osSignalClear(usbh_thread_id,1); 
 } // end while 
}

bool USB_configured (void)		// Prüfe ob USB Host oder Client konfiguriert
{
if ((PINSEL1&(0x0F<<26))==0) return (FALSE);
 else return (TRUE); 	
}

int init_msd (char *letter) 	// Initialisiere und mounte Laufwerk
{
 fsStatus result;							// Ergebnis 
	
  if (finit (letter) == fsOK) // Initialisiere Dateisystem
	{ 
   result=fmount(letter);			// Mounte Laufwerk	
   if (result == fsOK) return (TRUE);	// Erfolg
	 else if (result == fsNoFileSystem) putstr (T_uMsd);
	 else puterror (USB_ERROR, result);		// Fehler fsStatus numerischen Wert ausgeben	
   funinit (letter);										// Dateisystem Ressourcen wieder freigeben						
  }
	
 return (FALSE);
}

bool flashdiskinfo (void)		// Ausgabe der USB Stick Eigenschaften
{
 //uint block_count = 0;											// Anzahl Speicherblocks 	
 //uint block_size = 0;												// Blockgröße	
 //uint serial = 0;														// Seriennummer
 uchar fattype;													
 fsDriveInfo fsinfo;
 int64_t dfree;
	
 /* (puterror(USBH_MSC_ReadCapacity(0, &block_count, &block_size))) return(false); 
					putstr (T_blocks);
					putnumber (block_count,0);
					putstr (T_bsize);
					putnumber (block_size,0);									
 if (puterror(fvol("U0:",cbuf,&serial))) return(false);
					putstr (T_vol);
					putstr(cbuf);
					putstr (T_ser);
					putnumber (serial,0); */
 if (puterror(USB_ERROR,finfo("U0:",&fsinfo))) return(false);
					switch (fsinfo.fs_type)
					{
						case fsTypeFAT12:	fattype=12; break;
						case fsTypeFAT16:	fattype=16; break;
						case fsTypeFAT32:	fattype=32; break; 
						default: fattype=0;
					}
					putstr (T_fsys);
					if (!fattype) putstr (T_unk);
					else { putstr (T_fat); putnumber(fattype,0); }
					//putstr (T_cap);
					//putnumber (fsinfo.capacity,0);
					putstr (T_free);
					dfree=ffree("U0:");										// freien Flash Speicher ermitteln
					if (dfree<0) { puterror (USB_ERROR, -dfree); return(false); }
					putnumber (dfree/1048576,0);  				// durch Mbyte teilen und ausgeben	
					putstr (T_MB);
 return (true);		 			
}

bool find_and_open (text * file_ending)				// Sucht und öffnet Datei SC_ALL oder SC_serno
{																							// im USB flash disc Verzeichnis /VIASIS
 																							// Übergabe file_ending - Dateiendung ".BIN" oder ".PAR"
 uchar i=0;	
 char	fname[16];															// Kopie Dateiname 

 while (i<2)																	// 2 mal suchen - nach Seriennummer.* und SC_ALL.*
 {
	osDelay(10);																 
	redirect_char_Out(putcbuf);									// Zeichenausgabe an Puffer	 
	cind=0;																			// reset Pufferindex 	
  if (i==0)	 					// Gerätespezifischer Dateiname - Seriennummer.*
  {
	 if (!isalpha(fp.serno[0]))									// Keine gültige Seriennummer weiter mit Globaldatei
    { i++; continue; }		
	 putstr (fp.serno);													// Gerätespezifischer Dateiname mit Seriennummer	
  } 		
	else								// Globaldatei - SC_ALL.*
	{
	 putstr(T_sc );															// Beginn Dateiname "SC_" nach Puffer
	 putstr(T_all);															// Globaldatei SC_ALL Text "ALL"	
  }		
	putstr(file_ending);												// Dateiendung
	cbuf[cind]=0;																// Dateinamen abschließen
	strcopy (cbuf, fname); 											// Dateiname nach fname kopieren
	redirect_char_Out(putb);										// Zeichenausgabe an Schnittstellen
	if (open_read_file()) 											// in cbuf spezifierte Datei gefunden	und offen?	 							
	{
	 redirect_char_Out(putcbuf);								// Zeichenausgabe an Puffer	
	 cind=0;																		// reset Pufferindex	
	 putstr(T_read);														// Text "Lese .. nach Puffer
	 putstr(fname);															// Dateinamen nach Puffer 	
	 cbuf[cind]=0;															// Dateinamen abschließen	
	 redirect_char_Out(putb);										// Zeichenausgabe wieder an Uarts		
	 return (TRUE);															// Erfolg und Ende   		
	}	
	i++;
 } // end while	 	
 return (FALSE);	
}		

void confirm_to_infofile (bool erfolg)			// Schreibt Texte "ok" oder "Fehler" in usbinfo.text
{
 redirect_char_Out(putcbuf);								// Zeichenausgabe an Puffer
 if (erfolg==true) putln(T_ok);							// Text "ok" oder
 else puterrstr (true);											// Text " Fehler" nach cbuf
 cbuf[cind]=0;															// Zeichenkette null terminieren
 write_cbuf_to_finf (); 										// Schreibe Pufferdaten nach usbinfo.txt
 cind=0;																		// Reset Pufferindex	
}

void infofile_eintrag (void)								// Erstellt usbinfo.txt Eintragskopf in cbuf 
{																						// Ausgabe erfolgt in open_infofile()
 redirect_char_Out(putcbuf);								// Zeichenausgabe an Puffer
 cind=0;																		// Reset Pufferzeiger	
 newline(); 
 putmstr(L_info);														// Kopfinformation ausgeben
 if (isalpha(*fp.serno)) putln(fp.serno);		// Seriennummer
 else newline();	
 send_date_time (2, 0);											// Datum und Zeit ausgeben	
 putstr(T_write);														// Text "Schreibe ... 		
 cbuf[cind]=0;															// Zeichenkette null terminieren	
}	

void USB_Client_service (void) 								// USB flash disc service
{																								
 int result;																	// Hilfsvariable
 uint dblocks=0;															// Hilfsvariable
	
 if (puterror (USB_ERROR, USBH_Initialize(0))==usbOK)				// Initialize USB Host 0 
 {
	putstr(T_ucon0); 															// Text "USB flash disc connected"		
	InitWatchdog(LONG_WD_32);											// Watchdog Interval auf 32 s setzen 
  	 
	while (1)
  {
	 osDelay (50);																									
	 if ( USBH_MSC_GetDeviceStatus(0) != usbOK) continue; 	// Prüfe MSD Device Status?	
	 
	 if (init_msd ("U0:")==TRUE)									// virtuelles MSD Laufwerk initialisieren	
	 {
		putstr(T_fldrv);														// Text "USB flash disc configured: "				
	  flashdiskinfo ();														// Ausgabe der USB Stick Eigenschaften					
	  infofile_eintrag ();												// Erstellt usbinfo.txt Eintragskopf in cbuf			
    redirect_char_Out(putb);										// Zeichenausgabe wieder an Uarts
		if (mkdir_viasis()== TRUE) 									// Erstelle Viasis Verzeichnis und USBINFO Datei
		{
		 if (open_infofile()==TRUE)									// Erstelle und/oder öffne  USBINFO Datei, schreibe Infos 
		 {
			result=send_vtf_file (messdaten(0), 2); 	// VTF Datei öffnen und schreiben
			if ((result==TRUE)&& (!((interfaces&GSM_LINK)&&(fp.servertyp==MQTT)))) delete_data();		// Erfolg und nicht im MQTT Betrieb? Ja, Messdaten im Speicher löschen 
			confirm_to_infofile (result); 						// Ergebnis VTF Datei nach usbinfo.txt	
			close_files (2);													// Schließe VTF Datei
			osDelay(10); 
			ResetWDT();	
			if (find_and_open(T_bin)) 	 							// Sucht und öffnet Datei SC_ALL oder SC_serno ".BIN"
			{
			 confirm_to_infofile (true);							// Datei öffnen Erfolg nach usbinfo.txt		
			 dblocks=read_usb_file (FIRMWARE_MAX_FILESIZE,0);	// USB Datei einlesen und in flash sichern
			 if (file_is_firmware())									// Prüfe ob es sich um Firmware Datei handelt		
			 {
				delete_protocol();											// Protokollzeiger im Parametersatz löschen
		    delete_data();													// Messdatenzeiger im Parametersatz löschen 
				i_reset=FIRM_UP_RES;										// Firmware Update Kennung setzen 
			 }
			 else puterror (IAP_ERROR_NO_ARM_CODE, -1);		// Keine Firmware Fehler ausgeben	
			}		
			close_files (4);													// Schließe ".BIN" Datei  
			ResetWDT(); 
			if (find_and_open(T_par)) 								// Sucht und öffnet Datei SC_ALL oder SC_serno ".PAR"
			{					
			 confirm_to_infofile (true);	  					// Datei öffnen Erfolg nach usbinfo.txt
			 result=read_usb_file (5, FREEPAGE);			// USB Datei einlesen und in flash sichern
			 redirect_char_Out(putb);									// Zeichenausgabe wieder an Uarts
			 result=Set_parameter (result);						// Lese USB Dateiparameter in Parameterblock
			 if (result==2 || (result==5 && ((fp.ex12==2)||(fp.ex12==4)))) // 2k oder 5k von plus oder Viatext empfangen?
			 {
				newline();															// An Terminal falls angeschlossen
				putln (T_parainit);											// Meldung "Parameter initialisation"..
				protocol(PARAMETER_INIT);								// protokollieren	
			 }
			 else puterror (PARAMETER_INIT_ERROR, -1); 	// Wenn Kennungen falsch oder Schreibfehler -> Fehler ausgeben	
			} 	
		  redirect_char_Out(putb);									// Zeichenausgabe wieder an Uarts
		  close_files (7);													// Schließe alle geöffneten Dateien 	  
			ResetWDT(); 
		  osDelay(100);						
		  funmount("U0:");													// unmounte Laufwerk
		  funinit("U0:");														// Dateisystem freigeben
		  ResetWDT();
		  osDelay(2000);				
	 	  break;  
		 }	 
	  }
	  else puterror(USB_ERROR, -1);
    funmount("U0:");														// Laufwerk freigeben		 
	 }	// end if init_MSD ok  	
	 funinit("U0:");															// Dateisystem freigeben	 
  } // end while (1)
	
  putstr (T_fldis);// Host ressourcen wieder freigeben	
	
	ResetWDT();
	osDelay(1000);
	newline();
	PINSEL3&=~USB_UP_LED;								// LED vom USB Controller abkoppeln
	FIO1DIR|=LED1;											// wieder als normalen Ausgang nutzen
  while (FIO0PIN&DPLUS)								// Solange der USB Stick aufgesteckt ist
	{
	 ResetWDT();												// Reset Watchdog	
	 if (FIO1PIN&LED1) FIO1CLR=LED1; 		// Toggle LED Zustand - blinkt
		else FIO1SET=LED1;		
	 osDelay(500);											// Warte 500 ms	
  }	
  if (i_reset==FIRM_UP_RES)						// Firmware Update?
	{	
   iap_programming (dblocks);					// IAP-Programmierung und Reset
	 puterror(IAP_PROGRAMM_ERROR, -1);	// Hier sollte der Programmzeiger nie ankommen -> IAP Programmierfehler		
	}	
	i_reset=SLEEP_RES;									// Setze Resetursache
	InitWatchdog(0);
	while (1);													// Warte auf Reset 
 } // end USB Hostinitialisierung
}

bool USBD_CDC0_ACM_SetLineCoding (CDC_LINE_CODING *line_coding) // Dummy setzt line coding info parameter 
{																																// nur für Parameterübergabe
// Called upon USB request to Set Line Coding.
// \param[in]   line_coding   pointer to \ref CDC_LINE_CODING structure.
// \return      true          set line coding request processed. 
// \return      false         set line coding request not supported or not processed.
	
	LineCoding.dwDTERate   = line_coding->dwDTERate;
  LineCoding.bDataBits   = line_coding->bDataBits;
  LineCoding.bParityType = line_coding->bParityType;
  LineCoding.bCharFormat = line_coding->bCharFormat;
	
	return true;
}

bool USBD_CDC0_ACM_GetLineCoding (CDC_LINE_CODING *line_coding) 	// Dummy
// Called upon USB request to Get Line Coding.
// \param[out]  line_coding   pointer to \ref CDC_LINE_CODING structure.
// \return      true          get line coding request processed. 
// \return      false         get line coding request not supported or not processed.	
{

  line_coding->dwDTERate   = LineCoding.dwDTERate;
  line_coding->bDataBits   = LineCoding.bDataBits;
  line_coding->bParityType = LineCoding.bParityType;
  line_coding->bCharFormat = LineCoding.bCharFormat;

  return true;
}

bool USBD_CDC0_ACM_SetControlLineState (uint16_t state) 	// Dummy
// Called upon Set Control Line State request.
// \param [in]  state         control line settings bitmap.
//                - bit 0: DTR state 
//                - bit 1: RTS state
// \return      true          set control line state request processed. 
// \return      false         set control line state request not supported or not processed.
{	
  // ToDo: add code for set control line state

  return true;
}

void USBD_CDC0_ACM_Initialize (void) // Dummy
// Called during USBD_Initialize to initialize the USB Device class.	
{
 LineCoding.dwDTERate 	= 	115200;
 LineCoding.bDataBits   =		ARM_USART_DATA_BITS_8;
 LineCoding.bParityType = 	ARM_USART_PARITY_NONE;
 LineCoding.bCharFormat =		ARM_USART_STOP_BITS_1; 
	
}

void USBD_CDC0_ACM_Reset (void) // Dummy
// Called upon USB Reset Event	
{
 
}

void USBD_CDC0_ACM_Uninitialize (void) // Dummy
// Called during USBD_Uninitialize to de-initialize the USB Device class.
{

}



