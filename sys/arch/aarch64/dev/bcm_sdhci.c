/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2837/BCM2711 (Raspberry Pi 3 / Pi 4) SD-card driver via the standard SDHCI
 * register interface.  The controller and its register layout are the same on both
 * SoCs; only the base address and a few board deltas differ:
 *   - Pi 3 (BCM2837): the Arasan/EMMC controller at peripheral+0x300000.  The
 *     firmware routes the microSD to the Broadcom SDHOST and the EMMC to the WiFi
 *     SDIO, so we re-mux GPIO48-53 to ALT3 to put the card on the SDHCI.
 *   - Pi 4 (BCM2711): the dedicated EMMC2 controller at peripheral+0x340000.  The
 *     microSD is hard-wired to EMMC2 on its own SD-interface pins (no GPIO re-mux),
 *     and its base clock is read as clock id 12 (EMMC2), not 1 (EMMC).
 * Common path: the SD init (CMD0/8/ACMD41/2/3/7) and single/multi-block reads
 * (CMD17/18) / writes (CMD24/25) per the SD physical-layer spec (the bare-metal-
 * standard approach; see docs/design/raspberry-pi-3b-bringup.md and
 * raspberry-pi-4-400-bringup.md).
 *
 * v1 polls the controller (boot-time pool reads).  IRQ-driven completion
 * (sleep-on-IRQ) is the follow-up so runtime reads don't block the scheduler.
 */

#if defined(BOARD_RPI3) || defined(BOARD_RPI4)

#include "bringup.h"       /* defines BCM_PERI_BASE for the MMIO bases below */
#include <sys/bus.h>       /* struct ubx_device + ubx_blk_ops */
#include <sys/descrip.h>   /* g_device_find (vfs_mount resolves the block device) */
#include <dev/partition.h> /* MBR partition parsing (sd0sN) */
#include <string.h>        /* memset / strncpy */

/* MMIO bases via the TTBR1 physmap.  The SDHCI controller lives at a different
 * peripheral offset on the two SoCs (Pi 3 Arasan EMMC = +0x300000; Pi 4 EMMC2 =
 * +0x340000), and the mailbox EMMC clock id + base-clock fallback differ. */
#define GPIO_BASE (PHYSMAP_BASE + BCM_PERI_BASE + 0x200000UL)
#ifdef BOARD_RPI4
#define EMMC_BASE (PHYSMAP_BASE + BCM_PERI_BASE + 0x340000UL) /* EMMC2 (dedicated SD) */
#define SD_MBOX_CLOCK_ID 12u                                  /* mailbox clock id: EMMC2 */
#define SD_BASE_CLK_FALLBACK 100000000u                       /* EMMC2 default base clock */
#else
#define EMMC_BASE (PHYSMAP_BASE + BCM_PERI_BASE + 0x300000UL) /* Arasan EMMC */
#define SD_MBOX_CLOCK_ID 1u                                   /* mailbox clock id: EMMC */
#define SD_BASE_CLK_FALLBACK 41666666u
#endif
#define EMMC(off) (*(volatile u_int32_t *)(EMMC_BASE + (off)))
#define GPIO(off) (*(volatile u_int32_t *)(GPIO_BASE + (off)))

/* EMMC registers (offsets from EMMC_BASE). */
#define EMMC_ARG2 0x00
#define EMMC_BLKSIZECNT 0x04
#define EMMC_ARG1 0x08
#define EMMC_CMDTM 0x0C
#define EMMC_RESP0 0x10
#define EMMC_RESP1 0x14
#define EMMC_RESP2 0x18
#define EMMC_RESP3 0x1C
#define EMMC_DATA 0x20
#define EMMC_STATUS 0x24
#define EMMC_CONTROL0 0x28
#define EMMC_CONTROL1 0x2C
#define EMMC_INTERRUPT 0x30
#define EMMC_INT_MASK 0x34
#define EMMC_INT_EN 0x38
#define EMMC_SLOTISR_VER 0xFC

/* Command flags. */
#define CMD_NEED_APP 0x80000000u
#define CMD_RSPNS_48 0x00020000u
#define CMD_RCA_MASK 0xFFFF0000u

/* STATUS bits. */
#define SR_READ_AVAILABLE 0x00000800u
#define SR_DAT_INHIBIT 0x00000002u
#define SR_CMD_INHIBIT 0x00000001u

