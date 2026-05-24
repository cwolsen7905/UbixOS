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
 * Intel 82801AA AC'97 audio driver for UbixOS.
 * Supports QEMU's AC97 device (PCI 8086:2415).
 * Provides 16-bit stereo PCM output at 48 kHz via /dev/audio.
 *
 * Programming model:
 *   - BAR0 (NAM): codec mixer registers accessed via I/O ports
 *   - BAR1 (NABM): bus-master DMA control registers
 *   - BDL: ring of 32 buffer descriptors; two ping-pong buffers with IOC
 *   - IRQ: fires on buffer completion; ISR refills from kernel ring buffer
 */

#include <pci/ac97.h>
#include <pci/pci.h>
#include <sys/bus.h>
#include <sys/dma_mem.h>
#include <sys/io.h>
#include <isa/8259.h>
#include <isa/irq.h>
#include <fs/devfs/devfs.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

/* Major number for /dev/audio — must not conflict with hd(1), kbd(1), mouse(13), net(14) */
#define AC97_MAJOR   15
#define AC97_MINOR    0

/* Module-level state — one AC'97 controller */
static struct ac97_softc ac97_sc;

/* -----------------------------------------------------------------------
 * Register accessors
 * --------------------------------------------------------------------- */

static inline u_int16_t
nam_rd16(struct ac97_softc *sc, u_int16_t reg)
{
	return inportWord(sc->nam_base + reg);
}

static inline void
nam_wr16(struct ac97_softc *sc, u_int16_t reg, u_int16_t val)
{
	outportWord(sc->nam_base + reg, val);
}

static inline u_int32_t
nabm_rd32(struct ac97_softc *sc, u_int16_t reg)
{
	return inportDWord(sc->nabm_base + reg);
}

static inline void
nabm_wr32(struct ac97_softc *sc, u_int16_t reg, u_int32_t val)
{
	outportDWord(sc->nabm_base + reg, val);
}

static inline u_int16_t
nabm_rd16(struct ac97_softc *sc, u_int16_t reg)
{
	return inportWord(sc->nabm_base + reg);
}

static inline void
nabm_wr16(struct ac97_softc *sc, u_int16_t reg, u_int16_t val)
{
	outportWord(sc->nabm_base + reg, val);
}

static inline u_int8_t
nabm_rd8(struct ac97_softc *sc, u_int16_t reg)
{
	return inportByte(sc->nabm_base + reg);
}

static inline void
nabm_wr8(struct ac97_softc *sc, u_int16_t reg, u_int8_t val)
{
	outportByte(sc->nabm_base + reg, val);
}

/* -----------------------------------------------------------------------
 * ISR — fires when a BDL entry with IOC completes
 * --------------------------------------------------------------------- */

