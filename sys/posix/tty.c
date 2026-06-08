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
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/io.h>
#include <sys/video.h>
#include <isa/rs232.h>
#include <fs/devfs/devfs.h>
#include <string.h>
#include <i386/signal.h>
#include <sys/descrip.h> /* struct file, struct fileOps (path B fileops) */
#include <sys/errno.h>
#include <lib/kmalloc.h>
#include <isa/kbd.h>      /* kbd_gui_mode */
#include <isa/atkbd.h>    /* tty_switch_slot, vesa_text_slot */
#include <lib/vesa.h>     /* vesa_text_mode */
#include <fs/vfs/file.h>  /* getchar */

/* True when an unblocked signal is pending (makes the blocking tty read loops
 * interruptible).  Mirrors the definition in sys/posix/vfs_calls.c. */
#define SIG_PENDING_UNBLOCKED(td) ((td)->sig_pending & ~(td)->sigmask.__bits[0])

static tty_term *terms = 0x0;
tty_term *tty_foreground = 0x0;
static struct spinLock tty_spinLock = SPIN_LOCK_INITIALIZER;

/* Scratch for tty_resize content preservation (too big for the kernel stack). */
static char g_tty_resize_tmp[TTY_MAX_COLS * TTY_MAX_ROWS * 2];

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

/*
 * Load FreeBSD-sane termios defaults and an 80x25 window onto a terminal.
 * Shared by tty_init() (first-time setup) and pty_alloc() (so a reused pty
 * slot returns to canonical mode even if the previous owner set raw mode).
 */
static void tty_init_termios(tty_term *t)
{
	t->t_termios.c_iflag = 0x2B02;
	t->t_termios.c_oflag = 0x3;
	t->t_termios.c_cflag = 0x4B00;
	t->t_termios.c_lflag = IEXTEN | ICANON | ISIG | ECHOCTL | ECHO | ECHOE | ECHOK | ECHOKE;
	t->t_termios.c_cc[0] = 4;    /* VEOF */
	t->t_termios.c_cc[1] = 255;  /* VEOL */
	t->t_termios.c_cc[2] = 255;  /* VEOL2 */
	t->t_termios.c_cc[3] = 127;  /* VERASE */
	t->t_termios.c_cc[4] = 23;   /* VWERASE */
	t->t_termios.c_cc[5] = 21;   /* VKILL */
	t->t_termios.c_cc[6] = 18;   /* VREPRINT */
	t->t_termios.c_cc[7] = 8;    /* spare */
	t->t_termios.c_cc[8] = 3;    /* VINTR */
	t->t_termios.c_cc[9] = 28;   /* VQUIT */
	t->t_termios.c_cc[10] = 26;  /* VSUSP */
	t->t_termios.c_cc[11] = 25;  /* VDSUSP */
	t->t_termios.c_cc[12] = 17;  /* VSTART */
	t->t_termios.c_cc[13] = 19;  /* VSTOP */
	t->t_termios.c_cc[14] = 22;  /* VLNEXT */
	t->t_termios.c_cc[15] = 15;  /* VDISCARD */
	t->t_termios.c_cc[16] = 1;   /* VMIN */
	t->t_termios.c_cc[17] = 0;   /* VTIME */
	t->t_termios.c_cc[18] = 20;  /* VSTATUS */
	t->t_termios.c_cc[19] = 255; /* spare */
	t->t_termios.c_ispeed = 9600;
	t->t_termios.c_ospeed = 9600;

	t->t_winsize.ws_row = 25;
	t->t_winsize.ws_col = 80;
	t->t_winsize.ws_xpixel = 0;
	t->t_winsize.ws_ypixel = 0;
	t->t_cols = 80;
	t->t_rows = 25;
}

/*
 * console_stdin_ready — g_console_stdin_ready hook (path B): drain the calling
 * process's controlling-terminal input (serial rx ring or kbd ring, through the
 * line discipline) and report whether a complete canonical line is waiting in
 * stdin[].  Used by sys_select / sys_poll so the generic fd layer never touches
 * the serial/kbd/tty symbols directly.
 *
 * @return non-zero if stdin has a line ready.
 */
