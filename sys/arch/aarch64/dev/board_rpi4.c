/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Raspberry Pi 4 Model B / Pi 400 (BCM2711) board definition (R4.0 bring-up).
 * Selected at compile time via BOARD_RPI4.  Peripheral base 0xFE000000 (vs the Pi
 * 3's 0x3F000000): PL011 UART0 at 0xFE201000, GPIO at 0xFE200000.  Unlike the Pi 3,
 * the BCM2711 has a real interrupt controller — a GIC-400 (GICv2) at 0xFF841000 /
 * 0xFF842000 — so it reuses g_gicv2_intc (the same driver QEMU virt uses), not the
 * BCM "ARM local" controller.  The BCM2711 also changed the GPIO pull-up/down
 * mechanism (GPIO_PUP_PDN_CNTRL_REGn, not GPPUD/GPPUDCLK).  See
 * docs/design/raspberry-pi-4-400-bringup.md.
 */

#ifdef BOARD_RPI4

#include "bringup.h"

#define GPIO_BASE (PHYSMAP_BASE + 0xFE200000UL)
#define PL011_BASE (PHYSMAP_BASE + 0xFE201000UL)
#define RPI_MMIO(a) (*(volatile u_int32_t *)(a))

/**
 * Early PL011 setup for the BCM2711: mux GPIO14/15 to ALT0 (header pins 8/10), set
 * their pull to none via the Pi-4 GPIO_PUP_PDN_CNTRL_REG0 (a different mechanism
 * than the Pi 3), and program 115200 8N1 @ 48 MHz UARTCLK (config.txt sets
 * init_uart_clock).  Runs before the first kprintf.
 */
static void rpi4_console_init(void)
{
	u_int32_t r = RPI_MMIO(GPIO_BASE + 0x04); /* GPFSEL1 */
	r &= ~((7u << 12) | (7u << 15));
	r |= ((4u << 12) | (4u << 15)); /* FSEL14/15 = ALT0 (TXD0/RXD0) */
	RPI_MMIO(GPIO_BASE + 0x04) = r;

	/* BCM2711 pull control: GPIO_PUP_PDN_CNTRL_REG0 @0xE4, 2 bits/pin.  pin14 = bits
	 * [29:28], pin15 = bits [31:30]; 0b00 = no pull. */
	r = RPI_MMIO(GPIO_BASE + 0xE4);
	r &= ~((3u << 28) | (3u << 30));
	RPI_MMIO(GPIO_BASE + 0xE4) = r;

	RPI_MMIO(PL011_BASE + 0x30) = 0;     /* CR = 0 (disable while configuring) */
	RPI_MMIO(PL011_BASE + 0x44) = 0x7FF; /* ICR — clear interrupts */
	RPI_MMIO(PL011_BASE + 0x24) = 26;    /* IBRD (115200 @ 48 MHz) */
	RPI_MMIO(PL011_BASE + 0x28) = 3;     /* FBRD */
	RPI_MMIO(PL011_BASE + 0x2C) = 0x70;  /* LCRH — 8 bits + FIFO */
	RPI_MMIO(PL011_BASE + 0x30) = 0x301; /* CR — UARTEN | TXE | RXE */
}

static struct aarch64_board g_board_rpi4 = {
    .name = "raspberrypi-4b",
    .ram_base = 0x0UL,         /* informational; real base is DTB-driven (g_ram_base) */
    .uart_base = 0xFE201000UL, /* PL011 UART0 */
    .gicd_base = 0xFF841000UL, /* GIC-400 distributor */
    .gicc_base = 0xFF842000UL, /* GIC-400 CPU interface */
    .intc = &g_gicv2_intc,     /* the Pi 4 has a GIC — reuse the QEMU-virt driver */
    .console_init = rpi4_console_init,
};

struct aarch64_board *g_board = &g_board_rpi4;

#endif /* BOARD_RPI4 */