/* INTERRUPT bits. */
#define INT_READ_RDY 0x00000020u
#define INT_WRITE_RDY 0x00000010u
#define INT_DATA_DONE 0x00000002u
#define INT_CMD_DONE 0x00000001u
#define INT_CMD_TIMEOUT 0x00010000u
#define INT_DATA_TIMEOUT 0x00100000u
#define INT_ERROR_MASK 0x017E8000u

/* CONTROL0/1 bits. */
#define C0_HCTL_DWIDTH 0x00000002u
#define C0_SD_BUS_POWER 0x00000100u   /* SDHCI Power Control: SD Bus Power (EMMC2) */
#define C0_SD_BUS_VOLT_33 0x00000E00u /* SDHCI Power Control: 3.3 V select (bits 11:9 = 0x7) */
#define C1_SRST_HC 0x01000000u
#define C1_TOUNIT_MAX 0x000E0000u
#define C1_CLK_EN 0x00000004u
#define C1_CLK_STABLE 0x00000002u
#define C1_CLK_INTLEN 0x00000001u

/* SLOTISR_VER. */
#define HOST_SPEC_NUM 0x00FF0000u
#define HOST_SPEC_NUM_SHIFT 16
#define HOST_SPEC_V2 1

/* SCR / OCR. */
#define SCR_SD_BUS_WIDTH_4 0x00000400u
#define SCR_SUPP_SET_BLKCNT 0x02000000u
#define SCR_SUPP_CCS 0x00000001u
#define ACMD41_VOLTAGE 0x00FF8000u
#define ACMD41_CMD_COMPLETE 0x80000000u
#define ACMD41_CMD_CCS 0x40000000u
#define ACMD41_ARG_HC 0x51FF8000u

/* SD commands (encoded for CMDTM: index<<24 | flags). */
#define CMD_GO_IDLE 0x00000000u
#define CMD_ALL_SEND_CID 0x02010000u
#define CMD_SEND_REL_ADDR 0x03020000u
#define CMD_CARD_SELECT 0x07030000u
#define CMD_SEND_IF_COND 0x08020000u
#define CMD_STOP_TRANS 0x0C030000u
#define CMD_READ_SINGLE 0x11220010u
#define CMD_READ_MULTI 0x12220032u
#define CMD_WRITE_SINGLE 0x18220000u /* CMD24: like READ_SINGLE but DAT_DIR=host→card */
#define CMD_WRITE_MULTI 0x19220022u  /* CMD25: multi-block + blkcnt-en, DAT_DIR write */
#define CMD_SET_BLOCKCNT 0x17020000u
#define CMD_APP_CMD 0x37000000u
#define CMD_SEND_OP_COND (0x29020000u | CMD_NEED_APP)
#define CMD_SEND_SCR (0x33220010u | CMD_NEED_APP)
#define CMD_SET_BUS_WIDTH (0x06020000u | CMD_NEED_APP)

#define SD_OK 0
#define SD_ERROR (-1)
#define SD_TIMEOUT (-2)

static u_int32_t g_sd_rca;                             /* relative card address (from CMD3) */
static u_int32_t g_sd_scr[2];                          /* SD config register words         */
static u_int32_t g_sd_ccs;                             /* SCR_SUPP_CCS if a high-capacity (SDHC) card */
static u_int32_t g_sd_hv;                              /* host controller spec version */
static int g_sd_err;                                   /* last error */
static u_int32_t g_sd_last_int;                        /* last INTERRUPT value seen by sd_int (debug) */
static u_int32_t g_sd_base_clk = SD_BASE_CLK_FALLBACK; /* EMMC base clock (Hz); set from the mailbox */

/** Busy-wait @us microseconds off the always-running virtual counter. */
static void sd_wait_us(u_int32_t us)
{
	u_int64_t f, t0, t, target;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(t0));
	target = t0 + (f * (u_int64_t)us) / 1000000ULL;
	do
		__asm__ volatile("mrs %0, cntvct_el0" : "=r"(t));
	while (t < target);
}

/**
 * Wait for the STATUS bits in @mask to clear (controller idle), or fail on error.
 */
