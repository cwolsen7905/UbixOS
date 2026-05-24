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
#include <ubixos/signal.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/io.h>
#include <sys/video.h>
#include <isa/rs232.h>
#include <fs/devfs/devfs.h>
#include <string.h>
#include <i386/signal.h>

static tty_term *terms = 0x0;
tty_term *tty_foreground = 0x0;
static struct spinLock tty_spinLock = SPIN_LOCK_INITIALIZER;

/* Protects stdin[]/stdinSize from concurrent access by the rs232 ISR and task
 * context readers.  Use irq_save/restore so the same lock is safe from both
 * ISR and task context without deadlocking on a single-CPU system. */
static inline u_int32_t irq_save_disable(void)
{
	u_int32_t flags;
	asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
	return (flags);
}
static inline void irq_restore(u_int32_t flags)
{
	asm volatile("pushl %0; popfl" : : "r"(flags) : "memory");
}

int tty_init()
{
	int i = 0x0;

	/* Allocate memory for terminals */
	terms = (tty_term *)kmalloc(sizeof(tty_term) * TTY_MAX_TERMS);
	if (terms == 0x0)
		kpanic("tty_init: Failed to allocate memory. File: %s, Line: %i\n", __FILE__, __LINE__);

	/* Set up all default terminal information */
	for (i = 0; i < TTY_MAX_TERMS; i++)
	{
		terms[i].tty_buffer = (char *)kmalloc(80 * 60 * 2);
		if (terms[i].tty_buffer == 0x0)
			kpanic("tty_init: Failed to allocate buffer memory. File: %s, Line: %i\n", __FILE__, __LINE__);

		terms[i].tty_pointer = terms[i].tty_buffer;
		terms[i].tty_x = 0x0;
		terms[i].tty_y = 0x0;
		terms[i].tty_colour = 0x0A + i;
		terms[i].stdinSize = 0;
		terms[i].t_linelen = 0;
		terms[i].t_echo = 1;
		terms[i].t_raw = 0;
		terms[i].t_type = TTY_TYPE_VGA;
		terms[i].t_eof = 0;
		terms[i].t_stopped = 0;

		/* Full termios — FreeBSD sane defaults */
		terms[i].t_termios.c_iflag = 0x2B02;
		terms[i].t_termios.c_oflag = 0x3;
		terms[i].t_termios.c_cflag = 0x4B00;
		terms[i].t_termios.c_lflag = IEXTEN | ICANON | ISIG | ECHOCTL | ECHO | ECHOE | ECHOK | ECHOKE;
		terms[i].t_termios.c_cc[0] = 4;    /* VEOF */
		terms[i].t_termios.c_cc[1] = 255;  /* VEOL */
		terms[i].t_termios.c_cc[2] = 255;  /* VEOL2 */
		terms[i].t_termios.c_cc[3] = 127;  /* VERASE */
		terms[i].t_termios.c_cc[4] = 23;   /* VWERASE */
		terms[i].t_termios.c_cc[5] = 21;   /* VKILL */
		terms[i].t_termios.c_cc[6] = 18;   /* VREPRINT */
		terms[i].t_termios.c_cc[7] = 8;    /* spare */
		terms[i].t_termios.c_cc[8] = 3;    /* VINTR */
		terms[i].t_termios.c_cc[9] = 28;   /* VQUIT */
		terms[i].t_termios.c_cc[10] = 26;  /* VSUSP */
		terms[i].t_termios.c_cc[11] = 25;  /* VDSUSP */
		terms[i].t_termios.c_cc[12] = 17;  /* VSTART */
		terms[i].t_termios.c_cc[13] = 19;  /* VSTOP */
		terms[i].t_termios.c_cc[14] = 22;  /* VLNEXT */
		terms[i].t_termios.c_cc[15] = 15;  /* VDISCARD */
		terms[i].t_termios.c_cc[16] = 1;   /* VMIN */
		terms[i].t_termios.c_cc[17] = 0;   /* VTIME */
		terms[i].t_termios.c_cc[18] = 20;  /* VSTATUS */
		terms[i].t_termios.c_cc[19] = 255; /* spare */
		terms[i].t_termios.c_ispeed = 9600;
		terms[i].t_termios.c_ospeed = 9600;

		terms[i].t_winsize.ws_row = 25;
		terms[i].t_winsize.ws_col = 80;
		terms[i].t_winsize.ws_xpixel = 0;
		terms[i].t_winsize.ws_ypixel = 0;
		terms[i].t_pgrp = 0;

		/* ANSI state machine */
		terms[i].t_esc_state = 0;
		terms[i].t_esc_priv = 0;
		terms[i].t_esc_nparams = 0;
		terms[i].t_default_colour = (u_int8_t)terms[i].tty_colour;
		terms[i].t_saved_x = 0;
		terms[i].t_saved_y = 0;
		terms[i].t_saved_colour = (u_int8_t)terms[i].tty_colour;
		memset(terms[i].t_esc_params, 0, sizeof(terms[i].t_esc_params));
	}

	/* Read tty0 current position (to migrate from kprintf). */
	outportByte(0x3D4, 0x0e);
	terms[0].tty_y = inportByte(0x3D5);
	outportByte(0x3D4, 0x0f);
	terms[0].tty_x = inportByte(0x3D5);
	if (terms[0].tty_y > 24)
		terms[0].tty_y = 24;

	/* Set up pointer for the foreground tty */
	tty_foreground = &terms[0];

	/* Set up the foreground ttys information */
	tty_foreground->tty_pointer = (char *)0xB8000;

	/* Register VGA virtual TTY nodes in devfs */
	devfs_makeNode("ttyv0", 'c', 4, 0);
	devfs_makeNode("ttyv1", 'c', 4, 1);
	devfs_makeNode("ttyv2", 'c', 4, 2);
	devfs_makeNode("ttyv3", 'c', 4, 3);
	devfs_makeNode("com1", 'c', 4, 4);

	/* Return to let kernel know initialization is complete */
	kprintf("tty0: ready\n");

	return (0x0);
}

