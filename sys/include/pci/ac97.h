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

#ifndef _PCI_AC97_H
#define _PCI_AC97_H

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/dma_mem.h>

/* -----------------------------------------------------------------------
 * PCI identity
 * --------------------------------------------------------------------- */
#define AC97_VENDOR 0x8086
#define AC97_DEVICE 0x2415

/* -----------------------------------------------------------------------
 * NAM (Native Audio Mixer) register offsets — relative to BAR0
 * --------------------------------------------------------------------- */
#define NAM_RESET 0x00      /* write to reset codec */
#define NAM_MASTER_VOL 0x02 /* master volume (0=full, 0x8000=mute) */
#define NAM_HP_VOL 0x04     /* headphone volume */
#define NAM_PCM_VOL 0x18    /* PCM out volume */
#define NAM_EXT_AUDIO 0x28  /* extended audio status / VRA capability */
#define NAM_EXT_VRA 0x0001  /* bit 0: enable variable rate audio */
#define NAM_PCM_RATE 0x2A   /* PCM front DAC rate (Hz) */

/* ioctl commands (shared with userland include/audio/audio.h) */
#define AUDIO_SET_RATE 0x4101
#define AUDIO_GET_RATE 0x4102
#define AUDIO_SET_VOLUME 0x4103 /* arg: u_int32_t* master volume 0..100 */
#define AUDIO_GET_VOLUME 0x4104 /* arg: u_int32_t* -> 0..100 */
#define AUDIO_SET_MUTE 0x4105   /* arg: u_int32_t* 0 = unmute, 1 = mute */
#define AUDIO_GET_MUTE 0x4106   /* arg: u_int32_t* -> 0/1 */

/* -----------------------------------------------------------------------
 * NABM (Native Audio Bus Master) register offsets — relative to BAR1
 * Channel bases:  PCM In = +0x00, PCM Out = +0x10, Mic = +0x20
 * --------------------------------------------------------------------- */
#define NABM_PO_BASE 0x10 /* PCM Out channel base */

/* Per-channel register offsets (add to channel base) */
#define NABM_BDL_ADDR 0x00 /* 32-bit physical address of BDL array */
#define NABM_CIV 0x04      /* current index value (RO, 8-bit) */
#define NABM_LVI 0x05      /* last valid index (RW, 8-bit, 0-31) */
#define NABM_SR 0x06       /* status register (RW, 16-bit) */
#define NABM_PICB 0x08     /* samples remaining in current buf (16-bit) */
#define NABM_PIV 0x0A      /* prefetched index value (RO, 8-bit) */
#define NABM_CR 0x0B       /* control register (RW, 8-bit) */

/* SR bits */
#define NABM_SR_BCIS (1 << 2)  /* buffer completion interrupt status */
#define NABM_SR_LVBCI (1 << 1) /* last valid buffer completion */
#define NABM_SR_CELV (1 << 0)  /* current == last valid */

/* CR bits */
#define NABM_CR_RPBM (1 << 0) /* run/pause DMA */
#define NABM_CR_RR (1 << 1)   /* reset channel registers */
#define NABM_CR_IOCE (1 << 4) /* interrupt-on-completion enable */

/* Global registers */
#define NABM_GLOB_CNT 0x2C /* global control (32-bit) */
#define NABM_GLOB_STA 0x30 /* global status (32-bit) */

#define NABM_GCNT_COLD (1 << 1)  /* cold reset release (operating) */
#define NABM_GSTA_CODEC (1 << 8) /* primary codec ready */

/* -----------------------------------------------------------------------
 * BDL entry — 8 bytes, must be 8-byte aligned
 * --------------------------------------------------------------------- */
#define AC97_BDL_ENTRIES 32
#define AC97_BDL_IOC (1 << 15) /* interrupt on completion */

struct ac97_bdle
{
	u_int32_t addr;  /* physical address of PCM buffer */
	u_int16_t len;   /* sample count (stereo 16-bit: bytes / 4) */
	u_int16_t flags; /* AC97_BDL_IOC etc. */
} __attribute__((packed));

