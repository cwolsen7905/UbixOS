/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * virtio-mmio + virtio-gpu (2D) driver — QEMU `virt`, aarch64 bring-up.
 *
 * Sibling of virtio_blk.c / virtio_net.c (same modern v2 virtio-mmio transport,
 * same split-virtqueue + polling model).  virtio-gpu drives the display over a
 * single control virtqueue with a request/response command protocol:
 *
 *   GET_DISPLAY_INFO  -> read the preferred scanout resolution
 *   RESOURCE_CREATE_2D-> create a host-side framebuffer resource
 *   ATTACH_BACKING    -> back it with a contiguous guest RAM buffer (the fb apps
 *                        and views draw into)
 *   SET_SCANOUT       -> connect the resource to scanout 0 (the display)
 *   TRANSFER_TO_HOST_2D + RESOURCE_FLUSH -> push the guest fb to the screen
 *
 * The backing buffer is the linear framebuffer: B8G8R8X8 (0x00RRGGBB little-
 * endian), matching the i386 VESA LFB layout objGFX expects.  virtio_gpu_flush()
 * is what the display layer calls to present a frame.
 */

#include "bringup.h"
#include <sys/types.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>   /* sysID */
#include <lib/kprintf.h>   /* kprintf */
#include <ubixos/vitals.h> /* systemVitals->sysTicks — GPU-command stall detector */
#include <string.h>

/* QEMU `virt` virtio-mmio window: 32 slots of 0x200 bytes at 0x0a000000. */
#define VIRTIO_MMIO_BASE (PHYSMAP_BASE + 0x0A000000UL) /* device regs via TTBR1 physmap (Phase 4) */
#define VIRTIO_MMIO_SLOT 0x200UL
#define VIRTIO_MMIO_SLOTS 32

#define VMMIO_MAGIC 0x000
#define VMMIO_VERSION 0x004
#define VMMIO_DEVICE_ID 0x008
#define VMMIO_DEVICE_FEATURES 0x010
#define VMMIO_DEVICE_FEATURES_SEL 0x014
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

#define VMMIO_MAGIC_VALUE 0x74726976
#define VIRTIO_ID_GPU 16

#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4
#define VSTAT_FEATURES_OK 8
#define VIRTIO_F_VERSION_1 32

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define QSIZE 8

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

/* ---- virtio-gpu 2D command protocol ----------------------------------- */

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO 0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D 0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF 0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT 0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH 0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106

#define VIRTIO_GPU_RESP_OK_NODATA 0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101

#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2 /* 0x00RRGGBB LE — matches the i386 LFB */
#define VIRTIO_GPU_MAX_SCANOUTS 16
#define GPU_RESOURCE_ID 1

struct virtio_gpu_ctrl_hdr
{
	u_int32_t type;
	u_int32_t flags;
	u_int64_t fence_id;
	u_int32_t ctx_id;
	u_int32_t padding;
};

struct virtio_gpu_rect
{
	u_int32_t x;
	u_int32_t y;
	u_int32_t width;
	u_int32_t height;
};

struct virtio_gpu_resp_display_info
{
	struct virtio_gpu_ctrl_hdr hdr;
	struct
	{
		struct virtio_gpu_rect r;
		u_int32_t enabled;
		u_int32_t flags;
	} pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};

struct virtio_gpu_resource_create_2d
{
	struct virtio_gpu_ctrl_hdr hdr;
	u_int32_t resource_id;
	u_int32_t format;
	u_int32_t width;
	u_int32_t height;
};

struct virtio_gpu_mem_entry
{
	u_int64_t addr;
	u_int32_t length;
	u_int32_t padding;
};

struct virtio_gpu_resource_attach_backing
{
	struct virtio_gpu_ctrl_hdr hdr;
	u_int32_t resource_id;
	u_int32_t nr_entries;
	struct virtio_gpu_mem_entry entry; /* single contiguous backing */
};

struct virtio_gpu_set_scanout
{
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	u_int32_t scanout_id;
	u_int32_t resource_id;
};

struct virtio_gpu_transfer_to_host_2d
{
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	u_int64_t offset;
	u_int32_t resource_id;
	u_int32_t padding;
};

struct virtio_gpu_resource_flush
{
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	u_int32_t resource_id;
	u_int32_t padding;
};

/* Driver state. */
static volatile u_int8_t *g_base;
static struct virtq_desc *g_desc;
static struct virtq_avail *g_avail;
static struct virtq_used *g_used;
static u_int16_t g_last_used;
static u_int8_t *g_cmd;  /* request DMA buffer */
static u_int8_t *g_resp; /* response DMA buffer */
static int g_ready;

