/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 PL011 UART console + a minimal kprintf (QEMU `virt`, bring-up).
 *
 * Freestanding — drives the PL011 at 0x09000000 directly.  Supports the format
 * specifiers the bring-up code needs: %c %s %d %u %x %p, the %l{d,u,x} 64-bit
 * variants, %% , and a leading zero/width like %08x.  Replaced by the portable
 * kprintf once the generic kernel is ported.
 */

#include "bringup.h"

/* QEMU `virt` PL011 UART0. */
#define PL011_BASE 0x09000000UL
#define UART_DR (*(volatile uint32_t *)(PL011_BASE + 0x00)) /* data */
#define UART_FR (*(volatile uint32_t *)(PL011_BASE + 0x18)) /* flags */
#define UART_FR_TXFF (1u << 5)                              /* TX FIFO full */

/**
 * Write one byte to the PL011, blocking while the TX FIFO is full.
 */
void uart_putc(char c)
{
	while (UART_FR & UART_FR_TXFF)
	{
		/* spin */
	}
	UART_DR = (uint32_t)(uint8_t)c;
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
 * Emit an unsigned value in @base, right-justified to @width with @pad.
 */
static void put_uint(uint64_t v, unsigned base, int width, char pad)
{
	static const char digits[] = "0123456789abcdef";
	char buf[32];
	int i = 0;

	if (v == 0)
		buf[i++] = '0';
	while (v != 0)
	{
		buf[i++] = digits[v % base];
		v /= base;
	}
	while (i < width)
		buf[i++] = pad;
	while (i-- > 0)
		uart_putc(buf[i]);
}

/**
 * Minimal vprintf to the console.  Not a full printf — just the bring-up subset.
 */
void kvprintf(const char *fmt, va_list ap)
{
	for (; *fmt != '\0'; fmt++)
	{
		if (*fmt != '%')
		{
			if (*fmt == '\n')
				uart_putc('\r');
			uart_putc(*fmt);
			continue;
		}

		fmt++;
		char pad = ' ';
		int width = 0;
		if (*fmt == '0')
		{
			pad = '0';
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (*fmt++ - '0');

		switch (*fmt)
		{
			case 's':
			{
				const char *s = va_arg(ap, const char *);
				uart_puts(s != 0 ? s : "(null)");
				break;
			}
			case 'c':
				uart_putc((char)va_arg(ap, int));
				break;
			case 'd':
			{
				int v = va_arg(ap, int);
				if (v < 0)
				{
					uart_putc('-');
					put_uint((uint64_t)(-(int64_t)v), 10, 0, ' ');
				}
				else
					put_uint((uint64_t)v, 10, width, pad);
				break;
			}
			case 'u':
				put_uint(va_arg(ap, unsigned), 10, width, pad);
				break;
			case 'x':
				put_uint(va_arg(ap, unsigned), 16, width, pad);
				break;
			case 'p':
				uart_puts("0x");
				put_uint((uint64_t)(uintptr_t)va_arg(ap, void *), 16, 16, '0');
				break;
			case 'l':
			{
				fmt++;
				if (*fmt == 'x')
					put_uint(va_arg(ap, uint64_t), 16, width, pad);
				else if (*fmt == 'u')
					put_uint(va_arg(ap, uint64_t), 10, width, pad);
				else if (*fmt == 'd')
				{
					int64_t v = va_arg(ap, int64_t);
					if (v < 0)
					{
						uart_putc('-');
						put_uint((uint64_t)(-v), 10, 0, ' ');
					}
					else
						put_uint((uint64_t)v, 10, width, pad);
				}
				else
					uart_putc('l');
				break;
			}
			case '%':
				uart_putc('%');
				break;
			default:
				uart_putc('%');
				uart_putc(*fmt);
				break;
		}
	}
}

/**
 * Console printf — the bring-up logging entry point.
 */
void kprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
}
