			//Bluetooth Initialisieren ! -> wird vom PC aufgerufen
		case 'Y': //Konfig Bluetooth 	(Laird Bluetooth Data Module 730-SA)
#if SteuerLP_4f_d2    //Hardware Mai 2013 -> damit aendert sich die Initialisierung (andere Pinbelegung, zusaetzlich Bluetooth)
			hop = Bluetooth_Connnect; //UrZustand merken
			Bluetooth_Connnect = 1; //damit wird printf auf das BT ausgegeben

			//Ablauf: Standart sind 9600 Baud, daher erst mal damit senden und dann mit 115200
			//falls schon 115200 drin waren sollte das kein Problem sein, denn es werden nur einige Fehlermeldungen produziert
			//9600 Baud:	

			//	!!!   Umstellung auf 9600 Baud, denn das ist default für BT !!!
			unsigned int bsel = 12; // Set Baud Rate fuer 32MHz!  9600 Baud normal mode, mit 0,2% Fehler
			unsigned int bscale = 4; //

			//Bluetooth:  USART E0 mit 9600 Baud:
			USARTE0.BAUDCTRLB = ((unsigned char) (bscale & 0x0F) << 4) | ((unsigned char) (bsel >> 8) & 0x0F);
			USARTE0.BAUDCTRLA = (unsigned char) (bsel & 0xFF);
			/*			//das ist schon eingetragen in der init:
			USARTE0.CTRLB = (USART_RXEN_bm | USART_TXEN_bm); //= 0x18;  Enable Rx und Tx
			USARTE0.CTRLC = 0x03; //8N1
			USARTE0.CTRLA |= USART_RXCINTLVL_HI_gc; //Receive Complete Interrupt Level
			 */
			printf("AT\r\n");		//AT, danach definierter Zustand (ok, oder Fehlermeldung)
			_delay_ms(100); 
			printf("AT&F3\r\n");	//AT & F3 //Reset = factory default: F3 == Minimum power consumption, UART baud rate set to 115200, Left and Right LED off
			_delay_ms(1000); 

			//jetzt weiter mit 115200 Baud: 
			bsel = 131; // Set Baud Rate fuer 32MHz!  115200baud normal mode, mit 0,01% Fehler
			bscale = -3; //bei -4 -> bsel= 262 @ 32 MHz / 115200 baud
			USARTE0.BAUDCTRLB = ((unsigned char) (bscale & 0x0F) << 4) | ((unsigned char) (bsel >> 8) & 0x0F);
			USARTE0.BAUDCTRLA = (unsigned char) (bsel & 0xFF);

			printf("AT\r\n");		//AT, danach definierter Zustand (ok, oder Fehlermeldung)
			_delay_ms(100); 

			printf("AT&F3\r\n");	//AT & F3 //Reset = factory default: F3 == Minimum power consumption, UART baud rate set to 115200, Left and Right LED off
			_delay_ms(1000);

			printf("ATE0\r\n");		//ATE0			//Disable echo  		//ein 'OK' muss zurück kommen!
			_delay_ms(300); 

			printf("AT+BTN=\"viasignBT_\"\r\n"); //AT+BTN="viasignBT"	//Set Friendly Name in Non-volatile Memory  
			_delay_ms(300); 

			printf("AT+BTK=\"1234\"\r\n"); //set the pin code (0-8 Zeichen)
			_delay_ms(300); 

			printf("ATS514=60\r\n"); //ATS514=60		//Pairing Timeout in seconds. This includes the time a host takes to supply the PIN number when PIN? messages are indicated
			_delay_ms(300);

			printf("ATS502=0\r\n"); //ATS502=1 		//default = 0  Authentication for incoming connections. Set to 1 to enable authentication
			_delay_ms(300);

			printf("ATS0=-1\r\n");	//ATS0=-1 		//default = 1  auto answer in x sec -> Number of RING indications before automatically answering an incoming connection. A value of 0 disables autoanswer. 
			_delay_ms(300);

			printf("ATS536=1\r\n"); //ATS536=1 		//der PC kann jetzt das BluetoothModul konfigurieren !!! -> Enter Remote Command Mode
			_delay_ms(300); 

			printf("ATS420=1\r\n"); //ATS420=1 		//default = 0  Sekundenzähler (wird bei jeden Reset neu gestartet) Use ATI420 to read the count value. It is basically the time the module has been powered up in seconds.
			_delay_ms(300); 


			printf("ATS538=1\r\n"); //ATS538=1  	//default =0!  If 1, then when a successful pairing occurs, it is automatically saved in the trusted device database, if the database has room to store it !
			_delay_ms(300); 

			/*	//reduce the trusted device database to one record:
			printf("ATS592=1\r\n"); //ATS592=1  	//default = 0  (0,1) Set this to 1 to reduce the trusted device database to one record when autosaving of pairing is enabled via S Reg 538.
			_delay_ms(300); 
			 */

			//printf("AT+BTD*\r\n"); //AT+BTD*{Remove All Trusted Devices}
			//_delay_ms(300); 

			printf("ATS593=1\r\n"); //ATS593=1 		//default = 0  (0,1) 	Automatically append last six digits of local Bluetooth address to the friendly name which was set via AT+BTN or AT+BTF.
			_delay_ms(300); 

			printf("ATS512=4\r\n");	//ATS512=4 	//default = 1  (0...7) Specify power up state: default: 1=Proceeds to a state as if AT+BTO was entered.  4=Connectable and discoverable (like AT+BTP)
			_delay_ms(300); 

			printf("AT&W\r\n");		//AT&W			//Einstellungen speichern
			_delay_ms(300); 

			printf("ATZ\r\n");		//ATZ				//Software Reset -> jetzt werden die Einstellungen gültig
			_delay_ms(2000); 

			printf("AT+BTN?\r\n");	//AT+BTN?		->	Read Friendly Name from Non-volatile Memory
			_delay_ms(300);

			printf("ati4\r\n");		//ati4  		-> 	BluetoothAdresse ausgeben (0016A4067613)
			_delay_ms(300); 

			printf("ati0\r\n");		//ati / ati0	-> 	(Laird Bluetooth Data Module 730-SA)
			_delay_ms(300); 
			Bluetooth_Connnect = hop; //UrZustand herstellen
#endif
			break;
