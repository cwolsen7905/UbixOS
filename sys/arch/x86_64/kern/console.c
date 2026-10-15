/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 COM1 serial console (long-mode bring-up).  Port-I/O 16550 UART — the
 * bring-up console until the generic kprintf/tty stack is linked in.
 */

#include "x86_64.h"
#include <ubixos/spinlock.h> /* g_rx_lock */

#define COM1 0x3F8
#define COM1_IRQ 4

/* Received bytes, filled by the COM1 RX interrupt once serial_rx_irq_enable()
 * has run.  One producer (the IRQ, always on the BSP: the PIC routes there) and
 * consumers serialised by g_rx_lock, so head and tail need no shared lock.  The
 * 16550 FIFO holds only 16 bytes; this holds a pasted line or a burst over the
 * serial socket until the console reader gets to it. */
#define SERIAL_RXBUF 4096
static volatile u8 g_rxbuf[SERIAL_RXBUF];
static volatile u32 g_rx_head; /* next slot the IRQ writes */
static volatile u32 g_rx_tail; /* next slot a reader takes */
static volatile int g_rx_irq;  /* 1 once RX is interrupt-driven */
static struct spinLock g_rx_lock = SPIN_LOCK_INITIALIZER;

/* outb/inb are shared inlines in x86_64.h. */

void serial_init(void)
{
	outb(COM1 + 1, 0x00); /* disable interrupts   */
	outb(COM1 + 3, 0x80); /* DLAB on              */
	outb(COM1 + 0, 0x01); /* divisor low (115200) */
	outb(COM1 + 1, 0x00); /* divisor high         */
	outb(COM1 + 3, 0x03); /* 8N1, DLAB off        */
	outb(COM1 + 2, 0xC7); /* enable + clear FIFO  */
	outb(COM1 + 4, 0x0B); /* RTS/DSR set          */
}

void serial_putc(char c)
{
	while ((inb(COM1 + 5) & 0x20) == 0)
		;
	outb(COM1, (u8)c);
}

void serial_puts(const char *s)
{
	for (; *s != '\0'; s++)
	{
		if (*s == '\n')
			serial_putc('\r');
		serial_putc(*s);
	}
}

/** Non-blocking COM1 read: the received byte, or -1 if none is waiting. */
/** @return non-zero if the UART holds a received byte (LSR bit 0: data ready). */
static int serial_lsr_ready(void)
{
	u8 lsr = inb(COM1 + 5);

	return ((u32)lsr & 0x01u) != 0u;
}

/**
 * Non-blocking COM1 read.  Once the RX interrupt is on, bytes come from the
 * ring it fills; before that (early boot) straight from the UART.
 *
 * @return the next received byte, or -1 if none is waiting.
 */
int serial_getc(void)
{
	int c = -1;

	if (!g_rx_irq)
	{
		if (!serial_lsr_ready())
			return -1;
		return (int)(unsigned char)inb(COM1);
	}
	spinLock(&g_rx_lock);
	if (g_rx_tail != g_rx_head)
	{
		c = g_rxbuf[g_rx_tail % SERIAL_RXBUF];
		g_rx_tail++;
	}
	spinUnlock(&g_rx_lock);
	return c;
}

/** Non-zero if a received byte is waiting on COM1 (the console line-discipline poll). */
int serial_rx_ready(void)
{
	if (g_rx_irq)
		return g_rx_tail != g_rx_head;
	return serial_lsr_ready();
}

/**
 * COM1 interrupt handler (IRQ4, from x86_64_irq): move every byte the UART
 * holds into the receive ring.  Bytes that arrive with the ring full are
 * dropped.
 *
 * @return the number of bytes queued.
 */
int serial_irq(void)
{
	int n = 0;

	while (serial_lsr_ready())
	{
		u8 c = inb(COM1);

		if (g_rx_head - g_rx_tail < SERIAL_RXBUF)
		{
			g_rxbuf[g_rx_head % SERIAL_RXBUF] = c;
			__asm__ __volatile__("" ::: "memory"); /* byte before head (x86 keeps store order) */
			g_rx_head++;
			n++;
		}
	}
	return n;
}

/**
 * Switch COM1 receive from polling to its interrupt: empty the UART into the
 * ring, enable the received-data interrupt, and unmask IRQ4 on the PIC.  Call
 * once, with the console reader ready to be woken (x86_64_console_rx_wake).
 */
void serial_rx_irq_enable(void)
{
	g_rx_irq = 1;
	serial_irq();
	outb(COM1 + 4, 0x0B); /* DTR, RTS, OUT2 (OUT2 gates the IRQ line) */
	outb(COM1 + 1, 0x01); /* IER: received data available */
	irq_unmask(COM1_IRQ);
}

void serial_puthex(u64 v)
{
	static const char hexd[] = "0123456789ABCDEF";
	char buf[19];
	int i = 18;
	buf[i--] = '\0';
	if (v == 0)
		buf[i--] = '0';
	while (v != 0 && i >= 2)
	{
		buf[i--] = hexd[v & 0xF];
		v >>= 4;
	}
	buf[i--] = 'x';
	buf[i] = '0';
	serial_puts(&buf[i]);
}

void serial_putdec(u64 v)
{
	char buf[21];
	int i = 20;
	buf[i--] = '\0';
	if (v == 0)
		buf[i--] = '0';
	while (v != 0 && i >= 0)
	{
		buf[i--] = (char)('0' + (v % 10));
		v /= 10;
	}
	serial_puts(&buf[i + 1]);
}