static int sd_status(u_int32_t mask)
{
	int cnt = 500000;
	while ((EMMC(EMMC_STATUS) & mask) != 0 && (EMMC(EMMC_INTERRUPT) & INT_ERROR_MASK) == 0 && cnt-- > 0)
		sd_wait_us(1);
	return ((cnt <= 0 || (EMMC(EMMC_INTERRUPT) & INT_ERROR_MASK) != 0) ? SD_ERROR : SD_OK);
}

/**
 * Wait for the INTERRUPT bit in @mask, acknowledge it, or report timeout/error.
 */
static int sd_int(u_int32_t mask)
{
	u_int32_t r, m = mask | INT_ERROR_MASK;
	int cnt = 1000000;
	while ((EMMC(EMMC_INTERRUPT) & m) == 0 && cnt-- > 0)
		sd_wait_us(1);
	r = EMMC(EMMC_INTERRUPT);
	g_sd_last_int = r;
	if (cnt <= 0 || (r & (INT_CMD_TIMEOUT | INT_DATA_TIMEOUT)) != 0)
	{
		EMMC(EMMC_INTERRUPT) = r;
		return (SD_TIMEOUT);
	}
	if ((r & INT_ERROR_MASK) != 0)
	{
		EMMC(EMMC_INTERRUPT) = r;
		return (SD_ERROR);
	}
	EMMC(EMMC_INTERRUPT) = mask;
	return (SD_OK);
}

/**
 * Issue an SD command (CMDTM-encoded @code) with argument @arg; ACMD commands are
 * prefixed with CMD55.  @return the 48-bit response (RESP0), or 0 on error
 * (g_sd_err set).
 */
static u_int32_t sd_cmd(u_int32_t code, u_int32_t arg)
{
	u_int32_t r;

	g_sd_err = SD_OK;
	if ((code & CMD_NEED_APP) != 0)
	{
		/* CMD55 (APP_CMD) always returns R1 (48-bit) — read it so the card enters
		 * the APP-CMD state before the ACMD.  The first ACMD41 runs with rca=0, so
		 * the response type must NOT be gated on rca (the bug that failed ACMD41). */
		(void)sd_cmd(CMD_APP_CMD | CMD_RSPNS_48, g_sd_rca);
		if (g_sd_err != SD_OK)
			return (0); /* CMD55 failed → the app command can't proceed */
		code &= ~CMD_NEED_APP;
	}
	if (sd_status(SR_CMD_INHIBIT) != SD_OK)
	{
		g_sd_err = SD_ERROR;
		return (0);
	}
	EMMC(EMMC_INTERRUPT) = EMMC(EMMC_INTERRUPT);
	EMMC(EMMC_ARG1) = arg;
	EMMC(EMMC_CMDTM) = code;
	if (code == CMD_SEND_OP_COND)
		sd_wait_us(1000);
	else if (code == CMD_SEND_IF_COND || code == CMD_APP_CMD)
		sd_wait_us(100);
	if ((r = sd_int(INT_CMD_DONE)) != SD_OK)
	{
		g_sd_err = r;
		return (0);
	}
	return (EMMC(EMMC_RESP0));
}

/* VideoCore property mailbox (shared transport in bcm_mbox.c): read the real EMMC
 * base clock.  The firmware leaves the EMMC clock at whatever it configured for the
 * WiFi SDIO, not the 41.66 MHz the divider assumed, so divisor=1 (for 25 MHz)
 * produced a garbled clock and CMD7 timed out. */
static volatile u_int32_t g_mbox[36] __attribute__((aligned(16)));

/** Property-mailbox GET_CLOCK_RATE for @clock_id (1 = EMMC).  @return Hz, or 0. */
static u_int32_t mbox_clock_rate(u_int32_t clock_id)
{
	g_mbox[0] = 8 * 4;      /* total message size */
	g_mbox[1] = 0;          /* request */
	g_mbox[2] = 0x00030002; /* GET_CLOCK_RATE tag */
	g_mbox[3] = 8;          /* value buffer size */
	g_mbox[4] = 4;          /* request length (clock id) */
	g_mbox[5] = clock_id;   /* (in) clock id */
	g_mbox[6] = 0;          /* (out) rate */
	g_mbox[7] = 0;          /* end tag */

	if (aarch64_mbox_prop(g_mbox) != 0)
		return (0);
	return (g_mbox[6]);
}

