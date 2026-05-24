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

#include <isa/atkbd.h>
#include <isa/pit.h>
#include <sys/bus.h>
#include <isa/kbd.h>
#include <fs/devfs/devfs.h>
#include <ubixos/vitals.h>
#include <isa/8259.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/io.h>
#include <sys/shutdown.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <ubixos/tty.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <ubixos/vitals.h>
#include <ubixos/signal.h>

static int atkbd_scan();

static unsigned int keyMap = 0x0;
static unsigned int ledStatus = 0x0;

/* Keyboard event ring buffer — fed by keyboardHandler, drained by sys_getkbd */
#define KBD_RING_SIZE 64
static volatile kbd_event_t kbd_ring[KBD_RING_SIZE];
static volatile int kbd_ring_head = 0;
static volatile int kbd_ring_tail = 0;

/* Set to 1 by sys_mapfb when a GUI compositor takes the framebuffer.
 * Prevents VGA-console getchar() from stealing events meant for views. */
volatile int kbd_gui_mode = 0;

void kbd_ring_push(uint32_t keycode, uint8_t pressed)
{
	int next = (kbd_ring_head + 1) % KBD_RING_SIZE;
	if (next == kbd_ring_tail)
		return; /* full — drop */
	kbd_ring[kbd_ring_head].keycode = keycode;
	kbd_ring[kbd_ring_head].pressed = pressed;
	kbd_ring_head = next;
}

int kbd_input_available(void)
{
	return (kbd_ring_tail != kbd_ring_head);
}

int kbd_getEvent(kbd_event_t *ev)
{
	if (kbd_ring_tail == kbd_ring_head)
		return (-1);
	ev->keycode = kbd_ring[kbd_ring_tail].keycode;
	ev->pressed = kbd_ring[kbd_ring_tail].pressed;
	kbd_ring_tail = (kbd_ring_tail + 1) % KBD_RING_SIZE;
	return (0);
}
static uInt32 controlKeys = 0x0;

static struct spinLock atkbdSpinLock = SPIN_LOCK_INITIALIZER;

volatile uint32_t reboot_at_tick = 0;

/* Deferred GUI→text VTY switch.  Set to target slot (0-3) by the keyboard ISR
 * when Ctrl+Alt+Fn is pressed in GUI mode; cleared by the VGA sys_read loop
 * after it has called vesa_text_mode() and tty_change() in task context. */
volatile int vesa_text_slot = -1;

/* Deferred text-mode VTY switch.  Set by keyboard ISR when Alt+Fn is pressed;
 * consumed in task context by the sys_read and sys_select loops. */
volatile int tty_switch_slot = -1;

