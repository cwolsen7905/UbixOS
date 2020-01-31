/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

#include <sys/_types.h>
#include <sys/select.h>

#ifndef NULL
#define NULL 0
#endif

typedef __uintfptr_t uintfptr_t;

typedef char *caddr_t;

/* unsigned integrals */
typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;
typedef __uint64_t uquad_t;

/* unsigned integrals (deprecated) */
typedef __uint8_t u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;
typedef __uint64_t u_quad_t;

typedef __int64_t quad_t;

typedef __int64_t daddr_t; /* disk address */
typedef __uint32_t u_daddr_t; /* unsigned disk address */

typedef unsigned char uInt8;
typedef unsigned short uInt16;
typedef unsigned int uInt32;
typedef unsigned int uInt;
typedef char Int8;
typedef short Int16;
typedef long Int32;

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef int pidType;

typedef int pid_t;
typedef int size_t; /* standard */

#ifndef NOBOOL
#ifndef __cplusplus
typedef enum {FALSE=0,TRUE=1}bool;
#endif
#else
#ifndef __cplusplus
#define FALSE 0
#define TRUE  1
typedef int bool;
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
typedef char *caddr_t;
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

typedef uint32_t uintmax_t;
typedef int32_t intmax_t;
typedef int32_t ptrdiff_t;
typedef uint32_t uintptr_t;

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
