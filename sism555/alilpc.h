/* Header Datei für LPC1766 Aliase 						*/
/* Die Aliase verlinken die Registernotation im			*/
/* Handbuch mit den CPU structures im Keil lpc17xx.h	*/

#include "lpc17xx.h"


#ifndef ALILPC_H_
#define ALILPC_H_

// Syntax Aliase für compiler
#define		ein				1
#define		aus				0
#define		TRUE			1
#define		FALSE			0
#define		uchar			unsigned char
#define		ushort		unsigned short
#define		uint			unsigned int  			  
#define		ulong			unsigned long
#define 	text			const char
#ifndef 	bool
#define		bool			unsigned char
#endif	
#define 	__at(_addr) 	__attribute__ ((at(_addr)))
#define 	__noinit			__attribute__ ((section("non_init"),zero_init))

// Aliase Code Read Protection Level
#define		CRP1			0x12345678
#define		CRP2			0x87654321
#define 	CRP3			0x43218765
#define		CRPNONE		0xFFFFFFFF
#define		CRP_key		(LPC_FLASH_BASE + 0x02FC)

// Aliase System Control
#define PCON			LPC_SC->PCON
#define PCONP			LPC_SC->PCONP
#define EXTMODE		LPC_SC->EXTMODE
#define EXTPOLAR	LPC_SC->EXTPOLAR
#define EXTINT		LPC_SC->EXTINT
#define RSID			LPC_SC->RSID
#define	SCS				LPC_SC->SCS
#define	PLL0CON  	LPC_SC->PLL0CON
#define	PLL0FEED	LPC_SC->PLL0FEED
#define	PLL0STAT	LPC_SC->PLL0STAT
#define	CLKSRCSEL	LPC_SC->CLKSRCSEL
#define	CCLKCFG		LPC_SC->CCLKCFG


// Bitflags PCONP - Power Control Register Peripheral
#define	PCTIM0		(1<<1)
#define	PCTIM1		(1<<2)
#define	PCUART0		(1<<3)
#define	PCUART1		(1<<4)
#define	PCPWM1		(1<<6)
#define PCI2C0		(1<<7)
#define	PCRTC			(1<<9)
#define PCSSP1		(1<<10)
#define PCADC			(1<<12)
#define	PCRIT			(1<<16)
#define	PCTIM2		(1<<22)
#define	PCTIM3		(1<<23)
#define	PCUART2		(1<<24)

// Bitflags RSID - Reset Source Identifikation Register
#define		POR	  (1<<0)			// Power On Reset Bit
#define		EXTR	(1<<1)			// Externer = Leiterplatten Reset
#define		WDTR	(1<<2)			// Watchdog Timer Reset Bit
#define		BODR	(1<<3)			// Brownout Detection Bit
#define		SYSR	(1<<4)			// Warmstart System Reset Bit

// Aliase Watchdog Timer
#define		WDMOD			LPC_WDT->WDMOD
#define		WDTC			LPC_WDT->WDTC
#define		WDFEED		LPC_WDT->WDFEED
#define		WDTV			LPC_WDT->WDTV
#define		WDCLKSEL	LPC_WDT->WDCLKSEL
// Bitflags WDT Register WDMOD
#define		WDEN		(1<<0)
#define		WDRESET	(1<<1)

// Aliase RIT Timer
#define	RICOMPVAL		LPC_RIT->RICOMPVAL
#define	RIMASK			LPC_RIT->RIMASK
#define	RICTRL			LPC_RIT->RICTRL
#define	RICOUNTER		LPC_RIT->RICOUNTER

// Bitflags RIT Control Register
#define RITINT		(1<<0)
#define RITENCLR	(1<<1)
#define	RITENBR		(1<<2)
#define	RITEN			(1<<3)

// Aliase GPIO Portkonfiguration - Pin connect 
#define PINSEL0		LPC_PINCON->PINSEL0
#define PINSEL1		LPC_PINCON->PINSEL1
#define PINSEL2		LPC_PINCON->PINSEL2
#define PINSEL3		LPC_PINCON->PINSEL3
#define PINSEL4		LPC_PINCON->PINSEL4
#define PINSEL7		LPC_PINCON->PINSEL7
#define PINSEL8		LPC_PINCON->PINSEL8
#define PINSEL9		LPC_PINCON->PINSEL9
#define PINSEL10	LPC_PINCON->PINSEL10

#define PINMODE0   	LPC_PINCON->PINMODE0
#define PINMODE1   	LPC_PINCON->PINMODE1
#define PINMODE2   	LPC_PINCON->PINMODE2
#define PINMODE3   	LPC_PINCON->PINMODE3
#define PINMODE4   	LPC_PINCON->PINMODE4
#define PINMODE5   	LPC_PINCON->PINMODE5
#define PINMODE6   	LPC_PINCON->PINMODE6
#define PINMODE7   	LPC_PINCON->PINMODE7
#define PINMODE8   	LPC_PINCON->PINMODE8
#define PINMODE9   	LPC_PINCON->PINMODE9

