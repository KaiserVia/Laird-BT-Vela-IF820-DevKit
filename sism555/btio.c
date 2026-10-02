//-----------------------------------------------------------------------------
//  FILE: btio.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Routinen f�r Bluetooth Modem Erweiterung
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
#include "i2cm.h"		// IC54 (PCA9554) Zugriff fuer CYSPP-Steuerung
#include "string.h"
#include "dtc_codes.h"

bool bt_command (text * command, text * answer, int pause) // Bluetooth Kommando senden, Antwort empfangen
{                                          // pause>0: Kommando+Antwort formatiert ans Terminal spiegeln
                                           // pause<0: still senden (nur intern); |pause|>10000: CRLF-Flag (RN4678)
 int result;
 uchar clen=0;
 uint timeout=BT_CHAR_TIMEOUT;
 uchar addcrlf=0;

 osDelay (2);
 MUX_BT_Select;                            // MUX Bluetooth Leitungsauswahl
 osDelay (2);

 while (*(command+clen)!=0) clen++;        // Kommandolaenge

 result = pause;
 if (result <0) result=-result;
 if (result > 10000) { addcrlf=1; result-=10000; }
 if (pause) timeout=result;

 if (pause>0)                              // gesendetes Kommando -> Terminal, eigene Zeile "  > <cmd>"
 { uchar cc=connect; connect=concpy&~UART1; newline(); putstr("  > "); putstrCR(command); connect=cc; }
 connect=UART1;                            // Kommando NUR ans Modul senden (kein roher Mix am Terminal)
 putstr (command);

 // FIX (5.93): Puffer-Wahl muss der ISR-Weiche (hard.c) exakt folgen - die routet nur beim IF820
 // nach rbuf1; bei Laird/RN4678 (kein CYSPP-Bypass moeglich) landet UART1 unveraendert in rbuf.
 if ((fp.btmodem==Laird) || (fp.btmodem==Roving))
 { bxi=rxi; result = wait_message (answer,timeout); }
 else
 { bxi1=rxi1; result = wait_message1 (answer,timeout); }

 osDelay (2);
 connect=concpy;                           // urspruengliche Verbindung wiederherstellen

 if (pause>0 && result>=0)                 // empfangene Antwort -> Terminal, eigene Zeile "  < <resp>"
 {
  osDelay (timeout/5);                      // Wartezeit naechster Befehl
  { uchar cc=connect; connect=concpy&~UART1; newline(); putstr("  < "); putstr(cbuf); if (addcrlf) newline(); connect=cc; }
 }

 if (cts_timeout)                          // CTS-Wait in putb() abgelaufen -> Warnung (throttled)
 { static uint last_cts_warn=0;
   cts_timeout=0;
   if ((uint)(mstimer-last_cts_warn) > 2000)
   { uchar cc=connect; connect&=~UART1;
     dtcerr(DTC_BT_CTS_TIMEOUT); newline(); connect=cc; last_cts_warn=mstimer; }
 }
 if (result<0) return (FALSE);
 return (TRUE);
}	

bool Init_BT_ch (uint baudrate)			// Konfiguriere uart1 und setze MUX Kanal auf BT modem 
{																// �bergabe Baudrate - Baudrate nur 9600 oder 115200
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
  {
   uchar tries;
   for (tries=0; tries<3; tries++)                    // GTU direkt nach /PING liefert manchmal keine Antwort -> Retry
   {
    bxi1=rxi1;                                          // RX-Reste (z. B. /PING-Nachlauf) verwerfen
    if (bt_command(T_gtu,T_gtu_d,400))                 // GTU senden, Antwort bis ",D=" in cbuf
    {
     p = strstr(cbuf,",F=");                           // Flow-Control-Feld suchen
     if (p) return ((p[3]=='0' && p[4]=='1') ? 1 : 0); // "F=01" -> an, sonst aus
    }
    osDelay(60);                                       // kurz setteln, dann erneut versuchen
   }
   return (-1);                                        // 3x keine verwertbare GTU-Antwort
  }
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
  {
   char cmd[48]; char *q; int i;
   strcpy(cmd,"STU,B=");                                       // STU-Kommando aus IF820_BAUD bauen (EINE Quelle)
   q=cmd+6; for (i=28;i>=0;i-=4) *q++=hexchar(IF820_BAUD>>i);  // Baudrate als 8 Hex-Ziffern anhaengen
   strcpy(q,T_stu_post);                                       // Suffix ,A=00,...,F=01,...CR
   if (!bt_command(cmd,T_stuok,300))      return (FALSE);      // STU (RAM) - Antwort kommt @ alter Baud
   osDelay(80);                                                // Modul seine UART auf Zielbaud umstellen lassen
   Init_BT_ch(IF820_BAUD);                                     // Host auf Zielbaud (Doku Bsp.1: nach STU-Antwort)
   osDelay(20); bt_command(T_CR,T_scfgok,-60);                 // setteln + Parser/Puffer leeren vor /SCFG
   if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);      // /SCFG @ neuer Baud -> Flash + verifiziert
   return (TRUE);
  }
  case Roving:  return (FALSE);   // TODO: RN4678 Flow-Control-Befehl
  case Laird:   return (FALSE);   // TODO: Laird Flow-Control-Register
  default:      dtcerr(DTC_BT_UNKNOWN_TYPE); return (FALSE);   // Fangnetz: neues Modul nicht still durchwinken
 }
}

bool bt_ensure_flowcontrol (void)    // generisch: pruefen, bei Bedarf setzen, verifizieren
{
 if (bt_get_flowcontrol()==1)                               // schon aktiv?
 { putln(" Flow Control: bereits aktiv"); return (TRUE); } // -> kein Flash-Write noetig
 if (!bt_set_flowcontrol())
 { dtcerr(DTC_BT_FLOWCTL_SET); newline(); return (FALSE); }
 if (bt_get_flowcontrol()==1)
 { putln(" Flow Control: aktiviert und gesichert"); return (TRUE); }
 dtcerr(DTC_BT_FLOWCTL_VERIFY); newline(); return (FALSE);
}

//-----------------------------------------------------------------------------
// Classic-Konfiguration (5.54): Sniff-Mode AUS + Class of Device seriell
//  - Sniff AUS  -> Modul lehnt vom PC angeforderten Stromsparmodus ab -> keine
//                  ~300 ms Latenz pro X/Y-Modem-Block mehr (Download bleibt schnell,
//                  unabhaengig davon, welcher PC/BT-Adapter Sniff anfordert).
//  - CoD seriell-> Windows sieht kein "Audiogeraet" mehr (Werksdefault 0x240418 =
//                  Kopfhoerer) und legt den SPP-COM-Port an.
// Nur IF820. Wirkt NICHT auf BLE (Sniff ist ein reines Classic-Konzept) und NICHT auf
// den unverbundenen Ruhezustand (Sniff gilt nur waehrend einer aktiven Verbindung) ->
// Batteriegeraete ohne bestehende BT-Verbindung sind unbeeinflusst.
//-----------------------------------------------------------------------------
// CoD des Moduls lesen und mit dem Zielwert IF820_COD vergleichen. Raw-Tail-Read wie
// bt_show_name(), weil C= das LETZTE Feld der GBTDC-Antwort ist (kein Folge-Anker fuer
// wait_message). Das Sniff-Flag ist NICHT nutzbar (GBTP F liest immer 0), daher gilt die
// CoD als Indikator: da CoD und Sniff in bt_set_btcfg() IMMER gemeinsam geschrieben werden,
// bedeutet "CoD == Zielwert" auch "Sniff bereits aus".
int bt_get_cod (void)                // -1 = keine Antwort, 0 = abweichend, 1 = == IF820_COD
{
 char buf[64]; uchar n=0, i; char *p; uchar dconn=connect;
 if (fp.btmodem!=IF820) return (-1);
 connect=UART1; putstr(T_gbtdc);                       // "GBTDC\r" an das Modul
 bxi1=rxi1;                                            // RX-Marke (rbuf1, 5.89)
 connect=dconn & ~UART1;                               // nichts zurueck ans Modul spiegeln
 osDelay(200);                                         // Antwort abwarten (kurze, feste Antwort)
 while (bxi1!=rxi1 && n<sizeof(buf)-1) buf[n++]=rbuf1[++bxi1];
 buf[n]=0; connect=dconn;
 p=strstr(buf,"C=");                                   // CoD-Feld der GBTDC-Antwort
 if (!p) return (-1);                                  // keine/keine verwertbare Antwort
 p+=2;
 for (i=0; i<8 && IF820_COD[i]; i++) if (p[i]!=IF820_COD[i]) return (0);  // 8 Hex vergleichen
 return (1);
}

bool bt_set_btcfg (void)             // Sniff AUS + CoD seriell -> RAM, dann /SCFG (Flash)
{
 if (fp.btmodem!=IF820) { dtcerr(DTC_BT_UNKNOWN_TYPE); return (FALSE); }
 if (!bt_command(T_sbtdc,T_sbtdcok,400)) return (FALSE);    // Class of Device -> seriell/unkategorisiert
 if (!bt_command(T_sbtp_ns,T_sbtpok,400)) return (FALSE);   // Classic-Parameter mit Sniff-Bit F=01 (aus)
 osDelay(20); bt_command(T_CR,T_scfgok,-60);                // setteln + Parser leeren vor /SCFG
 if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);     // dauerhaft in Flash sichern
 return (TRUE);
}

bool bt_ensure_btcfg (void)          // pruefen (ueber CoD), bei Bedarf Sniff+CoD setzen
{
 if (bt_get_cod()==1)
 { putln(" BT-Cfg: CoD seriell + Sniff aus (bereits gesetzt)"); return (TRUE); }
 if (!bt_set_btcfg())                                       // Fehlschlag ist NICHT fatal (nur Optimierung)
 { putln(" BT-Cfg: WARNUNG - Sniff/CoD nicht gesetzt"); return (FALSE); }
 if (bt_get_cod()==1)
 { putln(" BT-Cfg: CoD seriell + Sniff aus gesetzt und gesichert"); return (TRUE); }
 putln(" BT-Cfg: WARNUNG - Verifikation CoD fehlgeschlagen"); return (FALSE);
}

//-----------------------------------------------------------------------------
// Dauerhaftes BLE-Advertising (5.55): das Modul stoppt ab Werk nach ~90 s. Der Kunde
// kann aber jederzeit ans Geraet kommen -> Advertising darf nie aufhoeren. Umgesetzt
// ueber SAP: Low-Duty-Dauer O=0000 (unendlich) + Auto-Advertising F=01 (Boot+Disconnect).
// Idempotenz-Check ueber das O-Feld von GAP (zuverlaessig ruecklesbar, im Gegensatz zum
// SBTP-Sniff-Flag). Nur IF820.
//-----------------------------------------------------------------------------
int bt_get_adv (void)                // -1 = keine Antwort, 0 = abweichend, 1 = T=04 (low-duty) UND O=0000 UND L=Ziel
{
 char *pt, *po, *pl;
 if (fp.btmodem!=IF820) return (-1);
 { uchar tries;
   for (tries=0; tries<3; tries++)                     // wie GTU/GBTP: erster Versuch manchmal leer
   {
    if (bt_command(T_gap,T_gap_end,400))               // GAP bis ",F=" -> T-, L- und O-Feld komplett in cbuf
    {
     pt = strstr(cbuf,",T=");                          // Adv-Typ (04 = low-duty-only)
     pl = strstr(cbuf,",L=");                          // Low-Duty-Intervall (5.6x: Ziel 0140 = 200 ms)
     po = strstr(cbuf,",O=");                          // Low-Duty-Dauer (0000 = unendlich)
     if (pt && pl && po)
      return ((pt[3]=='0'&&pt[4]=='4' &&               // T=04  UND
               pl[3]=='0'&&pl[4]=='1'&&pl[5]=='4'&&pl[6]=='0' &&  // L=0140 UND
               po[3]=='0'&&po[4]=='0'&&po[5]=='0'&&po[6]=='0') ? 1 : 0);  // O=0000
    }
    osDelay(60);
   }
 }
 return (-1);
}

