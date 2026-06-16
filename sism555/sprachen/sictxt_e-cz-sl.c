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

#if (LANGUAGE == e_cz_sl)

// Sprachspezifische Texte
text T_ja[ANZSPRACHEN]			= {'y','a','d'};			// answer for yes
text T_nein[ANZSPRACHEN]   		= {'n','n','n'};		// answer for no
text T_jn[]						= "\f (y/n)?\f (a/n)?\f (d/n)?";
text T_spritm[ANZSPRACHEN][10] 	=	{"English","Cesky","Slovenian"};
text T_tver[]		= "\fVersion \fVerze \fRazlicica ";
text T_winit[]    	= "\fDefault parameter set\fTovarni nastaveni\fPrivzeti set parametrov";
text T_main[]		= "\f\nMain menu:\n1. Transmit stored data\n2. Online measurement\n3. Test functions\n4. Settings\n5. Radio modems\n6. Information and time\
\f\nHlavni nabidka:\n1. Prenos ulozenych dat\n2. Online mereni\n3. Testovaci funkce\n4. Nastaveni\n5. Radio modem\n6. Informace a cas\
\f\nGlavni meni:\n1. Prenos podatkov\n2. Meritev na liniji\n3. Testne funkcije\n4. Nastavitve\n5. Radio modems\n6. Informacije in cas";
text T_zuruck[]		= "\fBack\fZpet\fNazaj";
text T_ausw[]		= "\fYour choice 1...\fVase volba 1...\fVasa izbira 1...";
text T_estmenu[] 	= "\fTest functions:\n1. LED display\n2. Flash memory\n3. Real time clock\n4. Main battery\n5. Simulation\n6. Light sensor\
\fTestovaci funkce:\n1. LED display\n2. Flash pamet\n3. Aktualni cas\n4. Baterie\n5. Simulace\n6. Svetelny senzor\
\fTestne funkcije:\n1. LED prikazovalnik\n2. Flash spomin\n3. Realna casovna baza\n4. Stanje baterije\n5. Simulacija\n6. Senzor svetlobe";
text T_kein[]		= "\fNo\fNe\fNe";
text T_flash[]		= "\f Flash installed\f Flash pamet nainstalovana\f Flash vgrajen";
text T_erase[]		= "\f\nErase \f\nVymazat \f\nBrisi ";
text T_write[]		= "\f\nWrite \f\nNapsat \f\nPisi ";
text T_read[]			= "\fRead \fCist \fBrati ";
text T_page[]		= "\fPage: \fStrana: \fStran:";
text T_ok[]			= " ok";
text T_cont[]		= "\fContinue\fPokracovani\fNadaljuj";
text T_licht[]		= "\fLichtsensor\fSvetelny senzor\fSenzor svetlobe";
text T_optmenu[]	= "\f\nSettings:\f\nNastaveni:\f\nNastavitev:";
text T_vopt[]			=	"\fSpeed display options\fNastaveni rezimu\fMoznosti prikaza hitrosti";	
text T_vmin[]		= "\fMinimal speed displayed\fZobrazena minimalni rychlost\fMinimalna hitrost prikaza";
text T_vmax[]		= "\fMaximal speed displayed\fZobrazena maximalni rychlost\fMaksimalna hitrost prikaza";
text T_vblk[]		= "\fThreshold blinking LED\fPrah blikani LED diod\fPrag utripanja LED";
text T_vcolor[]		= "\fThreshold LED color change\fPrah zmeny barvy LED diod\fPrag spremembe barve LED";
text T_smenu[]		= "\f\nSpecial menu:\f\nSpecialni menu:\f\nPosebni meni:";
text T_mcyc[]		= "\fMeasurement cycle time\fDoba mericiho cyklu\fMerilni cikel";
text T_acyc[]		= "\fLED diplay time\fDoba zobrazeni\fCas prikaza LED";
text T_nk[]			= "\fDecimal places\fDesetinne misto\fDecimala";
text T_ende[]		= "\f<Quit with return>\n\f<Vratit se zpet>\n\f<Izhod z 'Enter'>\n";
text T_time[]		= "\fTime\fCas\fCas";
text T_date[]		= "\fDate\fDatum\fDatum";
text T_fdate[]		= "\f(dd.mm.yyyy)\f(dd.mm.rrrr)\f(dd.mm.llll)";
text T_ftime[]  	= "\f(hh:mm:ss)\f(hh:mm:ss)\f(uu:mm:ss)";
text T_fstime[]		= "\f(hh:mm)\f(hh:mm)\f(uu:mm)";
text T_infomenu[]	= "\f\nInformation:\f\nInformace:\f\nInformacije:";
text T_serialno[]	= "\f\nSerial number\f\nSeriove cislo\f\nSerijska stevilka";
text T_comment[]	= "\fComments\fKomentar\fKomentarji";
text T_protocol[]	= "\fProtocol\fProtokol\fProtokol";
text T_delete[]		= "\fDelete data\fSmazat data\fZbrisi podatke";
text T_weret[] 		= "\f<Continue with Return>\n\f<Pokracuj>\n\f<Nadaljujte z 'Enter'>\n";
text T_eraseall[]	= "\f\nAll data will be deleted\f\nVsechna data budou smazana\f\nVsi podatki bodo zbrisani";
text T_batt[]			= "\fBattery voltage\fNapeti na akumulatoru\fNapetost batrije";
text T_anzmw[]		= "\fNumber of values\fPocet hodnot\fStevilo meritev";
text T_daus[]			= "\f\nSend data\f\nPrenest data\f\nPoslji podatke";
text T_anzpr[]		= "\fNumber of log entries\fPocet hodnot v protokolu\fStevilo vnosov v dnevnik";
text T_bdir[]			= "\fBidirectional detection\fObousmerne mereni\fDvosmerna zaznava";
text T_vcor[]     = "\fSpeed correction factor\fKorekcni faktor rychlosti\fKorekcijski faktor hitrosti";
text T_vlim[]			= "\fSpeed limit\fRychlostni limit\fOmejitev hitrosti";
text T_dismod[]		=	"\fSpeed display\fRezim zobrazovani\fNacin prikaza hitrosti";
text T_dmodes[2][33]	= {"\fabsolute\fskutecna\fabsolutna", "\fdifference\fupravena\frazlika"};
text T_units[]	=	"\fSpeed measure unit\fJednotky rychlosti\fHitrost ukrep enota";
text T_voff[]		= "\fDisplay offset\fZobrazovani vypnuto\fZamik prikazovalnika";
text T_tmen[]		= "\fTime schedule\fCasovy harmonogram\fUrnik";
text T_pset[]		= "\fParameter set\fNastaveni parametru\fSet parametrov";
text T_somwin[]		= "\fDaylight saving time\fLetni cas\fNastavitev cas zima/poletje";
text T_zone[4][22]	= {"\foff\fvypnuto\fizklop","UTC+0","UTC+1","UTC+2"};
text T_wtag[]		= "\fWeekday:\fVsedni den:\fDan v tednu:";
text T_tag[7][29]	= {"\fSunday\fNedele\fNedelja","\fMonday\fPondeli\fPonedeljek","\fTuesday\fUtery\fTorek","\fWednesday\fStreda\fSreda","\fThursday\fCtvrtek\fCetrtek","\fFriday\fPatek\fPetek","\fSaturday\fSobota\fSobota"};
text T_offon[2][22]	= {"\foff\fvypnuto\fizklop","\fon\fzapnuto\fvklop"};
text T_ontage[]		= "\fOperation weekdays: \fCinnost v tydnu: \fDnevi obratovanja: ";
text T_tein[]		= "\fDaily start time: \fZacatek cinnosti y dany den: \fDnevni cas vklopa: ";
text T_taus[]		= "\fDaily stop time: \fUkonceni cinnosti v dany den: \fDnevni cas izklopa: ";
text T_led[]		= "\fLED display: \fZobrazovani: \fLED prikazovalnik: ";
text T_ext[]		= "\fExtensions thresholds\fPrahy doplnkovych symbolu\fPragovi dodatkov";
text T_symb[]		= "\fSymbol \fSymbol \fSimbol ";
text T_symled[]		= "\fLed symbols \fLED symboly \fLED simboli ";
text T_thr[]		= "\fThreshold\fPrah\fPrag";
text T_swgrp[5][SWGLEN] = {"\fDisplay pages\fZobrazeni\fPrikaz strani","\fRelays\fRele\fRelej","Power MOSFETs",\
"\fWarning lamp\fVystrazne svetlo\fOpozorilna lucka","\fSwitches\fPrepinace\fStikala"};
text T_swtyp[4][SWLEN] = {"\fLED page\fLED zobrazeni\fLED stran","\fRelay\fRele\fReleji","\fOutput\fVystup\fIzhod","\fSwitch\fPrepinac\fStikalo"};
text T_vmmin[]		= "\fLowest measured speed\fNejnizsi namerena rychlost\fNajnizja izmerjena hitrost";
text T_brght[]		= "\fOptimizing display\fNastaveni zobrazovani\fOptimizacija prikazovalnika";
text T_brsel[3][52]	= {"\fOperating time\fDoba provozu\fCas obratovanja","\fNormal\fNormalni\fNormalno","\fVisibility\fViditelnost\fVidljivost"};
text T_parakt[] = "\fActive parameter set\fAktivni nastaveni\fAktiven set parametrov";
text T_hell[]		= "\fLED brightness\fLED jas\fLED osvetlitev";
text T_col2[]		= "\fThreshold Led mixed color\fPrah stridani barev\fPrag vecbarvnih LED";
text T_sim[]		= "\fTest speed\fTest rychlosti\fTest hitrosti";
text T_modem[]		= "\fRadio modems:\n1. Bluetooth modem\n2. Cellular modem\
\fRadio modem:\n1. Modem Bluetooth\n2. Mobilní modem\
\fRadio modemi\n1. Bluetooth modem\n2. Celicni modem";
text T_btmenu[]		= "\f\nBluetooth menu\n1. Information\n2. Pin number\
\f\nBluetooth menu\n1. Informace\n2. Kod PIN\
\f\nBluetooth meni\n1. Informacija\n2. Pin stevilka";
text T_ninst[]		= "\fNot installed\n\fNenaistalovano\n\fNi vgrajeno\n";
text T_btbda[]		= "\fBluetooth address\fBluetooth nazev\fBluetooth naslov";
text T_btprot[]		= "\fProtocol\fProtokol\fProtokol";
text T_btname[]		= "\fDevice name\fNazev zarizeni\fIme naprave";
text T_pinno[]		= "\fPin number\fKod PIN\fPin stevilka";
text T_btlst[]		= "\fTrusted device list:\fSeznam zarizeni:\fTrusted seznam naprav:";
text T_dellst[]		= "\fClear list\fSeznam prazdny\fIzbriši seznam";
text T_noacc[]		= "\fAccess impossible!\n\fPristup nemozny\n\fDostop nemogoc!\n";
text T_gsmmen[]		= "\f\nMobile modem menu:\f\nNabídka mobilní modem:\f\nMeni mobilni modem:";
text T_vemail[]		= "\fEmail delivery: \fDorucovani e-mailu: \fEmail dostava: ";
text T_mailcyc[5][44]	= {"\fNone\fZadny\fBrez","\fMemory full\fPamet plna\fPoln spomin",\
"\fDaily\fDenne\fDnevno","\fWeekly\fTydne\fTedensko","\fMonthly\fMesicne\fMesecno"};
text T_mtag[]		= "\fDay of month\fDen v mesici\fDan v mesecu";
text T_mfstd[]		= "\fRadio network\fRádiová sít\fRadijsko omrežje";
text T_yconf[]		= "\fConfigure \fNastaveni \fKonfiguracija ";
text T_notcon[]		= "\fnot defined\fNenastaveno\fni definirana";
text T_change[]		= "\fChange\fZmena\fSprememba";
text T_err[]		= "\fError \fChyba \fNapaka ";
text T_txmail[]		= "\fSend email\fPoslat email\fPoslji email";
text T_txsms[]		= "\fSend SMS\fPoslat SMS\fPoslji SMS";
text T_mailset[]	= "\fEmail delivery set!\fNastavit dorucovani emailu!\fEmail posiljanje nastavljeno!";
text T_status[]		= "\fState\fStav\fStatus";
text T_mdaten[]		= "\fMeasurement data\fNamerena data\fMerilni podatki";
text T_inter[]		= "\fInterfaces\fRozhraní\fVmesniki";
text T_mnow[]		= "\fSend email\fPoslat email\fPoslji email";
text T_snow[]		= "\fSend SMS\fPoslat SMS\fPoslji SMS";
text T_emalarm[]	= "\fEmail alarm\fUpozorneni email\fEmail opozorilo";
text T_smsno[]		= "\fSMS phone number: \fCislo SMS: \fSMS mobilna stevilka: ";
text T_smsalarm[]	= "\fSMS alarm\fUpozorneni SMS\fSMS opozorilo";
text T_smsset[]		= "\fSMS delivery set!\fNastaveni dorucovani SMS!\fSMS posiljanje nastavljeno!";
text T_syserr[]		= "\fSystem error\fSystemova chyba\fSistemska napaka";
text T_rsens[]		= "\fRadar sensitivity\fCitlivost radaru\fObcutljivost radar";
text T_symname[]	= "\f\nSymbol name: \f\nNazev symbolu: \f\nIme simbola: ";
text T_symgrn[]		= "\fSelect symbol group\fVybrat skupinu symbolu\fIzberite skupino simbol";
text T_symdef[]		= "\fDefinition of symbols\fNastaveni symbolu\fOpredelitev simbolov";
text T_newgrp[]		= "\fNew symbol group name\fNazev nove skupiny symbolu\fNovo ime simbol skupine";
text T_usbdis[]		= "\fUnplug USB now!\fNyni odpojit USB!\fIzkljucite USB zdaj!";
text T_mgps[]			=	"\fGPS module\fModul GPS\fGPS modul";
text T_gpsmen[]		= "\f\nGPS menu:\n1. Position list\n2. Formatted data output\n3. Raw data (NMEA-183)\
\f\nMenu GPS:\n1. Seznam pozic\n2. Formatovany datovy vystup\n3. Nezpracovana data (NMEA-183)\
\f\nGPS menu:\n1. Seznam položaj\n2. Oblikovano izhodni podatki\n3. Surovi podatki (NMEA-183)";
text T_ctrlz[]		= "\fQuit with <CTRL-Z>\fOdejit s <CTRL-Z>\fKoncaj z <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPosition fix time\fMistni cas\fCas fiksna pozicija";
text T_gpsanz[]		= "\fNumber of GPS positions\fHodnota GPS pozice\fŠtevilo GPS položajev";
text T_betr[]			= "\fOperating hours\fProvozních hodin\fObratovalnih ur";
text T_sstr[]			= "\fSignal strength(0...31)\fSíla signálu(0...31)\fMoc signala(0...31)";
text T_tzone[]		= "\fTime zone\fCasové pásmo\fCasovni pas";
text T_conmqtt[]	= "\f\nConnect to MQTT server\f\nPripojení k serveru MQTT\f\nPovežite se s strežnikom MQTT";
text T_btdis[]		= "\f\nBluetooth disconnected\f\nPripojení Bluetooth je odpojeno\f\nPovezava Bluetooth prekinjena";
text T_wait[]			= "\fPlease wait...\fProsím, cekejte...\fProsim pocakaj...";
text T_gpscon[]		= "\f\nGPS positioning. \f\nGPS urcování polohy. \f\nGPS pozicioniranje. ";
text T_bereit[]		= "\fReady\fPripraven\f\nPripravljen";
text T_auto[]			= "\fAutomatic\fAutomatický\fAvtomatsko ";

// Ereignistexte in Landessprache (Protokollmeldungen), einsprachige Ereignistexte siehe sictxt.c
text T_poweron[]	= "\fPower on\fZapnout\fVklop";
text T_sommer[]		= "\fSummer time\fLetni cas\fPoletni cas";
text T_winter[]		= "\fWinter time\fZimni cas\fZimski cas";
text T_messstart[]	= "\fStart of measurement\fZacatek mereni\fZacetek meritev";
text T_memfull[]	= "\fMemory full\fPamet plna\fPoln spomin";
text T_defpar[]		= "\fDefault parameter set\fTovarni nastaveni\fPrivzeti set parametrov";
text T_batlow[]		= "\fBattery voltage < 11,5V\fNapeti akumulatoru < 11,5V\fNapetost baterije < 11,5V";
text T_parainit[]	= "Parameter initialisation";
#endif
