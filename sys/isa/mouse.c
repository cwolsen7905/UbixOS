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

#include <isa/mouse.h>
#include <isa/8259.h>
#include <sys/bus.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/io.h>
#include <lib/kprintf.h>

/* Ring buffer for decoded mouse events */
#define MOUSE_BUF_SIZE 64
static mouse_event_t mouse_buf[MOUSE_BUF_SIZE];
static volatile int mouse_head = 0;
static volatile int mouse_tail = 0;

/* PS/2 packet accumulator */
static uint8_t pkt[3];
static int pkt_idx = 0;

static void mouse_push(mouse_event_t *ev)
{
	int next = (mouse_head + 1) & (MOUSE_BUF_SIZE - 1);
	if (next != mouse_tail)
	{
		mouse_buf[mouse_head] = *ev;
		mouse_head = next;
	}
}

int mouse_getEvent(mouse_event_t *ev)
{
	if (mouse_tail == mouse_head)
		return -1;
	*ev = mouse_buf[mouse_tail];
	mouse_tail = (mouse_tail + 1) & (MOUSE_BUF_SIZE - 1);
	return 0;
}

static uInt8 kbdRead(void)
{
	unsigned long timeout;
	uInt8 stat, data;

	for (timeout = 50000L; timeout != 0; timeout--)
	{
		stat = inportByte(0x64);
		if ((stat & 0x01) != 0)
		{
			data = inportByte(0x60);
			if ((stat & 0xC0) == 0)
				return data;
		}
	}
	return (uInt8)-1;
}

static void kbdWrite(uInt16 port, uInt8 data)
{
	uInt32 timeout;
	uInt8 stat;

	for (timeout = 500000L; timeout != 0; timeout--)
	{
		stat = inportByte(0x64);
		if ((stat & 0x02) == 0)
			break;
	}
	if (timeout != 0)
		outportByte(port, data);
}

static uInt8 kbdWriteRead(uInt16 port, uInt8 data, const char *expect)
{
	int retval;

	kbdWrite(port, data);
	for (; *expect; expect++)
	{
		retval = kbdRead();
		if ((uInt8)*expect != retval)
			return retval;
	}
	return 0;
}

int mouseInit(void)
{
	uInt8 ccb;

	/* Enable the auxiliary (mouse) port */
	kbdWrite(0x64, 0xA8);

	/* Drain any stale bytes from the output buffer before reading CCB,
	 * so a pending keyboard scancode doesn't race with our 0x20 response. */
	while (inportByte(0x64) & 0x01)
		inportByte(0x60);

	/* Read the 8042 Command Byte, set bit 1 (aux int enable),
	 * clear bit 5 (aux clock disable).  Explicitly keep bit 0 (kbd int)
	 * and clear bit 4 (kbd clock disable) so the keyboard stays alive. */
	kbdWrite(0x64, 0x20);
	ccb = kbdRead();
	ccb |= 0x03;  /* enable kbd interrupt (bit 0) + aux interrupt (bit 1) */
	ccb &= ~0x30; /* enable kbd clock (bit 4) + aux clock (bit 5) */
	kbdWrite(0x64, 0x60);
	kbdWrite(0x60, ccb);

	/* Reset mouse to defaults */
	kbdWrite(0x64, 0xD4);
	kbdWriteRead(0x60, 0xF6, "\xFA");

	/* Enable data reporting (mouse starts sending packets) */
	kbdWrite(0x64, 0xD4);
	kbdWriteRead(0x60, 0xF4, "\xFA");

	setVector(&mouseISR, mVec + 12, dPresent + dInt + dDpl3);

	outportByte(sPic, eoi);
	outportByte(mPic, eoi);
	irqEnable(12);

	kprintf("psm0: isr=0x%X ccb=0x%X\n", &mouseISR, ccb);
	return 0;
}

asm(".globl mouseISR   \n"
    "mouseISR:         \n"
    "  pusha           \n"
    "  push %ds        \n"
    "  push %es        \n"
    "  push %fs        \n"
    "  push %gs        \n"
    "  call mouseHandler\n"
    "  pop  %gs        \n"
    "  pop  %fs        \n"
    "  pop  %es        \n"
    "  pop  %ds        \n"
    "  popa            \n"
    "  iret            \n");

void mouseHandler(void)
{
	static int first = 1;
	uint8_t byte = inportByte(0x60);
	if (first)
	{
		kprintf("psm0: IRQ12 firing, byte=0x%X\n", byte);
		first = 0;
	}

	/* Sync on start byte: bit 3 must be set in status byte */
	if (pkt_idx == 0 && !(byte & 0x08))
	{
		outportByte(sPic, eoi);
		outportByte(mPic, eoi);
		return;
	}

	pkt[pkt_idx++] = byte;

	if (pkt_idx == 3)
	{
		pkt_idx = 0;

		/* Discard overflow packets */
		if (pkt[0] & 0xC0)
		{
			outportByte(sPic, eoi);
			outportByte(mPic, eoi);
			return;
		}

		mouse_event_t ev;
		ev.buttons = pkt[0] & 0x07;

		/* dx: signed 9-bit from status sign bit + byte */
		ev.dx = (int16_t)(pkt[1] ? ((pkt[0] & 0x10) ? pkt[1] - 256 : pkt[1]) : 0);

		/* dy: signed 9-bit, PS/2 Y is inverted (positive = up) */
		ev.dy = (int16_t)(pkt[2] ? ((pkt[0] & 0x20) ? pkt[2] - 256 : pkt[2]) : 0);
		ev.dy = -ev.dy;

		mouse_push(&ev);
	}

	outportByte(sPic, eoi);
	outportByte(mPic, eoi);
}

static int mouse_ubx_probe(struct ubx_device *dev)
{
	(void)dev;
	return (0); /* PS/2 mouse presence verified during mouseInit() */
}

static int mouse_ubx_attach(struct ubx_device *dev)
{
	(void)dev;
	return (mouseInit());
}

struct ubx_driver mouse_ubx_driver = {
    .drv_name = "psm",
    .drv_probe = mouse_ubx_probe,
    .drv_attach = mouse_ubx_attach,
    .drv_detach = NULL,
};
