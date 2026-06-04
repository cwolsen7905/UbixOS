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
 * NE2000 / RealTek RTL8029(AS) Ethernet driver — modern newbus + lwIP path.
 *
 * Built to the same shape as the e1000 driver: a top-half IRQ handler that
 * only flags work and wakes a dedicated RX thread (sched_wakeup_chan), and a
 * thread that sleeps off the run queue (sched_wait_event_timeout) and drains
 * the chip's receive ring.  No busy-polling, no kmalloc in interrupt context.
 *
 * The DP8390 differs from the e1000 in one respect that shapes this code: its
 * receive path and its interrupt-status register share the same I/O port set,
 * so the RX drain briefly disables interrupts around each chip-register burst
 * (the top-half only ever touches the ISR register, so the windows can't
 * corrupt one another).  See ne2k_pull_one().
 *
 * Receive-ring overflow recovery (ISR_OVW) is not yet implemented; the RX
 * thread's safety timeout drains the ring fast enough for interactive use and
 * the e1000 remains the primary/QEMU-default NIC.  ISA attach (real vintage
 * hardware) is a future addition — ne2k_hw_init() already takes (iobase, irq).
 */

#include <pci/ne2k.h>
#include <pci/pci.h>
#include <sys/bus.h>
#include <sys/io.h>
#include <isa/irq.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <ubixos/wait.h>
#include <fs/devfs/devfs.h>
#include <lib/kprintf.h>
#include <sys/klog.h>
#include <string.h>

/* Published state */
u_int8_t ne2k_mac[6];
int ne2k_ready = 0;
volatile int ne2k_irq_pending = 0;

/* Private state — all static, at file top per style(9). */
static u_int32_t g_iobase = 0;
static u_int8_t g_irq = 0;
static struct spinLock g_tx_lock = SPIN_LOCK_INITIALIZER;

/* Single shared RX staging buffer handed to the lwIP bridge, e1000-style. */
static u_int8_t g_rx_packet[NE_MAX_FRAME];
static u_int16_t g_rx_len = 0;

/* Defined in sys/net/netif/ne2knetif.c */
extern struct netif ne2k_netif;
extern void ne2knetif_input(struct netif *netif);

/* ----------------------------------------------------------------------
 * Low-level register helpers
 * -------------------------------------------------------------------- */

static inline void ne2k_out(u_int16_t reg, u_int8_t val)
{
	outportByte(g_iobase + reg, val);
}

static inline u_int8_t ne2k_in(u_int16_t reg)
{
	return (inportByte(g_iobase + reg));
}

/*
 * Read `len` bytes of on-card buffer RAM starting at NIC address `addr`
 * using a remote-DMA read.  16-bit (word) transfers — `len` is rounded up to
 * an even count.  Caller must already hold the card on page 0 with no DMA in
 * flight; this is only ever called from the RX path under interrupts disabled.
 */
static void ne2k_read_mem(u_int16_t addr, void *dst, u_int16_t len)
{
	u_int16_t *p = (u_int16_t *)dst;
	u_int16_t words;
	u_int16_t i;

	if (len & 1)
		len++;
	words = len / 2;

	ne2k_out(NE_RBCR0, len & 0xFF);
	ne2k_out(NE_RBCR1, len >> 8);
	ne2k_out(NE_RSAR0, addr & 0xFF);
	ne2k_out(NE_RSAR1, addr >> 8);
	ne2k_out(NE_CMD, CMD_RD0 | CMD_STA);

	for (i = 0; i < words; i++)
		p[i] = inportWord(g_iobase + NE_DATA);

	ne2k_out(NE_ISR, ISR_RDC); /* acknowledge remote-DMA-complete */
}

/* ----------------------------------------------------------------------
 * Initialisation
 * -------------------------------------------------------------------- */

/*
 * Reset the DP8390 and read the 6-byte station address out of the PROM.
 * On a 16-bit card every PROM byte is duplicated, so the MAC occupies the
 * even bytes of the first 12 PROM bytes.
 */
