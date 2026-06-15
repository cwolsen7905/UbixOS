/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 COM1 serial console (long-mode bring-up).  Port-I/O 16550 UART — the
 * bring-up console until the generic kprintf/tty stack is linked in.
 */

#include "x86_64.h"

#define COM1 0x3F8

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
