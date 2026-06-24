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

#ifndef _UBIXOS_TTY_H
#define _UBIXOS_TTY_H

#include <sys/types.h>
#include <sys/ioctl.h>

/*
 * Slots 0-4 are the fixed physical terminals (ttyv0-3 + com1).  Slots
 * TTY_PTY_BASE..TTY_MAX_TERMS-1 form the pseudo-terminal pool handed out by
 * pty_alloc() for graphical terminal apps (e.g. bin/views/term).
 */
#define TTY_PTY_BASE 5
#define TTY_MAX_TERMS 13

/* Upper bound on a (pseudo-)terminal's cell grid; the cell buffer is sized for
 * this so a pty can be resized up to it without reallocating. */
#define TTY_MAX_COLS 200
#define TTY_MAX_ROWS 64

/* tty_setmode cmd values */
#define TTY_SETRAW 0  /* val 1 = raw, 0 = canonical */
#define TTY_SETECHO 1 /* val 1 = echo on, 0 = echo off */

/* t_type: controls how echo is delivered */
#define TTY_TYPE_VGA 0    /* VGA text console — echo via tty_print / backSpace() */
#define TTY_TYPE_SERIAL 1 /* COM1 serial — echo via rs232_putc() */
#define TTY_TYPE_PTY 2    /* pseudo-terminal — echo into tty_buffer via tty_print() */

typedef struct tty_termNode
{
	char *tty_buffer;
	char *tty_pointer;
	u_int8_t tty_colour;
	u_int16_t tty_x;
	u_int16_t tty_y;
	pidType owner;
	char stdin[512];
	int stdinSize;
	/* Line discipline */
	char t_linebuf[512];      /* canonical input buffer (getchar fills until Enter) */
	int t_linelen;            /* chars currently in t_linebuf */
	u_int8_t t_echo;          /* 1 = echo input to terminal (default) */
	u_int8_t t_raw;           /* 1 = raw mode: bypass line discipline */
	u_int8_t t_type;          /* TTY_TYPE_VGA or TTY_TYPE_SERIAL */
	u_int8_t t_eof;           /* 1 = VEOF delivered; next read with stdinSize==0 returns 0 */
	u_int8_t t_stopped;       /* 1 = IXON Ctrl-S received; output suspended */
	u_int8_t t_exclusive;     /* 1 = TIOCEXCL set; no further opens allowed */
	u_int8_t t_inuse;         /* pty pool: 1 = slot allocated (fixed slots always 1) */
	struct termios t_termios; /* full termios state for TIOCGETA/TIOCSETA */
	struct winsize t_winsize; /* window size for TIOCGWINSZ/TIOCSWINSZ */
	pid_t t_pgrp;             /* foreground process group (TIOCGPGRP/TIOCSPGRP) */
	u_int16_t t_cols;         /* active grid width  (cells); VGA consoles = 80 */
	u_int16_t t_rows;         /* active grid height (cells); VGA consoles = 25 */
	/* ANSI/VT100 escape sequence parser */
	u_int8_t t_esc_state;      /* 0=normal 1=ESC_seen 2=CSI_collecting */
	u_int8_t t_esc_priv;       /* 1 if '?' seen after CSI '[' */
	u_int8_t t_esc_nparams;    /* number of CSI params collected so far */
	u_int8_t t_default_colour; /* colour at init time — SGR 0 resets to this */
	u_int16_t t_esc_params[8]; /* CSI numeric parameters (';'-separated) */
	u_int16_t t_saved_x;       /* cursor save/restore (same encoding as tty_x) */
	u_int16_t t_saved_y;
	u_int8_t t_saved_colour;
	/* Raw pty master (posix_openpt): when a master fd is attached, the slave's
	 * output bytes are streamed verbatim (OPOST/ONLCR applied) into t_outbuf for
	 * the master to read() instead of being parsed into the VT100 cell grid — the
	 * remote SSH client renders.  Allocated by pty_open_master(), freed on the
	 * master's close. */
	char *t_outbuf;           /* raw master output ring (NULL = no master attached) */
	int t_outhead;            /* producer index (slave writes / echo) */
	int t_outtail;            /* consumer index (master read) */
	u_int8_t t_has_master;    /* 1 = a raw master fd owns this slot's output */
	int t_pts_num;            /* pts unit number (ptsname → /dev/pts/<n>) */
	int t_master_refs;        /* open master fds for this slot (fork dups share it) */
} tty_term;

#define TTY_OUTBUF_SIZE 8192 /* raw master output ring capacity (power of two) */

int tty_init();
tty_term *tty_find(u_int16_t);
int tty_print(char *, tty_term *);
void tty_inject(tty_term *tty, char ch); /* push one char through line discipline */

/*
 * Pseudo-terminal pool — backs graphical terminal apps.  pty_alloc() hands out
 * a free pool slot whose screen lives in its tty_buffer (never VGA); the app
 * feeds keystrokes with tty_inject_user() and reads the rendered cell grid back
 * with tty_snapshot().
 */
int pty_alloc(void);
void pty_free(int slot);

/*
 * Raw pty master (FreeBSD posix_openpt model).  pty_open_master() allocates a
 * pool slot, attaches a raw output ring, and returns the slot; the caller wraps
 * it in a master fd (FD_TYPE_PTMASTER).  The slave is /dev/pts/<slot's pts_num>.
 */
int pty_open_master(void);            /* alloc slot + ring; returns slot, -1 if full */
void pty_close_master(int slot);      /* drop a master ref; SIGHUP + free at the last */
void pty_master_fork_ref(int slot);   /* a fork dup'd a master fd — bump the ref */
int ptm_read(int slot, void *dst, int n, int nonblock); /* drain raw output; -EAGAIN if empty+nonblock */
int ptm_write(int slot, const void *src, int n); /* feed slave input via line discipline */
int ptm_output_pending(int slot);     /* bytes available for the master to read (select) */
int pty_slot_for_pts(int pts_num);    /* /dev/pts/<n> → pool slot, or -1 */
void tty_hangup_by_owner(pidType pid); /* SIGHUP + release ptys owned by a dying process */
int tty_inject_user(int slot, const char *buf, int n);
int tty_snapshot(int slot, void *dst, u_int16_t *x, u_int16_t *y);
int tty_resize(int slot, int cols, int rows); /* set a pty's grid size + winsize */

extern tty_term *tty_foreground;

#endif
