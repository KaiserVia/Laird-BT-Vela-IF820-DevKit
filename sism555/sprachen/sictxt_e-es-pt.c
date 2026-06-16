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

#if (LANGUAGE == e_es_pt)

// Sprachspezifische Texte
text T_ja[ANZSPRACHEN]         	= {'y','s','s'};      // Antwortkürzel für ja
text T_nein[ANZSPRACHEN]        = {'n','n','n'};      // dto. für nein
text T_jn[]                  	= "\f (y/n)?\f (s/n)?\f (s/n)?";
text T_spritm[ANZSPRACHEN][11]  = {"English","Spanish","Portuguese"};
text T_tver[]      = "\fVersion \fVersión \fVersão ";
text T_winit[]     = "\fDefault parameter set\fInicialización de la fábrica\fReinicialização com parâmetros de fábrica";
text T_main[]      = "\f\nMain menu:\n1. Transmit stored data\n2. Online measurement\n3. Test functions\n4. Settings\n5. Radio modems\n6. Information and time\
\f\nMenú principal:\n1. Extracción de datos\n2. Medición online\n3. Funciones de prueba\n4. Ajustes\n5. Radiomódems\n6. Información y hora\
\f\nMenú principal:\n1. Recolha de dados\n2. Leituras online\n3. Funções de testes\n4. Ajustes\n5. Modém Rádio\n6. Informação e hora";
text T_zuruck[]    = "\fBack\fAtrás\fVoltar";
text T_ausw[]      = "\fYour choice 1...\fSu selección 1...\fSua escolha 1...";
text T_estmenu[]   = "\fTest functions:\n1. LED display\n2. Flash memory\n3. Real time clock\n4. Main battery\n5. Simulation\n6. Light sensor\
\fMenú de prueba:\n1. Indicador LED\n2. Memoria flash\n3. Reloj en tiempo real\n4. Batería\n5. Simulación\n6. Sensor luminoso\
\fMenú de testes:\n1. Indicador LED\n2. Memória flash\n3. Relógio em tempo real\n4. Batería\n5. Simulacão\n6. Sensor luminoso";
text T_kein[]      = "\fNo\fNinguno\fNão";
text T_flash[]     = "\f Flash installed\f memoria flash instalada\f memória flash instalada";
text T_erase[]     = "\f\nErase \f\nEliminar \f\nLimpar ";
text T_write[]     = "\f\nWrite \f\nEscribir \f\nEscrever ";
text T_read[]			 = "\fRead \fLeer \fLer ";
text T_page[]      = "\fPage: \fPágina: \fPágina:";
text T_ok[]        = " ok";
text T_cont[]      = "\fContinue\fContinuar\fContinuar";
text T_licht[]     = "\fLichtsensor\fSensor luminoso\fSensor luminoso";
text T_optmenu[]   = "\f\nSettings:\f\nAjustes:\f\nAjustes:";
text T_vopt[]				=	"\fSpeed display options\fOpciones de visualización de velocidad\fOpções de exibição de velocidade";
text T_vmin[]      = "\fMinimal speed displayed\fVelocidad mínima\fVelocidade mínima";
text T_vmax[]      = "\fMaximal speed displayed\fVelocidad máxima\fVelocidade máxima";
text T_vblk[]      = "\fThreshold blinking LED\fUmbral LED parpadeante\fGama variação LED intermitente";
text T_vcolor[]    = "\fThreshold LED color change\fUmbral cambio de color LED\fGama variação da cor LED";
text T_smenu[]     = "\f\nSpecial menu:\f\nMenú especial:\f\nMenú especial:";
text T_mcyc[]      = "\fMeasurement cycle time\fCiclo de medición\fCiclo de medição";
text T_acyc[]      = "\fLED diplay time\fIntervalo de permanencia LED\fTempo Duração display activo";
text T_nk[]        = "\fDecimal places\fDecimales\fDécimal";
text T_ende[]      = "\f<Quit with return>\n\f<Fin con regreso>\n\f<Sair com retorno>\n";
text T_time[]      = "\fTime\fHora\fHora";
text T_date[]      = "\fDate\fFecha\fData";
text T_fdate[]     = "\f(dd.mm.yyyy)\f(DD.MM.AAAA)\f(DD.MM.AAAA)";
text T_ftime[]     = "\f(hh:mm:ss)\f(hh:mm:ss)\f(hh:mm:ss)";
text T_fstime[]    = "\f(hh:mm)\f(hh:mm)\f(hh:mm)";
text T_infomenu[]  = "\f\nInformation:\f\nInformación:\f\nInformação:";
text T_serialno[]  = "\f\nSerial number\f\nNuméro de série\f\nNº de serie";
text T_comment[]   = "\fComments\fComentario\fComente";
text T_protocol[]  = "\fProtocol\fProtocolo\fProtocolo";
text T_delete[]    = "\fDelete data\fEliminar datos\fLimpar dados";
text T_weret[]     = "\f<Continue with Return>\n\f<Continuar con regreso>\n\f<Continuar com retorno>\n";
text T_eraseall[]  = "\f\nAll data will be deleted\f\nSe han eliminado todos los datos\f\nDados apagados da mémoria";
text T_batt[]      = "\fBattery voltage\fTensión de la batería\fTensão da bateria";
text T_anzmw[]     = "\fNumber of values\fNº de valores de medición\fNº de valores medidos";
text T_daus[]      = "\f\nSend data\f\nFacilitación de datos\f\nEnvio de dados";
text T_anzpr[]     = "\fNumber of log entries\fNº de datos de protocolo\fNº de datos de armazenados";
text T_bdir[]      = "\fBidirectional detection\fRegistro bidireccional\fDetecção bidireccional";
text T_vcor[]      = "\fSpeed correction factor\fFactor de corrección de la velocidad\fFactor de correcção de velocidade";
text T_vlim[]      = "\fSpeed limit\fLímite de velocidad\fVelocidade máxima permitida";
text T_units[]		=	"\fSpeed measure unit\fUnidad de medida de velocidad\fUnidade de medida de velocidade";
text T_dismod[]		=	"\fSpeed display\fVisualización de la velocidad\fVisualização da velocidade";
text T_dmodes[2][33]	= {"\fabsolute\fabsoluto\fabsoluto", "\fdifference\fdiferencia\fdiferença"};
text T_voff[]      = "\fDisplay offset\fPantalla offset\fDisplay offset";
text T_tmen[]      = "\fTime schedule\fCalendario\fCalendario";
text T_pset[]      = "\fParameter set\fJuego de parámetros\fConfiguração de parâmetros";
text T_somwin[]    = "\fDaylight saving time\fCambio de hora verano/invierno\fMudança horário verão/inverno";
text T_zone[4][25] = {"\foff\fapagado\fdesactivado","UTC+0","UTC+1","UTC+2"};
text T_wtag[]      = "\fWeekday:\fDía de la semana:\fDia da semana:";
text T_tag[7][35]  = {"\fSunday\fDomingo\fDomingo","\fMonday\fLunes\fSegunda-feira","\fTuesday\fMartes\fTerça-feira","\fWednesday\fMiércoles\fQuarta-feira","\fThursday\fJueves\fQuinta-feira","\fFriday\fViernes\fSexta-feira","\fSaturday\fSábado\fSabado"};
text T_offon[2][25] = {"\foff\fapagado\fdesactivado","\fon\fencendido\factivado"};
text T_ontage[]    = "\fOperation weekdays: \fDía de encendido: \fDia de inicio funcionamento: ";
text T_tein[]      = "\fDaily start time: \fHora de encendido: \fHora inicio de funcionamento: ";
text T_taus[]      = "\fDaily stop time: \fHora de apagado: \fHora fim de funcionamento: ";
text T_led[]       = "\fLED display: \fIndicador LED: \fDisplay LED: ";
text T_ext[]       = "\fExtensions thresholds\fUmbrales de los extensiones\fGama de extensões";
text T_symb[]      = "\fSymbol  \fSímbolo \fSimbolo ";
text T_symled[]    = "\fLed symbols \fSímbolos LED \fSimbolos LED ";
text T_thr[]       = "\fThreshold\fValor umbral\fGama de valores";
text T_swgrp[5][SWGLEN] = {"\fLED display pages\fPáginas LED\fPáginas LED","\fRelays\fRelé\fRelé","Power MOSFETs","\fWarning lamp\fLámpara advertencia\fLâmpada aviso","\fSwitches\fInterruptores\fInterruptores"};
text T_swtyp[4][SWLEN] = {"\fLED page\fPágina LED\fPágina LED","\fRelay\fRelé\fRelé","\fOutput\fSalida\fSaída","\fSwitch\fInterruptor\fComutador"};
text T_vmmin[]      = "\fLowest measured speed\fVelocidad de medición más pequeña\fVelocidad de medición mínima";
text T_brght[]      = "\fOptimizing display\fOptimización de la pantalla\fOptimização do Display";
text T_brsel[3][56] = {"\fOperating time\fTiempo de duración\fTempo de duração","\fNormal\fNormal\fNormal","\fVisibility\fVisibilidad\fVisibilidade"};
text T_parakt[]		= "\fActive parameter set\fConjunto de parámetros activos\fConjunto de parâmetros activos";
text T_hell[]      	= "\fLED brightness\fLuminosidad LED\fLuminosidade dos LEDs";
text T_col2[]      	= "\fThreshold Led mixed color\fUmbral de combinaciones de colores LED\fGama de combinações de cores LED";
text T_sim[]      	= "\fTest speed\fVelocidad de prueba\fTeste de Velocidade";
text T_modem[]      = "\fRadio modems:\n1. Bluetooth modem\n2. Cellular modem\
\fRadiomódems\n1. Módem Bluetooth\n2. Módem celular\
\fRadiomodems\n1. Modem Bluetooth\n2. Modem celular";
text T_btmenu[]      = "\f\nBluetooth menu\n1. Information\n2. Pin number\
\f\nMenú Bluetooth\n1. Información\n2. Número PIN\
\f\nMenú Bluetooth\n1. Informação\n2. Código PIN";
text T_btprot[]		= "\fProtocol\fProtocole\fProtocol";
text T_ninst[]      = "\fNot installed!\n\f¡No instalado!\n\f¡Não instalado!\n";
text T_btbda[]      = "\fBluetooth address\fDirección Bluetooth\fEndereço Bluetooth";
text T_btname[]     = "\fDevice name\fNombre del dispositivo\fNome do dispositivo";
text T_pinno[]      = "\fPin number\fNúmero PIN\fCódigo PIN";
text T_btlst[]			= "\fTrusted device list:\fLista de aparatos de confianza:\fLista de dispositivos confiáveis:";
text T_dellst[]			= "\fClear list\fApagar lista\fEliminar lista";
text T_noacc[]      = "\fAccess impossible!\n\f¡Imposible acceder!\n\f¡Sem acesso!\n";
text T_gsmmen[]     = "\f\nMobile modem menu:\f\nMenú módem celular\f\nMenu de modem celular";
text T_vemail[]     = "\fEmail delivery: \fRemise d'email: \fEnvío correo electrónico: \fEnvio de E-mail: ";
text T_mailcyc[5][44] = {"\fNone\fNinguno\fNenhum","\fMemory full\fMemoria llena\fMemória Cheia",\
"\fDaily\fDiario\fDiário","\fWeekly\fSemanal\fSemanal","\fMonthly\fMensual\fMensal"};
text T_mtag[]       = "\fDay of month\fDía del mes\fDia do mês";
text T_mfstd[]			= "\fRadio network\fRed de radio\fRede de rádio";
text T_yconf[]      = "\fConfigure \fConfigurar \fConfigurar ";
text T_notcon[]     = "\fnot defined\fno marcado\fnão definido";
text T_change[]     = "\fChange\fModificar\fModificar";
text T_err[]        = "\fError \fError \fErro ";
text T_txmail[]     = "\fSend email\fEnviar correo electrónico\fEnviar E-mail";
text T_txsms[]      = "\fSend SMS\fEnviar SMS\fEnviar SMS";
text T_mailset[]    = "\fEmail delivery set!\f¡Envío de correo electrónico establecido!\f¡E-mail Enviado!";
text T_status[]     = "\fState\fEstatus\fEstado";
text T_mdaten[]     = "\fMeasurement data\fDatos de medición\fDados recolhidos";
text T_inter[]      = "Interfaces";
text T_mnow[]       = "\fSend email\fEnviar correo electrónico\fEnviar E-mail";
text T_snow[]       = "\fSend SMS\fEnviar SMS\fEnviar SMS";
text T_emalarm[]    = "\fEmail alarm\fAlarma de correo electrónico\fE-mail de Alarme";
text T_smsno[]      = "\fSMS phone number: \fNúmero telefónico para SMS: \fNúmero para envio de SMS: ";
text T_smsalarm[]   = "\fSMS alarm\fAlarma SMS\fSMS de Alarme";
text T_smsset[]     = "\fSMS delivery set!\f¡Envío de SMS establecido!\f¡SMS enviado!";
text T_syserr[]     = "\fSystem error\fError del sistema\fErro de sistema";
text T_rsens[]      = "\fRadar sensitivity\fSensibilidad del radar\fSensibilidade do radar";
text T_symname[]		= "\f\nSymbol name: \f\nNombre de símbolo: \f\nNome do símbolo: ";
text T_symgrn[]			= "\fSelect symbol group\fSeleccionar grupo de símbolos\fEscolha um grupo símbolo";
text T_symdef[]			= "\fDefinition of symbols\fDefinición de los símbolos\fDefinição de símbolos";
text T_newgrp[]			= "\fNew symbol group name\fNuevo nombre de grupo de símbolos\fNovo nome do símbolo do grupo";
text T_usbdis[]			= "\fUnplug USB now!\fDesconecte USB ahora!\fDesligue USB agora!";
text T_mgps[]			=	"\fGPS module\fMódulo GPS\fMódulo GPS";
text T_gpsmen[]		= "\f\nGPS menu:\n1. Position list\n2. Formatted data output\n3. Raw data (NMEA-183)\
\f\nMenú GPS:\n1. Lista Posición\n2. Salida de datos con formato\n3. Los datos en bruto (NMEA-183)\
\f\nMenu de GPS:\n1. Lista Posição\n2. Saída de dados formatados\n3. Dados não tratados (NMEA-183)";
text T_ctrlz[]		= "\fQuit with <CTRL-Z>\fSalir con <CTRL-Z>\fSair com <CTRL-Z>";
text T_gga[]			= "\nSatellites,UTC,Latitude,N/S,Longitude,E/W,HDOP";
text T_tposfix[]	= "\fPosition fix time\fTiempo posición fix\fTempo de posição fixa";
text T_gpsanz[]		= "\fNumber of GPS positions\fNúmero de posiciones GPS\fNúmero de posições de GPS";
text T_betr[]			= "\fOperating hours\fHoras de funcionamiento\fHoras de funcionamento";
text T_sstr[]			= "\fSignal strength(0...31)\fIntensidad de señal(0...31)\fForça do sinal(0...31)";
text T_tzone[]		= "\fTime zone\fZona horaria\fFuso horário";
text T_conmqtt[]	= "\f\nConnect to MQTT server\f\nConectar al servidor MQTT\f\nConecte-se ao servidor MQTT";
text T_btdis[]		= "\f\nBluetooth disconnected\f\nConexión Bluetooth desconectada\f\nConexão Bluetooth desconectada";
text T_wait[]			= "\fPlease wait...\fEspere por favor...\fPor favor, espere...";
text T_gpscon[]		= "\f\nGPS positioning. \f\nPosicionamiento GPS. \f\nPosicionamento GPS. ";
text T_bereit[]		= "\fReady\fListo\f\nPreparar";
text T_auto[]			= "\fAutomatic\fAutomáticamente\fAutomaticamente";

// Ereignistexte in Landessprache (Protokollmeldungen), einsprachige Ereignistexte siehe sictxt.c
text T_poweron[]    = "\fPower on\fEncendido\fEm Operação";
text T_sommer[]     = "\fSummer time\fEl horario de verano\fHorário de verão";
text T_winter[]     = "\fWinter time\fHorario de invierno\fHorário de inverno";
text T_messstart[]  = "\fStart of measurement\fInicio de la medición\fInicío da medição";
text T_memfull[]    = "\fMemory full\fMemoria llena\fMemória Cheia";
text T_defpar[]     = "\fDefault parameter set\fParámetros por defecto\fParâmetros por defeito";
text T_parainit[]   = "\fParameter initialisation\fInicialización de parámetros\fInicialização de parâmetros";
text T_batlow[]     = "\fBattery voltage < 11,5V\fTensión de la batería < 11,5V\fTensão da bateria < 11,5V";
#endif
