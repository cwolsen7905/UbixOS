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
#include <fs/devfs/devfs.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/types.h>
#include <isa/8259.h>
#include <isa/irq.h>
#include <lib/kmalloc.h>
#include <sys/klog.h>
#include <vmm/paging.h>
#include <vmm/vmm.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <net/netif.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Driver state
 * --------------------------------------------------------------------- */

/* ~50 ms at the 200 Hz scheduler tick: how often the RX thread wakes to poll
 * the descriptor ring as a safety net for a dropped e1000 PIC IRQ (QEMU). */
#define E1000_RX_SAFETY_TICKS 10

int          e1000_ready      = 0;
volatile int e1000_irq_pending = 0;
u_int8_t      e1000_mac[6];

/* Kernel-virtual base of the 128 KB MMIO region */
static volatile u_int8_t *e1000_mmio = NULL;

/* Descriptor rings — identity-mapped pages (virtual == physical).
 * vmm_find_free_page + vmm_remap_io_page guarantees this so TDBAL/RDBAL
 * can be set directly to the pointer value with no vmm_getRealAddr call. */
static struct e1000_rx_desc *rx_descs;
static struct e1000_tx_desc *tx_descs;

/* Per-descriptor packet buffers — virtual pointers */
static u_int8_t *rx_bufs[E1000_NUM_RX_DESC];
static u_int8_t *tx_bufs[E1000_NUM_TX_DESC];

static u_int32_t rx_tail;  /* next descriptor to check for received packet */
static u_int32_t tx_tail;  /* next free TX descriptor */
static struct spinLock e1000_tx_lock = SPIN_LOCK_INITIALIZER;

/* Scratch buffer handed to the lwIP bridge on each RX */
static u_int8_t e1000_rx_packet[E1000_BUF_SIZE];
static u_int16_t e1000_rx_len;

/* lwIP netif handle (defined in e1000netif.c) */
extern struct netif e1000_netif;

/* -----------------------------------------------------------------------
 * Register accessors
 * --------------------------------------------------------------------- */

static inline u_int32_t e1000_read(u_int32_t reg) {
	return *(volatile u_int32_t *)(e1000_mmio + reg);
}

static inline void e1000_write(u_int32_t reg, u_int32_t val) {
	*(volatile u_int32_t *)(e1000_mmio + reg) = val;
}

/* -----------------------------------------------------------------------
 * MMIO mapping
 * --------------------------------------------------------------------- */

/*
 * Identity-map the 128 KB MMIO BAR into kernel virtual space.
 * vmm_remap_io_page maps phys->phys and silently overwrites existing PTEs,
 * avoiding the panic that vmm_remap_page emits when a page is already present
 * (vmm_get_free_kernel_page pre-populates its range with real RAM pages).
 * PAGE_CACHE_DISABLED is required — MMIO must never be cached.
 */
static int e1000_map_mmio(u_int32_t phys) {
	u_int32_t pages = 32; /* 128 KB / 4 KB */
	u_int32_t i;

	for (i = 0; i < pages; i++)
		vmm_remap_io_page(phys + i * 0x1000,
		    KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED, sysID);
	e1000_mmio = (volatile u_int8_t *)phys;
	return 0;
}

/* -----------------------------------------------------------------------
 * Descriptor ring setup
 * --------------------------------------------------------------------- */

static int e1000_init_rx(void) {
	u_int32_t i, phys;

	/* Identity-map descriptor ring: virtual == physical, so RDBAL == pointer. */
	phys = vmm_find_free_page(sysID);
	if (!phys) {
		klog(KLOG_ERR, "e1000: cannot allocate RX descriptor ring");
		return -1;
	}
	vmm_remap_io_page(phys, KERNEL_PAGE_DEFAULT, sysID);
	rx_descs = (struct e1000_rx_desc *)phys;
	memset(rx_descs, 0, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));

	for (i = 0; i < E1000_NUM_RX_DESC; i++) {
		u_int32_t buf_phys = vmm_find_free_page(sysID);
		if (!buf_phys) {
			klog(KLOG_ERR, "e1000: cannot allocate RX buffer %u", i);
			return -1;
		}
		vmm_remap_io_page(buf_phys, KERNEL_PAGE_DEFAULT, sysID);
		rx_bufs[i] = (u_int8_t *)buf_phys;
		rx_descs[i].addr   = (u_int64_t)buf_phys; /* physical == virtual */
		rx_descs[i].status = 0;
	}
	rx_tail = 0;

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
	u_int32_t i, phys;

	/* Identity-map descriptor ring: virtual == physical, so TDBAL == pointer. */
	phys = vmm_find_free_page(sysID);
	if (!phys) {
		klog(KLOG_ERR, "e1000: cannot allocate TX descriptor ring");
		return -1;
	}
	vmm_remap_io_page(phys, KERNEL_PAGE_DEFAULT, sysID);
	tx_descs = (struct e1000_tx_desc *)phys;
	memset(tx_descs, 0, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));

	for (i = 0; i < E1000_NUM_TX_DESC; i++) {
		u_int32_t buf_phys = vmm_find_free_page(sysID);
		if (!buf_phys) {
			klog(KLOG_ERR, "e1000: cannot allocate TX buffer %u", i);
			return -1;
		}
		vmm_remap_io_page(buf_phys, KERNEL_PAGE_DEFAULT, sysID);
		tx_bufs[i] = (u_int8_t *)buf_phys;
		tx_descs[i].addr   = (u_int64_t)buf_phys; /* physical == virtual */
		tx_descs[i].status = E1000_TXD_STAT_DD;
	}
	tx_tail = 0;

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

