/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Raspberry Pi 3 Model B v1.2 (BCM2837) board definition (M1 board abstraction).
 * Selected at compile time via BOARD_RPI3.  Peripheral base 0x3F000000: PL011 UART
 * at 0x3F201000, no GIC (the BCM "ARM local" controller, g_bcm_intc, instead).  The
 * PL011 GPIO mux + baud are set up here because the Pi firmware does not reliably
 * leave them configured for a bare-metal kernel (confirmed in the M0/M0.75
 * standalone).  See docs/design/raspberry-pi-3b-bringup.md.
 */

#ifdef BOARD_RPI3

#include "bringup.h"

#define GPIO_BASE (PHYSMAP_BASE + 0x3F200000UL)
#define PL011_BASE (PHYSMAP_BASE + 0x3F201000UL)
#define RPI_MMIO(a) (*(volatile u_int32_t *)(a))

/** Busy-spin a few hundred cycles (between GPIO pull-control steps). */
static void rpi3_spin(int n)
{
	while (n-- > 0)
		__asm__ volatile("nop");
}

/**
 * Early PL011 setup: mux GPIO14/15 to ALT0 (header pins 8/10), clear pulls, and
 * program 115200 8N1 @ 48 MHz UARTCLK (config.txt pins init_uart_clock).  Mirrors
 * the HW-confirmed M0.75 sequence; runs before the first kprintf.
 */
static void rpi3_console_init(void)
{
	u_int32_t r = RPI_MMIO(GPIO_BASE + 0x04); /* GPFSEL1 */
	r &= ~((7u << 12) | (7u << 15));
	r |= ((4u << 12) | (4u << 15)); /* FSEL14/15 = ALT0 */
	RPI_MMIO(GPIO_BASE + 0x04) = r;
	RPI_MMIO(GPIO_BASE + 0x94) = 0; /* GPPUD = off */
	rpi3_spin(150);
	RPI_MMIO(GPIO_BASE + 0x98) = (1u << 14) | (1u << 15); /* GPPUDCLK0 */
	rpi3_spin(150);
	RPI_MMIO(GPIO_BASE + 0x98) = 0;

	RPI_MMIO(PL011_BASE + 0x30) = 0;     /* CR = 0 (disable while configuring) */
	RPI_MMIO(PL011_BASE + 0x44) = 0x7FF; /* ICR — clear interrupts */
	RPI_MMIO(PL011_BASE + 0x24) = 26;    /* IBRD (115200 @ 48 MHz) */
	RPI_MMIO(PL011_BASE + 0x28) = 3;     /* FBRD */
	RPI_MMIO(PL011_BASE + 0x2C) = 0x70;  /* LCRH — 8 bits + FIFO */
	RPI_MMIO(PL011_BASE + 0x30) = 0x301; /* CR — UARTEN | TXE | RXE */
}

static struct aarch64_board g_board_rpi3 = {
    .name = "raspberrypi-3b",
    .ram_base = 0x0UL,         /* informational; the real base is DTB-driven (g_ram_base) */
    .uart_base = 0x3F201000UL, /* PL011 UART0 */
    .gicd_base = 0,            /* no GIC */
    .gicc_base = 0,
    .intc = &g_bcm_intc,
    .console_init = rpi3_console_init,
};

struct aarch64_board *g_board = &g_board_rpi3;

#endif /* BOARD_RPI3 */
