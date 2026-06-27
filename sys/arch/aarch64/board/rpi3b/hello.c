/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Raspberry Pi 3 Model B v1.2 (BCM2837) - M0.5: self-init PL011, drop to EL1
 * (in start.S), parse the firmware DTB's /memory, and print the real memory map
 * + current EL over serial.  Confirms on real hardware - in isolation, before
 * the full kernel - that the EL2->EL1 drop works, the firmware passes a valid
 * DTB, and our minimal FDT reader parses the Pi's actual device tree.
 *
 * PL011 @ 0x3F201000 (peri base 0x3F000000); GPIO @ 0x3F200000.  We init the
 * PL011 ourselves (the Pi firmware doesn't reliably leave it configured).
 */

#define PL011_BASE 0x3F201000UL
#define UART_DR    0x00
#define UART_FR    0x18
#define UART_IBRD  0x24
#define UART_FBRD  0x28
#define UART_LCRH  0x2C
#define UART_CR    0x30
#define UART_ICR   0x44
#define FR_TXFF    0x20

#define GPIO_BASE  0x3F200000UL
#define GPFSEL1    0x04
#define GPPUD      0x94
#define GPPUDCLK0  0x98

#define MMIO(a) (*(volatile unsigned int *)(unsigned long)(a))

/* --- PL011 console (self-initialized; see M0 history in raspberry-pi-3b-bringup.md) --- */

static void short_delay(int n)
{
	while (n-- > 0)
		__asm__ volatile("nop");
}

/**
 * Mux GPIO14/15 to ALT0 (PL011 on header pins 8/10) and set 115200 8N1 @ 48 MHz.
 */
static void uart_init(void)
{
	unsigned int r = MMIO(GPIO_BASE + GPFSEL1);
	r &= ~((7u << 12) | (7u << 15));
	r |= ((4u << 12) | (4u << 15));
	MMIO(GPIO_BASE + GPFSEL1) = r;
	MMIO(GPIO_BASE + GPPUD) = 0;
	short_delay(150);
	MMIO(GPIO_BASE + GPPUDCLK0) = (1u << 14) | (1u << 15);
	short_delay(150);
	MMIO(GPIO_BASE + GPPUDCLK0) = 0;
	MMIO(PL011_BASE + UART_CR) = 0;
	MMIO(PL011_BASE + UART_ICR) = 0x7FF;
	MMIO(PL011_BASE + UART_IBRD) = 26;
	MMIO(PL011_BASE + UART_FBRD) = 3;
	MMIO(PL011_BASE + UART_LCRH) = 0x70;
	MMIO(PL011_BASE + UART_CR) = 0x301;
}

static inline void uart_putc(char c)
{
	while ((MMIO(PL011_BASE + UART_FR) & FR_TXFF) != 0)
		;
	MMIO(PL011_BASE + UART_DR) = (unsigned int)(unsigned char)c;
}

static void uart_puts(const char *s)
{
	for (; *s != '\0'; s++) {
		if (*s == '\n')
			uart_putc('\r');
		uart_putc(*s);
	}
}

/** Print a 64-bit value as 0x-prefixed hex. */
static void put_hex(unsigned long v)
{
	uart_puts("0x");
	for (int i = 60; i >= 0; i -= 4) {
		unsigned int d = (unsigned int)((v >> i) & 0xf);
		uart_putc((char)(d < 10 ? '0' + d : 'a' + d - 10));
	}
}

static void delay(volatile unsigned long n)
{
	while (n-- != 0)
		__asm__ volatile("nop");
}

/** @return the current Exception Level (1, 2, ...). */
static unsigned long current_el(void)
{
	unsigned long v;
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
	return (v >> 2);
}

/* --- minimal flattened-device-tree reader (big-endian on a little-endian CPU) --- */

#define FDT_BEGIN_NODE 1u
#define FDT_END_NODE   2u
#define FDT_PROP       3u
#define FDT_NOP        4u
#define FDT_END        9u

static unsigned int be32(const unsigned char *p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
}

static int streq(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

/** Read @n device-tree cells (n*4 big-endian bytes) into a 64-bit value. */
static unsigned long read_cells(const unsigned char *p, unsigned int n)
{
	unsigned long v = 0;
	for (unsigned int i = 0; i < n; i++) {
		v = (v << 32) | be32(p);
		p += 4;
	}
	return v;
}

/**
 * Walk the DTB for the /memory node's reg = <base size>, honoring the root's
 * #address-cells / #size-cells.  @return 1 and sets base + size on success.
 */
static int fdt_memory(const unsigned char *dtb, unsigned long *base, unsigned long *size)
{
	if (be32(dtb) != 0xd00dfeedu)
		return 0;
	const unsigned char *p = dtb + be32(dtb + 8);  /* off_dt_struct  */
	const char *strs = (const char *)(dtb + be32(dtb + 12)); /* off_dt_strings */
	unsigned int addr_cells = 2, size_cells = 2;
	int depth = 0, in_mem = 0, mem_depth = -1;

	for (;;) {
		unsigned int tok = be32(p);
		p += 4;
		if (tok == FDT_END)
			break;
		if (tok == FDT_NOP)
			continue;
		if (tok == FDT_BEGIN_NODE) {
			const char *name = (const char *)p;
			unsigned int len = 0;
			while (name[len] != '\0')
				len++;
			p += (len + 1 + 3) & ~3u;
			depth++;
			if (name[0] == 'm' && name[1] == 'e' && name[2] == 'm' && name[3] == 'o' && name[4] == 'r' &&
			    name[5] == 'y') {
				in_mem = 1;
				mem_depth = depth;
			}
			continue;
		}
		if (tok == FDT_END_NODE) {
			if (in_mem && depth == mem_depth)
				in_mem = 0;
			depth--;
			continue;
		}
		if (tok == FDT_PROP) {
			unsigned int plen = be32(p);
			const char *pn = strs + be32(p + 4);
			const unsigned char *val = p + 8;
			p += 8 + ((plen + 3) & ~3u);
			if (depth == 1 && streq(pn, "#address-cells"))
				addr_cells = be32(val);
			else if (depth == 1 && streq(pn, "#size-cells"))
				size_cells = be32(val);
			else if (in_mem && streq(pn, "reg")) {
				*base = read_cells(val, addr_cells);
				*size = read_cells(val + addr_cells * 4, size_cells);
				return 1;
			}
			continue;
		}
		break; /* unknown token */
	}
	return 0;
}

/**
 * M0.5 entry: report the entry EL, the current EL (proving the EL2->EL1 drop),
 * and the firmware DTB's /memory map.  Loops with a delay for easy probing.
 */
void m0_main(unsigned long dtb, unsigned long entry_el)
{
	uart_init();
	for (;;) {
		const unsigned char *d = (const unsigned char *)dtb;
		unsigned long base = 0, size = 0;

		uart_puts("\nuBixOS RPi3 M0.5\n");
		uart_puts("  entry EL: ");
		put_hex(entry_el);
		uart_puts("  now EL: ");
		put_hex(current_el());
		uart_puts("\n  DTB @ ");
		put_hex(dtb);
		if (be32(d) == 0xd00dfeedu) {
			uart_puts(" valid, size ");
			put_hex(be32(d + 4));
			uart_puts("\n");
			if (fdt_memory(d, &base, &size)) {
				uart_puts("  /memory base ");
				put_hex(base);
				uart_puts(" size ");
				put_hex(size);
				uart_puts("\n");
			} else {
				uart_puts("  /memory not found\n");
			}
		} else {
			uart_puts(" BAD magic ");
			put_hex(be32(d));
			uart_puts("\n");
		}
		delay(50000000UL);
	}
}