/**
 * Program the SDHCI clock divider for @freq Hz (against the real EMMC base clock,
 * g_sd_base_clk) and wait for the clock to stabilize.
 */
static int sd_clk(u_int32_t freq)
{
	u_int32_t d, c = g_sd_base_clk / freq, s = 32, x;
	int cnt = 100000;

	while ((EMMC(EMMC_STATUS) & (SR_CMD_INHIBIT | SR_DAT_INHIBIT)) != 0 && cnt-- > 0)
		sd_wait_us(1);
	EMMC(EMMC_CONTROL1) &= ~C1_CLK_EN;
	sd_wait_us(10);
	if (g_sd_hv > HOST_SPEC_V2)
	{
		d = c; /* SDHCI v3: 10-bit divided-clock value */
	}
	else
	{
		/* v1/v2: divisor is a power of two (the field holds the shift). */
		x = c - 1;
		if (x == 0)
			s = 0;
		else
		{
			if ((x & 0xFFFF0000u) == 0)
			{
				x <<= 16;
				s -= 16;
			}
			if ((x & 0xFF000000u) == 0)
			{
				x <<= 8;
				s -= 8;
			}
			if ((x & 0xF0000000u) == 0)
			{
				x <<= 4;
				s -= 4;
			}
			if ((x & 0xC0000000u) == 0)
			{
				x <<= 2;
				s -= 2;
			}
			if ((x & 0x80000000u) == 0)
				s -= 1;
			if (s > 0)
				s--;
			if (s > 7)
				s = 7;
		}
		d = (g_sd_hv > HOST_SPEC_V2) ? c : (u_int32_t)(1u << s);
	}
	EMMC(EMMC_CONTROL1) = (EMMC(EMMC_CONTROL1) & 0xFFFF003F) | ((d & 0xFF) << 8) | (((d >> 8) & 0x3) << 6);
	sd_wait_us(10);
	EMMC(EMMC_CONTROL1) |= C1_CLK_EN;
	sd_wait_us(10);
	cnt = 10000;
	while ((EMMC(EMMC_CONTROL1) & C1_CLK_STABLE) == 0 && cnt-- > 0)
		sd_wait_us(10);
	return (cnt <= 0 ? SD_ERROR : SD_OK);
}

/** Route GPIO48-53 (CLK/CMD/DAT0-3) to ALT3 so the microSD lands on the EMMC. */
static void sd_gpio_init(void)
{
#ifdef BOARD_RPI4
	/* Pi 4: the microSD is hard-wired to the EMMC2 controller on its own dedicated
	 * SD-interface pins (not GPIO 48-53), and the VideoCore firmware already powered
	 * and read the card to load start4.elf + kernel8.img — so no GPIO ALT muxing or
	 * pull configuration is needed here. */
#else
	u_int32_t r;

	/* GPFSEL4: GPIO48 (field 8) + GPIO49 (field 9) = ALT3 (7). */
	r = GPIO(0x10);
	r &= ~((7u << (8 * 3)) | (7u << (9 * 3)));
	r |= (7u << (8 * 3)) | (7u << (9 * 3));
	GPIO(0x10) = r;
	/* GPFSEL5: GPIO50-53 (fields 0-3) = ALT3 (7). */
	r = GPIO(0x14);
	r &= ~((7u << 0) | (7u << 3) | (7u << 6) | (7u << 9));
	r |= (7u << 0) | (7u << 3) | (7u << 6) | (7u << 9);
	GPIO(0x14) = r;
	/* Disable pulls on GPIO48-53 (bank 1, GPPUDCLK1 bits 16-21). */
	GPIO(0x94) = 0; /* GPPUD = off */
	sd_wait_us(2);
	GPIO(0x9C) = (0x3Fu << 16); /* GPPUDCLK1: GPIO48..53 */
	sd_wait_us(2);
	GPIO(0x9C) = 0;
#endif
}

/**
 * Bring up the SD card on the EMMC: reset, clock, the SD init command sequence,
 * SCR read, and 4-bit/high-speed selection.
 *
 * @return SD_OK on success, SD_ERROR/SD_TIMEOUT otherwise.
 */
