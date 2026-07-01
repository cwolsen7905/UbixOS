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
#define GRXFSIZ 0x024   /* receive FIFO size */
#define GNPTXFSIZ 0x028 /* non-periodic TX FIFO size */
#define GHWCFG2 0x048
#define HPTXFSIZ 0x100 /* host periodic TX FIFO size */
#define HCFG 0x400
#define HPRT 0x440

/* FIFO depths (32-bit words) — the DWC2 requires these be programmed after reset;
 * the defaults are too small for 64-byte packets, so larger IN transfers silently
 * fail.  Values from the USPi/Circle bare-metal driver (fit the Pi's DFIFO RAM). */
#define DWC_RX_FIFO 1024
#define DWC_NPTX_FIFO 1024
#define DWC_PTX_FIFO 1024

/* GRSTCTL FIFO-flush bits. */
#define GRSTCTL_RXFFLSH (1u << 4)
#define GRSTCTL_TXFFLSH (1u << 5)
#define GRSTCTL_TXFNUM_ALL (0x10u << 6)

/* GAHBCFG bits. */
#define GAHBCFG_GINT (1u << 0)    /* global interrupt enable */
#define GAHBCFG_DMAEN (1u << 5)   /* DMA enable */
#define GAHBCFG_HBSTLEN_INCR (1u << 1)

/* GUSBCFG bits. */
#define GUSBCFG_PHYSEL (1u << 6)      /* 1 = full-speed serial PHY */
#define GUSBCFG_TRDT_SHIFT 10        /* USB turnaround time [13:10] */
#define GUSBCFG_TRDT_MASK (0xFu << 10)
#define GUSBCFG_FHMOD (1u << 29)     /* force host mode */
#define GUSBCFG_FDMOD (1u << 30)     /* force device mode */

/* HCFG (host configuration) bits. */
#define HCFG_FSLSPCLKSEL_MASK (3u << 0)
#define HCFG_FSLSPCLKSEL_48MHZ (1u << 0) /* PHY clock select: 48 MHz (HS/internal PHY) */
#define HCFG_FSLSSUPP (1u << 2)          /* FS/LS-only support (0 for a high-speed port) */

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
static int g_root_lowspeed; /* root device negotiated low-speed (set in usb_init) */

/* M7.1 forward declarations (definitions follow aarch64_usb_init). */
static int usb_control_data(u_int32_t devaddr, u_int32_t mps, int in, void *data, u_int32_t len);
static int g_usb_dbg; /* when set, usb_control logs each stage's result */

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
 * Reset the root port (≥50 ms pulse) to enable it + (re)negotiate speed; updates
 * g_root_lowspeed.  @return the post-reset HPRT value.
 */
