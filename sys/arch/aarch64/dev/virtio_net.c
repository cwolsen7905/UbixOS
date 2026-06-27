/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * virtio-mmio + virtio-net driver (QEMU `virt`, aarch64 bring-up).
 *
 * Companion to virtio_blk.c: same modern (v2) virtio-mmio transport (32 slots of
 * 0x200 bytes at 0x0a000000), same split-virtqueue layout, same polling model
 * (no interrupts — an lwIP RX thread polls the receive used-ring cooperatively).
 *
 * virtio-net uses two queues: queue 0 = receive, queue 1 = transmit.  Every
 * packet on the wire is prefixed on the queue by a 12-byte struct virtio_net_hdr
 * (VIRTIO_F_VERSION_1); we negotiate no offloads, so the header is always zeroed
 * on TX and skipped on RX.  RX buffers are pre-posted at init and re-posted after
 * each delivery.
 *
 * DMA: all RAM is identity-mapped Normal-cacheable and QEMU's emulated virtio is
 * coherent with the guest, so a physical frame address doubles as the kernel
 * pointer used to fill descriptors/buffers; dsb barriers order ring updates.
 */

#include "bringup.h"
#include <sys/types.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h> /* sysID (frame allocator pool id) */
#include <string.h>

/* QEMU `virt` virtio-mmio window: 32 slots of 0x200 bytes at 0x0a000000. */
#define VIRTIO_MMIO_BASE (PHYSMAP_BASE + 0x0A000000UL) /* device regs via TTBR1 physmap (Phase 4) */
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
#define VIRTIO_ID_NET 1

/* Device status bits. */
#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4
#define VSTAT_FEATURES_OK 8

#define VIRTIO_F_VERSION_1 32 /* feature bit (high feature word) */
#define VIRTIO_NET_F_MAC 5    /* config.mac is valid (low feature word) */

/* Split-virtqueue structures (virtio 1.x). */
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

#define RX_QSIZE 16  /* receive queue depth (pre-posted buffers) */
#define TX_QSIZE 8   /* transmit queue depth (one request in flight) */
#define NET_BUF 2048 /* per-buffer size: 12B hdr + 1514B frame + slack */
#define VIRTIO_NET_HDR_LEN 12

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
	u_int16_t ring[RX_QSIZE]; /* sized to the larger of the two queues */
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
	struct virtq_used_elem ring[RX_QSIZE];
};

/* One split virtqueue (rings share a page: desc @0, avail @0x100, used @0x300). */
struct vq
{
	struct virtq_desc *desc;
	struct virtq_avail *avail;
	struct virtq_used *used;
	u_int16_t last_used;
	u_int16_t qsize;
	u_int32_t notify_idx; /* queue index for QUEUE_NOTIFY */
};

/* Driver state — one network device. */
static volatile u_int8_t *g_base;    /* MMIO base of the matched slot */
static struct vq g_rx;               /* receive queue (0) */
static struct vq g_tx;               /* transmit queue (1) */
static u_int8_t *g_rx_buf[RX_QSIZE]; /* receive DMA buffers */
static u_int8_t *g_tx_buf;           /* single transmit DMA buffer */
static int g_ready;                  /* non-zero once attached */

u_int8_t virtio_net_mac[6]; /* device MAC (read from config space) */
int virtio_net_ready;       /* exposed so the netif bridge can gate on link-up */

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
 * Post receive buffer @bufidx (a full RX_QSIZE descriptor + avail slot) so the
 * device can fill it with the next inbound packet.  The descriptor index equals
 * the buffer index (one descriptor per buffer, never chained).
 */
static void rx_post(int bufidx)
{
	g_rx.desc[bufidx].addr = AARCH64_PHYS_OF((uintptr_t)g_rx_buf[bufidx]);
	g_rx.desc[bufidx].len = NET_BUF;
	g_rx.desc[bufidx].flags = VIRTQ_DESC_F_WRITE; /* device writes the packet */
	g_rx.desc[bufidx].next = 0;

	g_rx.avail->ring[g_rx.avail->idx % g_rx.qsize] = (u_int16_t)bufidx;
	dsb();
	g_rx.avail->idx++;
	dsb();
}

/**
 * Configure virtqueue @qidx (regs already selected by the caller) with @qsize
 * entries, laying its rings into a freshly allocated page.
 *
 * @return 0 on success, -1 on allocation/availability failure.
 */