int aarch64_sd_card_init(void)
{
	int cnt, r;
	u_int32_t resp;

	sd_gpio_init();
	g_sd_hv = (EMMC(EMMC_SLOTISR_VER) & HOST_SPEC_NUM) >> HOST_SPEC_NUM_SHIFT;
	g_sd_base_clk = mbox_clock_rate(SD_MBOX_CLOCK_ID); /* 1 = EMMC (Pi 3), 12 = EMMC2 (Pi 4) */
	if (g_sd_base_clk < 1000000u || g_sd_base_clk > 500000000u)
		g_sd_base_clk = SD_BASE_CLK_FALLBACK; /* fall back to the board's common default */
	kprintf("sd: EMMC SLOTISR_VER=0x%X hv=%u base=%u Hz\n", EMMC(EMMC_SLOTISR_VER), g_sd_hv, g_sd_base_clk);

	/* Reset the host controller. */
	EMMC(EMMC_CONTROL0) = 0;
	EMMC(EMMC_CONTROL1) |= C1_SRST_HC;
	cnt = 100;
	while ((EMMC(EMMC_CONTROL1) & C1_SRST_HC) != 0 && cnt-- > 0)
		sd_wait_us(10000);
	if (cnt <= 0)
	{
		kprintf("sd: HC reset timeout\n");
		return (SD_ERROR);
	}

#ifdef BOARD_RPI4
	/* EMMC2 is a standard SDHCI 3.0 controller: enable SD Bus Power at 3.3 V before
	 * any command (the Pi 3 Arasan hard-wires power on, so it never needed this).
	 * Without it the controller won't drive the bus and CMD0 never completes. */
	EMMC(EMMC_CONTROL0) |= C0_SD_BUS_VOLT_33 | C0_SD_BUS_POWER;
	sd_wait_us(10);
#endif

	EMMC(EMMC_CONTROL1) |= C1_CLK_INTLEN | C1_TOUNIT_MAX;
	sd_wait_us(10);
	if (sd_clk(400000) != SD_OK)
	{
		kprintf("sd: clk(400kHz) failed\n");
		return (SD_ERROR);
	}
	EMMC(EMMC_INT_EN) = 0xFFFFFFFF;
	EMMC(EMMC_INT_MASK) = 0xFFFFFFFF;
	g_sd_scr[0] = g_sd_scr[1] = g_sd_rca = g_sd_ccs = 0;

	sd_cmd(CMD_GO_IDLE, 0);
	if (g_sd_err != SD_OK)
	{
		kprintf("sd: CMD0 (GO_IDLE) failed (int=0x%X status=0x%X ctl0=0x%X)\n",
		        g_sd_last_int,
		        EMMC(EMMC_STATUS),
		        EMMC(EMMC_CONTROL0));
		return (SD_ERROR);
	}
	resp = sd_cmd(CMD_SEND_IF_COND, 0x000001AA);
	if (g_sd_err != SD_OK)
	{
		kprintf("sd: CMD8 (SEND_IF_COND) failed\n");
		return (SD_ERROR);
	}
	kprintf("sd: CMD0/CMD8 ok (CMD8 resp=0x%X); ACMD41...\n", resp);

	cnt = 6;
	resp = 0;
	while ((resp & ACMD41_CMD_COMPLETE) == 0 && cnt-- > 0)
	{
		sd_wait_us(400);
		resp = sd_cmd(CMD_SEND_OP_COND, ACMD41_ARG_HC);
		if (g_sd_err != SD_OK)
		{
			kprintf("sd: ACMD41 command error\n");
			return (SD_ERROR);
		}
	}
	kprintf("sd: ACMD41 resp=0x%X\n", resp);
	if ((resp & ACMD41_CMD_COMPLETE) == 0 || (resp & ACMD41_VOLTAGE) == 0)
	{
		kprintf("sd: ACMD41 no complete/voltage\n");
		return (SD_ERROR);
	}
	if ((resp & ACMD41_CMD_CCS) != 0)
		g_sd_ccs = SCR_SUPP_CCS;

	sd_cmd(CMD_ALL_SEND_CID, 0);
	if (g_sd_err != SD_OK)
	{
		kprintf("sd: CMD2 (ALL_SEND_CID) failed\n");
		return (SD_ERROR);
	}
	g_sd_rca = sd_cmd(CMD_SEND_REL_ADDR, 0) & CMD_RCA_MASK;
	if (g_sd_err != SD_OK)
	{
		kprintf("sd: CMD3 (SEND_REL_ADDR) failed\n");
		return (SD_ERROR);
	}
	kprintf("sd: CMD2/CMD3 ok, RCA=0x%X\n", g_sd_rca);

	if (sd_clk(25000000) != SD_OK)
	{
		kprintf("sd: clk(25MHz) failed\n");
		return (SD_ERROR);
	}
	sd_cmd(CMD_CARD_SELECT, g_sd_rca);
	if (g_sd_err != SD_OK)
	{
		kprintf("sd: CMD7 (CARD_SELECT) failed (int=0x%X status=0x%X)\n", g_sd_last_int, EMMC(EMMC_STATUS));
		return (SD_ERROR);
	}
	kprintf("sd: CMD7 ok, reading SCR...\n");

	/* Read the SCR (one 8-byte block).  Non-fatal: the SCR only adds 4-bit-bus and
	 * SET_BLKCNT support; CCS (high-capacity) is already known from ACMD41, so a
	 * failed SCR read just leaves the card in 1-bit mode (slower but functional). */
	if (sd_status(SR_DAT_INHIBIT) == SD_OK)
	{
		EMMC(EMMC_BLKSIZECNT) = (1u << 16) | 8u;
		sd_cmd(CMD_SEND_SCR, 0);
		if (g_sd_err == SD_OK && sd_int(INT_READ_RDY) == SD_OK)
		{
			cnt = 100000;
			r = 0;
			while (r < 2 && cnt-- > 0)
			{
				if ((EMMC(EMMC_STATUS) & SR_READ_AVAILABLE) != 0)
					g_sd_scr[r++] = EMMC(EMMC_DATA);
				else
					sd_wait_us(1);
			}
			if (r != 2)
				kprintf("sd: SCR read incomplete (%d words) — using 1-bit mode\n", r);
		}
		else
		{
			kprintf("sd: SCR command failed — using 1-bit mode\n");
		}
	}

	if ((g_sd_scr[0] & SCR_SD_BUS_WIDTH_4) != 0)
	{
		sd_cmd(CMD_SET_BUS_WIDTH, g_sd_rca | 2);
		if (g_sd_err == SD_OK)
			EMMC(EMMC_CONTROL0) |= C0_HCTL_DWIDTH;
	}
	g_sd_scr[0] &= ~SCR_SUPP_CCS;
	g_sd_scr[0] |= g_sd_ccs;
	kprintf("sd: card ready (rca=0x%X ccs=%u scr0=0x%X)\n", g_sd_rca, g_sd_ccs, g_sd_scr[0]);
	return (SD_OK);
}

