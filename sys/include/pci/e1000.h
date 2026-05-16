/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PCI_E1000_H
#define _PCI_E1000_H

#include <sys/types.h>
#include <sys/bus.h>

/* PCI identification */
#define E1000_VENDOR_ID         0x8086u
#define E1000_DEVICE_82540EM   0x100Eu

/* MMIO register offsets (Intel 8254x Software Developer's Manual) */
#define E1000_REG_CTRL   0x0000u  /* Device Control */
#define E1000_REG_STATUS 0x0008u  /* Device Status */
#define E1000_REG_EERD   0x0014u  /* EEPROM Read */
#define E1000_REG_ICR    0x00C0u  /* Interrupt Cause Read (clears on read) */
#define E1000_REG_IMS    0x00D0u  /* Interrupt Mask Set/Read */
#define E1000_REG_IMC    0x00D8u  /* Interrupt Mask Clear */
#define E1000_REG_RCTL   0x0100u  /* Receive Control */
#define E1000_REG_TCTL   0x0400u  /* Transmit Control */
#define E1000_REG_TIPG   0x0410u  /* TX Inter-Packet Gap */
#define E1000_REG_RDBAL  0x2800u  /* RX Descriptor Base Low */
#define E1000_REG_RDBAH  0x2804u  /* RX Descriptor Base High */
#define E1000_REG_RDLEN  0x2808u  /* RX Descriptor Ring Length (bytes) */
#define E1000_REG_RDH    0x2810u  /* RX Descriptor Head */
#define E1000_REG_RDT    0x2818u  /* RX Descriptor Tail */
#define E1000_REG_TDBAL  0x3800u  /* TX Descriptor Base Low */
#define E1000_REG_TDBAH  0x3804u  /* TX Descriptor Base High */
#define E1000_REG_TDLEN  0x3808u  /* TX Descriptor Ring Length (bytes) */
#define E1000_REG_TDH    0x3810u  /* TX Descriptor Head */
#define E1000_REG_TDT    0x3818u  /* TX Descriptor Tail */
#define E1000_REG_TXDCTL 0x3828u  /* TX Descriptor Control */
#define E1000_REG_MTA    0x5200u  /* Multicast Table Array (128 x u32) */
#define E1000_REG_RAL0   0x5400u  /* Receive Address Low 0 (MAC bytes 0-3) */
#define E1000_REG_RAH0   0x5404u  /* Receive Address High 0 (MAC bytes 4-5 + AV) */

/* CTRL bits */
#define E1000_CTRL_FD    (1u << 0)   /* Full Duplex */
#define E1000_CTRL_LRST  (1u << 3)   /* Link Reset */
#define E1000_CTRL_ASDE  (1u << 5)   /* Auto-Speed Detection Enable */
#define E1000_CTRL_SLU   (1u << 6)   /* Set Link Up */
#define E1000_CTRL_ILOS  (1u << 7)   /* Invert Loss-of-Signal */
#define E1000_CTRL_RST   (1u << 26)  /* Device Reset */

/* ICR / IMS bits */
#define E1000_ICR_TXDW   (1u << 0)   /* TX Descriptor Written Back */
#define E1000_ICR_TXQE   (1u << 1)   /* TX Queue Empty */
#define E1000_ICR_LSC    (1u << 2)   /* Link Status Change */
#define E1000_ICR_RXO    (1u << 6)   /* RX Overrun */
#define E1000_ICR_RXT0   (1u << 7)   /* RX Timer Interrupt */

/* RCTL bits */
#define E1000_RCTL_EN    (1u << 1)   /* Receiver Enable */
#define E1000_RCTL_SBP   (1u << 2)   /* Store Bad Packets */
#define E1000_RCTL_UPE   (1u << 3)   /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE   (1u << 4)   /* Multicast Promiscuous Enable */
#define E1000_RCTL_BAM   (1u << 15)  /* Broadcast Accept Mode */
#define E1000_RCTL_SECRC (1u << 26)  /* Strip Ethernet CRC */
/* BSIZE bits [17:16] = 00 → 2048-byte buffers when BSEX=0 (default) */

/* TCTL bits */
#define E1000_TCTL_EN    (1u << 1)   /* Transmit Enable */
#define E1000_TCTL_PSP   (1u << 3)   /* Pad Short Packets */

/* TX descriptor command byte */
#define E1000_TXD_CMD_EOP  0x01u  /* End of Packet */
#define E1000_TXD_CMD_IFCS 0x02u  /* Insert FCS/CRC */
#define E1000_TXD_CMD_RS   0x08u  /* Report Status (set DD on completion) */

/* TX descriptor status byte */
#define E1000_TXD_STAT_DD  0x01u  /* Descriptor Done */

/* RX descriptor status byte */
#define E1000_RXD_STAT_DD  0x01u  /* Descriptor Done */
#define E1000_RXD_STAT_EOP 0x02u  /* End of Packet */

/* Ring sizes — must be a multiple of 8 */
#define E1000_NUM_RX_DESC  16u
#define E1000_NUM_TX_DESC  16u
#define E1000_BUF_SIZE     2048u

/* RX descriptor (16 bytes, must be 16-byte aligned) */
struct e1000_rx_desc {
	uint64_t addr;      /* Physical address of receive buffer */
	uint16_t length;
	uint16_t checksum;
	uint8_t  status;
	uint8_t  errors;
	uint16_t special;
} __attribute__((packed));

/* TX descriptor (16 bytes, must be 16-byte aligned) */
struct e1000_tx_desc {
	uint64_t addr;      /* Physical address of transmit buffer */
	uint16_t length;
	uint8_t  cso;       /* Checksum Offset */
	uint8_t  cmd;
	uint8_t  status;
	uint8_t  css;       /* Checksum Start Field */
	uint16_t special;
} __attribute__((packed));

/* Published state */
extern uint8_t         e1000_mac[6];
extern int             e1000_ready;
extern volatile int    e1000_irq_pending;

/* Public API */
int             initE1000(uint32_t bar0_phys, uint8_t irq);

/* newbus-lite driver registration — referenced by pci_drv_table[] in pci.c */
extern struct ubx_driver e1000_ubx_driver;
void            e1000_send_packet(const void *data, uint16_t len);
void            e1000_handle_irq(void);
void            e1000_thread(void);
void            e1000_isr(void);                            /* ASM stub */
const uint8_t  *e1000_get_rx_packet(uint16_t *out_len);    /* for netif bridge */

#endif /* _PCI_E1000_H */