#define PINMODE_OD0	LPC_PINCON->PINMODE_OD0
#define PINMODE_OD1	LPC_PINCON->PINMODE_OD1
#define PINMODE_OD2	LPC_PINCON->PINMODE_OD2
#define PINMODE_OD3	LPC_PINCON->PINMODE_OD3
#define PINMODE_OD4	LPC_PINCON->PINMODE_OD4

// Aliase GPIO Ports
#define FIO0DIR		LPC_GPIO0->FIODIR
#define FIO1DIR		LPC_GPIO1->FIODIR
#define FIO2DIR		LPC_GPIO2->FIODIR
#define FIO3DIR		LPC_GPIO3->FIODIR
#define FIO4DIR		LPC_GPIO4->FIODIR

#define FIO0MASK	LPC_GPIO0->FIOMASK
#define FIO1MASK	LPC_GPIO1->FIOMASK
#define FIO2MASK	LPC_GPIO2->FIOMASK
#define FIO3MASK	LPC_GPIO3->FIOMASK
#define FIO4MASK	LPC_GPIO4->FIOMASK

#define FIO0PIN		LPC_GPIO0->FIOPIN
#define FIO1PIN		LPC_GPIO1->FIOPIN
#define FIO2PIN		LPC_GPIO2->FIOPIN
#define FIO3PIN		LPC_GPIO3->FIOPIN
#define FIO4PIN		LPC_GPIO4->FIOPIN

#define FIO0SET		LPC_GPIO0->FIOSET
#define FIO1SET		LPC_GPIO1->FIOSET
#define FIO2SET		LPC_GPIO2->FIOSET
#define FIO3SET		LPC_GPIO3->FIOSET
#define FIO4SET		LPC_GPIO4->FIOSET

#define FIO0CLR		LPC_GPIO0->FIOCLR
#define FIO1CLR		LPC_GPIO1->FIOCLR
#define FIO2CLR		LPC_GPIO2->FIOCLR
#define FIO3CLR		LPC_GPIO3->FIOCLR
#define FIO4CLR		LPC_GPIO4->FIOCLR

// Syntax Aliase UARTS
#define U0		LPC_UART0
#define U1		LPC_UART1
#define U2		LPC_UART2
#define U3		LPC_UART3

// Bitflags UART Register
#define		RDR			(1<<0)			// Receiver data ready Flag in UxLSR
#define		THRE		(1<<5)			// Transmitter Holding Register Empty Flag
#define		TEMT		(1<<6)			// Transmitter FIFO Empty Flag
#define		THRE_INT_EN	(1<<1)	// Transmitter Holding Register Empty Interrupt Enable

// Syntax Aliase SSP
#define	SSP0	LPC_SSP0
#define	SSP1	LPC_SSP1

// Status Bits SPI/SSP Interface
#define 	TFE			(1<<0)	// Transmit FIFO empty
#define 	TNF			(1<<1)	// Transmit FIFO not full
#define 	RNE			(1<<2)	// Receive FIFO not empty
#define 	RFF			(1<<3)	// Receive FIFO full
#define 	BSY			(1<<4)	// SPI busy

// Syntax Aliase RTC
#define		RCCR		LPC_RTC->CCR
#define		ILR			LPC_RTC->ILR
#define 	CIIR		LPC_RTC->CIIR
#define		SEC			LPC_RTC->SEC
#define		MIN			LPC_RTC->MIN
#define		HOUR		LPC_RTC->HOUR
#define		DOM			LPC_RTC->DOM
#define		DOW			LPC_RTC->DOW
#define		MONTH		LPC_RTC->MONTH
#define		YEAR		LPC_RTC->YEAR
#define 	CTIME0	LPC_RTC->CTIME0
#define 	CTIME1	LPC_RTC->CTIME1
#define		GPREG0	LPC_RTC->GPREG0
#define		GPREG1	LPC_RTC->GPREG1
#define		GPREG2	LPC_RTC->GPREG2
#define		GPREG3	LPC_RTC->GPREG3
#define		GPREG4	LPC_RTC->GPREG4

// Bitflags RTC CCR Register
#define		CLKEN		(1<<0)	// RTC Clock enable
#define		CTCRST	(1<<1)	// Reset and stop 1 Hz Oszillator
#define		CCALEN	(1<<4)	// Calibration counter en-/disable=0/1

// Bitflags RTC CIIR Register 
#define		IMSEC		(1<<0)		// Sekundenwechsel generiert Interrupt
#define		IMMIN		(1<<1)		// Minutenwechsel generiert Interrupt

// Aliase Timer
#define		T0CR		LPC_TIM0->TCR	// Timer control register
#define		T1CR		LPC_TIM1->TCR
#define		T2CR		LPC_TIM2->TCR
#define		T3CR		LPC_TIM3->TCR

#define		T0CTCR	LPC_TIM0->CTCR	// Timer count control register
#define		T1CTCR	LPC_TIM1->CTCR
#define		T2CTCR	LPC_TIM2->CTCR
#define		T3CTCR	LPC_TIM3->CTCR

