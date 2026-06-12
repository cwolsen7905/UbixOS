/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * virtio-mmio + virtio-blk driver (QEMU `virt`, aarch64 bring-up).
 *
 * QEMU `virt` exposes virtio devices over the modern (v2) virtio-mmio transport:
 * 32 device slots of 0x200 bytes starting at 0x0a000000.  This is a minimal
 * polling driver — one virtqueue, one outstanding request, no interrupts — which
 * is plenty to mount a disk image and load the world.  It registers a
 * struct ubx_device with block ops so the buffer cache (bcache_read) + FAT can
 * read sectors with no knowledge of virtio.
 *
 * DMA: all RAM is identity-mapped Normal-cacheable and QEMU's emulated virtio is
 * coherent with the guest, so a physical frame address doubles as the kernel
 * pointer used to fill descriptors/buffers; dsb barriers order ring updates
 * against the device.
 */

#include "bringup.h"
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/descrip.h>   /* g_device_find hook (so vfs_mount finds this device) */
#include <dev/partition.h> /* MBR partition parsing (vtblk0sN) */
#include <dev/disk.h>      /* md_disk_list — Disk Utility enumeration hook */
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>
#include <string.h>

/* QEMU `virt` virtio-mmio window: 32 slots of 0x200 bytes at 0x0a000000. */
#define VIRTIO_MMIO_BASE 0x0A000000UL
#define VIRTIO_MMIO_SLOT 0x200UL
#define VIRTIO_MMIO_SLOTS 32

/* virtio-mmio registers (offsets within a slot). */
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
#define VMMIO_INTERRUPT_STATUS 0x060
#define VMMIO_INTERRUPT_ACK 0x064
#define VMMIO_STATUS 0x070
#define VMMIO_QUEUE_DESC_LOW 0x080
#define VMMIO_QUEUE_DESC_HIGH 0x084
#define VMMIO_QUEUE_DRIVER_LOW 0x090
#define VMMIO_QUEUE_DRIVER_HIGH 0x094
#define VMMIO_QUEUE_DEVICE_LOW 0x0A0
#define VMMIO_QUEUE_DEVICE_HIGH 0x0A4
#define VMMIO_CONFIG 0x100

#define VMMIO_MAGIC_VALUE 0x74726976 /* "virt" */
#define VIRTIO_ID_BLOCK 2

/* Device status bits. */
#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4
#define VSTAT_FEATURES_OK 8

#define VIRTIO_F_VERSION_1 32 /* feature bit (in the high feature word) */

/* Split-virtqueue structures (virtio 1.x). */
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define QSIZE 8 /* queue depth (we only keep one request in flight) */

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

/* virtio-blk request header (followed by data, then a 1-byte status). */
#define VIRTIO_BLK_T_IN 0  /* read  */
#define VIRTIO_BLK_T_OUT 1 /* write */
#define VIRTIO_BLK_S_OK 0

struct virtio_blk_req
{
	u_int32_t type;
	u_int32_t reserved;
	u_int64_t sector;
};

/* Driver state — one block device. */
static volatile u_int8_t *g_base;    /* MMIO base of the matched slot */
static struct virtq_desc *g_desc;    /* descriptor table */
static struct virtq_avail *g_avail;  /* available ring */
static struct virtq_used *g_used;    /* used ring */
static struct virtio_blk_req *g_hdr; /* request header (DMA) */
static u_int8_t *g_data;             /* 512-byte data bounce buffer (DMA) */
static volatile u_int8_t *g_status;  /* 1-byte status (DMA) */
static u_int16_t g_last_used;        /* last consumed used-ring index */
static int g_ready;                  /* non-zero once attached */
static struct ubx_device g_blk_dev;  /* the whole-disk block device (vtblk0) */
static struct ubx_blk_ops g_blk_ops; /* its block ops */

/* MBR partitions discovered on the disk (vtblk0sN).  The pool root lives on one
 * of these (type 0x9C); vfs_mount resolves it by minor via the find hook. */
