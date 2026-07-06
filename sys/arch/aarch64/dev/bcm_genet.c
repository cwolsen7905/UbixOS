/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2711 (Raspberry Pi 4 / Pi 400) GENET v5 gigabit Ethernet driver (R4.5).  GENET
 * is a memory-mapped MAC (unlike the Pi 3's USB-attached LAN9514) at ARM physical
 * 0xFD580000, with an integrated UniMAC + MDIO block driving an external RGMII PHY
 * (BCM54213PE).  Its DMA "descriptors" are a register file inside the GENET block
 * (RX at +0x2000, TX at +0x4000), each a {LENGTH_STATUS, ADDRESS_LO, ADDRESS_HI}
 * triple pointing at a packet buffer in RAM.  A default ring (queue 16) walks those
 * descriptors with producer/consumer indices.
 *
 * This is a polled driver exposing the same send()/poll_rx()/mac/ready interface as
 * the SMSC NIC (sys/arch/aarch64/net/virtio_netif.c bridges it into lwIP), so no
 * interrupt plumbing is needed.  The register layout + init sequence follow
 * FreeBSD's BSD-licensed if_genet driver (sys/arm64/broadcom/genet).  GENET DMA is
 * not cache-coherent with the A72 dcache, so packet buffers get an explicit clean
 * (TX) / invalidate (RX) around each transfer, like the VideoCore mailbox path.
 * See docs/design/raspberry-pi-4-400-bringup.md.
 */

#ifdef BOARD_RPI4

#include "bringup.h"
#include <string.h> /* memcpy / memset */

/* GENET register block via the TTBR1 physmap (mapped Device by the Pi 4 MMU block —
 * see the RPI4_DEVICE_BASE change in vmm/mmu.c). */
#define GENET_PHYS 0xFD580000UL
#define GENET_BASE (PHYSMAP_BASE + GENET_PHYS)
#define GENET(off) (*(volatile u_int32_t *)(GENET_BASE + (off)))

/* SYS block. */
#define GENET_SYS_REV_CTRL 0x000
#define GENET_SYS_PORT_CTRL 0x004
#define GENET_SYS_PORT_MODE_EXT_GPHY 3u
#define GENET_SYS_RBUF_FLUSH_CTRL 0x008
#define GENET_SYS_RBUF_FLUSH_RESET (1u << 1)

/* EXT block. */
#define GENET_EXT_RGMII_OOB_CTRL 0x08c
#define GENET_EXT_RGMII_LINK (1u << 4)
#define GENET_EXT_RGMII_OOB_DISABLE (1u << 5)
#define GENET_EXT_RGMII_MODE_EN (1u << 6)
#define GENET_EXT_RGMII_ID_MODE_DISABLE (1u << 16)

/* INTRL2_0 interrupt block (we poll, so we just mask everything off). */
#define GENET_INTRL2_CPU_CLEAR 0x208
#define GENET_INTRL2_CPU_SET_MASK 0x210
#define GENET_INTRL2_CPU_CLEAR_MASK 0x214

/* RBUF / TBUF. */
#define GENET_RBUF_CTRL 0x300
#define GENET_RBUF_ALIGN_2B (1u << 1)
#define GENET_RBUF_TBUF_SIZE_CTRL 0x3b4

/* UMAC block. */
#define GENET_UMAC_CMD 0x808
#define GENET_UMAC_CMD_TXEN (1u << 0)
#define GENET_UMAC_CMD_RXEN (1u << 1)
#define GENET_UMAC_CMD_SPEED_SHIFT 2
#define GENET_UMAC_CMD_SPEED_10 0u
#define GENET_UMAC_CMD_SPEED_100 1u
#define GENET_UMAC_CMD_SPEED_1000 2u
#define GENET_UMAC_CMD_SW_RESET (1u << 13)
#define GENET_UMAC_CMD_LCL_LOOP_EN (1u << 15)
#define GENET_UMAC_MAC0 0x80c
#define GENET_UMAC_MAC1 0x810
#define GENET_UMAC_MAX_FRAME_LEN 0x814
#define GENET_UMAC_MIB_CTRL 0xd80
#define GENET_UMAC_MIB_RESET_RX (1u << 0)
#define GENET_UMAC_MIB_RESET_RUNT (1u << 1)
#define GENET_UMAC_MIB_RESET_TX (1u << 2)

/* MDIO (inside the UMAC block). */
#define GENET_MDIO_CMD 0xe14
#define GENET_MDIO_VAL_MASK 0xffffu
#define GENET_MDIO_REG_SHIFT 16
#define GENET_MDIO_ADDR_SHIFT 21
#define GENET_MDIO_WRITE (1u << 26)
#define GENET_MDIO_READ (1u << 27)
#define GENET_MDIO_READ_FAILED (1u << 28)
#define GENET_MDIO_START_BUSY (1u << 29)

/* DMA descriptor file + ring control. */
#define GENET_RX_BASE 0x2000
#define GENET_TX_BASE 0x4000
#define GENET_DMA_DESC_SIZE 12u          /* {status, addr_lo, addr_hi} */
#define GENET_DMA_DEFAULT_QUEUE 16u      /* the default ring (qid) */
#define GENET_DMA_RING_BUF_SIZE_SHIFT 16 /* descriptor count in the high half */

/* Per-descriptor status/address (idx = 0..N-1). */
#define GENET_RX_DESC_STATUS(i) (GENET_RX_BASE + (i) * GENET_DMA_DESC_SIZE + 0x00)
#define GENET_RX_DESC_ADDR_LO(i) (GENET_RX_BASE + (i) * GENET_DMA_DESC_SIZE + 0x04)
#define GENET_RX_DESC_ADDR_HI(i) (GENET_RX_BASE + (i) * GENET_DMA_DESC_SIZE + 0x08)
#define GENET_TX_DESC_STATUS(i) (GENET_TX_BASE + (i) * GENET_DMA_DESC_SIZE + 0x00)
#define GENET_TX_DESC_ADDR_LO(i) (GENET_TX_BASE + (i) * GENET_DMA_DESC_SIZE + 0x04)
#define GENET_TX_DESC_ADDR_HI(i) (GENET_TX_BASE + (i) * GENET_DMA_DESC_SIZE + 0x08)

/* Descriptor LENGTH_STATUS bits. */
#define GENET_DESC_STATUS_BUFLEN_SHIFT 16
#define GENET_DESC_STATUS_BUFLEN_MASK 0x0fff0000u
#define GENET_RX_DESC_STATUS_SOP (1u << 13)
#define GENET_RX_DESC_STATUS_EOP (1u << 14)
#define GENET_RX_DESC_STATUS_RX_ERROR (1u << 2)
#define GENET_TX_DESC_STATUS_CRC (1u << 6)
#define GENET_TX_DESC_STATUS_QTAG_MASK 0x1f80u
#define GENET_TX_DESC_STATUS_SOP (1u << 13)
#define GENET_TX_DESC_STATUS_EOP (1u << 14)

/* Per-queue ring registers: base + 0xc00 + 0x40*qid + reg. */
#define GENET_RX_RING(qid) (GENET_RX_BASE + 0xc00 + 0x40 * (qid))
#define GENET_TX_RING(qid) (GENET_TX_BASE + 0xc00 + 0x40 * (qid))
#define GENET_RX_WRITE_PTR_LO(q) (GENET_RX_RING(q) + 0x00)
#define GENET_RX_WRITE_PTR_HI(q) (GENET_RX_RING(q) + 0x04)
#define GENET_RX_PROD_INDEX(q) (GENET_RX_RING(q) + 0x08)
#define GENET_RX_CONS_INDEX(q) (GENET_RX_RING(q) + 0x0c)
#define GENET_RX_RING_BUF_SIZE(q) (GENET_RX_RING(q) + 0x10)
#define GENET_RX_START_ADDR_LO(q) (GENET_RX_RING(q) + 0x14)
#define GENET_RX_START_ADDR_HI(q) (GENET_RX_RING(q) + 0x18)
#define GENET_RX_END_ADDR_LO(q) (GENET_RX_RING(q) + 0x1c)
#define GENET_RX_END_ADDR_HI(q) (GENET_RX_RING(q) + 0x20)
#define GENET_RX_XON_XOFF_THRES(q) (GENET_RX_RING(q) + 0x28)
#define GENET_RX_READ_PTR_LO(q) (GENET_RX_RING(q) + 0x2c)
#define GENET_RX_READ_PTR_HI(q) (GENET_RX_RING(q) + 0x30)
#define GENET_TX_READ_PTR_LO(q) (GENET_TX_RING(q) + 0x00)
#define GENET_TX_READ_PTR_HI(q) (GENET_TX_RING(q) + 0x04)
#define GENET_TX_CONS_INDEX(q) (GENET_TX_RING(q) + 0x08)
#define GENET_TX_PROD_INDEX(q) (GENET_TX_RING(q) + 0x0c)
#define GENET_TX_RING_BUF_SIZE(q) (GENET_TX_RING(q) + 0x10)
#define GENET_TX_START_ADDR_LO(q) (GENET_TX_RING(q) + 0x14)
#define GENET_TX_START_ADDR_HI(q) (GENET_TX_RING(q) + 0x18)
#define GENET_TX_END_ADDR_LO(q) (GENET_TX_RING(q) + 0x1c)
#define GENET_TX_END_ADDR_HI(q) (GENET_TX_RING(q) + 0x20)
#define GENET_TX_MBUF_DONE_THRES(q) (GENET_TX_RING(q) + 0x24)
#define GENET_TX_FLOW_PERIOD(q) (GENET_TX_RING(q) + 0x28)
#define GENET_TX_WRITE_PTR_LO(q) (GENET_TX_RING(q) + 0x2c)
#define GENET_TX_WRITE_PTR_HI(q) (GENET_TX_RING(q) + 0x30)

/* DMA ring config + control (base + 0x1040). */
#define GENET_RX_DMA_RING_CFG (GENET_RX_BASE + 0x1040)
#define GENET_RX_DMA_CTRL (GENET_RX_BASE + 0x1044)
#define GENET_RX_SCB_BURST_SIZE (GENET_RX_BASE + 0x104c)
#define GENET_TX_DMA_RING_CFG (GENET_TX_BASE + 0x1040)
#define GENET_TX_DMA_CTRL (GENET_TX_BASE + 0x1044)
#define GENET_TX_SCB_BURST_SIZE (GENET_TX_BASE + 0x104c)
#define GENET_DMA_CTRL_EN (1u << 0)
#define GENET_DMA_CTRL_RBUF_EN(qid) (1u << ((qid) + 1))
#define GENET_DMA_PROD_CONS_MASK 0xffffu

/* Standard clause-22 MII registers (the PHY). */
#define MII_BMCR 0x00
#define MII_BMCR_RESET (1u << 15)
#define MII_BMCR_ANEG_EN (1u << 12)
#define MII_BMCR_ANEG_RESTART (1u << 9)
#define MII_BMSR 0x01
#define MII_BMSR_LINK (1u << 2)
#define MII_PHYID1 0x02
#define MII_ANLPAR 0x05
#define MII_ANLPAR_100 ((1u << 8) | (1u << 7))
#define MII_ANLPAR_10 ((1u << 6) | (1u << 5))
#define MII_GBSR 0x0a
#define MII_GBSR_LP_1000 ((1u << 11) | (1u << 10))

/* BCM54213 LED control.  Register 0x1C is the shadow-register access port: a write
 * of (WRITE | shadow<<10 | data) programs shadow register `shadow`.  Shadow 0x0D
 * ("LED Selector 1") holds LED0's source in bits [3:0] and LED1's in bits [7:4]; the
 * 4-bit source codes select what drives each RJ45 LED. */
#define MII_BCM_SHADOW 0x1c
#define MII_BCM_SHADOW_WRITE (1u << 15)
#define MII_BCM_SHADOW_LED_SEL1 0x0d
#define BCM_LED_SRC_LINKSPD1 0x0 /* on when linked; encodes speed */
#define BCM_LED_SRC_ACTIVITY 0x3 /* blinks on TX/RX activity */

/* Ring sizing (powers of two so prod/cons masking works).  Modest for a polled
 * driver — plenty for DHCP + interactive SSH throughput. */
#define RX_DESC_COUNT 64u
#define TX_DESC_COUNT 32u
#define GENET_BUF_SIZE 2048u /* >= max Ethernet frame + slack */
#define MII_BUSY_RETRY 1000
#define GENET_LINK_RETRY 3000 /* ~3 s: firmware usually has link up already */

/* --- NIC interface consumed by virtio_netif.c (matches the SMSC/virtio backends). */
u_int8_t genet_mac[6];
int genet_ready;

/* Packet buffers (physmap RAM; GENET DMAs to their ARM physical addresses).  Cache-
 * line aligned so a clean/invalidate never touches a neighbouring buffer. */
static u_int8_t g_rx_buf[RX_DESC_COUNT][GENET_BUF_SIZE] __attribute__((aligned(64)));
static u_int8_t g_tx_buf[TX_DESC_COUNT][GENET_BUF_SIZE] __attribute__((aligned(64)));
static u_int32_t g_rx_cons; /* our consumer index (mod 2^16) */
static u_int32_t g_tx_prod; /* our producer index (mod 2^16) */
static int g_phy_addr = -1;
static int g_link_speed; /* 10 / 100 / 1000, or 0 if down */

/* VideoCore property mailbox scratch (16-byte aligned; shared transport in
 * bcm_mbox.c) for the board-MAC query. */
static volatile u_int32_t g_genet_mbox[8] __attribute__((aligned(16)));

/**
 * Read the board's assigned MAC (GET_BOARD_MAC_ADDRESS) into genet_mac; on failure,
 * synthesize a stable locally-administered MAC.  The Pi has no NIC EEPROM, so the
 * VideoCore mailbox is the authoritative source (same as the Pi 3 SMSC path).
 * @return 0 on success (always — the fallback never fails).
 */
static int genet_read_mac(void)
{
	g_genet_mbox[0] = 8 * 4;      /* total size */
	g_genet_mbox[1] = 0;          /* request */
	g_genet_mbox[2] = 0x00010003; /* GET_BOARD_MAC_ADDRESS */
	g_genet_mbox[3] = 8;          /* value buffer size */
	g_genet_mbox[4] = 0;          /* request length */
	g_genet_mbox[5] = 0;          /* (out) MAC bytes 0-3 */
	g_genet_mbox[6] = 0;          /* (out) MAC bytes 4-5 */
	g_genet_mbox[7] = 0;          /* end tag */

	if (aarch64_mbox_prop(g_genet_mbox) == 0 && (g_genet_mbox[5] | g_genet_mbox[6]) != 0)
	{
		genet_mac[0] = (u_int8_t)g_genet_mbox[5];
		genet_mac[1] = (u_int8_t)(g_genet_mbox[5] >> 8);
		genet_mac[2] = (u_int8_t)(g_genet_mbox[5] >> 16);
		genet_mac[3] = (u_int8_t)(g_genet_mbox[5] >> 24);
		genet_mac[4] = (u_int8_t)g_genet_mbox[6];
		genet_mac[5] = (u_int8_t)(g_genet_mbox[6] >> 8);
		if ((genet_mac[0] & 0x01) == 0) /* not a multicast/broadcast address */
			return (0);
	}
	/* Fallback: a fixed locally-administered unicast MAC. */
	genet_mac[0] = 0x02;
	genet_mac[1] = 0x00;
	genet_mac[2] = 0x00;
	genet_mac[3] = 0xBC;
	genet_mac[4] = 0x27;
	genet_mac[5] = 0xEB;
	return (0);
}

/** Busy-wait @us microseconds off the always-running virtual counter. */
static void genet_wait_us(u_int32_t us)
{
	u_int64_t f, t0, t, target;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(t0));
	target = t0 + (f * (u_int64_t)us) / 1000000ULL;
	do
		__asm__ volatile("mrs %0, cntvct_el0" : "=r"(t));
	while (t < target);
}