static int console_stdin_ready(void)
{
	tty_term *t = _current->term;

	if (t == NULL)
		return (0);

	if (t->t_type == TTY_TYPE_SERIAL)
	{
		int raw;
		while ((raw = serial_rx_getbyte()) >= 0)
			tty_inject(t, (char)raw);
		return (t->stdinSize > 0);
	}

	/* VGA console: drain the kbd ring through the line discipline (which echoes);
	 * ready only once a full line has flushed to the foreground term's stdin[]. */
	kbd_event_t kev;
	while (kbd_getEvent(&kev) == 0)
	{
		if (kev.pressed && kev.keycode != 0 && kev.keycode < 0x100 && tty_foreground != NULL)
			tty_inject(tty_foreground, (char)(u_int8_t)kev.keycode);
	}
	return (tty_foreground != NULL && tty_foreground->stdinSize > 0);
}

/*
 * tty_fo_read — fileops read handler for FD_TYPE_TTY / FD_TYPE_TTYV and the
 * controlling-terminal placeholder fds.  Moved out of sys/posix/vfs_calls.c
 * (path B) so the generic syscall core no longer references the console-read
 * symbols (getchar / serial_rx_getbyte / the Ctrl+Alt+Fn VT-switch globals /
 * tty_inject / signal_post_pgrp).  Three cases, by source: a TTYV fd reads its
 * bound tty_term (fp->data); a serial controlling terminal drains the rx ring
 * through the line discipline; the VGA controlling terminal blocks until it is
 * the foreground VTY (servicing deferred VT switches) then drains the keyboard.
 * Line-oriented: returns on '\n'/'\r', a full buffer, or EOF.  Sets
 * td_retval[0] = bytes read; returns 0 / errno.
 */
static int tty_fo_read(struct file *fp, struct thread *td, void *vbuf, size_t nbyte)
{
	volatile char *buf = vbuf;
	int x = 0;
	char c = 0x0;

	/* Specific virtual terminal fd: read from the bound tty_term. */
	if (fp != NULL && fp->fd_type == FD_TYPE_TTYV)
	{
		tty_term *t = (tty_term *)fp->data;
		/* Background process reading from its controlling tty stops on SIGTTIN. */
		if (t != NULL && t->t_pgrp != 0 && (pid_t)_current->pgrp != t->t_pgrp)
		{
			signal_post_pgrp((pid_t)_current->pgrp, SIGTTIN);
			td->td_retval[0] = -EINTR;
			return (EINTR);
		}
		for (;;)
		{
			if (SIG_PENDING_UNBLOCKED(td))
			{
				td->td_retval[0] = -EINTR;
				return (EINTR);
			}
			if (t->stdinSize > 0)
			{
				int i;
				c = t->stdin[0];
				t->stdinSize--;
				for (i = 0; i < t->stdinSize; i++)
					t->stdin[i] = t->stdin[i + 1];
				buf[x++] = c;
				if (c == '\n' || x >= (int)nbyte)
					break;
			}
			else if (t->t_eof)
			{
				t->t_eof = 0;
				break;
			}
			else if (!t->t_inuse)
			{
				/* pty released out from under us (master owner died): EOF so an
				 * orphaned shell unwinds instead of yielding forever. */
				break;
			}
			else
			{
				sched_yield();
			}
		}
		td->td_retval[0] = x;
		return (0);
	}

	/* Serial controlling terminal: drain rx ring → line discipline → stdin[]. */
	if (_current->term != NULL && _current->term->t_type == TTY_TYPE_SERIAL)
	{
		while (x < (int)nbyte)
		{
			int raw;
			while ((raw = serial_rx_getbyte()) >= 0)
				tty_inject(_current->term, (char)raw);

			if (_current->term->stdinSize > 0)
			{
				int i;
				c = _current->term->stdin[0];
				_current->term->stdinSize--;
				for (i = 0; i < _current->term->stdinSize; i++)
					_current->term->stdin[i] = _current->term->stdin[i + 1];
				buf[x++] = c;
				if (c == '\n' || x >= (int)nbyte)
					break;
			}
			else if (_current->term->t_eof)
			{
				_current->term->t_eof = 0;
				break;
			}
			else
			{
				if (SIG_PENDING_UNBLOCKED(td))
				{
					td->td_retval[0] = -EINTR;
					return (EINTR);
				}
				sched_yield();
			}
		}
		td->td_retval[0] = x;
		return (0);
	}

	/* VGA controlling terminal: block until this terminal is the active
	 * foreground, servicing deferred Ctrl+Alt+Fn switches, then drain the kbd. */
	{
		tty_term *t_bg = _current->ct_tty ? _current->ct_tty : _current->term;
		if (t_bg != NULL && t_bg->t_pgrp != 0 && (pid_t)_current->pgrp != t_bg->t_pgrp)
		{
			signal_post_pgrp((pid_t)_current->pgrp, SIGTTIN);
			td->td_retval[0] = -EINTR;
			return (EINTR);
		}
	}
	for (;;)
	{
		if (SIG_PENDING_UNBLOCKED(td))
		{
			td->td_retval[0] = -EINTR;
			return (EINTR);
		}
		if (vesa_text_slot >= 0)
		{
			int slot = vesa_text_slot;
			vesa_text_slot = -1;
			tty_change((u_int16_t)slot);
			vesa_text_mode();
			kbd_gui_mode = 0;
		}
		if (tty_switch_slot >= 0)
		{
			int slot = tty_switch_slot;
			tty_switch_slot = -1;
			tty_change((u_int16_t)slot);
		}
		if (kbd_gui_mode || _current->term != tty_foreground)
		{
			sched_yield();
			continue;
		}
		c = getchar();
		if (c != 0x0)
		{
			buf[x++] = c;
			if (c == '\n' || c == '\r' || x >= (int)nbyte)
				break;
		}
		else if (tty_foreground != NULL && tty_foreground->t_eof)
		{
			tty_foreground->t_eof = 0;
			break;
		}
		else
		{
			sched_yield();
		}
	}
	td->td_retval[0] = x;
	return (0);
}