#define		T0TC		LPC_TIM0->TC	// Timer counter register
#define		T1TC		LPC_TIM1->TC	
#define		T2TC		LPC_TIM2->TC	
#define		T3TC		LPC_TIM3->TC	

#define		T0PR		LPC_TIM0->PR	// Prescale register
#define		T1PR		LPC_TIM1->PR
#define		T2PR		LPC_TIM2->PR
#define		T3PR		LPC_TIM3->PR

#define		T0MCR		LPC_TIM0->MCR	// Match control register
#define		T1MCR		LPC_TIM1->MCR
#define		T2MCR		LPC_TIM2->MCR
#define		T3MCR		LPC_TIM3->MCR

#define		T0IR		LPC_TIM0->IR	// Interrupt register
#define		T1IR		LPC_TIM1->IR
#define		T2IR		LPC_TIM2->IR
#define		T3IR		LPC_TIM3->IR

#define		T0MR0		LPC_TIM0->MR0	// Timer 0 match register
#define		T0MR1		LPC_TIM0->MR1
#define		T0MR2		LPC_TIM0->MR2
#define		T0MR3		LPC_TIM0->MR3

#define		T1MR0		LPC_TIM1->MR0	// Timer 1 match register
#define		T1MR1		LPC_TIM1->MR1
#define		T1MR2		LPC_TIM1->MR2
#define		T1MR3		LPC_TIM1->MR3

#define		T2MR0		LPC_TIM2->MR0	// Timer 2 match register
#define		T2MR1		LPC_TIM2->MR1
#define		T2MR2		LPC_TIM2->MR2
#define		T2MR3		LPC_TIM2->MR3

#define		T3MR0		LPC_TIM3->MR0	// Timer 3 match register
#define		T3MR1		LPC_TIM3->MR1
#define		T3MR2		LPC_TIM3->MR2
#define		T3MR3		LPC_TIM3->MR3

#define		T0CCR		LPC_TIM0->CCR	// Timer capture control register
#define		T1CCR		LPC_TIM1->CCR
#define		T2CCR		LPC_TIM2->CCR
#define		T3CCR		LPC_TIM3->CCR

#define		T0CR0		LPC_TIM0->CR0 // Timer 0 capture register
#define		T0CR1		LPC_TIM0->CR1

#define		T1CR0		LPC_TIM1->CR0 // Timer 1 capture register
#define		T1CR1		LPC_TIM1->CR1

#define		T2CR0		LPC_TIM2->CR0 // Timer 2 capture register
#define		T2CR1		LPC_TIM2->CR1

#define		T3CR0		LPC_TIM3->CR0 // Timer 3 capture register
#define		T3CR1		LPC_TIM3->CR1

// Bitflags Capture Control Register
#define		CAP0RE	(1<<0)				// falling edge capture 0
#define		CAP0FE	(1<<1)				// rising edge capture 0
#define		CAP0I		(1<<2)				// interrupt on capture 0
#define		CAP1RE	(1<<3)				// falling edge capture 1
#define		CAP1FE	(1<<4)				// rising edge capture 1
#define		CAP1I		(1<<5)				// interrupt on capture 1


// Aliase ADC - A/D-Wandler Register
#define		ADCR		LPC_ADC->ADCR
#define		ADGDR		LPC_ADC->ADGDR
#define		ADSTAT	LPC_ADC->ADSTAT
#define		ADDR0		LPC_ADC->ADDR0

#define		PDN			(1<<21)			// 0 for power down	
#define 	STRTNOW	(1<<24)			// 001 start conversion now at ADCR bit 26:24 	

// Aliase PWM1 Timer
#define		PWMPR		LPC_PWM1->PR
#define		PWMMCR	LPC_PWM1->MCR
#define		PWMIR		LPC_PWM1->IR
#define		PWMPCR	LPC_PWM1->PCR
#define		PWMMR0	LPC_PWM1->MR0
#define		PWMMR3	LPC_PWM1->MR3
#define		PWMMR4	LPC_PWM1->MR4
#define		PWMMR5	LPC_PWM1->MR5
#define		PWMMR6	LPC_PWM1->MR6
#define		PWMTCR	LPC_PWM1->TCR
#define		PWMLER	LPC_PWM1->LER
#define		PWMTC		LPC_PWM1->TC

// Bitflags PWM
#define		PWMSEL5	(1<<5)
#define		PWMSEL6	(1<<6)
#define		PWMENA3	(1<<11)
#define		PWMENA5	(1<<13)
#define		PWMENA6	(1<<14)
#define		PWMMR0R	(1<<1)
#define		PWMMR0I	(1<<0)

// Register Cortex M3 Core
#define		SCB_AIRCR			(*((volatile unsigned int *)(0xE000ED0C)))	// Application Interrupt and Reset Control Register
#define		SCB_SCR				(*((volatile unsigned int *)(0xE000ED10)))	// System control register
#endif /* ALILPC_H_ */
