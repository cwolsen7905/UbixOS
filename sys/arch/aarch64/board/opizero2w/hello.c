/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Orange Pi Zero 2W (Allwinner H618) - M0 serial banner over UART0.
 *
 * UART0 is a Synopsys dw-apb (16550-compatible) UART at physical 0x05000000 with
 * 32-bit, 4-byte-spaced registers (DT reg-shift=2 / reg-io-width=4): THR at byte
 * offset 0x00, LSR at 0x14, the TX-holding-empty flag is LSR bit 5 (0x20).
 * U-Boot already muxes + clocks UART0 (it's the U-Boot console), so M0 just polls
 * and writes - no CCU/PIO setup needed.  Verified facts: see orange-pi-zero2w-
 * bringup.md.
 */

#define UART0_BASE 0x05000000UL
#define UART_THR   0x00 /* TX holding register             */
#define UART_LSR   0x14 /* line status register (reg 5<<2) */
#define LSR_THRE   0x20 /* bit 5: TX holding register empty */

/**
 * Write one byte to UART0, busy-waiting for the TX holding register to drain.
 */
static inline void uart_putc(char c)
{
	volatile unsigned int *lsr = (volatile unsigned int *)(UART0_BASE + UART_LSR);
	volatile unsigned int *thr = (volatile unsigned int *)(UART0_BASE + UART_THR);

	while ((*lsr & LSR_THRE) == 0)
		;
	*thr = (unsigned int)(unsigned char)c;
}

/**
 * Write a NUL-terminated string, translating '\n' to CR+LF for serial terminals.
 */
static void uart_puts(const char *s)
{
	for (; *s != '\0'; s++) {
		if (*s == '\n')
			uart_putc('\r');
		uart_putc(*s);
	}
}

/**
 * M0 entry: the smallest proof the board booted our code and the UART works.
 */
void m0_main(void)
{
	uart_puts("\n");
	uart_puts("uBixOS - Orange Pi Zero 2W (Allwinner H618)\n");
	uart_puts("M0: UART0 @ 0x05000000 is alive - U-Boot boot chain OK.\n");
}