static void
ac97_isr(void)
{
	struct ac97_softc *sc = &ac97_sc;
	u_int16_t sr;
	u_int8_t  fill_buf;
	u_int32_t avail;

	/* Always drain pi/mc status so spurious QEMU interrupt bits don't cause
	 * an IRQ storm on the shared IRQ 10 line. */
	{
		u_int16_t pi_sr = nabm_rd16(sc, 0x00 + NABM_SR);
		u_int16_t mc_sr = nabm_rd16(sc, 0x20 + NABM_SR);
		if (pi_sr) nabm_wr16(sc, 0x00 + NABM_SR, pi_sr);
		if (mc_sr) nabm_wr16(sc, 0x20 + NABM_SR, mc_sr);
	}

	/* Check BCIS on the PCM-out channel before acting. */
	sr = nabm_rd16(sc, NABM_PO_BASE + NABM_SR);
	if (!(sr & NABM_SR_BCIS))
		return;

	/* Acknowledge the PCM-out buffer completion */
	nabm_wr16(sc, NABM_PO_BASE + NABM_SR, sr);

	fill_buf    = sc->next_buf;
	sc->next_buf ^= 1;

	/* Drain ring into the next ping-pong buffer.
	 * Copy however many bytes are ready (up to AC97_BUF_BYTES) and pad
	 * any remainder with silence.  This avoids a full silent buffer on a
	 * near-underrun where a few bytes are available but less than a full
	 * buffer — the all-or-nothing approach turns a tiny gap into a 5 ms
	 * click every time the ring dips below AC97_BUF_BYTES. */
	avail = sc->ring_wr - sc->ring_rd;
	if (avail > (u_int32_t)AC97_BUF_BYTES)
		avail = AC97_BUF_BYTES;
	if (avail > 0) {
		u_int32_t rd    = sc->ring_rd & AC97_RING_MASK;
		u_int32_t tail  = AC97_RING_SIZE - rd;
		if (tail >= avail) {
			memcpy(sc->buf[fill_buf], sc->ring + rd, avail);
		} else {
			memcpy(sc->buf[fill_buf],        sc->ring + rd, tail);
			memcpy(sc->buf[fill_buf] + tail, sc->ring,      avail - tail);
		}
	}
	if (avail < (u_int32_t)AC97_BUF_BYTES)
		memset(sc->buf[fill_buf] + avail, 0, AC97_BUF_BYTES - avail);
	sc->ring_rd += avail;

	/* Advance LVI to include the newly filled buffer */
	sc->lvi = (sc->lvi + 1) & 31;
	sc->bdl[sc->lvi].addr  = sc->buf_dma[fill_buf].db_paddr;
	sc->bdl[sc->lvi].len   = AC97_BDL_LEN;
	sc->bdl[sc->lvi].flags = AC97_BDL_IOC;
	nabm_wr8(sc, NABM_PO_BASE + NABM_LVI, sc->lvi);
}

/* -----------------------------------------------------------------------
 * Codec initialisation
 * --------------------------------------------------------------------- */

static int
ac97_codec_init(struct ac97_softc *sc)
{
	u_int32_t i;

	/* Release cold reset so the codec comes out of reset */
	nabm_wr32(sc, NABM_GLOB_CNT, NABM_GCNT_COLD);

	/* Poll for primary codec ready (QEMU sets it instantly; real hw ~50 ms) */
	for (i = 0; i < 1000; i++) {
		if (nabm_rd32(sc, NABM_GLOB_STA) & NABM_GSTA_CODEC)
			break;
		/* burn a few I/O reads ≈ 1 µs each */
		inportByte(0x80);
		inportByte(0x80);
	}

	if (!(nabm_rd32(sc, NABM_GLOB_STA) & NABM_GSTA_CODEC)) {
		kprintf("ac97: codec not ready\n");
		return -1;
	}

	/* Software reset of codec registers */
	nam_wr16(sc, NAM_RESET, 0xFFFF);

	/* Unmute and set full volume */
	nam_wr16(sc, NAM_MASTER_VOL, 0x0000);
	nam_wr16(sc, NAM_HP_VOL,     0x0000);
	nam_wr16(sc, NAM_PCM_VOL,    0x0000);

	/* Enable Variable Rate Audio so userland can change sample rate via ioctl. */
	nam_wr16(sc, NAM_EXT_AUDIO, nam_rd16(sc, NAM_EXT_AUDIO) | NAM_EXT_VRA);

	/* Default output rate: 48 kHz. */
	nam_wr16(sc, NAM_PCM_RATE, 48000);

	kprintf("ac97: codec ready, VRA enabled, rate=48000 Hz\n");
	return 0;
}

/* -----------------------------------------------------------------------
 * DMA initialisation — allocate BDL + ping-pong buffers, start PCM out
 * --------------------------------------------------------------------- */