static u_int32_t usb_port_reset(void)
{
	u_int32_t hp, spd;

	hp = USB(HPRT) & ~HPRT_WC;
	USB(HPRT) = hp | HPRT_PRST;
	usb_wait_us(60000);
	hp = USB(HPRT) & ~HPRT_WC;
	USB(HPRT) = hp & ~HPRT_PRST;
	usb_wait_us(20000);

	hp = USB(HPRT);
	spd = (hp & HPRT_PSPD_MASK) >> HPRT_PSPD_SHIFT;
	g_root_lowspeed = (spd == 2);
	return (hp);
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

	/* Force host mode + set the PHY turnaround time (TRDT=5 for the internal HS
	 * UTMI+ PHY, per FreeBSD dwc_otg); the controller needs ~25 ms to settle. */
	USB(GUSBCFG) = (USB(GUSBCFG) & ~(GUSBCFG_FDMOD | GUSBCFG_TRDT_MASK)) | GUSBCFG_FHMOD |
	               (5u << GUSBCFG_TRDT_SHIFT);
	usb_wait_us(50000);

	/* Host clock select: FSLSPCLKSEL=1 (48 MHz) for the internal HS PHY, FSLSSUPP=0.
	 * REQUIRED — without a valid host clock the speed negotiation is flaky (FS one
	 * boot, HS the next) and transfers are unreliable.  (FreeBSD dwc_otg.) */
	USB(HCFG) = (USB(HCFG) & ~(HCFG_FSLSSUPP | HCFG_FSLSPCLKSEL_MASK)) | HCFG_FSLSPCLKSEL_48MHZ;

	/* Internal DMA + INCR bursts; re-enable the global interrupt line. */
	USB(GAHBCFG) = GAHBCFG_DMAEN | GAHBCFG_HBSTLEN_INCR | GAHBCFG_GINT;

	/* Program the FIFO sizes (REQUIRED — defaults are too small for 64-byte packets,
	 * so larger IN transfers silently no-op).  RX, then non-periodic TX after it,
	 * then periodic TX after that; depth in [31:16], start address in [15:0]. */
	USB(GRXFSIZ) = DWC_RX_FIFO;
	USB(GNPTXFSIZ) = (DWC_NPTX_FIFO << 16) | DWC_RX_FIFO;
	USB(HPTXFSIZ) = (DWC_PTX_FIFO << 16) | (DWC_RX_FIFO + DWC_NPTX_FIFO);

	/* Flush all TX FIFOs and the RX FIFO so they start clean. */
	USB(GRSTCTL) = GRSTCTL_TXFNUM_ALL | GRSTCTL_TXFFLSH;
	for (i = 0; i < 10000 && (USB(GRSTCTL) & GRSTCTL_TXFFLSH) != 0; i++)
		usb_wait_us(1);
	USB(GRSTCTL) = GRSTCTL_RXFFLSH;
	for (i = 0; i < 10000 && (USB(GRSTCTL) & GRSTCTL_RXFFLSH) != 0; i++)
		usb_wait_us(1);

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

	/* Reset the port to enable it + negotiate speed. */
	hp = usb_port_reset();
	spd = (hp & HPRT_PSPD_MASK) >> HPRT_PSPD_SHIFT;
	kprintf("usb: root port up (hprt=0x%X) device=%s-speed%s\n",
	        hp,
	        spd == 0 ? "high" : (spd == 1 ? "full" : "low"),
	        (hp & HPRT_PENA) ? ", enabled" : "");

	aarch64_usb_enumerate();
	return (0);
}

/* --- M7.1: host-channel control transfers + root-device enumeration --- */

/* Host channel n registers (0x500 + n*0x20). */
#define HC_BASE(n) (0x500 + (n) * 0x20)
#define HCCHAR(n) USB(HC_BASE(n) + 0x00)
#define HCSPLT(n) USB(HC_BASE(n) + 0x04) /* split-transaction control (0 = no split) */
#define HCINT(n) USB(HC_BASE(n) + 0x08)
#define HCTSIZ(n) USB(HC_BASE(n) + 0x10)
#define HCDMA(n) USB(HC_BASE(n) + 0x14)

/* HCCHAR bits. */
#define HCCHAR_CHENA (1u << 31)
#define HCCHAR_CHDIS (1u << 30)
#define HCCHAR_EPDIR_IN (1u << 15)
#define HCCHAR_LSDEV (1u << 17)
#define HCCHAR_MCNT1 (1u << 20)

/* HCINT bits. */
#define HCINT_XFRC (1u << 0)   /* transfer complete */
#define HCINT_CHH (1u << 1)    /* channel halted */
#define HCINT_STALL (1u << 3)
#define HCINT_NAK (1u << 4)
#define HCINT_ACK (1u << 5)
#define HCINT_XACTERR (1u << 7)
#define HCINT_ERRMASK (HCINT_STALL | HCINT_XACTERR | (1u << 8) | (1u << 10))

/* Data PIDs (HCTSIZ[31:29]). */
#define PID_DATA0 0u
#define PID_DATA1 2u
#define PID_SETUP 3u

/* USB endpoint types. */
#define EPTYP_CONTROL 0u

/* DMA bounce buffers (the controller DMAs to/from physical addresses). */
static u_int8_t g_setup[8] __attribute__((aligned(64)));
static u_int8_t g_data[256] __attribute__((aligned(64)));

