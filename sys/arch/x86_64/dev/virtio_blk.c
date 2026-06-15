/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 virtio-blk over legacy virtio-pci (Phase 5c).  A polled block driver:
 * find the PCI device, negotiate via the legacy I/O-BAR register window, set up
 * one virtqueue (the single contiguous page-aligned vring legacy mandates), and
 * read/write 512-byte sectors with the standard 3-descriptor request chain.  The
 * virtqueue request logic mirrors aarch64's virtio_blk.c; only the transport
 * (PCI I/O ports vs MMIO) differs.  This is the disk the FAT root mounts on (the
 * VFS wiring follows in 5c-2).
 *
 * Legacy virtio-pci was chosen over the modern (1.0 capability) transport for
 * bring-up simplicity — QEMU's transitional virtio-blk-pci exposes both, and the
 * I/O-port register block is far simpler than parsing PCI capabilities.
 */

#include "../x86_64.h"
#include "pci.h"
#include <vmm/vmm.h>     /* vmm_find_free_pages_contig, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>

/* PCI identity: Red Hat / virtio vendor; transitional block device id 0x1001. */
#define VIRTIO_VENDOR 0x1AF4
#define VIRTIO_BLK_DEVICE_LEGACY 0x1001

/* Legacy virtio-pci I/O register offsets (from the BAR0 I/O base). */
#define VPCI_HOST_FEATURES 0x00  /* r 32 */
#define VPCI_GUEST_FEATURES 0x04 /* w 32 */
#define VPCI_QUEUE_PFN 0x08      /* rw 32 — vring phys >> 12 */
#define VPCI_QUEUE_NUM 0x0C      /* r 16 — max queue size */
#define VPCI_QUEUE_SEL 0x0E      /* w 16 */
#define VPCI_QUEUE_NOTIFY 0x10   /* w 16 */
#define VPCI_STATUS 0x12         /* rw 8 */
#define VPCI_ISR 0x13            /* r 8 */

#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4

#define VRING_ALIGN 4096

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

#define VIRTIO_BLK_T_IN 0  /* read  */
#define VIRTIO_BLK_T_OUT 1 /* write */
#define VIRTIO_BLK_S_OK 0

struct virtq_desc
{
	u64 addr;
	u32 len;
	u16 flags;
	u16 next;
} __attribute__((packed));

struct virtq_avail
{
	u16 flags;
	u16 idx;
	u16 ring[]; /* [queue_num], then a u16 used_event */
} __attribute__((packed));

struct virtq_used_elem
{
	u32 id;
	u32 len;
} __attribute__((packed));

struct virtq_used
{
	u16 flags;
	u16 idx;
	struct virtq_used_elem ring[]; /* [queue_num], then a u16 avail_event */
} __attribute__((packed));

struct virtio_blk_req
{
	u32 type;
	u32 reserved;
	u64 sector;
} __attribute__((packed));

static u16 g_io;    /* BAR0 I/O base */
static u16 g_qnum;  /* negotiated queue size */
static int g_ready; /* 1 once the device is usable */
static u16 g_last_used;

static struct virtq_desc *g_desc;
static struct virtq_avail *g_avail;
static struct virtq_used *g_used;
static struct virtio_blk_req *g_hdr;
static u8 *g_data;            /* 512-byte DMA bounce buffer */
static volatile u8 *g_status; /* 1-byte status */

/** @return ALIGN(@x, @a). */
static u64 align_up(u64 x, u64 a)
{
	return (x + a - 1) & ~(a - 1);
}

/**
 * Submit the 3-descriptor request currently staged in g_desc[0..2] and poll the
 * used ring for completion.  @return 0 on VIRTIO_BLK_S_OK, -1 otherwise.
 */
static int submit_and_wait(void)
{
	g_avail->ring[g_avail->idx % g_qnum] = 0; /* head descriptor index */
	__asm__ __volatile__("mfence" ::: "memory");
	g_avail->idx++;
	__asm__ __volatile__("mfence" ::: "memory");
	outw((u16)(g_io + VPCI_QUEUE_NOTIFY), 0); /* notify queue 0 */

	while (g_used->idx == g_last_used)
		__asm__ __volatile__("pause" ::: "memory");
	g_last_used++;
	__asm__ __volatile__("mfence" ::: "memory");

	return (*g_status == VIRTIO_BLK_S_OK) ? 0 : -1;
}