/* -----------------------------------------------------------------------
 * Ping-pong buffer sizing
 *
 * AC97_BUF_FRAMES : stereo frames per DMA buffer (L+R pair = 4 bytes each)
 * AC97_BUF_BYTES  : bytes per DMA buffer = frames × 4
 * AC97_BDL_LEN    : value written to BDL ctl_len field.
 *                   The AC97 BDL length unit is one 16-bit sample (2 bytes).
 *                   QEMU reads (ctl_len × 2) bytes per entry, so for a
 *                   4096-byte buffer we need ctl_len = 4096 / 2 = 2048.
 *
 * AC97_RING_MASK is used ONLY for indexing (ring[idx & MASK]).
 * Never apply it to the distance (ring_wr - ring_rd) — that would make
 * a full ring (distance == RING_SIZE) look identical to an empty ring
 * (distance == 0).  Always compute distance as the raw u_int32_t difference.
 * --------------------------------------------------------------------- */
/*
 * Latency budget:
 *   DMA buffer  = 512 frames × 4 B = 2048 B = 10.7 ms @ 48 kHz
 *   Startup lag = 2 × 10.7 ms ≈ 21 ms  (two ping-pong buffers before first sound)
 *   Ring depth  = 8 × 2048 B = 16 KB ≈ 85 ms @ 48 kHz
 *   Total worst-case latency ≈ 106 ms — good for game sound effects.
 *
 *   DOOM at 35 fps writes ~5044 B per tick; with 2048-byte DMA buffers the
 *   non-integer ratio (5044/2048 = 2.46) leaves 6.23 ms margin before the
 *   next ISR fires after a game write — 14× more than the 256-frame layout.
 *   RING_SIZE = 8 × 2048 = 16384 = 2^14 (power of 2 for RING_MASK bitmask).
 *   The ISR uses memcpy (with wrap-around split) for minimum ISR overhead.
 */
#define AC97_BUF_FRAMES 512
#define AC97_BUF_BYTES (AC97_BUF_FRAMES * 4)
#define AC97_BDL_LEN (AC97_BUF_BYTES / 2) /* BDL len in 16-bit samples */
#define AC97_RING_BUFS 8                  /* 8 × 2 KB = 16 KB ≈ 85 ms @ 48 kHz */
#define AC97_RING_SIZE (AC97_BUF_BYTES * AC97_RING_BUFS)
#define AC97_RING_MASK (AC97_RING_SIZE - 1) /* index mask ONLY — not for distance */

/* -----------------------------------------------------------------------
 * Driver private state
 * --------------------------------------------------------------------- */
struct ac97_softc
{
	struct ubx_device *sc_dev;

	u_int16_t nam_base;  /* BAR0 I/O port base */
	u_int16_t nabm_base; /* BAR1 I/O port base */
	u_int8_t irq;

	struct dma_buf bdl_dma;
	struct ac97_bdle *bdl; /* virtual alias of bdl_dma */

	struct dma_buf buf_dma[2]; /* ping-pong PCM buffers */
	u_int8_t *buf[2];

	/* Kernel ring buffer written by sys_write, drained by ISR */
	u_int8_t ring[AC97_RING_SIZE];
	volatile u_int32_t ring_rd; /* modified by ISR */
	volatile u_int32_t ring_wr; /* modified by write path */

	u_int8_t next_buf; /* ping-pong index to fill next (0 or 1) */
	u_int8_t lvi;      /* current last-valid-index sent to hardware */
	u_int8_t running;  /* DMA started */

	u_int8_t vol_pct; /* master volume 0..100 (mixer state) */
	u_int8_t muted;   /* 1 = output muted */
};

/* Called from devfs write path */
int ac97_ring_write(const char *src, int len);

extern struct ubx_driver ac97_ubx_driver;

#endif /* _PCI_AC97_H */
