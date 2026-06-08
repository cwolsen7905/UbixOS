/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * virtio-mmio + virtio-input driver — QEMU `virt`, aarch64 bring-up.
 *
 * Sibling of the other virtio drivers (modern v2 transport, split virtqueue,
 * polling).  QEMU presents one virtio-input device per input source — a
 * virtio-keyboard-device and a virtio-mouse-device are separate DeviceID-18
 * slots — so this driver tracks several instances.  Each has an event virtqueue
 * (queue 0) the device fills with `struct virtio_input_event {type,code,value}`
 * (the Linux input ABI: EV_KEY/EV_REL/EV_ABS).  Buffers are pre-posted and
 * re-posted after each delivery; virtio_input_poll() drains every device and
 * hands raw events to a callback (the display layer cooks them into keystrokes +
 * pointer motion).
 */

#include "bringup.h"
#include <sys/types.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h> /* sysID */
#include <string.h>

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
#define VIRTIO_ID_INPUT 18

#define VSTAT_ACKNOWLEDGE 1
#define VSTAT_DRIVER 2
#define VSTAT_DRIVER_OK 4
#define VSTAT_FEATURES_OK 8
#define VIRTIO_F_VERSION_1 32

#define VIRTQ_DESC_F_WRITE 2
#define EVQSIZE 16 /* pre-posted event buffers per device */

/* virtio-input config selects (read the device name to label kbd vs mouse). */
#define VIRTIO_INPUT_CFG_ID_NAME 0x01

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
	u_int16_t ring[EVQSIZE];
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
	struct virtq_used_elem ring[EVQSIZE];
};

/* The Linux input event the device delivers (8 bytes). */
struct virtio_input_event
{
	u_int16_t type;
	u_int16_t code;
	u_int32_t value;
};

struct vinput_dev
{
	volatile u_int8_t *base;
	struct virtq_desc *desc;
	struct virtq_avail *avail;
	struct virtq_used *used;
	u_int16_t last_used;
	struct virtio_input_event *ev; /* EVQSIZE-entry event buffer array */
	char name[32];
};

#define MAX_INPUT 4
static struct vinput_dev g_devs[MAX_INPUT];
static int g_ndev;

static inline void dsb(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
}

static inline u_int32_t rd(volatile u_int8_t *base, u_int32_t off)
{
	return *(volatile u_int32_t *)(base + off);
}

static inline void wr(volatile u_int8_t *base, u_int32_t off, u_int32_t val)
{
	*(volatile u_int32_t *)(base + off) = val;
}

/**
 * (Re-)post event buffer @i of device @d so the device can deliver the next
 * input event into it (one write-only descriptor per event slot).
 */
static void ev_post(struct vinput_dev *d, int i)
{
	d->desc[i].addr = (u_int64_t)(uintptr_t)&d->ev[i];
	d->desc[i].len = sizeof(struct virtio_input_event);
	d->desc[i].flags = VIRTQ_DESC_F_WRITE;
	d->desc[i].next = 0;
	d->avail->ring[d->avail->idx % EVQSIZE] = (u_int16_t)i;
	dsb();
	d->avail->idx++;
	dsb();
}

/**
 * Read the device's human-readable name from config space into @d->name
 * (used only to distinguish keyboard from mouse in logs).
 */
static void read_name(struct vinput_dev *d)
{
	u_int32_t size, i;

	*(volatile u_int8_t *)(d->base + VMMIO_CONFIG + 0) = VIRTIO_INPUT_CFG_ID_NAME; /* select */
	*(volatile u_int8_t *)(d->base + VMMIO_CONFIG + 1) = 0;                        /* subsel */
	dsb();
	size = *(volatile u_int8_t *)(d->base + VMMIO_CONFIG + 2);
	if (size > sizeof(d->name) - 1)
		size = sizeof(d->name) - 1;
	for (i = 0; i < size; i++)
		d->name[i] = *(volatile char *)(d->base + VMMIO_CONFIG + 8 + i);
	d->name[size] = '\0';
}

/**
 * Bring one virtio-input device up: negotiate VERSION_1, set up its event
 * virtqueue, and pre-post all event buffers.
 *
 * @return 0 on success, -1 on failure.
 */