bool bt_set_adv (void)               // dauerhaftes Advertising -> RAM, dann /SCFG (Flash)
{
 if (fp.btmodem!=IF820) { dtcerr(DTC_BT_UNKNOWN_TYPE); return (FALSE); }
 if (!bt_command(T_sap,T_sapok,400)) return (FALSE);       // SAP: O=0000 (unendlich) + F=01 (Auto-Adv)
 osDelay(20); bt_command(T_CR,T_scfgok,-60);               // setteln + Parser leeren vor /SCFG
 if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);    // dauerhaft in Flash sichern
 return (TRUE);
}

bool bt_ensure_adv (void)            // pruefen (ueber O-Feld), bei Bedarf setzen, verifizieren
{
 if (bt_get_adv()==1)
 { putln(" BT-Adv: dauerhaft (bereits gesetzt)"); return (TRUE); }
 if (!bt_set_adv())                                        // Fehlschlag NICHT fatal (nur Auffindbarkeit)
 { putln(" BT-Adv: WARNUNG - Advertising nicht gesetzt"); return (FALSE); }
 if (bt_get_adv()==1)
 { putln(" BT-Adv: dauerhaft gesetzt und gesichert"); return (TRUE); }
 putln(" BT-Adv: WARNUNG - Verifikation Advertising fehlgeschlagen"); return (FALSE);
}

//-----------------------------------------------------------------------------
// App-Config (5.57): Echo aus (A1), CYSPP-Paketierung (A2), Just-Works-Security (A5)
//-----------------------------------------------------------------------------
// A1 - Command-Echo: GPEM liefert M als LETZTES Feld -> Raw-Tail-Read (wie bt_get_cod).
int bt_get_echo (void)               // -1 = keine Antwort, 0 = Echo an, 1 = Echo aus (M=00)
{
 char buf[48]; uchar n, t; char *p; uchar dconn=connect;
 if (fp.btmodem!=IF820) return (-1);
 for (t=0; t<3; t++)                                   // Retry: erster Read kann leer sein (vermeidet
 {                                                     //         sonst einen unnoetigen Reflash+Reboot)
  n=0; connect=UART1; putstr(T_gpem); bxi1=rxi1;       // GPEM ans Modul, RX-Marke (rbuf1, 5.89)
  connect=dconn & ~UART1; osDelay(200);               // nichts spiegeln, Antwort abwarten
  while (bxi1!=rxi1 && n<sizeof(buf)-1) buf[n++]=rbuf1[++bxi1];
  buf[n]=0; connect=dconn;
  p=strstr(buf,"M=");                                  // Echo-Feld (im Header/GPEM kein "M=")
  if (p) return ((p[2]=='0'&&p[3]=='0') ? 1 : 0);      // M=00 -> Echo aus
  osDelay(60);
 }
 return (-1);
}

bool bt_set_echo (void)              // Command-Echo aus -> RAM, dann /SCFG
{
 if (fp.btmodem!=IF820) return (FALSE);
 if (!bt_command(T_spem,T_spemok,300)) return (FALSE);     // SPEM,M=0 (Echo aus)
 osDelay(20); bt_command(T_CR,T_scfgok,-60);
 if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);    // dauerhaft sichern
 return (TRUE);
}

// A2 - CYSPP-Paketierung: Wait-Feld ueber .CYSPPGK ruecklesbar (Anker ",L=" -> W komplett).
int bt_get_pkt (void)                // -1 = keine Antwort, 0 = anders, 1 = W=01 (1 ms)
{
 char *p; uchar t;
 if (fp.btmodem!=IF820) return (-1);
 for (t=0; t<3; t++)
 {
  if (bt_command(T_cypgk,T_cypgk_end,400))
  { p = strstr(cbuf,",W="); if (p) return ((p[3]=='0'&&p[4]=='1') ? 1 : 0); }  // W=01
  osDelay(60);
 }
 return (-1);
}

bool bt_set_pkt (void)               // CYSPP-Wait auf 1 ms -> RAM, dann /SCFG
{
 if (fp.btmodem!=IF820) return (FALSE);
 if (!bt_command(T_cypsk,T_cypskok,400)) return (FALSE);   // .CYSPPSK M=01,W=01,L=14,E=0D
 osDelay(20); bt_command(T_CR,T_scfgok,-60);
 if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);
 return (TRUE);
}

// A5 - Security: M-Feld ueber GSBP ruecklesbar (Anker ",B=" -> M komplett). M=00 == Just Works.
int bt_get_sec (void)                // -1 = keine Antwort, 0 = anders, 1 = Just-Works (M=00)
{
 char *p; uchar t;
 if (fp.btmodem!=IF820) return (-1);
 for (t=0; t<3; t++)
 {
  if (bt_command(T_gsbp,T_gsbp_end,400))
  { p = strstr(cbuf,",M="); if (p) return ((p[3]=='0'&&p[4]=='0') ? 1 : 0); }  // M=00
  osDelay(60);
 }
 return (-1);
}

bool bt_set_sec (void)               // Just-Works festschreiben -> RAM, dann /SCFG
{
 if (fp.btmodem!=IF820) return (FALSE);
 if (!bt_command(T_ssbp,T_ssbpok,400)) return (FALSE);     // SSBP M=00,B=00,K=10,P=00,I=03,F=01
 osDelay(20); bt_command(T_CR,T_scfgok,-60);
 if (!bt_command(T_scfg,T_scfgok,1000)) return (FALSE);
 return (TRUE);
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
   strcpy(want,"N="); strcat(want,T_viasis); strcat(want,"_"); strcat(want,fp.serno);  // N=VIASIS_<serno>
   if (!bt_command("GDN,T=01\r",want,300)) return (0);   // BT-Classic-Name == VIASIS_<serno>?
   strcat(want,"_BLE");                                  // -> N=VIASIS_<serno>_BLE
   if (!bt_command("GDN,T=00\r",want,300)) return (0);   // BLE-Name == VIASIS_<serno>_BLE?
   return (1);
  case Roving:  return (-1);   // TODO: RN4678 GN-Abfrage
  case Laird:   return (-1);   // TODO: Laird AT+BTN? Abfrage
  default:      return (-1);
 }
}

void bt_reboot (void)   // IF820: /RBT senden + auf Boot warten (Classic-Name wird dann live)
{                        // TEMPORAER (5.85): Boot-Ausgabe jetzt sichtbar statt verworfen (vorher
                          // bxi=rxi direkt nach dem Warten - was das Modul in dieser Zeit sagt,
                          // war nie zu sehen). Damit laesst sich ein Reboot-Verify-Fehlschlag
                          // (F11018) live nachvollziehen.
 uchar cc, w; char c; uint t0=mstimer; bool bol=TRUE;
 cc=connect; connect|=UART1; putstr(T_rbt); connect=cc;   // /RBT senden
 bxi1=rxi1;                                // FIX (5.89): rbuf1 (Command-Mode-Puffer)
 cc=connect; connect=concpy & ~UART1;      // Rohausgabe nur ans Terminal, nichts zurueck ans Modul
 for (w=0; w<8; w++)                       // ~8s Gesamtwartezeit (vorher 6s+1.5s, hier durchgehend
 {                                          // mitgelesen statt nur teilweise verworfen)
  osDelay(1000); ResetWDT();
  while (bxi1!=rxi1)
  {
   c = rbuf1[++bxi1];
   if (c=='\r') continue;                  // CR ueberspringen, LF traegt den Zeilenumbruch
   if (bol) { putc('['); putnumber(mstimer-t0,0); putstr("ms] "); bol=FALSE; }
   putc((c=='\n' || (c>=32 && c<127)) ? c : '.');
   if (c=='\n') bol=TRUE;
  }
 }
 newline(); connect=cc; bxi1=rxi1;
}

bool bt_set_name (void)              // Name = VIASIS_<serno> schreiben (SDN$ -> RAM+Flash)
{
 char cmd[56];
 switch (fp.btmodem)
 {
  case IF820:
   strcpy(cmd,"SDN$,T=00,N="); strcat(cmd,T_viasis); strcat(cmd,"_"); strcat(cmd,fp.serno); strcat(cmd,"_BLE"); strcat(cmd,"\r");  // BLE-Name: VIASIS_<serno>_BLE
   if (!bt_command(cmd,T_sdnok,1500)) return (FALSE);  // BLE-Name setzen (Flash-Write -> grosszuegiger Timeout)
    osDelay(300);                                       // Flash-Write von T=00 abschliessen lassen, bevor T=01 folgt
   strcpy(cmd,"SDN$,T=01,N="); strcat(cmd,T_viasis); strcat(cmd,"_"); strcat(cmd,fp.serno); strcat(cmd,"\r");
   if (!bt_command(cmd,T_sdnok,1500))                  // BT-Classic-Name setzen (Flash-Write)
    { osDelay(300); if (!bt_command(cmd,T_sdnok,1500)) return (FALSE); }   // 1 Retry gegen Flash-Write-Timing
   return (TRUE);
  case Roving:  return (FALSE);   // TODO: RN4678 SN,
  case Laird:   return (FALSE);   // TODO: Laird AT+BTN/AT+BTF
  default:      dtcerr(DTC_BT_UNKNOWN_TYPE); return (FALSE);
 }
}

bool bt_ensure_name (void)           // generisch: pruefen, bei Bedarf schreiben, verifizieren
{
 if (!fp.serno[0]) { dtcerr(DTC_BT_NO_SERIAL); newline(); return (FALSE); }
 if (bt_get_name()==1) { putln(" BT-Name: bereits gesetzt"); return (TRUE); }   // kein Flash-Write
 if (!bt_set_name())   { dtcerr(DTC_BT_NAME_SET); newline(); return (FALSE); }
 if (bt_get_name()!=1) { dtcerr(DTC_BT_NAME_VERIFY); newline(); return (FALSE); }  // verify VOR Reboot (Modul stabil)
 if (fp.btmodem==IF820) bt_reboot();   // Classic-Name live machen - erst NACH erfolgreicher Verifikation
 putln(" BT-Name: gesetzt und gesichert"); return (TRUE);
}

void bt_show_name (void)             // aktuellen BT-Namen vom Modul lesen und am Terminal ausgeben
{
 char buf[80]; uchar n=0, i; char *p; uchar dconn=connect;
 if (fp.btmodem!=IF820) return;      // derzeit nur IF820 (RN4678/Laird: GN bzw. AT+BTN? -> TODO)
 connect=UART1; putstr(T_gdn);       // "GDN\r" an das Modul
 bxi1=rxi1;                          // RX-Marke (rbuf1, 5.89)
 connect=dconn & ~UART1;             // Ausgabe nur ans Terminal, nicht zurueck ans Modul
 osDelay(200);                       // Antwort abwarten
 while (bxi1!=rxi1 && n<sizeof(buf)-1) buf[n++]=rbuf1[++bxi1];
 buf[n]=0;
 newline();                          // eigene Zeile (nicht an vorherige Meldung kleben)
 put2str(T_btname,T_ist);            // Label "Device name ..."
 p=strstr(buf,"N=");                 // Namensfeld der GDN-Antwort
 if (p) { p+=2; for (i=0; p[i] && p[i]!='\r' && p[i]!='\n'; i++) putc(p[i]); }
 else putstr("?");
 newline();
 connect=dconn;
}