static void ne2k_reset_and_read_mac(void)
{
	u_int8_t prom[32];
	int i;

	/* Hardware reset: read the reset port, write the value back. */
	ne2k_out(NE_RESET, ne2k_in(NE_RESET));
	for (i = 0; i < 10000; i++)
		if (ne2k_in(NE_ISR) & ISR_RST)
			break;
	ne2k_out(NE_ISR, 0xFF); /* clear all interrupt-status bits */

	/* Stop, page 0, abort DMA. */
	ne2k_out(NE_CMD, CMD_RD2 | CMD_STP);
	ne2k_out(NE_DCR, 0x01); /* word-wide DMA (WTS)                */
	ne2k_out(NE_RBCR0, 0x00);
	ne2k_out(NE_RBCR1, 0x00);
	ne2k_out(NE_IMR, 0x00); /* mask all interrupts during setup   */
	ne2k_out(NE_ISR, 0xFF);
	ne2k_out(NE_RCR, 0x20); /* monitor mode — buffer nothing yet  */
	ne2k_out(NE_TCR, 0x02); /* internal loopback during setup     */

	/* Read 32 bytes of PROM from on-card address 0 via remote DMA. */
	ne2k_read_mem(0x0000, prom, sizeof(prom));
	for (i = 0; i < 6; i++)
		ne2k_mac[i] = prom[i * 2];
}

/**
 * Bring the NE2000/RTL8029 at the given I/O base + IRQ up to a running,
 * interrupt-driven receive state and wire its IRQ to the shared dispatcher.
 *
 * @param iobase  I/O port base (from the PCI I/O BAR, or a fixed ISA base).
 * @return 0 on success.
 */
int ne2k_hw_init(u_int32_t iobase, u_int8_t irq)
{
	int i;

	g_iobase = iobase;
	g_irq = irq;

	klog(KLOG_INFO, "ne2k: initializing RTL8029 at I/O 0x%X IRQ=%u", iobase, irq);

	ne2k_reset_and_read_mac();

	klog(KLOG_NOTICE,
	     "ne2k: MAC %02X:%02X:%02X:%02X:%02X:%02X",
	     ne2k_mac[0],
	     ne2k_mac[1],
	     ne2k_mac[2],
	     ne2k_mac[3],
	     ne2k_mac[4],
	     ne2k_mac[5]);

	/* Program the receive ring. */
	ne2k_out(NE_CMD, CMD_RD2 | CMD_STP); /* page 0, stop          */
	ne2k_out(NE_PSTART, NE_RX_START_PAGE);
	ne2k_out(NE_BNRY, NE_RX_START_PAGE); /* host owns up to here  */
	ne2k_out(NE_PSTOP, NE_RX_STOP_PAGE);
	ne2k_out(NE_TPSR, NE_TX_START_PAGE);

	/* Load the station address into PAR0..5 (page 1) and accept-all
	 * multicast into MAR0..7, then set CURR one page past PSTART. */
	ne2k_out(NE_CMD, CMD_RD2 | CMD_PS0 | CMD_STP); /* page 1, stop */
	for (i = 0; i < 6; i++)
		ne2k_out(NE_PAR0 + i, ne2k_mac[i]);
	for (i = 0; i < 8; i++)
		ne2k_out(NE_MAR0 + i, 0xFF);
	ne2k_out(NE_CURR, NE_RX_START_PAGE + 1);

	/* Back to page 0, start the card. */
	ne2k_out(NE_CMD, CMD_RD2 | CMD_STA);
	ne2k_out(NE_ISR, 0xFF);
	ne2k_out(NE_TCR, 0x00); /* normal transmit mode  */
	ne2k_out(NE_RCR, 0x04); /* accept broadcast + own MAC */

	/* Wire the IRQ via the shared dispatch layer, then unmask receive
	 * (PRX) and ring-overflow (OVW) interrupts.  TX completion is polled
	 * in ne2k_send_packet(), so PTX stays masked. */
	irq_register(irq, ne2k_handle_irq);
	ne2k_out(NE_IMR, ISR_PRX | ISR_OVW);

	ne2k_ready = 1;
	klog(KLOG_NOTICE, "ne2k: ready (irq=%u)", irq);
	return (0);
}

/* ----------------------------------------------------------------------
 * Transmit
 * -------------------------------------------------------------------- */

/**
 * Transmit one Ethernet frame.  Frames shorter than the 60-byte Ethernet
 * minimum are zero-padded.  Synchronous: copies into card RAM via remote DMA,
 * waits for DMA completion, then issues the transmit command.
 */
