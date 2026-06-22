//-----------------------------------------------------------------------------
//  FILE: btio.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen für Bluetooth Modem Erweiterung
//-----------------------------------------------------------------------------
//  HARDWARE:   sis3003-MB, version H2
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   23.03.2015
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:			22.04.2015:	Hardware version H1 -> H2
//											13.07.2015: Hardware version H2 -> H3
//              	
//-----------------------------------------------------------------------------

#include "libtool.h"
#include "btio.h"
#include "gsmio.h"
#include "sio.h"
#include "sictxt.h"
#include "string.h"
#define DTCBASE 10000
#include "dtc.h"

bool bt_command (text * command, text * answer, int pause) // Bluetooth Kommando senden, Antwort empfangen
{																						// Übergaben:	command	-	 Zeiger auf Kommandostring
																						//						answer	- Zeiger auf erforderliche Antwortsequenz
																						//						|pause|	- Wartezeit auf und nach Antort
																						//						pause > 0 Kommandosequenz samt Antwort ausgeben
																						//						pause < 0 Nur Antwort ausgeben
																						//						pause = 0 keine Wartezeit nach Antwort, keine Ausgabe
																						//						Wenn |pause| > 10000 dann CRLF bei Ausgabe und |pause|=|pause|-10000
																						
 int result;															// Hilfsvariable Ergebnis Antwortsequenz	
 uchar clen=0;														// Zähl- und Laufvariable
 uint timeout=BT_CHAR_TIMEOUT;						// Minimalwartezeit
 uchar addcrlf=0;	
	 	 
 osDelay (2);															// Warten
 MUX_BT_Select;														// MUX Bluetooth Leitungsauswahl	
 osDelay (2);															// Warten	
	
 while (*(command+clen)!=0) clen++;				// Ermittle Kommandolänge
	
 result = pause;
 if (result <0) result=-result;						
 if (result > 10000) { addcrlf=1; result-=10000; }
 if (pause) timeout=result;
	 
 if (pause>0) connect|=UART1;							// Ausgabe an bisherige Verbindungen und an UART1	
 else connect=UART1;											// Ausgabe nur an UART1
 putstr (command);												// Kommando senden			
 
 bxi=rxi;																	// Indizes Empfangspuffer gleichsetzen 
 result =	wait_message (answer,timeout);	// Warte auf Antwort	

 osDelay (2);															// Warten	
 connect=concpy;	 												// ursprüngliche Verbindung wiederherstellen
 
 if (result>=0)				// Antwort erhalten	
 {	
  if (pause>0) 
	{	
	 osDelay (timeout/5);								// Wartezeit nächster Befehl
	 putstr (cbuf);											// Sende- und Antwortsequenz an Terminal senden     
	 if (addcrlf) newline();						// RN4678 neue Zeile anfügen	
	 connect|=UART1;										// Ausgabe wieder an BT Modem umleiten
	}	
  else if (pause<0) putxchr (cbuf+2, cind-6);		// Antwortsequenz senden ohne OK		 									 
 }	 

 if (result<0) return (FALSE); 
 return (TRUE); 						
}	

bool Init_BT_ch (uint baudrate)			// Konfiguriere uart1 und setze MUX Kanal auf BT modem 
{																// Übergabe Baudrate - Baudrate nur 9600 oder 115200
 uart1_request(MUX_BT, baudrate);		// State machine: MUX switch + UART1 config
 if ((FIO2PIN&CTS)==0) return (TRUE);		// CTS Low BT Modem empfangsbereit?	
 return(FALSE);	// nicht bereit
}

