/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Raspberry Pi 3 Model B v1.2 (BCM2837) - M0 serial banner over PL011 (UART0).
 *
 * We do NOT trust the firmware to leave the PL011 configured: M0 routes GPIO14/15
 * to ALT0 (PL011 TXD/RXD on header pins 8/10, overriding any BT routing) and sets
 * 115200 8N1 @ 48 MHz UARTCLK (config.txt: enable_uart=1, init_uart_clock=48000000)
 * itself, then loops the banner with a small delay so a scope on pin 8 and minicom
 * both see continuous activity.  PL011 @ 0x3F201000 (peri base 0x3F000000): DR
 * 0x00, FR 0x18 (TX-full = bit 5), IBRD 0x24, FBRD 0x28, LCRH 0x2C, CR 0x30, ICR
 * 0x44.  GPIO @ 0x3F200000.  See raspberry-pi-3b-bringup.md.
 */

#define PL011_BASE 0x3F201000UL
#define UART_DR    0x00
#define UART_FR    0x18
#define UART_IBRD  0x24
#define UART_FBRD  0x28
#define UART_LCRH  0x2C
#define UART_CR    0x30
#define UART_ICR   0x44
#define FR_TXFF    0x20 /* FR bit 5: TX FIFO full */

#define GPIO_BASE  0x3F200000UL
#define GPFSEL1    0x04 /* GPIO10-19 function select (GPIO14/15 = bits 12-17) */
#define GPPUD      0x94
#define GPPUDCLK0  0x98

#define MMIO(a) (*(volatile unsigned int *)(unsigned long)(a))

/**
 * Busy-spin a few hundred cycles - used between the GPIO pull-control steps.
 */
static void short_delay(int n)
{
	while (n-- > 0)
		__asm__ volatile("nop");
}

/**
 * Configure the PL011 ourselves: mux GPIO14/15 to ALT0 (so TXD0/RXD0 reach header
 * pins 8/10 regardless of the firmware's BT routing), clear pulls, then 115200
 * 8N1 with TX/RX enabled (IBRD/FBRD assume a 48 MHz UARTCLK).
 */
static void uart_init(void)
{
	unsigned int r = MMIO(GPIO_BASE + GPFSEL1);
	r &= ~((7u << 12) | (7u << 15)); /* clear FSEL14, FSEL15 */
	r |= ((4u << 12) | (4u << 15));  /* ALT0 = 0b100 = PL011 */
	MMIO(GPIO_BASE + GPFSEL1) = r;

	MMIO(GPIO_BASE + GPPUD) = 0;
	short_delay(150);
	MMIO(GPIO_BASE + GPPUDCLK0) = (1u << 14) | (1u << 15);
	short_delay(150);
	MMIO(GPIO_BASE + GPPUDCLK0) = 0;

	MMIO(PL011_BASE + UART_CR) = 0;      /* disable while configuring   */
	MMIO(PL011_BASE + UART_ICR) = 0x7FF; /* clear pending interrupts    */
	MMIO(PL011_BASE + UART_IBRD) = 26;   /* 48e6/(16*115200) = 26.04    */
	MMIO(PL011_BASE + UART_FBRD) = 3;    /* .04 * 64 ~= 3               */
	MMIO(PL011_BASE + UART_LCRH) = 0x70; /* 8 bits + FIFO enable        */
	MMIO(PL011_BASE + UART_CR) = 0x301;  /* UARTEN | TXE | RXE          */
}

/**
 * Write one byte to PL011, busy-waiting while the TX FIFO is full.
 */
static inline void uart_putc(char c)
{
	while ((MMIO(PL011_BASE + UART_FR) & FR_TXFF) != 0)
		;
	MMIO(PL011_BASE + UART_DR) = (unsigned int)(unsigned char)c;
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
 * Rough busy-delay (no timer yet at M0) - a fraction of a second between banners.
 */
static void delay(volatile unsigned long n)
{
	while (n-- != 0)
		__asm__ volatile("nop");
}

/**
 * M0 entry: configure PL011, then loop the banner forever with a small delay so
 * the TX pin (and minicom) show continuous activity for probing.
 */
void m0_main(void)
{
	uart_init();
	for (;;) {
		uart_puts("uBixOS RPi3 M0: PL011 @ 0x3F201000 alive\n");
		delay(10000000UL);
	}
}
