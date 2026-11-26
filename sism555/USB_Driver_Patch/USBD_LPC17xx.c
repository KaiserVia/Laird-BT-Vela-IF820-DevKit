/* -----------------------------------------------------------------------------
 * Copyright (c) 2013-2014 ARM Ltd.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *
 * $Date:        2. July 2014
 * $Revision:    V2.00
 *
 * Driver:       Driver_USBD0
 * Configured:   via RTE_Device.h configuration file
 * Project:      USB Device Driver for NXP LPC17xx
 * -----------------------------------------------------------------------------
 * Use the following configuration settings in the middleware component
 * to connect to this driver.
 *
 *   Configuration Setting                Value
 *   ---------------------                -----
 *   Connect to hardware via Driver_USBD# = 0
 * -------------------------------------------------------------------------- */

/* ALTERED SOURCE (viaTraffic, sism555 5.u17, 2026-11-25) - not the original Keil/ARM file:
 *  USBD_IRQ(): received OUT packets are read into a scratch buffer and only the
 *  remaining space of the active transfer is copied (nothing if the transfer
 *  buffer is NULL). The original wrote the whole packet to
 *  transfer_info.data+transferred without a length check; an 8-byte packet on
 *  EP0 OUT while a zero-length status stage (data == NULL) was armed wrote to
 *  address 0 -> imprecise bus fault -> HardFault -> watchdog reset. Seen when a
 *  PC closes/opens the virtual COM port while the device sends (approx. 5 % per
 *  close in the test).
 *  5.u21: USBD_ReadSetupPacket() releases usbd_sie_busy and the USB IRQ also when
 *  no SETUP packet is pending (original left both locked -> USB dead).
 *
 * History:
 *  Version 2.00
 *    - Initial release for CMSIS Drivers v2.01 API
 */

#include <stdint.h>
#include <string.h>

#include "LPC17xx.h"
#include "USBD_LPC17xx.h"

#include "Driver_USBD.h"

#include "RTE_Device.h"
#include "RTE_Components.h"

#define USBD_STATE_INITIALIZED     (1 << 0)
#define USBD_STATE_POWERED         (1 << 1)
#define USBD_STATE_CONNECTED       (1 << 2)

#if (RTE_USB_USB0 == 0)
#error "USB0 is not enabled in the RTE_Device.h!"
#endif

/* External Variables */
extern volatile uint32_t usb_role;

/* External Functions */
extern int32_t USB_PinsConfigure   (void);
extern int32_t USB_PinsUnconfigure (void);

static uint32_t usbd_state = 0;

// Function check_and_set_var tries to set variable (uint32_t) to 1 if it was interrupted or var was already 1 
// it returns false, if it succeeded it returns true
__inline static bool check_and_set_var (uint32_t *addr) {
  bool val;
  do {
    val = __LDREXW (addr);
    if (val & 1) {
      __CLREX (); 
      return false;
    }
  } while (__STREXW (1, addr));
  return true;
}


#define ARM_USBD_DRV_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(2,00)

// Driver Version
static const ARM_DRIVER_VERSION usbd_driver_version = { ARM_USBD_API_VERSION, ARM_USBD_DRV_VERSION };

// Driver Capabilities
static const ARM_USBD_CAPABILITIES usbd_driver_capabilities = {
  1,  // vbus_detection
  1,  // event_vbus_on
  1,  // event_vbus_off
};

// Number of Endpoints
#define USBD_EP_NUM      16             // Maximum is 16 (should come from RTE_Device.h)

static ARM_USBD_SignalDeviceEvent_t   cbDeviceEvent;
static ARM_USBD_SignalEndpointEvent_t cbEndpointEvent;
static ARM_USBD_STATE                 usbd_dev_state = {0, 0, 0};

static endpoint_info_t  endpoint_info[USBD_EP_NUM*2];
static transfer_info_t  transfer_info[USBD_EP_NUM*2];
static bool             usbd_int_active              = false;
static uint32_t         usbd_sie_busy                = 0;
static bool             usbd_ep0_setup_pending       = false;
static bool             usbd_ep0_out_zlp_pending     = false;


/**
  \fn          uint32_t EP_GetPhyIndex (uint32_t ep_addr)
  \brief       Get Endpoint Physical Index
  \param[in]   ep_addr:    Endpoint Address
                - ep.0..3: Address
                - ep.7:    Direction
  \return      Endpoint Physical Index
*/
static uint32_t EP_GetPhyIndex (uint32_t ep_addr) {
  return (((ep_addr & 0x0F) << 1) + ((ep_addr >> 7) & 1));
}