//-----------------------------------------------------------------------------
// Generische Flow-Control-Verwaltung (RTS/CTS) - je Modul ein case
//-----------------------------------------------------------------------------
int bt_get_flowcontrol (void)        // Abfrage: -1=unbekannt, 0=aus, 1=an
{
 char *p;
 switch (fp.btmodem)
 {
  case IF820:
   if (!bt_command(T_gtu,T_gtu_d,300)) return (-1);   // GTU senden, Antwort bis ",D=" in cbuf
   p = strstr(cbuf,",F=");                            // Flow-Control-Feld suchen
   if (!p) return (-1);
   return ((p[3]=='0' && p[4]=='1') ? 1 : 0);         // "F=01" -> an
  case Roving:  return (-1);   // TODO: RN4678 Flow-Control-Abfrage
  case Laird:   return (-1);   // TODO: Laird ATS-Register-Abfrage
  default:      return (-1);   // unbekanntes/neues Modul
 }
}

bool bt_set_flowcontrol (void)       // RTS/CTS modulspezifisch aktivieren (+ persistieren)
{
 switch (fp.btmodem)
 {
  case IF820:
   if (!bt_command(T_stuf1,T_stuok,300))  return (FALSE);   // STU ...F=01 (ins RAM)
   if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);   // /SCFG (RAM -> Flash)
   return (TRUE);
  case Roving:  return (FALSE);   // TODO: RN4678 Flow-Control-Befehl
  case Laird:   return (FALSE);   // TODO: Laird Flow-Control-Register
  default:      dtcerr(E_bt); return (FALSE);   // Fangnetz: neues Modul nicht still durchwinken
 }
}

bool bt_ensure_flowcontrol (void)    // generisch: pruefen, bei Bedarf setzen, verifizieren
{
 if (bt_get_flowcontrol()==1)                               // schon aktiv?
 { putln(" Flow Control: bereits aktiv"); return (TRUE); } // -> kein Flash-Write noetig
 if (!bt_set_flowcontrol())
 { dtcerr("Flow Control: setzen fehlgeschlagen"); newline(); return (FALSE); }
 if (bt_get_flowcontrol()==1)
 { putln(" Flow Control: aktiviert und gesichert"); return (TRUE); }
 dtcerr("Flow Control: Verifikation fehlgeschlagen"); newline(); return (FALSE);
}

//-----------------------------------------------------------------------------
// Generische Device-Name-Verwaltung  (Name = VIASIS_<serno>)
//-----------------------------------------------------------------------------
int bt_get_name (void)               // 1 = Name == VIASIS_<serno>, 0 = anders, -1 = unbekannt
{
 char want[48];
 switch (fp.btmodem)
 {
  case IF820:
   strcpy(want,"N="); strcat(want,T_viasis); strcat(want,"_"); strcat(want,fp.serno);
   if (!bt_command("GDN,T=00\r",want,300)) return (0);   // BLE-Name == VIASIS_<serno>?
   strcat(want,"_BT");                                   // Classic-Name = VIASIS_<serno>_BT
   if (!bt_command("GDN,T=01\r",want,300)) return (0);   // BT-Classic-Name == VIASIS_<serno>_BT?
   return (1);
  case Roving:  return (-1);   // TODO: RN4678 GN-Abfrage
  case Laird:   return (-1);   // TODO: Laird AT+BTN? Abfrage
  default:      return (-1);
 }
}

bool bt_set_name (void)              // Name = VIASIS_<serno> schreiben (SDN$ -> RAM+Flash)
{
 char cmd[56];
 switch (fp.btmodem)
 {
  case IF820:
   strcpy(cmd,"SDN$,T=00,N="); strcat(cmd,T_viasis); strcat(cmd,"_"); strcat(cmd,fp.serno); strcat(cmd,"\r");
   if (!bt_command(cmd,T_sdnok,500)) return (FALSE);   // BLE-Name setzen
   strcpy(cmd,"SDN$,T=01,N="); strcat(cmd,T_viasis); strcat(cmd,"_"); strcat(cmd,fp.serno); strcat(cmd,"_BT"); strcat(cmd,"\r");
   if (!bt_command(cmd,T_sdnok,500)) return (FALSE);   // BT-Classic-Name setzen
   {                                                   // Modul-Reboot: Classic-Name geht erst nach Boot live
    uchar cc=connect, w;
    connect|=UART1; putstr(T_rbt); connect=cc;         // /RBT senden
    bxi=rxi;
    for (w=0; w<5; w++) { osDelay(1000); ResetWDT(); putc('.'); if (bxi!=rxi) break; }  // auf Boot warten
    newline(); osDelay(300); bxi=rxi;                  // setteln + RX leeren
   }
   return (TRUE);
  case Roving:  return (FALSE);   // TODO: RN4678 SN,
  case Laird:   return (FALSE);   // TODO: Laird AT+BTN/AT+BTF
  default:      dtcerr(E_bt); return (FALSE);
 }
}