static int input_setup(struct vinput_dev *d)
{
	uintptr_t qpage, evpage;
	int i;

	wr(d->base, VMMIO_STATUS, 0);
	dsb();
	wr(d->base, VMMIO_STATUS, VSTAT_ACKNOWLEDGE);
	wr(d->base, VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER);
	wr(d->base, VMMIO_DRIVER_FEATURES_SEL, 0);
	wr(d->base, VMMIO_DRIVER_FEATURES, 0);
	wr(d->base, VMMIO_DRIVER_FEATURES_SEL, 1);
	wr(d->base, VMMIO_DRIVER_FEATURES, 1u << (VIRTIO_F_VERSION_1 - 32));
	wr(d->base, VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK);
	if ((rd(d->base, VMMIO_STATUS) & VSTAT_FEATURES_OK) == 0)
		return (-1);

	wr(d->base, VMMIO_QUEUE_SEL, 0); /* eventq */
	if (rd(d->base, VMMIO_QUEUE_NUM_MAX) == 0)
		return (-1);
	wr(d->base, VMMIO_QUEUE_NUM, EVQSIZE);

	qpage = vmm_find_free_page(sysID);
	evpage = vmm_find_free_page(sysID);
	if (qpage == 0 || evpage == 0)
		return (-1);
	memset((void *)qpage, 0, PAGE_SIZE);
	memset((void *)evpage, 0, PAGE_SIZE);

	d->desc = (struct virtq_desc *)(qpage + 0);
	d->avail = (struct virtq_avail *)(qpage + 256);
	d->used = (struct virtq_used *)(qpage + 512);
	d->ev = (struct virtio_input_event *)evpage;
	d->last_used = 0;

	wr(d->base, VMMIO_QUEUE_DESC_LOW, (u_int32_t)(uintptr_t)d->desc);
	wr(d->base, VMMIO_QUEUE_DESC_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)d->desc >> 32));
	wr(d->base, VMMIO_QUEUE_DRIVER_LOW, (u_int32_t)(uintptr_t)d->avail);
	wr(d->base, VMMIO_QUEUE_DRIVER_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)d->avail >> 32));
	wr(d->base, VMMIO_QUEUE_DEVICE_LOW, (u_int32_t)(uintptr_t)d->used);
	wr(d->base, VMMIO_QUEUE_DEVICE_HIGH, (u_int32_t)((u_int64_t)(uintptr_t)d->used >> 32));
	dsb();
	wr(d->base, VMMIO_QUEUE_READY, 1);
	wr(d->base, VMMIO_STATUS, VSTAT_ACKNOWLEDGE | VSTAT_DRIVER | VSTAT_FEATURES_OK | VSTAT_DRIVER_OK);
	dsb();

	read_name(d);
	for (i = 0; i < EVQSIZE; i++)
		ev_post(d, i);
	dsb();
	wr(d->base, VMMIO_QUEUE_NOTIFY, 0);
	return (0);
}

/**
 * Drain pending input events from every attached device and hand each to
 * @deliver as (device index, type, code, value).  Non-blocking — call from a
 * poll thread.  Re-posts each consumed buffer.
 *
 * @return the number of events delivered this call.
 */
int virtio_input_poll(void (*deliver)(int dev, u_int16_t type, u_int16_t code, u_int32_t value))
{
	int n, count = 0;

	for (n = 0; n < g_ndev; n++)
	{
		struct vinput_dev *d = &g_devs[n];
		dsb();
		while (d->used->idx != d->last_used)
		{
			struct virtq_used_elem *e = &d->used->ring[d->last_used % EVQSIZE];
			u_int32_t i = e->id;
			if (i < EVQSIZE && deliver != 0)
				deliver(n, d->ev[i].type, d->ev[i].code, d->ev[i].value);
			d->last_used++;
			ev_post(d, (int)i);
			count++;
		}
		dsb();
		wr(d->base, VMMIO_QUEUE_NOTIFY, 0);
	}
	return (count);
}

/**
 * Scan the virtio-mmio slots for every virtio-input device and bring each up.
 *
 * @return the number of input devices attached (0 if none).
 */
int aarch64_virtio_input_init(void)
{
	int slot;

	for (slot = 0; slot < VIRTIO_MMIO_SLOTS && g_ndev < MAX_INPUT; slot++)
	{
		volatile u_int8_t *base = (volatile u_int8_t *)(VIRTIO_MMIO_BASE + (u_int64_t)slot * VIRTIO_MMIO_SLOT);

		if (*(volatile u_int32_t *)(base + VMMIO_MAGIC) != VMMIO_MAGIC_VALUE ||
		    *(volatile u_int32_t *)(base + VMMIO_DEVICE_ID) != VIRTIO_ID_INPUT)
			continue;
		if (*(volatile u_int32_t *)(base + VMMIO_VERSION) != 2)
			continue;

		g_devs[g_ndev].base = base;
		if (input_setup(&g_devs[g_ndev]) != 0)
		{
			kprintf("virtio-input: slot %d setup failed\n", slot);
			continue;
		}
		kprintf("virtio-input: input%d at slot %d (\"%s\")\n", g_ndev, slot, g_devs[g_ndev].name);
		g_ndev++;
	}

	if (g_ndev == 0)
		kprintf("virtio-input: no input devices found\n");
	return (g_ndev);
}