/**
  \fn          void SIE_WrCmd (uint32_t cmd)
  \brief       Write command to Serial Interface Engine (SIE)
  \param[in]   cmd     Command
*/
static void SIE_WrCmd (uint32_t cmd) {

  LPC_USB->USBDevIntClr = USBD_CCEMTY_INT;
  LPC_USB->USBCmdCode   = cmd;
  while ((LPC_USB->USBDevIntSt & USBD_CCEMTY_INT) == 0);
}

/**
  \fn          void SIE_WrCmdData (uint32_t cmd, uint32_t data)
  \brief       Write command data to Serial Interface Engine (SIE)
  \param[in]   cmd     Command
  \param[in]   data    Data
*/
static void SIE_WrCmdData (uint32_t cmd, uint32_t data) {

  LPC_USB->USBDevIntClr = USBD_CCEMTY_INT;
  LPC_USB->USBCmdCode   = cmd;
  while ((LPC_USB->USBDevIntSt & USBD_CCEMTY_INT) == 0);
  LPC_USB->USBDevIntClr = USBD_CCEMTY_INT;
  LPC_USB->USBCmdCode   = data;
  while ((LPC_USB->USBDevIntSt & USBD_CCEMTY_INT) == 0);
}

/**
  \fn          void SIE_EP_WrCmd (uint32_t ep_phy, uint32_t cmd)
  \brief       Write Endpoint command to Serial Interface Engine (SIE)
  \param[in]   ep_phy  Physical Endpoint index
  \param[in]   cmd     Command
*/
static void SIE_EP_WrCmd (uint32_t ep_phy, uint32_t cmd) {

  LPC_USB->USBDevIntClr = USBD_CCEMTY_INT;
  LPC_USB->USBCmdCode   = USBD_CMD_SEL_EP(ep_phy);
  while ((LPC_USB->USBDevIntSt & USBD_CCEMTY_INT) == 0);
  LPC_USB->USBDevIntClr = USBD_CCEMTY_INT;
  LPC_USB->USBCmdCode   = cmd;
}

/**
  \fn          uint32_t SIE_RdCmdData (uint32_t cmd)
  \brief       Read command data from Serial Interface Engine (SIE)
  \param[in]   cmd  Command
  \return      Read command data
*/
static uint32_t SIE_RdCmdData (uint32_t cmd) {
  uint32_t USBCmdData;

  LPC_USB->USBDevIntClr = USBD_CCEMTY_INT | USBD_CDFULL_INT;
  LPC_USB->USBCmdCode   = cmd;
  while ((LPC_USB->USBDevIntSt & USBD_CDFULL_INT) == 0);
  USBCmdData            = LPC_USB->USBCmdData; 

  return USBCmdData;
}

/**
  \fn          void USBD_Reset (void)
  \brief       Called after usbd reset interrupt to reset configuration
*/
static void USBD_Reset (void) {

  // Disable Endpoint interrupts and clear all interrupts
  LPC_USB->USBEpIntEn   = 0;
  LPC_USB->USBEpIntClr  = 0xFFFFFFFF;
  LPC_USB->USBDevIntClr = 0xFFFFFFFF;

  // Clear all endpoint and transfer information
  memset(endpoint_info, 0, sizeof(endpoint_info));
  memset(transfer_info, 0, sizeof(transfer_info));
  usbd_ep0_setup_pending   = false;
  usbd_ep0_out_zlp_pending = false;
}

/**
  \fn          void USBD_ConfigEP (uint32_t ep_phy, uint32_t ep_max_packet_size)
  \brief       Configure Endpoint
  \param[in]   ep_phy  Physical Endpoint index
  \param[in]   ep_max_packet_size Endpoint Maximum Packet Size
*/
static void USBD_ConfigEP (uint32_t ep_phy, uint32_t ep_max_packet_size) {

  LPC_USB->USBEpInd    = ep_phy;
  LPC_USB->USBMaxPSize = ep_max_packet_size;
  while ((LPC_USB->USBDevIntSt & USBD_EP_RLZED_INT) == 0);
  LPC_USB->USBDevIntClr = USBD_EP_RLZED_INT;
}

