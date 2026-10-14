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
#include <ubixos/spinlock.h> /* g_rx_lock */

/* PL011 UART0, base from the active board (M1: g_board->uart_base; QEMU 0x09000000,
 * Pi 0x3F201000), reached through the TTBR1 physmap (Phase 4: TTBR0 user-only). */
#define PL011_BASE (PHYSMAP_BASE + g_board->uart_base)
#define UART_DR (*(volatile u_int32_t *)(PL011_BASE + 0x00))   /* data */
#define UART_FR (*(volatile u_int32_t *)(PL011_BASE + 0x18))   /* flags */
#define UART_FR_RXFE (1u << 4)                                 /* RX FIFO empty */
#define UART_FR_TXFF (1u << 5)                                 /* TX FIFO full */
#define UART_IMSC (*(volatile u_int32_t *)(PL011_BASE + 0x38)) /* interrupt mask */
#define UART_ICR (*(volatile u_int32_t *)(PL011_BASE + 0x44))  /* interrupt clear */
/* RX (bit 4: FIFO at its trigger level) + RT (bit 6: receive timeout, bytes left waiting). */
#define UART_INT_RXRT 0x50u

/* Received bytes, filled by the PL011 interrupt once uart_rx_irq_enable() has
 * run.  One producer (the IRQ, routed to the BSP only) and consumers serialised
 * by g_rx_lock, so head and tail need no shared lock. */
#define UART_RXBUF 4096
static volatile u_int8_t g_rxbuf[UART_RXBUF];
static volatile u_int32_t g_rx_head; /* next slot the IRQ writes */
static volatile u_int32_t g_rx_tail; /* next slot a reader takes */
static volatile int g_rx_irq;        /* 1 once RX is interrupt-driven */
static struct spinLock g_rx_lock = SPIN_LOCK_INITIALIZER;

/** @return non-zero if the PL011 RX FIFO is empty. */
static int uart_rx_empty(void)
{
	unsigned int fr = UART_FR;

	return ((fr & 0x10u) != 0u); /* FR.RXFE */
}

/** @return the next byte from the PL011 RX FIFO (call only when not empty). */
static u_int8_t uart_rx_byte(void)
{
	unsigned int dr = UART_DR;

	return ((u_int8_t)(dr & 0xFFu));
}

/**
 * Non-blocking read of one byte from the PL011 RX FIFO.
 *
 * @return the byte (0-255), or -1 if no input is available.
 */
int uart_getc(void)
{
	int c = -1;

	if (!g_rx_irq)
	{
		if (uart_rx_empty())
			return (-1);
		return ((int)uart_rx_byte());
	}
	spinLock(&g_rx_lock);
	if (g_rx_tail != g_rx_head)
	{
		c = g_rxbuf[g_rx_tail % UART_RXBUF];
		g_rx_tail++;
	}
	spinUnlock(&g_rx_lock);
	return (c);
}

/**
 * Non-zero if a received byte is waiting (peek, non-consuming): in the RX ring
 * once the interrupt is on, else in the PL011 FIFO.
 */
int uart_rx_ready(void)
{
	if (g_rx_irq)
		return (g_rx_tail != g_rx_head);
	return (!uart_rx_empty());
}

/**
 * PL011 interrupt handler (from the intc dispatch): clear the interrupt, move
 * every byte in the RX FIFO into the ring, and wake the console readers.  Bytes
 * that arrive with the ring full are dropped.
 *
 * The clear comes first: a byte that lands after the drain loop's last empty
 * check raises the interrupt again.  Clearing after the drain would discard
 * that byte's interrupt and leave it unread until the next one.
 */
void uart_irq(void)
{
	int n = 0;

	UART_ICR = UART_INT_RXRT;
	while (!uart_rx_empty())
	{
		u_int8_t c = uart_rx_byte();

		if (g_rx_head - g_rx_tail < UART_RXBUF)
		{
			g_rxbuf[g_rx_head % UART_RXBUF] = c;
			__asm__ volatile("dmb ish" ::: "memory"); /* byte visible before head */
			g_rx_head++;
			n++;
		}
	}
	if (n > 0)
		aarch64_console_rx_wake();
}

/**
 * Switch PL011 receive from polling to its interrupt, on a board that names
 * the UART's interrupt and whose controller can route it.  Call once, with the
 * console reader ready to be woken (aarch64_console_rx_wake).
 *
 * @return 1 if receive is now interrupt-driven, 0 if the console keeps polling.
 */
int uart_rx_irq_enable(void)
{
	if (g_board->uart_irq == 0 || g_board->intc->irq_enable == 0x0)
		return (0);
	g_rx_irq = 1;
	uart_irq(); /* take anything already in the FIFO */
	UART_IMSC = UART_INT_RXRT;
	g_board->intc->irq_enable(g_board->uart_irq);
	return (1);
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
	/* Board-specific early UART setup (the Pi muxes GPIO14/15 + sets the PL011
	 * baud; QEMU leaves this NULL — the firmware presets the port). */
	if (g_board->console_init != 0)
		g_board->console_init();
	kconsole_register(&g_pl011_console);
}
