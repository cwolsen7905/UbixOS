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

/*
 * Intel 8254x (82540EM) Gigabit Ethernet driver for UbixOS.
 *
 * Supports QEMU's default e1000 model (-device e1000).
 * Interrupt-driven RX via PIC; synchronous TX with descriptor polling.
 * Bridges to lwIP via e1000netif.c.
 */

#include <pci/e1000.h>
#include <pci/pci.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/types.h>
#include <isa/8259.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <vmm/paging.h>
#include <vmm/vmm.h>
#include <ubixos/sched.h>
#include <net/netif.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Driver state
 * --------------------------------------------------------------------- */

int          e1000_ready      = 0;
volatile int e1000_irq_pending = 0;
uint8_t      e1000_mac[6];

/* Kernel-virtual base of the 128 KB MMIO region */
static volatile uint8_t *e1000_mmio = NULL;

/* Descriptor rings — 16-byte aligned via padding */
static uint8_t rx_desc_mem[E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc) + 16];
static uint8_t tx_desc_mem[E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc) + 16];

static struct e1000_rx_desc *rx_descs;
static struct e1000_tx_desc *tx_descs;

/* Per-descriptor packet buffers — virtual pointers */
static uint8_t *rx_bufs[E1000_NUM_RX_DESC];
static uint8_t *tx_bufs[E1000_NUM_TX_DESC];

static uint32_t rx_tail;  /* next descriptor to check for received packet */
static uint32_t tx_tail;  /* next free TX descriptor */

/* Scratch buffer handed to the lwIP bridge on each RX */
static uint8_t e1000_rx_packet[E1000_BUF_SIZE];
static uint16_t e1000_rx_len;

/* lwIP netif handle (defined in e1000netif.c) */
extern struct netif e1000_netif;

/* -----------------------------------------------------------------------
 * Register accessors
 * --------------------------------------------------------------------- */

static inline uint32_t e1000_read(uint32_t reg) {
	return *(volatile uint32_t *)(e1000_mmio + reg);
}

static inline void e1000_write(uint32_t reg, uint32_t val) {
	*(volatile uint32_t *)(e1000_mmio + reg) = val;
}

/* Spin until a register bit is set or a limit is reached */
static int e1000_wait_bit(uint32_t reg, uint32_t mask, int set, int limit) {
	int i;
	for (i = 0; i < limit; i++) {
		uint32_t v = e1000_read(reg);
		if (set  && (v & mask)) return 0;
		if (!set && !(v & mask)) return 0;
	}
	return -1;
}

/* -----------------------------------------------------------------------
 * MMIO mapping
 * --------------------------------------------------------------------- */

/*
 * Map 128 KB of physical MMIO space into kernel virtual address space.
 * We allocate kernel virtual pages and remap them to the physical BAR.
 * PAGE_CACHE_DISABLED is critical — MMIO must not be cached.
 */
static int e1000_map_mmio(uint32_t phys) {
	uint32_t pages = 32; /* 128 KB / 4 KB */
	uint32_t virt;
	uint32_t i;

	void *vbase = vmm_getFreeKernelPage(sysID, pages);
	if (!vbase) {
		kprintf("e1000: cannot allocate kernel virtual pages for MMIO\n");
		return -1;
	}
	virt = (uint32_t)vbase;

	for (i = 0; i < pages; i++) {
		if (vmm_remapPage(phys + i * 0x1000, virt + i * 0x1000,
		    KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED, sysID, 0) == 0) {
			kprintf("e1000: vmm_remapPage failed at page %u\n", i);
			return -1;
		}
	}
	e1000_mmio = (volatile uint8_t *)virt;
	return 0;
}

/* -----------------------------------------------------------------------
 * Descriptor ring setup
 * --------------------------------------------------------------------- */