bool bt_ensure_name (void)           // generisch: pruefen, bei Bedarf schreiben, verifizieren
{
 if (!fp.serno[0]) { dtcerr("BT-Name: keine Seriennummer"); newline(); return (FALSE); }
 if (bt_get_name()==1) { putln(" BT-Name: bereits gesetzt"); return (TRUE); }   // kein Flash-Write
 if (!bt_set_name())   { dtcerr("BT-Name: setzen fehlgeschlagen"); newline(); return (FALSE); }
 if (bt_get_name()==1) { putln(" BT-Name: gesetzt und gesichert"); return (TRUE); }
 dtcerr("BT-Name: Verifikation fehlgeschlagen"); newline(); return (FALSE);
}

void init_bluetooth (void)		// Bluetooth Modem initialisieren
{ 		
 int32_t result=0;	
 uint32_t pin=0;	
 uint8_t retry=4;
	
 ResetWDT(); 
 if ((connect&UART1)|(fp.btmodem!=0))	return;	// Zugriffskonflikt - z. B. Konfiguration per GSM oder bereits konfiguriert
 fp.btmodem=0;												// Reset Bluetooth installiert	
 // === IF820 (EZ-Serial) Erkennung ueber Normalpfad (115200) ===
 if (Init_BT_ch(115200))                // MUX=BT, UART1=115200, CTS low (Modul bereit)?
 {
  if (bt_command(T_ping,T_pingok,300))  // /PING senden, auf "@R,...,/PING,0000" warten
  {
   fp.btmodem=IF820;                    // IF820 erkannt
   interfaces|=BT_LINK;                 // BT-Interface aktiv
  }
 }
 if (fp.btmodem==IF820)                 // erkannt -> Laird/RN4678-Kaskade ueberspringen
 {
  put2str(T_bt,T_if820); putstr(T_dpkt); putnumber(115200,0); newline();
  bt_ensure_flowcontrol();              // generisch: RTS/CTS sicherstellen (Query+Set+Verify)
  bt_ensure_name();                     // generisch: BT-Name VIASIS_<serno> sicherstellen
  uart1_release(MUX_BT);
  connect=concpy;
  osDelay(10);
  clear_comchange();
  return;
 }
 // === Ende IF820-Erkennung; sonst weiter mit Laird/RN4678 ===
 if (!Init_BT_ch(460800)) 							// Konfiguriere uart1 und setze MUX Kanal auf BT modem, Abbruch bei CTS hi
 { newline(); dtcerr(E_bt); return; }	 // putnumber(FIO2PIN,0x80);
 pin=atoi(fp.serno+4); 								// letzte 4 Digits der Seriennummer auswerten	
 if (!isdigit(fp.serno[0]) | !pin)    // Abbruch wenn Seriennummer nicht gesetzt oder letzte 4 Digits null
 { newline(); dtcerr(E_errser); newline(); return; } 	
 fp.btpin=atoi(fp.serno)*10000+pin;		// 6 stellige Pinnummer setzen
 
 if ((FIO2PIN&CTS)==0)								// CTS Hi Modem empfangsbereit?	
 {	
	put2str(T_bt,T_laird);							// "Bluetooth Laird" Modultyp ausgeben
	putstr(T_dpkt); putnumber(460800,0); newline();	// Startbaudrate anzeigen
  while (retry--)											// Versuche Laird mit 460800, 115200, 9600, 307200
  {																			// Versuche Laird mit AT Kommandos anzusprechen
	 result=bt_command(T_F1,T_OKNZ,300);	// Sende AT&F1 - Reset auf Werkseinstellung, 
   if (result)	break;									// OK erhalten?
   connect=UART0;
   newline();														// Neue Zeile für nächstes Kommandos		
	 if      (retry==3) { Init_UART1 (115200); putnumber(115200,0); newline(); }	// teste 115200 Baud
	 else if (retry==2) { Init_UART1 (9600);   putnumber(9600,0);   newline(); }	// teste 9600 Baud
	 else if (retry==1) { Init_UART1 (307200); putnumber(307200,0); newline(); }	// teste 307200 Baud
	 connect=UART1;															// Nur an UART1 senden
	 putstr(T_AT);															// Sende AT\r um Modul AT Parser zurueckzusetzen
	 connect=concpy;														// Verbindung wiederherstellen
	 osDelay(300);															// Warte auf Modul-Antwort (OK oder ERROR)
	 bxi=rxi;																		// Empfangspuffer leeren
  }	
	
  if (result) 												// Antwort erhalten? Ja --> Laird BT
  { 	
   Init_UART1 (9600);									// mit niedriger Baudrate weiter
	 result=FALSE;											// reset Ergebnis
	 if (bt_command(T_ATE0,T_OKNZ,500)) 	// Sende ATE0, ok erhalten?	
   {				
	 putstr (T_S521);												// Sende ATS521=460800 neue Baudrate
	 osDelay(3);																// Warte 3 ms bis letztes Bit gesendet	
	 Init_UART1 (460800);												// Initialisiert UART1 auf neue Baudrate
	  result = wait_ok_time (300);			// OK Antwort erhalten?
	  connect&=~UART1;									// nicht an BT Modem senden
    putstr(cbuf);		  								// Ergebnis an Terminal
   } // end if Echo aus ok		
  } // end if Antwort erhalten 
 
  if (result>0) {  													// Laird, weiter mit hoher Baudrate	
   if (bt_command(T_S502,T_OKNZ,100)) {			// Sende ATS502=1, Authentifizierung für eingehende Verbindungen
    if (bt_command(T_S504,T_OKNZ,100)) { 		// Sende ATS504=1, Autoresponse, suppress messages
		 if (bt_command(T_S507,T_OKNZ,100)) { 	// Sende ATS507=2, do not scan for ^^^ local mode ESC sequence
		  if (bt_command(T_S508,T_OKNZ,100)) {	// Sende ATS508=1000,	page scan interval 1 sec.
			 connect|=UART1;											// Sende an Terminal und BT Modem
			 putstr(T_BTK);											// AT+BTK="
			 putnumber(fp.btpin,0xC4);					// 4 stellige Pinnummer
			 putc('"');													// Abschluss-Quote
			 bxi=rxi;
			 putc(CR);													// Kommandoabschluss
			 if (wait_ok_time(300)>0)						// OK erhalten? Setze PIN
		   {	
			  connect=concpy;
			  putstr(cbuf);											// Ergebnis an Terminal
			  connect|=UART1;										// Sende an Terminal und BT Modem
			  putstr (T_BTN);								 			// Baue "Bluetooth device (friendly) name" auf
			  putstr (T_viasis);									// Text "VIASIS"		
			  putc ('_');													// Understrich
			  putstr (fp.serno);									// Serienummer
			  putc ('"');
			  bxi=rxi;													// Empfangspuffer leeren vor CR
			  putc(CR);		
			  result = wait_ok_time (300);				// OK Antwort erhalten?
			  osDelay(100); 
			  connect=concpy;
			  putstr(cbuf); 
       } } } } } } // end if result>0 			 
			 
  if (result>0) {	
	 if (bt_command(T_S514,T_OKNZ,100)) {			// Setze Timeout Verbindungsschlüssel auf 60s, OK erhalten?	
	  if (bt_command(T_S512,T_OKNZ,100)) {		// Sende ATS512=4 Modem sicht- und verbindbar, OK erhalten?
     if (bt_command(T_S538,T_OKNZ,100)) {		// Sende ATS538=1 Sichere last pairing in data base, OK erhalten?		 
		  if (bt_command(T_ATW,T_OKNZ,1000)) { 	// Sende AT&W - sichere Parameter, OK erhalten?
       if (bt_command(T_ATZ,T_OKNZ,2000))  	// Sende ATZ - Reset mit neuen Parametern, OK erhalten? 			 
		   {
				ResetWDT();													// watchdog reset	
				fp.btmodem=Laird;										// Laird BT modem gefunden und konfiguriert
				interfaces|=BT_LINK;								// BT interface ist aktiv	
				} } } } } } // end if result>0
 
  if (fp.btmodem==0)												// Kein Laird erkannt -> prüfen ob RN4678 vorhanden
	{
	 if (Init_BT_ch(115200)) 									// CTS noch low?
   {
		put2str(T_bt,T_rn4678);														// "Bluetooth Micro" Modultyp ausgeben
		putstr(T_dpkt); putnumber(115200,0); newline();	// Startbaudrate anzeigen
		retry=4;
    while (retry--)																									// Versuche RN4678 mit 115200, 9600, 307200, 460800
    {		 
		 result=bt_command(T_$,T_cmdp,10300);	// Sende $$$, warte 300ms auf CMD> prompt 
     if (result)	break;																							// CMD> erhalten? 
		 connect=UART0;
		 newline();																																// Neue Zeile für nächstes Kommandos	
		 if      (retry==3) { Init_UART1 (9600);   putnumber(9600,0);   newline(); }	// teste 9600 Baud
		 else if (retry==2) { Init_UART1 (307200); putnumber(307200,0); newline(); }	// teste 307200 Baud
		 else if (retry==1) { Init_UART1 (460800); putnumber(460800,0); newline(); }	// teste 460800 Baud
		 osDelay(500);															// Silence gap fuer $$$ Kommandomodus Erkennung
		 bxi=rxi;																		// Empfangspuffer leeren
		}
		
		if (result>0) result=bt_command(T_sf1,T_AOK,12000); // Reset to Factory default
		if (result>0)																				// Factory Reset erfolgreich?
		{		
		 result=bt_command(T_sr1,T_reb,10500);							// Reboot Command
		 Init_BT_ch(115200);																// Factory default Baudrate nach SF,1
		 if (wait_message(T_boot,13000)<0) result=0;				// Warte 3s auf "Reboot" Nachricht 											
		 ResetWDT();
		 if (result>0)																			// Rebooting erhalten?
     { 			 		  																
      result=bt_command(T_$,T_cmdp,10300);							// Sende $$$, warte auf CMD> prompt			
		  if (result>0) 																		// Wenn ok setze zuerst den device name
		  {
		   putstr(T_sno);																		// Set device name Kommando
		   putstr (T_viasis);																// Text "VIASIS"		
		   putc ('_');																			// Unterstrich
			 putstr	(fp.serno);																// Seriennummer
		   result=bt_command(T_CR,T_AOK,12000);							// nur Kommandoabschluss	
		  }
		  if (result>0) result=bt_command(T_auth2,T_AOK,10300);	// Setze Authorization Methode 2 - Pairing ohne PIN
			if (result>0) result=bt_command(T_ssp,T_AOK,10300);		// Bluetooth classic service name
			if (result>0) result=bt_command(T_powr,T_AOK,10300);	// Setze Maximum Transmit Power Stufe 4
		  if (result>0) 
			{
       putstr(T_sp);
       putnumber (fp.btpin,0xC6);														// Pinnummer immer 6 stellig		
			 result=bt_command(T_CR,T_AOK,10300);									// Setze default Pin, 6 digits der Seriennummer
			}
		  if (result>0) result=bt_command(T_dual,T_AOK,10300);	// SG, setze nur BT dualer Modus			
		  if (result>0) result=bt_command(T_su,T_AOK,10300);		// Baudrate auf 307200 setzen
			if (result>0) result=bt_command(T_so,T_AOK,10300);		// Statusmeldungen abschalten
		  if (result>0) 
			{
			 result=bt_command(T_sr1,T_reb,10500);								// Reboot Command, wait for ack
			 Init_BT_ch(307200);																// Baudrate 307200 einstellen
			 putc('.'); osDelay(1000); ResetWDT();							// Wait for module reboot with feedback
			 putc('.'); osDelay(1000); ResetWDT();
			 newline();
			 bxi=rxi;																					// Flush RX buffer before $$$
		   result=bt_command(T_$,T_cmdp,10300);									// Sende $$$, warte auf CMD> prompt 				
			 if (result) result = bt_command(T_min,T_end,10300);	// --- END erhalten?
			 if (result>0) // end erhalten? 
			 {
				ResetWDT();											// watchdog reset	
				fp.btmodem=Roving;								// Roving BT modem gefunden und konfiguriert
				interfaces|=BT_LINK;							// BT interface ist aktiv		
			 }	
			}	
		 } // end if Reboot Nachricht erhalten			
	  } // end if Factory Reset erfolgreich 
   } // end if Init_BT_ch		 
  }	// end if kein Laird	
 } // end if CTS lo
 uart1_release(MUX_BT);								// Release UART1
 connect=concpy;												// Verbindungszustand wieder herstellen
 osDelay (10);													// Warte auf Satzsteuerzeichen hinter letztem Ok 		
 clear_comchange ();										// Bereinige Schnittstellenwechsel
 if (fp.btmodem==0) { newline(); dtcerr(E_bt);	// Konfigurationsfehler oder nicht da
	 newline(); }
}	

