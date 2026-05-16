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
#include <sys/bus.h>
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

/* Descriptor rings — identity-mapped pages (virtual == physical).
 * vmm_findFreePage + vmm_remapIOPage guarantees this so TDBAL/RDBAL
 * can be set directly to the pointer value with no vmm_getRealAddr call. */
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
 * Identity-map the 128 KB MMIO BAR into kernel virtual space.
 * vmm_remapIOPage maps phys->phys and silently overwrites existing PTEs,
 * avoiding the panic that vmm_remapPage emits when a page is already present
 * (vmm_getFreeKernelPage pre-populates its range with real RAM pages).
 * PAGE_CACHE_DISABLED is required — MMIO must never be cached.
 */
static int e1000_map_mmio(uint32_t phys) {
	uint32_t pages = 32; /* 128 KB / 4 KB */
	uint32_t i;

	for (i = 0; i < pages; i++)
		vmm_remapIOPage(phys + i * 0x1000,
		    KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED, sysID);

	e1000_mmio = (volatile uint8_t *)phys;
	return 0;
}

/* -----------------------------------------------------------------------
 * Descriptor ring setup
 * --------------------------------------------------------------------- */

static int e1000_init_rx(void) {
	uint32_t i, phys;

	/* Identity-map descriptor ring: virtual == physical, so RDBAL == pointer. */
	phys = vmm_findFreePage(sysID);
	if (!phys) {
		kprintf("e1000: cannot allocate RX descriptor ring\n");
		return -1;
	}
	vmm_remapIOPage(phys, KERNEL_PAGE_DEFAULT, sysID);
	rx_descs = (struct e1000_rx_desc *)phys;
	memset(rx_descs, 0, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));

	for (i = 0; i < E1000_NUM_RX_DESC; i++) {
		uint32_t buf_phys = vmm_findFreePage(sysID);
		if (!buf_phys) {
			kprintf("e1000: cannot allocate RX buffer %u\n", i);
			return -1;
		}
		vmm_remapIOPage(buf_phys, KERNEL_PAGE_DEFAULT, sysID);
		rx_bufs[i] = (uint8_t *)buf_phys;
		rx_descs[i].addr   = (uint64_t)buf_phys; /* physical == virtual */
		rx_descs[i].status = 0;
	}
	rx_tail = 0;

	kprintf("e1000: RX ring virt=phys=0x%X buf[0]=0x%X\n", phys, (uint32_t)rx_bufs[0]);

	e1000_write(E1000_REG_RDBAL, phys);
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
	uint32_t i, phys;

	/* Identity-map descriptor ring: virtual == physical, so TDBAL == pointer. */
	phys = vmm_findFreePage(sysID);
	if (!phys) {
		kprintf("e1000: cannot allocate TX descriptor ring\n");
		return -1;
	}
	vmm_remapIOPage(phys, KERNEL_PAGE_DEFAULT, sysID);
	tx_descs = (struct e1000_tx_desc *)phys;
	memset(tx_descs, 0, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));

	for (i = 0; i < E1000_NUM_TX_DESC; i++) {
		uint32_t buf_phys = vmm_findFreePage(sysID);
		if (!buf_phys) {
			kprintf("e1000: cannot allocate TX buffer %u\n", i);
			return -1;
		}
		vmm_remapIOPage(buf_phys, KERNEL_PAGE_DEFAULT, sysID);
		tx_bufs[i] = (uint8_t *)buf_phys;
		tx_descs[i].addr   = (uint64_t)buf_phys; /* physical == virtual */
		tx_descs[i].status = E1000_TXD_STAT_DD;
	}
	tx_tail = 0;

	kprintf("e1000: TX ring virt=phys=0x%X buf[0]=0x%X\n", phys, (uint32_t)tx_bufs[0]);

	e1000_write(E1000_REG_TDBAL, phys);
	e1000_write(E1000_REG_TDBAH, 0);
	e1000_write(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
	e1000_write(E1000_REG_TDH, 0);
	e1000_write(E1000_REG_TDT, 0);

	/* No descriptor write-back batching — write back each descriptor immediately. */
	e1000_write(E1000_REG_TXDCTL, 0);

	/* Recommended TIPG for 802.3 (from Intel manual) */
	e1000_write(E1000_REG_TIPG, 0x0060200Au);

	e1000_write(E1000_REG_TCTL,
	    E1000_TCTL_EN |
	    E1000_TCTL_PSP |
	    (0x0F << 4)  |   /* CT: collision threshold */
	    (0x40 << 12));   /* COLD: 64 byte slot time for full-duplex gigabit */

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

	/* Verify identity mapping: vmm_getRealAddr must equal the pointer. */
	if (tail == 0) {
		uint32_t virt  = (uint32_t)tx_descs;
		uint32_t phys2 = vmm_getRealAddr(virt);
		kprintf("e1000: send virt=0x%X vmm_getRealAddr=0x%X %s\n",
		    virt, phys2,
		    (virt == phys2) ? "MATCH" : "MISMATCH!");
		/* Also dump PDE[0] and PTE[0x31F] directly for diagnosis */
		{
			uint32_t *pd = (uint32_t *)0xC0400000; /* PD_BASE_ADDR */
			uint32_t *pt = (uint32_t *)0xC0000000; /* PT_BASE_ADDR + PD[0]*0x1000 */
			kprintf("e1000: PDE[0]=0x%X PTE[0x31F]=0x%X\n",
			    pd[0], pt[0x31F]);
		}
	}

	/* Wait for the descriptor to be free (DD set by hardware).
	 * Volatile cast: QEMU writes DD via DMA; compiler cannot observe the change. */
	for (i = 0; i < 100000; i++) {
		if (*(volatile uint8_t *)&tx_descs[tail].status & E1000_TXD_STAT_DD)
			break;
	}
	if (!(*(volatile uint8_t *)&tx_descs[tail].status & E1000_TXD_STAT_DD)) {
		kprintf("e1000: TX timeout desc[%u] TDH=%u TDT=%u TCTL=0x%X\n",
		    tail, e1000_read(E1000_REG_TDH), e1000_read(E1000_REG_TDT),
		    e1000_read(E1000_REG_TCTL));
		return;
	}

	memcpy(tx_bufs[tail], data, len);

	tx_descs[tail].length = len;
	tx_descs[tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
	tx_descs[tail].status = 0;

	/* Full memory barrier: descriptor stores must be globally visible before
	 * the TDT MMIO write triggers QEMU's DMA read of the descriptor. */
	__sync_synchronize();

	/* Dump raw descriptor bytes (phys == virt with identity map). */
	{
		uint8_t *raw = (uint8_t *)&tx_descs[tail];
		kprintf("e1000: desc[%u] phys=virt=0x%X raw: "
		    "%02X%02X%02X%02X%02X%02X%02X%02X "
		    "%02X%02X%02X%02X%02X%02X%02X%02X\n",
		    tail, (uint32_t)&tx_descs[tail],
		    raw[0],  raw[1],  raw[2],  raw[3],
		    raw[4],  raw[5],  raw[6],  raw[7],
		    raw[8],  raw[9],  raw[10], raw[11],
		    raw[12], raw[13], raw[14], raw[15]);
	}

	tx_tail = (tail + 1) % E1000_NUM_TX_DESC;
	if (tail == 0) {
		kprintf("e1000: ring TDBAL=0x%X TDBAH=0x%X TDLEN=%u TDH=%u TDT(pre)=%u TCTL=0x%X\n",
		    e1000_read(E1000_REG_TDBAL), e1000_read(E1000_REG_TDBAH),
		    e1000_read(E1000_REG_TDLEN),
		    e1000_read(E1000_REG_TDH), e1000_read(E1000_REG_TDT),
		    e1000_read(E1000_REG_TCTL));
	}
	{
		uint32_t tdh_before = e1000_read(E1000_REG_TDH);
		e1000_write(E1000_REG_TDT, tx_tail);
		uint32_t tdh_after = e1000_read(E1000_REG_TDH);
		uint8_t  dd_after  = *(volatile uint8_t *)&tx_descs[tail].status;
		kprintf("e1000: TX len=%u desc[%u] TDH %u->%u ICR=0x%X DD_after=0x%X\n",
		    len, tail, tdh_before, tdh_after,
		    e1000_read(E1000_REG_ICR), dd_after);
	}
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

	while (*(volatile uint8_t *)&rx_descs[rx_tail].status & E1000_RXD_STAT_DD) {
		if (*(volatile uint8_t *)&rx_descs[rx_tail].status & E1000_RXD_STAT_EOP) {
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
		/* Poll ring directly — catch packets if IRQ is not firing.
		 * Volatile cast prevents the compiler from hoisting the status
		 * read out of this loop (QEMU DMA is invisible to the optimizer). */
		if (*(volatile uint8_t *)&rx_descs[rx_tail].status & E1000_RXD_STAT_DD)
			e1000_rx_process();
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

	/* Disable all interrupts before reset */
	e1000_write(E1000_REG_IMC, 0xFFFFFFFFu);

	/* Software reset — puts the device into a known state so QEMU's DMA
	 * state machine is ready.  PCI config space (bus master, BAR addresses)
	 * is unaffected by CTRL.RST; the caller re-enables bus master after we
	 * return. */
	e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_RST);
	for (i = 0; i < 20000; i++) {
		if (!(e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST))
			break;
	}
	if (e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST)
		kprintf("e1000: reset did not clear — continuing anyway\n");

	/* Disable interrupts again (reset re-enables them) */
	e1000_write(E1000_REG_IMC, 0xFFFFFFFFu);

	/* Set full-duplex, link up, auto-speed detection; clear ILOS/LRST */
	e1000_write(E1000_REG_CTRL,
	    (e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD)
	    & ~(E1000_CTRL_LRST | E1000_CTRL_ILOS));
	kprintf("e1000: CTRL=0x%X STATUS=0x%X\n",
	    e1000_read(E1000_REG_CTRL), e1000_read(E1000_REG_STATUS));

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

	kprintf("e1000: MAC %02X:%02X:%02X:%02X:%02X:%02X STATUS=0x%X\n",
	    e1000_mac[0], e1000_mac[1], e1000_mac[2],
	    e1000_mac[3], e1000_mac[4], e1000_mac[5],
	    e1000_read(E1000_REG_STATUS));

	if (e1000_init_rx() != 0) return -1;
	if (e1000_init_tx() != 0) return -1;

	/* Wire IRQ */
	vec = (irq >= 8) ? (sVec + (irq - 8)) : (mVec + irq);
	setVector(&e1000_isr, vec, dInt + dPresent + dDpl3);
	irqEnable(irq);

	/* Enable RX timer, RX overrun, and link-change interrupts.
	 * TXDW (TX descriptor written back) is not needed — we poll DD in send. */
	e1000_write(E1000_REG_IMS, E1000_ICR_RXT0 | E1000_ICR_RXO | E1000_ICR_LSC);
	kprintf("e1000: vec=0x%X irq=%u IMS=0x%X\n",
	    vec, irq, e1000_read(E1000_REG_IMS));

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

/* -----------------------------------------------------------------------
 * newbus-lite PCI driver registration
 * --------------------------------------------------------------------- */

static int
e1000_ubx_probe(struct ubx_device *dev)
{
	if (dev->dev_vendor == E1000_VENDOR_ID &&
	    dev->dev_device_id == E1000_DEVICE_82540EM)
		return (0);
	return (-1);
}

static int
e1000_ubx_attach(struct ubx_device *dev)
{
	uint32_t bar0, cmd;
	uint8_t irq;
	int i, ret;

	bar0 = 0;
	irq  = 0;

	for (i = 0; i < dev->dev_nres; i++) {
		if (dev->dev_res[i].r_type == UBX_RES_MEMORY && bar0 == 0)
			bar0 = dev->dev_res[i].r_start;
		else if (dev->dev_res[i].r_type == UBX_RES_IRQ)
			irq = (uint8_t)dev->dev_res[i].r_start;
	}

	cmd = pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 4);
	cmd |= 0x06; /* Memory Space Enable + Bus Master Enable */
	pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, cmd, 4);

	kprintf("pci: found e1000 @ BAR0=0x%X IRQ=%u\n", bar0, irq);
	ret = initE1000(bar0, irq);

	/* Re-enable bus mastering — CTRL.RST inside initE1000 may have cleared it. */
	cmd = pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 4);
	cmd |= 0x06;
	pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, cmd, 4);
	kprintf("pci: e1000 PCI CMD after init=0x%X (bus master %s)\n",
	    pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 4),
	    (cmd & 0x04) ? "on" : "OFF");

	return (ret);
}

struct ubx_driver e1000_ubx_driver = {
	.drv_name   = "e1000",
	.drv_probe  = e1000_ubx_probe,
	.drv_attach = e1000_ubx_attach,
	.drv_detach = NULL,
};
