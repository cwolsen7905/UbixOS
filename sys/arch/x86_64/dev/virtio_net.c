/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 virtio-net over legacy virtio-pci.  A polled NIC driver: find the PCI
 * device, run the legacy status handshake, set up the receive (queue 0) and
 * transmit (queue 1) virtqueues (each the single contiguous page-aligned vring
 * legacy mandates), pre-post receive buffers, and expose send/poll to the lwIP
 * netif bridge (net/virtio_netif.c).
 *
 * The transport (PCI I/O ports + QUEUE_PFN) mirrors this arch's virtio_blk.c; the
 * net-specific queue logic (two queues, the virtio_net_hdr, RX buffer ring) mirrors
 * aarch64's dev/virtio_net.c.  Legacy without VIRTIO_NET_F_MRG_RXBUF uses a 10-byte
 * virtio_net_hdr (the modern/mergeable header is 12) — we negotiate no features, so
 * 10 it is.
 */

#include "../x86_64.h"
#include "pci.h"
#include <vmm/vmm.h>     /* vmm_find_free_pages_contig, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>

/* PCI identity: Red Hat / virtio vendor; transitional network device id 0x1000. */
#define VIRTIO_VENDOR 0x1AF4
#define VIRTIO_NET_DEVICE_LEGACY 0x1000

/* Legacy virtio-pci I/O register offsets (from the BAR0 I/O base). */
#define VPCI_HOST_FEATURES 0x00  /* r 32 */
#define VPCI_GUEST_FEATURES 0x04 /* w 32 */
#define VPCI_QUEUE_PFN 0x08      /* rw 32 — vring phys >> 12 */
#define VPCI_QUEUE_NUM 0x0C      /* r 16 — max queue size */
#define VPCI_QUEUE_SEL 0x0E      /* w 16 */
#define VPCI_QUEUE_NOTIFY 0x10   /* w 16 */
#define VPCI_STATUS 0x12         /* rw 8 */
#define VPCI_ISR 0x13            /* r 8 */
#define VPCI_NET_CONFIG 0x14     /* device-specific config (MAC) when MSI-X off */

#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4

#define VRING_ALIGN 4096

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

#define NET_HDR_LEN 10 /* legacy virtio_net_hdr, no MRG_RXBUF */
#define NET_BUF 2048   /* per RX/TX buffer: hdr + max Ethernet frame + slack */
#define RX_BUFS 32     /* receive buffers pre-posted to the device */
#define BUFS_PER_PAGE (PAGE_SIZE / NET_BUF)

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
	u16 ring[]; /* [qsize], then a u16 used_event */
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
	struct virtq_used_elem ring[]; /* [qsize], then a u16 avail_event */
} __attribute__((packed));

/* One legacy split virtqueue (desc | avail | pad | used in one contiguous run). */
struct vq
{
	struct virtq_desc *desc;
	struct virtq_avail *avail;
	struct virtq_used *used;
	u16 last_used;
	u16 qsize;
	u16 notify_idx; /* queue index for QUEUE_NOTIFY */
};

static u16 g_io;       /* BAR0 I/O base */
static int g_ready;    /* 1 once the device is usable */
static struct vq g_rx; /* receive queue (0) */
static struct vq g_tx; /* transmit queue (1) */
static u8 *g_rx_buf[RX_BUFS];
static u8 *g_tx_buf;

u8 virtio_net_mac[6]; /* device MAC (read from config space) */
int virtio_net_ready; /* exposed so the netif bridge can gate on link-up */

/** @return ALIGN(@x, @a). */
static u64 align_up(u64 x, u64 a)
{
	return (x + a - 1) & ~(a - 1);
}

/**
 * Configure legacy virtqueue @qidx, laying its rings into one freshly allocated
 * contiguous run and handing the device its page-frame number.  Legacy virtio-pci
 * has NO writable queue-size register: the queue size is fixed at the device's
 * read-only QUEUE_NUM, so the vring must be laid out for exactly that many entries
 * (sizing it smaller puts the used ring where the device won't write it — TX/RX
 * then never complete).  @return the queue size, or 0 on failure.
 */
