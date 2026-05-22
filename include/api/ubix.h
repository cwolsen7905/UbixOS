#ifndef _API_UBIX_H
#define _API_UBIX_H

#include <stddef.h>

int ubix_test(void);
char *ubix_getcwd(char *buf, size_t size);

/* TTY line discipline control */
int tty_setraw(int val);   /* 1 = raw mode, 0 = canonical (default) */
int tty_setecho(int val);  /* 1 = echo on (default), 0 = echo off */

/* Claim the serial TTY as this process's controlling terminal (slot 1 = COM1).
 * Analogous to FreeBSD login_tty() — must be called by ttyd before fork. */
int settty(int slot);

/* Returns 1 if the views compositor is running, 0 if not.
 * Call before DISPLAY_CLAIM to give a clean error instead of hanging. */
int views_running(void);

#endif /* _API_UBIX_H */
