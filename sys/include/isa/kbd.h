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

#ifndef _ISA_KBD_H
#define _ISA_KBD_H

#include <sys/types.h>

struct kbd_event {
	uint32_t keycode;   /* translated ASCII or KEY_* special value */
	uint8_t  pressed;   /* 1 = key down, 0 = key up */
};
typedef struct kbd_event kbd_event_t;

/* Special key codes (>= 0x100 to avoid ASCII clash) */
#define KEY_UP     0x100
#define KEY_DOWN   0x101
#define KEY_LEFT   0x102
#define KEY_RIGHT  0x103
#define KEY_F1     0x104
#define KEY_F2     0x105
#define KEY_F3     0x106
#define KEY_F4     0x107
#define KEY_F5     0x108
#define KEY_F6     0x109
#define KEY_F7     0x10A
#define KEY_F8     0x10B
#define KEY_F9     0x10C
#define KEY_F10    0x10D
#define KEY_F11    0x10E
#define KEY_F12    0x10F
#define KEY_HOME   0x110
#define KEY_END    0x111
#define KEY_PGUP   0x112
#define KEY_PGDN   0x113
#define KEY_INS    0x114
#define KEY_DEL    0x115
#define KEY_LSHIFT 0x116   /* left or right shift (modifier press/release) */
#define KEY_LCTRL  0x117   /* left ctrl */
#define KEY_LALT   0x118   /* left alt */
#define KEY_ESC    0x1B

int  kbd_getEvent(kbd_event_t *ev);
void kbd_ring_push(uint32_t keycode, uint8_t pressed);
int  kbd_input_available(void);

/* Set to 1 by sys_mapfb when a GUI compositor maps the framebuffer.
 * Prevents VGA console processes from draining the keyboard ring. */
extern volatile int kbd_gui_mode;

/* Deferred VTY switch target (0-3).  Set to -1 when idle.
 * Written by keyboard ISRs/callbacks; consumed in task context by
 * sys_read / sys_select after calling tty_change(). */
extern volatile int tty_switch_slot;

/* Deferred GUI→text VTY switch target (0-3).  Set to -1 when idle.
 * Written by keyboard ISRs/callbacks on Ctrl+Alt+Fn; consumed in task
 * context by sys_read / sys_select after calling vesa_text_mode(). */
extern volatile int vesa_text_slot;

#endif /* _ISA_KBD_H */
