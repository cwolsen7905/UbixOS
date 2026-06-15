/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 bring-up C entry.  Reached from start.S in 64-bit long mode with paging
 * on (identity map of the low 1 GB).  Phase 1: bring up the COM1 serial console
 * and print a banner — proof the long-mode transition + 64-bit C ABI work.  The
 * rest of the kernel (paging allocator, then the i386 drivers widened to 64-bit)
 * grows from here.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define COM1 0x3F8

static inline void outb(u16 port, u8 val)
{
	__asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port)
{
	u8 r;
	__asm__ __volatile__("inb %1, %0" : "=a"(r) : "Nd"(port));
	return r;
}

/** Initialise COM1 (16550): 115200 8N1, FIFO on. */
static void serial_init(void)
{
	outb(COM1 + 1, 0x00); /* disable interrupts          */
	outb(COM1 + 3, 0x80); /* DLAB on                      */
	outb(COM1 + 0, 0x01); /* divisor low  (115200)        */
	outb(COM1 + 1, 0x00); /* divisor high                 */
	outb(COM1 + 3, 0x03); /* 8N1, DLAB off                */
	outb(COM1 + 2, 0xC7); /* enable + clear FIFO          */
	outb(COM1 + 4, 0x0B); /* RTS/DSR set                  */
}

static void serial_putc(char c)
{
	while ((inb(COM1 + 5) & 0x20) == 0)
		;
	outb(COM1, (u8)c);
}

static void serial_puts(const char *s)
{
	for (; *s != '\0'; s++)
	{
		if (*s == '\n')
			serial_putc('\r');
		serial_putc(*s);
	}
}

/* Print a 64-bit value as hex — confirms 64-bit registers/ABI really work. */
static void serial_puthex(unsigned long v)
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

/**
 * x86-64 kernel C entry.  @mb_magic / @mb_info are the multiboot magic + info
 * pointer start.S preserved (zero-extended) in the SysV arg registers.
 */
void kmain_x86_64(u32 mb_magic, u32 mb_info)
{
	serial_init();
	serial_puts("\nuBixOS x86_64 (long mode) - boot OK\n");
	serial_puts("x86_64 bring-up: COM1 up, PAE+LME+paging on, 64-bit C ABI live.\n");
	serial_puts("multiboot magic=");
	serial_puthex(mb_magic);
	serial_puts(" info=");
	serial_puthex(mb_info);
	serial_puts("\n");

	for (;;)
		__asm__ __volatile__("hlt");
}
