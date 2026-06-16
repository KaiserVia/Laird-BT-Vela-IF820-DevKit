//-----------------------------------------------------------------------------
//  FILE: sictxt.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Texte deutsch/französisch/italienisch
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
//-----------------------------------------------------------------------------
//	COMPILATION: THUMB CODE
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   30.11.2010
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//              	30.11.2010: File creation
//-----------------------------------------------------------------------------

#include "../sictxt.h"	

#if ((LANGUAGE == d_f_i)|(LANGUAGE == d_f_i_kry))

// Sprachspezifische Texte
text T_ja[ANZSPRACHEN]			= {'j','o','s'};		// Antwortkürzel für ja
text T_nein[ANZSPRACHEN]   		= {'n','n','n'};		// dto. für nein
text T_spritm[ANZSPRACHEN][10] 	= {"German","French","Italian"};
text T_jn[]						= "\f (j/n)?\f (o/n)?\f (s/n)?";
text T_tver[]			= "\fVersion \fVersion \fVersione ";
text T_winit[]    = "\fWerkinitialisierung\fInitialiser les paramètres\fSet parametri default";
text T_main[]			= "\f\nHauptmenü:\n1. Datenausgabe\n2. Online messen\n3. Testfunktionen\n4. Einstellungen\n5. Funkmodems\n6. Information und Zeit\
\f\nMenu principal:\n1. Transfert des données\n2. Affichage en connexion PC\n3. Test du système\n4. Changer les paramètres\n5. Les modems radio\n6. Information et temps\
\f\nMenu principale:\n1. Emissione dati\n2. Misure online\n3. Funzioni di prova\n4. Opzioni\n5. Radio modem\n6. Informazioni e tempo";
text T_zuruck[]		= "\fzurück\fRetour\fIndietro";
text T_ausw[]			= "\fIhre Auswahl 1...\fVotre choix 1...\fVostra selezione 1...";
text T_estmenu[] 	= "\fTestmenü:\n1. LED-Anzeige\n2. Flash Speicher\n3. Echtzeituhr\n4. Batterie\n5. Simulation\n6. Lichtsensor\
\fTest du système:\n1. Affichage DEL\n2. Mémoire Flash\n3. Horodateur\n4. Batterie\n5. Simulation\n6. Capteur de lumière\
\fMenu di prova:\n1. Display LED\n2. Memoria Flash\n3. Orologio tempo reale\n4. Batteria\n5. Simulazione\n6. Sensore di luminosità";
text T_kein[]			= "\fKein\fNon\fNessuna";
text T_flash[]		= "\f Flash Speicher installiert\f mèmoire Flash installé\f memoria Flash installato";
text T_erase[]		= "\f\nLösche \f\nEfface \f\nElimina  ";
text T_write[]		= "\f\nSchreibe \f\nEcrit \f\nAggiungi ";
text T_read[]			= "\fLese \fLire \fLeggere "; 
text T_page[]			= "\fSeite: \fpage: \fpagina:";
text T_ok[]				= " ok";
text T_cont[]			= "\fFortfahren\fContinuer\fContinua";
text T_licht[]		= "\fLichtsensor\fCapteur de lumière\fSensore di luminosità";
text T_optmenu[]	= "\f\nEinstellungen:\f\nChanger les paramètres:\f\nOpzioni:";
text T_vopt[]			=	"\fOptionen Geschwindigkeitsanzeige\fOptions d'affichage de la vitesse\fOpzioni di visualizzazione velocità";	
text T_vmin[]			= "\fMindestgeschwindigkeit\fAffichage vitesse minimale\fVelocità minima";
text T_vmax[]			= "\fMaximalgeschwindigkeit\fAffichage vitesse maximale\fVelocità massima";
text T_vblk[]			= "\fSchwelle blinkende LED\fSeuil DEL clignotant\fSoglia LED lampeggiante";
text T_vcolor[]		= "\fSchwelle LED Farbumschaltung\fSeuil DEL changé de couleur\fSoglia LED colore cambiano";
text T_smenu[]		= "\f\nSondermenü:\f\nMenu spécial:\f\nMenù speciale:";
text T_mcyc[]			= "\fMesszyklus\fCycle de mesure\fCyclo di misura";
text T_acyc[]			= "\fLED Anzeigedauer\fDurée d'affichage DEL\fDurata del display LED";
text T_nk[]				= "\fNachkommastelle\fDécimale\fDecimale";
text T_ende[]			= "\f<Ende mit Return>\n\f<Fin avec 'Entrée'>\n\f<Esci con return>\n";
text T_time[]			= "\fZeit\fHeure\fOra";
text T_date[]			= "\fDatum\fDate\fData";
text T_fdate[]		= "\f(TT.MM.JJJJ)\f(jj.mm.aaaa)\f(GG.MM.AAAA)";
text T_ftime[]  	= "\f(hh:mm:ss)\f(hh:mm:ss)\f(HH:MM:SS)";
text T_fstime[]		= "\f(hh:mm)\f(hh:mm)\f(HH:MM)";
text T_infomenu[]	= "\fInformation:\fInformation:\fInformazioni:";
text T_serialno[]	= "\f\nSeriennummer\f\nNuméro de série\f\nNumero di serie";
text T_comment[]	= "\fAnmerkungen\fCommentaire\fCommenta";
text T_protocol[]	= "\fProtokoll\fJournal\fProtocollo";
text T_delete[]		= "\fDaten löschen\fEffacer les données\fEliminare i dati";
text T_weret[] 		= "\f<Weiter mit Return>\n\f<Suite avec 'Entrée'>\n\f<Continua con return>\n";
text T_eraseall[]	= "\f\nAlle Daten werden gelöscht\f\nToutes les données seront effacées\f\nTutti i dati saranno cancellati";
text T_batt[]			= "\fBatteriespannung\fTension de la batterie\fTensione batteria";
text T_anzmw[]		= "\fAnzahl Messwerte\fNombre des données\fNumero valori misurati";
text T_daus[]			= "\f\nDaten ausgeben\f\nTransfer données\f\nTrasferimento dati";
text T_anzpr[]		= "\fAnzahl Protokolldaten\fNombre de données de log\fNumero di dati di log";
text T_bdir[]			= "\fBidirektionale Erfassung\fDétection bidirectionnel\fRilevamento bidirezionale";
text T_vcor[]     = "\fKorrekturfaktor Geschwindigkeit\fFacteur de correction de la vitesse\fFattore di correzione di velosit";
text T_vlim[]			= "\fTempolimit\fLimitation de vitesse\fLimite di velocitá";
text T_units[]		=	"\fGeschwindigkeitsmesseinheit\fUnité de mesure de vitesse\fUnità di misura della velocità";
text T_dismod[]		=	"\fAnzeige Geschwindigkeit\fAffichage de la vitesse\fVisualizzazione della velocità";
text T_dmodes[2][33]	= {"\fabsolut\fabsolu\fassoluto", "\fdifferenz\fdifférence\fdifferenza"};
text T_voff[]			= "\fAnzeigeoffset\fEcart minimale\fOffset di indicazione";
text T_tmen[]			= "\fZeitplanung\fPlanification du temps\fProgrammazione dei tempi";
text T_pset[]			= "\fParametersatz\fJeu de paramètres\fSet di parametri";
text T_somwin[]		= "\fSommer-/Winterzeitumstellung\fPermutation été/hiver\fImpostazione ora solare/legale";
text T_zone[4][28]= {"\faus\fdésactive\fdisattivata","UTC+0","UTC+1","UTC+2"};
text T_wtag[]			= "\fWochentag: \fJour ouvrable: \fGiorno settimanale: ";
text T_tag[7][29]	= {"\fSonntag\fDimanche\fDomenica","\fMontag\fLundi\fLunedi","\fDienstag\fMardi\fMartedi","\fMittwoch\fMercredi\fMercoledi","\fDonnerstag\fJeudi\fGiovedi","\fFreitag\fVendredi\fVenerdi","\fSamstag\fSamedi\fSabato"};
text T_offon[2][28]	= {"\faus\fdésactive\fdisattiva","\fein\factivé\fattiva"};
text T_ontage[]		= "\fEinschalttage: \fJour opérationnel: \fGiorni di attivazione: ";
text T_tein[]			= "\fEinschaltzeit: \fHeure du début: \fOrario di attivazione: ";
text T_taus[]			= "\fAusschaltzeit: \fHeure de la fin: \fOrario di disattivazione: ";
text T_led[]			= "\fLED Anzeige: \fAffichage á DEL: \fDisplay á LED: ";
text T_ext[]			= "\fErweiterungen\fExtensions\fEstensioni";
text T_symb[]			= "\fSymbol \fSymbole \fSimbolo ";
text T_symled[]		= "\fLed Symbole \fSymboles DEL \fSimboli di Led ";
text T_thr[]			= "\fSchwellwert\fSeuil\fSoglia";
text T_swgrp[5][SWGLEN] = {"\fAnzeigeseiten\fPages d'afficher\fPagine di vis.","\fRelais\fRelais\fRelè","Power MOSFETs",\
"\fWarnleuchten\fLampe alerte\fSpia di avvertimento","\fSchalter\fCommutateurs\fCommutatore"};
text T_swtyp[4][SWLEN] = {"\fAnzeigeseite\fPage d'afficher\fPagina","\fRelais\fRelais\fRelè","\fSchalter\fCommutateur\fCommutatore","\fAusgang\fSortie\fUscita"};
text T_vmmin[]		= "\fKleinste Messgeschwindigkeit\fVitesse mesurée minimal\fMinima misura velocità";
text T_brght[]		= "\fAnzeigeoptimierung\fOptimisation d'affichage\fOttimizzazione display";
text T_brsel[3][52]	= {"\fBetriebsdauer\fDurée de fonction\fOperativo in tempo","\fAusgeglichen\fÉquilibré\fEquilibrato","\fSichtbarkeit\fVisibilité\fVisibilità"};
text T_parakt[] 	= "\fAktiver Parametersatz\fJeu de paramètres actif\fSet di parametri attivo";
text T_hell[]			= "\fLED Helligkeit\fLuminosité DEL\fLuminosità del LED";
text T_col2[]			= "\fSchwelle LED Mischfarbe\fSeuil DEL de couleur mélangée\fSoglia LED colore composto";
text T_sim[]			= "\fTestgeschwindigkeit\fVitesse par test\fVelocità di prova";
text T_modem[]		= "\fFunkmodems:\n1. Bluetooth Modem\n2. Mobilfunk Modem\
\fLes modems radio:\n1. Modem Bluetooth\n2. Modem cellulaire\
\fRadio modem\n1. Bluetooth Modem\n2. Modem cellulare";
text T_btmenu[]		= "\f\nBluetooth Menü\n1. Information\n2. Pinnummer\
\f\nMenu bluetooth\n1. Information\n2. Code PIN\
\f\nMenu Bluetooth\n1. Informazioni\n2. Numero PIN";
text T_btprot[]		= "\fProtokoll\fProtocole\fProtocollo";
text T_ninst[]		= "\fNicht installiert\n\fNon installé\n\fNon installato\n";
text T_btbda[]		= "\fBluetooth Adresse\fL'adresse Bluetooth\fBluetooth indirizzo";
text T_btname[]		= "\fGerätename\fNom du périphérique\fNome dispositivo";
text T_pinno[]		= "\fPinnummer\fCode PIN\fNumero PIN";
text T_btlst[]		= "\fListe vertrauter Geräte:\fListe d'appareils de confiance:\fElenco dispositivi di fiducia:";
text T_dellst[]		= "\fListe löschen\fEffacer liste\fElimina elenco";
text T_noacc[]		= "\fZugriff nicht möglich!\n\fAccès impossible\n\fImpossibile l'accesso\n";
text T_gsmmen[]		= "\f\nMobilfunk Menü:\f\nMenu modem cellulaire\f\nMenu modem cellulare";
text T_vemail[]		= "\fEmail Versand: \fRemise d'email: \fEmail di consegna: ";
text T_mailcyc[5][44]	= {"\fKeiner\fNon\fNo","\fSpeicher voll\fMemoire pleine\fMemoria piena",\
"\fTäglich\fQuotidien\fQuotidiano","\fWöchentlich\fHebdomadaire\fSettimanale","\fMonatlich\fMensuel\fMensile"};
text T_mtag[]			= "\fTag des Monats\fJour du mois\fGiorno del mese";
text T_mfstd[]		= "\fFunknetz\fRéseau radio\fRete radio";		
text T_yconf[]		= "\fKonfiguriere \fConfigurer \fConfigura ";
text T_notcon[]		= "\fnicht definiert\fnon défini\fnon impostato";
text T_change[]		= "\fÄndern\fChanger\fCambiare";
text T_err[]			= "\fFehler \fErreur \fErrore ";
text T_txmail[]		= "\fSende Email\fEnvoyer email\fInvia email";
text T_txsms[]		= "\fSende SMS\fEnvoyer SMS\fInvia SMS";
text T_mailset[]	= "\fEmail Versand eingerichtet!\fEnsemble envoi de d'email!\fEmail consegna impostato!";
text T_status[]		= "\fStatus\fStatut\fStato";
text T_mdaten[]		= "\fMessdaten\fDonnees\fDati";
text T_inter[]		= "\fSchnittstellen\fInterfaces\fInterfacce";
text T_mnow[]			= "\fEmail versenden\fEnvoyer un email\fInviare una email";
text T_snow[]			= "\fSMS versenden\fEnvoyer un SMS\fInviare una SMS";
text T_emalarm[]	= "\fEmail Alarm\fAlerte Mail\fMail alert";
text T_smsno[]		= "\fSMS Rufnummer: \fNuméro SMS: \fSMS numero: ";
text T_smsalarm[]	= "\fSMS Alarm\fAlerte SMS\fSMS alert";
text T_smsset[]		= "\fSMS Versand eingerichtet!\fEnsemble envoi de SMS\fSMS consegna impostato!";
text T_syserr[]		= "\fSystemfehler\fErreur système\fErrore di sistema";
text T_rsens[]		= "\fRadar Empfindlichkeit\fSensibilité radar\fSensibilità del radar";
text T_symname[]	= "\f\nSymbolname: \f\nNom de symbole: \f\nNome del simbolo: ";
text T_symgrn[]		= "\fAuswahl der Symbolgruppe\fSélectionnez le groupe de symbole\fSeleziona gruppo simbolo";
text T_symdef[]		= "\fDefinition der Symbole\fDéfinition des symboles\fDefinizione di simboli";
text T_newgrp[]		= "\fNeuer Symbolgruppenname\fNouveau nom de groupe de symboles\fNuovo nome del gruppo simbolo";
text T_usbdis[]		= "\fTrennen Sie jetzt die USB Verbindung jetzt!\fDébranchez USB maintenant!\fScollegare USB ora!";
text T_mgps[]			=	"\fGPS Modul\fModule GPS\fModulo GPS";
text T_gpsmen[]		= "\f\nGPS Menü:\n1. Positionsliste\n2. Formatierte GPS Datenausgabe\n3. GPS Rohdaten (NMEA-183)\
\f\nMenu GPS:\n1. Liste de positions\n2. Données formatées GPS\n3. Données brutes GPS (NMEA-183)\
\f\nMenu GPS:\n1. Lista posizioni GPS\n2. Formattati uscita dei dati GPS\n3. Uscita dati grezzi GPS (NMEA-183)";
text T_ctrlz[]		= "\fBeenden mit <CTRL-Z>\fQuittez avec <CTRL-Z>\fTerminare con <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPositionsbestimmungszeit\fDélai de localisation\fTempo di misura di posizione";
text T_gpsanz[]		= "\fPositionsbestimmungen\fNombre de positions GPS\fPosizioni GPS memorizzati";
text T_betr[]			= "\fBetriebsstunden\fHeures de fonctionnement\fOrario d'esercizio";
text T_sstr[]			= "\fSignalstärke(0...31)\fPuissance du signal(0...31)\fPotenza del segnale(0...31)";
text T_tzone[]		= "\fZeitzone\fFuseau horaire\fFuso orario";
text T_conmqtt[]	= "\f\nVerbinde mit MQTT Server\f\nSe connecter au serveur MQTT\f\nCollegarsi a MQTT Server";
text T_btdis[]		= "\f\nBluetoothverbindung getrennt\f\nConnexion Bluetooth déconnectée\f\nConnessione Bluetooth scollegata";
text T_wait[]			= "\fBitte warten...\fS.v.p., attendez...\fAttendere prego...";
text T_gpscon[]		= "\f\nGPS-Positionsbestimmung. \f\nPositionnement GPS \f\nPosizionamento GPS. ";
text T_bereit[]		= "\fBereit\fPrêt\f\nPronto";
text T_auto[]			= "\fAutomatisch\fAutomatiquement\fAutomaticamente";

// Ereignistexte in Landessprache (Protokollmeldungen), einsprachige Ereignistexte siehe sictxt.c
text T_poweron[]	= "\fEingeschaltet\fPuissance\fAccensione";
text T_sommer[]		= "\fSommerzeit\fHeure d'été\fOra legale";
text T_winter[]		= "\fWinterzeit\fHeure d'hiver\fOra solare";
text T_messstart[]	= "\fMessbeginn\fDébut de mesure\fInizio di misura";
text T_memfull[]	= "\fSpeicher voll \fMémoire pleine \fMemoria piena ";
text T_defpar[]		= "\fStandardeinstellung\fParamètres defaut\fParametri di default";
text T_batlow[]		= "\fBatteriespannung < 11,5V\fTension de la batterie < 11,5V\fTensione batteria < 11,5V";
text T_parainit[]	= "Parameter initialisation";
//text T_test[]		= "Testeintrag";

 #endif
