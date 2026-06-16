//-----------------------------------------------------------------------------
//  FILE: sictxt.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Texte englisch/französisch/niederländisch
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
//-----------------------------------------------------------------------------
//	COMPILATION: THUMB CODE
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   25.07.2012
//-----------------------------------------------------------------------------              
//  AUTHOR :	JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//              	25.07.2012: File creation
//-----------------------------------------------------------------------------

#include "../sictxt.h"	

#if ((LANGUAGE == e_f_nl)|(LANGUAGE == e_f_nl_kry)|(LANGUAGE == e_f_nl_pol))
// Sprachspezifische Texte
text T_spritm[ANZSPRACHEN][10] =	{"English","French","Dutch"};
text T_ja[ANZSPRACHEN]			= {'y','o','j'};		// Antwortkürzel für ja
text T_nein[ANZSPRACHEN]   		= {'n','n','n'};		// dto. für nein
text T_jn[]						= "\f (y/n)?\f (o/n)?\f (j/n)?";
text T_tver[]		= "\fVersion \fVersion \fVersie ";
text T_winit[]    	= "\fDefault parameter set\fInitialiser les paramètres\fDefault parameter set";
text T_main[]		= "\f\nMain menu:\n1. Transmit stored data\n2. Online measurement\n3. Test functions\n4. Settings\n5. Radio modems\n6. Information and time\
\f\nMenu principal:\n1. Transfert des données\n2. Affichage en connexion PC\n3. Test du système\n4. Changer les paramètres\n5. Les Modems radio\n6. Information et temps\
\f\nHoofdmenu:\n1. Gegevensoutput\n2. Online meten\n3. Testfuncties\n4. Instellingen\n5. Radio modems\n6. Informatie en tijd";
text T_zuruck[]		= "\fBack\fRetour\fTerug";
text T_ausw[]		= "\fYour choice 1...\fVotre choix 1...\fUw keuze 1...";
text T_estmenu[] 	= "\fTest functions:\n1. LED display\n2. Flash memory\n3. Real time clock\n4. Main battery\n5. Simulation\n6. Light sensor\
\fTest du système:\n1. Affichage DEL\n2. Mémoire Flash\n3. Horodateur\n4. Batterie\n5. Simulation\n6. Capteur de lumière\
\fTestfuncties:\n1. LED display\n2. Flash memory\n3. Real time clock\n4. Main battery\n5. Simulatie\n6. Lichtsensor";
text T_kein[]		= "\fNo\fNon\fKeen";
text T_flash[]		= "\f Flash installed\f mèmoire Flash installé\f Flash geïnstalleerd";
text T_erase[]		= "\f\nErase \f\nEfface \f\nWis ";
text T_write[]		= "\f\nWrite \f\nEcrit \f\nSchrijf ";
text T_read[]			= "\fRead \fLire \fLees "; 
text T_page[]		= "\fPage: \fpage: \fPagina:";
text T_ok[]			= " ok";
text T_cont[]		= "\fContinue\fContinuer\fDoorgaan";
text T_licht[]		= "\fLichtsensor\fCapteur de lumière\fLichtsensor";
text T_optmenu[]	= "\f\nSettings:\f\nChanger les paramètres:\f\nInstellingen:";
text T_vopt[]			=	"\fSpeed display options\fOptions d'affichage de la vitesse\fOpties snelheidsdisplay";	
text T_vmin[]		= "\fMinimal speed displayed\fAffichage vitesse minimale\fMinimale snelheid weergegeven";
text T_vmax[]		= "\fMaximal speed displayed\fAffichage vitesse maximale\fMaximale snelheid weergegeven";
text T_vblk[]		= "\fThreshold blinking LED\fSeuil DEL clignotant\fKnipperen LED drempel";
text T_vcolor[]		= "\fThreshold LED color change\fSeuil DEL changé de couleur\fLED kleur omschakeling drempel";
text T_smenu[]		= "\f\nSpecial menu:\f\nMenu spécial:\f\nSpeciaal menu:";
text T_mcyc[]		= "\fMeasurement cycle time\fCycle de mesure\fCyclustijd van de meting";
text T_acyc[]		= "\fLED diplay time\fDurée d'affichage LED\fLED weergave tijd";
text T_nk[]			= "\fDecimal places\fDécimale\fDecimale";
text T_ende[]		= "\f<Quit with return>\n\f<Fin avec 'Entrée'>\n\f<Einde met Return>\n";
text T_time[]		= "\fTime\fHeure\fTijd";
text T_date[]		= "\fDate\fDate\fDatum";
text T_fdate[]		= "\f(dd.mm.yyyy)\f(jj.mm.aaaa)\f(dd.mm.jjjj)";
text T_ftime[]  	= "\f(hh:mm:ss)\f(hh:mm:ss)\f(uu:mm:ss)";
text T_fstime[]		= "\f(hh:mm)\f(hh:mm)\f(uu:ss)";
text T_infomenu[]	= "\f\nInformation:\f\nInformation:\f\nInformatie:";
text T_serialno[]	= "\f\nSerial number\f\nNuméro de série\f\nVolgnummer";
text T_comment[]	= "\fComments\fCommentaire\fOpmerkingen";
text T_protocol[]	= "\fProtocol\fJournal\fProtocol";
text T_delete[]		= "\fDelete data\fEffacer les données\fGegevens wissen";
text T_weret[] 		= "\f<Continue with Return>\n\f<Suite avec 'Entre'>\n\f<Doorgaan d.m.v. Enter>\n";
text T_eraseall[]	= "\f\nAll data will be deleted\f\nToutes les données seront effacées\f\nAlle gegevens worden verwijderd";
text T_batt[]		= "\fBattery voltage\fTension de la batterie\fBatterij voltage";
text T_anzmw[]		= "\fNumber of values\fNombre des données\fAantal meetwaarden";
text T_daus[]		= "\f\nSend data\f\nTransfer données\f\nGegevens output";
text T_anzpr[]		= "\fNumber of log entries\fNombre de données de log\fAantal loggegevens";
text T_bdir[]		= "\fBidirectional detection\fDétection bidirectionnel\fBidirectionele detectie";
text T_vcor[]      	= "\fSpeed correction factor\fFacteur de correction de la vitesse\fSnelheid correctie factor";
text T_vlim[]		= "\fSpeed limit\fLimitation de vitesse\fSnelheidslimiet";
text T_units[]		=	"\fSpeed measure unit\fUnité de mesure de vitesse\fSpeed meeteenheid";
text T_dismod[]		=	"\fSpeed display\fAffichage de la vitesse\fSnelheid weergave";
text T_dmodes[2][33]	= {"\fabsolute\fabsolu\fabsoluut", "\fdifference\fdifférence\fverschil"};
text T_voff[]		= "\fDisplay offset\fEcart minimale\fWeergave offset";
text T_tmen[]		= "\fTime schedule\fPlanification du temps\fTijdschema";
text T_pset[]		= "\fParameter set\fJeu de paramètres\fParameterset";
text T_somwin[]		= "\fDaylight saving time\fPermutation été/hiver\fOmschakeling zomer-/wintertijd";
text T_zone[4][20]	= {"\foff\fdésactive\fuit","UTC+0","UTC+1","UTC+2"};
text T_wtag[]		= "\fWeekday:\fJour de la semaine:\fWeekdag:";
text T_tag[7][29]	= {"\fSunday\fDimanche\fZondag","\fMonday\fLundi\fMaandag","\fTuesday\fMardi\fDinsdag","\fWednesday\fMercredi\fWoensdag","\fThursday\fJeudi\fDonderdag","\fFriday\fVendredi\fVrijdag","\fSaturday\fSamedi\fZaterdag"};
text T_offon[2][20]	= {"\foff\fdésactive\fuit","\fon\factivé\faan"};
text T_ontage[]		= "\fOperation weekdays: \fJour opérationnel: \fInschakeldagen: ";
text T_tein[]		= "\fDaily start time: \fHeure du début: \fInschakeltijd: ";
text T_taus[]		= "\fDaily stop time: \fHeure de la fin: \fUitschakeltijd: ";
text T_led[]		= "\fLED display: \fAffichage á DEL: \fLED display: ";
text T_ext[]		= "\fExtensions thresholds\fSeuils d'extensions\fDrempels extensies";
text T_symb[]		= "\fSymbol \fSymbole \fSymbool ";
text T_symled[]		= "\fLed symbols \fSymboles DEL \fLed symbolen ";
text T_thr[]		= "\fThreshold\fSeuil\fDrempel";
text T_swgrp[5][SWGLEN] = {"\fDisplay pages\fPages d'afficher\fDisplay pagina's","\fRelays\fRelais\fRelais","Power MOSFETs",\
"\fWarning lamp\fLampe alerte\fWaarschuwingslampje","\fSwitches\fCommutateurs\fSchakelaars"};
text T_swtyp[4][SWLEN] = {"\fLED page\fPage DEL\fLED pagina","\fRelay\fRelais\fRelais","\fOutput\fSortie\fUitgang","\fSwitch\fCommutateur\fSchakelaar"};
text T_vmmin[]		= "\fLowest measured speed\fVitesse mesurée minimal\fLaagste gemeten snelheid";
text T_brght[]		= "\fOptimizing display\fOptimisation d'affichage\fOptimaliseren van weergave";
text T_brsel[3][52]	= {"\fOperating time\fDurée de fonction\fBedrijfsduur","\fBalanced\fÉquilibré\fEvenwichtig","\fVisibility\fVisibilité\fZichtbaarheid"};
text T_parakt[] = "\fActive parameter set\fJeu de paramètres actif\fActieve parameterset";
text T_hell[]		= "\fLED brightness\fLuminosité DEL\fHelderheid van de LED";
text T_col2[]		= "\fThreshold Led mixed color\fSeuil de couleur mélangée des DEL\fDrempel Led gemengde kleur";
text T_sim[]		= "\fTest speed\fVitesse par test\fTest snelheid";
text T_modem[]		= "\fRadio modems:\n1. Bluetooth modem\n2. Cellular modem\
\fLes modems radio:\n1. Modem Bluetooth\n2. Modem cellulaire\
\fRadio modems\n1. Bluetooth modem\n2. Cellular modem";
text T_btmenu[]		= "\f\nBluetooth menu\n1. Information\n2. Pin number\
\f\nMenu bluetooth\n1. Information\n2. Code PIN\
\f\nBluetooth menu\n1. Informatie\n2. Pin nummer";
text T_btprot[]		= "\fProtocol\fProtocole\fProtocol";
text T_ninst[]		= "\fNot installed\n\fNon installé\n\fNiet geïnstalleerd\n";
text T_btbda[]		= "\fBluetooth address\fL'adresse Bluetooth\fBluetooth adres";
text T_btname[]		= "\fDevice name\fNom du périphérique\fApparaatnaam";
text T_pinno[]		= "\fPin number\fCode PIN\fPin nummer";
text T_btlst[]		= "\fTrusted device list:\fListe d'appareils de confiance:\fLijst vertrouwde apparaaten:";
text T_dellst[]		= "\fClear list\fEffacer liste\fLijst wissen";
text T_noacc[]		= "\fAccess impossible!\n\fAccès impossible\n\fToegang niet mogelijk\n";
text T_gsmmen[]		= "\f\nMobile modem menu:\f\nMenu modem cellulaire\f\nCellular modem menu";
text T_vemail[]		= "\fEmail delivery: \fRemise d'email: \fEmail verzending: ";
text T_mailcyc[5][44]	= {"\fNone\fNon\fGeen","\fMemory full\fMemoire pleine\fGeheugen vol",\
"\fDaily\fQuotidien\fDagelijks","\fWeekly\fHebdomadaire\fWekelijks","\fMonthly\fMensuel\fMaandelijks"};
text T_mtag[]		= "\fDay of month\fJour du mois\fDag van de maand";
text T_mfstd[]		= "\fRadio network\fRéseau radio\fRadionetwerk";
text T_yconf[]		= "\fConfigure \fConfigurer \fConfigureren ";
text T_notcon[]		= "\fnot defined\fnon défini\fniet gedefinieerd";
text T_change[]		= "\fChange\fChanger\fVeranderen";
text T_err[]			= "\fError \fErreur \fFout ";
text T_txmail[]		= "\fSend email\fEnvoyer email\fStuur email";
text T_txsms[]		= "\fSend SMS\fEnvoyer un SMS\fStuur SMS";
text T_mailset[]	= "\fEmail delivery set!\fEnsemble envoi de d'email!\fEmail levering set!";
text T_status[]		= "\fState\fStatut\fStaat";
text T_mdaten[]		= "\fMeasurement data\fDonnees\fGegevens";
text T_inter[]		= "Interfaces";
text T_mnow[]		= "\fSend email\fEnvoyer un email\fEmail deze";
text T_snow[]		= "\fSend SMS\fEnvoyer un SMS\fSMS deze";
text T_emalarm[]	= "\fEmail alarm\fAlerte Mail\fEmail alert";
text T_smsno[]		= "\fSMS phone number: \fNuméro SMS: \fNummer voor SMS: ";
text T_smsalarm[]	= "\fSMS alarm\fAlerte SMS\fSMS alert";
text T_smsset[]		= "\fSMS delivery set!\fEnsemble envoi de SMS\fSMS levering set!";
text T_syserr[]		= "\fSystem error\fErreur système\fSysteemfout";
text T_rsens[]		= "\fRadar sensitivity\fSensibilité radar\fRadargevoeligheid";
text T_symname[]	= "\f\nSymbol name: \f\nNom de symbole: \f\nSymbool naam: ";
text T_symgrn[]		= "\fSelect symbol group\fSélectionnez le groupe de symbole\fSelecteer symboolgroep";
text T_symdef[]		= "\fDefinition of symbols\fDéfinition des symboles\fVerklaring van de symbolen";
text T_newgrp[]		= "\fNew symbol group name\fNouveau nom de groupe de symboles\fNieuwe naam symboolgroep";
text T_usbdis[]		= "\fUnplug USB now!\fDébranchez USB maintenant!\fVerwijder de USB nu!";
text T_mgps[]			=	"\fGPS module\fModule GPS\fGPS-module";
text T_gpsmen[]		= "\f\nGPS menu:\n1. Position list\n2. Formatted data output\n3. Raw data (NMEA-183)\
\f\nMenu GPS:\n1. Liste de positions\n2. Données formatées GPS\n3. Données brutes GPS (NMEA-183)\
\f\nGPS menu:\n1. Positie lijst\n2. Opgemaakte gegevensuitvoer\n3. Ruwe data (NMEA-183)";
text T_ctrlz[]		= "\fQuit with <CTRL-Z>\fQuittez avec <CTRL-Z>\fStoppen met <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPosition fix time\fDélai de localisation\fPositiebepalingstijd";
text T_gpsanz[]		= "\fNumber of GPS positions\fNombre de positions GPS\fAantal GPS-posities";
text T_betr[]			= "\fOperating hours\fHeures de fonctionnement\fBedrijfsuren";
text T_sstr[]			= "\fSignal strength(0...31)\fPuissance du signal(0...31)\fSignaalsterkte(0...31)";
text T_tzone[]		= "\fTime zone\fFuseau horaire\fTijdzone";
text T_conmqtt[]	= "\f\nConnecting MQTT server\f\nSe connecter au serveur MQTT\f\nMaak verbinding met MQTT server";
text T_btdis[]		= "\f\nBluetooth disconnected\f\nConnexion Bluetooth déconnectée\f\nBluetooth verbroken";
text T_wait[]			= "\fPlease wait...\fS.v.p., attendez...\fEven geduld aub...";
text T_gpscon[]		= "\f\nGPS positioning. \f\nPositionnement GPS. \f\nGPS-positionering. ";
text T_bereit[]		= "\fReady\fPrêt\f\nKlaar";
text T_auto[]			= "\fAutomatic\fAutomatiquement\fAutomatisch";

// Ereignistexte in Landessprache (Protokollmeldungen), einsprachige Ereignistexte siehe sictxt.c
text T_poweron[]	= "\fPower on\fPuissance\fIngeschakeld";
text T_sommer[]		= "\fSummer time\fHeure d'été\fZomertijd";
text T_winter[]		= "\fWinter time\fHeure d'hiver\fWintertijd";
text T_messstart[]	= "\fStart of measurement\fDébut de mesure\fMeting is gestart";
text T_memfull[]	= "\fMemory full\fMémoire pleine\fGeheugen vol";
text T_defpar[]		= "\fDefault parameter set\fParamètres defaut\fStandaard parameter set";
text T_batlow[]		= "\fBattery voltage < 11,5V\fTension de la batterie < 11,5V\fAccuspanning < 11,5V";
text T_parainit[]	= "Parameter initialisation";
#endif