/**
 * Read @num 512-byte blocks starting at LBA @lba into @buffer (a kernel pointer).
 *
 * @return the number of bytes read, or -1 on error.
 */
int aarch64_sd_readblock(u_int32_t lba, u_int8_t *buffer, u_int32_t num)
{
	u_int32_t c = 0, d;
	u_int32_t *buf = (u_int32_t *)(void *)buffer;

	if (num < 1)
		num = 1;
	if (sd_status(SR_DAT_INHIBIT) != SD_OK)
		return (-1);

	if (g_sd_ccs != 0)
	{
		if (num > 1 && (g_sd_scr[0] & SCR_SUPP_SET_BLKCNT) != 0)
		{
			sd_cmd(CMD_SET_BLOCKCNT, num);
			if (g_sd_err != SD_OK)
				return (-1);
		}
		EMMC(EMMC_BLKSIZECNT) = (num << 16) | 512u;
		sd_cmd(num == 1 ? CMD_READ_SINGLE : CMD_READ_MULTI, lba);
		if (g_sd_err != SD_OK)
			return (-1);
	}
	else
	{
		EMMC(EMMC_BLKSIZECNT) = (1u << 16) | 512u;
	}

	while (c < num)
	{
		if (g_sd_ccs == 0)
		{
			sd_cmd(CMD_READ_SINGLE, (lba + c) * 512u);
			if (g_sd_err != SD_OK)
				return (-1);
		}
		if (sd_int(INT_READ_RDY) != SD_OK)
			return (-1);
		for (d = 0; d < 128; d++)
			buf[d] = EMMC(EMMC_DATA);
		buf += 128;
		c++;
	}
	if (num > 1 && (g_sd_scr[0] & SCR_SUPP_SET_BLKCNT) == 0 && g_sd_ccs != 0)
		sd_cmd(CMD_STOP_TRANS, 0);
	return ((int)(num * 512u));
}

