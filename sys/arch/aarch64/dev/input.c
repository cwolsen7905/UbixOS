/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 input syscalls: the keyboard/mouse half of the desktop platform.
 *
 * i386 cooks AT keyboard scancodes and PS/2 mouse packets in its ISA drivers and
 * exposes them via sys_getkbd / sys_getmouse (sys/kern/fb.c).  aarch64 has no
 * PS/2: input arrives as raw Linux input events (type/code/value) from the
 * virtio-input devices (sys/arch/aarch64/dev/virtio_input.c).  This file drains
 * those, cooks them into the same kbd_event / mouse_event the compositor expects,
 * and queues them for sys_getkbd / sys_getmouse.
 *
 * The keyboard cook reuses the happy accident that a Linux input EV_KEY `code`
 * is the traditional AT scancode index — the same index the i386 keyboardMap is
 * built on — so a compact US base/shift table here yields the identical ASCII
 * keycodes the i386 path produces.
 */

#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <isa/kbd.h>   /* struct kbd_event */
#include <isa/mouse.h> /* struct mouse_event */
#include <ubixos/spinlock.h>

/* virtio-input poll (sys/arch/aarch64/dev/virtio_input.c). */
int virtio_input_poll(void (*deliver)(int dev, u_int16_t type, u_int16_t code, u_int32_t value));

/* SMP: sys_getkbd (terminal) and sys_getmouse (views) run on different CPUs and
 * both call input_drain(), which drains EVERY virtio-input device and writes both
 * cooked rings — so two concurrent callers race the shared virtqueue indices and
 * ring head/tail pointers, dropping events (the dead mouse under SMP).  Serialise
 * the whole drain + ring read with one lock. */
static struct spinLock g_input_lock = SPIN_LOCK_INITIALIZER;

/* Linux input ABI subset (linux/input-event-codes.h). */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define REL_X 0x00
#define REL_Y 0x01
#define REL_WHEEL 0x08
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define KEY_LEFTCTRL 29
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_RIGHTCTRL 97
#define KEY_LEFTALT 56
#define KEY_RIGHTALT 100
#define KEY_KP_UP 103
#define KEY_KP_LEFT 105
#define KEY_KP_RIGHT 106
#define KEY_KP_DOWN 108

/* Special (non-ASCII) keycodes the compositor understands (include/sys/kbd.h). */
#define KEY_UP 0x100
#define KEY_DOWN 0x101
#define KEY_LEFT 0x102
#define KEY_RIGHT 0x103

#define RING 64

/*
 * US keyboard map indexed by AT/Linux keycode: {base, shifted} ASCII.  Mirrors
 * columns 0/1 of the i386 keyboardMap so both arches emit identical keycodes.
 * 0 = no character (modifier / unmapped).  Enter is 0x0D and Backspace 0x7F, as
 * on i386 (login/pgets accepts \r and DEL).
 */
static const u_int8_t g_keymap[90][2] = {
    [1] = {0x1B, 0x1B},                                                               /* ESC */
    [2] = {'1', '!'},    [3] = {'2', '@'},    [4] = {'3', '#'},    [5] = {'4', '$'},  /* 1-4 */
    [6] = {'5', '%'},    [7] = {'6', '^'},    [8] = {'7', '&'},    [9] = {'8', '*'},  /* 5-8 */
    [10] = {'9', '('},   [11] = {'0', ')'},   [12] = {'-', '_'},   [13] = {'=', '+'}, /* 9-= */
    [14] = {0x7F, 0x7F}, [15] = {'\t', '\t'},                                         /* BkSp Tab */
    [16] = {'q', 'Q'},   [17] = {'w', 'W'},   [18] = {'e', 'E'},   [19] = {'r', 'R'}, [20] = {'t', 'T'},
    [21] = {'y', 'Y'},   [22] = {'u', 'U'},   [23] = {'i', 'I'},   [24] = {'o', 'O'}, [25] = {'p', 'P'},
    [26] = {'[', '{'},   [27] = {']', '}'},   [28] = {0x0D, 0x0D}, /* Enter */
    [30] = {'a', 'A'},   [31] = {'s', 'S'},   [32] = {'d', 'D'},   [33] = {'f', 'F'}, [34] = {'g', 'G'},
    [35] = {'h', 'H'},   [36] = {'j', 'J'},   [37] = {'k', 'K'},   [38] = {'l', 'L'}, [39] = {';', ':'},
    [40] = {'\'', '"'},  [41] = {'`', '~'},   [43] = {'\\', '|'},  [44] = {'z', 'Z'}, [45] = {'x', 'X'},
    [46] = {'c', 'C'},   [47] = {'v', 'V'},   [48] = {'b', 'B'},   [49] = {'n', 'N'}, [50] = {'m', 'M'},
    [51] = {',', '<'},   [52] = {'.', '>'},   [53] = {'/', '?'},   [57] = {' ', ' '}, /* Space */
};

