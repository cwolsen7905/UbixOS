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
static int g_root_lowspeed;      /* root device negotiated low-speed (set in usb_init) */
static u_int32_t g_smsc_addr;    /* USB address of the SMSC LAN9514 Ethernet (0 = not found) */
static u_int32_t g_smsc_mps0;    /* its ep0 max packet size */

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

	/* Slave/PIO mode (no DMA) — data moves through the FIFOs by CPU; just enable the
	 * global interrupt line.  (FreeBSD dwc_otg uses slave mode on the Pi; the internal
	 * DMA path could not reliably complete SET_ADDRESS.) */
	USB(GAHBCFG) = GAHBCFG_GINT;

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

/* (Slave/PIO mode moves data through the FIFOs by CPU load/store — no DMA, so no
 * cache maintenance of the data buffers is needed.) */

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

/* Slave/PIO-mode FIFO access (no DMA — FreeBSD dwc_otg's proven path on the Pi).
 * TX pushes packet words into the channel's DFIFO window; RX pops a status word
 * from GRXSTSP then reads the payload from the (channel-0) DFIFO window. */
#define GRXSTSP 0x020   /* RX status pop (reading it consumes the entry) */
#define GNPTXSTS 0x02C  /* non-periodic TX FIFO/queue status */
#define DFIFO(ch) (0x1000 + (ch) * 0x1000)
#define GINTSTS_RXFLVL (1u << 4)   /* RX FIFO non-empty */
#define GINTSTS_NPTXFE (1u << 5)   /* non-periodic TX FIFO empty */
#define GRXSTS_PKTSTS(x) (((x) >> 17) & 0xF)
#define GRXSTS_BCNT(x) (((x) >> 4) & 0x7FF)
#define GRXSTS_IN_DATA 2u  /* host: IN data packet received */
#define GRXSTS_IN_COMP 3u  /* host: IN transfer complete */
#define GRXSTS_HALTED 7u   /* host: channel halted */

/**
 * Run one control transfer on host channel 0 in SLAVE/PIO mode.  OUT/SETUP data is
 * pushed word-by-word into the channel TX FIFO; IN data is drained from the RX FIFO
 * via GRXSTSP.  Completion is HCINT.XFRC.  @return 0 on success, -1 on error;
 * retries NAK/transaction errors.
 */
