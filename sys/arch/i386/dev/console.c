/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * i386 console sinks for the registered-sink kconsole layer.
 *
 * Two devices are registered at boot: the COM1 serial port (always-on debug
 * sink, expands '\n' to CR/LF) and the VGA text console (the primary visible
 * console, suspendable when the compositor claims the screen).  The formatting
 * engine and the kprintf entry point are arch-neutral (sys/lib/kprintf.c +
 * sys/lib/kconsole.c); this file owns only the i386 device plumbing that used
 * to live inline in kprintf.c.
 */

#include <lib/kconsole.h>
#include <sys/io.h>
#include <sys/video.h>

/* legacy runtime mute knob for the VGA console (sys/lib/kprintf.c) */
extern int printOff; // NOLINT(readability-identifier-naming): pre-existing ABI global, cannot rename

static int g_serial_initialized = 0;

/**
 * Initialise the COM1 UART (115200 8N1, FIFO on).  Idempotent.
 */
static void serial_init(void)
{
	outportByte(0x3F8 + 1, 0x00); /* disable interrupts */
	outportByte(0x3F8 + 3, 0x80); /* enable DLAB (set baud rate divisor) */
	outportByte(0x3F8 + 0, 0x01); /* divisor lo: 115200 baud */
	outportByte(0x3F8 + 1, 0x00); /* divisor hi */
	outportByte(0x3F8 + 3, 0x03); /* 8 bits, no parity, one stop bit */
	outportByte(0x3F8 + 2, 0xC7); /* enable FIFO, clear, 14-byte threshold */
	outportByte(0x3F8 + 4, 0x03); /* RTS+DTR */
	g_serial_initialized = 1;
}

/**
 * COM1 sink putc — expands '\n' to CR/LF for terminal-friendly serial logs.
 */
static void serial_putc(int c)
{
	if (c == '\n')
	{
		serial_putc('\r');
	}
	/* Wait for the transmit holding register to empty (bit 5 of the LSR). */
	while ((inportByte(0x3F8 + 5) & 0x20) == 0)
	{
		/* spin */
	}
	outportByte(0x3F8, (u_int8_t)c);
}

/**
 * VGA sink putc — writes the i386 text console, honouring the legacy printOff
 * mute knob.  '\n' keeps full-newline semantics (column reset + line advance),
 * so no CR/LF translation happens here.
 */
static void vga_putc(int c)
{
	if (printOff != 0x0)
	{
		return;
	}
	kprint_putc(c);
}

static struct kconsole g_com1_console = {serial_putc, "com1", KC_SERIAL, 0};
static struct kconsole g_vga_console = {vga_putc, "vga", KC_PRIMARY | KC_SUSPENDABLE, 0};

/**
 * Register the i386 console sinks (COM1 then VGA) with the kconsole layer.
 *
 * Called early in kmain, before the first kprintf, so no boot output is lost.
 * Serial is registered first so it is the first sink emitted to.
 */
void kconsole_arch_init(void)
{
	if (!g_serial_initialized)
	{
		serial_init();
	}
	kconsole_register(&g_com1_console);
	kconsole_register(&g_vga_console);
}
