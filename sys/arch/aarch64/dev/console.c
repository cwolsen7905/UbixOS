/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 console tty layer: wire the PL011 UART into the VFS console fileops so
 * a userland process's read()/write() on fds 0/1/2 reach the serial console.
 * Provides a cooked-ish line discipline on read (echo, CRLF, backspace; returns
 * a line on Enter) — enough for an interactive prompt (a login/shell).  Installs
 * the path-B console hooks (g_console_ops + friends) that are NULL by default on
 * an arch without the full i386 tty.c.
 */
#include "bringup.h"
#include <sys/descrip.h>
#include <ubixos/sched.h> /* sched_yield */

/**
 * Write @nbyte bytes to the console (PL011).  '\n' is mapped to CRLF.
 */
static int console_fo_write(struct file *fp, struct thread *td, const void *buf, size_t nbyte)
{
	const char *p = (const char *)buf;
	size_t i;

	(void)fp;
	for (i = 0; i < nbyte; i++)
	{
		if (p[i] == '\n')
			uart_putc('\r');
		uart_putc(p[i]);
	}
	td->td_retval[0] = (int)nbyte;
	return (0);
}

/**
 * Cooked line read: block (yielding) until a full line arrives, echoing input
 * and honouring Enter (CR/LF) + backspace.  Returns up to @nbyte bytes incl. the
 * terminating '\n'.
 */
static int console_fo_read(struct file *fp, struct thread *td, void *buf, size_t nbyte)
{
	char *out = (char *)buf;
	size_t n = 0;

	(void)fp;
	if (nbyte == 0)
	{
		td->td_retval[0] = 0;
		return (0);
	}

	for (;;)
	{
		int c = uart_getc();
		if (c < 0)
		{
			sched_yield();
			continue;
		}
		if (c == '\r')
			c = '\n';
		if (c == 0x7f || c == 0x08) /* DEL / Backspace */
		{
			if (n > 0)
			{
				n--;
				uart_putc('\b');
				uart_putc(' ');
				uart_putc('\b');
			}
			continue;
		}
		if (c == '\n')
		{
			uart_putc('\r');
			uart_putc('\n');
			out[n++] = '\n';
			break;
		}
		uart_putc((char)c); /* echo */
		out[n++] = (char)c;
		if (n >= nbyte)
			break;
	}
	td->td_retval[0] = (int)n;
	return (0);
}

static struct fileOps console_ops = {
    .read = console_fo_read,
    .write = console_fo_write,
    .close = 0x0,
};

/**
 * g_tty_print hook: emit a NUL-terminated string to the console.
 */
static void console_print(const char *s, void *term)
{
	(void)term;
	uart_puts(s);
}

/**
 * g_tty_getchar hook: block until one character is available, return it.
 */
static int console_getchar_hook(void)
{
	int c;
	while ((c = uart_getc()) < 0)
		sched_yield();
	return (c);
}

/**
 * g_console_stdin_ready hook: non-blocking — non-zero if input is waiting.
 */
static int console_stdin_ready(void)
{
	return (uart_rx_ready());
}

/**
 * Install the PL011-backed console into the VFS console hooks.  After this a
 * process whose fds 0/1/2 carry g_console_ops can read()/write() the console.
 */
void aarch64_console_init(void)
{
	g_console_ops = &console_ops;
	g_tty_print = console_print;
	g_tty_getchar = console_getchar_hook;
	g_console_stdin_ready = console_stdin_ready;
}

/**
 * Set up @t's fds 0/1/2 as console placeholder descriptors (FD_TYPE_TTY, no
 * backing fileDescriptor_t, dispatched through g_console_ops).  Call before a
 * task runs so its stdin/stdout/stderr reach the console.
 *
 * @return 0 on success, -1 if a descriptor couldn't be allocated.
 */
int aarch64_console_setup_fds(struct thread *td)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		struct file *fp = 0x0;
		int fd = -1;
		if (falloc(td, &fp, &fd) != 0 || fp == 0x0)
			return (-1);
		fp->fd = 0x0; /* TTY placeholder: no underlying VFS descriptor */
		fp->fd_type = FD_TYPE_TTY;
		fp->f_ops = g_console_ops;
	}
	return (0);
}
