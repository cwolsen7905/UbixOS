/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

#include <ubixos/tty.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/io.h>
#include <sys/video.h>
#include <isa/rs232.h>
#include <fs/devfs/devfs.h>
#include <string.h>

static tty_term *terms = 0x0;
tty_term *tty_foreground = 0x0;
static struct spinLock tty_spinLock = SPIN_LOCK_INITIALIZER;

/* Protects stdin[]/stdinSize from concurrent access by the rs232 ISR and task
 * context readers.  Use irq_save/restore so the same lock is safe from both
 * ISR and task context without deadlocking on a single-CPU system. */
static inline uint32_t irq_save_disable(void) {
	uint32_t flags;
	asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
	return (flags);
}
static inline void irq_restore(uint32_t flags) {
	asm volatile("pushl %0; popfl" : : "r"(flags) : "memory");
}

int tty_init() {
  int i = 0x0;

  /* Allocate memory for terminals */
  terms = (tty_term *) kmalloc(sizeof(tty_term) * TTY_MAX_TERMS);
  if (terms == 0x0)
    kpanic("tty_init: Failed to allocate memory. File: %s, Line: %i\n", __FILE__, __LINE__);

  /* Set up all default terminal information */
  for (i = 0; i < TTY_MAX_TERMS; i++) {
    terms[i].tty_buffer = (char *) kmalloc(80 * 60 * 2);
    if (terms[i].tty_buffer == 0x0)
      kpanic("tty_init: Failed to allocate buffer memory. File: %s, Line: %i\n", __FILE__, __LINE__);

    terms[i].tty_pointer = terms[i].tty_buffer;
    terms[i].tty_x      = 0x0;
    terms[i].tty_y      = 0x0;
    terms[i].tty_colour = 0x0A + i;
    terms[i].stdinSize  = 0;
    terms[i].t_linelen  = 0;
    terms[i].t_echo     = 1;
    terms[i].t_raw      = 0;
    terms[i].t_type     = TTY_TYPE_VGA;
  }

  /* Read tty0 current position (to migrate from kprintf). */
  outportByte(0x3D4, 0x0e);
  terms[0].tty_y = inportByte(0x3D5);
  outportByte(0x3D4, 0x0f);
  terms[0].tty_x = inportByte(0x3D5);

  /* Set up pointer for the foreground tty */
  tty_foreground = &terms[0];

  /* Set up the foreground ttys information */
  tty_foreground->tty_pointer = (char *) 0xB8000;

  /* Register VGA virtual TTY nodes in devfs */
  devfs_makeNode("ttyv0", 'c', 4, 0);
  devfs_makeNode("ttyv1", 'c', 4, 1);
  devfs_makeNode("ttyv2", 'c', 4, 2);
  devfs_makeNode("ttyv3", 'c', 4, 3);
  devfs_makeNode("com1",  'c', 4, 4);

  /* Return to let kernel know initialization is complete */
  kprintf("tty0: ready\n");

  return (0x0);
}

/*
 This will change the specified tty. It ultimately copies the screen
 to the foreground buffer copies the new ttys buffer to the screen and
 adjusts a couple pointers and we are good to go.
 */
int tty_change(uInt16 tty) {

  if (tty >= TTY_MAX_TERMS)
    kpanic("Error: Changing to an invalid tty. File: %s, Line: %i\n", __FILE__, __LINE__);

  /* Copy display buffer to tty buffer */
  memcpy(tty_foreground->tty_buffer, (char *) 0xB8000, (80 * 25 * 2));

  /* Copy new tty buffer to display buffer */
  memcpy((char *) 0xB8000, terms[tty].tty_buffer, (80 * 25 * 2));

  /*
   Set the tty_pointer to the internal buffer so I can continue 
   writing to what it believes is the screen
   */
  tty_foreground->tty_pointer = tty_foreground->tty_buffer;

  terms[tty].tty_pointer = (char *) 0xB8000;

  /* set new foreground tty */
  tty_foreground = &terms[tty];

  /* Adjust cursor when we change consoles */
  outportByte(0x3D4, 0x0F);
  outportByte(0x3D5, tty_foreground->tty_x);
  outportByte(0x3D4, 0x0E);
  outportByte(0x3D5, tty_foreground->tty_y);

  return (0x0);
}