//-----------------------------------------------------------------------------
// CYSPP-Steuerung (IC54/PCA9554 Bit 7): Command-Mode erzwingen / SPP trennen
//-----------------------------------------------------------------------------
// Zustandsmerker: treibt der HOST die CYSPP-Leitung gerade als Ausgang HIGH?
// Wichtig, weil ein extern gehaltenes HIGH == "CYSPP lokal de-asserted" ist: Das Modul
// baut dann WEDER SPP NOCH den BLE-CYSPP-Datenkanal auf bzw. beendet einen laufenden
// sofort (EZ-Serial: "the data pipe will remain open until the peer disconnects, or the
// CYSPP pin is de-asserted locally"). Bleibt die Leitung ungewollt Ausgang, ist BT tot.
volatile uchar bt_cyspp_out = 0;   // volatile: wird auch aus GSM-/MQTT-Thread gesetzt (uart1_request)

void bt_cmdmode (void)   // CYSPP HIGH treiben -> laufende SPP-Verbindung trennen + Command-Mode
{
#if BT_CYSPP
 // Modul-Guard (5.52): IC54 Bit7 ist bei ALLEN drei BT-Modulen dieselbe Leitung (BT_LINK),
 // aber nur beim IF820 bidirektional (CYSPP) und damit vom Host treibbar. Beim RN4678 und
 // beim Laird BT730 ist sie ein reiner STATUS-AUSGANG des Moduls -> ein Host-Treiber waere
 // ein Pegelkonflikt gegen den Modulausgang, und BT_LINK wuerde faelschlich "nicht
 // verbunden" lesen. fp.btmodem==0 (Erstboot / Modultausch / Erkennung noch nicht gelaufen)
 // ist bewusst erlaubt: dort MUSS der Command-Mode erzwingbar sein, sonst scheitert die
 // IF820-Erkennung an einem Modul, das noch im Datenmodus steht.
 if ((fp.btmodem==Roving) || (fp.btmodem==Laird)) return;
 write_i2c_dev(IC54, 2, (0x80<<8)|0x01);   // PCA9554 Output-Register (0x01): Bit 7 = HIGH
 write_i2c_dev(IC54, 2, (0x7F<<8)|0x03);   // PCA9554 Config (0x03): Bit 7 = Ausgang, Bit 0-6 = Eingang
 bt_cyspp_out = 1;                         // Host treibt CYSPP -> Modul kann keinen Datenkanal oeffnen
#endif
}

void bt_release (void)   // CYSPP loslassen -> Bit 7 wieder Eingang (Status lesbar, bereit fuer Verbindung)
{
#if BT_CYSPP
 // Flag NUR bei erfolgreichem I2C-Zugriff loeschen: sonst gelten die Sicherungen in
 // uart1_release()/Minutentick als erledigt, obwohl die Leitung weiter getrieben wird.
 if (write_i2c_dev(IC54, 2, (0xFF<<8)|0x03))  // PCA9554 Config (0x03): alle Pins Eingang (Werks-/Init-Zustand)
  bt_cyspp_out = 0;                        // Leitung frei -> Modul darf CYSPP selbst auf LOW ziehen
#endif
}

// IF820: offene BT/SPP-Verbindung SAUBER trennen.
// KORREKTUR (5.74): Schritt 2 (5.71) hatte bt_cmdmode() hier ersatzlos entfernt, weil der
// DVK-Nachweis aktives CYSPP-Treiben WAEHREND laufendem Betrieb verbietet. Am Geraet bestaetigt
// falsch fuer DIESEN Fall: ohne erzwungenen Command-Mode erreicht /DIS ein Modul im Datenmodus
// gar nicht (geht als Nutzdaten unter) - das Modul erfaehrt nie vom gewollten Trennen und haelt
// CYSPP dauerhaft LOW (Split-Brain, RS232 wird unbenutzbar: jede Ausgabe wartet auf CTS-Timeout
// Richtung eines Moduls, das nicht mehr antwortet - W11015 im Geraetelog bestaetigt).
// Unterschied zum DVK-Fall: hier wollen wir die Verbindung IMMER bewusst UND SOFORT beenden -
// unabhaengig vom Transport (Classic oder BLE) - das widerspricht dem DVK-Befund nicht, der sich
// auf ungewolltes Stoeren laufender Uebertragung bezieht, nicht auf eine gewollte Terminierung.
// Ablauf: CYSPP HIGH -> Command-Mode, dann /DIS (gap_disconnect) -> Modul baut Verbindung aktiv
// ab. Defensiv: quittiert das Modul /DIS nicht, wird trotzdem freigegeben.
void bt_disconnect (void)
{
 if (fp.btmodem!=IF820) return;
 concpy = connect;                          // WICHTIG: bt_command() stellt am Ende connect=concpy her
                                            // -> ohne dies wuerde connect auf einen veralteten Wert gesetzt
                                            //    (Timeout-Kontext) und Terminal/BT-Routing kaputtgehen.
 bt_cmdmode();                              // CYSPP HIGH -> Command-Mode (bewusste, gezielte Trennung)
 osDelay(150);                              // Modul Zeit geben: Datenmodus -> Command-Mode
 bt_command(T_dis, T_disok, -300);          // /DIS still senden (kein Terminal-Spam), Antwort abwarten
 osDelay(50);                               // kurze Nachlaufzeit fuer die Modul-Antwort
 bt_release();                              // CYSPP wieder als Eingang
 bxi1=rxi1;                                 // FIX (5.89): RX-Reste (/DIS-Antwort) liegen jetzt in rbuf1
 // FIX (5.93): Rennen bt_release() (bt_cyspp_out=0) <-> Aufrufer loescht BT_LINK erst NACH
 // dieser Funktion (measure.c) - in diesem Fenster liest die ISR-Weiche "Bruecke aktiv" (BT_LINK
 // noch gesetzt, bt_cyspp_out schon 0) und haette Nachlauf-Bytes (Rest der /DIS-Antwort, spaete
 // @E,...-Disconnect-Events) faelschlich nach rbuf statt rbuf1 geroutet - dort wuerden sie von
 // mainmenu() als Eingabe fehlinterpretiert (die urspruengliche Selbstnavigations-Bugklasse).
 // Da diese Trennung ohnehin immer bewusst und sofort ist, kann rbuf hier gefahrlos verworfen
 // werden - echte Tastatureingaben sind in diesem Moment ebenfalls hinfaellig.
 bxi=rxi;
 connect = concpy;                          // definierter Zustand (== urspruenglich); Aufrufer maskiert BT-Bits
}

// Bewertung des Paares CYSPP (IC54 Bit7, ueber I2C) / CONNECTION (P0.19, direkt am LPC) - beide low aktiv.
static char *bt_pair_verdict (uchar cyspp, uchar conn)
{
 if (!cyspp && !conn) return "beide LOW  -> verbunden, Datenpipe steht (konsistent)";
 if (cyspp && conn)   return "beide HIGH -> idle, nicht verbunden (konsistent)";
 if (!cyspp)          return "CYSPP LOW, CONNECTION HIGH -> Datenmodus aktiv, Pipe aber noch nicht vollstaendig (BLE ohne Subscribe? Aufbau/Abbau?)";
 return                      "CYSPP HIGH, CONNECTION LOW -> ABWEICHUNG (unerwartet)";
}

void bt_cyspp_monitor (void)   // Live-Vergleich CYSPP/BT_LINK (IC54 Bit7, I2C) UND CONNECTION (P0.19, direkter GPIO)
{                              // CONNECTION = IF820 Pin 33/P27/CONN_IND, seit 5.109 direkt am LPC (vorher IC54 Bit5)
 int st, cfg; uchar now7=1, last7=0xFF, now5, last5=0xFF, sec=0;
 uint cc0=comchange_count;                  // TEMPORAER (5.69): Startwert fuer Delta-Anzeige
 bt_release();                              // sicherstellen: CYSPP als Eingang -> Modul kann treiben
 putln("CYSPP(IC54 bit7) vs. CONNECTION(P0.19) monitor. Now try to connect via BT. Key = exit (max 30s).");
 putstr(" comchange_count bei Start: "); putnumber(cc0,0); newline();  // TEMPORAER (5.69)
 for (sec=0; sec<255; sec++)                // laeuft bis Taste/comchange, aber begrenzt
 {
  st = read_i2cdev(IC54, 0, 1);             // IC54 Eingangsregister lesen
  if (st < 0) putln(" I2C read error (IC54)");
  else now7 = (uchar)((st>>7)&1);           // Bit7 = CYSPP-Leitung (bei I2C-Fehler letzter Wert bleibt)
  now5 = (FIO0PIN & CONN_PIN) ? 1 : 0;      // CONNECTION direkt vom LPC-Pin P0.19 (kein I2C)
  if (st >= 0 && (now7 != last7 || now5 != last5))   // nur bei Aenderung eines der beiden ausgeben
  {
   // DIAGNOSE: treibt UNSER Board Bit7 gerade selbst (Ausgang), oder ist es Eingang - dann kann
   // ein gelesenes LOW nur vom Modul (oder extern) kommen, nicht von uns. Config-Register (0x03):
   // Bit=1 -> Eingang, Bit=0 -> Ausgang (PCA9554-Konvention). bt_cyspp_out zusaetzlich als
   // Software-Sicht (sollte mit der Config uebereinstimmen, wenn nicht: eigener Fehler).
   cfg = read_i2cdev(IC54, 3, 1);           // IC54 Konfigurations-/Richtungsregister lesen
   putstr(" CYSPP bit7 = "); putstr(now7 ? "HIGH" : "LOW ");
   putstr("   CONNECTION P0.19 = "); putstr(now5 ? "HIGH" : "LOW ");
   putstr("   [comchange_count="); putnumber(comchange_count,0); putstr("]"); newline();   // TEMPORAER (5.69)
   putstr("   -> "); putln(bt_pair_verdict(now7, now5));
   putstr("   (Bit7-Richtung="); putstr((cfg>=0) ? (((cfg>>7)&1) ? "Eingang" : "AUSGANG (wir treiben!)") : "?");
   putstr(", bt_cyspp_out="); putnumber(bt_cyspp_out,0); putstr(")"); newline();
   last7 = now7; last5 = now5;
  }
  ResetWDT();
  if (getkey(120) >= 0) break;               // ~120 ms warten; Taste (1) oder comchange (0) -> Ende
 }
 newline(); putstr(" CYSPP/CONNECTION monitor ended. comchange_count: "); putnumber(cc0,0);  // TEMPORAER (5.69)
 putstr(" -> "); putnumber(comchange_count,0);
 putstr(" (Delta="); putnumber(comchange_count-cc0,0); putstr(")"); newline();
}