bool test_BT	(void)							// Prüfe ob BT Modul antwortet
{
 bool result=FALSE;
 uint32_t baud=BT_BAUD;																// Baudrate BT
	

 if (fp.btmodem==IF820)                       // IF820: Lebenstest per /PING
 {
  if (Init_BT_ch(baud)) result=bt_command(T_ping,T_pingok,300);
  uart1_release(MUX_BT);
  if (!result) puterror(BLUETOOTH_ERROR,-1);
  return(result);
 }
 if (Init_BT_ch (baud))									// Konfiguriere uart1 und setze MUX Kanal auf BT modem
 {
	if (fp.btmodem>Laird)									// RN4678?
  { 
	 result=bt_command(T_$,T_cmdp,0);									// Sende $$$, warte auf CMD> prompt		
	 if (result) result = bt_command(T_min,T_end,0);	// Return to data mode	
	}
	else result=bt_command(T_AT,T_OKNZ,0);			// Laird - Sende AT - OK erhalten? -> TRUE Rückgabe
 } // end if Init_BT_ch
 uart1_release(MUX_BT);														// Release UART1
 if (!result) puterror (BLUETOOTH_ERROR,-1);	// Fehlermeldung
 return(result);
}	

void send_bt_info (void)							// Bluetooth Informationen ausgeben
{	
 bool result=FALSE;
 uint32_t baud=BT_BAUD;
 

 if (fp.btmodem==IF820)                       // IF820: Basis-Info ausgeben
 {
  if (Init_BT_ch(baud)) result=bt_command(T_ping,T_pingok,300);
  if (result) { put2str(T_bt,T_if820); newline(); }
  uart1_release(MUX_BT);
  if (!result) puterror(BLUETOOTH_ERROR,-1);
  return;
 }
 if (Init_BT_ch (baud))										// Konfiguriere uart1 und setze MUX Kanal auf BT modem
 {
	if (fp.btmodem>Laird) 									// RN4678?
	{
	 result=bt_command(T_$,T_cmdp,0);				// Sende $$$, warte auf CMD> prompt	
	 if (result) result=bt_command(T_V,T_CR,0);		// Firmware Version
	 if (result) putln(cbuf);											// Antwort ausgeben
   if (result) result=bt_command(T_gds,T_CR,0); // BT / MAC Adresse
	 if (result)
   {
		put2str(T_btbda,T_ist);								// Text	"Bluetooth Adresse
    putln(cbuf);													// Antwort ausgeben		 
	 }	
	 if (result) result=bt_command(T_gn,T_CR,0); // Device name
   if (result)
   {
		put2str(T_btname,T_ist);							// Device name
		putln(cbuf);													// Antwort ausgeben 
	 }		 
	 if (result) 		 
	 {
		putstr(T_btprot);											// Text "Protokoll"
		putstr(T_dpkt);
		putstr (T_spp);
    putstr (T_col);
    putln (T_ble);		 
		result = bt_command(T_min,T_end,0);		// Return to data mode 
	 }	 	 
	}
	else 																		// Laird
  if (bt_command(T_ATI0,T_OKNZ,-250))			// Abfrage und Ausgabe Modultyp
  {	 
   putstr(T_tver);												// Text	"Version...
   if (bt_command(T_ATI3,T_OKNZ,-250))		// Abfrage und Ausgabe Firmware Version
	 {	
	  put2str(T_btbda,T_ist);								// Text	"Bluetooth Adresse
    if (bt_command(T_ATI4,T_OKNZ,-250))		// Abfrage lokale Bluetooth Adresse des Moduls
	  {	
		 put2str(T_btname,T_ist);							// Device name	 
		 if (bt_command(T_BTNG,T_OKNZ,-250))	// Abfrage device name
		 {
			putln(T_btlst);											// Text "Trusted device list... 
			result=bt_command(T_BTT, T_OKNZ, -500);	
			if (result) 
				if (ja(T_dellst)>0)											// Abfrage "Liste löschen?"
					result=bt_command(T_BTDM, T_OKNZ, 0);	// Clear trusted device list
	   } } } } }
 newline();	
 uart1_release(MUX_BT);														// Release UART1		
 if (!result) puterror (BLUETOOTH_ERROR,-1);	// Fehlermeldung	 	 
}