void ne2k_send_packet(const void *data, u_int16_t len)
{
	const u_int8_t *src = data;
	u_int16_t xlen;
	u_int16_t words;
	u_int16_t i;
	u_int32_t flags;
	u_int8_t pad[NE_MAX_FRAME];

	if (!ne2k_ready)
		return;

	xlen = len;
	if (xlen < 60)
		xlen = 60; /* pad to Ethernet minimum frame size */
	if (xlen > NE_MAX_FRAME)
		xlen = NE_MAX_FRAME;

	memset(pad, 0, xlen);
	memcpy(pad, src, (len < xlen) ? len : xlen);

	spinLock(&g_tx_lock);
	save_flags(flags);
	__asm__ __volatile__("cli");

	/* Remote-DMA write of the frame into card RAM at the TX page. */
	ne2k_out(NE_ISR, ISR_RDC);
	ne2k_out(NE_RBCR0, xlen & 0xFF);
	ne2k_out(NE_RBCR1, xlen >> 8);
	ne2k_out(NE_RSAR0, 0x00);
	ne2k_out(NE_RSAR1, NE_TX_START_PAGE);
	ne2k_out(NE_CMD, CMD_RD1 | CMD_STA);

	words = (xlen + 1) / 2;
	for (i = 0; i < words; i++)
		outportWord(g_iobase + NE_DATA, ((u_int16_t *)pad)[i]);

	/* Wait for remote DMA to complete. */
	for (i = 0; i < 1000; i++)
		if (ne2k_in(NE_ISR) & ISR_RDC)
			break;
	ne2k_out(NE_ISR, ISR_RDC);

	/* Issue the transmit. */
	ne2k_out(NE_TPSR, NE_TX_START_PAGE);
	ne2k_out(NE_TBCR0, xlen & 0xFF);
	ne2k_out(NE_TBCR1, xlen >> 8);
	ne2k_out(NE_CMD, CMD_RD2 | CMD_TXP | CMD_STA);

	restore_flags(flags);
	spinUnlock(&g_tx_lock);
}

/* ----------------------------------------------------------------------
 * Receive
 * -------------------------------------------------------------------- */

/*
 * Pull at most one packet out of the receive ring into g_rx_packet.
 * Returns the frame length (without the 4-byte ring header), or 0 if the ring
 * is empty / the packet is malformed.
 *
 * Runs the whole chip-register sequence with interrupts disabled so the
 * top-half handler (which only touches NE_ISR) cannot interleave with the
 * page switch and remote-DMA reads.  The window is bounded by one frame's
 * worth of port I/O.
 */
static u_int16_t ne2k_pull_one(void)
{
	u_int32_t flags;
	u_int8_t hdr[4];
	u_int8_t curr, bnry, next;
	u_int16_t count, plen;

	save_flags(flags);
	__asm__ __volatile__("cli");

	/* CURR lives on page 1; read it then return to page 0. */
	ne2k_out(NE_CMD, CMD_RD2 | CMD_PS0 | CMD_STA);
	curr = ne2k_in(NE_CURR);
	ne2k_out(NE_CMD, CMD_RD2 | CMD_STA);

	bnry = ne2k_in(NE_BNRY);
	next = bnry + 1;
	if (next >= NE_RX_STOP_PAGE)
		next = NE_RX_START_PAGE;

	if (next == curr)
	{
		restore_flags(flags); /* ring empty */
		return (0);
	}

	/* The 4-byte ring header: status, next-page, count-lo, count-hi. */
	ne2k_read_mem(next * NE_PAGE_SIZE, hdr, sizeof(hdr));
	count = hdr[2] | (hdr[3] << 8);

	if (count < 4 + 14 || count > 4 + NE_MAX_FRAME || hdr[1] < NE_RX_START_PAGE || hdr[1] >= NE_RX_STOP_PAGE)
	{
		/* Malformed descriptor — resynchronise the ring to CURR and bail. */
		ne2k_out(NE_BNRY, (curr == NE_RX_START_PAGE ? NE_RX_STOP_PAGE : curr) - 1);
		restore_flags(flags);
		klog(KLOG_WARNING, "ne2k: bad rx descriptor (count=%u next=%u)", count, hdr[1]);
		return (0);
	}

	plen = count - 4; /* strip the ring header */
	ne2k_read_mem(next * NE_PAGE_SIZE + 4, g_rx_packet, plen);
	g_rx_len = plen;

	/* Release the consumed pages: BNRY = (page before the next packet). */
	ne2k_out(NE_BNRY, (hdr[1] == NE_RX_START_PAGE ? NE_RX_STOP_PAGE : hdr[1]) - 1);

	restore_flags(flags);
	return (plen);
}

/*
 * Drain every packet currently in the ring, handing each to the lwIP bridge.
 * Called only from the RX thread, so the (interrupts-enabled) lwIP handoff in
 * ne2knetif_input is never run with interrupts disabled.
 */