/** Read @count sectors at @lba into @buf (one request each).  @return 0/-1. */
int virtio_blk_read(u32 lba, u32 count, void *buf)
{
	if (!g_ready)
		return -1;

	for (u32 i = 0; i < count; i++)
	{
		g_hdr->type = VIRTIO_BLK_T_IN;
		g_hdr->reserved = 0;
		g_hdr->sector = (u64)lba + i;
		*g_status = 0xFF;

		g_desc[0].addr = (u64)(uintptr_t)g_hdr;
		g_desc[0].len = sizeof(struct virtio_blk_req);
		g_desc[0].flags = VIRTQ_DESC_F_NEXT;
		g_desc[0].next = 1;
		g_desc[1].addr = (u64)(uintptr_t)g_data;
		g_desc[1].len = 512;
		g_desc[1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
		g_desc[1].next = 2;
		g_desc[2].addr = (u64)(uintptr_t)g_status;
		g_desc[2].len = 1;
		g_desc[2].flags = VIRTQ_DESC_F_WRITE;
		g_desc[2].next = 0;

		if (submit_and_wait() != 0)
			return -1;
		memcpy((u8 *)buf + (u64)i * 512, g_data, 512);
	}
	return 0;
}

/** Write @count sectors at @lba from @buf (one request each).  @return 0/-1. */
int virtio_blk_write(u32 lba, u32 count, const void *buf)
{
	if (!g_ready)
		return -1;

	for (u32 i = 0; i < count; i++)
	{
		memcpy(g_data, (const u8 *)buf + (u64)i * 512, 512);
		g_hdr->type = VIRTIO_BLK_T_OUT;
		g_hdr->reserved = 0;
		g_hdr->sector = (u64)lba + i;
		*g_status = 0xFF;

		g_desc[0].addr = (u64)(uintptr_t)g_hdr;
		g_desc[0].len = sizeof(struct virtio_blk_req);
		g_desc[0].flags = VIRTQ_DESC_F_NEXT;
		g_desc[0].next = 1;
		g_desc[1].addr = (u64)(uintptr_t)g_data;
		g_desc[1].len = 512;
		g_desc[1].flags = VIRTQ_DESC_F_NEXT; /* device-readable */
		g_desc[1].next = 2;
		g_desc[2].addr = (u64)(uintptr_t)g_status;
		g_desc[2].len = 1;
		g_desc[2].flags = VIRTQ_DESC_F_WRITE;
		g_desc[2].next = 0;

		if (submit_and_wait() != 0)
			return -1;
	}
	return 0;
}

/**
 * Probe + initialise the virtio-blk-pci device: PCI find, enable I/O + bus
 * master, run the legacy status handshake, set up queue 0's contiguous vring, and
 * mark the device ready.  @return 0 on success, -1 if absent/unusable.
 */
int virtio_blk_init(void)
{
	struct pci_dev d;
	uintptr_t vring, dma;
	u64 desc_bytes, avail_bytes, used_off, total;

	if (!pci_find_device(VIRTIO_VENDOR, VIRTIO_BLK_DEVICE_LEGACY, &d))
	{
		serial_puts("virtio-blk: no PCI device (vendor 0x1AF4 dev 0x1001)\n");
		return -1;
	}
	if ((d.bar[0] & 0x1) == 0)
	{
		serial_puts("virtio-blk: BAR0 is not an I/O BAR\n");
		return -1;
	}
	g_io = (u16)(d.bar[0] & ~0x3U);
	pci_enable_io_busmaster(&d);

	/* Legacy status handshake: reset, ACKNOWLEDGE, DRIVER, (accept no features). */
	outb((u16)(g_io + VPCI_STATUS), 0);
	outb((u16)(g_io + VPCI_STATUS), VSTAT_ACKNOWLEDGE);
	outb((u16)(g_io + VPCI_STATUS), VSTAT_ACKNOWLEDGE | VSTAT_DRIVER);
	outl((u16)(g_io + VPCI_GUEST_FEATURES), 0);

	/* Select queue 0 and read its size. */
	outw((u16)(g_io + VPCI_QUEUE_SEL), 0);
	g_qnum = inw((u16)(g_io + VPCI_QUEUE_NUM));
	if (g_qnum == 0)
	{
		serial_puts("virtio-blk: queue 0 unavailable\n");
		return -1;
	}

	/* Lay out the single contiguous legacy vring: desc | avail | pad | used. */
	desc_bytes = (u64)16 * g_qnum;
	avail_bytes = 6 + (u64)2 * g_qnum;
	used_off = align_up(desc_bytes + avail_bytes, VRING_ALIGN);
	total = used_off + (6 + (u64)8 * g_qnum);

	vring = vmm_find_free_pages_contig((u32)((total + PAGE_SIZE - 1) / PAGE_SIZE), sysID);
	if (vring == 0)
	{
		serial_puts("virtio-blk: vring allocation failed\n");
		return -1;
	}
	memset((void *)vring, 0, (size_t)align_up(total, PAGE_SIZE));
	g_desc = (struct virtq_desc *)vring;
	g_avail = (struct virtq_avail *)(vring + desc_bytes);
	g_used = (struct virtq_used *)(vring + used_off);
	g_last_used = 0;

	/* Tell the device where the vring lives (page-frame number) and start it. */
	outw((u16)(g_io + VPCI_QUEUE_SEL), 0);
	outl((u16)(g_io + VPCI_QUEUE_PFN), (u32)(vring / VRING_ALIGN));
	outb((u16)(g_io + VPCI_STATUS), VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_DRIVER_OK);

	/* DMA buffers (one page carved into header + 512-byte data + status). */
	dma = vmm_find_free_pages_contig(1, sysID);
	if (dma == 0)
		return -1;
	memset((void *)dma, 0, PAGE_SIZE);
	g_hdr = (struct virtio_blk_req *)dma;
	g_data = (u8 *)(dma + 64);
	g_status = (volatile u8 *)(dma + 64 + 512);

	g_ready = 1;
	serial_puts("virtio-blk: ready (I/O base ");
	serial_puthex(g_io);
	serial_puts(", queue ");
	serial_putdec(g_qnum);
	serial_puts(")\n");
	return 0;
}