/** Clean a buffer out of the dcache so the controller's DMA sees our writes. */
static void dc_clean(void *p, u_int32_t len)
{
	uintptr_t a, e = (uintptr_t)p + len;
	/* Leading barrier: ensure the CPU's stores to the buffer are complete before we
	 * clean the line to RAM — without it the clean can flush stale data and the new
	 * stores never reach the controller's DMA (the stale-SETUP bug). */
	__asm__ volatile("dsb sy" ::: "memory");
	for (a = (uintptr_t)p & ~63UL; a < e; a += 64)
		__asm__ volatile("dc cvac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

/** Invalidate a buffer so we read the controller's DMA result, not stale cache. */
static void dc_inval(void *p, u_int32_t len)
{
	uintptr_t a, e = (uintptr_t)p + len;
	__asm__ volatile("dsb sy" ::: "memory");
	for (a = (uintptr_t)p & ~63UL; a < e; a += 64)
		__asm__ volatile("dc ivac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

/**
 * Cleanly halt/disable host channel 0 before (re)programming it.  The DWC2 leaves a
 * channel "halted but enabled" after a transfer; reprogramming without first
 * disabling it corrupts the next transfer (the first-transfer-works-then-breaks
 * pattern).  If CHENA is set, request a disable and wait for the halt, then clear
 * all pending interrupts.
 */
static void usb_chan0_halt(void)
{
	int i;

	if ((HCCHAR(0) & HCCHAR_CHENA) != 0)
	{
		HCINT(0) = 0xFFFFFFFF;
		HCCHAR(0) |= HCCHAR_CHENA | HCCHAR_CHDIS; /* enable+disable => halt request */
		for (i = 0; i < 100000 && (HCINT(0) & HCINT_CHH) == 0; i++)
			usb_wait_us(1);
	}
	HCINT(0) = 0xFFFFFFFF;
}

/**
 * Run one transfer on host channel 0 (polled, DMA).  @return 0 on XFRC, -1 on
 * timeout/error; retries on NAK.
 */
static int usb_chan0(u_int32_t devaddr, int dir_in, u_int32_t mps, u_int32_t pid, void *buf, u_int32_t len)
{
	u_int32_t pktcnt = (len + mps - 1) / mps;
	int tries, i;

	if (pktcnt == 0)
		pktcnt = 1;
	if (len != 0)
	{
		if (dir_in)
			dc_inval(buf, len);
		else
			dc_clean(buf, len);
	}

	for (tries = 0; tries < 10; tries++)
	{
		u_int32_t s = 0;

		/* DWC2 requires an IN transfer's XferSize to be a multiple of the max packet
		 * size (rounded up); programming the exact length truncates the transfer.  A
		 * zero-length IN (a no-data control's status stage) must still use XferSize=mps
		 * so the core actually issues the IN token — XferSize=0 completes spuriously
		 * (XFRC without a wire transaction), so the device never commits (SET_ADDRESS
		 * no-op).  The device's ZLP/short packet terminates it. */
		u_int32_t xfer = dir_in ? (pktcnt * mps) : len;

		usb_chan0_halt(); /* clean slate: disable the channel + clear interrupts */
		/* Start order per FreeBSD dwc_otg: HCTSIZ, then HCSPLT (0 = no split — the
		 * root device is a high-speed hub, so direct, not split, transactions), then
		 * HCCHAR with CHENA.  HCSPLT must be written explicitly; a stale split bit
		 * makes every transaction fail. */
		HCTSIZ(0) = (xfer & 0x7FFFF) | (pktcnt << 19) | (pid << 29);
		HCSPLT(0) = 0;
		HCDMA(0) = (u_int32_t)AARCH64_PHYS_OF((uintptr_t)buf);
		HCCHAR(0) = (mps & 0x7FF) | (0u << 11) /* ep0 */ | (dir_in ? HCCHAR_EPDIR_IN : 0) |
		            (g_root_lowspeed ? HCCHAR_LSDEV : 0) | (EPTYP_CONTROL << 18) | HCCHAR_MCNT1 |
		            ((devaddr & 0x7F) << 22) | HCCHAR_CHENA;

		/* Wait for the channel to halt (XFRC on success, or an error/NAK). */
		for (i = 0; i < 500000; i++)
		{
			s = HCINT(0);
			if (s & (HCINT_XFRC | HCINT_STALL | HCINT_NAK | HCINT_CHH | HCINT_ERRMASK))
				break;
			usb_wait_us(1);
		}
		if (s & HCINT_XFRC)
		{
			if (dir_in && len)
				dc_inval(buf, len);
			return (0);
		}
		if (s & HCINT_STALL)
		{
			kprintf("usb: ep0 STALL\n");
			return (-1); /* protocol stall — fatal, not retryable */
		}
		/* NAK, transaction error (XACTERR — transient, esp. right after
		 * SET_ADDRESS), babble, toggle error, or a bare halt: re-issue. */
		usb_wait_us(2000);
	}
	kprintf("usb: ep0 xfer failed addr=%u dir=%s len=%u pid=%u hcint=0x%X\n",
	        devaddr,
	        dir_in ? "IN" : "OUT",
	        len,
	        pid,
	        HCINT(0));
	return (-1);
}

/**
 * Standard control transfer on ep0 (SETUP / optional DATA / STATUS) to @devaddr.
 * @return 0 on success.
 */
static int usb_control(u_int32_t devaddr,
                       u_int32_t mps,
                       u_int8_t bmReqType,
                       u_int8_t bReq,
                       u_int16_t wValue,
                       u_int16_t wIndex,
                       u_int16_t wLength,
                       void *data)
{
	int in = (bmReqType & 0x80) != 0;

	g_setup[0] = bmReqType;
	g_setup[1] = bReq;
	g_setup[2] = (u_int8_t)wValue;
	g_setup[3] = (u_int8_t)(wValue >> 8);
	g_setup[4] = (u_int8_t)wIndex;
	g_setup[5] = (u_int8_t)(wIndex >> 8);
	g_setup[6] = (u_int8_t)wLength;
	g_setup[7] = (u_int8_t)(wLength >> 8);

	if (usb_chan0(devaddr, 0, mps, PID_SETUP, g_setup, 8) != 0)
	{
		if (g_usb_dbg)
			kprintf("usb:   SETUP stage failed\n");
		return (-1);
	}
	if (g_usb_dbg)
		kprintf("usb:   SETUP stage ok\n");
	if (wLength != 0)
	{
		if (usb_control_data(devaddr, mps, in, data, wLength) != 0)
			return (-1);
	}
	/* STATUS stage: opposite direction, zero-length, DATA1. */
	if (usb_chan0(devaddr, wLength && in ? 0 : 1, mps, PID_DATA1, g_data, 0) != 0)
	{
		if (g_usb_dbg)
			kprintf("usb:   STATUS stage failed\n");
		return (-1);
	}
	if (g_usb_dbg)
		kprintf("usb:   STATUS stage ok (status dir=%s)\n", (wLength && in) ? "OUT" : "IN");
	return (0);
}

/** Control DATA stage (DATA1, may span packets). */
static int usb_control_data(u_int32_t devaddr, u_int32_t mps, int in, void *data, u_int32_t len)
{
	if (usb_chan0(devaddr, in, mps, PID_DATA1, data, len) != 0)
		return (-1);
	return (0);
}

/**
 * M7.1: enumerate the device on the root port — GET_DESCRIPTOR(device, 8) to learn
 * ep0's max packet size, SET_ADDRESS(1), then the full 18-byte device descriptor;
 * report vendor/product/class.  (On the Pi 3 this is the LAN9514 internal hub.)
 */
int aarch64_usb_enumerate(void)
{
	u_int32_t mps0 = 8;
	u_int16_t vid, pid;
	u_int8_t cls;

	/* GET_DESCRIPTOR (DEVICE), first 8 bytes, at address 0. */
	if (usb_control(0, mps0, 0x80, 6, (1 << 8) | 0, 0, 8, g_data) != 0)
	{
		kprintf("usb: GET_DESCRIPTOR(8) failed\n");
		return (-1);
	}
	mps0 = g_data[7]; /* bMaxPacketSize0 */
	if (mps0 == 0)
		mps0 = 8;

	/* SET_ADDRESS(1). */
	if (usb_control(0, mps0, 0x00, 5, 1, 0, 0, 0) != 0)
	{
		kprintf("usb: SET_ADDRESS control failed\n");
		return (-1);
	}
	usb_wait_us(20000); /* the device needs time to adopt the new address */

	/* Full 18-byte device descriptor at the new address. */
	if (usb_control(1, mps0, 0x80, 6, (1 << 8) | 0, 0, 18, g_data) != 0)
	{
		kprintf("usb: GET_DESCRIPTOR(18)@addr1 failed\n");
		return (-1);
	}
	vid = (u_int16_t)(g_data[8] | (g_data[9] << 8));
	pid = (u_int16_t)(g_data[10] | (g_data[11] << 8));
	cls = g_data[4];
	kprintf("usb: enumerated dev addr 1: VID=0x%X PID=0x%X class=%u mps0=%u%s\n",
	        vid,
	        pid,
	        cls,
	        mps0,
	        cls == 9 ? " (hub)" : "");
	return (0);
}

#endif /* BOARD_RPI3 */
