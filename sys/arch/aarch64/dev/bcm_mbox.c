/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2837 (Raspberry Pi) VideoCore property mailbox transport (channel 8).  Shared
 * by the EMMC clock query (bcm_sdhci.c) and the framebuffer allocation (bcm_fb.c).
 * The caller fills a 16-byte-aligned property buffer (buf[0] = total byte size,
 * buf[1] = 0 request, then tag triples, then a 0 end tag); this hands it to the GPU
 * at its bus alias (phys | 0xC0000000) and cleans/invalidates the cache around the
 * call for coherency.  See docs/design/raspberry-pi-3b-bringup.md.
 */

#ifdef BOARD_RPI3

#include "bringup.h"

#define MBOX_BASE (PHYSMAP_BASE + 0x3F00B880UL)
#define MBOX_READ (*(volatile u_int32_t *)(MBOX_BASE + 0x00))
#define MBOX_STATUS (*(volatile u_int32_t *)(MBOX_BASE + 0x18))
#define MBOX_WRITE (*(volatile u_int32_t *)(MBOX_BASE + 0x20))
#define MBOX_FULL 0x80000000u
#define MBOX_EMPTY 0x40000000u
#define MBOX_CH_PROP 8u
#define GPU_BUS_ALIAS 0xC0000000u

/**
 * Perform a property-mailbox call on @msg (a 16-byte-aligned buffer; msg[0] = total
 * size in bytes).  Blocks until the GPU replies on channel 8.
 *
 * @return 0 if the GPU set the success code (msg[1] == 0x80000000), -1 otherwise.
 */
int aarch64_mbox_prop(volatile u_int32_t *msg)
{
	uintptr_t a, base = (uintptr_t)msg;
	u_int32_t bytes = msg[0];
	u_int32_t req;

	/* Clean the message out of the dcache so the GPU sees it. */
	for (a = base; a < base + bytes; a += 64)
		__asm__ volatile("dc cvac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");

	req = (((u_int32_t)AARCH64_PHYS_OF(base) | GPU_BUS_ALIAS) & ~0xFu) | MBOX_CH_PROP;
	while ((MBOX_STATUS & MBOX_FULL) != 0)
		;
	MBOX_WRITE = req;
	for (;;)
	{
		while ((MBOX_STATUS & MBOX_EMPTY) != 0)
			;
		if ((MBOX_READ & 0xFu) == MBOX_CH_PROP)
			break;
	}

	/* Invalidate so we read the GPU's reply, not stale cache. */
	__asm__ volatile("dsb sy" ::: "memory");
	for (a = base; a < base + bytes; a += 64)
		__asm__ volatile("dc ivac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");

	return (msg[1] == 0x80000000u ? 0 : -1);
}

#endif /* BOARD_RPI3 */