static int
ac97_dma_init(struct ac97_softc *sc)
{
	u_int32_t i;

	/* BDL: 32 entries × 8 bytes, 8-byte aligned */
	if (dma_alloc(sizeof(struct ac97_bdle) * AC97_BDL_ENTRIES, 8,
	    &sc->bdl_dma) != 0) {
		kprintf("ac97: BDL alloc failed\n");
		return -1;
	}
	sc->bdl = (struct ac97_bdle *)sc->bdl_dma.db_vaddr;
	memset(sc->bdl, 0, sizeof(struct ac97_bdle) * AC97_BDL_ENTRIES);

	/* Two ping-pong PCM buffers, pre-filled with silence */
	for (i = 0; i < 2; i++) {
		if (dma_alloc(AC97_BUF_BYTES, 4, &sc->buf_dma[i]) != 0) {
			kprintf("ac97: PCM buffer %u alloc failed\n", i);
			return -1;
		}
		sc->buf[i] = (u_int8_t *)sc->buf_dma[i].db_vaddr;
		memset(sc->buf[i], 0, AC97_BUF_BYTES);

		sc->bdl[i].addr  = sc->buf_dma[i].db_paddr;
		sc->bdl[i].len   = AC97_BDL_LEN;
		sc->bdl[i].flags = AC97_BDL_IOC;
	}

	sc->lvi      = 1;
	sc->next_buf = 0;

	/* Reset the PCM out channel */
	nabm_wr8(sc, NABM_PO_BASE + NABM_CR, NABM_CR_RR);
	for (i = 0; i < 100; i++)
		if (!(nabm_rd8(sc, NABM_PO_BASE + NABM_CR) & NABM_CR_RR))
			break;

	/* Clear any stale status bits on all three DMA channels.
	 * QEMU sets BCIS on pi/mc when it fails to open those host streams;
	 * if we don't clear them here, IRQ 10 storms immediately after attach. */
	nabm_wr16(sc, 0x00 + NABM_SR, 0x001E);  /* PCM in  */
	nabm_wr16(sc, NABM_PO_BASE + NABM_SR, 0x001E);  /* PCM out */
	nabm_wr16(sc, 0x20 + NABM_SR, 0x001E);  /* Mic     */

	/* Point hardware at BDL and set LVI */
	nabm_wr32(sc, NABM_PO_BASE + NABM_BDL_ADDR, sc->bdl_dma.db_paddr);
	nabm_wr8(sc,  NABM_PO_BASE + NABM_LVI,      sc->lvi);

	/* Enable IOC interrupt and start DMA */
	nabm_wr8(sc, NABM_PO_BASE + NABM_CR,
	    NABM_CR_IOCE | NABM_CR_RPBM);

	sc->running = 1;
	kprintf("ac97: DMA started, BDL phys=0x%X lvi=%u\n",
	    sc->bdl_dma.db_paddr, sc->lvi);
	return 0;
}

/* -----------------------------------------------------------------------
 * Ring buffer write — called from devfs write path (process context)
 * --------------------------------------------------------------------- */

int
ac97_ring_write(const char *src, int len)
{
	struct ac97_softc *sc = &ac97_sc;
	u_int32_t free_space, write_len;

	if (!sc->running || len <= 0)
		return 0;

	/*
	 * Non-blocking partial write.  Returns the number of bytes actually
	 * written (0 to len).  Returns 0 when the ring is full so the caller
	 * can yield from user mode — where IF=1 — and let the ISR drain the
	 * ring.  Blocking here with sched_yield() would keep IF=0 (syscall
	 * entry clears IF and never re-enables it), starving the AC97 IRQ.
	 */
	free_space = (u_int32_t)(AC97_RING_SIZE) - (sc->ring_wr - sc->ring_rd);

	if (free_space == 0)
		return 0;

	write_len = (u_int32_t)len;
	if (write_len > free_space)
		write_len = free_space;

	{
		u_int32_t wr   = sc->ring_wr & AC97_RING_MASK;
		u_int32_t tail = AC97_RING_SIZE - wr;
		if (tail >= write_len) {
			memcpy(sc->ring + wr, src, write_len);
		} else {
			memcpy(sc->ring + wr, src,        tail);
			memcpy(sc->ring,      src + tail, write_len - tail);
		}
	}
	sc->ring_wr += write_len;
	return (int)write_len;
}

/* -----------------------------------------------------------------------
 * devfs char_write hook — bridges ubx_device write to ring buffer
 * --------------------------------------------------------------------- */