/**
  \fn          void USBD_EnableEP (uint32_t ep_phy)
  \brief       Enable Endpoint
  \param[in]   ep_phy Physical Endpoint index
*/
static void USBD_EnableEP (uint32_t ep_phy) {

  LPC_USB->USBReEp    |= 1 << ep_phy;   // Realize endpoint
  while ((LPC_USB->USBDevIntSt & USBD_EP_RLZED_INT) == 0);
  LPC_USB->USBDevIntClr = USBD_EP_RLZED_INT;

  LPC_USB->USBEpIntEn |= 1 << ep_phy;   // Enable Endpoint interrupts
}

/**
  \fn          USBD_DisableEP (uint32_t ep_phy)
  \brief       Disable Endpoint
  \param[in]   ep_phy  Physical Endpoint index
*/
static void USBD_DisableEP (uint32_t ep_phy) {

  LPC_USB->USBEpIntEn &= ~(1 << ep_phy);  // Disable Endpoint interrupts

  LPC_USB->USBReEp    &= ~(1 << ep_phy);  // De-realize endpoint
  while ((LPC_USB->USBDevIntSt & USBD_EP_RLZED_INT) == 0);
  LPC_USB->USBDevIntClr = USBD_EP_RLZED_INT;
}

/**
  \fn          uint32_t USBD_ReadEP (uint32_t ep_log,
                                     uint8_t *data)
  \brief       Read data received on Endpoint
  \param[in]   ep_log Logical Endpoint
  \param[out]  data   Pointer to buffer for data to read
  \return      Number of bytes read
*/
static uint32_t usbd_rx_scratch[64/4];   // viaTraffic 5.u17: OUT packet buffer (max. packet 64, no ISO)

static uint32_t USBD_ReadEP (uint32_t ep_log, uint8_t *data) {
  uint32_t ep_phy;
  uint32_t num, i, val;

  ep_phy = EP_GetPhyIndex(ep_log);

  LPC_USB->USBCtrl = ((ep_log  & 0x0F) << 2) | USBD_CTRL_RD_EN;

  do {
    num = LPC_USB->USBRxPLen;
  } while ((num & USBD_PKT_RDY) == 0);
  num &= USBD_PKT_LNGTH_MASK;

  for (i = 0; (i + 4) <= num; i += 4) {
    *((__packed uint32_t *)data) = LPC_USB->USBRxData;
    data += 4;
  }
  if (i < num) {
    val = LPC_USB->USBRxData;
    for (; i < num; i++) {
      *(data++) = val;
      val >>= 8;
    }
  }

  LPC_USB->USBCtrl = 0;

  if (endpoint_info[ep_phy].type != ARM_USB_ENDPOINT_ISOCHRONOUS) {
    // Non-Isochronous Endpoint
    SIE_EP_WrCmd(ep_phy, USBD_CMD_CLR_BUF);
  }

  return num;
}

/**
  \fn          uint32_t USBD_ReceiveOnInactiveEP (uint32_t ep_log)
  \brief       Handle data received on Inactive Endpoint
  \param[in]   ep_log Logical Endpoint
*/
static void USBD_ReceiveOnInactiveEP (uint32_t ep_log) {
  uint32_t ep_phy;
  uint32_t num;

  ep_phy = EP_GetPhyIndex(ep_log);

  LPC_USB->USBCtrl = ((ep_log  & 0x0F) << 2) | USBD_CTRL_RD_EN;

  do {
    num = LPC_USB->USBRxPLen;
  } while ((num & USBD_PKT_RDY) == 0);
  num &= USBD_PKT_LNGTH_MASK;

  if ((ep_phy == 0) && (num == 0)) {
    usbd_ep0_out_zlp_pending = true;

    LPC_USB->USBCtrl = 0;

    SIE_EP_WrCmd(ep_phy, USBD_CMD_CLR_BUF);
  } else {
    LPC_USB->USBCtrl = 0;
  }
}

/**
  \fn          uint32_t USBD_WriteEP (uint32_t ep_log,
                                      uint8_t *data,
                                      uint32_t num)
  \brief       Write data to be transferred on Endpoint
  \param[in]   ep_log Logical Endpoint
  \param[out]  data   Pointer to buffer with data to write
  \param[in]   num    Number of data bytes to transfer
  \return      Number of bytes written
*/
static uint32_t USBD_WriteEP (uint32_t ep_log, uint8_t *data, uint32_t num) {
  uint32_t ep_phy;
  uint32_t i;

  ep_phy = EP_GetPhyIndex(ep_log | 0x80);

  if (num > endpoint_info[ep_phy].max_packet_size)
    num = endpoint_info[ep_phy].max_packet_size;

  LPC_USB->USBCtrl = ((ep_log & 0x0F) << 2) | USBD_CTRL_WR_EN;

  LPC_USB->USBTxPLen = num;

  for (i = 0; i < (num + 3) / 4; i++) {
    LPC_USB->USBTxData = *((__packed uint32_t *)data);
    data += 4;
  }

  LPC_USB->USBCtrl = 0;

  SIE_EP_WrCmd(ep_phy, USBD_CMD_VALID_BUF);

  return num;
}