static void ne2k_rx_process(void)
{
	while (ne2k_pull_one() > 0)
		ne2knetif_input(&ne2k_netif);
}

/* ----------------------------------------------------------------------
 * IRQ handler (top half) — runs from irq_dispatch with interrupts disabled
 * -------------------------------------------------------------------- */

void ne2k_handle_irq(void)
{
	u_int8_t isr = ne2k_in(NE_ISR);

	/* Acknowledge every asserted bit (touches only NE_ISR — safe against an
	 * RX-thread remote DMA, which never reads or writes this register). */
	ne2k_out(NE_ISR, isr);

	if (isr & (ISR_PRX | ISR_OVW))
	{
		ne2k_irq_pending = 1;
		sched_wakeup_chan((void *)&ne2k_irq_pending);
	}
}

/* ----------------------------------------------------------------------
 * RX thread (bottom half)
 * -------------------------------------------------------------------- */

/*
 * Wait predicate: work exists when the ISR flagged a receive, or a packet is
 * already sitting in the ring (CURR has advanced past BNRY).  Reading the ring
 * pointers here is a quick page-0/page-1 register peek; like e1000_rx_ready it
 * absorbs any wake/descriptor ordering quirk so a wake still drains everything.
 */
static int ne2k_rx_ready(void *arg)
{
	u_int8_t curr, next;
	u_int32_t flags;

	(void)arg;
	if (ne2k_irq_pending)
		return (1);

	save_flags(flags);
	__asm__ __volatile__("cli");
	ne2k_out(NE_CMD, CMD_RD2 | CMD_PS0 | CMD_STA);
	curr = ne2k_in(NE_CURR);
	ne2k_out(NE_CMD, CMD_RD2 | CMD_STA);
	next = ne2k_in(NE_BNRY) + 1;
	if (next >= NE_RX_STOP_PAGE)
		next = NE_RX_START_PAGE;
	restore_flags(flags);

	return (next != curr);
}

void ne2k_thread(void)
{
	for (;;)
	{
		/*
		 * Sleep off the run queue until the ISR wakes us.  The bounded
		 * timeout is the same safety net the e1000 RX thread uses: if a
		 * PIC IRQ is ever missed we still wake to drain the ring rather
		 * than stalling receive (and the whole IP stack) indefinitely.
		 */
		sched_wait_event_timeout((void *)&ne2k_irq_pending, ne2k_rx_ready, NULL, 10);
		ne2k_irq_pending = 0;
		ne2k_rx_process();
	}
}

/* ----------------------------------------------------------------------
 * Accessor for the lwIP bridge
 * -------------------------------------------------------------------- */

const u_int8_t *ne2k_get_rx_packet(u_int16_t *out_len)
{
	*out_len = g_rx_len;
	g_rx_len = 0;
	return (g_rx_packet);
}

/* ----------------------------------------------------------------------
 * newbus-lite PCI driver registration
 * -------------------------------------------------------------------- */

static int ne2k_ubx_probe(struct ubx_device *dev)
{
	if (dev->dev_vendor == NE2K_VENDOR_REALTEK && dev->dev_device_id == NE2K_DEVICE_RTL8029)
		return (0);
	return (-1);
}

static int ne2k_ubx_attach(struct ubx_device *dev)
{
	u_int32_t iobase = 0;
	u_int8_t irq = 0;
	u_int32_t cmd;
	int i;

	for (i = 0; i < dev->dev_nres; i++)
	{
		if (dev->dev_res[i].r_type == UBX_RES_IOPORT && iobase == 0)
			iobase = dev->dev_res[i].r_start;
		else if (dev->dev_res[i].r_type == UBX_RES_IRQ)
			irq = (u_int8_t)dev->dev_res[i].r_start;
	}

	if (iobase == 0)
	{
		kprintf("ne2k: no I/O BAR found\n");
		return (-1);
	}

	/* Enable I/O Space + Bus Master in the PCI command register. */
	cmd = pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 2);
	cmd |= 0x05;
	pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, cmd, 2);

	if (ne2k_hw_init(iobase, irq) != 0)
		return (-1);

	devfs_makeNode("ne0", 'c', 14, 2);
	return (0);
}

struct ubx_driver ne2k_ubx_driver = {
    .drv_name = "ne2k",
    .drv_probe = ne2k_ubx_probe,
    .drv_attach = ne2k_ubx_attach,
    .drv_detach = NULL,
};