/**
 * Write @num 512-byte blocks starting at LBA @lba from @buffer (a kernel pointer).
 * Mirror of aarch64_sd_readblock: CMD24 (single) / CMD25 (multi), pushing each block
 * into EMMC_DATA after WRITE_RDY and waiting DATA_DONE for the card's programming.
 *
 * @return the number of bytes written, or -1 on error.
 */
int aarch64_sd_writeblock(u_int32_t lba, const u_int8_t *buffer, u_int32_t num)
{
	u_int32_t c = 0, d;
	const u_int32_t *buf = (const u_int32_t *)(const void *)buffer;

	if (num < 1)
		num = 1;
	if (sd_status(SR_DAT_INHIBIT) != SD_OK)
		return (-1);

	if (g_sd_ccs != 0)
	{
		if (num > 1 && (g_sd_scr[0] & SCR_SUPP_SET_BLKCNT) != 0)
		{
			sd_cmd(CMD_SET_BLOCKCNT, num);
			if (g_sd_err != SD_OK)
				return (-1);
		}
		EMMC(EMMC_BLKSIZECNT) = (num << 16) | 512u;
		sd_cmd(num == 1 ? CMD_WRITE_SINGLE : CMD_WRITE_MULTI, lba);
		if (g_sd_err != SD_OK)
			return (-1);
	}
	else
	{
		EMMC(EMMC_BLKSIZECNT) = (1u << 16) | 512u;
	}

	while (c < num)
	{
		if (g_sd_ccs == 0)
		{
			sd_cmd(CMD_WRITE_SINGLE, (lba + c) * 512u);
			if (g_sd_err != SD_OK)
				return (-1);
		}
		if (sd_int(INT_WRITE_RDY) != SD_OK)
			return (-1);
		for (d = 0; d < 128; d++)
			EMMC(EMMC_DATA) = buf[d];
		if (g_sd_ccs == 0 && sd_int(INT_DATA_DONE) != SD_OK) /* per-command programming wait */
			return (-1);
		buf += 128;
		c++;
	}
	/* CCS multi/single: one DATA_DONE at the end of the transfer. */
	if (g_sd_ccs != 0 && sd_int(INT_DATA_DONE) != SD_OK)
		return (-1);
	if (num > 1 && (g_sd_scr[0] & SCR_SUPP_SET_BLKCNT) == 0 && g_sd_ccs != 0)
		sd_cmd(CMD_STOP_TRANS, 0);
	return ((int)(num * 512u));
}

/* --- block-device registration (mirrors virtio_blk.c so the buffer cache + the
 *     MBR partition layer + the UbixFS mount path use the SD with no SD knowledge) --- */

static struct ubx_device sd_blk_dev;                      /* whole-disk device (sd0) */
static struct ubx_blk_ops sd_blk_ops;                     /* its block ops */
static struct ubp_partition sd_parts[MBR_MAX_PARTITIONS]; /* MBR partitions (sd0sN) */
static int sd_npart;
static int sd_ready;
static int sd_write_ok; /* the write path passed its self-test */
static u_int8_t sd_scratch[512] __attribute__((aligned(4)));
static u_int8_t sd_verify[512] __attribute__((aligned(4)));

/**
 * ubx_blk_ops::read — read @count 512-byte sectors at @lba into @buf.
 * @return 0 on success, -1 on error.
 */
static int sd_blk_read(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	(void)dev;
	if (!sd_ready)
		return (-1);
	return (aarch64_sd_readblock(lba, (u_int8_t *)buf, count) == (int)(count * 512u) ? 0 : -1);
}

/**
 * ubx_blk_ops::write — write @count 512-byte sectors at @lba from @buf.
 * @return 0 on success, -1 on error.
 */
static int sd_blk_write(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	(void)dev;
	if (!sd_ready || !sd_write_ok) /* writes stay disabled until the self-test passes */
		return (-1);
	return (aarch64_sd_writeblock(lba, (const u_int8_t *)buf, count) == (int)(count * 512u) ? 0 : -1);
}