/** Clean @len bytes at kernel VA @va out of the dcache (push CPU writes to RAM for a
 *  GENET DMA read, e.g. a TX buffer). */
static void dc_clean(uintptr_t va, u_int32_t len)
{
	uintptr_t a;
	for (a = va & ~63UL; a < va + len; a += 64)
		__asm__ volatile("dc cvac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

/** Invalidate @len bytes at kernel VA @va from the dcache (drop stale lines so the
 *  CPU reads what a GENET DMA wrote, e.g. an RX buffer). */
static void dc_inval(uintptr_t va, u_int32_t len)
{
	uintptr_t a;
	__asm__ volatile("dsb sy" ::: "memory");
	for (a = va & ~63UL; a < va + len; a += 64)
		__asm__ volatile("dc ivac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

/**
 * MDIO clause-22 read of PHY @phy register @reg via the GENET MDIO block.
 * @return the 16-bit value, or 0 on read-failed/timeout.
 */
static u_int32_t mdio_read(int phy, int reg)
{
	u_int32_t v;
	int i;

	GENET(GENET_MDIO_CMD) =
	    GENET_MDIO_READ | ((u_int32_t)phy << GENET_MDIO_ADDR_SHIFT) | ((u_int32_t)reg << GENET_MDIO_REG_SHIFT);
	GENET(GENET_MDIO_CMD) |= GENET_MDIO_START_BUSY;
	for (i = 0; i < MII_BUSY_RETRY; i++)
	{
		v = GENET(GENET_MDIO_CMD);
		if ((v & GENET_MDIO_START_BUSY) == 0)
			break;
		genet_wait_us(10);
	}
	if ((v & GENET_MDIO_READ_FAILED) != 0)
		return (0);
	return (v & GENET_MDIO_VAL_MASK);
}

/** MDIO clause-22 write of @val to PHY @phy register @reg. */
static void mdio_write(int phy, int reg, u_int32_t val)
{
	int i;

	GENET(GENET_MDIO_CMD) = GENET_MDIO_WRITE | ((u_int32_t)phy << GENET_MDIO_ADDR_SHIFT) |
	                        ((u_int32_t)reg << GENET_MDIO_REG_SHIFT) | (val & GENET_MDIO_VAL_MASK);
	GENET(GENET_MDIO_CMD) |= GENET_MDIO_START_BUSY;
	for (i = 0; i < MII_BUSY_RETRY; i++)
	{
		if ((GENET(GENET_MDIO_CMD) & GENET_MDIO_START_BUSY) == 0)
			break;
		genet_wait_us(10);
	}
}

/** Probe MDIO addresses 0-31 for a responding PHY.  @return its address, or -1. */
static int mdio_find_phy(void)
{
	int a;
	u_int32_t id;

	for (a = 0; a < 32; a++)
	{
		id = mdio_read(a, MII_PHYID1);
		if (id != 0 && id != 0xffff)
			return (a);
	}
	return (-1);
}

/**
 * Bring up the RGMII PHY link and resolve its speed.  The VideoCore firmware has
 * usually already auto-negotiated (it can netboot), so we poll the existing link
 * rather than force a multi-second renegotiation; if it is down we kick auto-neg and
 * wait up to ~3 s.  @return the link speed (10/100/1000), or 0 if no link.
 */
static int phy_bring_up_link(int phy)
{
	u_int32_t bmsr, gbsr, anlpar;
	int i;

	/* BMSR link bit latches low, so read twice for the current state. */
	(void)mdio_read(phy, MII_BMSR);
	bmsr = mdio_read(phy, MII_BMSR);
	if ((bmsr & MII_BMSR_LINK) == 0)
	{
		/* Kick auto-negotiation and wait for link. */
		mdio_write(phy, MII_BMCR, MII_BMCR_ANEG_EN | MII_BMCR_ANEG_RESTART);
		for (i = 0; i < GENET_LINK_RETRY; i++)
		{
			(void)mdio_read(phy, MII_BMSR);
			bmsr = mdio_read(phy, MII_BMSR);
			if ((bmsr & MII_BMSR_LINK) != 0)
				break;
			genet_wait_us(1000);
		}
	}
	if ((bmsr & MII_BMSR_LINK) == 0)
		return (0);

	/* Resolve the negotiated speed (clause-22): 1000BASE-T status, else the 100/10
	 * auto-neg link-partner ability. */
	gbsr = mdio_read(phy, MII_GBSR);
	if ((gbsr & MII_GBSR_LP_1000) != 0)
		return (1000);
	anlpar = mdio_read(phy, MII_ANLPAR);
	if ((anlpar & MII_ANLPAR_100) != 0)
		return (100);
	if ((anlpar & MII_ANLPAR_10) != 0)
		return (10);
	return (100); /* linked but unclear — assume 100 rather than stall */
}

/**
 * Configure the RJ45 link/activity LEDs on the BCM54213 PHY: LED0 = link+speed
 * (green — lit while linked), LED1 = activity (blinks on traffic).  The firmware
 * usually leaves these enabled, but re-asserting them makes the port LEDs light up
 * reliably once GENET has brought the link up.
 */
static void genet_phy_leds(int phy)
{
	mdio_write(phy,
	           MII_BCM_SHADOW,
	           MII_BCM_SHADOW_WRITE | (MII_BCM_SHADOW_LED_SEL1 << 10) |
	               ((BCM_LED_SRC_ACTIVITY << 4) | BCM_LED_SRC_LINKSPD1));
}

/** Reset the GENET MAC (SYS RBUF flush + UMAC software reset + MIB clear). */
static void genet_reset(void)
{
	u_int32_t v;

	v = GENET(GENET_SYS_RBUF_FLUSH_CTRL);
	GENET(GENET_SYS_RBUF_FLUSH_CTRL) = v | GENET_SYS_RBUF_FLUSH_RESET;
	genet_wait_us(10);
	GENET(GENET_SYS_RBUF_FLUSH_CTRL) = v & ~GENET_SYS_RBUF_FLUSH_RESET;
	genet_wait_us(10);
	GENET(GENET_SYS_RBUF_FLUSH_CTRL) = 0;
	genet_wait_us(10);

	GENET(GENET_UMAC_CMD) = 0;
	GENET(GENET_UMAC_CMD) = GENET_UMAC_CMD_LCL_LOOP_EN | GENET_UMAC_CMD_SW_RESET;
	genet_wait_us(10);
	GENET(GENET_UMAC_CMD) = 0;

	GENET(GENET_UMAC_MIB_CTRL) = GENET_UMAC_MIB_RESET_RUNT | GENET_UMAC_MIB_RESET_RX | GENET_UMAC_MIB_RESET_TX;
	GENET(GENET_UMAC_MIB_CTRL) = 0;

	/* Mask + clear all interrupts (we poll). */
	GENET(GENET_INTRL2_CPU_SET_MASK) = 0xffffffffu;
	GENET(GENET_INTRL2_CPU_CLEAR) = 0xffffffffu;
}

/** Program UMAC_MAC0/MAC1 from genet_mac. */
static void genet_set_hwaddr(void)
{
	GENET(GENET_UMAC_MAC0) = ((u_int32_t)genet_mac[0] << 24) | ((u_int32_t)genet_mac[1] << 16) |
	                         ((u_int32_t)genet_mac[2] << 8) | (u_int32_t)genet_mac[3];
	GENET(GENET_UMAC_MAC1) = ((u_int32_t)genet_mac[4] << 8) | (u_int32_t)genet_mac[5];
}

/** Set up the RX DMA ring (queue 16): arm every descriptor with its buffer, program
 *  the ring registers, and enable the ring. */
static void genet_init_rx_ring(void)
{
	const u_int32_t q = GENET_DMA_DEFAULT_QUEUE;
	u_int32_t i, pa;

	g_rx_cons = 0;
	GENET(GENET_RX_SCB_BURST_SIZE) = 0x08;
	GENET(GENET_RX_WRITE_PTR_LO(q)) = 0;
	GENET(GENET_RX_WRITE_PTR_HI(q)) = 0;
	GENET(GENET_RX_PROD_INDEX(q)) = 0;
	GENET(GENET_RX_CONS_INDEX(q)) = 0;
	GENET(GENET_RX_RING_BUF_SIZE(q)) = (RX_DESC_COUNT << GENET_DMA_RING_BUF_SIZE_SHIFT) | GENET_BUF_SIZE;
	GENET(GENET_RX_START_ADDR_LO(q)) = 0;
	GENET(GENET_RX_START_ADDR_HI(q)) = 0;
	GENET(GENET_RX_END_ADDR_LO(q)) = RX_DESC_COUNT * GENET_DMA_DESC_SIZE / 4 - 1;
	GENET(GENET_RX_END_ADDR_HI(q)) = 0;
	GENET(GENET_RX_XON_XOFF_THRES(q)) = (5u << 16) | (RX_DESC_COUNT >> 4);
	GENET(GENET_RX_READ_PTR_LO(q)) = 0;
	GENET(GENET_RX_READ_PTR_HI(q)) = 0;

	/* Point each descriptor at its RX buffer (invalidated so a later cache eviction
	 * can't clobber the DMA'd frame). */
	for (i = 0; i < RX_DESC_COUNT; i++)
	{
		dc_inval((uintptr_t)g_rx_buf[i], GENET_BUF_SIZE);
		pa = (u_int32_t)AARCH64_PHYS_OF((uintptr_t)g_rx_buf[i]);
		GENET(GENET_RX_DESC_ADDR_LO(i)) = pa;
		GENET(GENET_RX_DESC_ADDR_HI(i)) = 0;
	}

	GENET(GENET_RX_DMA_RING_CFG) = (1u << q);
	GENET(GENET_RX_DMA_CTRL) = GENET(GENET_RX_DMA_CTRL) | GENET_DMA_CTRL_EN | GENET_DMA_CTRL_RBUF_EN(q);
}

/** Set up the TX DMA ring (queue 16) and enable it. */
static void genet_init_tx_ring(void)
{
	const u_int32_t q = GENET_DMA_DEFAULT_QUEUE;

	g_tx_prod = 0;
	GENET(GENET_TX_SCB_BURST_SIZE) = 0x08;
	GENET(GENET_TX_READ_PTR_LO(q)) = 0;
	GENET(GENET_TX_READ_PTR_HI(q)) = 0;
	GENET(GENET_TX_CONS_INDEX(q)) = 0;
	GENET(GENET_TX_PROD_INDEX(q)) = 0;
	GENET(GENET_TX_RING_BUF_SIZE(q)) = (TX_DESC_COUNT << GENET_DMA_RING_BUF_SIZE_SHIFT) | GENET_BUF_SIZE;
	GENET(GENET_TX_START_ADDR_LO(q)) = 0;
	GENET(GENET_TX_START_ADDR_HI(q)) = 0;
	GENET(GENET_TX_END_ADDR_LO(q)) = TX_DESC_COUNT * GENET_DMA_DESC_SIZE / 4 - 1;
	GENET(GENET_TX_END_ADDR_HI(q)) = 0;
	GENET(GENET_TX_MBUF_DONE_THRES(q)) = 1;
	GENET(GENET_TX_FLOW_PERIOD(q)) = 0;
	GENET(GENET_TX_WRITE_PTR_LO(q)) = 0;
	GENET(GENET_TX_WRITE_PTR_HI(q)) = 0;

	GENET(GENET_TX_DMA_RING_CFG) = (1u << q);
	GENET(GENET_TX_DMA_CTRL) = GENET(GENET_TX_DMA_CTRL) | GENET_DMA_CTRL_EN | GENET_DMA_CTRL_RBUF_EN(q);
}

/** Program the RGMII out-of-band control + UMAC speed for the resolved link. */
static void genet_config_link(int speed)
{
	u_int32_t cmd, oob, spd;

	spd = (speed == 1000)  ? GENET_UMAC_CMD_SPEED_1000
	      : (speed == 100) ? GENET_UMAC_CMD_SPEED_100
	                       : GENET_UMAC_CMD_SPEED_10;
	cmd = GENET(GENET_UMAC_CMD);
	cmd &= ~(3u << GENET_UMAC_CMD_SPEED_SHIFT);
	cmd |= (spd << GENET_UMAC_CMD_SPEED_SHIFT);
	GENET(GENET_UMAC_CMD) = cmd;

	/* RGMII: drive the link, enable RGMII mode, disable the MAC-side clock delay
	 * (the BCM54213 PHY on the Pi 4 applies the RX/TX delays itself). */
	oob = GENET(GENET_EXT_RGMII_OOB_CTRL);
	oob &= ~GENET_EXT_RGMII_OOB_DISABLE;
	oob |= GENET_EXT_RGMII_LINK | GENET_EXT_RGMII_MODE_EN | GENET_EXT_RGMII_ID_MODE_DISABLE;
	GENET(GENET_EXT_RGMII_OOB_CTRL) = oob;
}

/**
 * Bring up the BCM2711 GENET Ethernet MAC + its RGMII PHY and register it as the
 * lwIP NIC backend (genet_mac / genet_ready / genet_send / genet_poll_rx).
 *
 * @return 0 on success, -1 if the MAC/PHY did not come up.
 */
int aarch64_genet_init(void)
{
	u_int32_t rev;

	rev = GENET(GENET_SYS_REV_CTRL);
	kprintf("genet: SYS_REV_CTRL=0x%X (BCM2711 GENET)\n", rev);

	/* MAC address from the VideoCore mailbox (the Pi has no NIC EEPROM). */
	genet_read_mac();
	kprintf("genet: MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
	        genet_mac[0],
	        genet_mac[1],
	        genet_mac[2],
	        genet_mac[3],
	        genet_mac[4],
	        genet_mac[5]);

	genet_reset();
	GENET(GENET_SYS_PORT_CTRL) = GENET_SYS_PORT_MODE_EXT_GPHY;
	genet_set_hwaddr();

	/* Disable the DMA engines before (re)programming the rings. */
	GENET(GENET_RX_DMA_CTRL) = GENET(GENET_RX_DMA_CTRL) & ~GENET_DMA_CTRL_EN;
	GENET(GENET_TX_DMA_CTRL) = GENET(GENET_TX_DMA_CTRL) & ~GENET_DMA_CTRL_EN;
	GENET(GENET_RX_DMA_RING_CFG) = 0;
	GENET(GENET_TX_DMA_RING_CFG) = 0;

	genet_init_rx_ring();
	genet_init_tx_ring();

	/* PHY: find it, bring up the link, program the MAC speed/RGMII. */
	g_phy_addr = mdio_find_phy();
	if (g_phy_addr < 0)
	{
		kprintf("genet: no MDIO PHY found\n");
		return (-1);
	}
	g_link_speed = phy_bring_up_link(g_phy_addr);
	genet_phy_leds(g_phy_addr); /* light the RJ45 link/activity LEDs */
	kprintf("genet: PHY at MDIO addr %d, link %s", g_phy_addr, g_link_speed ? "" : "DOWN\n");
	if (g_link_speed)
		kprintf("up at %d Mbps\n", g_link_speed);
	genet_config_link(g_link_speed ? g_link_speed : 1000);

	/* Enable the MAC. */
	GENET(GENET_UMAC_MAX_FRAME_LEN) = 1536;
	GENET(GENET_RBUF_CTRL) = GENET(GENET_RBUF_CTRL) | GENET_RBUF_ALIGN_2B;
	GENET(GENET_RBUF_TBUF_SIZE_CTRL) = 1;
	GENET(GENET_UMAC_CMD) = GENET(GENET_UMAC_CMD) | GENET_UMAC_CMD_TXEN | GENET_UMAC_CMD_RXEN;

	genet_ready = 1;
	kprintf("genet: ready (RX %u / TX %u descriptors)\n", RX_DESC_COUNT, TX_DESC_COUNT);
	return (0);
}

/**
 * Transmit one Ethernet frame (RBUF_ALIGN_2B is set, so the frame is placed at a
 * 2-byte offset in the TX buffer to match the RX alignment; GENET ignores the pad).
 *
 * @return 0 on success, -1 if the frame is too big or the TX ring is full.
 */
int genet_send(const void *frame, u_int32_t len)
{
	const u_int32_t q = GENET_DMA_DEFAULT_QUEUE;
	u_int32_t idx, cons, inflight, status;
	u_int8_t *buf;

	if (!genet_ready || len == 0 || len + 2 > GENET_BUF_SIZE)
		return (-1);

	/* Ring-full check against the hardware consumer index. */
	cons = GENET(GENET_TX_CONS_INDEX(q)) & GENET_DMA_PROD_CONS_MASK;
	inflight = (g_tx_prod - cons) & GENET_DMA_PROD_CONS_MASK;
	if (inflight >= TX_DESC_COUNT)
		return (-1);

	idx = g_tx_prod & (TX_DESC_COUNT - 1);
	buf = g_tx_buf[idx];
	buf[0] = 0;
	buf[1] = 0; /* 2-byte RBUF alignment pad */
	memcpy(buf + 2, frame, len);
	dc_clean((uintptr_t)buf, len + 2);

	GENET(GENET_TX_DESC_ADDR_LO(idx)) = (u_int32_t)AARCH64_PHYS_OF((uintptr_t)buf);
	GENET(GENET_TX_DESC_ADDR_HI(idx)) = 0;
	status = GENET_TX_DESC_STATUS_SOP | GENET_TX_DESC_STATUS_EOP | GENET_TX_DESC_STATUS_CRC |
	         GENET_TX_DESC_STATUS_QTAG_MASK |
	         (((len + 2) << GENET_DESC_STATUS_BUFLEN_SHIFT) & GENET_DESC_STATUS_BUFLEN_MASK);
	GENET(GENET_TX_DESC_STATUS(idx)) = status;

	g_tx_prod = (g_tx_prod + 1) & GENET_DMA_PROD_CONS_MASK;
	GENET(GENET_TX_PROD_INDEX(q)) = g_tx_prod;
	return (0);
}

/**
 * Drain the RX ring, delivering each received frame to @deliver.  The buffers stay
 * armed in their descriptors; advancing CONS_INDEX hands them back to GENET for
 * reuse.  @return the number of frames delivered.
 */
int genet_poll_rx(void (*deliver)(const u_int8_t *, u_int32_t))
{
	const u_int32_t q = GENET_DMA_DEFAULT_QUEUE;
	u_int32_t prod, total, idx, status, len, n;
	int delivered = 0;

	if (!genet_ready)
		return (0);

	prod = GENET(GENET_RX_PROD_INDEX(q)) & GENET_DMA_PROD_CONS_MASK;
	total = (prod - g_rx_cons) & GENET_DMA_PROD_CONS_MASK;

	for (n = 0; n < total; n++)
	{
		idx = g_rx_cons & (RX_DESC_COUNT - 1);
		status = GENET(GENET_RX_DESC_STATUS(idx));
		len = (status & GENET_DESC_STATUS_BUFLEN_MASK) >> GENET_DESC_STATUS_BUFLEN_SHIFT;

		/* A good frame is a single SOP+EOP descriptor with no RX_ERROR.  Drop the
		 * 2-byte RBUF alignment pad at the head of the buffer. */
		if ((status & (GENET_RX_DESC_STATUS_SOP | GENET_RX_DESC_STATUS_EOP | GENET_RX_DESC_STATUS_RX_ERROR)) ==
		        (GENET_RX_DESC_STATUS_SOP | GENET_RX_DESC_STATUS_EOP) &&
		    len > 2 && len <= GENET_BUF_SIZE)
		{
			dc_inval((uintptr_t)g_rx_buf[idx], len);
			deliver(g_rx_buf[idx] + 2, len - 2);
			delivered++;
		}

		g_rx_cons = (g_rx_cons + 1) & GENET_DMA_PROD_CONS_MASK;
		GENET(GENET_RX_CONS_INDEX(q)) = g_rx_cons;
	}
	return (delivered);
}

#endif /* BOARD_RPI4 */
