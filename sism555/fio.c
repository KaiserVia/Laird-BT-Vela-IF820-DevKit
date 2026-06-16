//-----------------------------------------------------------------------------
//  FILE: fio.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Stream I/O Routinen basierend auf <stdio.h>
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

#include <stdio.h>                      /* Standard I/O .h-file               */
#include "rl_fs.h"
#include "sictxt.h"
#include "xmodem.h"
#include "flash.h"

FILE *finf=NULL;								// Zeiger auf USBINFO.TXT Datei
FILE *fvtf=NULL;								// Zeiger auf VTF Datei
FILE *frd=NULL;									// Zeiger auf Lesedatei, d. h. Parameter oder Firmware Datei

bool mkdir_viasis (void) 				// Wechsle ins, bzw. ggf erstelle Viasis Verzeichnis
{
 int retry=5;										// 5 Versuche ins Verzeichnis wechseln und/oder es zu erstellen
	
 while (retry--)								// Dekrementiere Versuche
 {
	if (fchdir(T_viasis)==fsOK)  break;
	fmkdir (T_viasis);						// VIASIS Verzeichnis erstellen
	osDelay(10);									// 10 ms warten 
 }
 
 if (retry)															// Ins Verzeichnis gewechselt
	if (ffree("U0:")> (int64_t)BLOCKSIZE)	// freien Speicher auf USB Stick ermitteln
		return (TRUE);											// Rückgabe Erfolg
	
 return  (FALSE);		// Misserfolg - Verzeichnis nicht vorhanden, zu wenig Platz ...
						
}

int write_cbuf_to_finf (void) //Schreibe Pufferdaten nach usbinfo.txt
{
 int result;
 result=fputs (cbuf,finf);
 fflush(finf);	
 return(result); 
}	

bool open_infofile (void)	 // Erstelle und/oder öffne  USBINFO Datei
{
 if ((finf = fopen(T_fusbi,"a"))!=NULL)  	// Erstelle USBINFO.TXT Datei
	if (write_cbuf_to_finf()>=0)						// Puffertext "VIASIS 3003M...  in usbinfo.txt Datei  		 	
	 return (TRUE);	 // Erfolg
 return (FALSE);	// kein Erfolg
}

void close_files (uchar fileflag)					// Schließe geöffnete Dateien
{																					// Übergabe fileflag - 	bit 0 Infodatei
																					//											bit 1 VTF Datei
																					//											bit 2 Lesedatei
	
 if (fileflag&1) if (finf!=NULL) fclose (finf);			// Schließe USBINFO.TXT Datei, falls offen 
 if (fileflag&2) if (fvtf!=NULL) fclose (fvtf);			// Schließe VTF Datei wieder	
 if (fileflag&4) if (frd!=NULL) fclose (frd);				// Schließe geöffnete Lesedatei		
}	

bool open_vtf_file (void) // Erstelle in cbuf spezifizierte VTF Datei 
{
 write_cbuf_to_finf();										// Dateinamen in usbinfo.txt schreiben	
 if ((fvtf = fopen(cbuf,"w"))!=NULL)  		// Erstelle VTF Datei mit Schreibzugriff
 	return (TRUE); 			// Erfolg 
 return (FALSE);	// kein Erfolg	 
}	

int write_block (void)		// Schreibe 1 k Block auf USB-Stick
{																
 if (fwrite (cbuf,BLOCKSIZE,1,fvtf)==1)		// Block, 1024 Byte schreiben
  return (1);															// Erfolg
 return (USB_WRITE_ERROR);								// Schreibfehler, Misserfolg
}	

fsStatus fs_get_time (fsTime *ltime)		// Bereitstellung Zeit Dateiinformation
{
 ltime->hr=HOUR;
 ltime->min=MIN;
 ltime->sec=SEC;
 ltime->day=DOM;
 ltime->mon=MONTH;
 ltime->year=YEAR;	
 return (fsOK);
}

bool open_read_file (void)	// Öffne in cbuf spezifizierte Datei zum Lesen
{
 if ((frd = fopen(cbuf,"r"))!=NULL) 			// Versuche Datei zu öffnen
	 return (TRUE); 												// Erfolg	
 return (FALSE);													// kein Erfolg
}	

int read_usb_file (uint maxblock, int pageoffset)	// offene USB MSD Datei lesen, Daten in onboard Flash schreiben
{																									// maxblock 	- max. Anzahl gelesener 1k Blocks
 uint result;																			// pageoffset -	Seitenoffset Flash Speicher
 uint blocks=0; 	
 do
 {
	P_fpar=(uchar *)&cbuf;													// Pointer auf erstes Byte in SRAM Puffer 
	result=fread(P_fpar,1,BLOCKSIZE,frd);						// 1k aus Lesedatei nach cbuf
  if (result)
	{	
   flash_pcom(P_fpar,(blocks*pagepk)+pageoffset,FLASH_ERASE_WRITE_1,BLOCKSIZE);	// Flash Seite(n) je nach Seitengröße beschreiben	 	 
	 blocks++; 	
	}	
 } while ((result==BLOCKSIZE)&&(blocks<maxblock));
 
 return (blocks);			// Rückgabe Anzahl gelesener 1k Datenblocks								
}	