int tty_print(char *string, tty_term *term) {
  unsigned int bufferOffset = 0x0, character = 0x0, i = 0x0;
  spinLock(&tty_spinLock);

  /* We Need To Get The Y Position */
  bufferOffset = term->tty_y;
  bufferOffset <<= 8;

  /* Then We Need To Add The X Position */
  bufferOffset += term->tty_x;
  bufferOffset <<= 1;

  while ((character = *string++)) {
    switch (character) {
      case '\n':
        bufferOffset = (bufferOffset / 160) * 160 + 160;
      break;
      default:
        term->tty_pointer[bufferOffset++] = character;
        term->tty_pointer[bufferOffset++] = term->tty_colour;
      break;
    } /* switch */

    /* Check To See If We Are Out Of Bounds */
    if (bufferOffset >= 160 * 25) {
      for (i = 0; i < 160 * 24; i++) {
        term->tty_pointer[i] = term->tty_pointer[i + 160];
      }
      for (i = 0; i < 80; i++) {
        term->tty_pointer[(160 * 24) + (i * 2)] = 0x20;
        term->tty_pointer[(160 * 24) + (i * 2) + 1] = term->tty_colour;
      }
      bufferOffset -= 160;
    }
  }

  bufferOffset >>= 1; /* Set the new cursor position  */
  term->tty_x = (bufferOffset & 0xFF);
  term->tty_y = (bufferOffset >> 0x8);

  if (term == tty_foreground) {
    outportByte(0x3D4, 0x0f);
    outportByte(0x3D5, term->tty_x);
    outportByte(0x3D4, 0x0e);
    outportByte(0x3D5, term->tty_y);
  }

  spinUnlock(&tty_spinLock);

  return (0x0);
}

tty_term *tty_find(uInt16 tty) {
  if (tty >= TTY_MAX_TERMS)
    return (NULL);
  return (&terms[tty]);
}

/*
 * tty_inject — push one ASCII character through the TTY line discipline.
 *
 * This is the single input entry point regardless of the physical source:
 * PS/2 keyboard, COM1 serial, or future telnet socket.  Echo and editing
 * (backspace, Ctrl-U, newline coalescing) are applied here.
 */
void
tty_inject(tty_term *tty, char ch)
{
	char echo_buf[2];
	int  i;

	if (tty == NULL)
		return;

	echo_buf[0] = ch;
	echo_buf[1] = '\0';

	switch (ch) {
	case '\b':
		if (tty->t_raw) {
			uint32_t f = irq_save_disable();
			if (tty->stdinSize < 512)
				tty->stdin[tty->stdinSize++] = '\b';
			irq_restore(f);
		} else if (tty->t_linelen > 0) {
			tty->t_linelen--;
			if (tty->t_echo) {
				if (tty->t_type == TTY_TYPE_SERIAL) {
					rs232_putc('\b');
					rs232_putc(' ');
					rs232_putc('\b');
				} else {
					backSpace();
				}
			}
		}
		break;
	case 0x15: /* Ctrl-U: erase line */
		if (!tty->t_raw)
			tty->t_linelen = 0;
		break;
	case '\r':
		ch = '\n';
		/* FALLTHROUGH */
	case '\n':
		if (tty->t_raw) {
			uint32_t f = irq_save_disable();
			if (tty->stdinSize < 512)
				tty->stdin[tty->stdinSize++] = '\n';
			irq_restore(f);
		} else {
			uint32_t f = irq_save_disable();
			for (i = 0; i < tty->t_linelen && tty->stdinSize < 512; i++)
				tty->stdin[tty->stdinSize++] = tty->t_linebuf[i];
			if (tty->stdinSize < 512)
				tty->stdin[tty->stdinSize++] = '\n';
			irq_restore(f);
			tty->t_linelen = 0;
			if (tty->t_echo) {
				if (tty->t_type == TTY_TYPE_SERIAL) {
					rs232_putc('\r');
					rs232_putc('\n');
				} else {
					tty_print("\n", tty);
				}
			}
		}
		break;
	default:
		if ((unsigned char)ch < 0x20)
			break;
		if (tty->t_raw) {
			uint32_t f = irq_save_disable();
			if (tty->stdinSize < 512)
				tty->stdin[tty->stdinSize++] = ch;
			irq_restore(f);
		} else {
			if (tty->t_linelen < 511) {
				tty->t_linebuf[tty->t_linelen++] = ch;
				if (tty->t_echo) {
					if (tty->t_type == TTY_TYPE_SERIAL) {
						rs232_putc(ch);
					} else {
						tty_print(echo_buf, tty);
					}
				}
			}
		}
		break;
	}
}

/***
 END
 ***/