/*
 * tty_fo_write — fileops write handler for FD_TYPE_TTY / FD_TYPE_TTYV and the
 * controlling-terminal placeholder fds (fp->fd == NULL).  Moved out of
 * sys/posix/vfs_calls.c (path B fileops) so the generic syscall core no longer
 * references the console output symbols (rs232_putc / tty_print).  A TTYV fd
 * writes to its bound tty_term (fp->data); everything else writes to the
 * process's controlling terminal (ct_tty, else term).  Sets td_retval[0],
 * returns 0 / errno.
 */
static int tty_fo_write(struct file *fp, struct thread *td, const void *vbuf, size_t nbyte)
{
	char *buffer;
	tty_term *t;

	if (fp != NULL && fp->fd_type == FD_TYPE_TTYV)
		t = (tty_term *)fp->data;
	else
		t = _current->ct_tty ? _current->ct_tty : _current->term;

	/* Phase 12: a background process writing to its controlling tty with TOSTOP
	 * set stops on SIGTTOU. */
	if (t != NULL && t->t_pgrp != 0 && (pid_t)_current->pgrp != t->t_pgrp && (t->t_termios.c_lflag & TOSTOP))
	{
		signal_post_pgrp((pid_t)_current->pgrp, SIGTTOU);
		td->td_retval[0] = -EINTR;
		return (EINTR);
	}

	buffer = kmalloc(nbyte + 1);
	if (!buffer)
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	memset(buffer, '\0', nbyte + 1);
	memcpy(buffer, vbuf, nbyte);

	if (t != NULL && t->t_type == TTY_TYPE_SERIAL)
	{
		size_t i;
		for (i = 0; i < nbyte; i++)
			rs232_putc(buffer[i]);
	}
	else if (t != NULL)
	{
		tty_print(buffer, t);
	}
	else
	{
		kprintf("%s", buffer);
	}
	kfree(buffer);
	td->td_retval[0] = nbyte;
	return (0);
}

/*
 * Console/TTY fileops vector.  read/close stay inline in vfs_calls.c for now
 * (read is the larger VT-switch state machine — path B step iii-read; close has
 * no tty symbols).  Installed into g_console_ops by tty_init(); g_console_ops is
 * NULL on architectures without a TTY layer (this file is not linked there),
 * where the generic core's console fall-through simply returns an error.
 */
