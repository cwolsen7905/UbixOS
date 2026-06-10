/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * Freestanding <stdint.h> shim for compiling the portable UbixFS core
 * (lib/ubixfs_core, written for a hosted environment) inside the kernel, which
 * builds with -nostdinc.  The core only needs the fixed-width integer types.
 *
 * <sys/types.h> already provides the signed C99 names (int8_t..int64_t) and the
 * BSD unsigned names (u_int8_t..u_int64_t); here we add only the C99 unsigned
 * spellings (uintN_t), mapped onto the BSD ones so a "uint64_t" in the core is
 * the exact same type as a kernel "u_int64_t".
 */
#ifndef _UBIXFS_COMPAT_STDINT_H
#define _UBIXFS_COMPAT_STDINT_H

#include <sys/types.h>

typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
typedef u_int64_t uint64_t;

#endif /* _UBIXFS_COMPAT_STDINT_H */