static kbd_event_t g_kbd_ring[RING];
static u_int32_t g_kbd_head, g_kbd_tail;
static mouse_event_t g_mouse_ring[RING];
static u_int32_t g_mouse_head, g_mouse_tail;

/* Cooking state accumulated across a device packet (ended by EV_SYN). */
static int g_shift;
static int g_ctrl;
static u_int8_t g_buttons;
static int g_dx, g_dy;
static int g_wheel;
static int g_mouse_dirty;

/**
 * Push a cooked keystroke onto the keyboard ring (drop if full).
 */
static void kbd_push(u_int32_t keycode, u_int8_t pressed)
{
	u_int32_t next = (g_kbd_head + 1) % RING;

	if (keycode == 0 || next == g_kbd_tail)
		return;
	g_kbd_ring[g_kbd_head].keycode = keycode;
	g_kbd_ring[g_kbd_head].pressed = pressed;
	g_kbd_head = next;
}

/**
 * Push a cooked mouse event (relative motion + button state) onto the ring.
 */
static void mouse_push(int dx, int dy, u_int8_t buttons, int wheel)
{
	u_int32_t next = (g_mouse_head + 1) % RING;

	if (next == g_mouse_tail)
		return;
	g_mouse_ring[g_mouse_head].dx = (int16_t)dx;
	g_mouse_ring[g_mouse_head].dy = (int16_t)dy;
	g_mouse_ring[g_mouse_head].buttons = buttons;
	g_mouse_ring[g_mouse_head].wheel = (int8_t)wheel;
	g_mouse_head = next;
}

/**
 * Translate a keyboard EV_KEY into a kbd_event keycode (ASCII, an arrow KEY_*,
 * or 0 for a modifier/unmapped key) and push it.  Tracks shift state.
 */
static void cook_key(u_int16_t code, u_int32_t value)
{
	u_int32_t kc;

	/* Modifiers: track state for the Ctrl/Shift ASCII folding AND emit the
	 * compositor modifier keycode (KEY_LSHIFT/LCTRL/LALT) as its own event, so
	 * GUI apps that treat a modifier as a real key — DOOM maps Ctrl->fire,
	 * Alt->strafe, Shift->run — receive it.  The terminal ignores these codes
	 * (term's key_to_seq drops KEY_LSHIFT..KEY_LALT).  Mirrors i386 atkbd.c. */
	if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT)
	{
		g_shift = (value != 0);
		kbd_push(KEY_LSHIFT, (u_int8_t)(value != 0));
		return;
	}

	if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL)
	{
		g_ctrl = (value != 0);
		kbd_push(KEY_LCTRL, (u_int8_t)(value != 0));
		return;
	}

	if (code == KEY_LEFTALT || code == KEY_RIGHTALT)
	{
		kbd_push(KEY_LALT, (u_int8_t)(value != 0));
		return;
	}

	switch (code)
	{
		case KEY_KP_UP:
			kc = KEY_UP;
			break;
		case KEY_KP_DOWN:
			kc = KEY_DOWN;
			break;
		case KEY_KP_LEFT:
			kc = KEY_LEFT;
			break;
		case KEY_KP_RIGHT:
			kc = KEY_RIGHT;
			break;
		default:
			kc = (code < 90) ? g_keymap[code][g_shift ? 1 : 0] : 0;
			/* Ctrl + letter → ASCII control code (Ctrl-C = 0x03, Ctrl-D = 0x04,
			 * …) so the tty line discipline sees VINTR/VEOF/VSUSP and raises the
			 * matching signal. */
			if (g_ctrl && kc != 0)
			{
				u_int32_t lc = (kc >= 'A' && kc <= 'Z') ? kc + 32 : kc;
				if (lc >= 'a' && lc <= 'z')
					kc = lc - 'a' + 1;
			}
			break;
	}

	/* value: 1 = press, 2 = autorepeat, 0 = release. */
	kbd_push(kc, (u_int8_t)(value != 0));
}

