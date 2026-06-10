/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * Freestanding <stddef.h> shim for compiling the portable UbixFS core in the
 * kernel (built with -nostdinc).  size_t / ptrdiff_t come from <sys/types.h>;
 * here we add only NULL.
 */
#ifndef _UBIXFS_COMPAT_STDDEF_H
#define _UBIXFS_COMPAT_STDDEF_H

#include <sys/types.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _UBIXFS_COMPAT_STDDEF_H */