u_int8_t *virtio_gpu_fb;     /* linear framebuffer (B8G8R8X8) */
u_int32_t virtio_gpu_width;  /* scanout width  */
u_int32_t virtio_gpu_height; /* scanout height */
u_int32_t virtio_gpu_pitch;  /* bytes per row (width * 4) */

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
 * Issue one control-queue command: a read descriptor (the request at g_cmd,
 * @reqlen bytes) chained to a write descriptor (the response into g_resp,
 * @resplen bytes); notify the device and poll the used ring.
 *
 * @return the response type word (VIRTIO_GPU_RESP_*), or 0 on a ring error.
 */
static u_int32_t gpu_cmd(u_int32_t reqlen, u_int32_t resplen)
{
	struct virtio_gpu_ctrl_hdr *resp = (struct virtio_gpu_ctrl_hdr *)g_resp;

	g_desc[0].addr = AARCH64_PHYS_OF((uintptr_t)g_cmd);
	g_desc[0].len = reqlen;
	g_desc[0].flags = VIRTQ_DESC_F_NEXT;
	g_desc[0].next = 1;
	g_desc[1].addr = AARCH64_PHYS_OF((uintptr_t)g_resp);
	g_desc[1].len = resplen;
	g_desc[1].flags = VIRTQ_DESC_F_WRITE;
	g_desc[1].next = 0;

	g_avail->ring[g_avail->idx % QSIZE] = 0;
	dsb();
	g_avail->idx++;
	dsb();

	/* Stall detector: time the notify+poll round trip in scheduler ticks.  Under
	 * HVF the QUEUE_NOTIFY MMIO write traps and QEMU services the queue
	 * synchronously, so a healthy command completes in 0 ticks; anything that
	 * crosses a tick boundary (~10 ms at 100 Hz) is abnormal and worth logging.
	 * Silent on the fast path, so it can stay in for the intermittent-slow-boot
	 * hunt without flooding serial.log. */
	u_int32_t t0 = systemVitals ? systemVitals->sysTicks : 0;

	mmio_wr(VMMIO_QUEUE_NOTIFY, 0);

	while (g_used->idx == g_last_used)
		dsb();
	g_last_used++;
	dsb();

	{
		u_int32_t dt = (systemVitals ? systemVitals->sysTicks : 0) - t0;
		if (dt >= 2)
			kprintf("virtio-gpu: SLOW cmd type=0x%X waited %u ticks (resp=0x%X)\n",
			        ((struct virtio_gpu_ctrl_hdr *)g_cmd)->type,
			        dt,
			        resp->type);
	}
	return resp->type;
}

/**
 * Fill the request buffer header (type + zeroed fields) before a command.
 */
static void cmd_hdr(u_int32_t type)
{
	struct virtio_gpu_ctrl_hdr *h = (struct virtio_gpu_ctrl_hdr *)g_cmd;
	memset(g_cmd, 0, 256);
	h->type = type;
}

/**
 * Present the current framebuffer contents: copy the guest backing to the host
 * resource then flush it to scanout 0.  Called by the display layer per frame.
 */
