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

#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include <sys/_null.h>
#include <sys/_types.h>
#include <sys/select.h>

typedef __uintfptr_t uintfptr_t;

typedef char *caddr_t;

/* Unsigned integral types — BSD canonical names */
typedef __uint8_t  u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;
typedef __uint64_t u_quad_t;
typedef __int64_t  quad_t;

typedef __int64_t  daddr_t;    /* disk address */
typedef u_int32_t  u_daddr_t;  /* unsigned disk address */

/* BSD traditional unsigned types */
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;

typedef int pidType;
/*
 * register_t is the natural machine word (FreeBSD semantics): 32-bit on the
 * i386 ILP32 target, 64-bit on the aarch64 LP64 target.  `long` is exactly
 * this width on both (4 bytes ILP32, 8 bytes LP64), so i386 is byte-identical.
 * The syscall argument structs (PADL_/PADR_ padding) and the table dispatcher
 * (ksyscall_dispatch) size their arguments in units of register_t and index
 * the saved trapframe register slots as register_t words — so this MUST match
 * the width of one trapframe register, or every table-dispatched syscall reads
 * its second and later arguments from the wrong half of a register.
 */
typedef long register_t;

typedef int pid_t;
typedef int size_t; /* standard */

#ifndef NOBOOL
#ifndef __cplusplus
/* C23 (GCC 16+) makes bool a keyword — guard the typedef. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#  define FALSE 0
#  define TRUE  1
#else
typedef enum {FALSE=0,TRUE=1}bool;
#endif
#endif
#else
#ifndef __cplusplus
#define FALSE 0
#define TRUE  1
/* bool is a keyword in C23; skip the typedef there too. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
typedef int bool;
#endif
#endif
#endif

#ifndef _INO_T_DECLARED
typedef __ino_t ino_t; /* inode number */
#define _INO_T_DECLARED
#endif

#ifndef _INT8_T_DECLARED
typedef __int8_t int8_t;
#define _INT8_T_DECLARED
#endif

#ifndef _INT16_T_DECLARED
typedef __int16_t int16_t;
#define _INT16_T_DECLARED
#endif

#ifndef _INT32_T_DECLARED
typedef __int32_t int32_t;
#define _INT32_T_DECLARED
#endif

#ifndef _INT64_T_DECLARED
typedef __int64_t int64_t;
#define _INT64_T_DECLARED
#endif

typedef __ssize_t ssize_t;
typedef __int64_t off_t;
typedef __uint32_t vm_offset_t;

typedef __uid_t uid_t; /* user id */
typedef __gid_t gid_t; /* group id */
typedef __blkcnt_t blkcnt_t;
typedef __blksize_t blksize_t;
typedef __fflags_t fflags_t;

#ifndef _TIME_T_DECLARED
typedef __time_t time_t;
#define _TIME_T_DECLARED
#endif

/* uintmax_t / intmax_t are the widest integer types — 64-bit, not 32-bit.  The
 * old u_int32_t/int32_t typedefs truncated kvprintf's `num` (declared uintmax_t)
 * so every %lx/%jx printed only the low 32 bits of a 64-bit value (e.g. an
 * 0x2_00000000 user VA showed as 0x0) — a kernel-wide debug-output bug. */
typedef __uint64_t uintmax_t;
typedef __int64_t intmax_t;
/* The compiler's pointer-difference type — 64-bit on LP64 (aarch64/x86_64),
 * 32-bit on ILP32 (i386).  Was hard-coded int32_t, which truncated any 64-bit
 * pointer cast through ptrdiff_t (e.g. lwIP's LWIP_CONST_CAST): harmless while
 * the kernel ran at low VAs that fit in 32 bits, but a high-half kernel VA
 * (0xFFFFFF80_........) lost its top half. */
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __uintptr_t uintptr_t;

#define __ULONG_MAX     0xffffffffUL
#define __USHRT_MAX     0xffff          /* max value for an unsigned short */

#define       ULONG_MAX       __ULONG_MAX
#define       USHRT_MAX       __USHRT_MAX

/* select a type for digits in base B: use unsigned short if they fit */
#if ULONG_MAX == 0xffffffff && USHRT_MAX >= 0xffff
typedef unsigned short digit;
#else
typedef u_long digit;
#endif

#define _QUAD_HIGHWORD 1
#define _QUAD_LOWWORD 0

#define H _QUAD_HIGHWORD
#define L _QUAD_LOWWORD

#define __CHAR_BIT 8
#define CHAR_BIT        __CHAR_BIT

#define QUAD_BITS       (sizeof(quad_t) * CHAR_BIT)
#define LONG_BITS       (sizeof(long) * CHAR_BIT)
#define HALF_BITS       (sizeof(long) * CHAR_BIT / 2)
#define HHALF(x)        ((x) >> HALF_BITS)
#define LHALF(x)        ((x) & ((1 << HALF_BITS) - 1))
#define LHUP(x)         ((x) << HALF_BITS)

typedef unsigned int qshift_t;

#define B       (1 << HALF_BITS)        /* digit base */

/* Combine two `digits' to make a single two-digit number. */
#define COMBINE(a, b) (((u_long)(a) << HALF_BITS) | (b))

#ifndef _MODE_T_DECLARED
typedef __mode_t mode_t; /* permissions */
#define _MODE_T_DECLARED
#endif

#endif /* END _SYS_TYPES_H */
