/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * virtio-sound (virtio-mmio) PCM playback for QEMU `virt` (Phase 15a).
 *
 * Brings up the virtio sound device (device id 25), opens one output PCM stream
 * at 44.1 kHz / signed-16-bit / stereo (what i_sound_ubix.c mixes), and exposes
 * it as the device-model char node /dev/audio with the same dev_char_write /
 * dev_char_ioctl contract the i386 AC'97 driver provides — so libaudio and
 * vDoom run unchanged.
 *
 *   controlq (queue 0): the PCM control lifecycle (PCM_INFO -> SET_PARAMS ->
 *                       PREPARE -> START), each a request/response pair.
 *   txq      (queue 2): PCM playback.  Each message is a 3-descriptor chain:
 *                       xfer header (stream_id, device-readable) -> PCM frames
 *                       (device-readable) -> status (device-writable).
 *
 * A ring of single-page period buffers is kept in flight; dev_char_write copies
 * a caller buffer into a free period and submits it, reclaiming completed
 * periods from the used ring.  When every period is busy it returns 0 (the
 * non-blocking contract libaudio's audio_write retries against).
 */

#include "bringup.h"
#include <sys/types.h>
#include <sys/bus.h>        /* struct ubx_device */
#include <sys/descrip.h>    /* g_device_find hook (devfs -> this device) */
#include <fs/devfs/devfs.h> /* devfs_makeNode */
#include <vmm/vmm.h>        /* vmm_find_free_page */
#include <lib/kmalloc.h>    /* sysID */
#include <lib/kprintf.h>
#include <string.h>

/* /dev/audio ioctls — mirror include/audio/audio.h (the userland header is not
 * on the kernel include path; the i386 AC'97 driver keeps the same copies in
 * pci/ac97.h). */
#define AUDIO_SET_RATE 0x4101
#define AUDIO_GET_RATE 0x4102
#define AUDIO_SET_VOLUME 0x4103
#define AUDIO_GET_VOLUME 0x4104
#define AUDIO_SET_MUTE 0x4105
#define AUDIO_GET_MUTE 0x4106

/* ---- virtio-mmio transport (shared layout with virtio_gpu.c) ----------- */
#define VIRTIO_MMIO_BASE 0x0A000000UL
#define VIRTIO_MMIO_SLOT 0x200UL
#define VIRTIO_MMIO_SLOTS 32

#define VMMIO_MAGIC 0x000
#define VMMIO_VERSION 0x004
#define VMMIO_DEVICE_ID 0x008
#define VMMIO_DRIVER_FEATURES 0x020
#define VMMIO_DRIVER_FEATURES_SEL 0x024
#define VMMIO_QUEUE_SEL 0x030
#define VMMIO_QUEUE_NUM_MAX 0x034
#define VMMIO_QUEUE_NUM 0x038
#define VMMIO_QUEUE_READY 0x044
#define VMMIO_QUEUE_NOTIFY 0x050
#define VMMIO_STATUS 0x070
#define VMMIO_QUEUE_DESC_LOW 0x080
#define VMMIO_QUEUE_DESC_HIGH 0x084
#define VMMIO_QUEUE_DRIVER_LOW 0x090
#define VMMIO_QUEUE_DRIVER_HIGH 0x094
#define VMMIO_QUEUE_DEVICE_LOW 0x0A0
#define VMMIO_QUEUE_DEVICE_HIGH 0x0A4
#define VMMIO_CONFIG 0x100

#define VMMIO_MAGIC_VALUE 0x74726976
#define VIRTIO_ID_SOUND 25

#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4
#define VSTAT_FEATURES_OK 8
#define VIRTIO_F_VERSION_1 32

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

#define QSIZE 64 /* descriptors per queue (>= 3 * N_PERIODS for txq) */

struct virtq_desc
{
	u_int64_t addr;
	u_int32_t len;
	u_int16_t flags;
	u_int16_t next;
};
struct virtq_avail
{
	u_int16_t flags;
	u_int16_t idx;
	u_int16_t ring[QSIZE];
};
struct virtq_used_elem
{
	u_int32_t id;
	u_int32_t len;
};
struct virtq_used
{
	u_int16_t flags;
	u_int16_t idx;
	struct virtq_used_elem ring[QSIZE];
};

/* ---- virtio-sound control protocol (virtio spec v1.2 §5.14) ------------ */
#define VIRTIO_SND_R_PCM_INFO 0x0100
#define VIRTIO_SND_R_PCM_SET_PARAMS 0x0101
#define VIRTIO_SND_R_PCM_PREPARE 0x0102
#define VIRTIO_SND_R_PCM_START 0x0104
#define VIRTIO_SND_S_OK 0x8000

#define VIRTIO_SND_PCM_FMT_S16 5
#define VIRTIO_SND_PCM_RATE_44100 6
#define VIRTIO_SND_D_OUTPUT 0

struct virtio_snd_hdr
{
	u_int32_t code;
};
struct virtio_snd_query_info
{
	struct virtio_snd_hdr hdr;
	u_int32_t start_id;
	u_int32_t count;
	u_int32_t size;
};
struct virtio_snd_pcm_hdr
{
	struct virtio_snd_hdr hdr;
	u_int32_t stream_id;
};
struct virtio_snd_pcm_set_params
{
	struct virtio_snd_pcm_hdr hdr;
	u_int32_t buffer_bytes;
	u_int32_t period_bytes;
	u_int32_t features;
	u_int8_t channels;
	u_int8_t format;
	u_int8_t rate;
	u_int8_t padding;
};
struct virtio_snd_pcm_xfer
{
	u_int32_t stream_id;
};
struct virtio_snd_pcm_status
{
	u_int32_t status;
	u_int32_t latency_bytes;
};

#define STREAM_ID 0
#define N_PERIODS 8
#define PERIOD_BYTES 4096 /* one page each, so the PCM buffer is contiguous */

/* ---- driver state ------------------------------------------------------ */
static volatile u_int8_t *g_base; /* this device's MMIO window */

/* controlq (queue 0) */
static struct virtq_desc *g_cq_desc;
static struct virtq_avail *g_cq_avail;
static struct virtq_used *g_cq_used;
static u_int16_t g_cq_last;
static u_int8_t *g_cmd;  /* control request buffer  */
static u_int8_t *g_resp; /* control response buffer */

/* txq (queue 2) */
static struct virtq_desc *g_tx_desc;
static struct virtq_avail *g_tx_avail;
static struct virtq_used *g_tx_used;
static u_int16_t g_tx_last;
static u_int8_t *g_period[N_PERIODS];       /* PCM data, one page each */
static struct virtio_snd_pcm_xfer *g_xfer;  /* N_PERIODS xfer headers */
static struct virtio_snd_pcm_status *g_pst; /* N_PERIODS status slots  */
static u_int8_t g_period_busy[N_PERIODS];   /* 1 while submitted to the device */

static int g_ready;
static u_int32_t g_rate = 44100;
static u_int32_t g_vol = 100;
static u_int32_t g_mute;

static inline u_int32_t mmio_rd(u_int32_t off)
{
	return *(volatile u_int32_t *)(g_base + off);
}
static inline void mmio_wr(u_int32_t off, u_int32_t val)
{
	*(volatile u_int32_t *)(g_base + off) = val;
}
static inline void dsb(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
}

/**
 * Program one virtqueue's address registers + size and mark it ready.  @qsel is
 * the queue index; @desc/@avail/@used are the (identity-mapped) ring addresses.
 */
static void setup_queue(u_int32_t qsel, void *desc, void *avail, void *used)
{
	mmio_wr(VMMIO_QUEUE_SEL, qsel);
	if (mmio_rd(VMMIO_QUEUE_NUM_MAX) < QSIZE)
		kprintf("virtio-snd: queue %u max < %u\n", qsel, QSIZE);
	mmio_wr(VMMIO_QUEUE_NUM, QSIZE);
	mmio_wr(VMMIO_QUEUE_DESC_LOW, (u_int32_t)(uintptr_t)desc);
	mmio_wr(VMMIO_QUEUE_DESC_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)desc >> 32));
	mmio_wr(VMMIO_QUEUE_DRIVER_LOW, (u_int32_t)(uintptr_t)avail);
	mmio_wr(VMMIO_QUEUE_DRIVER_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)avail >> 32));
	mmio_wr(VMMIO_QUEUE_DEVICE_LOW, (u_int32_t)(uintptr_t)used);
	mmio_wr(VMMIO_QUEUE_DEVICE_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)used >> 32));
	dsb();
	mmio_wr(VMMIO_QUEUE_READY, 1);
	dsb();
}