void virtio_gpu_flush(void)
{
	struct virtio_gpu_transfer_to_host_2d *xfer;
	struct virtio_gpu_resource_flush *fl;

	if (!g_ready)
		return;

	cmd_hdr(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
	xfer = (struct virtio_gpu_transfer_to_host_2d *)g_cmd;
	xfer->r.x = 0;
	xfer->r.y = 0;
	xfer->r.width = virtio_gpu_width;
	xfer->r.height = virtio_gpu_height;
	xfer->offset = 0;
	xfer->resource_id = GPU_RESOURCE_ID;
	gpu_cmd(sizeof(*xfer), sizeof(struct virtio_gpu_ctrl_hdr));

	cmd_hdr(VIRTIO_GPU_CMD_RESOURCE_FLUSH);
	fl = (struct virtio_gpu_resource_flush *)g_cmd;
	fl->r.x = 0;
	fl->r.y = 0;
	fl->r.width = virtio_gpu_width;
	fl->r.height = virtio_gpu_height;
	fl->resource_id = GPU_RESOURCE_ID;
	gpu_cmd(sizeof(*fl), sizeof(struct virtio_gpu_ctrl_hdr));
}

/**
 * Set up the control virtqueue and bring the device to DRIVER_OK.
 *
 * @return 0 on success, -1 on failure.
 */
static int gpu_setup_queue(void)
{
	uintptr_t qpage, iopage;

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
		kprintf("virtio-gpu: device rejected FEATURES_OK\n");
		return (-1);
	}

	mmio_wr(VMMIO_QUEUE_SEL, 0); /* controlq */
	if (mmio_rd(VMMIO_QUEUE_NUM_MAX) == 0)
	{
		kprintf("virtio-gpu: controlq unavailable\n");
		return (-1);
	}
	mmio_wr(VMMIO_QUEUE_NUM, QSIZE);

	qpage = vmm_find_free_page(sysID);
	iopage = vmm_find_free_page(sysID);
	if (qpage == 0 || iopage == 0)
		return (-1);
	/* Access the vring + cmd/resp buffers via the physmap (Convention B); the
	 * device gets PHYSICAL qpage/iopage in the queue registers + descriptor .addr. */
	memset((void *)(uintptr_t)AARCH64_VIRT_OF(qpage), 0, PAGE_SIZE);
	memset((void *)(uintptr_t)AARCH64_VIRT_OF(iopage), 0, PAGE_SIZE);

	g_desc = (struct virtq_desc *)(uintptr_t)AARCH64_VIRT_OF(qpage + 0);
	g_avail = (struct virtq_avail *)(uintptr_t)AARCH64_VIRT_OF(qpage + 256);
	g_used = (struct virtq_used *)(uintptr_t)AARCH64_VIRT_OF(qpage + 512);
	g_cmd = (u_int8_t *)(uintptr_t)AARCH64_VIRT_OF(iopage + 0);
	g_resp = (u_int8_t *)(uintptr_t)AARCH64_VIRT_OF(iopage + 2048);
	g_last_used = 0;

	mmio_wr(VMMIO_QUEUE_DESC_LOW, (u_int32_t)qpage);
	mmio_wr(VMMIO_QUEUE_DESC_HIGH, (u_int32_t)((u_int64_t)qpage >> 32));
	mmio_wr(VMMIO_QUEUE_DRIVER_LOW, (u_int32_t)(qpage + 256));
	mmio_wr(VMMIO_QUEUE_DRIVER_HIGH, (u_int32_t)((u_int64_t)(qpage + 256) >> 32));
	mmio_wr(VMMIO_QUEUE_DEVICE_LOW, (u_int32_t)(qpage + 512));
	mmio_wr(VMMIO_QUEUE_DEVICE_HIGH, (u_int32_t)((u_int64_t)(qpage + 512) >> 32));
	dsb();
	mmio_wr(VMMIO_QUEUE_READY, 1);
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK | VSTAT_DRIVER_OK);
	dsb();
	return (0);
}

/**
 * Query the preferred resolution, create a 2D resource backed by a contiguous
 * framebuffer, and connect it to scanout 0.
 *
 * @return 0 on success, -1 on any command failure.
 */