void e1000_send_packet(const void *data, u_int16_t len) {
	u_int32_t tail;
	int i;

	spinLock(&e1000_tx_lock);
	tail = tx_tail;

	if (len > E1000_BUF_SIZE) {
		klog(KLOG_ERR, "e1000: TX packet too large (%u)", len);
		spinUnlock(&e1000_tx_lock);
		return;
	}

	/* Wait for the descriptor to be free (DD set by hardware).
	 * Volatile cast: QEMU writes DD via DMA; compiler cannot observe the change. */
	for (i = 0; i < 100000; i++) {
		if (*(volatile u_int8_t *)&tx_descs[tail].status & E1000_TXD_STAT_DD)
			break;
		__asm__ volatile("pause");
		if ((i & 0x3FF) == 0x3FF)
			sched_yield();
	}
	if (!(*(volatile u_int8_t *)&tx_descs[tail].status & E1000_TXD_STAT_DD)) {
		klog(KLOG_ERR, "e1000: TX timeout desc[%u] TDH=%u TDT=%u",
		    tail, e1000_read(E1000_REG_TDH), e1000_read(E1000_REG_TDT));
		spinUnlock(&e1000_tx_lock);
		return;
	}

	memcpy(tx_bufs[tail], data, len);

	tx_descs[tail].length = len;
	tx_descs[tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
	tx_descs[tail].status = 0;

	/* Full memory barrier: descriptor stores must be globally visible before
	 * the TDT MMIO write triggers QEMU's DMA read of the descriptor. */
	__sync_synchronize();

	tx_tail = (tail + 1) % E1000_NUM_TX_DESC;
	e1000_write(E1000_REG_TDT, tx_tail);
	spinUnlock(&e1000_tx_lock);
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

	while (*(volatile u_int8_t *)&rx_descs[rx_tail].status & E1000_RXD_STAT_DD) {
		if (*(volatile u_int8_t *)&rx_descs[rx_tail].status & E1000_RXD_STAT_EOP) {
			u_int16_t len = rx_descs[rx_tail].length;
			if (len <= E1000_BUF_SIZE) {
				memcpy(e1000_rx_packet, rx_bufs[rx_tail], len);
				e1000_rx_len = len;
				/* Notify lwIP bridge — defined in e1000netif.c */
				extern void e1000netif_input(struct netif *);
				e1000netif_input(&e1000_netif);
			} else {
				klog(KLOG_WARNING, "e1000: oversized rx frame %u bytes dropped", len);
			}
		}

		/* Return descriptor to hardware: write RDT with the just-consumed
		 * index before advancing — this is how QEMU's e1000 model expects
		 * the tail to be updated (write-back-then-advance convention). */
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
	u_int32_t icr = e1000_read(E1000_REG_ICR); /* reading clears ICR */

	if (icr & E1000_ICR_LSC) {
		u_int32_t status = e1000_read(E1000_REG_STATUS);
		klog(KLOG_NOTICE, "e1000: link %s", (status & 0x02) ? "up" : "down");
	}
	if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXO)) {
		e1000_irq_pending = 1;
		/* Wake the RX thread (sleeping in sched_wait_event on this address).
		 * Safe from ISR context: sched_wakeup_chan() never yields. */
		sched_wakeup_chan(&e1000_irq_pending);
	}
}

/* -----------------------------------------------------------------------
 * RX thread — wakes on IRQ flag and drains the ring
 * --------------------------------------------------------------------- */

/*
 * Wait predicate for the RX thread: woken work exists when the ISR flagged a
 * receive interrupt, or a descriptor's DD (descriptor-done) bit is already
 * visible.  Checking DD as well as the flag absorbs the QEMU quirk where the
 * descriptor write becomes visible slightly out of order with the PIC IRQ — any
 * wake then still drains everything ready.
 */
