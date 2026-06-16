//-----------------------------------------------------------------------------
//  FILE: sictxt.c			PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Texte englisch/französisch/slowenisch
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
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

#if (LANGUAGE == e_f_sl)

// Sprachspezifische Texte
text T_ja[ANZSPRACHEN]			= {'y','o','d'};		// answer for yes
text T_nein[ANZSPRACHEN]   		= {'n','n','n'};		// answer for no
text T_jn[]						= "\f (y/n)?\f (o/n)?\f (d/n)?";
text T_spritm[ANZSPRACHEN][10] 	=	{"English","French","Slovenian"};
text T_tver[]		= "\fVersion \fVersion \fRazlicica ";
text T_winit[]    	= "\fDefault parameter set\fInitialiser les paramètres\fPrivzeti set parametrov";
text T_main[]		= "\f\nMain menu:\n1. Transmit stored data\n2. Online measurement\n3. Test functions\n4. Settings\n5. Radio modems\n6. Information and time\
\f\nMenu principal:\n1. Transfert des données\n2. Affichage en connexion PC\n3. Test du système\n4. Changer les paramètres\n5. Les Modems radio\n6. Information et temps\
\f\nGlavni meni:\n1. Prenos podatkov\n2. Meritev na liniji\n3. Testne funkcije\n4. Nastavitve\n5. Radio modems\n6. Informacije in cas";
text T_zuruck[]		= "\fBack\fRetour\fNazaj";
text T_ausw[]		= "\fYour choice 1...\fVotre choix 1...\fVasa izbira 1...";
text T_estmenu[] 	= "\fTest functions:\n1. LED display\n2. Flash memory\n3. Real time clock\n4. Main battery\n5. Simulation\n6. Light sensor\
\fTest du système:\n1. Affichage DEL\n2. Mémoire Flash\n3. Horodateur\n4. Batterie\n5. Simulation\n6. Capteur de lumière\
\fTestne funkcije:\n1. LED prikazovalnik\n2. Flash spomin\n3. Realna casovna baza\n4. Stanje baterije\n5. Simulacija\n6. Senzor svetlobe";
text T_kein[]		= "\fNo\fNon\fNe";
text T_flash[]		= "\f Flash installed\f mèmoire Flash installé\f Flash vgrajen";
text T_erase[]		= "\f\nErase \f\nEfface \f\nBrisi ";
text T_write[]		= "\f\nWrite \f\nEcrit \f\nPisi ";
text T_read[]			= "\fRead \fLire \fBrati ";
text T_page[]		= "\fPage: \fpage: \fStran:";
text T_ok[]			= " ok";
text T_cont[]		= "\fContinue\fContinuer\fNadaljuj";
text T_licht[]		= "\fLichtsensor\fCapteur de lumière\fSenzor svetlobe";
text T_optmenu[]	= "\f\nSettings:\f\nChanger les paramètres:\f\nNastavitev:";
text T_vopt[]			=	"\fSpeed display options\fOptions d'affichage de la vitesse\fMožnosti prikaza hitrosti";	
text T_vmin[]		= "\fMinimal speed displayed\fAffichage vitesse minimale\fMinimalna hitrost prikaza";
text T_vmax[]		= "\fMaximal speed displayed\fAffichage vitesse maximale\fMaksimalna hitrost prikaza";
text T_vblk[]		= "\fThreshold blinking LED\fSeuil DEL clignotant\fPrag utripanja LED";
text T_vcolor[]		= "\fThreshold LED color change\fSeuil DEL changé de couleur\fPrag spremembe barve LED";
text T_smenu[]		= "\f\nSpecial menu:\f\nMenu spécial:\f\nPosebni meni:";
text T_mcyc[]		= "\fMeasurement cycle time\fCycle de mesure\fMerilni cikel";
text T_acyc[]		= "\fLED diplay time\fDurée d'affichage LED\fCas prikaza LED";
text T_nk[]			= "\fDecimal places\fDécimale\fDecimala";
text T_ende[]		= "\f<Quit with return>\n\f<Fin avec 'Entrée'>\n\f<Izhod z 'Enter'>\n";
text T_time[]		= "\fTime\fHeure\fCas";
text T_date[]		= "\fDate\fDate\fDatum";
text T_fdate[]		= "\f(dd.mm.yyyy)\f(jj.mm.aaaa)\f(dd.mm.llll)";
text T_ftime[]  	= "\f(hh:mm:ss)\f(hh:mm:ss)\f(uu:mm:ss)";
text T_fstime[]		= "\f(hh:mm)\f(hh:mm)\f(uu:mm)";
text T_infomenu[]	= "\f\nInformation:\f\nInformation:\f\nInformacije:";
text T_serialno[]	= "\f\nSerial number\f\nNuméro de série\f\nSerijska stevilka";
text T_comment[]	= "\fComments\fCommentaire\fKomentarji";
text T_protocol[]	= "\fProtocol\fJournal\fProtokol";
text T_delete[]		= "\fDelete data\fEffacer les données\fZbrisi podatke";
text T_weret[] 		= "\f<Continue mit Return>\n\f<Suite avec 'Entre'>\n\f<Nadaljujte z 'Enter'>\n";
text T_eraseall[]	= "\f\nAll data will be deleted\f\nToutes les données seront effacées\f\nVsi podatki bodo zbrisani";
text T_batt[]		= "\fBattery voltage\fTension de la batterie\fNapetost batrije";
text T_anzmw[]		= "\fNumber of values\fNombre des données\fStevilo meritev";
text T_daus[]		= "\f\nSend data\f\nTransfer données\f\nPoslji podatke";
text T_anzpr[]		= "\fNumber of log entries\fNombre de données de log\fStevilo vnosov v dnevnik";
text T_bdir[]		= "\fBidirectional detection\fDétection bidirectionnel\fDvosmerna zaznava";
text T_vcor[]      	= "\fSpeed correction factor\fFacteur de correction de la vitesse\fKorekcijski faktor hitrosti";
text T_vlim[]		= "\fSpeed limit\fLimitation de vitesse\fOmejitev hitrosti";
text T_dismod[]		=	"\fSpeed display\fAffichage de la vitesse\fNacin prikaza hitrosti";
text T_dmodes[2][33]	= {"\fabsolute\fabsolu\fabsolutna", "\fdifference\fdifférence\frazlika"};
text T_units[]	=	"\fSpeed measure unit\fUnité de mesure de vitesse\fHitrost ukrep enota";
text T_voff[]		= "\fDisplay offset\fEcart minimale\fZamik prikazovalnika";
text T_tmen[]		= "\fTime schedule\fPlanification du temps\fUrnik";
text T_pset[]		= "\fParameter set\fJeu de paramètres\fSet parametrov";
text T_somwin[]		= "\fDaylight saving time\fPermutation été/hiver\fNastavitev cas zima/poletje";
text T_zone[4][22]	= {"\foff\fdésactive\fizklop","UTC+0","UTC+1","UTC+2"};
text T_wtag[]		= "\fWeekday:\fJour de la semaine:\fDan v tednu:";
text T_tag[7][29]	= {"\fSunday\fDimanche\fNedelja","\fMonday\fLundi\fPonedeljek","\fTuesday\fMardi\fTorek","\fWednesday\fMercredi\fSreda","\fThursday\fJeudi\fCetrtek","\fFriday\fVendredi\fPetek","\fSaturday\fSamedi\fSobota"};
text T_offon[2][22]	= {"\foff\fdésactive\fizklop","\fon\factivé\fvklop"};
text T_ontage[]		= "\fOperation weekdays: \fJour opérationnel: \fDnevi obratovanja: ";
text T_tein[]		= "\fDaily start time: \fHeure du début: \fDnevni cas vklopa: ";
text T_taus[]		= "\fDaily stop time: \fHeure de la fin: \fDnevni cas izklopa: ";
text T_led[]		= "\fLED display: \fAffichage á DEL: \fLED prikazovalnik: ";
text T_ext[]		= "\fExtensions thresholds\fSeuils d'extensions\fPragovi dodatkov";
text T_symb[]		= "\fSymbol \fSymbole \fSimbol ";
text T_symled[]		= "\fLed symbols \fSymboles DEL \fLED simboli ";
text T_thr[]		= "\fThreshold\fSeuil\fPrag";
text T_swgrp[5][SWGLEN] = {"\fDisplay pages\fPages d'afficher\fPrikaz strani","\fRelays\fRelais\fRelej","Power MOSFETs",\
"\fWarning lamp\fLampe alerte\fOpozorilna lucka","\fSwitches\fCommutateurs\fStikala"};
text T_swtyp[4][SWLEN] = {"\fLED page\fPage d'afficher DEL\fLED stran","\fRelay\fRelais\fReleji","\fOutput\fSortie\fIzhod","\fSwitch\fCommutateur\fStikalo"};
text T_vmmin[]		= "\fLowest measured speed\fVitesse mesurée minimal\fNajnizja izmerjena hitrost";
text T_brght[]		= "\fOptimizing display\fOptimisation d'affichage\fOptimizacija prikazovalnika";
text T_brsel[3][52]	= {"\fOperating time\fDurée de fonction\fCas obratovanja","\fNormal\fNormale\fNormalno","\fVisibility\fVisibilité\fVidljivost"};
text T_parakt[] = "\fActive parameter set\fJeu de paramètres actif\fAktiven set parametrov";
text T_hell[]		= "\fLED brightness\fLuminosité DEL\fLED osvetlitev";
text T_col2[]		= "\fThreshold Led mixed color\fSeuil de couleur mélangée des DEL\fPrag vecbarvnih LED";
text T_sim[]		= "\fTest speed\fVitesse par test\fTest hitrosti";
text T_modem[]		= "\fRadio modems:\n1. Bluetooth modem\n2. GSM/GPRS modem\
\fLes modems radio:\n1. Modem Bluetooth\n2. Modem GSM/GPRS\
\fRadio modemi\n1. Bluetooth modem\n2. GSM/GPRS modem";
text T_btmenu[]		= "\f\nBluetooth menu\n1. Device name\n2. Pin number\n3. Information\
\f\nMeni bluetooth\n1. Nom du périphérique\n2. Code PIN\n3. Information\
\f\nBluetooth meni\n1. Ime naprave\n2. Pin stevilka\n3. Informacija";
text T_ninst[]		= "\fNot installed\n\fNon installé\n\fNi vgrajeno\n";
text T_btbda[]		= "\fBluetooth address\fL'adresse Bluetooth\fBluetooth naslov";
text T_btname[]		= "\fDevice name\fNom du périphérique\fIme naprave";
text T_pinno[]		= "\fPin number\fCode PIN\fPin stevilka";
text T_btlst[]		= "\fTrusted device list:\fListe d'appareils de confiance:\fTrusted seznam naprav:";
text T_dellst[]		= "\fClear list\fEffacer liste\fIzbriši seznam";
text T_noacc[]		= "\fAccess impossible!\n\fAccès impossible\n\fDostop nemogoc!\n";
text T_gsmmen[]		= "\f\nGSM/Email menu:\f\nGSM/Email menu:\f\nGSM/Email meni:";
text T_vemail[]		= "\fEmail delivery: \fRemise d'email: \fEmail dostava: ";
text T_mailcyc[5][44]	= {"\fNone\fNon\fBrez","\fMemory full\fMemoire pleine\fPoln spomin",\
"\fDaily\fQuotidien\fDnevno","\fWeekly\fHebdomadaire\fTedensko","\fMonthly\fMensuel\fMesecno"};
text T_mtag[]		= "\fDay of month\fJour du mois\fDan v mesecu";
text T_service[4][12]	={"GPRS","SMTP-Server","EMAIL","SMS"};
text T_yconf[]		= "\fConfigure \fConfigurer \fKonfiguracija ";
text T_notcon[]		= "\fnot defined\fnon défini\fni definirana";
text T_change[]		= "\fChange\fChanger\fSprememba";
text T_err[]		= "\fError \fErreur \fNapaka ";
text T_txmail[]		= "\fSend email\fEnvoyer un e-mail\fPoslji email";
text T_txsms[]		= "\fSend SMS\fEnvoyer un SMS\fPoslji SMS";
text T_mailset[]	= "\fEmail delivery set!\fEnsemble envoi de d'email!\fEmail posiljanje nastavljeno!";
text T_status[]		= "\fState\fStatut\fStatus";
text T_mdaten[]		= "\fMeasurement data\fDonnees\fMerilni podatki";
text T_inter[]		= "\fInterface options\fOptions d'interface\fMoznosti vmesnika";
text T_mnow[]		= "\fSend email\fEnvoyer un email\fPoslji email";
text T_snow[]		= "\fSend SMS\fEnvoyer un SMS\fPoslji SMS";
text T_emalarm[]	= "\fEmail alarm\fAlerte Mail\fEmail opozorilo";
text T_smsno[]		= "\fSMS phone number: \fNuméro SMS: \fSMS mobilna stevilka: ";
text T_smsalarm[]	= "\fSMS alarm\fAlerte SMS\fSMS opozorilo";
text T_smsset[]		= "\fSMS delivery set!\fEnsemble envoi de SMS\fSMS posiljanje nastavljeno!";
text T_syserr[]		= "\fSystem error\fErreur système\fSistemska napaka";
text T_rsens[]		= "\fRadar sensitivity\fSensibilité radar\fObcutljivost radar";
text T_symname[]	= "\f\nSymbol name: \f\nNom de symbole: \f\nNome del simbolo: ";
text T_symgrn[]		= "\fSelect symbol group\fSélectionnez le groupe de symbole\fIzberite skupino simbol";
text T_symdef[]		= "\fDefinition of symbols\fDéfinition des symboles\fOpredelitev simbolov";
text T_newgrp[]		= "\fNew symbol group name\fNouveau nom de groupe de symboles\fNovo ime simbol skupine";
text T_usbdis[]		= "\fUnplug USB now!\fDébranchez USB maintenant!\fIzkljucite USB zdaj!";
text T_mgps[]			=	"\fGPS module\fModule GPS\fGPS modul";
text T_gpsmen[]		= "\f\nGPS menu:\n1. Position list\n2. Formatted data output\n3. Raw data (NMEA-183)\
\f\nMenu GPS:\n1. Liste de positions\n2. Données formatées GPS\n3. Données brutes GPS (NMEA-183)\
\f\nGPS menu:\n1. Seznam položaj\n2. Oblikovano izhodni podatki\n3. Surovi podatki (NMEA-183)";
text T_ctrlz[]		= "\fQuit with <CTRL-Z>\fQuittez avec <CTRL-Z>\fKoncaj z <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPosition fix time\fDélai de localisation\fCas fiksna pozicija";
text Tgpsanz[]		= "\fNumber of GPS positions\fNombre de positions GPS\fŠtevilo GPS položajev";

// Ereignistexte (Protokollmeldungen)
text T_poweron[]	= "\fPower on\fPuissance\fVklop";
text T_sommer[]		= "\fSummer time\fHeure d'été\fPoletni cas";
text T_winter[]		= "\fWinter time\fHeure d'hiver\fZimski cas";
text T_messstart[]	= "\fStart of measurement\fDébut de mesure\fZacetek meritev";
text T_memfull[]	= "\fMemory full\fMémoire pleine\fPoln spomin";
text T_defpar[]		= "\fDefault parameter set\fParamètres defaut\fPrivzeti set parametrov";
text T_batlow[]		= "\fBattery voltage < 11,5V\fTension de la batterie < 11,5V\fNapetost baterije < 11,5V";

#endif
