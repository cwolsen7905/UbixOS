/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 desktop input: the keyboard/mouse half of the platform.  The compositor
 * (views) polls sys_getkbd / sys_getmouse each frame and forwards events to the
 * focused window.  On the PC the input devices are the 8042 PS/2 controller
 * (keyboard at port 0x60, mouse on the aux channel) — read by polling here, so no
 * IRQ wiring is needed.  Keyboard scancodes (set 1) are cooked into the ASCII /
 * KEY_* keycodes the compositor expects (the same kbd_event aarch64's virtio_input
 * produces).  Sibling of aarch64's dev/input.c.
 */

#include "../x86_64.h"
#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <isa/kbd.h>   /* struct kbd_event, KEY_* */
#include <isa/mouse.h> /* struct mouse_event */
#include <ubixos/sched.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64 /* read: status; bit0 = output-buffer full, bit5 = from aux (mouse) */
#define PS2_OBF 0x01    /* output buffer full */
#define PS2_AUX 0x20    /* data is from the mouse */

/* US-QWERTY set-1 make-code -> ASCII, unshifted then shifted.  Index = scancode
 * (0x00..0x39 cover the main block; the rest are 0 = ignored). */
static const char g_sc_lower[0x40] = {0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',  '\b',
                                      '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',  '[', ']', '\n', 0,
                                      'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z',
                                      'x',  'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',  0,   ' '};
static const char g_sc_upper[0x40] = {0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  '\b',
                                      '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
                                      'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',  'Z',
                                      'X',  'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' '};

static int g_shift; /* either shift held */
static int g_ext;   /* the previous byte was 0xE0 (extended scancode) */

/**
 * sys_getkbd (native) — return the next keyboard event, polling the 8042.  Tracks
 * shift state + the 0xE0 extended prefix (arrow keys -> KEY_*).  @return 0 with
 * *ev filled, or -1 if no key is waiting.
 */
int sys_getkbd(struct thread *td, struct sys_getkbd_args *args)
{
	u8 st = inb(PS2_STATUS);
	u8 sc;
	int pressed;

	/* Only consume keyboard bytes here; leave mouse (aux) bytes for sys_getmouse. */
	if ((st & PS2_OBF) == 0 || (st & PS2_AUX))
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	sc = inb(PS2_DATA);

	if (sc == 0xE0)
	{
		g_ext = 1;
		td->td_retval[0] = -1;
		return (-1);
	}

	pressed = (sc & 0x80) == 0;
	sc &= 0x7F;

	/* Shift make/break — track, emit nothing. */
	if (sc == 0x2A || sc == 0x36)
	{
		g_shift = pressed;
		td->td_retval[0] = -1;
		return (-1);
	}

	if (g_ext)
	{
		/* Extended (arrow) keys: only the press edge is interesting to the UI. */
		u_int32_t kc = 0;
		g_ext = 0;
		switch (sc)
		{
			case 0x48:
				kc = KEY_UP;
				break;
			case 0x50:
				kc = KEY_DOWN;
				break;
			case 0x4B:
				kc = KEY_LEFT;
				break;
			case 0x4D:
				kc = KEY_RIGHT;
				break;
			default:
				td->td_retval[0] = -1;
				return (-1);
		}
		args->ev->keycode = kc;
		args->ev->pressed = (u_int8_t)pressed;
		td->td_retval[0] = 0;
		return (0);
	}

	if (sc >= 0x40 || g_sc_lower[sc] == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	args->ev->keycode = (u_int32_t)(u8)(g_shift ? g_sc_upper[sc] : g_sc_lower[sc]);
	args->ev->pressed = (u_int8_t)pressed;
	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_getmouse (native) — no mouse yet on x86_64 (the PS/2 aux channel needs an
 * enable + 3-byte packet decode).  Reports "no event" so the compositor runs
 * keyboard-only for now.
 */
int sys_getmouse(struct thread *td, struct sys_getmouse_args *args)
{
	(void)args;
	td->td_retval[0] = -1;
	return (-1);
}
