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
 * IRQ dispatch — shared handler table for hardware IRQs 0-15.
 * Drivers call irq_register() instead of setVector() + irqEnable().
 * The dispatch stub (irq_stubs.S) calls irq_dispatch() which walks the
 * handler chain and sends a single EOI.  Handlers must NOT send EOI.
 */

#include <isa/irq.h>
#include <isa/8259.h>
#include <sys/idt.h>
#include <sys/io.h>
#include <lib/kprintf.h>

extern void *irq_entry_table[16]; /* irq_stubs.S */

static void (*irq_handlers[16][IRQ_HANDLERS_MAX])(void);
static int   irq_handler_count[16];

void
irq_dispatch(int irq)
{
	int i;

	for (i = 0; i < irq_handler_count[irq]; i++)
		irq_handlers[irq][i]();

	/* Send EOI — slave then master for IRQs 8-15 */
	if (irq >= 8)
		outportByte(0xA0, 0x20);
	outportByte(0x20, 0x20);
}

void
irq_register(uint8_t irq, void (*handler)(void))
{
	uint16_t vec;

	if (irq > 15) {
		kprintf("irq_register: bad IRQ %d\n", irq);
		return;
	}
	if (irq_handler_count[irq] >= IRQ_HANDLERS_MAX) {
		kprintf("irq_register: IRQ %d handler table full\n", irq);
		return;
	}

	if (irq_handler_count[irq] == 0) {
		/* First driver for this IRQ — install dispatch stub */
		vec = (irq >= 8) ? (sVec + (irq - 8)) : (mVec + irq);
		setVector(irq_entry_table[irq], vec, dInt + dPresent + dDpl0);
		irqEnable(irq);
		kprintf("irq: IRQ %d → dispatch stub installed\n", irq);
	} else {
		kprintf("irq: IRQ %d shared (handler %d)\n", irq,
		    irq_handler_count[irq] + 1);
	}

	irq_handlers[irq][irq_handler_count[irq]++] = handler;
}