// Roher EZ-Serial-Event-Mitschnitt auf UART1 (Diagnose, Testmenue): druckt alles, was am
// Modul ankommt, unveraendert am Terminal aus - OHNE es zu interpretieren und OHNE es ans
// Modul zurueckzusenden. Ziel: waehrend ein BLE-Client verbindet/subscribed, die rohen
// @E,...-Eventzeilen sichtbar machen (v.a. gap_connected "C", gatts_data_written "W" beim
// CCCD-Write, und vor allem gap_disconnected "DIS" mit dem reason=-Code - der unterscheidet
// "lokal/Modul-initiiert" (0x0916, passt zur CYSPP-Pin-Theorie) von "Supervision-Timeout"
// (0x0908, eher RF/Brownout).
//
// WICHTIG (5.81-Neubau): Fruehere Fassungen (5.62-5.80) lasen rbuf ueber denselben Index
// (bxi), den auch mainmenu()/PIN-Abfrage benutzen, und blockierten dafuer 180s lang die
// gesamte Hauptschleife. Das machte den Sniffer zum alleinigen "Besitzer" des gemeinsamen
// Ringpuffers waehrend seiner Laufzeit - jede ECHTE Eingabe (Menue, vor allem eine gerade
// laufende BT-PIN-Abfrage) wurde dabei als Rohdaten mitgelesen und verworfen, statt bei
// mainmenu() anzukommen (siehe project-status.md Update 5.79/5.80 fuer die Bug-Historie).
// Neubau: der Sniffer bekommt einen EIGENEN, unabhaengigen Leseindex (sniff_i), der rxi
// genau wie bxi verfolgt, aber bxi selbst nie anfasst. mainmenu() bekommt dadurch exakt
// dieselben Bytes wie ohne Sniffer - der Sniffer ist ein reiner Mitleser, kein Verbraucher.
// Start/Ende laufen nicht mehr blockierend: bt_event_sniff() setzt nur den Startzustand und
// kehrt sofort zurueck, bt_sniff_poll() (aus measure.c's Hauptschleife, einmal je Durchlauf)
// liest non-blocking nach und gibt am RS232-Terminal aus. Kein osDelay(), kein Blockieren -
// das Hauptmenue bleibt waehrend der gesamten 180s normal bedienbar.
// FIX (5.89): Sniff liest aus rbuf1 (UART1-Command-Mode-Puffer) - das ist ohnehin genau der
// Puffer, in dem die interessanten @E,.../@R,...-Ereignisse landen (die treten per Definition
// nur auf, waehrend die CYSPP-Bruecke NICHT aktiv ist).
// FIX (5.92): KEIN eigener Leseindex mehr (fruehere sniff_i) - der war vom eigentlichen
// Ueberlauf-/Wiederfreigabe-Mechanismus (bxi1, siehe UART1_IRQHandler/measure.c) komplett
// entkoppelt. Die 5.91-Ergaenzung ("rbuf1 immer leeren") hielt dadurch nur sniff_i aktuell,
// NICHT bxi1 - der Puffer konnte aus bxi1s Sicht trotzdem volllaufen und den UART1-Interrupt
// dauerhaft abschalten (genau das vom Anwender wiederholt beobachtete "Modul antwortet auf
// KEINER Baudrate mehr" - kein Hardware-Defekt, sondern unser eigener abgeschalteter
// Empfangsinterrupt). Fix: bt_sniff_poll() liest jetzt ueber bxi1 - denselben Index, den auch
// bt_command() und die Ueberlauf-Wiederfreigabe in measure.c benutzen. Nur EIN Verbraucher-
// Index fuer rbuf1, konsistent mit der Ueberlauflogik.
static bool sniff_active=FALSE;
static uint sniff_t0;                         // Startzeitpunkt (mstimer) fuer Dauer und Zeitstempel
static bool sniff_bol;                        // Zeilenanfang (fuer Zeitstempel je Zeile)
#define SNIFF_DURATION_MS  180000UL           // 180s, deckt einen kompletten "--runs 3"-Durchlauf
                                               // von ble_cyspp_probe.py ab (Connect allein dauert
                                               // bei diesem Setup 13-30s je Versuch)

void bt_event_sniff (void)
{
 if (fp.btmodem!=IF820) { putln(" nur IF820"); return; }
 Init_BT_ch(BT_BAUD);                         // UART1 aktiv anfordern (verdraengt GSM/GPS falls noetig,
                                               // wie bei send_bt_info() - MUX schaltet auf BT)
 putln("Event-Sniff (Hintergrund): rohe Modul-Ausgabe, laeuft 180s im Hintergrund mit - Hauptmenue");
 putln("bleibt normal bedienbar. Jetzt BLE/Classic verbinden.");
 bxi1=rxi1;                                   // sauberer Start, alte Reste ignorieren (rbuf1)
 sniff_t0=mstimer;
 sniff_bol=TRUE;
 sniff_active=TRUE;
}

// Non-blocking nachlesen - einmal je Hauptschleifendurchlauf aus measure.c aufgerufen.
// Beruehrt bxi/rbuf-Verbrauch NICHT, daher voellig unabhaengig von mainmenu()/PIN-Abfrage.
// FIX (5.89-Ergaenzung, 5.92-Korrektur): rbuf1 (UART1-Command-Mode-Puffer) wird sonst NUR bei
// aktivem Event-Sniff bzw. waehrend eines bt_command()-Aufrufs geleert - im normalen Betrieb
// (kein Sniff aktiv, kein Kommando gerade unterwegs) liest NIEMAND rbuf1, er kann volllaufen und
// den UART1-Interrupt dauerhaft abschalten. Deshalb wird rbuf1 jetzt IMMER geleert - und zwar
// ueber bxi1, denselben Index, den auch bt_command() und die Ueberlauf-Wiederfreigabe in
// measure.c benutzen (5.91 hatte faelschlich einen SEPARATEN Index sniff_i aktuell gehalten,
// der mit der eigentlichen Ueberlauflogik gar nichts zu tun hatte - rbuf1 konnte aus bxi1s Sicht
// trotzdem volllaufen und den UART1-Empfang dauerhaft lahmlegen). Nur die Anzeige mit
// Zeitstempel ist an sniff_active gebunden, das Leeren selbst nicht mehr.
void bt_sniff_poll (void)
{
 char c; uchar cc;
 if (bxi1!=rxi1)
 {
  if (sniff_active)
  {
   cc=connect; connect=concpy & ~UART1;        // Ausgabe nur ans Terminal, nichts zurueck ans Modul
   while (bxi1!=rxi1)
   {
    c = rbuf1[++bxi1];
    if (c=='\r') continue;                     // CR ueberspringen, LF traegt den Zeilenumbruch
    if (sniff_bol) { putc('['); putnumber(mstimer-sniff_t0,0); putstr("ms] "); sniff_bol=FALSE; }
    putc((c=='\n' || (c>=32 && c<127)) ? c : '.');
    if (c=='\n') sniff_bol=TRUE;
   }
   connect=cc;
  }
  else bxi1=rxi1;                               // kein Sniff aktiv -> nur leeren, nichts anzeigen
 }
 if (!sniff_active) return;
 if ((uint)(mstimer-sniff_t0)>=SNIFF_DURATION_MS)
 {
  sniff_active=FALSE;
  uart1_release(MUX_BT);                      // UART1 wieder freigeben (GSM/GPS koennen zurueck)
  newline(); putln(" Event-Sniff (Hintergrund) beendet (180s abgelaufen).");
 }
}

//-----------------------------------------------------------------------------
// BLE-CYSPP-Transfer-Test (Gegenstueck zum PC-Python-Skript, siehe Doku/tools):
// empfaengt CRC/Sequenz-Frames ueber UART1 (vom PC per BLE-Write transparent durch CYSPP
// durchgereicht), prueft CRC-32 und sendet je gueltigem Frame ein ACK1-Frame zurueck (geht
// ebenso transparent per CYSPP/BLE-Notify an den PC). Wire-Format identisch zum Referenztest
// des Zulieferers (SupplierSourceCode/.../tests/Bluetooth-Simulation/ble/cyspp_dvk_hil.py):
//   Frame: "CYS1" + Sequenz(4B LE) + Groesse(2B LE) + CRC32(4B LE) + Payload
//   ACK:   "ACK1" + Sequenz(4B LE)
//
// TEMPORAERER DIAGNOSE-CODE (bewusste Entscheidung, siehe project-status.md Update 5.67/5.68):
// Nur ueber das Testmenue erreichbar, kein Teil des produktiven BT-Ablaufs. Wieder entfernen,
// sobald die BLE-CYSPP-Diagnose abgeschlossen ist (bt_transfer_echo, crc32_calc, bt_send_ack,
// btio.h-Deklaration, Menuepunkt 8->3 in bt_diag_menu()).
//-----------------------------------------------------------------------------

// CRC-32 (IEEE 802.3 / identisch zu Python zlib.crc32) - bitweise, keine 256-Byte-Tabelle
// noetig (Frames sind klein, kein Zeitdruck).
static uint32_t crc32_calc (const uchar *data, uint len)
{
 uint32_t crc = 0xFFFFFFFF; uint i; uchar j;
 for (i=0; i<len; i++)
 {
  crc ^= data[i];
  for (j=0; j<8; j++)
   crc = (crc>>1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(crc & 1)));
 }
 return crc ^ 0xFFFFFFFF;
}

// ACK-Frame senden. WICHTIG: pchar() direkt statt putc() - putc() wuerde jedes 0x0A-Byte in den
// Binaerdaten faelschlich in "\r\n" verwandeln (LF->CRLF-Übersetzung, siehe sio.c putc()).
static void bt_send_ack (uint32_t seq)
{
 uchar cc=connect; uchar i;
 connect=UART1;
 pchar('A'); pchar('C'); pchar('K'); pchar('1');
 for (i=0; i<4; i++) pchar((uchar)(seq>>(8*i)));   // little-endian, wie Python struct '<4sI'
 connect=cc;
}