void set_bt_name (void)							// Setze Bluetooth device (friendly) name
{	
 int32_t result;
 uint32_t baud; 	
 uint8_t maxlen;
 uint8_t concopy=connect;
 char	btnamebuf[40];

 if (fp.btmodem==IF820) { maxlen=32; baud=BT_BAUD; }
 else if (fp.btmodem>Laird) { maxlen=16;	baud=BT_BAUD; }		// Device name bei RN4678 nur 16 character
 else { maxlen=40; baud=BT_BAUD; }																// Laird 40 characters
	
 put2str(T_btname,T_ist);
 result=getline(btnamebuf,maxlen,'a');			// Abfrage Bluetooth Gerätename
 if (result>0) 															// Eingabe erfolgt?				
 {
	Init_BT_ch (baud);												// Konfiguriere uart1 und setze MUX Kanal auf BT modem 				
	if (fp.btmodem==IF820)
	{
	 char cmd[60];
	 strcpy(cmd,"SDN$,T=00,N="); strcat(cmd,btnamebuf); strcat(cmd,"\r"); result=bt_command(cmd,T_sdnok,500);
	 if (result) { strcpy(cmd,"SDN$,T=01,N="); strcat(cmd,btnamebuf); strcat(cmd,"_BT"); strcat(cmd,"\r"); result=bt_command(cmd,T_sdnok,500); }
	}
	else if (fp.btmodem>Laird)  						
	{
   result=bt_command(T_$,T_cmdp,0);					// Sende $$$, warte auf CMD> prompt
	 if (result)															// CMD> prompt?
   {	
    connect=UART1;		 
    putstr(T_sno);													// Set device name Kommando		
	  putln (btnamebuf);											// Neuen Device Name senden
		if (wait_message(T_AOK,250)>0) 					// AOK erhalten?
		{	
		 result=bt_command(T_sr1,T_reb,0);				// Reboot Command
		 if (result)															// Reboot Nachricht empfangen
     {			 
		  osDelay(1000);													// Warte 1s				
		  result=bt_command(T_$,T_cmdp,0);				// Sende $$$, warte auf CMD> prompt	
			if (result) result = bt_command(T_min,T_end,0);	// Gehe in Datenmodus, END Nachricht erhalten? 
		 }	// end if Reboot 
		} // end if AOK	
		else result=0;
	 } //end if CMD prompt	
  }	// end if Roving	
	else // Laird
  {		
	 connect=UART1;	
	 putstr (T_BTN);													// AT+BTN (device name) Kommando senden
   putstr (btnamebuf);											// Neuen Device Name senden
   putc('"');																// Zeichenkettenbschluss senden
   result=bt_command (T_CR, T_OKNZ, 0);			// Kommandoabschluss senden
   if (result)			
	 {	
		connect=UART1;  
		putstr (T_BTF);													// AT+BTF (device friendly name) Kommando senden
    putstr (btnamebuf);											// Neuen Device Name senden		 
	  putc('"');															// Zeichenkettenbschluss senden		 		
    result=bt_command (T_CR, T_OKNZ, 0);		// Kommandoabschluss senden
   }
  }
  connect=concopy;
  uart1_release(MUX_BT);														// Release UART1
	if (!result) puterror (BLUETOOTH_ERROR,-1);	// Fehlermeldung	
	else putln (T_OKNZ);
 } // end Eingabe	
} 