/*
 This will change the specified tty. It ultimately copies the screen
 to the foreground buffer copies the new ttys buffer to the screen and
 adjusts a couple pointers and we are good to go.
 */
int tty_change(u_int16_t tty)
{

	if (tty >= TTY_MAX_TERMS)
		kpanic("Error: Changing to an invalid tty. File: %s, Line: %i\n", __FILE__, __LINE__);

	/* Copy display buffer to tty buffer */
	memcpy(tty_foreground->tty_buffer, (char *)0xB8000, (80 * 25 * 2));

	/* Copy new tty buffer to display buffer */
	memcpy((char *)0xB8000, terms[tty].tty_buffer, (80 * 25 * 2));

	/*
	 Set the tty_pointer to the internal buffer so I can continue
	 writing to what it believes is the screen
	 */
	tty_foreground->tty_pointer = tty_foreground->tty_buffer;

	terms[tty].tty_pointer = (char *)0xB8000;

	/* set new foreground tty */
	tty_foreground = &terms[tty];

	/* Adjust cursor when we change consoles */
	outportByte(0x3D4, 0x0F);
	outportByte(0x3D5, tty_foreground->tty_x);
	outportByte(0x3D4, 0x0E);
	outportByte(0x3D5, tty_foreground->tty_y);

	return (0x0);
}

/* ANSI colour: black red green yellow blue magenta cyan white → VGA indices */
static const u_int8_t ansi_to_vga[8] = {0, 4, 2, 6, 1, 5, 3, 7};

/* Scroll the terminal up one line and leave cursor on the last row. */
static void tty_scroll(tty_term *term)
{
	unsigned int i;
	for (i = 0; i < 160u * 24u; i++)
		term->tty_pointer[i] = term->tty_pointer[i + 160];
	for (i = 0; i < 80u; i++)
	{
		term->tty_pointer[(160u * 24u) + (i * 2u)] = 0x20;
		term->tty_pointer[(160u * 24u) + (i * 2u) + 1] = term->tty_colour;
	}
}

