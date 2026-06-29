/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2837 (Raspberry Pi 3) USB host: Synopsys DWC2 (dwc_otg) controller @
 * peripheral+0x980000 (M7).  The Pi's only external I/O bus — it gates USB HID
 * (keyboard/mouse) AND the onboard Ethernet (a USB LAN9514).  DWC2 is undocumented
 * and uses a custom (non-EHCI) host interface; this is built in phases.
 *
 * M7.0 (this file, so far): power the controller (VideoCore mailbox SET_POWER_STATE),
 * core soft-reset, force host mode, enable DMA + the host port, and detect a
 * connected device + its speed.  Control transfers / enumeration / HID / Ethernet
 * are later phases.  Register bit definitions per the DWC2 databook + the compact
 * bare-metal references (CSUD, USPi).  See docs/design/raspberry-pi-3b-bringup.md.
 */

#ifdef BOARD_RPI3

#include "bringup.h"

#define USB_BASE (PHYSMAP_BASE + 0x3F980000UL)
#define USB(off) (*(volatile u_int32_t *)(USB_BASE + (off)))

/* Core global registers. */
#define GOTGCTL 0x000
#define GAHBCFG 0x008
#define GUSBCFG 0x00C
#define GRSTCTL 0x010
#define GINTSTS 0x014
#define GINTMSK 0x018
#define GHWCFG2 0x048
#define HCFG 0x400
#define HPRT 0x440

/* GAHBCFG bits. */
#define GAHBCFG_GINT (1u << 0)    /* global interrupt enable */
#define GAHBCFG_DMAEN (1u << 5)   /* DMA enable */
#define GAHBCFG_HBSTLEN_INCR (1u << 1)

/* GUSBCFG bits. */
#define GUSBCFG_PHYSEL (1u << 6)   /* 1 = full-speed serial PHY */
#define GUSBCFG_FHMOD (1u << 29)   /* force host mode */
#define GUSBCFG_FDMOD (1u << 30)   /* force device mode */

/* GRSTCTL bits. */
#define GRSTCTL_CSRST (1u << 0)    /* core soft reset */
#define GRSTCTL_AHBIDL (1u << 31)  /* AHB master idle */

/* GINTSTS bits. */
#define GINTSTS_CMOD (1u << 0)     /* current mode: 1 = host */

/* HPRT bits (write-1-to-clear change bits live here too — preserve carefully). */
#define HPRT_PCSTS (1u << 0)       /* port connect status */
#define HPRT_PCDET (1u << 1)       /* connect detected (W1C) */
#define HPRT_PENA (1u << 2)        /* port enable (W1C!) */
#define HPRT_PENCHNG (1u << 3)     /* enable/disable change (W1C) */
#define HPRT_PRST (1u << 8)        /* port reset */
#define HPRT_PPWR (1u << 12)       /* port power */
#define HPRT_PSPD_SHIFT 17
#define HPRT_PSPD_MASK (3u << 17)  /* 0=high, 1=full, 2=low */

/* The change bits in HPRT are write-1-to-clear; a read-modify-write that sets a
 * control bit must mask them off, or it clears them as a side effect. */
#define HPRT_WC (HPRT_PCDET | HPRT_PENA | HPRT_PENCHNG)

static volatile u_int32_t g_usb_mbox[36] __attribute__((aligned(16)));

/** Busy-wait @us microseconds off the virtual counter. */
static void usb_wait_us(u_int32_t us)
{
	u_int64_t f, t0, t, target;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(t0));
	target = t0 + (f * (u_int64_t)us) / 1000000ULL;
	do
		__asm__ volatile("mrs %0, cntvct_el0" : "=r"(t));
	while (t < target);
}