void set_bt_pin (void)							// Setze Pinnummer
{
 int result;
 uint32_t baud=BT_BAUD;										// Baud rate BT
 uint8_t len=4;														// Pin length Laird	
 uint32_t pin;
 uint8_t concopy=connect;	

 if (fp.btmodem==IF820)                       // IF820: PIN ungenutzt (Just Works)
 { putln(" IF820: PIN nicht verwendet (Just Works)"); return; }
 if (fp.btmodem>Laird) 
 {	 
	 len=6;
 }	 
	 
 put2str(T_pinno,T_ist);		
 result=getline(cbuf,len,'d');							// len-stellige Pinnummer einlesen
 pin=atoi(cbuf);
	 
 if (pin && (result>0))										// Wert eingegeben und nicht null?
 {	
  result=0;	 
  if (Init_BT_ch (baud))									// Konfiguriere uart1 und setze MUX Kanal auf BT modem, wenn CTS low dann
	{
	 if (fp.btmodem>Laird) 									// RN4678?
   {		 
	  result=bt_command(T_$,T_cmdp,0);			// Sende $$$, warte auf CMD> prompt
	  if (result)														// CMD> prompt?
    {	
		 connect=UART1;	
	   putstr(T_sp);												// RN4678? Ja, SP Kommando
		 putnumber(pin,0xC6);									// Pinnummer senden
		 result=bt_command (T_CR, T_AOK, 0);
		 //if (result) result=bt_command(T_sr1,T_reb,0);	// Auskommentiert da Statusmeldungen jetzt aus sind		 	
		 if (result)																			// Pinnummer Quittung empfangen
     {			
			connect=UART1; 
			putstr (T_sr1);											// Reboot Command
		  osDelay(1000);											// Warte 1s	
		  fp.btpin=pin;												// PIN übernehmen		
		  result=bt_command(T_$,T_cmdp,0);		// Sende $$$, warte auf CMD> prompt	
			if (result) result = bt_command(T_min,T_end,0);	// Gehe in Datenmodus, END Nachricht erhalten? 			
		 }	// end if Reboot 	
    }
	 }		
	 else 																  // Laird 730-SA
	 {
		connect=UART1; 
		putstr(T_BTK);													// Laird Kommando AT+BTK	
		putnumber(pin,0xC4);										// Pinnummer 4 digits senden 		
	 	putc('"');															// Abschluss Pinnummer
		result=bt_command (T_CR, T_OKNZ, 0); 		// Kommandoabschluss, warte auf OK Antwort
    if (result) fp.btpin=pin;								// PIN übernehmen		 
   }		 
  }	// end if Init_BT_ch	
	
	connect=concopy; 
	if (!result) puterror (BLUETOOTH_ERROR,-1);	// Fehlermeldung
	else putln (T_OKNZ);	
 }
 uart1_release(MUX_BT);														// Release UART1
}