/**
 * Issue one control-queue command (request at g_cmd / @reqlen, response into
 * g_resp / @resplen) and busy-poll for completion.  The kernel is
 * non-preemptible, so a tight poll here is safe.
 *
 * @return the response status code (VIRTIO_SND_S_OK on success).
 */
static u_int32_t snd_ctl(u_int32_t reqlen, u_int32_t resplen)
{
	struct virtio_snd_hdr *resp = (struct virtio_snd_hdr *)g_resp;
	u_int16_t head = g_cq_avail->idx % QSIZE;

	g_cq_desc[0].addr = (u_int64_t)(uintptr_t)g_cmd;
	g_cq_desc[0].len = reqlen;
	g_cq_desc[0].flags = VIRTQ_DESC_F_NEXT;
	g_cq_desc[0].next = 1;
	g_cq_desc[1].addr = (u_int64_t)(uintptr_t)g_resp;
	g_cq_desc[1].len = resplen;
	g_cq_desc[1].flags = VIRTQ_DESC_F_WRITE;
	g_cq_desc[1].next = 0;

	g_cq_avail->ring[head] = 0;
	dsb();
	g_cq_avail->idx++;
	dsb();
	mmio_wr(VMMIO_QUEUE_NOTIFY, 0);

	/* Bounded poll: a working device completes in microseconds.  If the host
	 * audio backend is dead (QEMU "no host audio driver"), the device may never
	 * service the control queue — without this cap the non-preemptible kernel
	 * busy-spins forever and wedges boot.  On timeout return a non-OK status so
	 * the caller aborts init gracefully and the box boots without audio. */
	{
		u_int32_t spins = 0;
		while (g_cq_used->idx == g_cq_last)
		{
			if (++spins > 100000000u)
			{
				kprintf("virtio-snd: control queue timeout — device not responding, skipping audio\n");
				return (0xFFFFFFFFu); /* != VIRTIO_SND_S_OK */
			}
			dsb();
		}
	}
	g_cq_last++;
	dsb();
	return resp->code; /* response header's code field carries the status */
}

