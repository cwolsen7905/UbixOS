/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 (LP64) pointer-width / size types.  Sibling of i386/ansi.h (ILP32) and
 * aarch64/ansi.h; reached via <machine/ansi.h>.  Getting these right is load-
 * bearing: with the 32-bit i386 definitions, every (uintptr_t) pointer cast in
 * machine-independent code (kmalloc coalescing, the VMM, the ELF loader) silently
 * truncates a 64-bit address to 32 bits.
 */
#ifndef _X86_64_ANSI_H
#define _X86_64_ANSI_H

typedef long          __intptr_t;
typedef unsigned long __uintptr_t;
typedef unsigned long __uintfptr_t;
typedef unsigned long __size_t;
typedef long          __ptrdiff_t;

#endif /* _X86_64_ANSI_H */
