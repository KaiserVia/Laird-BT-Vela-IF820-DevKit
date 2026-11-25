/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 - 2014 ARM Ltd.
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
 * $Date:        5. May 2014
 * $Revision:    V2.00
 *
 * Project:      USB Device Driver definitions for NXP LPC17xx
 * -------------------------------------------------------------------- */

#ifndef __USBD_LPC17XX_H
#define __USBD_LPC17XX_H

#include <stdint.h>

/* Device Interrupt Bit Definitions */
#define USBD_FRAME_INT          (1 << 0)
#define USBD_EP_FAST_INT        (1 << 1)
#define USBD_EP_SLOW_INT        (1 << 2)
#define USBD_DEV_STAT_INT       (1 << 3)
#define USBD_CCEMTY_INT         (1 << 4)
#define USBD_CDFULL_INT         (1 << 5)
#define USBD_RxENDPKT_INT       (1 << 6)
#define USBD_TxENDPKT_INT       (1 << 7)
#define USBD_EP_RLZED_INT       (1 << 8)
#define USBD_ERR_INT            (1 << 9)

/* Rx & Tx Packet Length Definitions */
#define USBD_PKT_LNGTH_MASK     (0x03FF)
#define USBD_PKT_DV             (1 << 10)
#define USBD_PKT_RDY            (1 << 11)

/* USB Control Definitions */
#define USBD_CTRL_RD_EN         (1 << 0)
#define USBD_CTRL_WR_EN         (1 << 1)

/* Command Codes */
#define USBD_CMD_SET_ADDR       (0x00D00500)
#define USBD_CMD_CFG_DEV        (0x00D80500)
#define USBD_CMD_SET_MODE       (0x00F30500)
#define USBD_CMD_RD_FRAME       (0x00F50500)
#define USBD_DAT_RD_FRAME       (0x00F50200)
#define USBD_CMD_RD_TEST        (0x00FD0500)
#define USBD_DAT_RD_TEST        (0x00FD0200)
#define USBD_CMD_SET_DEV_STAT   (0x00FE0500)
#define USBD_CMD_GET_DEV_STAT   (0x00FE0500)
#define USBD_DAT_GET_DEV_STAT   (0x00FE0200)
#define USBD_CMD_GET_ERR_CODE   (0x00FF0500)
#define USBD_DAT_GET_ERR_CODE   (0x00FF0200)
#define USBD_CMD_RD_ERR_STAT    (0x00FB0500)
#define USBD_DAT_RD_ERR_STAT    (0x00FB0200)
#define USBD_DAT_WR_BYTE(x)     (0x00000100 | ((x) << 16))
#define USBD_CMD_SEL_EP(x)      (0x00000500 | ((x) << 16))
#define USBD_DAT_SEL_EP(x)      (0x00000200 | ((x) << 16))
#define USBD_CMD_SEL_EP_CLRI(x) (0x00400500 | ((x) << 16))
#define USBD_DAT_SEL_EP_CLRI(x) (0x00400200 | ((x) << 16))
#define USBD_CMD_SET_EP_STAT(x) (0x00400500 | ((x) << 16))
#define USBD_CMD_CLR_BUF        (0x00F20500)
#define USBD_DAT_CLR_BUF        (0x00F20200)
#define USBD_CMD_VALID_BUF      (0x00FA0500)

/* Device Address Register Definitions */
#define USBD_DEV_ADDR_MASK      (0x7F)
#define USBD_DEV_EN             (0x80)

/* Device Configure Register Definitions */
#define USBD_CONF_DVICE         (1 << 0)

/* Device Mode Register Definitions */
#define USBD_AP_CLK             (1 << 0)
#define USBD_INAK_CI            (1 << 1)
#define USBD_INAK_CO            (1 << 2)
#define USBD_INAK_II            (1 << 3)
#define USBD_INAK_IO            (1 << 4)
#define USBD_INAK_BI            (1 << 5)
#define USBD_INAK_BO            (1 << 6)

/* Device Status Register Definitions */
#define USBD_DEV_CON            (1 << 0)
#define USBD_DEV_CON_CH         (1 << 1)
#define USBD_DEV_SUS            (1 << 2)
#define USBD_DEV_SUS_CH         (1 << 3)
#define USBD_DEV_RST            (1 << 4)

/* Error Code Register Definitions */
#define USBD_ERR_EC_MASK        (0x0F)
#define USBD_ERR_EA             (0x10)

/* Error Status Register Definitions */
#define USBD_ERR_PID            (1 << 0)
#define USBD_ERR_UEPKT          (1 << 1)
#define USBD_ERR_DCRC           (1 << 2)
#define USBD_ERR_TIMOUT         (1 << 3)
#define USBD_ERR_EOP            (1 << 4)
#define USBD_ERR_B_OVRN         (1 << 5)
#define USBD_ERR_BTSTF          (1 << 6)
#define USBD_ERR_TGL            (1 << 7)

/* Endpoint Select Register Definitions */
#define USBD_EP_SEL_F           (1 << 0)
#define USBD_EP_SEL_ST          (1 << 1)
#define USBD_EP_SEL_STP         (1 << 2)
#define USBD_EP_SEL_PO          (1 << 3)
#define USBD_EP_SEL_EPN         (1 << 4)
#define USBD_EP_SEL_B_1_FULL    (1 << 5)
#define USBD_EP_SEL_B_2_FULL    (1 << 6)

/* Endpoint Status Register Definitions */
#define USBD_EP_STAT_ST         (1 << 0)
#define USBD_EP_STAT_DA         (1 << 1)
#define USBD_EP_STAT_RF_MO      (1 << 2)
#define USBD_EP_STAT_CND_ST     (1 << 3)

/* Clear Buffer Register Definitions */
#define USBD_CLR_BUF_PO         (1 << 0)

/* DMA Interrupt Bit Definitions */
#define USBD_EOT_INT            (1 << 0)
#define USBD_NDD_REQ_INT        (1 << 1)
#define USBD_SYS_ERR_INT        (1 << 2)

// Endpoint information structure
typedef struct _endpoint_info_t {
  uint8_t   type;
  uint16_t  max_packet_size;
} endpoint_info_t;

// Transfer information structure
typedef struct _transfer_info_t {
  uint8_t   ep_addr;
  uint8_t  *data;
  uint32_t  num;
  uint32_t  transferred;
  uint8_t   active;
} transfer_info_t;

#endif /* __USBD_LPC17XX_H */