/* Execute a complete CSI sequence.  Returns updated bufferOffset. */
static unsigned int tty_csi_execute(tty_term *term, unsigned int bufferOffset, char cmd)
{
	unsigned int linear, row, col, i;
	unsigned int p0, p1;

	linear = bufferOffset / 2u;
	row = linear / 80u;
	col = linear % 80u;

	p0 = term->t_esc_params[0];
	p1 = term->t_esc_params[1];

	switch (cmd)
	{
	case 'A': /* cursor up */
		if (p0 == 0)
			p0 = 1;
		row = (row >= p0) ? row - p0 : 0;
		bufferOffset = (row * 80u + col) * 2u;
		break;
	case 'B': /* cursor down */
		if (p0 == 0)
			p0 = 1;
		row += p0;
		if (row >= 25u)
			row = 24u;
		bufferOffset = (row * 80u + col) * 2u;
		break;
	case 'C': /* cursor right */
		if (p0 == 0)
			p0 = 1;
		col += p0;
		if (col >= 80u)
			col = 79u;
		bufferOffset = (row * 80u + col) * 2u;
		break;
	case 'D': /* cursor left */
		if (p0 == 0)
			p0 = 1;
		col = (col >= p0) ? col - p0 : 0;
		bufferOffset = (row * 80u + col) * 2u;
		break;
	case 'G': /* cursor horizontal absolute (1-based col) */
		col = (p0 > 0u ? p0 - 1u : 0u);
		if (col >= 80u)
			col = 79u;
		bufferOffset = (row * 80u + col) * 2u;
		break;
	case 'H':
	case 'f': /* cursor position: row;col (1-based) */
		row = (p0 > 0u ? p0 - 1u : 0u);
		col = (p1 > 0u ? p1 - 1u : 0u);
		if (row >= 25u)
			row = 24u;
		if (col >= 80u)
			col = 79u;
		bufferOffset = (row * 80u + col) * 2u;
		break;
	case 'J': /* erase display */
		switch (p0)
		{
		case 0: /* cursor to end */
			for (i = bufferOffset; i < 160u * 25u; i += 2u)
			{
				term->tty_pointer[i] = ' ';
				term->tty_pointer[i + 1] = term->tty_colour;
			}
			break;
		case 1: /* start to cursor */
			for (i = 0; i <= bufferOffset && i + 1 < 160u * 25u; i += 2u)
			{
				term->tty_pointer[i] = ' ';
				term->tty_pointer[i + 1] = term->tty_colour;
			}
			break;
		case 2:
		case 3: /* whole screen */
			for (i = 0; i < 160u * 25u; i += 2u)
			{
				term->tty_pointer[i] = ' ';
				term->tty_pointer[i + 1] = term->tty_colour;
			}
			bufferOffset = 0;
			break;
		}
		break;
	case 'K': /* erase line */
		switch (p0)
		{
		case 0: /* cursor to end of line */
			for (i = col; i < 80u; i++)
			{
				term->tty_pointer[row * 160u + i * 2u] = ' ';
				term->tty_pointer[row * 160u + i * 2u + 1] = term->tty_colour;
			}
			break;
		case 1: /* start of line to cursor */
			for (i = 0; i <= col; i++)
			{
				term->tty_pointer[row * 160u + i * 2u] = ' ';
				term->tty_pointer[row * 160u + i * 2u + 1] = term->tty_colour;
			}
			break;
		case 2: /* whole line */
			for (i = 0; i < 80u; i++)
			{
				term->tty_pointer[row * 160u + i * 2u] = ' ';
				term->tty_pointer[row * 160u + i * 2u + 1] = term->tty_colour;
			}
			break;
		}
		break;
	case 'P': /* delete characters (shift left) */
		if (p0 == 0)
			p0 = 1;
		for (i = col; i + p0 < 80u; i++)
		{
			term->tty_pointer[row * 160u + i * 2u] = term->tty_pointer[row * 160u + (i + p0) * 2u];
			term->tty_pointer[row * 160u + i * 2u + 1] = term->tty_pointer[row * 160u + (i + p0) * 2u + 1];
		}
		for (i = 80u - p0; i < 80u; i++)
		{
			term->tty_pointer[row * 160u + i * 2u] = ' ';
			term->tty_pointer[row * 160u + i * 2u + 1] = term->tty_colour;
		}
		break;
	case 'm':
	{ /* SGR — select graphic rendition */
		unsigned int k;
		unsigned int nparams = (term->t_esc_nparams > 0) ? term->t_esc_nparams : 1;
		for (k = 0; k < nparams; k++)
		{
			unsigned int v = term->t_esc_params[k];
			switch (v)
			{
			case 0:
				term->tty_colour = term->t_default_colour;
				break;
			case 1: /* bold → bright fg */
				term->tty_colour |= 0x08u;
				break;
			case 2:
			case 22: /* dim / normal */
				term->tty_colour &= (u_int8_t)~0x08u;
				break;
			case 5:
			case 6: /* blink → bright bg */
				term->tty_colour |= 0x80u;
				break;
			case 7:
			{ /* reverse video */
				u_int8_t fg = term->tty_colour & 0x07u;
				u_int8_t bg = (term->tty_colour >> 4) & 0x07u;
				term->tty_colour = (u_int8_t)((fg << 4) | bg | (term->tty_colour & 0x88u));
				break;
			}
			case 27:
				break; /* reverse off — no tracking */
			case 39:       /* default fg */
				term->tty_colour = (u_int8_t)((term->tty_colour & 0xF0u) | (term->t_default_colour & 0x0Fu));
				break;
			case 49: /* default bg */
				term->tty_colour = (u_int8_t)((term->tty_colour & 0x0Fu) | (term->t_default_colour & 0xF0u));
				break;
			default:
				if (v >= 30u && v <= 37u)
					term->tty_colour = (u_int8_t)((term->tty_colour & 0xF8u) | ansi_to_vga[v - 30u]);
				else if (v >= 90u && v <= 97u)
					term->tty_colour = (u_int8_t)((term->tty_colour & 0xF0u) | (ansi_to_vga[v - 90u] | 8u));
				else if (v >= 40u && v <= 47u)
					term->tty_colour = (u_int8_t)((term->tty_colour & 0x8Fu) | (u_int8_t)(ansi_to_vga[v - 40u] << 4u));
				else if (v >= 100u && v <= 107u)
					term->tty_colour = (u_int8_t)((term->tty_colour & 0x0Fu) | (u_int8_t)((ansi_to_vga[v - 100u] | 8u) << 4u));
				break;
			}
		}
		break;
	}
	case 's': /* save cursor */
		term->t_saved_x = term->tty_x;
		term->t_saved_y = term->tty_y;
		term->t_saved_colour = term->tty_colour;
		break;
	case 'u': /* restore cursor */
		term->tty_x = term->t_saved_x;
		term->tty_y = term->t_saved_y;
		term->tty_colour = term->t_saved_colour;
		bufferOffset = ((unsigned int)term->tty_y * 256u + term->tty_x) * 2u;
		break;
	/* Intentionally ignored: h/l (mode set/reset), r (scroll region), n (DSR) */
	default:
		break;
	}
	return (bufferOffset);
}

