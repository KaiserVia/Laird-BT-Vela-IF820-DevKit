
/*
 * Auto generated Run-Time-Environment Configuration File
 *      *** Do not modify ! ***
 *
 * Project: 'sism' 
 * Target:  'sism' 
 */

#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H


/*
 * Define the Device Header File: 
 */
#define CMSIS_device_header "LPC17xx.h"

/*  ARM::CMSIS:RTOS:Keil RTX:4.75.0 */
#define RTE_CMSIS_RTOS                  /* CMSIS-RTOS */
        #define RTE_CMSIS_RTOS_RTX              /* CMSIS-RTOS Keil RTX */
/*  Keil.MDK-Pro::File System:CORE:LFN:6.2.0 */
#define RTE_FileSystem_Core             /* File System Core */
          #define RTE_FileSystem_LFN              /* File System with Long Filename support */
/*  Keil.MDK-Pro::File System:Drive:USB:6.2.0 */
#define RTE_FileSystem_Drive_USB_0      /* File System USB Drive 0 */

/*  Keil.MDK-Pro::USB:CORE:6.2.0 */
#define RTE_USB_Core                    /* USB Core */
/*  Keil.MDK-Pro::USB:Device:6.2.0 */
#define RTE_USB_Device_0                /* USB Device 0 */

/*  Keil.MDK-Pro::USB:Device:CDC:6.2.0 */
#define RTE_USB_Device_CDC_0            /* USB Device CDC instance 0 */

/*  Keil.MDK-Pro::USB:Host:6.2.0 */
#define RTE_USB_Host_0                  /* USB Host 0 */

/*  Keil.MDK-Pro::USB:Host:MSC:6.2.0 */
#define RTE_USB_Host_MSC                /* USB Host MSC */
/*  Keil::CMSIS Driver:I2C:2.01 */
#define RTE_Drivers_I2C0                /* Driver I2C0 */
        #define RTE_Drivers_I2C1                /* Driver I2C1 */
        #define RTE_Drivers_I2C2                /* Driver I2C2 */
/*  Keil::CMSIS Driver:USB Device:2.00 */
#define RTE_Drivers_USBD0               /* Driver USBD0 */
/*  Keil::CMSIS Driver:USB Host:2.00 */
#define RTE_Drivers_USBH0               /* Driver USBH0 */
/*  Keil::Device:Startup:1.0.0 */
#define RTE_DEVICE_STARTUP_LPC17XX      /* Device Startup for NXP17XX */


#endif /* RTE_COMPONENTS_H */