static int gpu_setup_display(void)
{
	struct virtio_gpu_resp_display_info *di;
	struct virtio_gpu_resource_create_2d *c2d;
	struct virtio_gpu_resource_attach_backing *ab;
	struct virtio_gpu_set_scanout *ss;
	u_int32_t w, h, fbpages;
	uintptr_t fbphys;

	/* GET_DISPLAY_INFO */
	cmd_hdr(VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
	if (gpu_cmd(sizeof(struct virtio_gpu_ctrl_hdr), sizeof(struct virtio_gpu_resp_display_info)) !=
	    VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
	{
		kprintf("virtio-gpu: GET_DISPLAY_INFO failed\n");
		return (-1);
	}
	di = (struct virtio_gpu_resp_display_info *)g_resp;
	w = di->pmodes[0].r.width;
	h = di->pmodes[0].r.height;
	if (w == 0 || h == 0)
	{
		w = 1024;
		h = 768;
	}

	fbpages = (w * h * 4 + PAGE_SIZE - 1) / PAGE_SIZE;
	fbphys = vmm_find_free_pages_contig(fbpages, sysID);
	if (fbphys == 0)
	{
		kprintf("virtio-gpu: framebuffer alloc (%u pages) failed\n", fbpages);
		return (-1);
	}
	/* fbphys stays physical for the device backing (ATTACH_BACKING below).  The
	 * kernel reaches the fb (fbcon draws on it, this memset) via the physmap;
	 * sys_mapfb converts virtio_gpu_fb back to physical to map it into userland. */
	memset((void *)(uintptr_t)AARCH64_VIRT_OF(fbphys), 0, (size_t)fbpages * PAGE_SIZE);
	virtio_gpu_fb = (u_int8_t *)(uintptr_t)AARCH64_VIRT_OF(fbphys);
	virtio_gpu_width = w;
	virtio_gpu_height = h;
	virtio_gpu_pitch = w * 4;

	/* RESOURCE_CREATE_2D */
	cmd_hdr(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
	c2d = (struct virtio_gpu_resource_create_2d *)g_cmd;
	c2d->resource_id = GPU_RESOURCE_ID;
	c2d->format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
	c2d->width = w;
	c2d->height = h;
	if (gpu_cmd(sizeof(*c2d), sizeof(struct virtio_gpu_ctrl_hdr)) != VIRTIO_GPU_RESP_OK_NODATA)
	{
		kprintf("virtio-gpu: RESOURCE_CREATE_2D failed\n");
		return (-1);
	}

	/* ATTACH_BACKING (single contiguous entry) */
	cmd_hdr(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
	ab = (struct virtio_gpu_resource_attach_backing *)g_cmd;
	ab->resource_id = GPU_RESOURCE_ID;
	ab->nr_entries = 1;
	ab->entry.addr = (u_int64_t)fbphys;
	ab->entry.length = w * h * 4;
	if (gpu_cmd(sizeof(*ab), sizeof(struct virtio_gpu_ctrl_hdr)) != VIRTIO_GPU_RESP_OK_NODATA)
	{
		kprintf("virtio-gpu: ATTACH_BACKING failed\n");
		return (-1);
	}

	/* SET_SCANOUT 0 */
	cmd_hdr(VIRTIO_GPU_CMD_SET_SCANOUT);
	ss = (struct virtio_gpu_set_scanout *)g_cmd;
	ss->r.width = w;
	ss->r.height = h;
	ss->scanout_id = 0;
	ss->resource_id = GPU_RESOURCE_ID;
	if (gpu_cmd(sizeof(*ss), sizeof(struct virtio_gpu_ctrl_hdr)) != VIRTIO_GPU_RESP_OK_NODATA)
	{
		kprintf("virtio-gpu: SET_SCANOUT failed\n");
		return (-1);
	}
	return (0);
}

/**
 * Scan the virtio-mmio slots for a GPU, bring it up, set up a scanout-backed
 * framebuffer, and present an initial test pattern (so a graphical QEMU shows
 * a non-blank screen and the command path is exercised end to end).
 *
 * @return 0 on success, -1 if no device / setup failed.
 */
int aarch64_virtio_gpu_init(void)
{
	int slot;
	u_int32_t x, y;

	kprintf("virtio-gpu: init begin\n");
	for (slot = 0; slot < VIRTIO_MMIO_SLOTS; slot++)
	{
		volatile u_int8_t *base = (volatile u_int8_t *)(VIRTIO_MMIO_BASE + (u_int64_t)slot * VIRTIO_MMIO_SLOT);

		if (*(volatile u_int32_t *)(base + VMMIO_MAGIC) != VMMIO_MAGIC_VALUE ||
		    *(volatile u_int32_t *)(base + VMMIO_DEVICE_ID) != VIRTIO_ID_GPU)
			continue;

		g_base = base;
		if (*(volatile u_int32_t *)(base + VMMIO_VERSION) != 2)
		{
			kprintf("virtio-gpu: slot %d is legacy (v1) — unsupported\n", slot);
			return (-1);
		}
		kprintf("virtio-gpu: device at slot %d, setting up queue\n", slot);
		if (gpu_setup_queue() != 0)
			return (-1);
		kprintf("virtio-gpu: queue ready, setting up display\n");
		if (gpu_setup_display() != 0)
			return (-1);
		kprintf("virtio-gpu: display ready\n");
		g_ready = 1;

		/* Test pattern: a simple gradient so a graphical run is visibly alive. */
		for (y = 0; y < virtio_gpu_height; y++)
		{
			u_int32_t *row = (u_int32_t *)(virtio_gpu_fb + (u_int64_t)y * virtio_gpu_pitch);
			for (x = 0; x < virtio_gpu_width; x++)
				row[x] = ((x * 255 / virtio_gpu_width) << 16) | ((y * 255 / virtio_gpu_height) << 8);
		}
		virtio_gpu_flush();

		kprintf("virtio-gpu: vtgpu0 at slot %d, %ux%u fb @0x%lx\n",
		        slot,
		        virtio_gpu_width,
		        virtio_gpu_height,
		        (u_int64_t)(uintptr_t)virtio_gpu_fb);
		return (0);
	}

	kprintf("virtio-gpu: no GPU device found in the virtio-mmio window\n");
	return (-1);
}