int tty_print(char *string, tty_term *term)
{
	unsigned int bufferOffset = 0x0, character = 0x0;

	/* IXON: output suspended by Ctrl-S */
	if (term->t_stopped)
		return (0);

	spinLock(&tty_spinLock);

	/* Reconstruct byte offset from the split tty_y:tty_x linear cursor. */
	bufferOffset = term->tty_y;
	bufferOffset <<= 8;
	bufferOffset += term->tty_x;
	bufferOffset <<= 1;

	while ((character = (unsigned char)*string++))
	{
		/* ── ANSI/VT100 state machine ── */
		if (term->t_esc_state == 1)
		{
			/* Received ESC — next char determines sequence type */
			switch (character)
			{
			case '[':
				term->t_esc_state = 2;
				term->t_esc_priv = 0;
				term->t_esc_nparams = 0;
				memset(term->t_esc_params, 0, sizeof(term->t_esc_params));
				break;
			case 'M':
			{ /* reverse index — scroll down if at top */
				unsigned int row = (bufferOffset / 2u) / 80u;
				if (row > 0u)
				{
					bufferOffset -= 160u;
				}
				else
				{
					unsigned int j;
					for (j = 160u * 24u; j >= 160u; j--)
						term->tty_pointer[j] = term->tty_pointer[j - 160u];
					for (j = 0; j < 80u; j++)
					{
						term->tty_pointer[j * 2u] = ' ';
						term->tty_pointer[j * 2u + 1] = term->tty_colour;
					}
				}
				term->t_esc_state = 0;
				break;
			}
			case '=':
			case '>': /* application/numeric keypad — ignore */
				term->t_esc_state = 0;
				break;
			default:
				term->t_esc_state = 0;
				break;
			}
			continue;
		}

		if (term->t_esc_state == 2)
		{
			/* Collecting CSI parameters */
			if (character == '?')
			{
				term->t_esc_priv = 1;
				continue;
			}
			if (character >= '0' && character <= '9')
			{
				if (term->t_esc_nparams == 0)
					term->t_esc_nparams = 1;
				term->t_esc_params[term->t_esc_nparams - 1] = (u_int16_t)(term->t_esc_params[term->t_esc_nparams - 1] * 10u + (unsigned int)(character - '0'));
				continue;
			}
			if (character == ';')
			{
				if (term->t_esc_nparams < 8u)
					term->t_esc_nparams++;
				continue;
			}
			/* Final character — dispatch */
			if (term->t_esc_nparams == 0 && character != 's' && character != 'u')
				term->t_esc_nparams = 1;
			bufferOffset = tty_csi_execute(term, bufferOffset, (char)character);
			term->t_esc_state = 0;
			/* Skip scroll check for control commands */
			if (bufferOffset < 160u * 25u)
				continue;
			/* Fall through to scroll if somehow past end */
		}

		if (term->t_esc_state != 0)
		{
			term->t_esc_state = 0;
			continue;
		}

		/* ── Normal character output ── */
		switch (character)
		{
		case 0x1B: /* ESC */
			term->t_esc_state = 1;
			break;
		case '\r': /* carriage return — column 0, same row */
			bufferOffset = (bufferOffset / 160u) * 160u;
			break;
		case '\n':
			/* Phase 9: OPOST+ONLCR → col 0 of next row; OPOST only → same col, next row */
			if (term->t_termios.c_oflag & OPOST)
			{
				if (term->t_termios.c_oflag & ONLCR)
					bufferOffset = (bufferOffset / 160u) * 160u + 160u; /* CR+LF */
				else
					bufferOffset += 160u; /* LF only — stay in same column */
			}
			break;
		case '\b': /* move cursor left without erasing */
			if (bufferOffset >= 2u)
				bufferOffset -= 2u;
			break;
		case '\t':
		{ /* advance to next 8-column tab stop */
			unsigned int col = (bufferOffset / 2u) % 80u;
			unsigned int spaces = 8u - (col % 8u);
			while (spaces-- > 0 && (bufferOffset / 2u) % 80u < 79u)
			{
				term->tty_pointer[bufferOffset++] = ' ';
				term->tty_pointer[bufferOffset++] = term->tty_colour;
			}
			break;
		}
		default:
			if (character >= 0x20u)
			{
				term->tty_pointer[bufferOffset++] = (char)character;
				term->tty_pointer[bufferOffset++] = term->tty_colour;
			}
			break;
		} /* switch */

		/* Scroll if past last line */
		if (bufferOffset >= 160u * 25u)
		{
			tty_scroll(term);
			bufferOffset -= 160u;
		}
	} /* while */

	bufferOffset >>= 1; /* convert byte offset back to linear cursor position */
	term->tty_x = (u_int16_t)(bufferOffset & 0xFFu);
	term->tty_y = (u_int16_t)(bufferOffset >> 8u);

	if (term == tty_foreground)
	{
		outportByte(0x3D4, 0x0f);
		outportByte(0x3D5, term->tty_x);
		outportByte(0x3D4, 0x0e);
		outportByte(0x3D5, term->tty_y);
	}

	spinUnlock(&tty_spinLock);
	return (0x0);
}