static int e1000_rx_ready(void *arg) {
	(void)arg;
	if (e1000_irq_pending)
		return 1;
	return (*(volatile u_int8_t *)&rx_descs[rx_tail].status & E1000_RXD_STAT_DD) != 0;
}

void e1000_thread(void) {
	for (;;) {
		/*
		 * Sleep (off the run queue) until the ISR wakes us via
		 * sched_wakeup_chan(&e1000_irq_pending).  The bounded timeout (~50 ms)
		 * is a safety net: QEMU occasionally fails to deliver the e1000 PIC
		 * IRQ, so we wake periodically to poll the descriptor ring's DD bit
		 * (checked by e1000_rx_ready) rather than stalling RX — and the whole
		 * IP stack with it — until the next IRQ.  Still no busy spin: idle
		 * between wakes.
		 */
		sched_wait_event_timeout(&e1000_irq_pending, e1000_rx_ready, NULL, E1000_RX_SAFETY_TICKS);
		e1000_irq_pending = 0;
		e1000_rx_process();
	}
}


/* -----------------------------------------------------------------------
 * Initialization entry point (called from pci_init / init.h)
 * --------------------------------------------------------------------- */

int initE1000(u_int32_t bar0_phys, u_int8_t irq) {
	u_int32_t ral, rah;
	int      i;

	klog(KLOG_INFO, "e1000: initializing 82540EM at BAR0=0x%X IRQ=%u", bar0_phys, irq);

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
		klog(KLOG_WARNING, "e1000: reset did not clear, continuing anyway");

	/* Disable interrupts again (reset re-enables them) */
	e1000_write(E1000_REG_IMC, 0xFFFFFFFFu);

	/* Set full-duplex, link up, auto-speed detection; clear ILOS/LRST */
	e1000_write(E1000_REG_CTRL,
	    (e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD)
	    & ~(E1000_CTRL_LRST | E1000_CTRL_ILOS));
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

	klog(KLOG_NOTICE, "e1000: MAC %02X:%02X:%02X:%02X:%02X:%02X",
	    e1000_mac[0], e1000_mac[1], e1000_mac[2],
	    e1000_mac[3], e1000_mac[4], e1000_mac[5]);

	if (e1000_init_rx() != 0) return -1;
	if (e1000_init_tx() != 0) return -1;

	/* Wire IRQ via shared dispatch layer */
	irq_register(irq, e1000_handle_irq);

	/* Enable RX timer, RX overrun, and link-change interrupts.
	 * TXDW (TX descriptor written back) is not needed — we poll DD in send. */
	e1000_write(E1000_REG_IMS, E1000_ICR_RXT0 | E1000_ICR_RXO | E1000_ICR_LSC);

	e1000_ready = 1;

	klog(KLOG_NOTICE, "e1000: ready (irq=%u)", irq);
	return 0;
}

/* -----------------------------------------------------------------------
 * Accessors for the lwIP bridge
 * --------------------------------------------------------------------- */

const u_int8_t *e1000_get_rx_packet(u_int16_t *out_len) {
	*out_len = e1000_rx_len;
	e1000_rx_len = 0;
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
	u_int32_t bar0, cmd;
	u_int8_t irq;
	int i, ret;

	bar0 = 0;
	irq  = 0;

	for (i = 0; i < dev->dev_nres; i++) {
		if (dev->dev_res[i].r_type == UBX_RES_MEMORY && bar0 == 0)
			bar0 = dev->dev_res[i].r_start;
		else if (dev->dev_res[i].r_type == UBX_RES_IRQ)
			irq = (u_int8_t)dev->dev_res[i].r_start;
	}

	klog(KLOG_INFO, "e1000: found at bus=%u slot=%u func=%u BAR0=0x%X IRQ=%u",
	    dev->dev_bus, dev->dev_slot, dev->dev_func, bar0, irq);

	cmd = pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 2);
	cmd |= 0x07; /* I/O Space + Memory Space + Bus Master */
	pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, cmd, 2);

	ret = initE1000(bar0, irq);

	/* Re-enable bus mastering — CTRL.RST inside initE1000 may have cleared it. */
	cmd = pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 2);
	cmd |= 0x07; /* I/O Space + Memory Space + Bus Master */
	pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, cmd, 2);

	if (ret == 0)
		devfs_makeNode("em0", 'c', 14, 0);

	return (ret);
}

struct ubx_driver e1000_ubx_driver = {
	.drv_name   = "e1000",
	.drv_probe  = e1000_ubx_probe,
	.drv_attach = e1000_ubx_attach,
	.drv_detach = NULL,
};
