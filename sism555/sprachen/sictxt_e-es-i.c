//-----------------------------------------------------------------------------
//  FILE: sictxt.c         PROJECT: sis3000M
//-----------------------------------------------------------------------------
//  COMMENTS:  Texte englisch/spanisch/portugiesisch
//-----------------------------------------------------------------------------
//  HARDWARE:   viasis 3003 - MB, revision 1.1
//-----------------------------------------------------------------------------
//   COMPILATION: THUMB CODE
//-----------------------------------------------------------------------------
//  VERSION :  0.01
//-----------------------------------------------------------------------------
//  CREATED :   19.07.2013
//-----------------------------------------------------------------------------              
//  AUTHOR :   JG
//-----------------------------------------------------------------------------
//  MODIFICATIONS:
//                 19.07.2013: File creation
//-----------------------------------------------------------------------------

#include "../sictxt.h"   

#if (LANGUAGE == e_es_i)

// Sprachspezifische Texte
text T_ja[ANZSPRACHEN]         	= {'y','s','s'};      // Antwortkürzel für ja
text T_nein[ANZSPRACHEN]        = {'n','n','n'};      // dto. für nein
text T_jn[]                  	= "\f (y/n)?\f (s/n)?\f (s/n)?";
text T_spritm[ANZSPRACHEN][11]  = {"English","Spanish","Italian"};
text T_tver[]      = "\fVersion \fVersión \fVersione ";
text T_winit[]     = "\fDefault parameter set\fInicialización de la fábrica\fSet parametri default";
text T_main[]      = "\f\nMain menu:\n1. Transmit stored data\n2. Online measurement\n3. Test functions\n4. Settings\n5. Radio modems\n6. Information and time\
\f\nMenú principal:\n1. Extracción de datos\n2. Medición online\n3. Funciones de prueba\n4. Ajustes\n5. Radiomódems\n6. Información y hora\
\f\nMenu principale:\n1. Emissione dati\n2. Misure online\n3. Funzioni di prova\n4. Opzioni\n5. Radio modem\n6. Informazioni e tempo";
text T_zuruck[]    = "\fBack\fAtrás\fIndietro";
text T_ausw[]      = "\fYour choice 1...\fSu selección 1...\fVostra selezione 1...";
text T_estmenu[]   = "\fTest functions:\n1. LED display\n2. Flash memory\n3. Real time clock\n4. Main battery\n5. Simulation\n6. Light sensor\
\fMenú de prueba:\n1. Indicador LED\n2. Memoria flash\n3. Reloj en tiempo real\n4. Batería\n5. Simulación\n6. Sensor luminoso\
\fMenu di prova:\n1. Display LED\n2. Memoria Flash\n3. Orologio tempo reale\n4. Batteria\n5. Simulazione\n6. Sensore di luminosità";
text T_kein[]      = "\fNo\fNinguno\fNessuna";
text T_flash[]     = "\f Flash installed\f memoria flash instalada\f  memoria Flash installato";
text T_erase[]     = "\f\nErase \f\nEliminar \f\nElimina ";
text T_write[]     = "\f\nWrite \f\nEscribir \f\nAggiungi ";
text T_read[]			 = "\fRead \fLeer \fLeggere ";
text T_page[]      = "\fPage: \fPágina: \fpagina:";
text T_ok[]        = " ok";
text T_cont[]      = "\fContinue\fContinuar\fContinua";
text T_licht[]     = "\fLichtsensor\fSensor luminoso\fSensore di luminosità";
text T_optmenu[]   = "\f\nSettings:\f\nAjustes:\f\nOpzioni:";
text T_vopt[]				=	"\fSpeed display options\fOpciones de visualización de velocidad\fOpzioni di visualizzazione velocità";
text T_vmin[]      = "\fMinimal speed displayed\fVelocidad mínima\fVelocità minima";
text T_vmax[]      = "\fMaximal speed displayed\fVelocidad máxima\fVelocità massima";
text T_vblk[]      = "\fThreshold blinking LED\fUmbral LED parpadeante\fSoglia LED lampeggiante";
text T_vcolor[]    = "\fThreshold LED color change\fUmbral cambio de color LED\fSoglia LED colore cambiano";
text T_smenu[]     = "\f\nSpecial menu:\f\nMenú especial:\f\nMenù speciale:";
text T_mcyc[]      = "\fMeasurement cycle time\fCiclo de medición\fCyclo di misura";
text T_acyc[]      = "\fLED diplay time\fIntervalo de permanencia LED\fDurata del display LED";
text T_nk[]        = "\fDecimal places\fDecimales\fDecimale";
text T_ende[]      = "\f<Quit with return>\n\f<Fin con regreso>\n\f<Esci con return>\n";
text T_time[]      = "\fTime\fHora\fOra";
text T_date[]      = "\fDate\fFecha\fData";
text T_fdate[]     = "\f(dd.mm.yyyy)\f(DD.MM.AAAA)\f(GG.MM.AAAA)";
text T_ftime[]     = "\f(hh:mm:ss)\f(hh:mm:ss)\f(HH:MM:SS)";
text T_fstime[]    = "\f(hh:mm)\f(hh:mm)\f(HH:MM)";
text T_infomenu[]  = "\f\nInformation:\f\nInformación:\f\nInformazioni:";
text T_serialno[]  = "\f\nSerial number\f\nNuméro de série\f\nNumero di serie";
text T_comment[]   = "\fComments\fComentario\fCommenta";
text T_protocol[]  = "\fProtocol\fProtocolo\fProtocollo";
text T_delete[]    = "\fDelete data\fEliminar datos\fEliminare i dati";
text T_weret[]     = "\f<Continue with Return>\n\f<Continuar con regreso>\n\f<Continua con return>\n";
text T_eraseall[]  = "\f\nAll data will be deleted\f\nSe han eliminado todos los datos\f\nTutti i dati saranno cancellati";
text T_batt[]      = "\fBattery voltage\fTensión de la batería\fTensione batteria";
text T_anzmw[]     = "\fNumber of values\fNº de valores de medición\fNumero valori misurati";
text T_daus[]      = "\f\nSend data\f\nFacilitación de datos\f\nTrasferimento dati";
text T_anzpr[]     = "\fNumber of log entries\fNº de datos de protocolo\fNumero di dati di log";
text T_bdir[]      = "\fBidirectional detection\fRegistro bidireccional\fRilevamento bidirezionale";
text T_vcor[]      = "\fSpeed correction factor\fFactor de corrección de la velocidad\fFattore di correzione della velocità";
text T_vlim[]      = "\fSpeed limit\fLímite de velocidad\fLimite di velocitá";
text T_units[]		=	"\fSpeed measure unit\fUnidad de medida de velocidad\fUnità di misura della velocità";
text T_dismod[]		=	"\fSpeed display\fVisualización de la velocidad\fVisualizzazione della velocità";
text T_dmodes[2][33]	= {"\fabsolute\fabsoluto\fassoluto", "\fdifference\fdiferencia\fdifferenza"};
text T_voff[]      = "\fDisplay offset\fPantalla offset\fOffset di indicazione";
text T_tmen[]      = "\fTime schedule\fCalendario\fProgrammazione dei tempi";
text T_pset[]      = "\fParameter set\fJuego de parámetros\fSet di parametri";
text T_somwin[]    = "\fDaylight saving time\fCambio de hora verano/invierno\fImpostazione ora solare/legale";
text T_zone[4][25] = {"\foff\fapagado\fdisattivata","UTC+0","UTC+1","UTC+2"};
text T_wtag[]      = "\fWeekday:\fDía de la semana:\fGiorno settimanale:";
text T_tag[7][35]  = {"\fSunday\fDomingo\fDomenica","\fMonday\fLunes\fLunedi","\fTuesday\fMartes\fMartedi","\fWednesday\fMiércoles\fMercoledi","\fThursday\fJueves\fGiovedi","\fFriday\fViernes\fVenerdi","\fSaturday\fSábado\fSabato"};
text T_offon[2][25] = {"\foff\fapagado\fdisattiva","\fon\fencendido\fattiva"};
text T_ontage[]    = "\fOperation weekdays: \fDía de encendido: \fGiorni di attivazione: ";
text T_tein[]      = "\fDaily start time: \fHora de encendido: \fOrario di attivazione: ";
text T_taus[]      = "\fDaily stop time: \fHora de apagado: \fOrario di disattivazione: ";
text T_led[]       = "\fLED display: \fIndicador LED: \fDisplay á LED: ";
text T_ext[]       = "\fExtensions thresholds\fUmbrales de los extensiones\fEstensioni";
text T_symb[]      = "\fSymbol  \fSímbolo \fSimbolo ";
text T_symled[]    = "\fLed symbols \fSímbolos LED \fSimboli di Led ";
text T_thr[]       = "\fThreshold\fValor umbral\fSoglia";
text T_swgrp[5][SWGLEN] = {"\fLED pages\fPáginas LED\fPagine di LED","\fRelays\fRelé\fRelè","Power MOSFETs","\fWarning lamp\fLámpara advertencia\fSpia luminosa","\fSwitches\fInterruptores\fCommutatore"};
text T_swtyp[4][SWLEN] = {"\fLED page\fPágina LED\fPagina","\fRelay\fRelé\fRelè","\fOutput\fSalida\fUscita","\fSwitch\fInterruptor\fComutador"};
text T_vmmin[]      = "\fLowest measured speed\fVelocidad de medición más pequeña\fMinima misura velocità";
text T_brght[]      = "\fOptimizing display\fOptimización de la pantalla\fOttimizzazione display";
text T_brsel[3][56] = {"\fOperating time\fTiempo de duración\fOperativo in tempo","\fNormal\fNormal\fEquilibrato","\fVisibility\fVisibilidad\fVisibilità"};
text T_parakt[]		= "\fActive parameter set\fConjunto de parámetros activos\fSet di parametri attivo";
text T_hell[]      	= "\fLED brightness\fLuminosidad LED\fLuminosità del LED";
text T_col2[]      	= "\fThreshold Led mixed color\fUmbral de combinaciones de colores LED\fSoglia LED colore composto";
text T_sim[]      	= "\fTest speed\fVelocidad de prueba\fVelocità di prova";
text T_modem[]      = "\fRadio modems:\n1. Bluetooth modem\n2. Cellular modem\
\fRadiomódems\n1. Módem Bluetooth\n2. Módem celular\
\fRadio modem\n1. Bluetooth Modem\n2. Modem cellulare";
text T_btmenu[]      = "\f\nBluetooth menu\n1. Information\n2. Pin number\
\f\nMenú Bluetooth\n1. Información\n2. Número PIN\
\f\nMenu Bluetooth\n1. Informazioni\n2. Numero PIN";
text T_btprot[]		= "\fProtocol\fProtocole\fProtocollo";
text T_ninst[]      = "\fNot installed!\n\f¡No instalado!\n\fNon installato\n";
text T_btbda[]      = "\fBluetooth address\fDirección Bluetooth\fBluetooth indirizzo";
text T_btname[]     = "\fDevice name\fNombre del dispositivo\fNome dispositivo";
text T_pinno[]      = "\fPin number\fNúmero PIN\fNumero PIN";
text T_btlst[]			= "\fTrusted device list:\fLista de aparatos de confianza:\fElenco dispositivi di fiducia:";
text T_dellst[]			= "\fClear list\fApagar lista\fElimina elenco";
text T_noacc[]      = "\fAccess impossible!\n\f¡Imposible acceder!\n\fImpossibile l'accesso!\n";
text T_gsmmen[]     = "\f\nMobile modem menu:\f\nMenú módem celular\f\nMenu modem cellulare";
text T_vemail[]     = "\fEmail delivery: \fRemise d'email: \fEmail di consegna: ";
text T_mailcyc[5][44] = {"\fNone\fNinguno\fNo","\fMemory full\fMemoria llena\fMemoria piena",\
"\fDaily\fDiario\fQuotidiano","\fWeekly\fSemanal\fSettimanale","\fMonthly\fMensual\fMensile"};
text T_mtag[]       = "\fDay of month\fDía del mes\fGiorno del mese";
text T_mfstd[]			= "\fRadio network\fRed de radio\fRete radio";
text T_yconf[]      = "\fConfigure \fConfigurar \fConfigura ";
text T_notcon[]     = "\fnot defined\fno marcado\fnon impostato";
text T_change[]     = "\fChange\fModificar\fCambiare";
text T_err[]        = "\fError \fError \fErrore ";
text T_txmail[]     = "\fSend email\fEnviar correo electrónico\fInvia email";
text T_txsms[]      = "\fSend SMS\fEnviar SMS\fInvia SMS";
text T_mailset[]    = "\fEmail delivery set!\f¡Envío de correo electrónico establecido!\fEmail consegna impostato";
text T_status[]     = "\fState\fEstatus\fStato";
text T_mdaten[]     = "\fMeasurement data\fDatos de medición\fDati";
text T_inter[]      = "\fInterfaces\fInterfaces\fInterfacce";
text T_mnow[]       = "\fSend email\fEnviar correo electrónico\fInviare una email";
text T_snow[]       = "\fSend SMS\fEnviar SMS\fInviare una SMS";
text T_emalarm[]    = "\fEmail alarm\fAlarma de correo electrónico\fAvviso di posta";
text T_smsno[]      = "\fSMS phone number: \fNúmero telefónico para SMS: \fSMS numero: ";
text T_smsalarm[]   = "\fSMS alarm\fAlarma SMS\fSMS alert";
text T_smsset[]     = "\fSMS delivery set!\f¡Envío de SMS establecido!\fSMS consegna impostato!";
text T_syserr[]     = "\fSystem error\fError del sistema\fErrore di sistema";
text T_rsens[]      = "\fRadar sensitivity\fSensibilidad del radar\fSensibilità del radar";
text T_symname[]		= "\f\nSymbol name: \f\nNombre de símbolo: \f\nNome del símbolo: ";
text T_symgrn[]			= "\fSelect symbol group\fSeleccionar grupo de símbolos\fSeleziona gruppo simbolo";
text T_symdef[]			= "\fDefinition of symbols\fDefinición de los símbolos\fDefinizione di simboli";
text T_newgrp[]			= "\fNew symbol group name\fNuevo nombre de grupo de símbolos\fNuovo nome del gruppo simbolo";
text T_usbdis[]			= "\fUnplug USB now!\fDesconecte USB ahora!\fScollegare USB ora!";
text T_mgps[]			=	"\fGPS module\fMódulo GPS\fModulo GPS";
text T_gpsmen[]		= "\f\nGPS menu:\n1. Position list\n2. Formatted data output\n3. Raw data (NMEA-183)\
\f\nMenú GPS:\n1. Lista Posición\n2. Salida de datos con formato\n3. Los datos en bruto (NMEA-183)\
\f\nMenu GPS:\n1. Lista posizioni GPS\n2. Formattati uscita dei dati GPS\n3. Uscita dati grezzi GPS (NMEA-183)";
text T_ctrlz[]		= "\fQuit with <CTRL-Z>\fSalir con <CTRL-Z>\fTerminare con <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPosition fix time\fTiempo posición fix\fTempo di misura di posizione";
text T_gpsanz[]		= "\fNumber of GPS positions\fNúmero de posiciones GPS\fPosizioni GPS memorizzati";
text T_betr[]			= "\fOperating hours\fHoras de funcionamiento\fOrario d'esercizio";
text T_sstr[]			= "\fSignal strength(0...31)\fIntensidad de señal(0...31)\fPotenza del segnale(0...31)";
text T_tzone[]		= "\fTime zone\fZona horaria\fFuso orario";
text T_conmqtt[]	= "\f\nConnect to MQTT server\f\nConectar al servidor MQTT\f\nCollegarsi a MQTT Server";
text T_btdis[]		= "\f\nBluetooth disconnected\f\nConexión Bluetooth desconectada\f\nConnessione Bluetooth scollegata";
text T_wait[]			= "\fPlease wait...\fEspere por favor...\fAttendere prego...";
text T_gpscon[]		= "\f\nGPS positioning. \f\nPosicionamiento GPS. \f\nPosizionamento GPS. ";
text T_bereit[]		= "\fReady\fListo\f\nPronto";
text T_auto[]			= "\fAutomatic\fAutomáticamente\fAutomaticamente";


// Ereignistexte in Landessprache (Protokollmeldungen), einsprachige Ereignistexte siehe sictxt.c
text T_poweron[]    = "\fPower on\fEncendido\fAccensione";
text T_sommer[]     = "\fSummer time\fEl horario de verano\fOra legale";
text T_winter[]     = "\fWinter time\fHorario de invierno\fOra solare";
text T_messstart[]  = "\fStart of measurement\fInicio de la medición\fInizio di misura";
text T_memfull[]    = "\fMemory full\fMemoria llena\fMemoria piena";
text T_defpar[]     = "\fDefault parameter set\fParámetros por defecto\fParametri di default";
text T_parainit[]   = "\fParameter initialisation\fInicialización de parámetros\fInizializzazione parametri";
text T_batlow[]     = "\fBattery voltage < 11,5V\fTensión de la batería < 11,5V\fTensione batteria < 11,5V";
#endif
