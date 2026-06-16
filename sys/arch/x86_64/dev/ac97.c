/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 Intel 82801AA AC'97 audio driver (QEMU `-device AC97`, PCI 8086:2415).
 * Presents the device-model char node /dev/audio (major 20) with the same
 * dev_char_write / dev_char_ioctl contract as i386's sys/pci/ac97.c and aarch64's
 * dev/virtio_sound.c, so the `aural` mixer + libaudio veneer run unchanged.
 *
 * Why AC'97 (not virtio-sound): the bring-up speaks only LEGACY virtio-pci, but
 * QEMU's virtio-sound is modern-only (same situation as virtio-gpu, which is why
 * the framebuffer uses std-VGA).  AC'97 is a plain legacy PCI device with two
 * I/O BARs — a perfect fit for this arch's legacy-PCI model.
 *
 * Why POLL-based (no IRQ): the x86_64 bring-up wires only the timer IRQ; the
 * other device drivers (virtio-blk/net) and aarch64's whole virtio stack poll.
 * dev_char_write stages PCM into a software ring; each call then drains the ring
 * into the 32-entry buffer-descriptor ring, keeping a small window of buffers
 * queued just ahead of the play head (CIV) and advancing LVI to the last buffer
 * filled.  Writing LVI also resumes the engine from a halted (DCH) state, so an
 * underrun simply pauses output until the next write.  `aural` paces writes every
 * ~10 ms — far inside the queued window, so playback is continuous.
 *
 * Programming model (registers per the AC'97 spec):
 *   BAR0 (NAM)  — codec mixer registers (reset / volume / VRA / rate)
 *   BAR1 (NABM) — bus-master DMA control; PCM-out channel base = +0x10
 */

#include "../x86_64.h"
#include "pci.h"
#include <vmm/vmm.h>     /* vmm_find_free_pages_contig, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>
#include <sys/bus.h>     /* struct ubx_device */
#include <sys/descrip.h> /* g_device_find hook */
#include <fs/devfs/devfs.h>

/* PCI identity. */
#define AC97_VENDOR 0x8086
#define AC97_DEVICE 0x2415

/* NAM (mixer) register offsets — relative to BAR0. */
#define NAM_RESET 0x00
#define NAM_MASTER_VOL 0x02
#define NAM_PCM_VOL 0x18
#define NAM_EXT_AUDIO 0x28
#define NAM_EXT_VRA 0x0001
#define NAM_PCM_RATE 0x2A

/* NABM (bus-master) register offsets — relative to BAR1.  PCM-out base = 0x10. */
#define NABM_PO_BASE 0x10
#define NABM_BDL_ADDR 0x00 /* 32-bit phys of the BDL array */
#define NABM_CIV 0x04      /* current index value (RO, 8-bit) */
#define NABM_LVI 0x05      /* last valid index (RW, 8-bit, 0..31) */
#define NABM_SR 0x06       /* status (RW, 16-bit) */
#define NABM_CR 0x0B       /* control (RW, 8-bit) */
#define NABM_CR_RPBM (1 << 0)
#define NABM_CR_RR (1 << 1)
#define NABM_GLOB_CNT 0x2C
#define NABM_GLOB_STA 0x30
#define NABM_GCNT_COLD (1 << 1)
#define NABM_GSTA_CODEC (1 << 8)

/* ioctl commands (shared with userland include/audio/audio.h). */
#define AUDIO_SET_RATE 0x4101
#define AUDIO_GET_RATE 0x4102
#define AUDIO_SET_VOLUME 0x4103
#define AUDIO_GET_VOLUME 0x4104
#define AUDIO_SET_MUTE 0x4105
#define AUDIO_GET_MUTE 0x4106

/* BDL ring + buffer sizing.  512 stereo S16 frames/buffer = 2048 B ≈ 10.7 ms @
 * 48 kHz; 32 buffers form the hardware ring.  The BDL length field counts 16-bit
 * samples (bytes / 2).  Software ring decouples the caller's write size from the
 * DMA buffer size. */
#define AC97_BDL_ENTRIES 32
#define AC97_BDL_IOC (1 << 15)
#define AC97_BUF_BYTES 2048
#define AC97_BDL_LEN (AC97_BUF_BYTES / 2)
#define AC97_RING_SIZE (AC97_BUF_BYTES * 8) /* 16 KB ≈ 85 ms @ 48 kHz */

#define AC97_MAJOR 20
#define AC97_MINOR 0

struct ac97_bdle
{
	u32 addr;  /* phys of the PCM buffer */
	u16 len;   /* length in 16-bit samples */
	u16 flags; /* AC97_BDL_IOC etc. */
} __attribute__((packed));

static u16 g_nam;  /* BAR0 I/O base (mixer) */
static u16 g_nabm; /* BAR1 I/O base (bus-master) */
static int g_ready;

static struct ac97_bdle *g_bdl;     /* 32 descriptors (physmap alias) */
static u32 g_bdl_phys;              /* BDL physical address */
static u8 *g_buf[AC97_BDL_ENTRIES]; /* per-entry PCM buffer (physmap alias) */
static u32 g_buf_phys[AC97_BDL_ENTRIES];
static u8 g_head;    /* next buffer index to fill with fresh PCM (kept ahead of CIV) */
static u8 g_started; /* RPBM engaged (DMA running) */

/* How many buffers of real data to keep queued ahead of the play head.  Bounds
 * output latency to AC97_FILL_WINDOW x ~10.7 ms (about 85 ms); deeper than
 * `aural`'s ~10 ms feed cadence so the engine never drains it between writes. */
#define AC97_FILL_WINDOW 8

static u8 g_ring[AC97_RING_SIZE];
static u32 g_ring_rd;
static u32 g_ring_wr;

static u8 g_vol_pct = 100;
static u8 g_muted;
static u32 g_rate = 48000;

static struct ubx_device g_audio_dev;
static void *(*g_prev_device_find)(int major, int minor);

/* ---- register accessors ------------------------------------------------- */

static inline u16 nam_rd(u16 reg)
{
	return inw((u16)(g_nam + reg));
}
static inline void nam_wr(u16 reg, u16 val)
{
	outw((u16)(g_nam + reg), val);
}
static inline u8 nabm_rd8(u16 reg)
{
	return inb((u16)(g_nabm + reg));
}
static inline void nabm_wr8(u16 reg, u8 val)
{
	outb((u16)(g_nabm + reg), val);
}
static inline void nabm_wr16(u16 reg, u16 val)
{
	outw((u16)(g_nabm + reg), val);
}
static inline u32 nabm_rd32(u16 reg)
{
	return inl((u16)(g_nabm + reg));
}
static inline void nabm_wr32(u16 reg, u32 val)
{
	outl((u16)(g_nabm + reg), val);
}

/* ---- codec + DMA setup -------------------------------------------------- */

/**
 * Bring the primary codec out of cold reset, unmute, enable variable-rate audio,
 * and set the default 48 kHz output rate.  @return 0 on success, -1 if the codec
 * never reports ready.
 */
static int ac97_codec_init(void)
{
	int i;

	nabm_wr32(NABM_GLOB_CNT, NABM_GCNT_COLD);
	for (i = 0; i < 1000; i++)
	{
		if (nabm_rd32(NABM_GLOB_STA) & NABM_GSTA_CODEC)
			break;
		inb(0x80); /* ~1 µs I/O delay */
	}
	if (!(nabm_rd32(NABM_GLOB_STA) & NABM_GSTA_CODEC))
	{
		serial_puts("ac97: codec not ready\n");
		return -1;
	}

	nam_wr(NAM_RESET, 0xFFFF);
	nam_wr(NAM_MASTER_VOL, 0x0000); /* unmute, full volume */
	nam_wr(NAM_PCM_VOL, 0x0000);
	nam_wr(NAM_EXT_AUDIO, (u16)(nam_rd(NAM_EXT_AUDIO) | NAM_EXT_VRA));
	nam_wr(NAM_PCM_RATE, (u16)g_rate);
	return 0;
}

/**
 * Allocate the BDL + 32 PCM buffers (one contiguous physical run, accessed by the
 * CPU through the physmap and by the device through their physical addresses),
 * pre-fill them with silence, point the PCM-out channel at the BDL, and start the
 * DMA engine.  @return 0 on success, -1 on allocation failure.
 */
static int ac97_dma_init(void)
{
	/* 1 page BDL + 16 pages of buffers (2 × 2048-byte buffers per 4 KB page). */
	u32 bufs_pages = (AC97_BDL_ENTRIES * AC97_BUF_BYTES + PAGE_SIZE - 1) / PAGE_SIZE;
	uintptr_t base = vmm_find_free_pages_contig(1 + bufs_pages, sysID);
	uintptr_t bufbase;
	int i;

	if (base == 0)
	{
		serial_puts("ac97: DMA alloc failed\n");
		return -1;
	}

	g_bdl = (struct ac97_bdle *)P2V(base);
	g_bdl_phys = (u32)base;
	memset(g_bdl, 0, AC97_BDL_ENTRIES * sizeof(struct ac97_bdle));

	bufbase = base + PAGE_SIZE;
	for (i = 0; i < AC97_BDL_ENTRIES; i++)
	{
		uintptr_t phys = bufbase + (uintptr_t)i * AC97_BUF_BYTES;
		g_buf[i] = (u8 *)P2V(phys);
		g_buf_phys[i] = (u32)phys;
		memset(g_buf[i], 0, AC97_BUF_BYTES);
		g_bdl[i].addr = g_buf_phys[i];
		g_bdl[i].len = AC97_BDL_LEN;
		g_bdl[i].flags = AC97_BDL_IOC;
	}
	g_head = 0;
	g_started = 0;

	/* Reset the PCM-out channel and clear any stale status bits. */
	nabm_wr8(NABM_PO_BASE + NABM_CR, NABM_CR_RR);
	for (i = 0; i < 100; i++)
		if (!(nabm_rd8(NABM_PO_BASE + NABM_CR) & NABM_CR_RR))
			break;
	nabm_wr16(NABM_PO_BASE + NABM_SR, 0x001E);

	/* Point the engine at the BDL but do NOT start it yet (no IOCE — we poll CIV,
	 * never take an interrupt).  ac97_feed engages RPBM once the first real PCM is
	 * queued, so playback begins on actual data rather than a lap of silence. */
	nabm_wr32(NABM_PO_BASE + NABM_BDL_ADDR, g_bdl_phys);
	nabm_wr8(NABM_PO_BASE + NABM_LVI, 0);
	return 0;
}

/* ---- PCM feed (poll-based) ---------------------------------------------- */

/**
 * Drain up to AC97_BUF_BYTES from the software ring into @dst, zero-padding the
 * remainder of the descriptor's fixed-size buffer (so a short final period plays
 * cleanly rather than replaying stale audio).  @return bytes drained from the
 * ring (0 means the ring was empty — nothing to queue).
 */
static u32 fill_from_ring(u8 *dst)
{
	u32 avail = g_ring_wr - g_ring_rd;

	if (avail > (u32)AC97_BUF_BYTES)
		avail = AC97_BUF_BYTES;
	if (avail > 0)
	{
		u32 rd = g_ring_rd % AC97_RING_SIZE;
		u32 tail = AC97_RING_SIZE - rd;
		if (tail >= avail)
		{
			memcpy(dst, g_ring + rd, avail);
		}
		else
		{
			memcpy(dst, g_ring + rd, tail);
			memcpy(dst + tail, g_ring, avail - tail);
		}
		g_ring_rd += avail;
		if (avail < (u32)AC97_BUF_BYTES)
			memset(dst + avail, 0, AC97_BUF_BYTES - avail);
	}
	return avail;
}

/**
 * Keep a window of real PCM queued just ahead of the play head.  Reads the
 * current index CIV; if the queue write head has fallen behind (a prior
 * underrun let the engine catch up), it snaps back to one buffer ahead of CIV.
 * Then it drains the software ring into successive buffers — up to
 * AC97_FILL_WINDOW ahead of CIV — advancing LVI to the last buffer filled and
 * engaging the DMA engine on the first real data.  Writing LVI while RPBM is
 * set also resumes the engine from a halted (DCH) state after an underrun.
 */
static void ac97_feed(void)
{
	u8 civ = nabm_rd8(NABM_PO_BASE + NABM_CIV);
	u8 ahead = (u8)((g_head - civ) & (AC97_BDL_ENTRIES - 1));
	u8 last = g_head;
	int filled = 0;

	/* If the engine has played past our write head (or hasn't started), restart
	 * queuing one buffer ahead of where it is now. */
	if (ahead == 0 || ahead > AC97_FILL_WINDOW)
		g_head = (u8)((civ + 1) & (AC97_BDL_ENTRIES - 1));

	while ((g_ring_wr - g_ring_rd) > 0 && (u8)((g_head - civ) & (AC97_BDL_ENTRIES - 1)) <= AC97_FILL_WINDOW)
	{
		if (fill_from_ring(g_buf[g_head]) == 0)
			break;
		last = g_head;
		g_head = (u8)((g_head + 1) & (AC97_BDL_ENTRIES - 1));
		filled = 1;
	}

	if (filled)
	{
		nabm_wr8(NABM_PO_BASE + NABM_LVI, last);
		if (!g_started)
		{
			nabm_wr8(NABM_PO_BASE + NABM_CR, NABM_CR_RPBM);
			g_started = 1;
		}
	}
}

/**
 * dev_char_write hook (/dev/audio): stage PCM into the software ring, then feed
 * the DMA buffers.  Non-blocking: returns the number of bytes accepted (0 when
 * the ring is full, so libaudio/aural retries after yielding).
 */
static int ac97_dev_write(struct ubx_device *dev, const char *buf, int len)
{
	u32 free_space, n;

	(void)dev;
	if (!g_ready || len <= 0)
		return 0;

	free_space = (u32)AC97_RING_SIZE - (g_ring_wr - g_ring_rd);
	if (free_space == 0)
	{
		ac97_feed(); /* try to make room from already-played buffers */
		free_space = (u32)AC97_RING_SIZE - (g_ring_wr - g_ring_rd);
		if (free_space == 0)
			return 0;
	}

	n = (u32)len;
	if (n > free_space)
		n = free_space;
	{
		u32 wr = g_ring_wr % AC97_RING_SIZE;
		u32 tail = AC97_RING_SIZE - wr;
		if (tail >= n)
		{
			memcpy(g_ring + wr, buf, n);
		}
		else
		{
			memcpy(g_ring + wr, buf, tail);
			memcpy(g_ring, buf + tail, n - tail);
		}
	}
	g_ring_wr += n;

	ac97_feed();
	return (int)n;
}

/**
 * Apply the cached volume/mute to the codec.  AC'97 master/PCM volume is a 6-bit
 * per-channel attenuation (0 = loudest, 0x3F = silent) with bit 15 as mute.
 */
static void ac97_apply_volume(void)
{
	u16 v;

	if (g_muted || g_vol_pct == 0)
	{
		v = 0x8000;
	}
	else
	{
		u8 pct = g_vol_pct > 100 ? 100 : g_vol_pct;
		u8 atten = (u8)(((100 - pct) * 0x3F) / 100);
		v = (u16)((atten << 8) | atten);
	}
	nam_wr(NAM_MASTER_VOL, v);
	nam_wr(NAM_PCM_VOL, v);
}

/**
 * dev_char_ioctl hook (/dev/audio): the AUDIO_* control surface (rate / volume /
 * mute).  @return 0 on a handled command, -1 otherwise.
 */
static int ac97_dev_ioctl(struct ubx_device *dev, u_int32_t cmd, void *arg)
{
	(void)dev;
	switch (cmd)
	{
		case AUDIO_SET_RATE:
			g_rate = *(u32 *)arg;
			nam_wr(NAM_PCM_RATE, (u16)g_rate);
			return 0;
		case AUDIO_GET_RATE:
			*(u32 *)arg = nam_rd(NAM_PCM_RATE);
			return 0;
		case AUDIO_SET_VOLUME:
			g_vol_pct = (u8)(*(u32 *)arg > 100 ? 100 : *(u32 *)arg);
			ac97_apply_volume();
			return 0;
		case AUDIO_GET_VOLUME:
			*(u32 *)arg = g_vol_pct;
			return 0;
		case AUDIO_SET_MUTE:
			g_muted = *(u32 *)arg ? 1 : 0;
			ac97_apply_volume();
			return 0;
		case AUDIO_GET_MUTE:
			*(u32 *)arg = g_muted;
			return 0;
		default:
			break;
	}
	return -1;
}

/* ---- device-model registration ------------------------------------------ */

/**
 * The devfs char/block lookup.  The MI bus.c device table isn't linked on the
 * x86_64 bring-up, but devfs + drivers call ubx_device_find() directly, so
 * delegate to the g_device_find hook (which virtio-blk and this driver install).
 * Mirrors aarch64 ksupport; replaces the former -1 stub in syscall_stubs.c.
 */
struct ubx_device *ubx_device_find(int major, int minor)
{
	if (g_device_find == 0)
		return 0;
	return (struct ubx_device *)g_device_find(major, minor);
}

/**
 * g_device_find hook: resolve /dev/audio's (major, minor) to the AC'97 device;
 * delegate everything else (the virtio-blk node) to the previously-installed
 * hook.  Mirrors aarch64's snd_device_find chaining.
 */
static void *ac97_device_find(int major, int minor)
{
	if (major == AC97_MAJOR && minor == AC97_MINOR)
		return &g_audio_dev;
	return g_prev_device_find != 0 ? g_prev_device_find(major, minor) : 0;
}

/**
 * Probe the PCI bus for QEMU's AC'97, bring up the codec + DMA, and register the
 * /dev/audio char node.  @return 0 on success, -1 if absent / setup failed.
 */
int x86_64_ac97_init(void)
{
	struct pci_dev d;

	if (!pci_find_device(AC97_VENDOR, AC97_DEVICE, &d))
	{
		serial_puts("ac97: no AC97 device on the PCI bus\n");
		return -1;
	}

	/* Both BARs are I/O space (low bit set); mask it off for the port base. */
	g_nam = (u16)(d.bar[0] & ~0x3u);
	g_nabm = (u16)(d.bar[1] & ~0x3u);
	if (g_nam == 0 || g_nabm == 0)
	{
		serial_puts("ac97: missing I/O BARs\n");
		return -1;
	}
	pci_enable_io_busmaster(&d);

	if (ac97_codec_init() != 0 || ac97_dma_init() != 0)
		return -1;
	g_ready = 1;

	memset(&g_audio_dev, 0, sizeof(g_audio_dev));
	g_audio_dev.dev_char_write = ac97_dev_write;
	g_audio_dev.dev_char_ioctl = ac97_dev_ioctl;
	strncpy(g_audio_dev.dev_nameunit, "ac97", sizeof(g_audio_dev.dev_nameunit) - 1);

	g_prev_device_find = g_device_find;
	g_device_find = ac97_device_find;
	devfs_makeNode("audio", 'c', AC97_MAJOR, AC97_MINOR);

	serial_puts("ac97: /dev/audio ready (48kHz S16 stereo, NAM ");
	serial_puthex(g_nam);
	serial_puts(" NABM ");
	serial_puthex(g_nabm);
	serial_puts(")\n");
	return 0;
}