static struct ubp_partition g_parts[MBR_MAX_PARTITIONS];
static int g_npart;

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
 * g_device_find hook: vfs_mount resolves a (major, minor) to a block device
 * through this.  minor 0 (and the legacy whole-disk mount, major/minor 0/0) maps
 * to the whole disk; minor N>0 maps to MBR partition N (vtblk0sN), so the pool
 * root in slot 3 resolves at (major 1, minor 3) — the same scheme as i386 ad0sN.
 */
static void *virtio_blk_device_find(int major, int minor)
{
	int i;

	(void)major;
	if (!g_ready)
		return (NULL);
	if (minor > 0)
	{
		for (i = 0; i < g_npart; i++)
			if (g_parts[i].minor == minor)
				return (&g_parts[i].dev);
		return (NULL); /* asked for a partition that does not exist */
	}
	return (&g_blk_dev);
}

/**
 * Read @count sectors starting at @lba into @buf via virtio-blk, one sector per
 * request (polling the used ring).  Matches struct ubx_blk_ops::read.
 *
 * @return 0 on success, -1 on a device error.
 */
static int virtio_blk_read(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	(void)dev;
	if (!g_ready)
		return (-1);

	for (u_int32_t i = 0; i < count; i++)
	{
		g_hdr->type = VIRTIO_BLK_T_IN;
		g_hdr->reserved = 0;
		g_hdr->sector = (u_int64_t)lba + i;
		*g_status = 0xFF;

		/* Three-descriptor chain: header (R) -> data (W) -> status (W). */
		g_desc[0].addr = (u_int64_t)(uintptr_t)g_hdr;
		g_desc[0].len = sizeof(struct virtio_blk_req);
		g_desc[0].flags = VIRTQ_DESC_F_NEXT;
		g_desc[0].next = 1;
		g_desc[1].addr = (u_int64_t)(uintptr_t)g_data;
		g_desc[1].len = 512;
		g_desc[1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
		g_desc[1].next = 2;
		g_desc[2].addr = (u_int64_t)(uintptr_t)g_status;
		g_desc[2].len = 1;
		g_desc[2].flags = VIRTQ_DESC_F_WRITE;
		g_desc[2].next = 0;

		g_avail->ring[g_avail->idx % QSIZE] = 0; /* head descriptor index */
		dsb();
		g_avail->idx++;
		dsb();
		mmio_wr(VMMIO_QUEUE_NOTIFY, 0);

		while (g_used->idx == g_last_used)
			dsb(); /* poll for completion */
		g_last_used++;
		dsb();

		if (*g_status != VIRTIO_BLK_S_OK)
			return (-1);
		memcpy((u_int8_t *)buf + (u_int64_t)i * 512, g_data, 512);
	}
	return (0);
}

/**
 * Write @count sectors starting at @lba from @buf via virtio-blk, one sector per
 * request (polling the used ring).  Matches struct ubx_blk_ops::write.
 *
 * Mirrors virtio_blk_read with a VIRTIO_BLK_T_OUT request; the data descriptor
 * is device-readable (no VIRTQ_DESC_F_WRITE), and the payload is staged through
 * the same g_data bounce buffer the read path uses.
 *
 * @return 0 on success, -1 on a device error.
 */
static int virtio_blk_write(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	(void)dev;
	if (!g_ready)
		return (-1);

	for (u_int32_t i = 0; i < count; i++)
	{
		memcpy(g_data, (const u_int8_t *)buf + (u_int64_t)i * 512, 512);

		g_hdr->type = VIRTIO_BLK_T_OUT;
		g_hdr->reserved = 0;
		g_hdr->sector = (u_int64_t)lba + i;
		*g_status = 0xFF;

		/* Three-descriptor chain: header (R) -> data (R) -> status (W). */
		g_desc[0].addr = (u_int64_t)(uintptr_t)g_hdr;
		g_desc[0].len = sizeof(struct virtio_blk_req);
		g_desc[0].flags = VIRTQ_DESC_F_NEXT;
		g_desc[0].next = 1;
		g_desc[1].addr = (u_int64_t)(uintptr_t)g_data;
		g_desc[1].len = 512;
		g_desc[1].flags = VIRTQ_DESC_F_NEXT;
		g_desc[1].next = 2;
		g_desc[2].addr = (u_int64_t)(uintptr_t)g_status;
		g_desc[2].len = 1;
		g_desc[2].flags = VIRTQ_DESC_F_WRITE;
		g_desc[2].next = 0;

		g_avail->ring[g_avail->idx % QSIZE] = 0; /* head descriptor index */
		dsb();
		g_avail->idx++;
		dsb();
		mmio_wr(VMMIO_QUEUE_NOTIFY, 0);

		while (g_used->idx == g_last_used)
			dsb(); /* poll for completion */
		g_last_used++;
		dsb();

		if (*g_status != VIRTIO_BLK_S_OK)
			return (-1);
	}
	return (0);
}

/**
 * Set up virtqueue 0 and bring the device to DRIVER_OK.
 *
 * @return 0 on success, -1 on failure.
 */
static int virtio_blk_setup_queue(void)
{
	uintptr_t qpage, iopage;
	u_int32_t qmax;

	mmio_wr(VMMIO_STATUS, 0); /* reset */
	dsb();
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE);
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER);

	/* Accept VIRTIO_F_VERSION_1 (high feature word, bit 32) and no others. */
	mmio_wr(VMMIO_DRIVER_FEATURES_SEL, 0);
	mmio_wr(VMMIO_DRIVER_FEATURES, 0);
	mmio_wr(VMMIO_DRIVER_FEATURES_SEL, 1);
	mmio_wr(VMMIO_DRIVER_FEATURES, 1u << (VIRTIO_F_VERSION_1 - 32));

	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK);
	if ((mmio_rd(VMMIO_STATUS) & VSTAT_FEATURES_OK) == 0)
	{
		kprintf("virtio-blk: device rejected FEATURES_OK\n");
		return (-1);
	}

	mmio_wr(VMMIO_QUEUE_SEL, 0);
	qmax = mmio_rd(VMMIO_QUEUE_NUM_MAX);
	if (qmax == 0)
	{
		kprintf("virtio-blk: queue 0 unavailable\n");
		return (-1);
	}
	mmio_wr(VMMIO_QUEUE_NUM, QSIZE);

	/* One page holds the split-ring (desc @0, avail @256, used @512); a second
	 * page holds the request header + data + status DMA buffers. */
	qpage = vmm_find_free_page(sysID);
	iopage = vmm_find_free_page(sysID);
	if (qpage == 0 || iopage == 0)
		return (-1);
	memset((void *)qpage, 0, PAGE_SIZE);
	memset((void *)iopage, 0, PAGE_SIZE);

	g_desc = (struct virtq_desc *)(qpage + 0);
	g_avail = (struct virtq_avail *)(qpage + 256);
	g_used = (struct virtq_used *)(qpage + 512);
	g_hdr = (struct virtio_blk_req *)(iopage + 0);
	g_data = (u_int8_t *)(iopage + 64);
	g_status = (volatile u_int8_t *)(iopage + 64 + 512);
	g_last_used = 0;

	mmio_wr(VMMIO_QUEUE_DESC_LOW, (u_int32_t)(uintptr_t)g_desc);
	mmio_wr(VMMIO_QUEUE_DESC_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)g_desc >> 32));
	mmio_wr(VMMIO_QUEUE_DRIVER_LOW, (u_int32_t)(uintptr_t)g_avail);
	mmio_wr(VMMIO_QUEUE_DRIVER_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)g_avail >> 32));
	mmio_wr(VMMIO_QUEUE_DEVICE_LOW, (u_int32_t)(uintptr_t)g_used);
	mmio_wr(VMMIO_QUEUE_DEVICE_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)g_used >> 32));
	dsb();
	mmio_wr(VMMIO_QUEUE_READY, 1);

	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK | VSTAT_DRIVER_OK);
	dsb();
	return (0);
}