/** Power the USB host controller on via the VideoCore mailbox (device id 3). */
static int usb_power_on(void)
{
	g_usb_mbox[0] = 8 * 4;
	g_usb_mbox[1] = 0;
	g_usb_mbox[2] = 0x00028001; /* SET_POWER_STATE */
	g_usb_mbox[3] = 8;
	g_usb_mbox[4] = 8;
	g_usb_mbox[5] = 3;       /* device id 3 = USB HCD */
	g_usb_mbox[6] = (1 | 2); /* power on + wait for stable */
	g_usb_mbox[7] = 0;
	if (aarch64_mbox_prop(g_usb_mbox) != 0)
		return (-1);
	return ((g_usb_mbox[6] & 1) ? 0 : -1); /* bit0 = powered on */
}

/**
 * M7.0: bring up the DWC2 core in host mode and detect a device on the root port.
 *
 * @return 0 if a device is connected (speed logged), -1 otherwise.
 */
int aarch64_usb_init(void)
{
	u_int32_t hp, spd;
	int i;

	if (usb_power_on() != 0)
	{
		kprintf("usb: controller power-on failed\n");
		return (-1);
	}

	/* Disable the global interrupt while we configure. */
	USB(GAHBCFG) &= ~GAHBCFG_GINT;

	/* Core soft reset: wait for AHB idle, pulse CSRST, wait for it to self-clear. */
	for (i = 0; i < 100000 && (USB(GRSTCTL) & GRSTCTL_AHBIDL) == 0; i++)
		usb_wait_us(1);
	USB(GRSTCTL) |= GRSTCTL_CSRST;
	for (i = 0; i < 100000 && (USB(GRSTCTL) & GRSTCTL_CSRST) != 0; i++)
		usb_wait_us(1);
	if ((USB(GRSTCTL) & GRSTCTL_CSRST) != 0)
	{
		kprintf("usb: core soft reset timeout\n");
		return (-1);
	}
	usb_wait_us(10000);

	/* Force host mode; the controller needs ~25 ms to settle into it. */
	USB(GUSBCFG) = (USB(GUSBCFG) & ~GUSBCFG_FDMOD) | GUSBCFG_FHMOD;
	usb_wait_us(50000);

	/* Internal DMA + INCR bursts; re-enable the global interrupt line. */
	USB(GAHBCFG) = GAHBCFG_DMAEN | GAHBCFG_HBSTLEN_INCR | GAHBCFG_GINT;

	kprintf("usb: DWC2 core up, mode=%s (gintsts=0x%X)\n",
	        (USB(GINTSTS) & GINTSTS_CMOD) ? "host" : "device",
	        USB(GINTSTS));

	/* Power the root port (don't disturb the W1C change bits). */
	hp = USB(HPRT) & ~HPRT_WC;
	USB(HPRT) = hp | HPRT_PPWR;
	usb_wait_us(50000);

	/* Wait briefly for a device to be detected. */
	for (i = 0; i < 100 && (USB(HPRT) & HPRT_PCSTS) == 0; i++)
		usb_wait_us(10000);
	if ((USB(HPRT) & HPRT_PCSTS) == 0)
	{
		kprintf("usb: no device on the root port\n");
		return (-1);
	}

	/* Reset the port (≥50 ms) to enable it, then read the negotiated speed. */
	hp = USB(HPRT) & ~HPRT_WC;
	USB(HPRT) = hp | HPRT_PRST;
	usb_wait_us(60000);
	hp = USB(HPRT) & ~HPRT_WC;
	USB(HPRT) = hp & ~HPRT_PRST;
	usb_wait_us(20000);

	hp = USB(HPRT);
	spd = (hp & HPRT_PSPD_MASK) >> HPRT_PSPD_SHIFT;
	kprintf("usb: root port up (hprt=0x%X) device=%s-speed%s\n",
	        hp,
	        spd == 0 ? "high" : (spd == 1 ? "full" : "low"),
	        (hp & HPRT_PENA) ? ", enabled" : "");
	return (0);
}

#endif /* BOARD_RPI3 */