// Empfangsschleife: feste Laufzeit, kein frueher Ausstieg (gleicher Grund wie bt_event_sniff,
// 5.62-Fix - rbuf/bxi/rxi ist der gemeinsame UART0/UART1-Ringpuffer).
void bt_transfer_echo (void)
{
 enum { ST_SYNC, ST_HDR, ST_PAYLOAD } state = ST_SYNC;
 uchar buf[270];                              // 14 Byte Header + max. 256 Byte Payload
 uchar sync4[4] = {0,0,0,0};
 uint n=0;                                    // Bytes des aktuellen Frames bereits im Puffer
 uint32_t want_seq=0, want_crc=0, want_size=0;
 uint32_t frames_ok=0, frames_bad=0;
 uchar cc; uint loop; char c;

 if (fp.btmodem!=IF820) { putln(" nur IF820"); return; }
 if (bt_pininit)                              // FIX (5.80): siehe bt_event_sniff() - dieselbe Funktion
 {                                             // verschluckt sonst eine waehrenddessen ueber BT eingegebene
                                               // echte PIN als Diagnose-Rohdaten.
  putln("Transfer-Test: PIN-Abfrage noch offen - erst PIN ueber BT eingeben, dann erneut starten.");
  return;
 }
 Init_BT_ch(BT_BAUD);                         // UART1 aktiv anfordern (wie bt_event_sniff)
 cc=connect; connect=concpy & ~UART1;
 putln("Transfer-Test: wartet auf CYS1-Frames (CRC/Seq), sendet ACK1 zurueck. Laufzeit 120s.");
 connect=cc;
 bxi=rxi;
 for (loop=0; loop<12000; loop++)             // 12000 * 10 ms = 120 s, FEST
 {
  while (bxi!=rxi)
  {
   c = rbuf[++bxi];
   if (state==ST_SYNC)
   {
    sync4[0]=sync4[1]; sync4[1]=sync4[2]; sync4[2]=sync4[3]; sync4[3]=(uchar)c;
    if (sync4[0]=='C' && sync4[1]=='Y' && sync4[2]=='S' && sync4[3]=='1')
    { buf[0]='C'; buf[1]='Y'; buf[2]='S'; buf[3]='1'; n=4; state=ST_HDR; }
   }
   else if (state==ST_HDR)
   {
    buf[n++]=(uchar)c;
    if (n==14)                                // 4 Magic + 4 Seq + 2 Size + 4 CRC
    {
     want_seq  = (uint32_t)buf[4] | ((uint32_t)buf[5]<<8) | ((uint32_t)buf[6]<<16) | ((uint32_t)buf[7]<<24);
     want_size = (uint32_t)buf[8] | ((uint32_t)buf[9]<<8);
     want_crc  = (uint32_t)buf[10] | ((uint32_t)buf[11]<<8) | ((uint32_t)buf[12]<<16) | ((uint32_t)buf[13]<<24);
     if (want_size==0 || want_size>256) { state=ST_SYNC; n=0; }  // unplausibel -> Resync
     else state=ST_PAYLOAD;
    }
   }
   else                                       // ST_PAYLOAD
   {
    buf[n++]=(uchar)c;
    if (n==(uint)(14+want_size))
    {
     if (crc32_calc(&buf[14],(uint)want_size)==want_crc) { frames_ok++; bt_send_ack(want_seq); }
     else frames_bad++;
     state=ST_SYNC; n=0; sync4[0]=sync4[1]=sync4[2]=sync4[3]=0;
    }
   }
  }
  ResetWDT();
  // Bei 8000 B/s fallen pro 10-ms-Poll rund 80 Byte an und bleiben damit sicher unter den
  // etwa 255 nutzbaren Bytes des gemeinsamen Ringpuffers. Die 100-ms-Fassung verlor bereits
  // vor der hier folgenden Wiederfreigabe von UART1 Daten.
  if (rbuf_full && bxi==rxi) { rbuf_full=0; NVIC_EnableIRQ(UART1_IRQn); }
  // 5.78-Fix (wie bt_event_sniff): echten Schnittstellenwechsel waehrend der 120s nicht
  // verschlucken - siehe Kommentar dort.
  if (comchange) { if (!osActiveThread) communication_change(); else clear_comchange(); }
  osDelay(10);
 }
 bxi=rxi;                                     // Rest verwerfen - nichts darf ans Menue durchsickern
 cc=connect; connect=concpy & ~UART1;
 newline(); putstr(" Transfer-Test beendet: "); putnumber(frames_ok,0); putstr(" ok, ");
 putnumber(frames_bad,0); putstr(" fehlerhaft."); newline();
 connect=cc;
 uart1_release(MUX_BT);
}

// Contention-Test (Messprotokoll_CYSPP_Diagnose.md, Schritt 6): IC54 Bit7 (CYSPP-Leitung)
// fuer kurze Zeit aktiv als Ausgang LOW treiben - Gegenstueck zu bt_cmdmode() (das HIGH
// treibt). Ziel: am R78 (100 Ohm, Serienwiderstand in der CYSPP-Leitung) die Spannungs-
// differenz zwischen Host- und Modulseite messen, um eine Bus-Contention zu erkennen, falls
// das Modul die Leitung gleichzeitig anders treibt. Bewusst kurz gehalten (<1s), danach sofort
// bt_release() (alle Pins Eingang, Werks-/Init-Zustand).
void bt_cyspp_contention_test (void)
{
#if BT_CYSPP
 if ((fp.btmodem==Roving) || (fp.btmodem==Laird)) { putln(" nur IF820 (Modul-Guard)"); return; }
 putln("Contention-Test: IC54 Bit7 wird 500 ms aktiv LOW getrieben - jetzt Delta-U ueber R78 messen.");
 // FIX: bt_cyspp_out fehlte hier (im Gegensatz zu bt_cmdmode()) - communication_change() hat das
 // testbedingte LOW deshalb als echten neuen BT-Connect gedeutet (Init_BT_ch, startmessage,
 // bt_pininit ausgeloest), obwohl gar keine reale Verbindung gemeint war. Reproduziert: PIN-Prompt-
 // Endlosschleife bei Nutzung dieses Menuepunkts, faelschlich zunaechst als HW-Defekt gedeutet.
 bt_cyspp_out = 1;                            // Host treibt CYSPP -> keine Status-Fehlinterpretation
 write_i2c_dev(IC54, 2, (0x00<<8)|0x01);      // PCA9554 Output-Register (0x01): Bit 7 = LOW
 write_i2c_dev(IC54, 2, (0x7F<<8)|0x03);      // PCA9554 Config (0x03): Bit 7 = Ausgang
 osDelay(500);                                // kurz halten (< 1 s laut Messprotokoll)
 bt_release();                                // sofort zurueck: alle Pins Eingang (setzt bt_cyspp_out=0)
 putln(" Contention-Test beendet, Leitung wieder Eingang.");
#else
 putln(" BT_CYSPP nicht aktiv (Testmodus) - Contention-Test uebersprungen.");
#endif
}

// Sammel-Einstieg Testmenue (ein Ziffernplatz fuer die BT-Diagnosen): waehlt zwischen Event-
// Mitschnitt, Contention-Test und Transfer-Test.
// WICHTIG (5.68/5.69-Fix): getc()/getbyte() brechen SOFORT mit -1 ab, wenn das globale
// comchange-Flag (Schnittstellenwechsel, gesetzt von EINT1_IRQHandler bei JEDER IC54-
// Pegelaenderung, z. B. die floatende CYSPP-Leitung) beim Aufruf bereits gesetzt ist - noch
// BEVOR ueberhaupt einmal gewartet/yieldet wird. Der 5.68-Fix (bis zu 20 Wiederholungen) half
// deshalb NICHT: ohne jeden Yield dazwischen bekommt nichts anderes im System die Chance,
// comchange zu loeschen - alle 20 Versuche schlagen in Mikrosekunden fehl.
//
// SCHWERER FOLGEFEHLER (5.72-Fix): Die 5.70-Fassung rief bei gesetztem comchange blind
// clear_comchange() auf, um den Merker "wegzuwerfen". Das ist FALSCH, wenn comchange durch
// eine ECHTE BT_LINK-Statusaenderung (z. B. ein Disconnect) ausgeloest wurde: die einzige
// Stelle, die comchange tatsaechlich AUSWERTET und `connect`/BT_LINK korrekt nachfuehrt, ist
// communication_change() (libtool.c), aufgerufen aus measure.c NUR wenn `comchange` (noch)
// gesetzt ist. clear_comchange() loescht den Merker aber OHNE communication_change()
// aufzurufen - die Firmware bekommt den echten Disconnect dann NIE mehr mit, `connect&BT_LINK`
// bleibt für immer auf "verbunden" haengen (Split-Brain: das Modul/Windows haben laengst
// getrennt, unsere Firmware denkt weiter "verbunden" -> jede Terminalausgabe wird weiter
// sinnlos Richtung UART1 gespiegelt und wartet dort auf CTS/THRE-Timeouts -> das beobachtete
// "RS232 wird nach einem BT-Disconnect ploetzlich sehr langsam"). Fix: statt blind zu loeschen,
// wird bei gesetztem comchange jetzt communication_change() selbst aufgerufen (macht die echte
// Verarbeitung UND loescht comchange danach) - mit derselben Schutzbedingung wie in measure.c
// (`!osActiveThread`, GSM/MQTT-Verbindungsthread nicht stoeren).
// TEMPORAER (Alt-Firmware-Vergleichstest): setzt das Modul per /RFAC auf Werkszustand zurueck,
// damit eine alte MCU-Firmware (z. B. 5.50) das Modul wieder frisch/passend erkennen und
// konfigurieren kann - das Modul behaelt seine EZ-Serial-Konfiguration sonst dauerhaft im
// eigenen Speicher, unabhaengig davon, welche MCU-Firmware geflasht ist.
// WICHTIG (5.86-Korrektur): KEIN Power-Cycle zwischen Factory-Reset und Flashen der Zielfirmware!
// Ein Zwischen-Reboot wuerde von der noch installierten AKTUELLEN Firmware (deren eigene
// Werksmodul-Erkennung, siehe init_bluetooth()) sofort wieder erkannt und automatisch mit den
// AKTUELLEN Einstellungen neu konfiguriert - der Factory-Reset waere fuer die Katz, bevor die
// gewuenschte (alte) Firmware ueberhaupt zum Zug kommt. Direkt nach /RFAC die Zielfirmware
// flashen (der Flash-Vorgang bootet direkt in die neue Firmware, kein Zwischenlauf der alten).
void bt_factory_reset (void)
{
 if (fp.btmodem!=IF820) { putln(" nur IF820"); return; }
 Init_BT_ch(BT_BAUD);
 putln("Factory-Reset (/RFAC) - Modul wird auf Werkszustand zurueckgesetzt, Baud -> 115200.");
 // FIX (Layout): bt_command() haengt bei positivem pause KEIN abschliessendes Newline an die
 // "< ..."-Antwortzeile (nur bei RN4678-CRLF-Flag) - ohne eigenes newline() lief die folgende
 // Meldung bisher auf derselben Zeile weiter ("< ...,0000 /RFAC bestaetigt..."), siehe generelle
 // Menue-Layout-Konvention (immer eigene Zeile pro Meldung).
 if (bt_command(T_rfac,T_rfacok,1000)) { newline(); putln(" /RFAC bestaetigt (@R,...,/RFAC,0000)."); }
 else { newline(); putln(" keine /RFAC-Bestaetigung erhalten (evtl. andere Baud/Zustand?)."); }
 uart1_release(MUX_BT);
 putln("WICHTIG: JETZT SOFORT die Zielfirmware flashen - KEIN Power-Cycle vorher (die aktuelle");
 putln("Firmware wuerde das Werksmodul sonst beim naechsten Boot automatisch neu konfigurieren)!");
}

// Anwenderwunsch (2026-09-30): manuell erzwungenes Trennen zum Testen des Split-Brain-Problems
// ("Modul haelt CYSPP LOW, obwohl der reale Peer laengst weg ist") - direkt am Geraet auswertbar,
// ob ein erzwungenes /DIS (statt Power-Cycle) den haengenden Zustand aufloest. Zeigt vorher/nachher
// den rohen IC54-Pegel + Richtungsregister, damit sofort sichtbar ist, ob sich ueberhaupt etwas
// aendert (und ob die Leitung je als Ausgang von UNS getrieben war, siehe bt_cyspp_monitor()).
void bt_force_disconnect_test (void)
{
 int st, cfg;
 if (fp.btmodem!=IF820) { putln(" nur IF820"); return; }
 st = read_i2cdev(IC54, 0, 1); cfg = read_i2cdev(IC54, 3, 1);
 putstr(" VOR /DIS: bit7="); putstr((st>=0)?(((st>>7)&1)?"HIGH":"LOW"):"?");
 putstr(", CONNECTION(P0.19)="); putstr((FIO0PIN&CONN_PIN)?"HIGH":"LOW");
 putstr(", Bit7-Richtung="); putstr((cfg>=0)?(((cfg>>7)&1)?"Eingang":"AUSGANG"):"?");
 putstr(", bt_cyspp_out="); putnumber(bt_cyspp_out,0); newline();
 putln(" erzwinge Trennung: bt_disconnect() (Command-Mode + /DIS)...");
 bt_disconnect();
 connect&=~(UART1|BT_LINK);          // Software-Zustand ebenfalls zuruecksetzen (wie measure.c es tun wuerde)
 bt_pininit=0;
 osDelay(200);                        // Modul Zeit geben, die Leitung nach /DIS freizugeben
 st = read_i2cdev(IC54, 0, 1); cfg = read_i2cdev(IC54, 3, 1);
 putstr(" NACH /DIS: bit7="); putstr((st>=0)?(((st>>7)&1)?"HIGH":"LOW"):"?");
 putstr(", CONNECTION(P0.19)="); putstr((FIO0PIN&CONN_PIN)?"HIGH":"LOW");
 putstr(", Bit7-Richtung="); putstr((cfg>=0)?(((cfg>>7)&1)?"Eingang":"AUSGANG"):"?");
 putstr(", bt_cyspp_out="); putnumber(bt_cyspp_out,0); newline();
}