static struct fileOps tty_ops = {
    .read = tty_fo_read,
    .write = tty_fo_write,
    .close = 0x0, /* close has no tty symbols — stays inline in vfs_calls.c */
};
/* g_console_ops itself is defined in sys/kern/descrip.c (generic, linked on every
 * arch); tty_init() installs &tty_ops into it.  On arches without a TTY layer it
 * stays NULL and the generic syscall core's tty fall-throughs error cleanly. */

int tty_init()
{
	int i = 0x0;

	g_console_ops = &tty_ops;
	g_tty_find = (void *(*)(u_int16_t))tty_find;
	g_tty_inject = (void (*)(void *, char))tty_inject;
	g_tty_signal = (void (*)(void *, int))signal_post_tty;
	g_console_stdin_ready = console_stdin_ready;

	/* Allocate memory for terminals */
	terms = (tty_term *)kmalloc(sizeof(tty_term) * TTY_MAX_TERMS);
	if (terms == 0x0)
		kpanic("tty_init: Failed to allocate memory. File: %s, Line: %i\n", __FILE__, __LINE__);

	/* Set up all default terminal information */
	for (i = 0; i < TTY_MAX_TERMS; i++)
	{
		terms[i].tty_buffer = (char *)kmalloc(TTY_MAX_COLS * TTY_MAX_ROWS * 2);
		if (terms[i].tty_buffer == 0x0)
			kpanic("tty_init: Failed to allocate buffer memory. File: %s, Line: %i\n", __FILE__, __LINE__);

		terms[i].tty_pointer = terms[i].tty_buffer;
		terms[i].tty_x = 0x0;
		terms[i].tty_y = 0x0;
		terms[i].t_cols = 80;
		terms[i].t_rows = 25;
		terms[i].tty_colour = 0x0A + i;
		terms[i].stdinSize = 0;
		terms[i].t_linelen = 0;
		terms[i].t_echo = 1;
		terms[i].t_raw = 0;
		terms[i].t_type = TTY_TYPE_VGA;
		terms[i].t_eof = 0;
		terms[i].t_stopped = 0;
		/* Fixed physical terminals are always live; pty pool starts free. */
		terms[i].t_inuse = (i < TTY_PTY_BASE) ? 1 : 0;

		/* Full termios + 80x25 window — FreeBSD sane defaults */
		tty_init_termios(&terms[i]);
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
	unsigned int cols = term->t_cols, rows = term->t_rows;
	unsigned int stride = cols * 2u;          /* bytes per row */
	unsigned int last = stride * (rows - 1u); /* byte offset of the last row */
	for (i = 0; i < last; i++)
		term->tty_pointer[i] = term->tty_pointer[i + stride];
	for (i = 0; i < cols; i++)
	{
		term->tty_pointer[last + (i * 2u)] = 0x20;
		term->tty_pointer[last + (i * 2u) + 1] = term->tty_colour;
	}
}

/* Execute a complete CSI sequence.  Returns updated bufferOffset. */
static unsigned int tty_csi_execute(tty_term *term, unsigned int bufferOffset, char cmd)
{
	unsigned int linear, row, col, i;
	unsigned int p0, p1;
	unsigned int cols = term->t_cols, rows = term->t_rows;
	unsigned int stride = cols * 2u; /* bytes per row */

	linear = bufferOffset / 2u;
	row = linear / cols;
	col = linear % cols;

	p0 = term->t_esc_params[0];
	p1 = term->t_esc_params[1];

	switch (cmd)
	{
		case 'A': /* cursor up */
			if (p0 == 0)
				p0 = 1;
			row = (row >= p0) ? row - p0 : 0;
			bufferOffset = (row * cols + col) * 2u;
			break;
		case 'B': /* cursor down */
			if (p0 == 0)
				p0 = 1;
			row += p0;
			if (row >= rows)
				row = rows - 1u;
			bufferOffset = (row * cols + col) * 2u;
			break;
		case 'C': /* cursor right */
			if (p0 == 0)
				p0 = 1;
			col += p0;
			if (col >= cols)
				col = cols - 1u;
			bufferOffset = (row * cols + col) * 2u;
			break;
		case 'D': /* cursor left */
			if (p0 == 0)
				p0 = 1;
			col = (col >= p0) ? col - p0 : 0;
			bufferOffset = (row * cols + col) * 2u;
			break;
		case 'G': /* cursor horizontal absolute (1-based col) */
			col = (p0 > 0u ? p0 - 1u : 0u);
			if (col >= cols)
				col = cols - 1u;
			bufferOffset = (row * cols + col) * 2u;
			break;
		case 'H':
		case 'f': /* cursor position: row;col (1-based) */
			row = (p0 > 0u ? p0 - 1u : 0u);
			col = (p1 > 0u ? p1 - 1u : 0u);
			if (row >= rows)
				row = rows - 1u;
			if (col >= cols)
				col = cols - 1u;
			bufferOffset = (row * cols + col) * 2u;
			break;
		case 'J': /* erase display */
			switch (p0)
			{
				case 0: /* cursor to end */
					for (i = bufferOffset; i < stride * rows; i += 2u)
					{
						term->tty_pointer[i] = ' ';
						term->tty_pointer[i + 1] = term->tty_colour;
					}
					break;
				case 1: /* start to cursor */
					for (i = 0; i <= bufferOffset && i + 1 < stride * rows; i += 2u)
					{
						term->tty_pointer[i] = ' ';
						term->tty_pointer[i + 1] = term->tty_colour;
					}
					break;
				case 2:
				case 3: /* whole screen */
					for (i = 0; i < stride * rows; i += 2u)
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
					for (i = col; i < cols; i++)
					{
						term->tty_pointer[row * stride + i * 2u] = ' ';
						term->tty_pointer[row * stride + i * 2u + 1] = term->tty_colour;
					}
					break;
				case 1: /* start of line to cursor */
					for (i = 0; i <= col; i++)
					{
						term->tty_pointer[row * stride + i * 2u] = ' ';
						term->tty_pointer[row * stride + i * 2u + 1] = term->tty_colour;
					}
					break;
				case 2: /* whole line */
					for (i = 0; i < cols; i++)
					{
						term->tty_pointer[row * stride + i * 2u] = ' ';
						term->tty_pointer[row * stride + i * 2u + 1] = term->tty_colour;
					}
					break;
			}
			break;
		case 'L':
		{ /* insert lines — shift rows at cursor and below down by p0 */
			unsigned int j;
			if (p0 == 0)
				p0 = 1;
			if (p0 > rows - row)
				p0 = rows - row;
			for (i = rows; i > row + p0; i--)
			{
				unsigned int dst = (i - 1) * stride;
				unsigned int src = (i - 1 - p0) * stride;
				for (j = 0; j < stride; j++)
					term->tty_pointer[dst + j] = term->tty_pointer[src + j];
			}
			for (i = row; i < row + p0; i++)
			{
				for (j = 0; j < cols; j++)
				{
					term->tty_pointer[i * stride + j * 2u] = ' ';
					term->tty_pointer[i * stride + j * 2u + 1] = term->tty_colour;
				}
			}
			break;
		}
		case 'M':
		{ /* delete lines — shift rows below cursor up by p0 */
			unsigned int j;
			if (p0 == 0)
				p0 = 1;
			if (p0 > rows - row)
				p0 = rows - row;
			for (i = row; i + p0 < rows; i++)
			{
				unsigned int dst = i * stride;
				unsigned int src = (i + p0) * stride;
				for (j = 0; j < stride; j++)
					term->tty_pointer[dst + j] = term->tty_pointer[src + j];
			}
			for (i = rows - p0; i < rows; i++)
			{
				for (j = 0; j < cols; j++)
				{
					term->tty_pointer[i * stride + j * 2u] = ' ';
					term->tty_pointer[i * stride + j * 2u + 1] = term->tty_colour;
				}
			}
			break;
		}
		case '@': /* insert characters — shift chars at cursor right by p0 */
			if (p0 == 0)
				p0 = 1;
			if (p0 > cols - col)
				p0 = cols - col;
			for (i = cols; i > col + p0; i--)
			{
				term->tty_pointer[row * stride + (i - 1) * 2u] =
				    term->tty_pointer[row * stride + (i - 1 - p0) * 2u];
				term->tty_pointer[row * stride + (i - 1) * 2u + 1] =
				    term->tty_pointer[row * stride + (i - 1 - p0) * 2u + 1];
			}
			for (i = col; i < col + p0; i++)
			{
				term->tty_pointer[row * stride + i * 2u] = ' ';
				term->tty_pointer[row * stride + i * 2u + 1] = term->tty_colour;
			}
			break;
		case 'P': /* delete characters (shift left) */
			if (p0 == 0)
				p0 = 1;
			for (i = col; i + p0 < cols; i++)
			{
				term->tty_pointer[row * stride + i * 2u] =
				    term->tty_pointer[row * stride + (i + p0) * 2u];
				term->tty_pointer[row * stride + i * 2u + 1] =
				    term->tty_pointer[row * stride + (i + p0) * 2u + 1];
			}
			for (i = cols - p0; i < cols; i++)
			{
				term->tty_pointer[row * stride + i * 2u] = ' ';
				term->tty_pointer[row * stride + i * 2u + 1] = term->tty_colour;
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
						term->tty_colour =
						    (u_int8_t)((fg << 4) | bg | (term->tty_colour & 0x88u));
						break;
					}
					case 27:
						break; /* reverse off — no tracking */
					case 39:       /* default fg */
						term->tty_colour = (u_int8_t)((term->tty_colour & 0xF0u) |
						                              (term->t_default_colour & 0x0Fu));
						break;
					case 49: /* default bg */
						term->tty_colour = (u_int8_t)((term->tty_colour & 0x0Fu) |
						                              (term->t_default_colour & 0xF0u));
						break;
					default:
						if (v >= 30u && v <= 37u)
							term->tty_colour = (u_int8_t)((term->tty_colour & 0xF8u) |
							                              ansi_to_vga[v - 30u]);
						else if (v >= 90u && v <= 97u)
							term->tty_colour = (u_int8_t)((term->tty_colour & 0xF0u) |
							                              (ansi_to_vga[v - 90u] | 8u));
						else if (v >= 40u && v <= 47u)
							term->tty_colour =
							    (u_int8_t)((term->tty_colour & 0x8Fu) |
							               (u_int8_t)(ansi_to_vga[v - 40u] << 4u));
						else if (v >= 100u && v <= 107u)
							term->tty_colour =
							    (u_int8_t)((term->tty_colour & 0x0Fu) |
							               (u_int8_t)((ansi_to_vga[v - 100u] | 8u) << 4u));
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
	unsigned int cols = term->t_cols, rows = term->t_rows;
	unsigned int stride = cols * 2u; /* bytes per row */

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
					unsigned int row = (bufferOffset / 2u) / cols;
					if (row > 0u)
					{
						bufferOffset -= stride;
					}
					else
					{
						unsigned int j;
						for (j = stride * (rows - 1u); j >= stride; j--)
							term->tty_pointer[j] = term->tty_pointer[j - stride];
						for (j = 0; j < cols; j++)
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
				term->t_esc_params[term->t_esc_nparams - 1] =
				    (u_int16_t)(term->t_esc_params[term->t_esc_nparams - 1] * 10u +
				                (unsigned int)(character - '0'));
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
			if (bufferOffset < stride * rows)
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
				bufferOffset = (bufferOffset / stride) * stride;
				break;
			case '\n':
				/* Phase 9: OPOST+ONLCR → col 0 of next row; OPOST only → same col, next row */
				if (term->t_termios.c_oflag & OPOST)
				{
					if (term->t_termios.c_oflag & ONLCR)
						bufferOffset = (bufferOffset / stride) * stride + stride; /* CR+LF */
					else
						bufferOffset += stride; /* LF only — stay in same column */
				}
				break;
			case '\b': /* move cursor left without erasing */
				if (bufferOffset >= 2u)
					bufferOffset -= 2u;
				break;
			case '\t':
			{ /* advance to next 8-column tab stop */
				unsigned int col = (bufferOffset / 2u) % cols;
				unsigned int spaces = 8u - (col % 8u);
				while (spaces-- > 0 && (bufferOffset / 2u) % cols < cols - 1u)
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
		if (bufferOffset >= stride * rows)
		{
			tty_scroll(term);
			bufferOffset -= stride;
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
					else if (tty->t_type == TTY_TYPE_PTY)
					{
						/* Erase into the pty cell grid; backSpace()
						 * would poke the VGA hardware cursor. */
						tty_print("\b \b", tty);
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
					else if (tty->t_type == TTY_TYPE_PTY)
					{
						/* Erase into the pty cell grid; backSpace()
						 * would poke the VGA hardware cursor. */
						tty_print("\b \b", tty);
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

/*
 * pty_alloc — hand out a free pseudo-terminal slot from the pty pool.
 *
 * The slot renders into its own tty_buffer (never the VGA framebuffer) and is
 * reset to a blank 80x25 grid in canonical mode, so a slot reused after tcsh
 * left it in raw mode comes back clean.
 *
 * @return slot index, or -1 if the pool is exhausted.
 */
int pty_alloc(void)
{
	int slot;

	for (slot = TTY_PTY_BASE; slot < TTY_MAX_TERMS; slot++)
	{
		tty_term *t = &terms[slot];
		int i;

		if (t->t_inuse)
			continue;

		t->t_inuse = 1;
		t->t_type = TTY_TYPE_PTY;
		t->tty_pointer = t->tty_buffer;
		t->tty_x = 0;
		t->tty_y = 0;
		t->tty_colour = 0x07;
		t->stdinSize = 0;
		t->t_linelen = 0;
		t->t_echo = 1;
		t->t_raw = 0;
		t->t_eof = 0;
		t->t_stopped = 0;
		t->t_exclusive = 0;
		t->t_pgrp = 0;
		/* Record the allocating process as the pty master "owner" (the graphical
		 * terminal app).  When that process dies, tty_hangup_by_owner() hangs up
		 * the slave session so an orphaned shell does not leak. */
		t->owner = (_current != NULL) ? (pidType)_current->id : 0;
		t->t_esc_state = 0;
		t->t_esc_priv = 0;
		t->t_esc_nparams = 0;
		t->t_default_colour = 0x07;
		t->t_saved_x = 0;
		t->t_saved_y = 0;
		t->t_saved_colour = 0x07;
		memset(t->t_esc_params, 0, sizeof(t->t_esc_params));
		tty_init_termios(t);

		/* Blank the cell grid: space glyph + default attribute. */
		for (i = 0; i < (int)t->t_cols * t->t_rows; i++)
		{
			t->tty_buffer[i * 2] = ' ';
			t->tty_buffer[i * 2 + 1] = (char)0x07;
		}
		return (slot);
	}
	return (-1);
}

/*
 * pty_free — return a pseudo-terminal slot to the pool.  Ignored for fixed
 * physical terminals (slot < TTY_PTY_BASE).
 */
void pty_free(int slot)
{
	if (slot < TTY_PTY_BASE || slot >= TTY_MAX_TERMS)
		return;
	terms[slot].t_inuse = 0;
	terms[slot].t_pgrp = 0;
	terms[slot].owner = 0;
}

/**
 * Hang up every pty whose master owner is the given (dying) process.
 *
 * Implements the POSIX "controlling-process death" semantics: when the process
 * that allocated a pty master (the graphical terminal app) exits, the slave's
 * session is sent SIGHUP and the slot is released.  Without this, a shell that
 * called setsid() for its pty — putting it in its own session and process group
 * — escapes the parent's process-group/session teardown on logout and leaks
 * (the shell keeps running, holding its pages, across the next login).
 *
 * Called from endTask() while the dying task is still _current.
 */
void tty_hangup_by_owner(pidType pid)
{
	int slot;

	for (slot = TTY_PTY_BASE; slot < TTY_MAX_TERMS; slot++)
	{
		tty_term *t = &terms[slot];

		if (!t->t_inuse || t->owner != pid)
			continue;

		/* Post SIGHUP first (signal_post_tty's fallback finds the slave via its
		 * ct_tty pointer), then free the slot — the master is gone. */
		signal_post_tty(t, SIGHUP);
		pty_free(slot);
	}
}

/*
 * tty_inject_user — feed a userland keystroke buffer through a pty's line
 * discipline (the graphical terminal's keyboard → shell stdin path).
 *
 * @return bytes injected, or -1 for an invalid/unallocated slot.
 */
int tty_inject_user(int slot, const char *buf, int n)
{
	int i;

	if (slot < TTY_PTY_BASE || slot >= TTY_MAX_TERMS || !terms[slot].t_inuse)
		return (-1);
	if (buf == NULL || n < 0)
		return (-1);
	for (i = 0; i < n; i++)
		tty_inject(&terms[slot], buf[i]);
	return (n);
}

/*
 * tty_snapshot — copy a pty's rendered 80x25 cell grid and cursor out to
 * userland for the graphical terminal to draw.
 *
 * @param dst  destination for 80*25*2 bytes of char+attribute cells.
 * @return 0 on success, -1 for an invalid/unallocated slot.
 */
int tty_snapshot(int slot, void *dst, u_int16_t *x, u_int16_t *y)
{
	if (slot < TTY_PTY_BASE || slot >= TTY_MAX_TERMS || !terms[slot].t_inuse)
		return (-1);
	if (dst == NULL)
		return (-1);
	memcpy(dst, terms[slot].tty_buffer, (size_t)terms[slot].t_cols * terms[slot].t_rows * 2);
	if (x != NULL)
		*x = terms[slot].tty_x;
	if (y != NULL)
		*y = terms[slot].tty_y;
	return (0);
}

/*
 * tty_resize — set a pseudo-terminal's grid to cols x rows (clamped to the
 * TTY_MAX_* bounds), update its winsize for TIOCGWINSZ, blank the grid, and home
 * the cursor.  The caller (the terminal app) is expected to SIGWINCH its shell
 * so it re-queries the size and redraws.  @return 0 on success, -1 on error.
 */
int tty_resize(int slot, int cols, int rows)
{
	tty_term *t;
	int i, n, r, c, oc, orow, mc, mr;
	unsigned int linear, crow, ccol;

	if (slot < TTY_PTY_BASE || slot >= TTY_MAX_TERMS || !terms[slot].t_inuse)
		return (-1);
	t = &terms[slot];

	if (cols < 8)
		cols = 8;
	if (cols > TTY_MAX_COLS)
		cols = TTY_MAX_COLS;
	if (rows < 2)
		rows = 2;
	if (rows > TTY_MAX_ROWS)
		rows = TTY_MAX_ROWS;

	oc = t->t_cols;   /* old grid width  */
	orow = t->t_rows; /* old grid height */

	/* Save the old content (stride changes, so we can't reflow in place). */
	memcpy(g_tty_resize_tmp, t->tty_buffer, (size_t)oc * orow * 2);

	t->t_cols = (u_int16_t)cols;
	t->t_rows = (u_int16_t)rows;
	t->t_winsize.ws_col = (u_int16_t)cols;
	t->t_winsize.ws_row = (u_int16_t)rows;

	/* Blank the new grid, then re-lay the old cells top-left up to the overlap
	 * (preserves the visible prompt/output; wrapped lines keep their old wrap). */
	n = cols * rows;
	for (i = 0; i < n; i++)
	{
		t->tty_buffer[i * 2] = ' ';
		t->tty_buffer[i * 2 + 1] = (char)0x07;
	}
	mc = (cols < oc) ? cols : oc;
	mr = (rows < orow) ? rows : orow;
	for (r = 0; r < mr; r++)
		for (c = 0; c < mc; c++)
		{
			t->tty_buffer[(r * cols + c) * 2] = g_tty_resize_tmp[(r * oc + c) * 2];
			t->tty_buffer[(r * cols + c) * 2 + 1] = g_tty_resize_tmp[(r * oc + c) * 2 + 1];
		}

	/* Re-base the cursor (a split linear index) onto the new width. */
	linear = ((unsigned int)t->tty_y << 8) | t->tty_x;
	crow = linear / (unsigned int)oc;
	ccol = linear % (unsigned int)oc;
	if (crow >= (unsigned int)rows)
		crow = (unsigned int)rows - 1u;
	if (ccol >= (unsigned int)cols)
		ccol = (unsigned int)cols - 1u;
	linear = crow * (unsigned int)cols + ccol;
	t->tty_x = (u_int16_t)(linear & 0xFFu);
	t->tty_y = (u_int16_t)(linear >> 8u);
	return (0);
}