/**
 * virtio_input_poll callback: fold one raw Linux input event into the cooking
 * state.  Keyboard keys queue immediately; mouse motion/buttons accumulate and
 * are emitted as one mouse_event at the EV_SYN packet boundary.
 */
static void cook_event(int dev, u_int16_t type, u_int16_t code, u_int32_t value)
{
	(void)dev;
	switch (type)
	{
		case EV_KEY:
			if (code == BTN_LEFT)
			{
				g_buttons = value ? (g_buttons | MOUSE_BTN_LEFT) : (g_buttons & ~MOUSE_BTN_LEFT);
				g_mouse_dirty = 1;
			}
			else if (code == BTN_RIGHT)
			{
				g_buttons = value ? (g_buttons | MOUSE_BTN_RIGHT) : (g_buttons & ~MOUSE_BTN_RIGHT);
				g_mouse_dirty = 1;
			}
			else if (code == BTN_MIDDLE)
			{
				g_buttons = value ? (g_buttons | MOUSE_BTN_MIDDLE) : (g_buttons & ~MOUSE_BTN_MIDDLE);
				g_mouse_dirty = 1;
			}
			else
			{
				cook_key(code, value);
			}
			break;

		case EV_REL:
			if (code == REL_X)
				g_dx += (int)(int32_t)value;
			else if (code == REL_Y)
				g_dy += (int)(int32_t)value;
			else if (code == REL_WHEEL)
				g_wheel += (int)(int32_t)value;
			g_mouse_dirty = 1;
			break;

		case EV_SYN:
			if (g_mouse_dirty)
			{
				mouse_push(g_dx, g_dy, g_buttons, g_wheel);
				g_dx = 0;
				g_dy = 0;
				g_wheel = 0;
				g_mouse_dirty = 0;
			}
			break;

		default:
			break;
	}
}

/**
 * Drain all pending virtio-input events into the cooked kbd/mouse rings.
 */
static void input_drain(void)
{
	virtio_input_poll(cook_event);
}

/**
 * sys_getkbd (native 46) — return the next cooked keystroke, draining the
 * virtio-input queues first.
 *
 * @return 0 with *args->ev filled, or -1 if no keystroke is queued.
 */
int sys_getkbd(struct thread *td, struct sys_getkbd_args *args)
{
	spinLock(&g_input_lock);
	input_drain();

	if (g_kbd_tail == g_kbd_head)
	{
		spinUnlock(&g_input_lock);
		td->td_retval[0] = -1;
		return (-1);
	}
	*args->ev = g_kbd_ring[g_kbd_tail];
	g_kbd_tail = (g_kbd_tail + 1) % RING;
	spinUnlock(&g_input_lock);
	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_getmouse (native 44) — return the next cooked mouse event, draining the
 * virtio-input queues first.
 *
 * @return 0 with *args->ev filled, or -1 if no event is queued.
 */
int sys_getmouse(struct thread *td, struct sys_getmouse_args *args)
{
	spinLock(&g_input_lock);
	input_drain();

	if (g_mouse_tail == g_mouse_head)
	{
		spinUnlock(&g_input_lock);
		td->td_retval[0] = -1;
		return (-1);
	}
	*args->ev = g_mouse_ring[g_mouse_tail];
	g_mouse_tail = (g_mouse_tail + 1) % RING;
	spinUnlock(&g_input_lock);
	td->td_retval[0] = 0;
	return (0);
}