/**
  \fn          ARM_DRIVER_VERSION USBD_GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
static ARM_DRIVER_VERSION USBD_GetVersion (void) { return usbd_driver_version; }

/**
  \fn          ARM_USBD_CAPABILITIES USBD_GetCapabilities (void)
  \brief       Get driver capabilities.
  \return      \ref ARM_USBD_CAPABILITIES
*/
static ARM_USBD_CAPABILITIES USBD_GetCapabilities (void) { return usbd_driver_capabilities; }

/**
  \fn          int32_t USBD_Initialize (ARM_USBD_SignalDeviceEvent_t   cb_device_event,
                                        ARM_USBD_SignalEndpointEvent_t cb_endpoint_event)
  \brief       Initialize USB Device Interface.
  \param[in]   cb_device_event    Pointer to \ref ARM_USBD_SignalDeviceEvent
  \param[in]   cb_endpoint_event  Pointer to \ref ARM_USBD_SignalEndpointEvent
  \return      \ref execution_status
*/
static int32_t USBD_Initialize (ARM_USBD_SignalDeviceEvent_t   cb_device_event,
                                ARM_USBD_SignalEndpointEvent_t cb_endpoint_event) {
  int32_t ret_val;

  if (usbd_state & USBD_STATE_POWERED)     return ARM_DRIVER_ERROR;
  if (usbd_state & USBD_STATE_INITIALIZED) return ARM_DRIVER_OK;

  // Initialize USBD Run-time Resources
  cbDeviceEvent   = cb_device_event;
  cbEndpointEvent = cb_endpoint_event;

  usb_role = ARM_USB_ROLE_DEVICE;

  // Setup pin functions
  ret_val = USB_PinsConfigure ();
  if (ret_val == -1) {
    usb_role = ARM_USB_ROLE_NONE;
    return ARM_DRIVER_ERROR;
  }

  usbd_state = USBD_STATE_INITIALIZED;

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_Uninitialize (void)
  \brief       De-initialize USB Device Interface.
  \return      \ref execution_status
*/
static int32_t USBD_Uninitialize (void) {

  if (  usbd_state & USBD_STATE_POWERED)      return ARM_DRIVER_ERROR;
  if (!(usbd_state & USBD_STATE_INITIALIZED)) return ARM_DRIVER_OK;

  // Reset pin functions
  if (USB_PinsUnconfigure () == -1) return ARM_DRIVER_ERROR;

  usb_role   = ARM_USB_ROLE_NONE;
  usbd_state = 0;

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_PowerControl (ARM_POWER_STATE state)
  \brief       Control USB Device Interface Power.
  \param[in]   state  Power state
  \return      \ref execution_status
*/
static int32_t USBD_PowerControl (ARM_POWER_STATE state) {

  if (!(usbd_state & USBD_STATE_INITIALIZED)) return ARM_DRIVER_ERROR;

  switch (state) {
    case ARM_POWER_OFF:
      if (!(usbd_state & USBD_STATE_POWERED)) return ARM_DRIVER_OK;

      NVIC_DisableIRQ(USB_IRQn);        // Disable USB interrupt

      usbd_state &= ~USBD_STATE_POWERED;

      LPC_USB->USBDevIntEn  = 0;

      LPC_USB->USBClkCtrl = 0;          // Device and AHB clock disable
      while (LPC_USB->USBClkSt & 0x1A);

      LPC_SC->PCONP &= ~(1UL << 31);    // USB PCLK -> disable USB Peripheral
      break;

    case ARM_POWER_FULL:
      if (  usbd_state & USBD_STATE_POWERED ) return ARM_DRIVER_OK;

      LPC_SC->PCONP |=  (1UL << 31);    // USB PCLK -> enable USB Peripheral

      LPC_USB->USBClkCtrl = 0x1A;       // Device and AHB clock enable
      while ((LPC_USB->USBClkSt & 0x1A) != 0x1A);

      LPC_USB->USBDevIntEn  = USBD_DEV_STAT_INT | USBD_EP_SLOW_INT;

      usbd_state |=  USBD_STATE_POWERED;

      NVIC_ClearPendingIRQ (USB_IRQn);  // Clear pending USB interrupt
      NVIC_EnableIRQ(USB_IRQn);         // Enable USB interrupt
      break;

    default:
      return ARM_DRIVER_ERROR_UNSUPPORTED;
  }
  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_DeviceConnect (void)
  \brief       Connect USB Device.
  \return      \ref execution_status
*/
static int32_t USBD_DeviceConnect (void) {

  if (!(usbd_state & USBD_STATE_POWERED))  return ARM_DRIVER_ERROR;
  if (  usbd_state & USBD_STATE_CONNECTED) return ARM_DRIVER_OK;

  if (!check_and_set_var(&usbd_sie_busy)) return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  SIE_WrCmdData(USBD_CMD_SET_DEV_STAT, USBD_DAT_WR_BYTE(USBD_DEV_CON));

  usbd_sie_busy  = 0;
  usbd_state    |= USBD_STATE_CONNECTED;

  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);
  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_DeviceDisconnect (void)
  \brief       Disconnect USB Device.
  \return      \ref execution_status
*/
static int32_t USBD_DeviceDisconnect (void) {

  if (!(usbd_state & USBD_STATE_POWERED))   return ARM_DRIVER_ERROR;
  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_OK;

  if (!check_and_set_var(&usbd_sie_busy)) return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  SIE_WrCmdData(USBD_CMD_SET_DEV_STAT, USBD_DAT_WR_BYTE(0));

  usbd_sie_busy = 0;
  usbd_state   &= ~USBD_STATE_CONNECTED;

  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          ARM_USBD_STATE USBD_DeviceGetState (void)
  \brief       Get current USB Device State.
  \return      Device State \ref ARM_USBD_STATE
*/
static ARM_USBD_STATE USBD_DeviceGetState (void) {

  return usbd_dev_state;
}

/**
  \fn          int32_t USBD_DeviceRemoteWakeup (void)
  \brief       Trigger USB Remote Wakeup.
  \return      \ref execution_status
*/
static int32_t USBD_DeviceRemoteWakeup (void) {

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  USBD_PowerControl (ARM_POWER_FULL);

  SIE_WrCmdData(USBD_CMD_SET_DEV_STAT, USBD_DAT_WR_BYTE(SIE_RdCmdData(USBD_CMD_GET_DEV_STAT) & (~USBD_DEV_SUS)));

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_DeviceSetAddress (uint8_t dev_addr)
  \brief       Set USB Device Address.
  \param[in]   dev_addr  Device Address
  \return      \ref execution_status
*/
static int32_t USBD_DeviceSetAddress (uint8_t dev_addr) {

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  SIE_WrCmdData(USBD_CMD_SET_ADDR, USBD_DAT_WR_BYTE(USBD_DEV_EN | dev_addr));
  SIE_WrCmdData(USBD_CMD_SET_ADDR, USBD_DAT_WR_BYTE(USBD_DEV_EN | dev_addr));

  // Set device configured
  SIE_WrCmdData(USBD_CMD_CFG_DEV,  USBD_DAT_WR_BYTE(USBD_CONF_DVICE));

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_ReadSetupPacket (uint8_t *setup)
  \brief       Read setup packet received over Control Endpoint.
  \param[out]  setup  Pointer to buffer for setup packet
  \return      \ref execution_status
*/
int32_t USBD_ReadSetupPacket (uint8_t *setup) {

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  NVIC_DisableIRQ(USB_IRQn);

  if (!usbd_ep0_setup_pending) {        // if setup packet not received
    // viaTraffic 5.u21: original returned here without releasing usbd_sie_busy and the USB IRQ ->
    // every later driver call returned BUSY, USB dead until reset
    usbd_sie_busy = 0;
    NVIC_EnableIRQ(USB_IRQn);
    return ARM_DRIVER_ERROR;
  }

  usbd_ep0_setup_pending = false;
  USBD_ReadEP(0, setup);

  usbd_sie_busy = 0;
  NVIC_EnableIRQ(USB_IRQn);

  if (usbd_ep0_setup_pending)           // if new setup packet received during read
    return ARM_DRIVER_ERROR;

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_EndpointConfigure (uint8_t  ep_addr,
                                               uint8_t  ep_type,
                                               uint16_t ep_max_packet_size)
  \brief       Configure USB Endpoint.
  \param[in]   ep_addr  Endpoint Address
                - ep_addr.0..3: Address
                - ep_addr.7:    Direction
  \param[in]   ep_type  Endpoint Type (ARM_USB_ENDPOINT_xxx)
  \param[in]   ep_max_packet_size Endpoint Maximum Packet Size
  \return      \ref execution_status
*/
static int32_t USBD_EndpointConfigure (uint8_t  ep_addr,
                                       uint8_t  ep_type,
                                       uint16_t ep_max_packet_size) {
  uint32_t ep_phy;

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  ep_phy = EP_GetPhyIndex(ep_addr);

  // Store Endpoint settings in memory
  endpoint_info[ep_phy].type            = ep_type;
  endpoint_info[ep_phy].max_packet_size = ep_max_packet_size;

  USBD_ConfigEP(ep_phy, ep_max_packet_size);
  SIE_WrCmdData (USBD_CMD_SET_EP_STAT(ep_phy), USBD_DAT_WR_BYTE(0));
  USBD_EnableEP(ep_phy);

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_EndpointUnconfigure (uint8_t ep_addr)
  \brief       Unconfigure USB Endpoint.
  \param[in]   ep_addr  Endpoint Address
                - ep_addr.0..3: Address
                - ep_addr.7:    Direction
  \return      \ref execution_status
*/
static int32_t USBD_EndpointUnconfigure (uint8_t ep_addr) {
  uint32_t ep_phy;

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  ep_phy = EP_GetPhyIndex(ep_addr);

  USBD_DisableEP(ep_phy);

  // Clear Endpoint settings in memory
  memset(&endpoint_info[ep_phy], 0, sizeof(endpoint_info_t));

  // Clear transfer settings in memory
  memset(&transfer_info[ep_phy], 0, sizeof(transfer_info_t));

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_EndpointStall (uint8_t ep_addr, bool stall)
  \brief       Set/Clear Stall for USB Endpoint.
  \param[in]   ep_addr  Endpoint Address
                - ep_addr.0..3: Address
                - ep_addr.7:    Direction
  \param[in]   stall  Operation
                - \b false Clear
                - \b true Set
  \return      \ref execution_status
*/
static int32_t USBD_EndpointStall (uint8_t ep_addr, bool stall) {
  uint32_t ep_phy;

  ep_phy = EP_GetPhyIndex(ep_addr);

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  if (stall) {
    SIE_WrCmdData(USBD_CMD_SET_EP_STAT(ep_phy), USBD_DAT_WR_BYTE(USBD_EP_STAT_ST));
  } else {
    SIE_WrCmdData(USBD_CMD_SET_EP_STAT(ep_phy), USBD_DAT_WR_BYTE(0));
  }

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USBD_EndpointTransfer (uint8_t ep_addr, uint8_t *data, uint32_t num)
  \brief       Read data from or Write data to USB Endpoint.
  \param[in]   ep_addr  Endpoint Address
                - ep_addr.0..3: Address
                - ep_addr.7:    Direction
  \param[out]  data Pointer to buffer for data to read or with data to write
  \param[in]   num  Number of data bytes to transfer
  \return      \ref execution_status
*/
static int32_t USBD_EndpointTransfer (uint8_t ep_addr, uint8_t *data, uint32_t num) {
  uint32_t ep_phy, ep_msk;

  ep_phy = EP_GetPhyIndex(ep_addr);

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (transfer_info[ep_phy].active)         return ARM_DRIVER_ERROR_BUSY;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  ep_msk = 1 << ep_phy;

  // Store transfer settings in memory
  transfer_info[ep_phy].ep_addr        = ep_addr;
  transfer_info[ep_phy].data           = data;
  transfer_info[ep_phy].num            = num;
  transfer_info[ep_phy].transferred    = 0;

  if (ep_addr & 0x80) {                                               // for IN Endpoint
    transfer_info[ep_phy].active       = 1;
    transfer_info[ep_phy].transferred += USBD_WriteEP(ep_phy >> 1, transfer_info[ep_phy].data, transfer_info[ep_phy].num);
  } else if ((!ep_addr) && (!num) && (usbd_ep0_out_zlp_pending)) {    // for EP0 OUT ZLP Endpoint already pending
    usbd_ep0_out_zlp_pending = false;
    cbEndpointEvent(0, ARM_USBD_EVENT_OUT);
  } else {                                                            // for OUT Endpoint
    transfer_info[ep_phy].active       = 1;
    LPC_USB->USBEpIntSet = ep_msk;                                     // force interrupt to handle received data
  }

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          uint32_t USBD_EndpointTransferGetResult (uint8_t ep_addr)
  \brief       Get result of USB Endpoint transfer.
  \param[in]   ep_addr  Endpoint Address
                - ep_addr.0..3: Address
                - ep_addr.7:    Direction
  \return      number of successfully transfered data bytes
*/
static uint32_t USBD_EndpointTransferGetResult (uint8_t ep_addr) {

  if (!(usbd_state & USBD_STATE_CONNECTED)) return 0;

  return (transfer_info[EP_GetPhyIndex(ep_addr)].transferred);
}

/**
  \fn          int32_t USBD_EndpointTransferAbort (uint8_t ep_addr)
  \brief       Abort current USB Endpoint transfer.
  \param[in]   ep_addr  Endpoint Address
                - ep_addr.0..3: Address
                - ep_addr.7:    Direction
  \return      \ref execution_status
*/
static int32_t USBD_EndpointTransferAbort (uint8_t ep_addr) {
  uint32_t ep_phy;

  if (!(usbd_state & USBD_STATE_CONNECTED)) return ARM_DRIVER_ERROR;
  if (!check_and_set_var(&usbd_sie_busy))   return ARM_DRIVER_ERROR_BUSY;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  ep_phy = EP_GetPhyIndex(ep_addr);

  // Clear transfer settings in memory
  memset(&transfer_info[ep_phy], 0, sizeof(transfer_info_t));
  if (ep_phy == 0) {
    usbd_ep0_setup_pending   = false;
    usbd_ep0_out_zlp_pending = false;
  }

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return ARM_DRIVER_OK;
}

/**
  \fn          uint16_t USBD_GetFrameNumber (void)
  \brief       Get current USB Frame Number.
  \return      Frame Number
*/
static uint16_t USBD_GetFrameNumber (void) {
  uint16_t val;

  if (!(usbd_state & USBD_STATE_CONNECTED)) return 0;
  if (!check_and_set_var(&usbd_sie_busy))   return 0;
  if (!usbd_int_active) NVIC_DisableIRQ(USB_IRQn);

  SIE_WrCmd(USBD_CMD_RD_FRAME);
  val  = SIE_RdCmdData(USBD_DAT_RD_FRAME);
  val |= SIE_RdCmdData(USBD_DAT_RD_FRAME) << 8;

  usbd_sie_busy = 0;
  if (!usbd_int_active) NVIC_EnableIRQ(USB_IRQn);

  return val;
}

/**
  \fn          void USBD_IRQ (void)
  \brief       USB Device Interrupt Routine (IRQ).
*/
void USBD_IRQ (void) {
  uint32_t dev_isr, ep_isr, ep_msk, ep_log, ep_phy, val, len;
  uint32_t evt_ep  = 0;
  uint8_t  evt_stp = 0;
  uint8_t  cnt     = 0;

  usbd_int_active = true;

  dev_isr = LPC_USB->USBDevIntSt & LPC_USB->USBDevIntEn;    // Device Int Mask Stat

  // Device Status Interrupt (Reset, Connect change, Suspend/Resume)
  if (dev_isr & USBD_DEV_STAT_INT) {
    LPC_USB->USBDevIntClr = USBD_DEV_STAT_INT;
    SIE_WrCmd(USBD_CMD_GET_DEV_STAT);
    val = SIE_RdCmdData(USBD_DAT_GET_DEV_STAT);   // Get device status
    if (val & USBD_DEV_CON_CH) {          // Connect change
      if (val & USBD_DEV_CON) {           // Connect
        usbd_dev_state.vbus   = true;
        usbd_dev_state.speed  = ARM_USB_SPEED_FULL;
        usbd_dev_state.active = true;
        cbDeviceEvent(ARM_USBD_EVENT_VBUS_ON);
      } else {                            // Disconnect
        usbd_dev_state.vbus   = false;
        usbd_dev_state.speed  = 0;
        usbd_dev_state.active = false;
        cbDeviceEvent(ARM_USBD_EVENT_VBUS_OFF);
      }
    }
    if (val & USBD_DEV_SUS_CH) {          // Suspend/Resume
      if (val & USBD_DEV_SUS) {           // Suspend
        usbd_dev_state.active = false;
        cbDeviceEvent(ARM_USBD_EVENT_SUSPEND);
      } else {                            // Resume
        usbd_dev_state.active = true;
        cbDeviceEvent(ARM_USBD_EVENT_RESUME);
      }
    }
    if (val & USBD_DEV_RST) {             // Device Reset
      USBD_Reset();
      cbDeviceEvent(ARM_USBD_EVENT_RESET);
    }

    goto isr_end;
  }

  // Endpoint's Slow Interrupt
  if (dev_isr & USBD_EP_SLOW_INT) {

    LPC_USB->USBDevIntClr = USBD_EP_SLOW_INT;

    ep_isr = LPC_USB->USBEpIntSt;
    for (ep_phy = 0; ep_isr; ep_phy++) {
      ep_msk = 1 << ep_phy;
      if (ep_isr & ep_msk) {
        ep_log = ep_phy >> 1;
        LPC_USB->USBEpIntClr = ep_msk;
        while ((LPC_USB->USBDevIntSt & USBD_CDFULL_INT) == 0);
        val = LPC_USB->USBCmdData;
        ep_isr &= ~ep_msk;
        if ((ep_phy & 1) == 0) {        // OUT Endpoint
          if (val & USBD_EP_SEL_STP) {
            usbd_ep0_setup_pending   = true;
            usbd_ep0_out_zlp_pending = false;
            evt_stp = 1;                                    // SETUP event
          } else if (transfer_info[ep_phy].active) {        // if transfer is active
            cnt = ((val >> 5) & 1) + ((val >> 6) & 1);      // number of full buffers
            if (cnt) {
              while (cnt--) {                               // while received data
                // viaTraffic 5.u17: never write more than the remaining transfer space, nothing to a NULL buffer
                uint32_t rem = transfer_info[ep_phy].num - transfer_info[ep_phy].transferred;
                uint32_t cp;
                len = USBD_ReadEP(ep_log, (uint8_t *)usbd_rx_scratch);
                cp  = (len < rem) ? len : rem;
                if (transfer_info[ep_phy].data == NULL) cp = 0U;
                if (cp != 0U) memcpy(transfer_info[ep_phy].data+transfer_info[ep_phy].transferred, usbd_rx_scratch, cp);
                transfer_info[ep_phy].transferred += cp;
                if ((transfer_info[ep_phy].transferred == transfer_info[ep_phy].num) ||       // if all data received or
                    (len < endpoint_info[ep_phy].max_packet_size)) {                          // if short packet received
                  transfer_info[ep_phy].active = 0;
                  evt_ep |= (1 << ep_phy);                  // OUT event
                  break;
                }
              }
            }
          } else {                                          // if transfer not yet active
            // read OUT ZLP if it was received on Control 0 Endpoint
            USBD_ReceiveOnInactiveEP(ep_log);
          }
        } else {                        // IN Endpoint
          if (transfer_info[ep_phy].active) {               // if transfer is active
            if (transfer_info[ep_phy].transferred == transfer_info[ep_phy].num) {
              transfer_info[ep_phy].active = 0;
              evt_ep |= (1 << ep_phy);                      // IN event
            } else {
              // Write data to send
              transfer_info[ep_phy].transferred += USBD_WriteEP(ep_log, transfer_info[ep_phy].data + transfer_info[ep_phy].transferred, transfer_info[ep_phy].num - transfer_info[ep_phy].transferred);
            }
          }
        }
      }
    }
  }

  for (ep_phy = 0; ep_phy < 32; ep_phy += 2) {
    if (evt_ep & (1 << (ep_phy+1))) {   // IN Endpoint event
      cbEndpointEvent(transfer_info[ep_phy+1].ep_addr, ARM_USBD_EVENT_IN);
    }
    if (evt_ep & (1 << ep_phy)) {       // OUT Endpoint event
      cbEndpointEvent(transfer_info[ep_phy].ep_addr, ARM_USBD_EVENT_OUT);
    }
  }
  if (evt_stp) cbEndpointEvent(0, ARM_USBD_EVENT_SETUP);

isr_end:
  usbd_int_active = false;
}

ARM_DRIVER_USBD Driver_USBD0 = {
  USBD_GetVersion,
  USBD_GetCapabilities,
  USBD_Initialize,
  USBD_Uninitialize,
  USBD_PowerControl,
  USBD_DeviceConnect,
  USBD_DeviceDisconnect,
  USBD_DeviceGetState,
  USBD_DeviceRemoteWakeup,
  USBD_DeviceSetAddress,
  USBD_ReadSetupPacket,
  USBD_EndpointConfigure,
  USBD_EndpointUnconfigure,
  USBD_EndpointStall,
  USBD_EndpointTransfer,
  USBD_EndpointTransferGetResult,
  USBD_EndpointTransferAbort,
  USBD_GetFrameNumber
};
