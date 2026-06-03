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

#ifndef _PCI_NE2K_H
#define _PCI_NE2K_H

#include <sys/types.h>
#include <sys/bus.h>

/*
 * NE2000 / RealTek RTL8029(AS) — National DP8390 "NIC" register set.
 *
 * The DP8390 core is identical across the classic ISA NE2000 cards of the
 * 1990s and the PCI RTL8029AS that QEMU emulates as `ne2k_pci`.  This driver
 * targets the PCI variant (probed via newbus like e1000/lnc); the hardware
 * core (ne2k_hw_init / _send / RX drain) takes an I/O base + IRQ, so an ISA
 * attach path for real vintage hardware (fixed base 0x300, IRQ 3/5/10/11) can
 * be layered on later without touching the register code.
 *
 * RTL8029AS PCI ID: vendor 0x10EC, device 0x8029.
 */
#define NE2K_VENDOR_REALTEK 0x10ECu
#define NE2K_DEVICE_RTL8029 0x8029u

/* ----------------------------------------------------------------------
 * DP8390 register offsets (relative to the I/O base).
 * Page 0 is the normal operating page; page 1 holds the station address
 * (PAR), multicast filter (MAR) and the CURRent receive page.
 * -------------------------------------------------------------------- */
#define NE_CMD 0x00    /* Command register (all pages)            */
#define NE_PSTART 0x01 /* P0W: receive ring start page            */
#define NE_PSTOP 0x02  /* P0W: receive ring stop page             */
#define NE_BNRY 0x03   /* P0RW: boundary page (last read by host) */
#define NE_TPSR 0x04   /* P0W: transmit page start                */
#define NE_TBCR0 0x05  /* P0W: transmit byte count low            */
#define NE_TBCR1 0x06  /* P0W: transmit byte count high           */
#define NE_ISR 0x07    /* P0RW: interrupt status                  */
#define NE_RSAR0 0x08  /* P0W: remote DMA start address low       */
#define NE_RSAR1 0x09  /* P0W: remote DMA start address high      */
#define NE_RBCR0 0x0A  /* P0W: remote DMA byte count low          */
#define NE_RBCR1 0x0B  /* P0W: remote DMA byte count high         */
#define NE_RCR 0x0C    /* P0W: receive configuration              */
#define NE_TCR 0x0D    /* P0W: transmit configuration             */
#define NE_DCR 0x0E    /* P0W: data configuration                 */
#define NE_IMR 0x0F    /* P0W: interrupt mask                     */

#define NE_PAR0 0x01 /* P1RW: station address byte 0 (..0x06)   */
#define NE_CURR 0x07 /* P1RW: current receive page              */
#define NE_MAR0 0x08 /* P1RW: multicast filter byte 0 (..0x0F)  */

#define NE_DATA 0x10  /* remote DMA data port                    */
#define NE_RESET 0x1F /* read+write-back to reset the card       */

/* Command register (NE_CMD) bits. */
#define CMD_STP 0x01 /* stop                                    */
#define CMD_STA 0x02 /* start                                   */
#define CMD_TXP 0x04 /* transmit packet                         */
#define CMD_RD0 0x08 /* remote DMA: read                        */
#define CMD_RD1 0x10 /* remote DMA: write                       */
#define CMD_RD2 0x20 /* remote DMA: abort/complete (no DMA)     */
#define CMD_PS0 0x40 /* register page 1                         */

/* Interrupt status / mask (NE_ISR / NE_IMR) bits. */
#define ISR_PRX 0x01 /* packet received OK                      */
#define ISR_PTX 0x02 /* packet transmitted OK                   */
#define ISR_RXE 0x04 /* receive error                           */
#define ISR_TXE 0x08 /* transmit error                          */
#define ISR_OVW 0x10 /* receive ring overwrite                  */
#define ISR_CNT 0x20 /* counter overflow                        */
#define ISR_RDC 0x40 /* remote DMA complete                     */
#define ISR_RST 0x80 /* reset / powered down                    */

/* On-card buffer-RAM page layout (16 KB at pages 0x40..0x80). */
#define NE_TX_START_PAGE 0x40 /* transmit buffer (6 pages = one 1518B frame) */
#define NE_RX_START_PAGE 0x46 /* receive ring start                          */
#define NE_RX_STOP_PAGE 0x80  /* receive ring stop (exclusive)               */

#define NE_PAGE_SIZE 256
#define NE_MAX_FRAME 1518 /* max Ethernet frame, no VLAN, no FCS         */

/* ----------------------------------------------------------------------
 * Published state
 * -------------------------------------------------------------------- */
extern u_int8_t ne2k_mac[6];
extern int ne2k_ready;
extern volatile int ne2k_irq_pending;

/* ----------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------- */
int ne2k_hw_init(u_int32_t iobase, u_int8_t irq);
void ne2k_send_packet(const void *data, u_int16_t len);
void ne2k_handle_irq(void);
void ne2k_thread(void);
const u_int8_t *ne2k_get_rx_packet(u_int16_t *out_len); /* for netif bridge */

/* newbus-lite driver registration — referenced by pci_drv_table[] in pci.c */
extern struct ubx_driver ne2k_ubx_driver;

#endif /* _PCI_NE2K_H */