tty_term *tty_find(u_int16_t tty)
{
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
void tty_inject(tty_term *tty, char ch)
{
	char echo_buf[2];
	int i;

	if (tty == NULL)
		return;

	/* Phase 8 IXON: Ctrl-S / Ctrl-Q flow control — consume before anything else. */
	if (tty->t_termios.c_iflag & IXON)
	{
		u_int8_t uc8 = (u_int8_t)ch;
		if (uc8 == tty->t_termios.c_cc[VSTOP])
		{
			tty->t_stopped = 1;
			return;
		}
		if (uc8 == tty->t_termios.c_cc[VSTART])
		{
			tty->t_stopped = 0;
			return;
		}
	}

	/* Phase 6: signal-generating characters (ISIG).  Consumed here; not
	 * forwarded to the line buffer or the raw ring. */
	if (tty->t_termios.c_lflag & ISIG)
	{
		u_int8_t uc = (u_int8_t)ch;
		if (uc == tty->t_termios.c_cc[VINTR])
		{
			signal_post_tty(tty, SIGINT);
			return;
		}
		if (uc == tty->t_termios.c_cc[VQUIT])
		{
			signal_post_tty(tty, SIGQUIT);
			return;
		}
		if (uc == tty->t_termios.c_cc[VSUSP])
		{
			signal_post_tty(tty, SIGTSTP);
			return;
		}
	}

	echo_buf[0] = ch;
	echo_buf[1] = '\0';

	/* Raw mode: every byte goes straight to the ring. */
	if (tty->t_raw)
	{
		u_int32_t f = irq_save_disable();
		if (tty->stdinSize < 512)
			tty->stdin[tty->stdinSize++] = (u_int8_t)ch;
		irq_restore(f);
		return;
	}

	/* Canonical mode: c_cc[] dispatch (Phase 7). */
	{
		u_int8_t uc = (u_int8_t)ch;

		/* VERASE: erase one character */
		if (uc == tty->t_termios.c_cc[VERASE])
		{
			if (tty->t_linelen > 0)
			{
				tty->t_linelen--;
				if (tty->t_echo && (tty->t_termios.c_lflag & ECHOE))
				{
					if (tty->t_type == TTY_TYPE_SERIAL)
					{
						rs232_putc('\b');
						rs232_putc(' ');
						rs232_putc('\b');
					}
					else
					{
						backSpace();
					}
				}
			}
			return;
		}

		/* VKILL: erase entire input line */
		if (uc == tty->t_termios.c_cc[VKILL])
		{
			tty->t_linelen = 0;
			if (tty->t_echo && (tty->t_termios.c_lflag & ECHOK))
			{
				if (tty->t_type == TTY_TYPE_SERIAL)
				{
					rs232_putc('\r');
					rs232_putc('\n');
				}
				else
				{
					tty_print("\n", tty);
				}
			}
			return;
		}

		/* VWERASE: erase last word (Ctrl-W) */
		if (uc == tty->t_termios.c_cc[VWERASE])
		{
			/* skip trailing spaces */
			while (tty->t_linelen > 0 && tty->t_linebuf[tty->t_linelen - 1] == ' ')
				tty->t_linelen--;
			/* erase non-space characters */
			while (tty->t_linelen > 0 && tty->t_linebuf[tty->t_linelen - 1] != ' ')
			{
				tty->t_linelen--;
				if (tty->t_echo && (tty->t_termios.c_lflag & ECHOE))
				{
					if (tty->t_type == TTY_TYPE_SERIAL)
					{
						rs232_putc('\b');
						rs232_putc(' ');
						rs232_putc('\b');
					}
					else
					{
						backSpace();
					}
				}
			}
			return;
		}

		/* VEOF: flush partial line without newline (Ctrl-D) */
		if (uc == tty->t_termios.c_cc[VEOF])
		{
			u_int32_t f = irq_save_disable();
			for (i = 0; i < tty->t_linelen && tty->stdinSize < 512; i++)
				tty->stdin[tty->stdinSize++] = tty->t_linebuf[i];
			irq_restore(f);
			tty->t_linelen = 0;
			/* stdinSize==0 here means empty line → reader gets 0 bytes (EOF) */
			tty->t_eof = 1;
			return;
		}

		/* CR → NL (Phase 8: gated on ICRNL) */
		if (ch == '\r')
		{
			if (tty->t_termios.c_iflag & ICRNL)
				ch = '\n';
			/* ICRNL off: CR falls through as 0x0D, discarded below */
		}

		/* End of line: flush line buffer to stdin ring */
		if (ch == '\n')
		{
			u_int32_t f = irq_save_disable();
			for (i = 0; i < tty->t_linelen && tty->stdinSize < 512; i++)
				tty->stdin[tty->stdinSize++] = tty->t_linebuf[i];
			if (tty->stdinSize < 512)
				tty->stdin[tty->stdinSize++] = '\n';
			irq_restore(f);
			tty->t_linelen = 0;
			if (tty->t_echo)
			{
				if (tty->t_type == TTY_TYPE_SERIAL)
				{
					rs232_putc('\r');
					rs232_putc('\n');
				}
				else
				{
					tty_print("\n", tty);
				}
			}
			return;
		}

		/* Discard unhandled control characters */
		if (uc < 0x20)
			return;

		/* Regular character: append to line buffer and echo */
		if (tty->t_linelen < 511)
		{
			tty->t_linebuf[tty->t_linelen++] = ch;
			if (tty->t_echo)
			{
				if (tty->t_type == TTY_TYPE_SERIAL)
				{
					rs232_putc(ch);
				}
				else
				{
					tty_print(echo_buf, tty);
				}
			}
		}
	}
}