static unsigned int keyboardMap[255][8] = {
    /*           Ascii, Shift, Ctrl, Alt, Num, Caps, Shift Caps, Shift Num */
    {0, 0, 0, 0, 0, 0, 0, 0},
    /* ESC */ {0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B},
    /* 1,! */ {0x31, 0x21, 0, 0, 0x31, 0x31, 0x21, 0x21},
    /* 2,@ */ {0x32, 0x40, 0, 0, 0x32, 0x32, 0x40, 0x40},
    /* 3,# */ {0x33, 0x23, 0, 0, 0x33, 0x33, 0x23, 0x23},
    /* 4,$ */ {0x34, 0x24, 0, 0, 0x34, 0x34, 0x24, 0x24},
    /* 5,% */ {0x35, 0x25, 0, 0, 0x35, 0x35, 0x25, 0x25},
    /* 6,^ */ {0x36, 0x5E, 0, 0, 0x36, 0x36, 0x5E, 0x5E},
    /* 7,& */ {0x37, 0x26, 0, 0, 0x37, 0x37, 0x26, 0x26},
    /* 8,* */ {0x38, 0x2A, 0, 0, 0x38, 0x38, 0x2A, 0x2A},
    /* 9.( */ {0x39, 0x28, 0, 0, 0x39, 0x39, 0x28, 0x28},
    /* 0,) */ {0x30, 0x29, 0, 0, 0x30, 0x30, 0x29, 0x29},
    /* -,_ */ {0x2D, 0x5F, 0, 0, 0x2D, 0x2D, 0x5F, 0x5F},
    /* =,+ */ {0x3D, 0x2B, 0, 0, 0x3D, 0x3D, 0x2B, 0x2B},
    /*  14 */ {0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F},
    /*  15 */ {0x09, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x71, 0x51, 0, 0, 0, 0, 0, 0},
    /*     */ {0x77, 0x57, 0, 0, 0, 0, 0, 0},
    /* e,E */ {0x65, 0x45, 0x05, 0, 0, 0, 0, 0},
    /*     */ {0x72, 0x52, 0, 0, 0, 0, 0, 0},
    /*     */ {0x74, 0x54, 0, 0, 0, 0, 0, 0},
    /*     */ {0x79, 0x59, 0, 0, 0, 0, 0, 0},
    /*     */ {0x75, 0x55, 0, 0, 0, 0, 0, 0},
    /*     */ {0x69, 0x49, 0, 0, 0, 0, 0, 0},
    /*     */ {0x6F, 0x4F, 0, 0, 0, 0, 0, 0},
    /*     */ {0x70, 0x50, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5B, 0x7B, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5D, 0x7D, 0, 0, 0, 0, 0, 0},
    /*     */ {0x0A, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* a,A */ {0x61, 0x41, 0x01, 0, 0, 0, 0, 0},
    /*     */ {0x73, 0x53, 0, 0, 0, 0, 0, 0},
    /* d,D */ {0x64, 0x44, 0x04, 0, 0, 0, 0, 0},
    /* f,F */ {0x66, 0x46, 0x06, 0, 0, 0, 0, 0},
    /* g,G */ {0x67, 0x47, 0x07, 0, 0, 0, 0, 0},
    /* h,H */ {0x68, 0x48, 0x08, 0, 0, 0, 0, 0},
    /*     */ {0x6A, 0x4A, 0, 0, 0, 0, 0, 0},
    /*     */ {0x6B, 0x4B, 0, 0, 0, 0, 0, 0},
    /*     */ {0x6C, 0x4C, 0, 0, 0, 0, 0, 0},
    /*     */ {0x3B, 0x3A, 0, 0, 0, 0, 0, 0},
    /*     */ {0x27, 0x22, 0, 0, 0, 0, 0, 0},
    /*     */ {0x60, 0x7E, 0, 0, 0, 0, 0, 0},
    /*     */ {0x2A, 0x0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5C, 0x3C, 0, 0, 0, 0, 0, 0},
    /*     */ {0x7A, 0x5A, 0, 0, 0, 0, 0, 0},
    /* x,X */ {0x78, 0x58, 0x18, 0, 0, 0, 0, 0},
    /* c,C */ {0x63, 0x43, 0x03, 0x9, 0, 0, 0, 0},
    /*     */ {0x76, 0x56, 0, 0, 0, 0, 0, 0},
    /* b,B */ {0x62, 0x42, 0x02, 0, 0, 0, 0, 0},
    /*     */ {0x6E, 0x4E, 0, 0, 0, 0, 0, 0},
    /* m,M */ {0x6D, 0x4D, 0x0D, 0, 0, 0, 0, 0},
    /*     */ {0x2C, 0x3C, 0, 0, 0, 0, 0, 0},
    /*     */ {0x2E, 0x3E, 0, 0, 0, 0, 0, 0},
    /*     */ {0x2F, 0x3F, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x20, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* F1  */ {0xF100, 0, 0, 0x3000, 0, 0, 0, 0},
    /* F2  */ {0xF200, 0, 0, 0x3001, 0, 0, 0, 0},
    /* F3  */ {0xF300, 0, 0, 0x3002, 0, 0, 0, 0},
    /* F4  */ {0xF400, 0, 0, 0x3003, 0, 0, 0, 0},
    /* F5  */ {0xF500, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4000, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4100, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4200, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4300, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4400, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4700, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4800, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4900, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x2D, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4B00, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4C00, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4D00, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x2B, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x4F00, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5000, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5100, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5200, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0x5300, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0},
    /*     */ {0, 0, 0, 0, 0, 0, 0, 0}};

/************************************************************************

 Function: int atkbd_init

 Description: This function is used to turn on the keyboard

 Notes:

 02/20/2004 - Approved for quality

 ************************************************************************/
int atkbd_init()
{

	/* Insert the IDT vector for the keyboard handler */
	setVector(&atkbd_isr, mVec + 0x1, dPresent + dInt + dDpl0);

	/* Set the LEDS to their defaults */
	setLED();

	/* Clear Keyboard */
	atkbd_scan();

	/* Turn on the keyboard vector */
	irqEnable(0x1);

	devfs_makeNode("kbd0", 'c', 1, 0);

	/* Print out information on keyboard */
	kprintf("atkbd0: isr=0x%X\n", &atkbd_isr);

	/* Return so we know everything went well */
	return (0x0);
}

/*
 * 2-23-2004 mji  I think the pusha/popa should be pushal/popal
 */

asm(".globl atkbd_isr       \n"
    "atkbd_isr:             \n"
    "push $0x80\n"
    "push $0x80\n"
    "  pusha                \n" /* Save all registers           */
    "  push %ds             \n"
    "  push %es             \n"
    "  push %fs             \n"
    "  push %gs             \n"
    "  push %esp            \n"
    "  call keyboardHandler \n"
    "  add $0x4,%esp\n"
    "  mov $0x20,%dx        \n"
    "  mov $0x20,%ax        \n"
    "  outb %al,%dx         \n"
    "  pop %gs              \n"
    "  pop %fs              \n"
    "  pop %es              \n"
    "  pop %ds              \n"
    "  popa                 \n"
    "  add $0x8,%esp\n"
    "  iret                 \n" /* Exit interrupt                           */
);

static int atkbd_scan()
{
	int code = 0x0;
	int val = 0x0;

	code = inportByte(0x60);
	val = inportByte(0x61);

	outportByte(0x61, val | 0x80);
	outportByte(0x61, val);

	return (code);
}

void keyboardHandler(struct trapframe *frame)
{
	int      key;
	uint32_t kc;

	if (spinTryLock(&atkbdSpinLock))
		return;

	key = atkbd_scan();
	if (key >= 255) {
		spinUnlock(&atkbdSpinLock);
		return;
	}

	/* Modifier key state tracking.
	 * Also push KEY_L* events to the ring so that GUI apps (e.g. DOOM) can
	 * treat Ctrl, Alt, and Shift as first-class keys (fire/strafe/run). */
	if (key == 0x1D) {
		if (!(controlKeys & controlKey)) {
			controlKeys |= controlKey;
			kbd_ring_push(KEY_LCTRL, 1);
		}
	}
	if (key == 0x80 + 0x1D) {
		controlKeys &= ~controlKey;
		kbd_ring_push(KEY_LCTRL, 0);
	}
	if (key == 0x38) {
		if (!(controlKeys & altKey)) {
			controlKeys |= altKey;
			kbd_ring_push(KEY_LALT, 1);
		}
	}
	if (key == 0x80 + 0x38) {
		controlKeys &= ~altKey;
		kbd_ring_push(KEY_LALT, 0);
	}
	if ((key == 0x2A || key == 0x36) && !(controlKeys & shiftKey)) {
		controlKeys |= shiftKey;
		kbd_ring_push(KEY_LSHIFT, 1);
	}
	if ((key == 0x80 + 0x2A) || (key == 0x80 + 0x36)) {
		controlKeys &= ~shiftKey;
		kbd_ring_push(KEY_LSHIFT, 0);
	}

	/* Lock keys */
	if (key == 0x3A) { ledStatus ^= ledCapslock;   setLED(); }
	if (key == 0x45) { ledStatus ^= ledNumlock;     setLED(); }
	if (key == 0x46) { ledStatus ^= ledScrolllock;  setLED(); }

	/* keyMap index */
	if (controlKeys == 0)      keyMap = 0;
	else if (controlKeys == 1) keyMap = 1;
	else if (controlKeys == 2) keyMap = 2;
	else if (controlKeys == 4) keyMap = 3;

	/* For key-up events: modifiers already pushed above; push key-up for
	 * regular keys so callers (DOOM, etc.) can clear their held-key state. */
	if (key >= 0x80) {
		uint8_t down_scan = (uint8_t)(key & 0x7F);
		/* Skip modifier scancodes — already handled above */
		if (down_scan != 0x1D && down_scan != 0x38 &&
		    down_scan != 0x2A && down_scan != 0x36) {
			uint32_t kc_up = keyboardMap[down_scan][keyMap];
			if (kc_up != 0)
				kbd_ring_push(kc_up, 0);
		}
		spinUnlock(&atkbdSpinLock);
		return;
	}

	/* Ctrl+Alt+F1..F4 (scancodes 0x3B-0x3E): switch from GUI to text VTY.
	 * Must be checked before the keyMap lookup because Ctrl+Alt is not
	 * a mapped modifier combination.  Safe to set vesa_text_slot here and
	 * return — the actual biosCall happens in vfs_calls.c sys_read context. */
	if ((controlKeys & controlKey) && (controlKeys & altKey) &&
	    key >= 0x3B && key <= 0x3E) {
		vesa_text_slot = key - 0x3B;   /* F1→0, F2→1, F3→2, F4→3 */
		spinUnlock(&atkbdSpinLock);
		return;
	}

	kc = keyboardMap[key][keyMap];
	if (kc == 0) {
		spinUnlock(&atkbdSpinLock);
		return;
	}

	/* Virtual console switch — Alt+F1..F4 only (0x3000-0x3003).
	 * Bare F-keys produce 0xF100-0xF500 and fall through to kbd_ring.
	 * Limit to VGA slots 0-3; slot 4 is serial and must not be switched.
	 * Defer the actual tty_change (9 KB memcpy) to task context; the ISR
	 * only records the target slot.  Consumed in sys_read / sys_select. */
	if ((kc >> 8) == 0x30 && (kc & 0xFF) <= 3) {
		tty_switch_slot = (int)(kc & 0xFF);
		spinUnlock(&atkbdSpinLock);
		return;
	}

	/* ISR-context emergency keys: handle and do not forward to ring.
	 * Ctrl-C/Ctrl-\/Ctrl-Z are now handled by tty_inject via ISIG. */
	switch (kc) {
	case 0x09: /* Ctrl-Tab / Alt-C: immediate reboot */
		sys_shutdown(REBOOT);
		break;
	case 0x0D: /* Ctrl-M: 5-second countdown reboot */
		if (reboot_at_tick == 0) {
			reboot_at_tick = systemVitals->sysTicks + 5 * PIT_TIMER;
			kprintf("\nSystem rebooting in 5 seconds...\n");
		}
		spinUnlock(&atkbdSpinLock);
		return;
	case 0x18: /* Ctrl-X: kernel panic */
		if (tty_foreground != NULL &&
		    tty_foreground->owner == _current->id)
			die_if_kernel("CTRL-X", frame, frame->tf_eax);
		spinUnlock(&atkbdSpinLock);
		return;
	}

	/* All other keys: push to ring — TTY and GUI both read from there */
	kbd_ring_push(kc, 1);
	spinUnlock(&atkbdSpinLock);
}

void setLED()
{
	int retries;

	outportByte(0x60, 0xED);
	for (retries = 100000; retries-- && (inportByte(0x64) & 2);)
		;
	outportByte(0x60, ledStatus);
	for (retries = 100000; retries-- && (inportByte(0x64) & 2);)
		;
}

/*
 * kbd_apply_event — translate a raw PS/2 keycode and inject into the
 * foreground TTY via the common tty_inject() line discipline.
 */
static void
kbd_apply_event(uint32_t kc)
{
	/* Specials (>= 0x100) and null keycodes are not TTY characters */
	if (kc == 0 || kc >= 0x100 || tty_foreground == NULL)
		return;
	tty_inject(tty_foreground, (char)(uint8_t)kc);
}

int
getchar()
{
	kbd_event_t ev;
	int         retKey;
	uInt32      i;

	if (tty_foreground == NULL)
		return (0);

	/* Drain pending key events from the ring through the line discipline */
	while (kbd_getEvent(&ev) == 0)
		if (ev.pressed)
			kbd_apply_event(ev.keycode);

	/* Read one char from the stdin ring under IRQ disable so the rs232 ISR
	 * cannot corrupt the ring while we shift the contents. */
	uint32_t eflags;
	asm volatile("pushfl; popl %0; cli" : "=r"(eflags) : : "memory");
	if (tty_foreground->stdinSize == 0) {
		asm volatile("pushl %0; popfl" : : "r"(eflags) : "memory");
		return (0);
	}
	retKey = (unsigned char)tty_foreground->stdin[0];
	tty_foreground->stdinSize--;
	for (i = 0; i < (uInt32)tty_foreground->stdinSize; i++)
		tty_foreground->stdin[i] = tty_foreground->stdin[i + 1];
	asm volatile("pushl %0; popfl" : : "r"(eflags) : "memory");

	return (retKey);
}

static int atkbd_ubx_probe(struct ubx_device *dev)
{
	(void)dev;
	return (0); /* AT keyboard is always present on i386 */
}

static int atkbd_ubx_attach(struct ubx_device *dev)
{
	(void)dev;
	return (atkbd_init());
}

struct ubx_driver atkbd_ubx_driver = {
    .drv_name = "atkbd",
    .drv_probe = atkbd_ubx_probe,
    .drv_attach = atkbd_ubx_attach,
    .drv_detach = NULL,
};
