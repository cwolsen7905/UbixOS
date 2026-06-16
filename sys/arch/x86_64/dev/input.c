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
#define PS2_CMD 0x64    /* write: 8042 controller command */
#define PS2_OBF 0x01    /* output buffer full */
#define PS2_IBF 0x02    /* input buffer full (must be clear before writing) */
#define PS2_AUX 0x20    /* data is from the mouse */

#define PS2_CMD_ENABLE_AUX 0xA8  /* enable the auxiliary (mouse) device */
#define PS2_CMD_WRITE_AUX 0xD4   /* next byte written to 0x60 goes to the mouse */
#define MOUSE_SET_DEFAULTS 0xF6  /* mouse: restore defaults */
#define MOUSE_ENABLE_REPORT 0xF4 /* mouse: enable streaming data reports */
#define MOUSE_ACK 0xFA

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

static u8 g_mouse_pkt[3]; /* the PS/2 mouse 3-byte packet being assembled */
static int g_mouse_idx;   /* how many bytes of g_mouse_pkt are filled */

/** Spin until the 8042 input buffer is empty so a command/data byte can be written. */
static void ps2_wait_write(void)
{
	int spin = 100000;
	while ((inb(PS2_STATUS) & PS2_IBF) && --spin)
		;
}

/** Spin until the 8042 output buffer is full so a response byte can be read. */
static void ps2_wait_read(void)
{
	int spin = 100000;
	while (((inb(PS2_STATUS) & PS2_OBF) == 0) && --spin)
		;
}

/**
 * Send a one-byte command to the mouse (aux device) and return its response.
 * The 8042 routes the next 0x60 write to the mouse via the 0xD4 prefix; the mouse
 * answers with MOUSE_ACK (0xFA).
 */
static u8 mouse_write(u8 cmd)
{
	ps2_wait_write();
	outb(PS2_CMD, PS2_CMD_WRITE_AUX);
	ps2_wait_write();
	outb(PS2_DATA, cmd);
	ps2_wait_read();
	return inb(PS2_DATA);
}

/**
 * Bring up the PS/2 mouse on the 8042 aux channel: enable the aux device, restore
 * its defaults, and turn on streaming data reports.  Polled (no IRQ): sys_getmouse
 * drains the resulting packets from the output buffer.  Called once at boot.
 */
void x86_64_mouse_init(void)
{
	ps2_wait_write();
	outb(PS2_CMD, PS2_CMD_ENABLE_AUX);

	mouse_write(MOUSE_SET_DEFAULTS);
	mouse_write(MOUSE_ENABLE_REPORT);
	g_mouse_idx = 0;
}

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
 * sys_getmouse (native) — return the next mouse event, draining aux bytes from the
 * 8042 and assembling the standard 3-byte PS/2 packet.  Returns as soon as one
 * packet decodes; any trailing bytes stay buffered for the next call.  dy is
 * negated because PS/2 reports +Y up while the screen grows +Y down.
 * @return 0 with *args->ev filled, or -1 if no complete packet is available.
 */
int sys_getmouse(struct thread *td, struct sys_getmouse_args *args)
{
	for (;;)
	{
		u8 st = inb(PS2_STATUS);
		u8 b, flags;
		int dx, dy;

		/* Only consume aux (mouse) bytes; leave keyboard bytes for sys_getkbd. */
		if ((st & PS2_OBF) == 0 || (st & PS2_AUX) == 0)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		b = inb(PS2_DATA);

		/* Resync: the first byte of every packet has bit 3 set; if we are at index
		 * 0 and it is clear, we are mid-stream — drop the byte and keep looking. */
		if (g_mouse_idx == 0 && (b & 0x08) == 0)
			continue;

		g_mouse_pkt[g_mouse_idx++] = b;
		if (g_mouse_idx < 3)
			continue;
		g_mouse_idx = 0;

		flags = g_mouse_pkt[0];
		if (flags & 0xC0) /* X/Y overflow — discard the bogus packet */
			continue;

		dx = g_mouse_pkt[1];
		dy = g_mouse_pkt[2];
		if (flags & 0x10) /* X sign bit */
			dx -= 256;
		if (flags & 0x20) /* Y sign bit */
			dy -= 256;

		args->ev->dx = (int16_t)dx;
		args->ev->dy = (int16_t)(-dy);
		args->ev->buttons =
		    (u_int8_t)(((flags & 0x01) ? MOUSE_BTN_LEFT : 0) | ((flags & 0x02) ? MOUSE_BTN_RIGHT : 0) |
		               ((flags & 0x04) ? MOUSE_BTN_MIDDLE : 0));
		td->td_retval[0] = 0;
		return (0);
	}
}
