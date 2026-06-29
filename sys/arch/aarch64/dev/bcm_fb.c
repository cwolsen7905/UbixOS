/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2837 (Raspberry Pi 3) framebuffer via the VideoCore property mailbox (M6).
 * Asks the GPU to allocate a 32-bpp linear framebuffer in RAM, then points the
 * shared display-stack globals (virtio_gpu_fb/width/height/pitch — the de-facto
 * framebuffer interface fbcon + display.c/sys_mapfb consume) at it.  Unlike
 * virtio-gpu (a transfer+flush present model), the VideoCore scans the buffer out
 * of RAM continuously, so "present" is just a dcache clean (aarch64_fb_present,
 * display.c) to push CPU writes to the scanout.  See raspberry-pi-3b-bringup.md.
 */

#ifdef BOARD_RPI3

#include "bringup.h"

/* The display-stack framebuffer interface (defined in virtio_gpu.c; on the Pi the
 * virtio device is absent so we populate these directly). */
extern u_int8_t *virtio_gpu_fb;
extern u_int32_t virtio_gpu_width;
extern u_int32_t virtio_gpu_height;
extern u_int32_t virtio_gpu_pitch;

/* Request 1080p; the GPU clamps to the monitor's max if EDID reports lower.  We use
 * whatever width/height/pitch the mailbox returns, so a clamp is handled gracefully. */
#define FB_WIDTH 1920
#define FB_HEIGHT 1080

static volatile u_int32_t g_fbmbox[36] __attribute__((aligned(16)));

/**
 * Allocate the framebuffer from the VideoCore and publish it to the display stack.
 *
 * @return 0 on success, -1 if the mailbox call failed or returned no buffer.
 */
int aarch64_bcm_fb_init(void)
{
	u_int32_t base, pitch, w, h;

	/* Exact 35-word layout from the known-good bare-metal reference (bztsrc): the
	 * set-virtual-OFFSET tag (0x48009) is required — omitting it made the GPU ignore
	 * the resolution sets and hand back a degenerate 640x480/pitch=0 default. */
	g_fbmbox[0] = 35 * 4; /* total size */
	g_fbmbox[1] = 0;      /* request */
	g_fbmbox[2] = 0x00048003;
	g_fbmbox[3] = 8;
	g_fbmbox[4] = 8; /* set physical w/h */
	g_fbmbox[5] = FB_WIDTH;
	g_fbmbox[6] = FB_HEIGHT;
	g_fbmbox[7] = 0x00048004;
	g_fbmbox[8] = 8;
	g_fbmbox[9] = 8; /* set virtual w/h */
	g_fbmbox[10] = FB_WIDTH;
	g_fbmbox[11] = FB_HEIGHT;
	g_fbmbox[12] = 0x00048009;
	g_fbmbox[13] = 8;
	g_fbmbox[14] = 8; /* set virtual offset */
	g_fbmbox[15] = 0;
	g_fbmbox[16] = 0;
	g_fbmbox[17] = 0x00048005;
	g_fbmbox[18] = 4;
	g_fbmbox[19] = 4; /* set depth */
	g_fbmbox[20] = 32;
	g_fbmbox[21] = 0x00048006;
	g_fbmbox[22] = 4;
	g_fbmbox[23] = 4; /* set pixel order */
	g_fbmbox[24] = 0; /* 0 = BGR (matches objGFX B8G8R8X8) */
	g_fbmbox[25] = 0x00040001;
	g_fbmbox[26] = 8;
	g_fbmbox[27] = 8;    /* allocate buffer */
	g_fbmbox[28] = 4096; /* (in) alignment -> (out) base */
	g_fbmbox[29] = 0;    /* (out) size */
	g_fbmbox[30] = 0x00040008;
	g_fbmbox[31] = 4;
	g_fbmbox[32] = 4; /* get pitch */
	g_fbmbox[33] = 0; /* (out) pitch */
	g_fbmbox[34] = 0; /* end tag */

	if (aarch64_mbox_prop(g_fbmbox) != 0 || g_fbmbox[28] == 0)
	{
		kprintf("bcm_fb: mailbox framebuffer allocation failed (base=0x%X)\n", g_fbmbox[28]);
		return (-1);
	}

	base = g_fbmbox[28] & 0x3FFFFFFFu; /* GPU bus address -> ARM physical */
	pitch = g_fbmbox[33];
	w = g_fbmbox[5];
	h = g_fbmbox[6];

	virtio_gpu_fb = (u_int8_t *)(uintptr_t)AARCH64_VIRT_OF(base); /* physmap VA */
	virtio_gpu_width = w;
	virtio_gpu_height = h;
	virtio_gpu_pitch = pitch;

	kprintf("bcm_fb: %ux%u pitch=%u fb phys=0x%X (va=0x%lX)\n",
	        w,
	        h,
	        pitch,
	        base,
	        (u_int64_t)(uintptr_t)virtio_gpu_fb);
	return (0);
}

#endif /* BOARD_RPI3 */