static u16 setup_vq(struct vq *q, u16 qidx)
{
	uintptr_t vring;
	u64 desc_bytes, avail_bytes, used_off, total;
	u16 qsize;

	outw((u16)(g_io + VPCI_QUEUE_SEL), qidx);
	qsize = inw((u16)(g_io + VPCI_QUEUE_NUM));
	if (qsize == 0)
		return 0;

	desc_bytes = (u64)16 * qsize;
	avail_bytes = 6 + (u64)2 * qsize;
	used_off = align_up(desc_bytes + avail_bytes, VRING_ALIGN);
	total = used_off + (6 + (u64)8 * qsize);

	vring = vmm_find_free_pages_contig((u32)((total + PAGE_SIZE - 1) / PAGE_SIZE), sysID);
	if (vring == 0)
		return 0;
	memset(P2V(vring), 0, (size_t)align_up(total, PAGE_SIZE));

	q->desc = (struct virtq_desc *)P2V(vring);
	q->avail = (struct virtq_avail *)((u8 *)P2V(vring) + desc_bytes);
	q->used = (struct virtq_used *)((u8 *)P2V(vring) + used_off);
	q->last_used = 0;
	q->qsize = qsize;
	q->notify_idx = qidx;

	outw((u16)(g_io + VPCI_QUEUE_SEL), qidx);
	outl((u16)(g_io + VPCI_QUEUE_PFN), (u32)(vring / VRING_ALIGN));
	return qsize;
}

/**
 * Post receive buffer @bufidx so the device can fill it with the next inbound
 * packet (descriptor index == buffer index, never chained, device-writable).
 */
static void rx_post(int bufidx)
{
	g_rx.desc[bufidx].addr = V2P(g_rx_buf[bufidx]);
	g_rx.desc[bufidx].len = NET_BUF;
	g_rx.desc[bufidx].flags = VIRTQ_DESC_F_WRITE;
	g_rx.desc[bufidx].next = 0;

	g_rx.avail->ring[g_rx.avail->idx % g_rx.qsize] = (u16)bufidx;
	__asm__ __volatile__("mfence" ::: "memory");
	g_rx.avail->idx++;
	__asm__ __volatile__("mfence" ::: "memory");
}

/**
 * Transmit a single Ethernet frame: prepend a zeroed virtio_net_hdr, post it as one
 * read-only descriptor, notify the device, and poll for completion.  @len is the
 * frame length (without the virtio header).  @return 0 on success, -1 otherwise.
 */
int virtio_net_send(const void *frame, u32 len)
{
	u16 head;

	if (!g_ready || len + NET_HDR_LEN > NET_BUF)
		return -1;

	memset(g_tx_buf, 0, NET_HDR_LEN);
	memcpy(g_tx_buf + NET_HDR_LEN, frame, len);

	head = (u16)(g_tx.avail->idx % g_tx.qsize);
	g_tx.desc[head].addr = V2P(g_tx_buf);
	g_tx.desc[head].len = NET_HDR_LEN + len;
	g_tx.desc[head].flags = 0; /* device reads */
	g_tx.desc[head].next = 0;

	g_tx.avail->ring[head] = head;
	__asm__ __volatile__("mfence" ::: "memory");
	g_tx.avail->idx++;
	__asm__ __volatile__("mfence" ::: "memory");
	outw((u16)(g_io + VPCI_QUEUE_NOTIFY), g_tx.notify_idx);

	{
		int spin = 0;
		while (g_tx.used->idx == g_tx.last_used)
		{
			__asm__ __volatile__("pause" ::: "memory");
			if (++spin == 20000000)
			{
				serial_puts("virtio-net: TX completion timeout (used ring stuck)\n");
				return -1;
			}
		}
	}
	g_tx.last_used++;
	__asm__ __volatile__("mfence" ::: "memory");
	return 0;
}

/**
 * Drain the receive used-ring: for each packet delivered, invoke @deliver with the
 * Ethernet frame (the virtio header skipped), then re-post the buffer.
 * Non-blocking.  @return the number of frames delivered this call.
 */