static int e1000_init_rx(void) {
	uint32_t i;

	/* Align descriptor array to 16 bytes */
	rx_descs = (struct e1000_rx_desc *)
	    (((uint32_t)rx_desc_mem + 15u) & ~15u);

	for (i = 0; i < E1000_NUM_RX_DESC; i++) {
		rx_bufs[i] = kmalloc(E1000_BUF_SIZE);
		if (!rx_bufs[i]) {
			kprintf("e1000: cannot allocate RX buffer %u\n", i);
			return -1;
		}
		rx_descs[i].addr   = (uint64_t)vmm_getRealAddr((uint32_t)rx_bufs[i]);
		rx_descs[i].status = 0;
	}
	rx_tail = 0;

	uint32_t rdba = vmm_getRealAddr((uint32_t)rx_descs);
	e1000_write(E1000_REG_RDBAL, rdba);
	e1000_write(E1000_REG_RDBAH, 0);
	e1000_write(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
	e1000_write(E1000_REG_RDH, 0);
	e1000_write(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);

	e1000_write(E1000_REG_RCTL,
	    E1000_RCTL_EN  |
	    E1000_RCTL_SBP |
	    E1000_RCTL_UPE |
	    E1000_RCTL_MPE |
	    E1000_RCTL_BAM |
	    E1000_RCTL_SECRC);

	return 0;
}

static int e1000_init_tx(void) {
	uint32_t i;

	tx_descs = (struct e1000_tx_desc *)
	    (((uint32_t)tx_desc_mem + 15u) & ~15u);

	for (i = 0; i < E1000_NUM_TX_DESC; i++) {
		tx_bufs[i] = kmalloc(E1000_BUF_SIZE);
		if (!tx_bufs[i]) {
			kprintf("e1000: cannot allocate TX buffer %u\n", i);
			return -1;
		}
		tx_descs[i].addr   = (uint64_t)vmm_getRealAddr((uint32_t)tx_bufs[i]);
		tx_descs[i].status = E1000_TXD_STAT_DD; /* mark all as done initially */
	}
	tx_tail = 0;

	uint32_t tdba = vmm_getRealAddr((uint32_t)tx_descs);
	e1000_write(E1000_REG_TDBAL, tdba);
	e1000_write(E1000_REG_TDBAH, 0);
	e1000_write(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
	e1000_write(E1000_REG_TDH, 0);
	e1000_write(E1000_REG_TDT, 0);

	/* Recommended TIPG for 802.3 (from Intel manual) */
	e1000_write(E1000_REG_TIPG, 0x0060200Au);

	e1000_write(E1000_REG_TCTL,
	    E1000_TCTL_EN |
	    E1000_TCTL_PSP |
	    (0x0F << 4)  |   /* CT: collision threshold */
	    (0x3F << 12));   /* COLD: collision distance (full duplex) */

	return 0;
}

/* -----------------------------------------------------------------------
 * Transmit
 * --------------------------------------------------------------------- */

void e1000_send_packet(const void *data, uint16_t len) {
	uint32_t tail = tx_tail;
	int i;

	if (len > E1000_BUF_SIZE) {
		kprintf("e1000: TX packet too large (%u)\n", len);
		return;
	}

	/* Wait for the descriptor to be free (DD set by hardware) */
	for (i = 0; i < 10000; i++) {
		if (tx_descs[tail].status & E1000_TXD_STAT_DD)
			break;
	}
	if (!(tx_descs[tail].status & E1000_TXD_STAT_DD)) {
		kprintf("e1000: TX timeout, dropping packet\n");
		return;
	}

	memcpy(tx_bufs[tail], data, len);

	tx_descs[tail].length = len;
	tx_descs[tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
	tx_descs[tail].status = 0;

	tx_tail = (tail + 1) % E1000_NUM_TX_DESC;
	e1000_write(E1000_REG_TDT, tx_tail);
}

/* -----------------------------------------------------------------------
 * Receive (called from e1000_thread)
 * --------------------------------------------------------------------- */

/*
 * Process all ready RX descriptors.  For each complete packet, copy into
 * the shared e1000_rx_packet buffer and call the lwIP bridge function.
 * Returns number of packets processed.
 */
static int e1000_rx_process(void) {
	int count = 0;

	while (rx_descs[rx_tail].status & E1000_RXD_STAT_DD) {
		if (rx_descs[rx_tail].status & E1000_RXD_STAT_EOP) {
			uint16_t len = rx_descs[rx_tail].length;
			if (len <= E1000_BUF_SIZE) {
				memcpy(e1000_rx_packet, rx_bufs[rx_tail], len);
				e1000_rx_len = len;
				/* Notify lwIP bridge — defined in e1000netif.c */
				extern void e1000netif_input(struct netif *);
				e1000netif_input(&e1000_netif);
			}
		}

		/* Return descriptor to hardware */
		rx_descs[rx_tail].status = 0;
		e1000_write(E1000_REG_RDT, rx_tail);
		rx_tail = (rx_tail + 1) % E1000_NUM_RX_DESC;
		count++;
	}
	return count;
}

/* -----------------------------------------------------------------------
 * IRQ handler (called from ISR stub)
 * --------------------------------------------------------------------- */

void e1000_handle_irq(void) {
	uint32_t icr = e1000_read(E1000_REG_ICR); /* reading clears ICR */

	if (icr & E1000_ICR_LSC) {
		uint32_t status = e1000_read(E1000_REG_STATUS);
		kprintf("e1000: link %s\n", (status & 0x02) ? "up" : "down");
	}
	if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXO)) {
		e1000_irq_pending = 1;
	}
}

/* -----------------------------------------------------------------------
 * RX thread — wakes on IRQ flag and drains the ring
 * --------------------------------------------------------------------- */

void e1000_thread(void) {
	while (1) {
		if (e1000_irq_pending) {
			e1000_irq_pending = 0;
			e1000_rx_process();
		}
		sched_yield();
	}
}

/* -----------------------------------------------------------------------
 * ISR stub (saves/restores full register state, sends EOI)
 * --------------------------------------------------------------------- */

asm(
	".globl e1000_isr         \n"
	"e1000_isr:               \n"
	"  pusha                  \n"
	"  push %ss               \n"
	"  push %ds               \n"
	"  push %es               \n"
	"  push %fs               \n"
	"  push %gs               \n"
	"  call e1000_handle_irq  \n"
	/* EOI to slave PIC (IRQ 11 is on slave) */
	"  mov $0xA0,%dx          \n"
	"  mov $0x20,%al          \n"
	"  outb %al,%dx           \n"
	/* EOI to master PIC (cascade line IRQ 2) */
	"  mov $0x20,%dx          \n"
	"  outb %al,%dx           \n"
	"  pop %gs                \n"
	"  pop %fs                \n"
	"  pop %es                \n"
	"  pop %ds                \n"
	"  pop %ss                \n"
	"  popa                   \n"
	"  iret                   \n"
);

/* -----------------------------------------------------------------------
 * Initialization entry point (called from pci_init / init.h)
 * --------------------------------------------------------------------- */

int initE1000(uint32_t bar0_phys, uint8_t irq) {
	uint32_t ral, rah;
	uint8_t  vec;
	int      i;

	kprintf("e1000: initializing 82540EM at BAR0=0x%X IRQ=%u\n", bar0_phys, irq);

	if (e1000_map_mmio(bar0_phys) != 0)
		return -1;

	/* Device reset */
	e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_RST);
	for (i = 0; i < 10000; i++) {
		if (!(e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST))
			break;
	}
	if (e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST) {
		kprintf("e1000: reset timeout\n");
		return -1;
	}

	/* Disable all interrupts while we configure */
	e1000_write(E1000_REG_IMC, 0xFFFFFFFFu);

	/* Set link up, auto-speed detection */
	e1000_write(E1000_REG_CTRL,
	    e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE);

	/* Clear multicast table */
	for (i = 0; i < 128; i++)
		e1000_write(E1000_REG_MTA + i * 4, 0);

	/* Read MAC from RAL0/RAH0 */
	ral = e1000_read(E1000_REG_RAL0);
	rah = e1000_read(E1000_REG_RAH0);

	e1000_mac[0] = (ral >>  0) & 0xFF;
	e1000_mac[1] = (ral >>  8) & 0xFF;
	e1000_mac[2] = (ral >> 16) & 0xFF;
	e1000_mac[3] = (ral >> 24) & 0xFF;
	e1000_mac[4] = (rah >>  0) & 0xFF;
	e1000_mac[5] = (rah >>  8) & 0xFF;

	kprintf("e1000: MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
	    e1000_mac[0], e1000_mac[1], e1000_mac[2],
	    e1000_mac[3], e1000_mac[4], e1000_mac[5]);

	if (e1000_init_rx() != 0) return -1;
	if (e1000_init_tx() != 0) return -1;

	/* Wire IRQ */
	vec = (irq >= 8) ? (sVec + (irq - 8)) : (mVec + irq);
	setVector(&e1000_isr, vec, dInt + dPresent + dDpl3);
	irqEnable(irq);

	/* Enable RX timer and link change interrupts */
	e1000_write(E1000_REG_IMS, E1000_ICR_RXT0 | E1000_ICR_RXO | E1000_ICR_LSC);

	e1000_ready = 1;
	kprintf("e1000: ready\n");
	return 0;
}

/* -----------------------------------------------------------------------
 * Accessors for the lwIP bridge
 * --------------------------------------------------------------------- */

const uint8_t *e1000_get_rx_packet(uint16_t *out_len) {
	*out_len = e1000_rx_len;
	return e1000_rx_packet;
}
