#ifndef _API_UBIX_H
#define _API_UBIX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

	int ubix_test(void);
	char *ubix_getcwd(char *buf, size_t size);

	/* TTY line discipline control */
	int tty_setraw(int val);  /* 1 = raw mode, 0 = canonical (default) */
	int tty_setecho(int val); /* 1 = echo on (default), 0 = echo off */

	/* Claim the serial TTY as this process's controlling terminal (slot 1 = COM1).
	 * Analogous to FreeBSD login_tty() — must be called by ttyd before fork. */
	int settty(int slot);

	/* Returns 1 if the views compositor is running, 0 if not.
	 * Call before DISPLAY_CLAIM to give a clean error instead of hanging. */
	int views_running(void);

	/* Pseudo-terminal pool — back a graphical terminal with a real kernel tty so
	 * an interactive shell (e.g. tcsh) runs exactly as on the text console.
	 * pty_alloc() returns a slot; open "/dev/ttyv<slot>" as the child's fd 0/1/2.
	 * pty_inject() feeds keystrokes; pty_snapshot() reads the 80x25 cell grid. */
	int pty_alloc(void);
	int pty_free(int slot);
	int pty_inject(int slot, const char *buf, int n);
	int pty_snapshot(int slot, void *dst, unsigned short *x, unsigned short *y);

#ifdef __cplusplus
}
#endif

#endif /* _API_UBIX_H */