/**
 * Reclaim every period buffer the device has finished playing (drain the txq
 * used ring), clearing its busy flag so dev_char_write can reuse it.
 */
static void tx_reclaim(void)
{
	dsb();
	while (g_tx_used->idx != g_tx_last)
	{
		u_int32_t head = g_tx_used->ring[g_tx_last % QSIZE].id;
		int period = (int)(head / 3); /* 3 descriptors per submission */
		if (period >= 0 && period < N_PERIODS)
			g_period_busy[period] = 0;
		g_tx_last++;
	}
}

/**
 * Bring the device to DRIVER_OK with the control + tx virtqueues set up.
 *
 * @return 0 on success, -1 on failure.
 */
static int snd_init_device(void)
{
	uintptr_t cqpage, iopage, txpage, hdrpage;
	int i;

	mmio_wr(VMMIO_STATUS, 0);
	dsb();
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE);
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER);
	mmio_wr(VMMIO_DRIVER_FEATURES_SEL, 0);
	mmio_wr(VMMIO_DRIVER_FEATURES, 0);
	mmio_wr(VMMIO_DRIVER_FEATURES_SEL, 1);
	mmio_wr(VMMIO_DRIVER_FEATURES, 1u << (VIRTIO_F_VERSION_1 - 32));
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK);
	if ((mmio_rd(VMMIO_STATUS) & VSTAT_FEATURES_OK) == 0)
	{
		kprintf("virtio-snd: device rejected FEATURES_OK\n");
		return (-1);
	}

	cqpage = vmm_find_free_page(sysID);  /* controlq rings        */
	iopage = vmm_find_free_page(sysID);  /* control cmd + resp    */
	txpage = vmm_find_free_page(sysID);  /* txq rings             */
	hdrpage = vmm_find_free_page(sysID); /* xfer + status headers */
	if (cqpage == 0 || iopage == 0 || txpage == 0 || hdrpage == 0)
		return (-1);
	memset((void *)cqpage, 0, PAGE_SIZE);
	memset((void *)txpage, 0, PAGE_SIZE);
	memset((void *)hdrpage, 0, PAGE_SIZE);

	g_cq_desc = (struct virtq_desc *)(cqpage + 0);
	g_cq_avail = (struct virtq_avail *)(cqpage + 1280);
	g_cq_used = (struct virtq_used *)(cqpage + 2048);
	g_cmd = (u_int8_t *)(iopage + 0);
	g_resp = (u_int8_t *)(iopage + 2048);
	g_cq_last = 0;

	g_tx_desc = (struct virtq_desc *)(txpage + 0);
	g_tx_avail = (struct virtq_avail *)(txpage + 1280);
	g_tx_used = (struct virtq_used *)(txpage + 2048);
	g_tx_last = 0;
	g_xfer = (struct virtio_snd_pcm_xfer *)(hdrpage + 0);
	g_pst = (struct virtio_snd_pcm_status *)(hdrpage + 1024);
	for (i = 0; i < N_PERIODS; i++)
	{
		g_period[i] = (u_int8_t *)(uintptr_t)vmm_find_free_page(sysID);
		if (g_period[i] == 0)
			return (-1);
		g_xfer[i].stream_id = STREAM_ID;
		g_period_busy[i] = 0;
	}

	setup_queue(0, g_cq_desc, g_cq_avail, g_cq_used); /* controlq */
	setup_queue(2, g_tx_desc, g_tx_avail, g_tx_used); /* txq      */
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK | VSTAT_DRIVER_OK);
	dsb();
	return (0);
}