// Anwenderwunsch (2026-09-30): CYSPP von Hand steuern - HIGH treiben ODER bewusst freigeben
// (Eingang/floatend), beliebig oft umschaltbar, bis der Anwender selbst '9' drueckt. Anders als
// bt_force_disconnect_test() (automatischer Kurzpuls) bleibt der gewaehlte Zustand hier so lange
// stehen, wie der Anwender es will - z. B. um zu testen, ob ein manuell erzwungenes HIGH einen
// haengenden Split-Brain-Zustand (CYSPP LOW ohne echten Peer) aufloest.
void bt_cyspp_manual_control (void)
{
 int c=-1, st, cfg; uchar tries;
 if (fp.btmodem!=IF820) { putln(" nur IF820"); return; }
 putln("CYSPP manuell steuern:  1 = HIGH treiben (Ausgang)   2 = Freigeben (Eingang/floatend)   9 = zurueck");
 for (;;)
 {
  st = read_i2cdev(IC54, 0, 1); cfg = read_i2cdev(IC54, 3, 1);
  putstr(" bit7="); putstr((st>=0)?(((st>>7)&1)?"HIGH":"LOW"):"?");
  putstr(", CONNECTION(P0.19)="); putstr((FIO0PIN&CONN_PIN)?"HIGH":"LOW");
  putstr(", Richtung="); putstr((cfg>=0)?(((cfg>>7)&1)?"Eingang":"AUSGANG"):"?");
  putstr(", bt_cyspp_out="); putnumber(bt_cyspp_out,0);
  putstr("   (1=HIGH treiben, 2=Freigeben, 9=zurueck) "); newline();
  for (tries=0, c=-1; tries<20; tries++)              // wie bt_diag_menu(): kein Busy-Spin
  {
   c = getc(TERMINAL_CHAR_TIMEOUT);
   if (c=='1' || c=='2' || c=='9') break;
   ResetWDT();
   if (c<0) osDelay(50);
  }
  if (c=='1') { bt_cmdmode(); putln(" -> CYSPP jetzt aktiv HIGH getrieben (bleibt so, bis du 2 waehlst)."); }
  else if (c=='2') { bt_release(); putln(" -> CYSPP jetzt Eingang/floatend (Modul darf die Leitung selbst treiben)."); }
  else if (c=='9') { if (bt_cyspp_out) bt_release(); break; }  // sicher aufraeumen: nie HIGH haengen lassen
  else putln(" keine gueltige Eingabe - Status erneut anzeigen.");
 }
}

void bt_diag_menu (void)
{
 int c=-1; uchar tries; uint cc0=comchange_count;   // TEMPORAER (5.69): Startwert fuer Delta-Anzeige
 putln("BT-Diagnose:  1 = Event-Sniff   2 = Contention-Test (Schritt 6)   3 = Transfer-Test (CRC/ACK)");
 putln("              4 = Factory-Reset (/RFAC, Alt-FW-Vergleich!)   5 = Force-Disconnect (/DIS)");
 putln("              6 = CYSPP manuell HIGH/Freigeben   9 = zurueck");
 putstr(" comchange_count: "); putnumber(cc0,0); newline();  // TEMPORAER (5.69, CYSPP-Diagnose)
 for (tries=0; tries<20; tries++)
 {
  if (comchange)                            // echten Schnittstellenwechsel richtig verarbeiten
  { if (!osActiveThread) communication_change(); else clear_comchange(); }
  c = getc(TERMINAL_CHAR_TIMEOUT);
  if (c=='1' || c=='2' || c=='3' || c=='4' || c=='5' || c=='6' || c=='9') break;
  if (c<0) osDelay(50);                    // kurz warten, dann erneut versuchen (statt Busy-Spin)
 }
 putstr(" comchange_count danach: "); putnumber(comchange_count,0);           // TEMPORAER (5.69)
 putstr(" (Delta="); putnumber(comchange_count-cc0,0); putstr(", "); putnumber(tries,0); putstr(" Versuche)"); newline();
 if (c=='1' || c=='2' || c=='3' || c=='4' || c=='5' || c=='6' || c=='9') { putc((char)c); newline(); }
 else { newline(); putln(" keine gueltige Eingabe (staendiger Schnittstellenwechsel?) - abgebrochen."); }
 if (c=='1') bt_event_sniff();
 else if (c=='2') bt_cyspp_contention_test();
 else if (c=='3') bt_transfer_echo();
 else if (c=='4') bt_factory_reset();
 else if (c=='5') bt_force_disconnect_test();
 else if (c=='6') bt_cyspp_manual_control();
 // c=='9' (zurueck) oder ungueltig -> einfach zurueck ins Testmenue, kein Diagnoseaufruf
 // HINWEIS (5.89): Der fruehere Flush hier (5.88, "bxi=rxi vor mainmenu()") ist nicht mehr
 // noetig - Command-Mode-/Protokollbytes (Event-Sniff, Factory-Reset, Contention-Test) landen
 // jetzt an der Quelle (UART1_IRQHandler, hard.c) in rbuf1 statt in rbuf und koennen mainmenu()
 // gar nicht mehr erreichen. bt_transfer_echo() (echte Transparent-Daten, bleibt auf rbuf) macht
 // seinen eigenen Flush bereits selbst am Ende.
}

//-----------------------------------------------------------------------------
// Gebremste IF820-Erkennung: /PING auf EINER Baud mit HARTER Zeitgrenze +
// Roh-RX-Dump am Terminal. Kein Haenger mehr, wenn bei falscher Baud Muell
// hereinkommt (wait_message wuerde sonst pro Byte den Timeout verlaengern).
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Diagnose-Ausgaben nur ans Terminal (nicht ins BT-Modul) - Ablaufverfolgung
//-----------------------------------------------------------------------------
static void bt_msg (char *m)              // Klartext-Zeile ans Terminal
{ uchar cc=connect; connect=concpy & ~UART1; newline(); putstr(m); connect=cc; }
static void bt_msgnum (char *m, uint32_t v)  // Klartext + Zahl ans Terminal
{ uchar cc=connect; connect=concpy & ~UART1; newline(); putstr(m); putnumber(v,0); connect=cc; }

static int bt_scan_ping (uint32_t baud)   // 1 = "/PING,0000" gesehen, sonst 0
{
 char buf[96]; int n=0, i; uchar cc; uint deadline;
 bt_tx_stuck=0;                                         // TX-Steckt-Zaehler fuer diese Baud nullen
 bt_msgnum(" scan @", baud);                            // welche Baud - VOR jeder Aktion
 Init_BT_ch(baud);
 bt_msg("  [ch] send CR");
 cc=connect; connect=UART1; putstr(T_CR); connect=cc;   // Modul-Parser: (Muell-)Zeile abschliessen (5.98)
 osDelay(20); bxi1=rxi1;                                // kurz setteln, dann RX-Puffer leeren (rbuf1, 5.89)
 bt_msg("  [CR] send /PING");
 cc=connect; connect=UART1; putstr(T_ping); connect=cc; // /PING senden (cts_bypass aktiv -> ohne CTS-Gate)
 bt_msg("  [PING] wait 250ms for reply");
 deadline=mstimer+250;                                  // HARTE Grenze 250 ms, unabhaengig von RX-Aktivitaet
 while ((int)(deadline-mstimer)>0)
 {
  ResetWDT();
  while (bxi1!=rxi1 && n<(int)sizeof(buf)-1) buf[n++]=rbuf1[++bxi1];
  osThreadYield();
 }
 buf[n]=0;
 cc=connect; connect=concpy & ~UART1;                   // Diagnose nur ans Terminal, nicht zurueck ans Modul
 putstr("  rx: ");
 if (!n) putstr("(none)");
 else for (i=0;i<n;i++) putc((buf[i]>=32 && buf[i]<127)?buf[i]:'.');
 putstr("  [TXstuck="); putnumber(bt_tx_stuck,0); putstr("]");
 newline(); connect=cc;
 return (strstr(buf,"/PING,0000")!=0) ? 1 : 0;
}