/**
 * Non-destructive write self-test: on a reserved scratch sector (the MBR→partition-1
 * gap, LBA 2000 — never part of any filesystem), save the original, write a pattern,
 * read it back + verify, then restore the original.  Enables writes only if the
 * round-trip matches, so a broken write path can never corrupt the pool.
 */
static void sd_write_selftest(void)
{
	const u_int32_t lba = 2000; /* reserved gap between the MBR and partition 1 */
	u_int32_t i;

	if (aarch64_sd_readblock(lba, sd_scratch, 1) != 512)
	{
		kprintf("sd: write self-test skipped (scratch read failed); pool read-only\n");
		return;
	}
	for (i = 0; i < 512; i++)
		sd_verify[i] = (u_int8_t)(i ^ 0xA5);
	if (aarch64_sd_writeblock(lba, sd_verify, 1) != 512)
	{
		kprintf("sd: write self-test FAILED (write error); pool read-only\n");
		return;
	}
	memset(sd_verify, 0, sizeof(sd_verify));
	if (aarch64_sd_readblock(lba, sd_verify, 1) != 512)
	{
		kprintf("sd: write self-test FAILED (readback error); pool read-only\n");
		(void)aarch64_sd_writeblock(lba, sd_scratch, 1);
		return;
	}
	for (i = 0; i < 512; i++)
	{
		if (sd_verify[i] != (u_int8_t)(i ^ 0xA5))
		{
			kprintf("sd: write self-test FAILED (mismatch @%u); pool read-only\n", i);
			(void)aarch64_sd_writeblock(lba, sd_scratch, 1);
			return;
		}
	}
	(void)aarch64_sd_writeblock(lba, sd_scratch, 1); /* restore the original content */
	sd_write_ok = 1;
	kprintf("sd: write self-test PASSED — writes enabled\n");
}

/** @return non-zero if SD writes are verified working (for the rw pool mount). */
int aarch64_sd_write_ok(void)
{
	return (sd_write_ok);
}

/**
 * g_device_find hook: resolve (major, minor) — minor 0 = the whole disk, minor N =
 * MBR partition N (sd0sN), so the pool root (type 0x9C) resolves by its minor.
 */
static void *sd_device_find(int major, int minor)
{
	int i;

	(void)major;
	if (!sd_ready)
		return (NULL);
	if (minor > 0)
	{
		for (i = 0; i < sd_npart; i++)
			if (sd_parts[i].minor == minor)
				return (&sd_parts[i].dev);
		return (NULL);
	}
	return (&sd_blk_dev);
}

/**
 * Bring up the microSD on the EMMC and register it as the block device sd0 (whole
 * disk + its MBR partitions), so vfs_mount can mount the UbixFS pool from it.
 *
 * @return the registered block device, or NULL on failure.
 */
struct ubx_device *aarch64_sd_init(void)
{
	if (aarch64_sd_card_init() != SD_OK)
	{
		kprintf("sd: card init failed\n");
		return (NULL);
	}

	sd_blk_ops.read = sd_blk_read;
	sd_blk_ops.write = sd_blk_write;
	memset(&sd_blk_dev, 0, sizeof(sd_blk_dev));
	sd_blk_dev.dev_blk_ops = &sd_blk_ops;
	strncpy(sd_blk_dev.dev_nameunit, "sd0", sizeof(sd_blk_dev.dev_nameunit) - 1);
	sd_ready = 1;
	g_device_find = sd_device_find;
	kprintf("sd: microSD on the EMMC ready (sd0)\n");
	sd_write_selftest(); /* verify the write path before any pool write can happen */

	sd_npart = mbr_parse_partitions(&sd_blk_dev, sd_parts, MBR_MAX_PARTITIONS);
	if (sd_npart < 0)
		sd_npart = 0;
	if (sd_npart == 0)
		kprintf("sd: no MBR partitions (bare disk)\n");
	return (&sd_blk_dev);
}

/**
 * @return the device minor of the UbixFS pool partition (MBR type 0x9C), or -1.
 */
int aarch64_sd_pool_minor(void)
{
	int i;

	for (i = 0; i < sd_npart; i++)
		if (sd_parts[i].type == MBR_TYPE_UBPOOL)
			return (sd_parts[i].minor);
	return (-1);
}

#endif /* BOARD_RPI3 || BOARD_RPI4 */