static int
ac97_dev_write(struct ubx_device *dev, const char *buf, int len)
{
	(void)dev;
	return ac97_ring_write(buf, len);
}

/* -----------------------------------------------------------------------
 * devfs char_ioctl hook — handles AUDIO_SET_RATE / AUDIO_GET_RATE
 * --------------------------------------------------------------------- */

static int
ac97_dev_ioctl(struct ubx_device *dev, u_int32_t cmd, void *arg)
{
	struct ac97_softc *sc = &ac97_sc;
	(void)dev;

	switch (cmd) {
	case AUDIO_SET_RATE:
		nam_wr16(sc, NAM_PCM_RATE, (u_int16_t)(*(u_int32_t *)arg));
		return (0);
	case AUDIO_GET_RATE:
		*(u_int32_t *)arg = nam_rd16(sc, NAM_PCM_RATE);
		return (0);
	}
	return (-1);
}

/* -----------------------------------------------------------------------
 * Probe / Attach
 * --------------------------------------------------------------------- */

static int
ac97_ubx_probe(struct ubx_device *dev)
{
	if (dev->dev_vendor == AC97_VENDOR && dev->dev_device_id == AC97_DEVICE)
		return 0;
	return -1;
}

static int
ac97_ubx_attach(struct ubx_device *dev)
{
	struct ac97_softc *sc = &ac97_sc;
	struct ubx_resource *res;
	u_int32_t i;

	kprintf("ac97: attaching (vendor=0x%X dev=0x%X)\n",
	    dev->dev_vendor, dev->dev_device_id);

	memset(sc, 0, sizeof(*sc));
	sc->sc_dev = dev;

	/* Extract BARs (NAM first, NABM second) and IRQ from resources */
	for (i = 0; i < (u_int32_t)dev->dev_nres; i++) {
		res = &dev->dev_res[i];
		if (res->r_type == UBX_RES_IOPORT) {
			if (sc->nam_base == 0)
				sc->nam_base = (u_int16_t)res->r_start;
			else if (sc->nabm_base == 0)
				sc->nabm_base = (u_int16_t)res->r_start;
		} else if (res->r_type == UBX_RES_IRQ && sc->irq == 0) {
			sc->irq = (u_int8_t)res->r_start;
		}
	}

	if (sc->nam_base == 0 || sc->nabm_base == 0) {
		kprintf("ac97: missing BARs (nam=0x%X nabm=0x%X)\n",
		    sc->nam_base, sc->nabm_base);
		return -1;
	}

	kprintf("ac97: NAM=0x%X NABM=0x%X IRQ=%u\n",
	    sc->nam_base, sc->nabm_base, sc->irq);

	/* Enable PCI I/O space (bit 0) and bus mastering (bit 2) */
	{
		u_int32_t cmd = pciRead(dev->dev_bus, dev->dev_slot,
		    dev->dev_func, 0x04, 2);
		pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func,
		    0x04, cmd | 0x05, 2);
	}

	/* Register the IRQ handler */
	irq_register(sc->irq, ac97_isr);

	/* Initialise codec and start DMA */
	if (ac97_codec_init(sc) != 0)
		return -1;
	if (ac97_dma_init(sc) != 0)
		return -1;

	/* Wire devfs hooks */
	dev->dev_char_write = ac97_dev_write;
	dev->dev_char_ioctl = ac97_dev_ioctl;
	ubx_device_register_block(dev, AC97_MAJOR, AC97_MINOR, NULL);
	devfs_makeNode("audio", 'c', AC97_MAJOR, AC97_MINOR);

	kprintf("ac97: /dev/audio ready\n");
	return 0;
}

/* -----------------------------------------------------------------------
 * Driver descriptor
 * --------------------------------------------------------------------- */

struct ubx_driver ac97_ubx_driver = {
	.drv_name   = "ac97",
	.drv_probe  = ac97_ubx_probe,
	.drv_attach = ac97_ubx_attach,
	.drv_detach = NULL,
};