void init_bluetooth (void)		// Bluetooth Modem initialisieren
{ 		
 int32_t result=0;	
 uint32_t pin=0; uint32_t detbaud=0;	
 uint8_t retry=4;
	
 ResetWDT(); 
 if (uart1_owner==MUX_GSM) return;      // UART1 gerade von GSM belegt -> nicht stoeren (Konfliktschutz)
 { uint sp=atoi(fp.serno+4);                // keine gueltige Seriennummer? -> BT-Name/PIN unmoeglich
   if (!isdigit((int)fp.serno[0]) || !sp)   // FRUEH erkennen, bevor das Modul angefasst wird
   { newline(); dtcerr(DTC_BT_NO_SERIAL); newline(); return; } }
 // Schritt 3 CYSPP-Plan (5.73): kein bt_cmdmode() mehr - DVK-Nachweis, CYSPP darf NIE aktiv
 // getrieben werden, auch nicht kurz zum Command-Mode-Erzwingen. Bewusster Rueckbau auf den
 // Stand vor Update 5.05: besteht eine aktive BT/BLE-Verbindung, scheitert die Erkennung unten
 // einfach (Modul bleibt transparent, /PING kommt auf keiner Baud durch) - Reconfig waehrend
 // einer aktiven Verbindung braucht dann wieder manuelles Trennen oder RS232 (wie vor 5.05
 // dokumentiert und akzeptiert).
 connect&=~(UART1|BT_LINK);             // virtuelle BT-Verbindung loesen (Befehle -> Modul, nicht ueber SPP)
 fp.btmodem=0;                          // Reset -> Erkennung laeuft (erneut) durch
 // ===========================================================================
 // IF820-Erkennung + Konfiguration (5.27) - klarer, idempotenter Ablauf:
 //  Erkennung : Modul + Baud finden (konfiguriert -> /PING@460800 mit Handshake;
 //              sonst Baud-Scan OHNE CTS, weil Werksmodul F=00 sein CTS HIGH haelt)
 //  Schritt 1/2: Baud 460800 + Flow Control F=01 (STU setzt beides) - nur wenn noetig
 //  Schritt 3 : Geraetename VIASIS_<serno> - nur schreiben wenn abweichend
 //  Schritt 4 : nur wenn etwas geaendert -> /RBT + Verifikation; sonst sofort fertig
 // ===========================================================================
 {
  uchar changed=0;

  { uchar cc=connect; connect=concpy&~UART1; newline(); putstr(" BT init (fw "); putstr(T_version); putstr("): command mode ok"); connect=cc; }  // Version dynamisch aus T_version -> kein Auseinanderlaufen

  // --- Erkennung A: konfiguriertes Modul? /PING @460800 MIT Handshake ---
  cts_bypass=0;
  if ((FIO2PIN&CTS)==0)                 // CTS schon LOW -> Modul meldet sich bereit
  {
   bt_msg(" Detection: probe 460800 with handshake");
   Init_BT_ch(IF820_BAUD); bt_command(T_CR,T_pingok,-60);
   if (bt_command(T_ping,T_pingok,300)) detbaud=IF820_BAUD;
  }
  // --- Erkennung B: sonst Werks-/Fehlzustand -> Baud-Scan OHNE CTS ---
  if (!detbaud)
  {
   static const uint32_t bdscan[] = { IF820_FACTORY_BAUD, IF820_BAUD, 921600, 307200, 230400, 9600 };
   uchar k;
   cts_bypass=1;                        // Werksmodul (F=00) -> ohne CTS-Handshake senden
   bt_msg(" Detection: baud scan without CTS (factory module)");
   for (k=0; k<sizeof(bdscan)/sizeof(bdscan[0]); k++)
    if (bt_scan_ping(bdscan[k])) { detbaud=bdscan[k]; break; }
  }

  if (detbaud)                          // === IF820 gefunden ===
  {
   fp.btmodem=IF820; interfaces|=BT_LINK;
   pin=atoi(fp.serno+4);
   if (isdigit(fp.serno[0]) && pin) fp.btpin=atoi(fp.serno)*10000+pin;  // 6-stellige App-Layer-PIN
   bt_msgnum(" -> IF820 found at baud ",detbaud);
   put2str(T_bt,T_if820); putstr(T_dpkt); putnumber(IF820_BAUD,0); newline();
   bt_command(T_dis,T_disok,500); osDelay(300);          // evtl. offene SPP-Verbindung schliessen

   // --- Schritt 1/2: Baud 460800 + Flow Control F=01 (STU setzt beides gemeinsam) ---
   if (detbaud!=IF820_BAUD || bt_get_flowcontrol()!=1)
   {
    bt_msg(" Step 1/2: set baud 460800 + flow control F=01");
    if (bt_set_flowcontrol()) bt_msg("   set and saved to flash (/SCFG)");
    else                      bt_msg("   ERROR while setting/saving");
    changed=1;
    cts_bypass=0;                       // Modul ist jetzt F=01 -> ab hier Handshake
   }
   else bt_msg(" Step 1/2: baud 460800 + F=01 already ok");
   cts_bypass=0;                        // Name/Verifikation immer mit Handshake

   // --- Schritt 3: Geraetename VIASIS_<serno> ---
   if (bt_get_name()==1) bt_msg(" Step 3: name already correct");
   else
   {
    bt_msg(" Step 3: write name VIASIS_<serno>");
    if (bt_set_name()) { changed=1; bt_msg("   name set"); }
    else               { dtcerr(DTC_BT_NAME_SET); newline(); }
   }

   // --- Schritt 3b: Sniff-Mode aus + Class of Device seriell (Durchsatz + Windows-Kategorie) ---
   // Idempotenz ueber die CoD (GBTDC), NICHT ueber das Sniff-Flag (GBTP F liest immer 0).
   if (bt_get_cod()==1) bt_msg(" Step 3b: serial CoD + sniff off already ok");
   else
   {
    bt_msg(" Step 3b: disable sniff + set serial CoD");
    if (bt_set_btcfg()) { changed=1; bt_msg("   sniff off + serial CoD set (/SCFG)"); }
    else                bt_msg("   WARNING: could not set sniff/CoD (non-fatal)");
   }

   // --- Schritt 3c: BLE-Advertising dauerhaft (Kunde kann jederzeit verbinden) ---
   if (bt_get_adv()==1) bt_msg(" Step 3c: permanent advertising already ok");
   else
   {
    bt_msg(" Step 3c: set permanent advertising");
    if (bt_set_adv()) { changed=1; bt_msg("   permanent advertising set (/SCFG)"); }
    else              bt_msg("   WARNING: could not set advertising (non-fatal)");
   }

   // --- Schritt 3d: Command-Echo aus (A1, MCU-Host) ---
   if (bt_get_echo()==1) bt_msg(" Step 3d: command echo off already ok");
   else
   {
    bt_msg(" Step 3d: disable command echo");
    if (bt_set_echo()) { changed=1; bt_msg("   echo off (/SCFG)"); }
    else               bt_msg("   WARNING: could not set echo (non-fatal)");
   }

   // --- Schritt 3e: CYSPP-Paketierung an 460800 (A2, Wait 5 ms -> 1 ms) ---
   if (bt_get_pkt()==1) bt_msg(" Step 3e: CYSPP packetization already ok");
   else
   {
    bt_msg(" Step 3e: set CYSPP packetization (W=1ms)");
    if (bt_set_pkt()) { changed=1; bt_msg("   packetization set (/SCFG)"); }
    else              bt_msg("   WARNING: could not set packetization (non-fatal)");
   }

   // --- Schritt 3f: Just-Works-Security festschreiben (A5) ---
   if (bt_get_sec()==1) bt_msg(" Step 3f: just-works security already ok");
   else
   {
    bt_msg(" Step 3f: set just-works security");
    if (bt_set_sec()) { changed=1; bt_msg("   security set (/SCFG)"); }
    else              bt_msg("   WARNING: could not set security (non-fatal)");
   }

   // --- Schritt 4: nur bei Aenderung speichern/rebooten + verifizieren ---
   if (changed)
   {
    bt_msg(" Step 4: change -> reboot (/RBT) + verify");
    bt_reboot();                        // Reboot: F=01 aus Flash, Classic-Name live (wartet auf Boot)
    cts_bypass=0;
    { uchar v; result=0;               // Modul braucht nach Reboot Anlaufzeit -> mehrere Versuche
      for (v=0; v<8 && !result; v++)    // FIX (5.94): 4->8 Versuche (verdoppelt), da ein frisches
                                        // Werksmodul beim ALLERERSTEN Reboot (Name+CoD+Advertising+
                                        // Paketierung+Security in einem Zug neu ins Modulflash
                                        // geschrieben) laenger braucht als ein Reboot mit weniger
                                        // Aenderungen (siehe F11018 auf einem brandneuen Board)
      { Init_BT_ch(IF820_BAUD); bt_command(T_CR,T_pingok,-60);
        result=bt_command(T_ping,T_pingok,300);
        if (!result) { bt_msg("   waiting for module after reboot..."); osDelay(400); } }
      if (!result)                      // nach mehreren Versuchen keine Antwort -> echter Fehler
      {
       // FIX (5.85): eigener, spezifischerer Fehlercode statt des generischen DTC_BT_UART_INIT -
       // damit dieser konkrete Fall (Modul antwortet nach /RBT nicht) im Protokoll von anderen
       // BT-UART-Init-Fehlern unterscheidbar ist.
       newline(); dtcerr(DTC_BT_REBOOT_VERIFY); newline();   // F11018
       fp.btmodem=0; connect&=~(UART1|BT_LINK);
       bt_release(); uart1_release(MUX_BT); connect=concpy; clear_comchange(); return;
      }
      bt_msg("   verify after reboot OK");
      puterror_dtc(DTC_BT_REBOOT_VERIFY_OK, v);   // I11019 - nur geloggt, wenn Retries noetig waren (v>0)
    }
   }
   else bt_msg(" Step 4: nothing changed -> done (no save/reboot)");

   bt_show_name();
   bt_release(); uart1_release(MUX_BT); connect=concpy; osDelay(10); clear_comchange();
   bt_msg("BT init done: IF820, 460800, F=01");
   return;
  }
  bt_msg(" -> no IF820 found");
  cts_bypass=0;                         // kein IF820 -> Legacy-Fallback (Laird/RN4678)
 }
 bt_release();          // CYSPP sicher als Eingang (falls Reconfig Command-Mode erzwang, IF820 aber nicht erkannt)
 bt_msg(" Fallback: no IF820 -> quick Laird/RN4678 detection");
 // === Ende IF820-Erkennung; sonst weiter mit Laird/RN4678 ===
 if (!Init_BT_ch(460800)) 							// Konfiguriere uart1 und setze MUX Kanal auf BT modem, Abbruch bei CTS hi
 { newline(); dtcerr(DTC_BT_UART_INIT); return; }	 // putnumber(FIO2PIN,0x80);
 pin=atoi(fp.serno+4); 								// letzte 4 Digits der Seriennummer auswerten	
 if (!isdigit(fp.serno[0]) | !pin)    // Abbruch wenn Seriennummer nicht gesetzt oder letzte 4 Digits null
 { newline(); dtcerr(DTC_BT_NO_SERIAL); newline(); return; } 	
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
   newline();														// Neue Zeile f�r n�chstes Kommandos		
	 if      (retry==3) { Init_UART1 (115200); putnumber(115200,0); newline(); }	// teste 115200 Baud
	 else if (retry==2) { Init_UART1 (9600);   putnumber(9600,0);   newline(); }	// teste 9600 Baud
	 else if (retry==1) { Init_UART1 (307200); putnumber(307200,0); newline(); }	// teste 307200 Baud
	 connect=UART1;															// Nur an UART1 senden
	 putstr(T_AT);															// Sende AT\r um Modul AT Parser zurueckzusetzen
	 connect=concpy;														// Verbindung wiederherstellen
	 osDelay(150);															// Warte kurz auf Modul-Antwort (OK oder ERROR)
	 bxi1=rxi1;										// FIX (5.93): fp.btmodem noch 0 -> ISR routet nach rbuf1, nicht rbuf
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
	  result = wait_message1 (T_OKNZ,300);			// FIX (5.93): fp.btmodem noch 0 -> rbuf1, nicht wait_ok_time/rbuf
	  connect&=~UART1;									// nicht an BT Modem senden
    putstr(cbuf);		  								// Ergebnis an Terminal
   } // end if Echo aus ok		
  } // end if Antwort erhalten 
 
  if (result>0) {  													// Laird, weiter mit hoher Baudrate	
   if (bt_command(T_S502,T_OKNZ,100)) {			// Sende ATS502=1, Authentifizierung f�r eingehende Verbindungen
    if (bt_command(T_S504,T_OKNZ,100)) { 		// Sende ATS504=1, Autoresponse, suppress messages
		 if (bt_command(T_S507,T_OKNZ,100)) { 	// Sende ATS507=2, do not scan for ^^^ local mode ESC sequence
		  if (bt_command(T_S508,T_OKNZ,100)) {	// Sende ATS508=1000,	page scan interval 1 sec.
			 connect|=UART1;											// Sende an Terminal und BT Modem
			 putstr(T_BTK);											// AT+BTK="
			 putnumber(fp.btpin,0xC4);					// 4 stellige Pinnummer
			 putc('"');													// Abschluss-Quote
			 bxi1=rxi1;											// FIX (5.93): rbuf1, fp.btmodem noch 0
			 putc(CR);													// Kommandoabschluss
			 if (wait_message1(T_OKNZ,300)>0)						// OK erhalten? Setze PIN
		   {	
			  connect=concpy;
			  putstr(cbuf);											// Ergebnis an Terminal
			  connect|=UART1;										// Sende an Terminal und BT Modem
			  putstr (T_BTN);								 			// Baue "Bluetooth device (friendly) name" auf
			  putstr (T_viasis);									// Text "VIASIS"		
			  putc ('_');													// Understrich
			  putstr (fp.serno);									// Serienummer
			  putc ('"');
			  bxi1=rxi1;													// Empfangspuffer leeren vor CR
			  putc(CR);		
			  result = wait_message1 (T_OKNZ,300);				// OK Antwort erhalten?
			  osDelay(100); 
			  connect=concpy;
			  putstr(cbuf); 
       } } } } } } // end if result>0 			 
			 
  if (result>0) {	
	 if (bt_command(T_S514,T_OKNZ,100)) {			// Setze Timeout Verbindungsschl�ssel auf 60s, OK erhalten?	
	  if (bt_command(T_S512,T_OKNZ,100)) {		// Sende ATS512=4 Modem sicht- und verbindbar, OK erhalten?
     if (bt_command(T_S538,T_OKNZ,100)) {		// Sende ATS538=1 Sichere last pairing in data base, OK erhalten?		 
		  if (bt_command(T_ATW,T_OKNZ,1000)) { 	// Sende AT&W - sichere Parameter, OK erhalten?
       if (bt_command(T_ATZ,T_OKNZ,2000))  	// Sende ATZ - Reset mit neuen Parametern, OK erhalten? 			 
		   {
				ResetWDT();													// watchdog reset	
				fp.btmodem=Laird;										// Laird BT modem gefunden und konfiguriert
				interfaces|=BT_LINK;								// BT interface ist aktiv	
				} } } } } } // end if result>0
 
  if (fp.btmodem==0)												// Kein Laird erkannt -> pr�fen ob RN4678 vorhanden
	{
	 dtcerr(DTC_BT_LAIRD_NF); newline();   // I11016 (Info: kein Laird gefunden)
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
		 newline();																																// Neue Zeile f�r n�chstes Kommandos	
		 if      (retry==3) { Init_UART1 (9600);   putnumber(9600,0);   newline(); }	// teste 9600 Baud
		 else if (retry==2) { Init_UART1 (307200); putnumber(307200,0); newline(); }	// teste 307200 Baud
		 else if (retry==1) { Init_UART1 (460800); putnumber(460800,0); newline(); }	// teste 460800 Baud
		 osDelay(250);															// Silence gap fuer $$$ Kommandomodus Erkennung
		 bxi1=rxi1;												// FIX (5.93): fp.btmodem noch 0 -> ISR routet nach rbuf1, nicht rbuf
		}
		
		if (result>0) result=bt_command(T_sf1,T_AOK,12000); // Reset to Factory default
		if (result>0)																				// Factory Reset erfolgreich?
		{		
		 result=bt_command(T_sr1,T_reb,10500);							// Reboot Command
		 Init_BT_ch(115200);																// Factory default Baudrate nach SF,1
		 if (wait_message1(T_boot,13000)<0) result=0;				// Warte 3s auf "Reboot" Nachricht 											
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
			 bxi1=rxi1;																					// Flush RX buffer before $$$
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
 if (fp.btmodem==0) { newline(); dtcerr(DTC_BT_RN_NF); newline(); dtcerr(DTC_BT_NOT_CONFIGURED);	// I11017 + F11010
	 newline(); }
}	

