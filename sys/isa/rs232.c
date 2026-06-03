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
 * COM1 (0x3F8, IRQ4) serial driver.
 *
 * TX path: kprintf() already writes to COM1 via its own serial_putc().
 *          rs232_putc() is a second public TX helper for TTY echo.
 *
 * RX path: the UART's "data available" interrupt (IER bit 0) fires on IRQ4.
 *          The ISR reads one byte from the RX data register and injects it
 *          into the foreground TTY via tty_inject() — the same line discipline
 *          that the PS/2 keyboard uses.  This lets a serial terminal drive
 *          the login prompt in run-debug / headless mode.
 */

#include <isa/rs232.h>
#include <isa/8259.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <i386/pcpu_asm.h>
#include <sys/io.h>
#include <lib/kprintf.h>
#include <ubixos/tty.h>
#include <ubixos/spinlock.h>

#define COM1_BASE  0x3F8

/* UART register offsets */
#define UART_RX    0   /* RX data (read) / TX data (write) */
#define UART_IER   1   /* Interrupt Enable Register */
#define UART_IIR   2   /* Interrupt Identification Register (read) */
#define UART_LCR   3   /* Line Control Register */
#define UART_MCR   4   /* Modem Control Register */
#define UART_LSR   5   /* Line Status Register */

/* LSR bits */
#define LSR_DR     0x01  /* Data Ready */
#define LSR_THRE   0x20  /* Transmit Holding Register Empty */

static struct spinLock rs232SpinLock = SPIN_LOCK_INITIALIZER;

/*
 * serial_rx_ring — raw byte ring filled by the ISR when the serial TTY has
 * been claimed by a process (e.g. ttyd).  Drained by the kernel read path
 * through the serial TTY's line discipline.
 */
#define SERIAL_RX_RING_SIZE 256
static volatile u_int8_t serial_rx_ring[SERIAL_RX_RING_SIZE];
static volatile int     serial_rx_head = 0;
static volatile int     serial_rx_tail = 0;

/*
 * serial_claimed — set to 1 by sys_settty() when a process claims the serial
 * TTY.  When 0 the ISR injects bytes into tty_foreground (passthrough mode).
 * When 1 the ISR pushes raw bytes into serial_rx_ring for the owning process.
 */
volatile int serial_claimed = 0;

static void
serial_rx_push(u_int8_t ch)
{
	int next = (serial_rx_head + 1) % SERIAL_RX_RING_SIZE;
	if (next == serial_rx_tail)
		return; /* ring full — drop */
	serial_rx_ring[serial_rx_head] = ch;
	serial_rx_head = next;
}

int
serial_rx_getbyte(void)
{
	int ch;
	if (serial_rx_tail == serial_rx_head)
		return (-1);
	ch = (int)(u_int8_t)serial_rx_ring[serial_rx_tail];
	serial_rx_tail = (serial_rx_tail + 1) % SERIAL_RX_RING_SIZE;
	return (ch);
}

/* Forward declaration for ISR trampoline */
void rs232_isr(void);

void
rs232_putc(char c)
{
	while ((inportByte(COM1_BASE + UART_LSR) & LSR_THRE) == 0)
		;
	outportByte(COM1_BASE + UART_RX, c);
}

void
rs232_handler(void)
{
	u_int8_t ch;

	if (spinTryLock(&rs232SpinLock))
		return;

	/* Drain all bytes the FIFO has ready */
	while (inportByte(COM1_BASE + UART_LSR) & LSR_DR) {
		ch = inportByte(COM1_BASE + UART_RX);
		if (serial_claimed) {
			/* Serial TTY is owned — push to ring for sys_read to drain */
			serial_rx_push(ch);
		} else {
			/* Passthrough mode — feed VGA console TTY directly */
			if (tty_foreground != NULL)
				tty_inject(tty_foreground, (char)ch);
		}
	}

	spinUnlock(&rs232SpinLock);
}

asm(".globl rs232_isr        \n"
    "rs232_isr:              \n"
    "push $0x80\n"
    "push $0x80\n"
    "  pusha                 \n"
    "  push %ds              \n"
    "  push %es              \n"
    "  push %fs              \n"
    "  push %gs              \n" ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU */
    "  push %esp             \n"
    "  call rs232_handler    \n"
    "  add $0x4,%esp\n"
    "  mov $0x20,%dx         \n"
    "  mov $0x20,%ax         \n"
    "  outb %al,%dx          \n"
    "  pop %gs               \n"
    "  pop %fs               \n"
    "  pop %es               \n"
    "  pop %ds               \n"
    "  popa                  \n"
    "  add $0x8,%esp\n"
    "  iret                  \n"
);

/*
 * rs232Init — enable COM1 RX interrupts.
 *
 * kprintf's serial_init() already configured the UART for 115200 8N1 TX.
 * All we add here is RX: enable IER bit 0 (data-available interrupt),
 * wire IRQ4 to our ISR, and unmask it in the PIC.
 */
int
rs232Init(void)
{
	/* Enable UART "data available" interrupt */
	outportByte(COM1_BASE + UART_IER, 0x01);

	/* Wire IRQ4 (COM1) to our ISR trampoline */
	setVector(&rs232_isr, mVec + 0x4, dPresent + dInt + dDpl0);
	irqEnable(0x4);

	kprintf("rs232: COM1 RX enabled on IRQ4\n");
	return (0);
}
