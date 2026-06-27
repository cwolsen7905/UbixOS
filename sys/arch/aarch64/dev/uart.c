/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 PL011 UART console + the console kprintf (QEMU `virt`).
 *
 * Freestanding — drives the PL011 at 0x09000000 directly.  Formatting is the
 * portable shared engine (kvprintf, sys/lib/kprintf.c); this file owns only the
 * arch console plumbing: the PL011 putc and the kprintf entry point that formats
 * into a buffer and emits it.
 */

#include "bringup.h"
#include <lib/kconsole.h>

/* QEMU `virt` PL011 UART0. */
#define PL011_BASE (PHYSMAP_BASE + 0x09000000UL)             /* via TTBR1 physmap (Phase 4: TTBR0 user-only) */
#define UART_DR (*(volatile u_int32_t *)(PL011_BASE + 0x00)) /* data */
#define UART_FR (*(volatile u_int32_t *)(PL011_BASE + 0x18)) /* flags */
#define UART_FR_RXFE (1u << 4)                               /* RX FIFO empty */
#define UART_FR_TXFF (1u << 5)                               /* TX FIFO full */

/**
 * Non-blocking read of one byte from the PL011 RX FIFO.
 *
 * @return the byte (0-255), or -1 if no input is available.
 */
int uart_getc(void)
{
	if (UART_FR & UART_FR_RXFE)
		return (-1);
	return ((int)(UART_DR & 0xFF));
}

/**
 * Non-zero if the PL011 RX FIFO has a byte waiting (peek, non-consuming).
 */
int uart_rx_ready(void)
{
	return ((UART_FR & UART_FR_RXFE) == 0);
}

/**
 * Write one byte to the PL011, blocking while the TX FIFO is full.
 */
void uart_putc(char c)
{
	while (UART_FR & UART_FR_TXFF)
	{
		/* spin */
	}
	UART_DR = (u_int32_t)(u_int8_t)c;
}

/**
 * Write a NUL-terminated string, translating '\n' to CRLF.
 */
void uart_puts(const char *s)
{
	while (*s != '\0')
	{
		if (*s == '\n')
			uart_putc('\r');
		uart_putc(*s++);
	}
}

/**
 * PL011 sink putc — emits one character, mapping '\n' to CR/LF for
 * terminal-friendly serial output.
 */
static void pl011_putc(int c)
{
	if (c == '\n')
		uart_putc('\r');
	uart_putc((char)c);
}

static struct kconsole g_pl011_console = {pl011_putc, "pl011", KC_SERIAL, 0};

/**
 * Register the aarch64 console sink (the PL011 serial port).
 *
 * Called early in kmain_aarch64, before the first kprintf.  aarch64 has no
 * in-kernel primary visible console (the graphical desktop is owned by views in
 * userland), so only the always-on serial sink is registered here; an on-screen
 * fbcon sink is a later phase.
 */
void kconsole_arch_init(void)
{
	kconsole_register(&g_pl011_console);
}
