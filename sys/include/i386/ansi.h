/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * i386 (IA-32) pointer-width / size types.  These are the only arithmetic
 * types whose size differs by architecture; everything else lives in
 * sys/_types.h.  Reached via <machine/ansi.h>.
 */
#ifndef _I386_ANSI_H
#define _I386_ANSI_H

typedef int          __intptr_t;
typedef unsigned int __uintptr_t;
typedef unsigned int __uintfptr_t;
typedef unsigned int __size_t;
typedef int          __ptrdiff_t;

#endif /* _I386_ANSI_H */
