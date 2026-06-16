/* Header Datei für fio.c, Projekt LPC1766 Test */

#ifndef FIO_H_
#define FIO_H_

#include "rl_fs.h"

extern bool mkdir_viasis (void); 							// Wechsle ins und erstelle ggf. Viasis Verzeichnis 
extern int write_cbuf_to_finf (void); 				// Schreibe Pufferdaten nach usbinfo.txt
extern bool open_infofile	(void);							// Erstelle und/oder öffne  USBINFO Datei
extern bool open_vtf_file (void); 						// Erstelle und öffne VTF Datei
extern void close_files (uchar fileflag);			// Schließe geöffnete Dateien
extern int write_block (void);								// Schreibe 1 k Block auf USB-Stick
extern fsStatus fs_get_time (fsTime *ltime);	// Bereitstellung Zeit Dateiinformation
extern bool open_read_file (void);						// Öffne in cbuf spezifizierte Datei zum Lesen
extern int read_usb_file (uint maxblock, int pageoffset);		// offene USB MSD Datei lesen
#endif /* FIO_H_ */

