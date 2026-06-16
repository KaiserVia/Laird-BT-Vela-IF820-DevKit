/* Header Datei für usb_tools.c, Projekt sis3000M	*/


#ifndef USB_TOOLS_H_
#define USB_TOOLS_H_

#include <rl_usb.h>

extern CDC_LINE_CODING LineCoding;				// USB Info serial line coding

extern void USB_HostService (void const *argument);	// Initialisiert und startet ggf. USB Host Service
extern bool USB_configured (void);									// Prüfe ob USB konfiguriert
extern void USB_Client_service (void);							// USB Flask Disk Bearbeitung

extern bool USBD_CDC0_ACM_SetLineCoding (CDC_LINE_CODING *line_coding); // Dummy setzt line coding info parameter
extern bool USBD_CDC0_ACM_GetLineCoding (CDC_LINE_CODING *line_coding);	// Dummy lädt line coding info parameter
extern bool USBD_CDC0_ACM_SetControlLineState (uint16_t state); 				// Dummy für hardware flow control UART
extern void USBD_CDC0_ACM_Initialize (void);													  // Dummy 
extern void USBD_CDC0_ACM_Reset (void);
extern void USBD_CDC0_ACM_Uninitialize (void);													 // Dummy

#endif		// end if USB_TOOLS_H_