bool test_BT	(void)							// Pr�fe ob BT Modul antwortet
{
 bool result=FALSE;
 uint32_t baud=BT_BAUD;																// Baudrate BT
	

 if (fp.btmodem==IF820)                       // IF820: Lebenstest per /PING
 {
  if (Init_BT_ch(baud))                       // Parser leeren (kalter Erst-Ping) + ein Retry
  { bt_command(T_CR,T_pingok,-60);
    result=bt_command(T_ping,T_pingok,300);
    if (!result) result=bt_command(T_ping,T_pingok,300); }
  uart1_release(MUX_BT);
  return(result);                             // KEIN puterror: Boot-Lebenstest; echten Fehler meldet init_bluetooth (F11005)
 }
 if (Init_BT_ch (baud))									// Konfiguriere uart1 und setze MUX Kanal auf BT modem
 {
	if (fp.btmodem>Laird)									// RN4678?
  { 
	 result=bt_command(T_$,T_cmdp,0);									// Sende $$$, warte auf CMD> prompt		
	 if (result) result = bt_command(T_min,T_end,0);	// Return to data mode	
	}
	else result=bt_command(T_AT,T_OKNZ,0);			// Laird - Sende AT - OK erhalten? -> TRUE R�ckgabe
 } // end if Init_BT_ch
 uart1_release(MUX_BT);														// Release UART1
 if (!result) puterror (DTC_BT_AT_FAIL,-1);	// Fehlermeldung
 return(result);
}	

// IF820-Abfrage: Kommando senden, KOMPLETTE Roh-Antwort ans Terminal, RX-Puffer danach leeren.
// Wichtig: feste Wartezeit statt Echo-Match (bt_command matcht sonst das Echo und laesst die
// eigentliche @R-Antwort im Puffer -> Bytes landen als Tastendruck im Menue -> Menue rast durch).
static void bt_query_show (char *cmd)
{
 char buf[128]; int n=0, i; uchar cc; uint deadline;
 cc=connect; connect=concpy & ~UART1;                     // "> gesendet" nur ans Terminal (ohne CR)
 newline(); putstr("  > ");
 for (i=0; cmd[i] && cmd[i]!='\r' && cmd[i]!='\n'; i++) putc(cmd[i]);
 connect=cc;
 osDelay(2); bxi1=rxi1;                                   // alten Pufferinhalt verwerfen (rbuf1, 5.89)
 cc=connect; connect=UART1; putstr(cmd); connect=cc;      // Kommando NUR ans Modul
 deadline=mstimer+400;                                    // feste Antwortzeit (keine Echo-Abhaengigkeit)
 while ((int)(deadline-mstimer)>0)
 { ResetWDT();
   while (bxi1!=rxi1 && n<(int)sizeof(buf)-1) buf[n++]=rbuf1[++bxi1];
   osThreadYield();
 }
 buf[n]=0;
 cc=connect; connect=concpy & ~UART1;                     // "< empfangen" nur ans Terminal
 putstr("  < ");
 if (!n) putstr("(no response)");
 else for (i=0; i<n; i++) if (buf[i]>=32 && buf[i]<127) putc(buf[i]);
 newline(); connect=cc;
 bxi1=rxi1;                                               // Rest sicher verwerfen -> keine Bytes ins Menue
}

// Durchsatz-Diagnose (5.53): Zaehler aus putb() anzeigen. Nur Anzeige, kein Eingriff.
// Lesart: Bremst das Modul haeufig per CTS, kostet jeder gebremste Byte mindestens 1 ms
// (osDelay) statt der ~22 us Uebertragungszeit bei 460800 Baud. "lost" ist die dadurch
// verlorene Zeit in Sekunden - steht die in der Groessenordnung der Downloaddauer, ist
// die Ursache bewiesen.
void bt_show_counters (void)
{
 uchar cc=connect; connect=concpy & ~UART1;      // nur ans Terminal, nicht ins Modul
 putstr("  < TX bytes=");   putnumber(bt_tx_bytes,0);
 putstr(" ctswait=");       putnumber(bt_cts_wait,0);
 putstr(" sleeps=");        putnumber(bt_cts_sleep,0);
 putstr(" lost=");          putnumber(bt_cts_sleep/1000,0); putstr("s");
 putstr(" ctsTO=");         putnumber(cts_timeout,0);
 putstr(" TXstuck=");       putnumber(bt_tx_stuck,0);
 putstr(" USBstuck=");      putnumber(usb_tx_stuck,0);
 newline(); connect=cc;
}

void bt_reset_counters (void)   // Zaehler nullen - je BT-Sitzung von vorn zaehlen
{
 bt_tx_bytes=0; bt_cts_wait=0; bt_cts_sleep=0; bt_tx_stuck=0; usb_tx_stuck=0;
}

void send_bt_info (void)							// Bluetooth Informationen ausgeben
{
 bool result=FALSE;
 uint32_t baud=BT_BAUD;


 if (fp.btmodem==IF820)                       // IF820: Firmware-Version + Einstellungen abfragen
 {
  if (Init_BT_ch(baud)) result=bt_command(T_ping,T_pingok,300);   // Lebenszeichen (Command-Mode noetig)
  if (result)
  {
   newline(); put2str(T_bt,T_if820); newline();                   // "BT: IF820"
   bt_query_show(T_qfv);                                          // Firmware/Stack/Protokoll/HW (E=/S=/P=/H=)
   bt_query_show(T_gtu);                                          // UART: Baud (B=), Flow Control (F=01/00), 8N1
   bt_query_show("GDN,T=00\r");                                   // BLE-Name  (N=VIASIS_<serno>_BLE)
   bt_query_show("GDN,T=01\r");                                   // Classic-Name (N=VIASIS_<serno>)
   bt_query_show(T_gba);                                          // Bluetooth MAC-Adresse (A=)
   bt_query_show(T_cypget);                                       // CYSPP-Parameter (E=enable,G=role,C=,L=,R=,M=)
   bt_query_show(T_gbtdc);                                        // Class of Device (C=) - Windows-Geraetekategorie
   bt_query_show(T_gbtp);                                         // Classic-Parameter, F Bit0 = Sniff-Mode
   bt_query_show(T_gap);                                          // Adv-Parameter (5.55): O=0000 = dauerhaft
   bt_query_show(T_gpem);                                         // Echo (5.57): M=00 = aus
   bt_query_show(T_cypgk);                                        // CYSPP-Paketierung (5.57): W=01 = 1 ms
   bt_query_show(T_gsbp);                                         // Security (5.57): M=00 = Just Works
   bt_query_show(T_rlh_ppcp);                                     // Verbindungsparameter-Praeferenz (5.6x): D=0C00180000005802
   bt_show_counters();                                            // Durchsatz-Diagnose der laufenden BT-Sitzung
  }
  osDelay(50); bxi1=rxi1;                                         // finaler Flush (rbuf1, 5.89)
  uart1_release(MUX_BT);
  if (!result) puterror(DTC_BT_PING,-1);
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
				if (ja(T_dellst)>0)											// Abfrage "Liste l�schen?"
					result=bt_command(T_BTDM, T_OKNZ, 0);	// Clear trusted device list
	   } } } } }
 newline();	
 uart1_release(MUX_BT);														// Release UART1		
 if (!result) puterror (DTC_BT_BTT_FAIL,-1);	// Fehlermeldung
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
 result=getline(btnamebuf,maxlen,'a');			// Abfrage Bluetooth Ger�tename
 if (result>0) 															// Eingabe erfolgt?				
 {
	Init_BT_ch (baud);												// Konfiguriere uart1 und setze MUX Kanal auf BT modem 				
	if (fp.btmodem==IF820)
	{
	 char cmd[60];
	 strcpy(cmd,"SDN$,T=00,N="); strcat(cmd,btnamebuf); strcat(cmd,"\r"); result=bt_command(cmd,T_sdnok,500);
	 if (result) { strcpy(cmd,"SDN$,T=01,N="); strcat(cmd,btnamebuf); strcat(cmd,"\r"); result=bt_command(cmd,T_sdnok,500); }
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
	if (!result) puterror (DTC_BT_CR_FAIL,-1);	// Fehlermeldung
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
		  fp.btpin=pin;												// PIN �bernehmen		
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
    if (result) fp.btpin=pin;								// PIN �bernehmen		 
   }		 
  }	// end if Init_BT_ch	
	
	connect=concopy; 
	if (!result) puterror (DTC_BT_PIN_FAIL,-1);	// Fehlermeldung
	else putln (T_OKNZ);	
 }
 uart1_release(MUX_BT);														// Release UART1
}