/**
 * Configure + start output PCM stream 0 at g_rate / S16 / stereo.
 *
 * @return 0 on success, -1 if any control command is rejected.
 */
static int snd_start_stream(void)
{
	struct virtio_snd_pcm_set_params *sp;
	struct virtio_snd_pcm_hdr *ph;

	sp = (struct virtio_snd_pcm_set_params *)g_cmd;
	memset(sp, 0, sizeof(*sp));
	sp->hdr.hdr.code = VIRTIO_SND_R_PCM_SET_PARAMS;
	sp->hdr.stream_id = STREAM_ID;
	sp->buffer_bytes = N_PERIODS * PERIOD_BYTES;
	sp->period_bytes = PERIOD_BYTES;
	sp->features = 0;
	sp->channels = 2;
	sp->format = VIRTIO_SND_PCM_FMT_S16;
	sp->rate = VIRTIO_SND_PCM_RATE_44100;
	if (snd_ctl(sizeof(*sp), sizeof(struct virtio_snd_hdr)) != VIRTIO_SND_S_OK)
	{
		kprintf("virtio-snd: SET_PARAMS rejected\n");
		return (-1);
	}

	ph = (struct virtio_snd_pcm_hdr *)g_cmd;
	memset(ph, 0, sizeof(*ph));
	ph->hdr.code = VIRTIO_SND_R_PCM_PREPARE;
	ph->stream_id = STREAM_ID;
	if (snd_ctl(sizeof(*ph), sizeof(struct virtio_snd_hdr)) != VIRTIO_SND_S_OK)
	{
		kprintf("virtio-snd: PREPARE rejected\n");
		return (-1);
	}

	ph->hdr.code = VIRTIO_SND_R_PCM_START;
	ph->stream_id = STREAM_ID;
	if (snd_ctl(sizeof(*ph), sizeof(struct virtio_snd_hdr)) != VIRTIO_SND_S_OK)
	{
		kprintf("virtio-snd: START rejected\n");
		return (-1);
	}
	return (0);
}

/**
 * dev_char_write hook (/dev/audio): queue one period of PCM for playback.
 * Copies up to PERIOD_BYTES into a free period buffer and submits it on the txq
 * as an xfer-header / data / status chain.  Non-blocking: returns 0 when every
 * period is still in flight (libaudio's audio_write retries).
 *
 * @return bytes accepted (0 if the ring is full).
 */
static int snd_dev_write(struct ubx_device *dev, const char *buf, int len)
{
	int period, base;
	u_int16_t head;
	size_t n;

	(void)dev;
	if (!g_ready || len <= 0)
		return (0);

	tx_reclaim();
	for (period = 0; period < N_PERIODS; period++)
		if (!g_period_busy[period])
			break;
	if (period == N_PERIODS)
		return (0); /* all periods busy — caller retries */

	n = (size_t)len < PERIOD_BYTES ? (size_t)len : PERIOD_BYTES;
	memcpy(g_period[period], buf, n);

	/* 3-descriptor chain: xfer header (r) -> PCM data (r) -> status (w). */
	base = period * 3;
	g_tx_desc[base + 0].addr = (u_int64_t)(uintptr_t)&g_xfer[period];
	g_tx_desc[base + 0].len = sizeof(struct virtio_snd_pcm_xfer);
	g_tx_desc[base + 0].flags = VIRTQ_DESC_F_NEXT;
	g_tx_desc[base + 0].next = base + 1;
	g_tx_desc[base + 1].addr = (u_int64_t)(uintptr_t)g_period[period];
	g_tx_desc[base + 1].len = (u_int32_t)n;
	g_tx_desc[base + 1].flags = VIRTQ_DESC_F_NEXT;
	g_tx_desc[base + 1].next = base + 2;
	g_tx_desc[base + 2].addr = (u_int64_t)(uintptr_t)&g_pst[period];
	g_tx_desc[base + 2].len = sizeof(struct virtio_snd_pcm_status);
	g_tx_desc[base + 2].flags = VIRTQ_DESC_F_WRITE;
	g_tx_desc[base + 2].next = 0;

	g_period_busy[period] = 1;
	head = g_tx_avail->idx % QSIZE;
	g_tx_avail->ring[head] = (u_int16_t)base;
	dsb();
	g_tx_avail->idx++;
	dsb();
	mmio_wr(VMMIO_QUEUE_NOTIFY, 2);
	return ((int)n);
}