/**
 * Scan the virtio-mmio slots for a block device, bring it up, and register it as
 * a ubx_device with block ops.
 *
 * @return the registered block device on success, NULL if none/failure.
 */
struct ubx_device *aarch64_virtio_blk_init(void)
{
	int slot;

	for (slot = 0; slot < VIRTIO_MMIO_SLOTS; slot++)
	{
		volatile u_int8_t *base = (volatile u_int8_t *)(VIRTIO_MMIO_BASE + (u_int64_t)slot * VIRTIO_MMIO_SLOT);
		u_int32_t magic = *(volatile u_int32_t *)(base + VMMIO_MAGIC);
		u_int32_t devid = *(volatile u_int32_t *)(base + VMMIO_DEVICE_ID);

		if (magic != VMMIO_MAGIC_VALUE || devid != VIRTIO_ID_BLOCK)
			continue;

		g_base = base;
		if (*(volatile u_int32_t *)(base + VMMIO_VERSION) != 2)
		{
			kprintf("virtio-blk: slot %d is legacy (v1) — unsupported\n", slot);
			return (NULL);
		}
		if (virtio_blk_setup_queue() != 0)
			return (NULL);

		g_blk_ops.read = virtio_blk_read;
		g_blk_ops.write = virtio_blk_write;
		memset(&g_blk_dev, 0, sizeof(g_blk_dev));
		g_blk_dev.dev_blk_ops = &g_blk_ops;
		strncpy(g_blk_dev.dev_nameunit, "vtblk0", sizeof(g_blk_dev.dev_nameunit) - 1);
		g_ready = 1;
		g_device_find = virtio_blk_device_find; /* let vfs_mount resolve to us */

		kprintf("virtio-blk: vtblk0 at mmio slot %d (0x%lx)\n", slot, (u_int64_t)(uintptr_t)base);

		/* Parse the MBR (if any) so the pool partition is mountable as vtblk0sN.
		 * A bare-FAT image (no MBR) simply yields 0 partitions and the whole-disk
		 * device stays the only one — preserving the legacy mount path. */
		g_npart = mbr_parse_partitions(&g_blk_dev, g_parts, MBR_MAX_PARTITIONS);
		if (g_npart < 0)
			g_npart = 0;
		if (g_npart == 0)
			kprintf("virtio-blk: no MBR partitions (bare disk)\n");

		return (&g_blk_dev);
	}

