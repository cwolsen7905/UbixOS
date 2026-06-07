/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 bring-up C entry (QEMU `virt`, Phase 11).
 *
 * Minimal proof-of-life: drive the PL011 UART directly and print a banner over
 * the serial console.  Freestanding — no kernel headers yet (the generic kernel
 * is not ported to aarch64 at this phase).  Phase 12 replaces this with real
 * exception/GIC/timer setup and wires kprintf to this UART.
 */

/* QEMU `virt` PL011 UART0 is at 0x09000000. */
#define PL011_BASE 0x09000000UL
#define UART_DR (*(volatile unsigned int *)(PL011_BASE + 0x00)) /* data */
#define UART_FR (*(volatile unsigned int *)(PL011_BASE + 0x18)) /* flags */
#define UART_FR_TXFF (1u << 5)                                  /* TX FIFO full */

/**
 * Write one byte to the PL011, blocking while the TX FIFO is full.
 */
static void uart_putc(char c)
{
	while (UART_FR & UART_FR_TXFF)
	{
		/* spin until there is room */
	}
	UART_DR = (unsigned int)(unsigned char)c;
}

/**
 * Write a NUL-terminated string, translating '\n' to CRLF for the console.
 */
static void uart_puts(const char *s)
{
	while (*s != '\0')
	{
		if (*s == '\n')
			uart_putc('\r');
		uart_putc(*s++);
	}
}

/**
 * First C code on aarch64.  Prints the bring-up banner and returns to the
 * park loop in start.S.
 */
void kmain_aarch64(void)
{
	uart_puts("\n");
	uart_puts("uBixOS aarch64 (QEMU virt) - boot OK\n");
	uart_puts("PL011 UART up; EL/MMU bring-up next (Phase 12+).\n");
}