int virtio_net_poll_rx(void (*deliver)(const u8 *frame, u32 len))
{
	int count = 0;

	if (!g_ready)
		return 0;

	__asm__ __volatile__("mfence" ::: "memory");
	while (g_rx.used->idx != g_rx.last_used)
	{
		struct virtq_used_elem *e = &g_rx.used->ring[g_rx.last_used % g_rx.qsize];
		u32 bufidx = e->id;
		u32 total = e->len; /* includes the virtio_net_hdr */

		if (bufidx < g_rx.qsize && total > NET_HDR_LEN)
		{
			if (deliver != 0)
				deliver(g_rx_buf[bufidx] + NET_HDR_LEN, total - NET_HDR_LEN);
			count++;
		}
		g_rx.last_used++;
		rx_post((int)bufidx);
		__asm__ __volatile__("mfence" ::: "memory");
		outw((u16)(g_io + VPCI_QUEUE_NOTIFY), g_rx.notify_idx);
	}
	return count;
}

/**
 * Probe + initialise the virtio-net-pci device: PCI find, enable I/O + bus master,
 * legacy status handshake (no features negotiated), set up the RX/TX queues, carve
 * the DMA buffers, pre-post all receive buffers, and reach DRIVER_OK.
 * @return 0 on success, -1 if absent/unusable.
 */
int x86_64_virtio_net_init(void)
{
	struct pci_dev d;
	uintptr_t bufpage = 0;
	int i;

	if (!pci_find_device(VIRTIO_VENDOR, VIRTIO_NET_DEVICE_LEGACY, &d))
	{
		serial_puts("virtio-net: no PCI device (vendor 0x1AF4 dev 0x1000)\n");
		return -1;
	}
	if ((d.bar[0] & 0x1) == 0)
	{
		serial_puts("virtio-net: BAR0 is not an I/O BAR\n");
		return -1;
	}
	g_io = (u16)(d.bar[0] & ~0x3U);
	pci_enable_io_busmaster(&d);

	/* Legacy status handshake: reset, ACKNOWLEDGE, DRIVER, accept no features. */
	outb((u16)(g_io + VPCI_STATUS), 0);
	outb((u16)(g_io + VPCI_STATUS), VSTAT_ACKNOWLEDGE);
	outb((u16)(g_io + VPCI_STATUS), VSTAT_ACKNOWLEDGE | VSTAT_DRIVER);
	outl((u16)(g_io + VPCI_GUEST_FEATURES), 0);

	/* MAC from device-specific config (6 bytes at NET_CONFIG, MSI-X disabled). */
	for (i = 0; i < 6; i++)
		virtio_net_mac[i] = inb((u16)(g_io + VPCI_NET_CONFIG + i));

	if (setup_vq(&g_rx, 0) == 0)
	{
		serial_puts("virtio-net: RX queue unavailable\n");
		return -1;
	}
	if (setup_vq(&g_tx, 1) == 0)
	{
		serial_puts("virtio-net: TX queue unavailable\n");
		return -1;
	}

	/* RX DMA buffers (NET_BUF each, packed BUFS_PER_PAGE per contiguous page); TX
	 * uses a single buffer on its own page. */
	for (i = 0; i < RX_BUFS; i++)
	{
		if ((i % BUFS_PER_PAGE) == 0)
		{
			bufpage = vmm_find_free_pages_contig(1, sysID);
			if (bufpage == 0)
				return -1;
			memset(P2V(bufpage), 0, PAGE_SIZE);
		}
		g_rx_buf[i] = (u8 *)P2V(bufpage) + (uintptr_t)(i % BUFS_PER_PAGE) * NET_BUF;
	}
	{
		uintptr_t txp = vmm_find_free_pages_contig(1, sysID);
		if (txp == 0)
			return -1;
		memset(P2V(txp), 0, PAGE_SIZE);
		g_tx_buf = (u8 *)P2V(txp);
	}

	g_ready = 1;
	for (i = 0; i < RX_BUFS; i++)
		rx_post(i);
	__asm__ __volatile__("mfence" ::: "memory");
	outw((u16)(g_io + VPCI_QUEUE_NOTIFY), g_rx.notify_idx);

	outb((u16)(g_io + VPCI_STATUS), VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_DRIVER_OK);

	virtio_net_ready = 1;
	serial_puts("virtio-net: vtnet0 ready, MAC ");
	for (i = 0; i < 6; i++)
	{
		serial_puthex(virtio_net_mac[i]);
		serial_puts(i < 5 ? ":" : "\n");
	}
	return 0;
}