static int setup_vq(struct vq *q, u_int32_t qidx, u_int16_t qsize)
{
	uintptr_t qpage;
	u_int32_t qmax;

	mmio_wr(VMMIO_QUEUE_SEL, qidx);
	qmax = mmio_rd(VMMIO_QUEUE_NUM_MAX);
	if (qmax == 0)
	{
		kprintf("virtio-net: queue %u unavailable\n", qidx);
		return (-1);
	}
	if (qsize > qmax)
		qsize = (u_int16_t)qmax;
	mmio_wr(VMMIO_QUEUE_NUM, qsize);

	qpage = vmm_find_free_page(sysID);
	if (qpage == 0)
		return (-1);
	/* Access the vring through the physmap (Convention B); the device gets the
	 * PHYSICAL qpage in the queue registers below + descriptor .addr fields. */
	memset((void *)(uintptr_t)AARCH64_VIRT_OF(qpage), 0, PAGE_SIZE);

	q->desc = (struct virtq_desc *)(uintptr_t)AARCH64_VIRT_OF(qpage + 0x000);
	q->avail = (struct virtq_avail *)(uintptr_t)AARCH64_VIRT_OF(qpage + 0x100);
	q->used = (struct virtq_used *)(uintptr_t)AARCH64_VIRT_OF(qpage + 0x300);
	q->last_used = 0;
	q->qsize = qsize;
	q->notify_idx = qidx;

	mmio_wr(VMMIO_QUEUE_DESC_LOW, (u_int32_t)qpage);
	mmio_wr(VMMIO_QUEUE_DESC_HIGH, (u_int32_t)((u_int64_t)qpage >> 32));
	mmio_wr(VMMIO_QUEUE_DRIVER_LOW, (u_int32_t)(qpage + 0x100));
	mmio_wr(VMMIO_QUEUE_DRIVER_HIGH, (u_int32_t)((u_int64_t)(qpage + 0x100) >> 32));
	mmio_wr(VMMIO_QUEUE_DEVICE_LOW, (u_int32_t)(qpage + 0x300));
	mmio_wr(VMMIO_QUEUE_DEVICE_HIGH, (u_int32_t)((u_int64_t)(qpage + 0x300) >> 32));
	dsb();
	mmio_wr(VMMIO_QUEUE_READY, 1);
	return (0);
}

/**
 * Transmit a single Ethernet frame: prepend a zeroed 12-byte virtio_net_hdr,
 * post it as one read-only descriptor, notify the device, and poll for
 * completion.  @len is the frame length (without the virtio header).
 *
 * @return 0 on success, -1 if the device is not ready or the frame is too large.
 */
int virtio_net_send(const void *frame, u_int32_t len)
{
	u_int16_t head;

	if (!g_ready || len + VIRTIO_NET_HDR_LEN > NET_BUF)
		return (-1);

	memset(g_tx_buf, 0, VIRTIO_NET_HDR_LEN);
	memcpy(g_tx_buf + VIRTIO_NET_HDR_LEN, frame, len);

	head = g_tx.avail->idx % g_tx.qsize;
	g_tx.desc[head].addr = AARCH64_PHYS_OF((uintptr_t)g_tx_buf);
	g_tx.desc[head].len = VIRTIO_NET_HDR_LEN + len;
	g_tx.desc[head].flags = 0; /* device reads */
	g_tx.desc[head].next = 0;

	g_tx.avail->ring[head] = head;
	dsb();
	g_tx.avail->idx++;
	dsb();
	mmio_wr(VMMIO_QUEUE_NOTIFY, g_tx.notify_idx);

	while (g_tx.used->idx == g_tx.last_used)
		dsb(); /* poll for completion */
	g_tx.last_used++;
	dsb();
	return (0);
}

/**
 * Drain the receive used-ring: for each packet the device has delivered, invoke
 * @deliver with the Ethernet frame (the 12-byte virtio header skipped), then
 * re-post the buffer.  Non-blocking — call from an RX poll thread.
 *
 * @return the number of frames delivered this call.
 */
int virtio_net_poll_rx(void (*deliver)(const u_int8_t *frame, u_int32_t len))
{
	int count = 0;

	if (!g_ready)
		return (0);

	dsb();
	while (g_rx.used->idx != g_rx.last_used)
	{
		struct virtq_used_elem *e = &g_rx.used->ring[g_rx.last_used % g_rx.qsize];
		u_int32_t bufidx = e->id;
		u_int32_t total = e->len; /* includes the 12-byte virtio_net_hdr */

		if (bufidx < g_rx.qsize && total > VIRTIO_NET_HDR_LEN)
		{
			if (deliver != 0)
				deliver(g_rx_buf[bufidx] + VIRTIO_NET_HDR_LEN, total - VIRTIO_NET_HDR_LEN);
			count++;
		}
		g_rx.last_used++;
		rx_post((int)bufidx); /* hand the buffer back to the device */
		dsb();
		mmio_wr(VMMIO_QUEUE_NOTIFY, g_rx.notify_idx);
	}
	return (count);
}

/**
 * Bring the matched virtio-net slot up: negotiate VERSION_1 (+ MAC), set up the
 * RX/TX queues, pre-post all receive buffers, and reach DRIVER_OK.
 *
 * @return 0 on success, -1 on failure.
 */
