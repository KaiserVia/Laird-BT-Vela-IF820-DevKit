//-----------------------------------------------------------------------------
//  FILE: sictxt.c			PROJECT: Viasis Compact / Viasis 3003
//-----------------------------------------------------------------------------
//  COMMENTS:  Texte deutsch/französisch/italienisch/polski
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003, revision 00, 13.10.2010
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

#include "..\sictxt.h"	

#if (LANGUAGE == d_f_pl)

// Sprachspezifische Texte
text T_ja[ANZSPRACHEN]			= {'j','o','t'};		// Antwortkürzel für ja
text T_nein[ANZSPRACHEN]   		= {'n','n','n'};		// dto. für nein
text T_jn[]						= "\f (j/n)?\f (o/n)?\f (t/n)?";
text T_spritm[ANZSPRACHEN][10] 	= {"German","French","Polski"};
text T_tver[]		= "\fVersion \fVersion \fWersja ";
text T_winit[]    	= "\fWerkinitialisierung\fInitialiser les paramètres\fInicjalizacja urz¹dzenia";
text T_main[]		= "\f\nHauptmenü:\n1. Datenausgabe\n2. Online messen\n3. Testfunktionen\n4. Einstellungen\n5. Funkmodems\n6. Information und Zeit\
\f\nMenu principal:\n1. Transfert des données\n2. Affichage en connexion PC\n3. Test du système\n4. Changer les paramètres\n5. Les modems radio\n6. Information et temps\
\f\nMenu g³ówne:\n1. Pobierz dane\n2. Pomiar online\n3. Funkcje testowe\n4. Parametry\n5. Modem\n6. Informacja i Czas";
text T_zuruck[]		= "\fzurück\fRetour\fWstecz";
text T_ausw[]		= "\fIhre Auswahl 1...\fVotre choix 1...\fTwój wybór 1...";
text T_estmenu[] 	= "\fTestmenü:\n1. LED-Anzeige\n2. Flash Speicher\n3. Echtzeituhr\n4. Batterie\n5. Simulation\n6. Lichtsensor\
\fTest du système:\n1. Affichage DEL\n2. Mémoire Flash\n3. Horodateur\n4. Batterie\n5. Simulation\n6. Capteur de lumière\
\fMenu testowe:\n1. Wyœwietlacz LED\n2. Pamiêæ Flash\n3. Czas rzeczywisty\n4. Bateria\n5. Symulacja\n6. Czujnik œwiat³a";
text T_kein[]		= "\fKein\fNon\f¯aden";
text T_flash[]		= "\f Flash Speicher installiert\f mèmoire Flash installé\f Pamiêæ flash zainstalowana";
text T_erase[]		= "\f\nLösche \f\nEfface \f\nUsuwam  ";
text T_write[]		= "\f\nSchreibe \f\nEcrit \f\nZapisuje ";
text T_read[]			= "\fLese \fLire \fCzytac ";
text T_page[]		= "\fSeite: \fpage: \fStrona: ";
text T_ok[]			= " ok";
text T_cont[]		= "\fFortfahren\fContinuer\fKontynuuj";
text T_licht[]		= "\fLichtsensor\fCapteur de lumière\fCzujnik œwiat³a";
text T_optmenu[]	= "\f\nEinstellungen:\f\nChanger les paramètres:\f\nUstawienia";
text T_vopt[]			=	"\fOptionen Geschwindigkeitsanzeige\fOptions d'affichage de la vitesse\fOpcje Predkosciomierz";	
text T_vmin[]		= "\fMindestgeschwindigkeit\fAffichage vitesse minimale\fMinimalna prêdkoœæ";
text T_vmax[]		= "\fMaximalgeschwindigkeit\fAffichage vitesse maximale\fMaksymalna prêdkoœæ";
text T_vblk[]		= "\fSchwelle blinkende LED\fSeuil DEL clignotant\fPróg migania LED";
text T_vcolor[]		= "\fSchwelle LED Farbumschaltung\fSeuil DEL changé de couleur\fPróg zmiany kolorów LED";
text T_smenu[]		= "\f\nSondermenü:\f\nMenu spécial:\f\nMenu specjalne:";
text T_mcyc[]		= "\fMesszyklus\fCycle de mesure\fCykl pomiarowy";
text T_acyc[]		= "\fLED Anzeigedauer\fDurée d'affichage DEL\fCzas wyœwietlania LED";
text T_nk[]			= "\fNachkommastelle\fDécimale\fPo przecinku";
text T_ende[]		= "\f<Ende mit Return>\n\f<Fin avec 'Entrée'>\n\f<Koniec i wstecz>\n";
text T_time[]		= "\fZeit\fHeure\fGodzina";
text T_date[]		= "\fDatum\fDate\fData";
text T_fdate[]		= "\f(TT.MM.JJJJ)\f(jj.mm.aaaa)\f(DD.MM.RRRR)";
text T_ftime[]  	= "\f(hh:mm:ss)\f(hh:mm:ss)\f(hh:mm:ss)";
text T_fstime[]		= "\f(hh:mm)\f(hh:mm)\f(hh:mm)";
text T_infomenu[]	= "\fInformation:\fInformation:\fInformacja:";
text T_serialno[]	= "\f\nSeriennummer\f\nNuméro de série\f\nNumer seryjny";
text T_comment[]	= "\fAnmerkungen\fCommentaire\fKomentarze";
text T_protocol[]	= "\fProtokoll\fJournal\fProtokó³";
text T_delete[]		= "\fDaten löschen\fEffacer les données\fUsuñ dane";
text T_weret[] 		= "\f<Weiter mit Return>\n\f<Suite avec 'Entrée'>\n\f<Dalej i wstecz>\n";
text T_eraseall[]	= "\f\nAlle Daten werden gelöscht\f\nToutes les données seront effacées\f\nWszystkie dane zostan¹ usuniête";
text T_batt[]			= "\fBatteriespannung\fTension de la batterie\fNapiêcie baterii";
text T_anzmw[]		= "\fAnzahl Messwerte\fNombre des données\fLiczba wartoœci pomiarowych";
text T_daus[]			= "\f\nDaten ausgeben\f\nTransfer données\f\nPobierz dane";
text T_anzpr[]		= "\fAnzahl Protokolldaten\fNombre de données de log\fLiczba danych protoko³ów";
text T_bdir[]			= "\fBidirektionale Erfassung\fDétection bidirectionnel\fPomiar w obu kierunkach";
text T_vcor[]   	= "\fKorrekturfaktor Geschwindigkeit\fFacteur de correction de la vitesse\fWspó³czynnik koryguj¹cy prêdkoœæ";
text T_vlim[]			= "\fTempolimit\fLimitation de vitesse\fOgraniczenie prêdkoœci";
text T_units[]		=	"\fGeschwindigkeitsmesseinheit\fUnité de mesure de vitesse\fUrzadzenie pomiaru predkosci";
text T_dismod[]		=	"\fAnzeige Geschwindigkeit\fAffichage de la vitesse\fTryb wyswietlania predkosci";
text T_dmodes[2][33]	= {"\fabsolut\fabsolu\fabsolut", "\fdifferenz\fdifférence\fróznica"};
text T_voff[]		= "\fAnzeigeoffset\fEcart minimale\fOffset wyœwietlania";
text T_tmen[]		= "\fZeitplanung\fPlanification du temps\fPlan czasowy";
text T_pset[]		= "\fParametersatz\fJeu de paramètres\fZestaw parametrów";
text T_somwin[]		= "\fSommer-/Winterzeitumstellung\fPermutation été/hiver\fCzas letni/zimowy";
text T_zone[4][28]	= {"\faus\fdésactive\fwy³¹czony","UTC+0","UTC+1","UTC+2"};
text T_wtag[]		= "\fWochentag: \fJour ouvrable: \fDzieñ tygodnia: ";
text T_tag[7][29]	= {"\fSonntag\fDimanche\fNiedziela","\fMontag\fLundi\fPoniedzia³ek","\fDienstag\fMardi\fWtorek","\fMittwoch\fMercredi\fŒroda","\fDonnerstag\fJeudi\fCzwartek","\fFreitag\fVendredi\fPi¹tek","\fSamstag\fSamedi\fSobota"};
text T_offon[2][28]	= {"\faus\fdésactive\fwy³¹czony","\fein\factivé\fw³¹czony"};
text T_ontage[]		= "\fEinschalttage: \fJour opérationnel: \fDni uruchomienia: ";
text T_tein[]		= "\fEinschaltzeit: \fHeure du début: \fGodzina uruchomienia: ";
text T_taus[]		= "\fAusschaltzeit: \fHeure de la fin: \fGodzina wy³¹czenia: ";
text T_led[]		= "\fLED Anzeige: \fAffichage á DEL: \fWyœwietlacz LED";
text T_ext[]		= "\fSchaltschwellen Erweiterungen\fSeuils des extensions\fRozszerzenie progów prze³¹czania";
text T_symb[]		= "\fSymbol \fSymbole \fSymbol ";
text T_symled[]		= "\fLed Symbole \fSymboles DEL \fSymbol LED ";
text T_thr[]		= "\fSchwellwert\fSeuil\fPróg";
text T_swgrp[5][SWGLEN] = {"\fLED Seiten\fPages d'afficher\fStrony wyœwietlania","\fRelais\fRelais\fPrzekazniki","Power MOSFETs",\
"\fWarnleuchten\fLampe alerte\fSwiatla ostrzegawcze","\fSchalter\fCommutateurs\fPrzelaczniki"};
text T_swtyp[4][SWLEN] = {"\fAnzeigeseite\fPage d'afficher\fStrona LED","\fRelais\fRelais\fPrzekaznik","\fSchalter\fCommutateur\fPrzelacznik","\fAusgang\fSortie\fWyjœcie"};
text T_vmmin[]		= "\fKleinste Messgeschwindigkeit\fVitesse mesurée minimal\fNajni¿sza prêdkoœæ";
text T_brght[]		= "\fAnzeigeoptimierung\fOptimisation d'affichage\fOptymalizacja wyœwietlania";
text T_brsel[3][52]	= {"\fBetriebsdauer\fDurée de fonction\fCzas pracy","\fNormal\fNormale\fNormalny","\fSichtbarkeit\fVisibilité\fWidocznoœæ"};
text T_parakt[] = "\fAktiver Parametersatz\fJeu de paramètres actif\fAktywny zestaw parametrów";
text T_hell[]		= "\fLED Helligkeit\fLuminosité DEL\fJasnoœæ LED";
text T_col2[]		= "\fSchwelle LED Mischfarbe\fSeuil DEL de couleur mélangée\fPróg mieszania kolorów LED";
text T_sim[]		= "\fTestgeschwindigkeit\fVitesse par test\fPrêdkoœæ testowa";
text T_modem[]		= "\fFunkmodems:\n1. Bluetooth Modem\n2. Mobilfunk Modem\
\fLes modems radio:\n1. Modem Bluetooth\n2. Modem Cellulaire\
\fModemy:\n1. Modem Bluetooth\n2. Modem komórkowy";
text T_btmenu[]		= "\f\nBluetooth Menü\n1. Information\n2. Pinnummer \
\f\nMenu Bluetooth\n1. Information\n2. Code PIN\
\f\nMenu Bluetooth\n1. Informacja\n2. Numer PIN";
text T_btprot[]		= "\fProtokoll\fProtocollo\fProtokol";
text T_ninst[]		= "\fNicht installiert\n\fNon installé\n\fNie zainstalowano\n";
text T_btbda[]		= "\fBluetooth Adresse\fL'adresse Bluetooth\fAdres Bluetooth";
text T_btname[]		= "\fGerätename\fNom du périphérique\fNazwa urz¹dzenia";
text T_pinno[]		= "\fPinnummer\fCode PIN\fNumer PIN";
text T_btlst[]		= "\fListe vertrauter Geräte:\fListe d'appareils de confiance:\fLista zaufanym urzadzenie:";
text T_dellst[]		= "\fListe löschen\fEffacer liste\fUsun liste";
text T_btvis[]		= "\fSichtbarkeit\fVisibilité\fWidodcznoœæ"; 
text T_noacc[]		= "\fZugriff nicht möglich!\n\fAccès impossible\n\fDostêp niemo¿liwy\n";
text T_gsmmen[]		= "\f\nMobilfunk Menü:\f\nMenu modem mobile\f\nMenu mobilny modem";
text T_vemail[]		= "\fEmail Versand: \fRemise d'email: \fWysy³ka Email ";
text T_mailcyc[5][44]	= {"\fKeiner\fNon\f¯aden","\fSpeicher voll\fMemoire pleine\fPamiêæ pe³na",\
"\fTäglich\fQuotidien\fDziennie","\fWöchentlich\fHebdomadaire\fTygodniowo","\fMonatlich\fMensuel\fMiesiêcznie"};
text T_mtag[]		= "\fTag des Monats\fJour du mois\fDzieñ miesi¹ca";
text T_mfstd[]		= "\fFunknetz\fRéseau radio\fSiec radiowa";
text T_yconf[]		= "\fKonfiguriere \fConfigurer \fKonfiguruje ";
text T_notcon[]		= "\fnicht definiert\fnon défini\fnie ustawione";
text T_change[]		= "\fÄndern\fChanger\fZmieñ";
text T_err[]		= "\fFehler \fErreur \fB³¹d ";
text T_txmail[]		= "\fSende Email\fEnvoyer email\fWyœlij Email";
text T_txsms[]		= "\fSende SMS\fEnvoyer SMS\fWyœlij SMS";
text T_mailset[]	= "\fEmail Versand eingerichtet!\fEnsemble envoi de d'email!\fWysy³ka Email skonfigurowana!";
text T_status[]		= "\fStatus\fStatut\fStatus";
text T_mdaten[]		= "\fMessdaten\fDonnees\fDane pomiarowe";
text T_inter[]		= "\fSchnittstellen\fInterfaces\fInterfejsi";
text T_mnow[]		= "\fEmail versenden\fEnvoyer un email\fWyœlij Email";
text T_snow[]		= "\fSMS versenden\fEnvoyer un SMS\fWyœlij SMS";
text T_emalarm[]	= "\fEmail Alarm\fAlerte Mail\fAlert Email";
text T_smsno[]		= "\fSMS Rufnummer: \fNuméro SMS: \fNumer SMS: ";
text T_smsalarm[]	= "\fSMS Alarm\fAlerte SMS\fAlert SMS";
text T_smsset[]		= "\fSMS Versand eingerichtet!\fEnsemble envoi de SMS\fWysy³ka SMS skonfigurowana!";
text T_syserr[]		= "\fSystemfehler\fErreur système\fB³¹d systemowy";
text T_rsens[]		= "\fRadar Empfindlichkeit\fSensibilité radar\fCzu³oœæ radaru";
text T_symname[]	= "\f\nSymbolname: \f\nNom de symbole: \f\nNazwa symbol: ";
text T_symgrn[]		= "\fAuswahl der Symbolgruppe\fSélectionnez le groupe de symbole\fWybór grup symboli";
text T_symdef[]		= "\fDefinition der Symbole\fDéfinition des symboles\fDefinicja symboli";
text T_newgrp[]		= "\fNeuer Symbolgruppenname\fNouveau nom de groupe de symboles\fNowa nazwa grupy symbolem";
text T_usbdis[]		= "\fTrennen Sie jetzt die USB Verbindung jetzt!\fDébranchez USB maintenant!\fOdlacz USB teraz!";
text T_mgps[]			=	"\fGPS Modul\fModule GPS\fModul GPS";
text T_gpsmen[]		= "\f\nGPS Menü:\n1. Positionsliste\n2. Formatierte GPS Datenausgabe\n3. GPS Rohdaten (NMEA-183)\
\f\nMenu GPS:\n1. Liste de positions\n2. Données formatées GPS\n3. Données brutes GPS (NMEA-183)\
\f\nMenu GPS:\n1. Lista pozycji\n2. Sformatowane dane wyjsciowe\n3. Surowce danych GPS (NMEA-183)";
text T_ctrlz[]		= "\fBeenden mit <CTRL-Z>\fQuittez avec <CTRL-Z>\fWyjdz z <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPositionsbestimmungszeit\fDélai de localisation\fCzas ustalenie pozycji";
text T_gpsanz[]		= "\fPositionsbestimmungen\fNombre de positions GPS\fLiczba pozycji GPS";
text T_betr[]			= "\fBetriebsstunden\fHeures de fonctionnement\fRoboczogodzin"; 
text T_sstr[]			= "\fSignalstärke(0...31)\fPuissance du signal(0...31)\fSila sygnalu(0...31)";
text T_tzone[]		= "\fZeitzone\fFuseau horaire\fStrefa czasu";
text T_conmqtt[]	= "\f\nVerbinde mit MQTT Server\f\nSe connecter au serveur MQTT\f\nPolacz sie z serwerem MQTT";
text T_btdis[]		= "\f\nBluetoothverbindung getrennt\f\nConnexion Bluetooth déconnectée\f\nRozlaczono polaczenie Bluetooth";
text T_wait[]			= "\fBitte warten...\fS.v.p., attendez...\fProsze czekac...";
text T_gpscon[]		= "\f\nGPS-Positionsbestimmung. \f\nPositionnement GPS \f\nPozycjonowanie GPS. ";
text T_bereit[]		= "\fBereit\fPrêt\f\nGotowy";
text T_auto[]			= "\fAutomatisch\fAutomatiquement\fAutomatyczny";

// Ereignistexte in Landessprache (Protokollmeldungen), einsprachige Ereignistexte siehe sictxt.c
text T_poweron[]	= "\fEingeschaltet\fPuissance\fW³aœciwoœci";
text T_sommer[]		= "\fSommerzeit\fHeure d'été\fCzas letni";
text T_winter[]		= "\fWinterzeit\fHeure d'hiver\fCzas zimowy";
text T_messstart[]	= "\fMessbeginn\fDébut de mesure\fPocz¹tek pomiarów";
text T_memfull[]	= "\fSpeicher voll \fMémoire pleine \fPe³na pamiêæ ";
text T_defpar[]		= "\fStandardeinstellung\fParamètres default\fUstawienia domyœlne";
text T_batlow[]		= "\fBatteriespannung < 11,5V\fTension de la batterie < 11,5V\fNapiêcie baterii < 11,5V";
text T_parainit[]	= "Parameter initialisation";

 #endif
