/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * Freestanding <stdlib.h> shim for compiling the portable UbixFS core in the
 * kernel (built with -nostdinc).  The core only uses malloc()/free(); they are
 * declared here and glued to kmalloc()/kfree() in ubfs_kshim.c, so the core
 * compiles unmodified and allocates from the kernel heap.
 */
#ifndef _UBIXFS_COMPAT_STDLIB_H
#define _UBIXFS_COMPAT_STDLIB_H

#include <sys/types.h>

void *malloc(size_t len);
void free(void *ptr);

#endif /* _UBIXFS_COMPAT_STDLIB_H */