static int usb_chan0(u_int32_t devaddr, int dir_in, u_int32_t mps, u_int32_t pid, void *buf, u_int32_t len)
{
	u_int32_t pktcnt = (len + mps - 1) / mps;
	int tries, i;

	if (pktcnt == 0)
		pktcnt = 1;

	for (tries = 0; tries < 4; tries++) /* capped: a control xfer completes fast or not at all */
	{
		u_int32_t s = 0;
		u_int32_t received = 0;
		/* IN XferSize is rounded up to whole packets; OUT is the exact byte count. */
		u_int32_t xfer = dir_in ? (pktcnt * mps) : len;

		usb_chan0_halt(); /* clean slate: disable the channel + clear interrupts */
		HCTSIZ(0) = (xfer & 0x7FFFF) | (pktcnt << 19) | (pid << 29);
		HCSPLT(0) = 0; /* no split — the root device is a high-speed hub */
		HCCHAR(0) = (mps & 0x7FF) | (0u << 11) /* ep0 */ | (dir_in ? HCCHAR_EPDIR_IN : 0) |
		            (g_root_lowspeed ? HCCHAR_LSDEV : 0) | (EPTYP_CONTROL << 18) | HCCHAR_MCNT1 |
		            ((devaddr & 0x7F) << 22) | HCCHAR_CHENA;

		/* OUT/SETUP: push the packet into the channel-0 TX FIFO (word by word). */
		if (!dir_in && len != 0)
		{
			u_int32_t words = (len + 3) / 4;
			u_int32_t w;
			for (i = 0; i < 100000 && (USB(GINTSTS) & GINTSTS_NPTXFE) == 0; i++)
				usb_wait_us(1);
			for (w = 0; w < words; w++)
				USB(DFIFO(0)) = ((const u_int32_t *)buf)[w];
		}

		/* Poll: drain RX-FIFO IN packets first (must pop GRXSTSP or the FIFO wedges),
		 * then watch HCINT for completion/error.  ~150 ms cap per attempt. */
		for (i = 0; i < 150000; i++)
		{
			if (dir_in && (USB(GINTSTS) & GINTSTS_RXFLVL) != 0)
			{
				u_int32_t rx = USB(GRXSTSP);
				u_int32_t bcnt = GRXSTS_BCNT(rx);
				if (GRXSTS_PKTSTS(rx) == GRXSTS_IN_DATA && bcnt != 0)
				{
					u_int32_t words = (bcnt + 3) / 4, w, k;
					for (w = 0; w < words; w++)
					{
						u_int32_t v = USB(DFIFO(0)); /* must read all words to drain */
						u_int32_t off = received + w * 4;
						for (k = 0; k < 4 && (off + k) < len; k++)
							((u_int8_t *)buf)[off + k] = (u_int8_t)(v >> (8 * k));
					}
					received += bcnt;
				}
				continue; /* re-check for more RX before looking at HCINT */
			}
			s = HCINT(0);
			if (s & (HCINT_XFRC | HCINT_STALL | HCINT_NAK | HCINT_CHH | HCINT_ERRMASK))
				break;
			usb_wait_us(1);
		}
		if (s & HCINT_XFRC)
			return (0);
		if (s & HCINT_STALL)
		{
			kprintf("usb: ep0 STALL\n");
			return (-1); /* protocol stall — fatal, not retryable */
		}
		usb_wait_us(2000); /* NAK / XACTERR / halt — re-issue */
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
 * M7.2: enumerate the device currently at address 0 (just after a hub port reset),
 * assign it @newaddr, and report VID/PID/class.  @return the class, or -1.
 */
static int usb_enum_at0(u_int32_t newaddr)
{
	u_int32_t mps0 = 8;
	u_int16_t vid, pid;
	u_int8_t cls;

	if (usb_control(0, mps0, 0x80, 6, (1 << 8) | 0, 0, 8, g_data) != 0)
		return (-1);
	mps0 = g_data[7];
	if (mps0 == 0)
		mps0 = 8;
	if (usb_control(0, mps0, 0x00, 5, newaddr, 0, 0, 0) != 0)
		return (-1);
	usb_wait_us(20000);
	if (usb_control(newaddr, mps0, 0x80, 6, (1 << 8) | 0, 0, 18, g_data) != 0)
		return (-1);
	vid = (u_int16_t)(g_data[8] | (g_data[9] << 8));
	pid = (u_int16_t)(g_data[10] | (g_data[11] << 8));
	cls = g_data[4];
	kprintf("usb:   downstream addr %u: VID=0x%X PID=0x%X class=%u%s\n",
	        newaddr,
	        vid,
	        pid,
	        cls,
	        (vid == 0x0424) ? " (SMSC LAN9514 Ethernet)" : "");
	if (vid == 0x0424 && pid == 0xEC00) /* the LAN9514 Ethernet function */
	{
		g_smsc_addr = newaddr;
		g_smsc_mps0 = mps0;
	}
	return (cls);
}

/**
 * M7.2: walk the hub at @hubaddr (mps @mps) — SET_CONFIGURATION, read the hub
 * descriptor for the port count, power every port, then reset + enumerate each
 * connected downstream device.  The LAN9514's internal Ethernet is a high-speed
 * downstream device (no split transactions needed); external LS/FS devices need
 * split transactions (a later milestone) and will fail to enumerate here for now.
 */
static void usb_hub_walk(u_int32_t hubaddr, u_int32_t mps)
{
	u_int32_t nports, p, next = 2;

	if (usb_control(hubaddr, mps, 0x00, 9, 1, 0, 0, 0) != 0) /* SET_CONFIGURATION(1) */
	{
		kprintf("usb: hub SET_CONFIGURATION failed\n");
		return;
	}
	/* Class GET_DESCRIPTOR, type 0x29 (hub); bNbrPorts is byte 2. */
	if (usb_control(hubaddr, mps, 0xA0, 6, (0x29 << 8), 0, 8, g_data) != 0)
	{
		kprintf("usb: hub descriptor read failed\n");
		return;
	}
	nports = g_data[2];
	kprintf("usb: hub has %u ports; powering + walking\n", nports);

	for (p = 1; p <= nports; p++) /* SET_FEATURE PORT_POWER (8) on each port */
		(void)usb_control(hubaddr, mps, 0x23, 3, 8, p, 0, 0);
	usb_wait_us(100000); /* power-good settle */

	for (p = 1; p <= nports; p++)
	{
		u_int16_t st;

		if (usb_control(hubaddr, mps, 0xA3, 0, 0, p, 4, g_data) != 0) /* GET port status */
			continue;
		st = (u_int16_t)(g_data[0] | (g_data[1] << 8));
		if ((st & 0x1) == 0)
			continue; /* nothing connected */

		(void)usb_control(hubaddr, mps, 0x23, 3, 4, p, 0, 0); /* SET_FEATURE PORT_RESET (4) */
		usb_wait_us(60000);
		(void)usb_control(hubaddr, mps, 0x23, 1, 0x14, p, 0, 0); /* CLEAR_FEATURE C_PORT_RESET (20) */
		usb_wait_us(20000);

		if (usb_control(hubaddr, mps, 0xA3, 0, 0, p, 4, g_data) == 0)
		{
			st = (u_int16_t)(g_data[0] | (g_data[1] << 8));
			g_root_lowspeed = (st & (1u << 9)) ? 1 : 0; /* bit9 = low-speed */
		}
		if (usb_enum_at0(next) >= 0)
			next++;
	}
}

/* --- M7.4: SMSC LAN9514 Ethernet (registers via vendor control requests) --- */

#define SMSC_UR_WRITE_REG 0xA0
#define SMSC_UR_READ_REG 0xA1
#define SMSC_ID_REV 0x000
#define SMSC_MAC_CSR 0x100
#define SMSC_MAC_ADDRH 0x104
#define SMSC_MAC_ADDRL 0x108

/** Read a 32-bit SMSC register @off (vendor control-IN).  @return 0 on success. */
static int smsc_read_reg(u_int32_t off, u_int32_t *val)
{
	if (usb_control(g_smsc_addr, g_smsc_mps0, 0xC0, SMSC_UR_READ_REG, 0, (u_int16_t)off, 4, g_data) != 0)
		return (-1);
	*val = (u_int32_t)g_data[0] | ((u_int32_t)g_data[1] << 8) | ((u_int32_t)g_data[2] << 16) |
	       ((u_int32_t)g_data[3] << 24);
	return (0);
}

/** Write a 32-bit SMSC register @off (vendor control-OUT).  @return 0 on success. */
static int smsc_write_reg(u_int32_t off, u_int32_t val)
{
	g_data[0] = (u_int8_t)val;
	g_data[1] = (u_int8_t)(val >> 8);
	g_data[2] = (u_int8_t)(val >> 16);
	g_data[3] = (u_int8_t)(val >> 24);
	return (usb_control(g_smsc_addr, g_smsc_mps0, 0x40, SMSC_UR_WRITE_REG, 0, (u_int16_t)off, 4, g_data));
}

/* The Pi's MAC lives in the VideoCore firmware (no EEPROM on the LAN9514), so read
 * it from the property mailbox — GET_BOARD_MAC_ADDRESS returns 6 bytes. */
u_int8_t smsc_mac[6];

/** A MAC is usable if it is unicast (bit0 of octet0 clear), not all-zero, not all-FF. */
static int mac_is_valid(const u_int8_t m[6])
{
	int i, all0 = 1, allf = 1;

	for (i = 0; i < 6; i++)
	{
		if (m[i] != 0x00)
			all0 = 0;
		if (m[i] != 0xFF)
			allf = 0;
	}
	return (!all0 && !allf && (m[0] & 0x01) == 0);
}

/** Read a mailbox property tag returning up to 2 words into g_usb_mbox[5],[6]. */
static int usb_mbox_get2(u_int32_t tag)
{
	g_usb_mbox[0] = 8 * 4;
	g_usb_mbox[1] = 0;
	g_usb_mbox[2] = tag;
	g_usb_mbox[3] = 8;
	g_usb_mbox[4] = 0;
	g_usb_mbox[5] = 0;
	g_usb_mbox[6] = 0;
	g_usb_mbox[7] = 0;
	return (aarch64_mbox_prop(g_usb_mbox));
}

/**
 * Pick a valid MAC into smsc_mac: prefer the board's real MAC (VideoCore mailbox);
 * else synthesize a stable locally-administered MAC (0x02-prefixed, unicast) derived
 * from the board serial; else a fixed fallback.  uBixOS always ends up with a
 * usable MAC even when the firmware / chip has none.
 */
static void usb_pick_mac(void)
{
	/* 1) the board's assigned MAC (GET_BOARD_MAC_ADDRESS). */
	if (usb_mbox_get2(0x00010003) == 0)
	{
		smsc_mac[0] = (u_int8_t)g_usb_mbox[5];
		smsc_mac[1] = (u_int8_t)(g_usb_mbox[5] >> 8);
		smsc_mac[2] = (u_int8_t)(g_usb_mbox[5] >> 16);
		smsc_mac[3] = (u_int8_t)(g_usb_mbox[5] >> 24);
		smsc_mac[4] = (u_int8_t)g_usb_mbox[6];
		smsc_mac[5] = (u_int8_t)(g_usb_mbox[6] >> 8);
		if (mac_is_valid(smsc_mac))
			return;
	}
	/* 2) a stable locally-administered MAC (02:...) from the board serial. */
	if (usb_mbox_get2(0x00010004) == 0 && (g_usb_mbox[5] | g_usb_mbox[6]) != 0)
	{
		smsc_mac[0] = 0x02; /* locally administered, unicast */
		smsc_mac[1] = (u_int8_t)(g_usb_mbox[6]);
		smsc_mac[2] = (u_int8_t)(g_usb_mbox[5] >> 24);
		smsc_mac[3] = (u_int8_t)(g_usb_mbox[5] >> 16);
		smsc_mac[4] = (u_int8_t)(g_usb_mbox[5] >> 8);
		smsc_mac[5] = (u_int8_t)(g_usb_mbox[5]);
		return;
	}
	/* 3) fixed fallback. */
	smsc_mac[0] = 0x02;
	smsc_mac[1] = 0x55;
	smsc_mac[2] = 0x42;
	smsc_mac[3] = 0x00;
	smsc_mac[4] = 0x00;
	smsc_mac[5] = 0x01;
}

/**
 * M7.4 (step 1+): SET_CONFIGURATION on the SMSC LAN9514, read the chip ID/revision,
 * and obtain the MAC (from the chip registers if programmed, else the VideoCore
 * mailbox).  Bulk data path (endpoints, TX/RX framing, lwIP bridge) is next.
 *
 * @return 0 on success, -1 if the device isn't present or ID read fails.
 */
int aarch64_usb_smsc_probe(void)
{
	u_int32_t id, macl = 0, mach = 0;

	if (g_smsc_addr == 0)
	{
		kprintf("smsc: no LAN9514 Ethernet device enumerated\n");
		return (-1);
	}
	(void)smsc_write_reg; /* used by the init sequence in the next step */

	/* Configure the device before register access (it is enumerated but not yet
	 * configured — vendor register reads can STALL until SET_CONFIGURATION). */
	if (usb_control(g_smsc_addr, g_smsc_mps0, 0x00, 9, 1, 0, 0, 0) != 0)
		kprintf("smsc: SET_CONFIGURATION failed\n");
	usb_wait_us(5000);

	if (smsc_read_reg(SMSC_ID_REV, &id) != 0)
	{
		kprintf("smsc: ID_REV read failed\n");
		return (-1);
	}
	kprintf("smsc: LAN9514 ID_REV=0x%X\n", id);

	(void)smsc_read_reg(SMSC_MAC_ADDRL, &macl);
	(void)smsc_read_reg(SMSC_MAC_ADDRH, &mach);
	/* A programmed MAC is neither all-zero (never set) nor all-ones (erased flash /
	 * no EEPROM — the Pi case).  Otherwise fall back to the VideoCore mailbox. */
	if (macl != 0 && macl != 0xFFFFFFFF)
	{
		smsc_mac[0] = (u_int8_t)macl;
		smsc_mac[1] = (u_int8_t)(macl >> 8);
		smsc_mac[2] = (u_int8_t)(macl >> 16);
		smsc_mac[3] = (u_int8_t)(macl >> 24);
		smsc_mac[4] = (u_int8_t)mach;
		smsc_mac[5] = (u_int8_t)(mach >> 8);
		kprintf("smsc: MAC (chip regs) %X:%X:%X:%X:%X:%X\n",
		        smsc_mac[0],
		        smsc_mac[1],
		        smsc_mac[2],
		        smsc_mac[3],
		        smsc_mac[4],
		        smsc_mac[5]);
	}
	else
	{
		usb_pick_mac(); /* mailbox → serial-derived LA MAC → fixed; always valid */
		kprintf("smsc: MAC %X:%X:%X:%X:%X:%X\n",
		        smsc_mac[0],
		        smsc_mac[1],
		        smsc_mac[2],
		        smsc_mac[3],
		        smsc_mac[4],
		        smsc_mac[5]);
	}
	return (0);
}

/**
 * M7.1: enumerate the device on the root port — GET_DESCRIPTOR(device, 8) to learn
 * ep0's max packet size, SET_ADDRESS(1), then the full 18-byte device descriptor;
 * report vendor/product/class.  (On the Pi 3 this is the LAN9514 internal hub.)
 * M7.2: if it is a hub, walk it to reach the downstream devices (the Ethernet).
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

	/* M7.2: if the root device is a hub (the LAN9514), walk it to reach the
	 * downstream devices — including the internal SMSC Ethernet function. */
	if (cls == 9)
		usb_hub_walk(1, mps0);

	/* M7.4: probe the SMSC Ethernet's registers if we reached it. */
	if (g_smsc_addr != 0)
		aarch64_usb_smsc_probe();
	return (0);
}

#endif /* BOARD_RPI3 */