/**
 * dev_char_ioctl hook (/dev/audio): the AUDIO_* control surface.  Rate is
 * accepted (the stream is fixed at 44.1 kHz, what the only caller mixes at);
 * volume/mute are tracked (virtio-sound has no master-volume control here).
 */
static int snd_dev_ioctl(struct ubx_device *dev, u_int32_t cmd, void *arg)
{
	(void)dev;
	switch (cmd)
	{
		case AUDIO_SET_RATE:
			g_rate = *(u_int32_t *)arg;
			return (0);
		case AUDIO_GET_RATE:
			*(u_int32_t *)arg = g_rate;
			return (0);
		case AUDIO_SET_VOLUME:
			g_vol = *(u_int32_t *)arg > 100 ? 100 : *(u_int32_t *)arg;
			return (0);
		case AUDIO_GET_VOLUME:
			*(u_int32_t *)arg = g_vol;
			return (0);
		case AUDIO_SET_MUTE:
			g_mute = *(u_int32_t *)arg ? 1 : 0;
			return (0);
		case AUDIO_GET_MUTE:
			*(u_int32_t *)arg = g_mute;
			return (0);
		default:
			break;
	}
	return (-1);
}

#define SND_MAJOR 20 /* /dev/audio (matches the AC'97 major; no conflict on aarch64) */
#define SND_MINOR 0

static struct ubx_device g_snd_dev;
static void *(*g_prev_device_find)(int major, int minor);

/**
 * g_device_find hook: resolve /dev/audio's (major, minor) to the sound device;
 * delegate everything else to the previously-installed hook (virtio-blk's), so
 * chaining adds the audio node without disturbing block-device lookup.
 */
static void *snd_device_find(int major, int minor)
{
	if (major == SND_MAJOR && minor == SND_MINOR)
		return (&g_snd_dev);
	return (g_prev_device_find != 0 ? g_prev_device_find(major, minor) : 0);
}

/**
 * Scan the virtio-mmio slots for a sound device, bring it up, start the output
 * stream, and register /dev/audio (device-model char node + write/ioctl hooks).
 *
 * @return 0 on success, -1 if no device / setup failed.
 */
int aarch64_virtio_sound_init(void)
{
	int slot;

	for (slot = 0; slot < VIRTIO_MMIO_SLOTS; slot++)
	{
		volatile u_int8_t *base = (volatile u_int8_t *)(VIRTIO_MMIO_BASE + (u_int64_t)slot * VIRTIO_MMIO_SLOT);

		if (*(volatile u_int32_t *)(base + VMMIO_MAGIC) != VMMIO_MAGIC_VALUE ||
		    *(volatile u_int32_t *)(base + VMMIO_DEVICE_ID) != VIRTIO_ID_SOUND)
			continue;

		g_base = base;
		if (snd_init_device() != 0 || snd_start_stream() != 0)
			return (-1);
		g_ready = 1;

		memset(&g_snd_dev, 0, sizeof(g_snd_dev));
		g_snd_dev.dev_char_write = snd_dev_write;
		g_snd_dev.dev_char_ioctl = snd_dev_ioctl;
		/* Chain the device-find hook so devfs_write's ubx_device_find resolves
		 * /dev/audio to this device (delegating other majors to virtio-blk's
		 * hook), then make the node. */
		g_prev_device_find = g_device_find;
		g_device_find = snd_device_find;
		devfs_makeNode("audio", 'c', SND_MAJOR, SND_MINOR);

		kprintf("virtio-snd: vtsnd0 at slot %d — /dev/audio ready (44.1kHz S16 stereo)\n", slot);
		return (0);
	}

	kprintf("virtio-snd: no sound device found in the virtio-mmio window\n");
	return (-1);
}
