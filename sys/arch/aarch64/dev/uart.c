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

/* QEMU `virt` PL011 UART0. */
#define PL011_BASE 0x09000000UL
#define UART_DR (*(volatile u_int32_t *)(PL011_BASE + 0x00)) /* data */
#define UART_FR (*(volatile u_int32_t *)(PL011_BASE + 0x18)) /* flags */
#define UART_FR_TXFF (1u << 5)                               /* TX FIFO full */

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
 * Console printf — formats via the shared kvprintf engine, then emits the
 * result on the PL011.
 *
 * @return the number of characters formatted.
 */
int kprintf(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = kvprintf(fmt, NULL, buf, 10, ap, sizeof(buf) - 1);
	va_end(ap);
	buf[n < (int)sizeof(buf) - 1 ? n : (int)sizeof(buf) - 1] = '\0';
	uart_puts(buf); /* uart_puts already maps '\n' -> CRLF */
	return n;
}
