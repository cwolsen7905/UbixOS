/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2837 (Raspberry Pi 3) SD-card driver via the Arasan/EMMC SDHCI controller at
 * peripheral+0x300000 (M4 storage).  The Pi-3 firmware routes the microSD to the
 * Broadcom SDHOST and the EMMC to the WiFi SDIO; we re-mux GPIO48-53 to ALT3 to put
 * the card on the standard SDHCI, then run the SD init (CMD0/8/ACMD41/2/3/7) and
 * single/multi-block reads (CMD17/18).  Register layout + sequence per the BCM2835
 * peripherals datasheet + the SD physical-layer spec (the bare-metal-standard
 * approach; see docs/design/raspberry-pi-3b-bringup.md).
 *
 * v1 polls the controller (boot-time pool reads).  IRQ-driven completion
 * (sleep-on-IRQ) is the follow-up so runtime reads don't block the scheduler.
 */

#ifdef BOARD_RPI3

#include "bringup.h"
#include <sys/bus.h>       /* struct ubx_device + ubx_blk_ops */
#include <sys/descrip.h>   /* g_device_find (vfs_mount resolves the block device) */
#include <dev/partition.h> /* MBR partition parsing (sd0sN) */
#include <string.h>        /* memset / strncpy */

/* MMIO bases via the TTBR1 physmap. */
#define EMMC_BASE (PHYSMAP_BASE + 0x3F300000UL)
#define GPIO_BASE (PHYSMAP_BASE + 0x3F200000UL)
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
#define INT_CMD_DONE 0x00000001u
#define INT_CMD_TIMEOUT 0x00010000u
#define INT_DATA_TIMEOUT 0x00100000u
#define INT_ERROR_MASK 0x017E8000u

/* CONTROL0/1 bits. */
#define C0_HCTL_DWIDTH 0x00000002u
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
#define CMD_SET_BLOCKCNT 0x17020000u
#define CMD_APP_CMD 0x37000000u
#define CMD_SEND_OP_COND (0x29020000u | CMD_NEED_APP)
#define CMD_SEND_SCR (0x33220010u | CMD_NEED_APP)
#define CMD_SET_BUS_WIDTH (0x06020000u | CMD_NEED_APP)

#define SD_OK 0
#define SD_ERROR (-1)
#define SD_TIMEOUT (-2)

static u_int32_t g_sd_rca;    /* relative card address (from CMD3) */
static u_int32_t g_sd_scr[2]; /* SD config register words         */
static u_int32_t g_sd_ccs;    /* SCR_SUPP_CCS if a high-capacity (SDHC) card */
static u_int32_t g_sd_hv;     /* host controller spec version */
static int g_sd_err;          /* last error */

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
		r = sd_cmd(CMD_APP_CMD | (g_sd_rca != 0 ? CMD_RSPNS_48 : 0), g_sd_rca);
		if (g_sd_rca != 0 && r == 0)
		{
			g_sd_err = SD_ERROR;
			return (0);
		}
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

/**
 * Program the SDHCI clock divider for @freq Hz (base clock ~41.66 MHz) and wait
 * for the clock to stabilize.
 */
static int sd_clk(u_int32_t freq)
{
	u_int32_t d, c = 41666666u / freq, s = 32, x;
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

	/* Reset the host controller. */
	EMMC(EMMC_CONTROL0) = 0;
	EMMC(EMMC_CONTROL1) |= C1_SRST_HC;
	cnt = 100;
	while ((EMMC(EMMC_CONTROL1) & C1_SRST_HC) != 0 && cnt-- > 0)
		sd_wait_us(10000);
	if (cnt <= 0)
		return (SD_ERROR);

	EMMC(EMMC_CONTROL1) |= C1_CLK_INTLEN | C1_TOUNIT_MAX;
	sd_wait_us(10);
	if (sd_clk(400000) != SD_OK)
		return (SD_ERROR);
	EMMC(EMMC_INT_EN) = 0xFFFFFFFF;
	EMMC(EMMC_INT_MASK) = 0xFFFFFFFF;
	g_sd_scr[0] = g_sd_scr[1] = g_sd_rca = g_sd_ccs = 0;

	sd_cmd(CMD_GO_IDLE, 0);
	if (g_sd_err != SD_OK)
		return (SD_ERROR);
	sd_cmd(CMD_SEND_IF_COND, 0x000001AA);
	if (g_sd_err != SD_OK)
		return (SD_ERROR);

	cnt = 6;
	resp = 0;
	while ((resp & ACMD41_CMD_COMPLETE) == 0 && cnt-- > 0)
	{
		sd_wait_us(400);
		resp = sd_cmd(CMD_SEND_OP_COND, ACMD41_ARG_HC);
		if (g_sd_err != SD_OK)
			return (SD_ERROR);
	}
	if ((resp & ACMD41_CMD_COMPLETE) == 0 || (resp & ACMD41_VOLTAGE) == 0)
		return (SD_ERROR);
	if ((resp & ACMD41_CMD_CCS) != 0)
		g_sd_ccs = SCR_SUPP_CCS;

	sd_cmd(CMD_ALL_SEND_CID, 0);
	g_sd_rca = sd_cmd(CMD_SEND_REL_ADDR, 0) & CMD_RCA_MASK;
	if (g_sd_err != SD_OK)
		return (SD_ERROR);

	if (sd_clk(25000000) != SD_OK)
		return (SD_ERROR);
	sd_cmd(CMD_CARD_SELECT, g_sd_rca);
	if (g_sd_err != SD_OK)
		return (SD_ERROR);

	/* Read the SCR (one 8-byte block). */
	if (sd_status(SR_DAT_INHIBIT) != SD_OK)
		return (SD_ERROR);
	EMMC(EMMC_BLKSIZECNT) = (1u << 16) | 8u;
	sd_cmd(CMD_SEND_SCR, 0);
	if (g_sd_err != SD_OK)
		return (SD_ERROR);
	if (sd_int(INT_READ_RDY) != SD_OK)
		return (SD_ERROR);
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
		return (SD_ERROR);

	if ((g_sd_scr[0] & SCR_SD_BUS_WIDTH_4) != 0)
	{
		sd_cmd(CMD_SET_BUS_WIDTH, g_sd_rca | 2);
		if (g_sd_err == SD_OK)
			EMMC(EMMC_CONTROL0) |= C0_HCTL_DWIDTH;
	}
	g_sd_scr[0] &= ~SCR_SUPP_CCS;
	g_sd_scr[0] |= g_sd_ccs;
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

/* --- block-device registration (mirrors virtio_blk.c so the buffer cache + the
 *     MBR partition layer + the UbixFS mount path use the SD with no SD knowledge) --- */

static struct ubx_device sd_blk_dev;                      /* whole-disk device (sd0) */
static struct ubx_blk_ops sd_blk_ops;                     /* its block ops */
static struct ubp_partition sd_parts[MBR_MAX_PARTITIONS]; /* MBR partitions (sd0sN) */
static int sd_npart;
static int sd_ready;

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
 * ubx_blk_ops::write — M4 v1 is read-only; SD writes (CMD24/25) are a follow-up.
 */
static int sd_blk_write(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	(void)dev;
	(void)lba;
	(void)count;
	(void)buf;
	return (-1);
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

#endif /* BOARD_RPI3 */
