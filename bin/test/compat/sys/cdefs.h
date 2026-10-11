/*
 * Minimal <sys/cdefs.h> for building FreeBSD sources against musl.
 * (C) 2002-2026 The UbixOS Project
 */
#ifndef _COMPAT_SYS_CDEFS_H_
#define _COMPAT_SYS_CDEFS_H_

#define __FBSDID(x)
#define __dead2 __attribute__((__noreturn__))
#define __printf0like(a, b) __attribute__((__format__(__printf__, a, b)))

#endif