static int virtio_net_setup(void)
{
	uintptr_t txpage, bufpage = 0;
	u_int32_t lo_feat;
	int i;

	mmio_wr(VMMIO_STATUS, 0); /* reset */
	dsb();
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE);
	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER);

	/* Read the low feature word; accept VIRTIO_NET_F_MAC if offered.  Accept
	 * VIRTIO_F_VERSION_1 in the high word; no offloads (checksum/GSO/mrg). */
	mmio_wr(VMMIO_DEVICE_FEATURES_SEL, 0);
	lo_feat = mmio_rd(VMMIO_DEVICE_FEATURES);
	mmio_wr(VMMIO_DRIVER_FEATURES_SEL, 0);
	mmio_wr(VMMIO_DRIVER_FEATURES, lo_feat & (1u << VIRTIO_NET_F_MAC));
	mmio_wr(VMMIO_DRIVER_FEATURES_SEL, 1);
	mmio_wr(VMMIO_DRIVER_FEATURES, 1u << (VIRTIO_F_VERSION_1 - 32));

	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK);
	if ((mmio_rd(VMMIO_STATUS) & VSTAT_FEATURES_OK) == 0)
	{
		kprintf("virtio-net: device rejected FEATURES_OK\n");
		return (-1);
	}

	/* MAC from config space (6 bytes at CONFIG+0), valid when F_MAC negotiated. */
	for (i = 0; i < 6; i++)
		virtio_net_mac[i] = *(volatile u_int8_t *)(g_base + VMMIO_CONFIG + i);

	if (setup_vq(&g_rx, 0, RX_QSIZE) != 0)
		return (-1);
	if (setup_vq(&g_tx, 1, TX_QSIZE) != 0)
		return (-1);

	/* RX DMA buffers: RX_QSIZE buffers of NET_BUF bytes, packed (2 per 4 KB) into
	 * contiguous identity-mapped pages.  TX uses a single buffer on its own page. */
	for (i = 0; i < RX_QSIZE; i++)
	{
		if ((i % (PAGE_SIZE / NET_BUF)) == 0)
		{
			bufpage = vmm_find_free_page(sysID);
			if (bufpage == 0)
				return (-1);
			memset((void *)(uintptr_t)AARCH64_VIRT_OF(bufpage), 0, PAGE_SIZE);
		}
		/* Access via the physmap (Convention B); rx_post's descriptor .addr takes
		 * AARCH64_PHYS_OF of this so the device DMAs by physical. */
		g_rx_buf[i] =
		    (u_int8_t *)(uintptr_t)AARCH64_VIRT_OF(bufpage + (uintptr_t)(i % (PAGE_SIZE / NET_BUF)) * NET_BUF);
	}
	txpage = vmm_find_free_page(sysID);
	if (txpage == 0)
		return (-1);
	memset((void *)(uintptr_t)AARCH64_VIRT_OF(txpage), 0, PAGE_SIZE);
	g_tx_buf = (u_int8_t *)(uintptr_t)AARCH64_VIRT_OF(txpage);

	for (i = 0; i < RX_QSIZE; i++)
		rx_post(i);
	dsb();
	mmio_wr(VMMIO_QUEUE_NOTIFY, g_rx.notify_idx);

	mmio_wr(VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK | VSTAT_DRIVER_OK);
	dsb();
	return (0);
}

/**
 * Scan the virtio-mmio slots for a network device and bring it up.
 *
 * @return 0 on success, -1 if no device was found / setup failed.
 */
int aarch64_virtio_net_init(void)
{
	int slot;

	for (slot = 0; slot < VIRTIO_MMIO_SLOTS; slot++)
	{
		volatile u_int8_t *base = (volatile u_int8_t *)(VIRTIO_MMIO_BASE + (u_int64_t)slot * VIRTIO_MMIO_SLOT);
		u_int32_t magic = *(volatile u_int32_t *)(base + VMMIO_MAGIC);
		u_int32_t devid = *(volatile u_int32_t *)(base + VMMIO_DEVICE_ID);

		if (magic != VMMIO_MAGIC_VALUE || devid != VIRTIO_ID_NET)
			continue;

		g_base = base;
		if (*(volatile u_int32_t *)(base + VMMIO_VERSION) != 2)
		{
			kprintf("virtio-net: slot %d is legacy (v1) — unsupported\n", slot);
			return (-1);
		}
		if (virtio_net_setup() != 0)
			return (-1);

		g_ready = 1;
		virtio_net_ready = 1;
		kprintf("virtio-net: vtnet0 at mmio slot %d, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		        slot,
		        virtio_net_mac[0],
		        virtio_net_mac[1],
		        virtio_net_mac[2],
		        virtio_net_mac[3],
		        virtio_net_mac[4],
		        virtio_net_mac[5]);
		return (0);
	}

	kprintf("virtio-net: no network device found in the virtio-mmio window\n");
	return (-1);
}