	kprintf("virtio-blk: no block device found in the virtio-mmio window\n");
	return (NULL);
}

/**
 * Return the device minor of the UbixFS pool partition (MBR type 0x9C), or -1 if
 * the disk has no pool partition.  Lets the boot path mount the pool root without
 * hardcoding a slot, so the disk layout can change independently of the kernel.
 */
int aarch64_virtio_blk_pool_minor(void)
{
	int i;

	for (i = 0; i < g_npart; i++)
		if (g_parts[i].type == MBR_TYPE_UBPOOL)
			return (g_parts[i].minor);
	return (-1);
}

/**
 * md_disk_list (sys/kern/disk_query.c hook): the aarch64 disk inventory — the
 * single virtio-blk drive.  Capacity is the virtio-blk config `capacity` field
 * (512-byte sectors) at the start of the device-specific config window.
 */
int md_disk_list(struct disk_hw *out, int max)
{
	u_int32_t lo, hi;

	if (max <= 0 || !g_ready)
		return (0);

	lo = *(volatile u_int32_t *)(g_base + VMMIO_CONFIG);
	hi = *(volatile u_int32_t *)(g_base + VMMIO_CONFIG + 4);

	out[0].dev = &g_blk_dev;
	strncpy(out[0].name, "vtblk0", sizeof(out[0].name) - 1);
	out[0].name[sizeof(out[0].name) - 1] = '\0';
	out[0].sectors = ((u_int64_t)hi << 32) | lo;
	strncpy(out[0].model, "virtio-blk", sizeof(out[0].model) - 1);
	out[0].model[sizeof(out[0].model) - 1] = '\0';
	return (1);
}
