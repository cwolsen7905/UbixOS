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

#ifndef _SYS__TYPES_H
#define _SYS__TYPES_H

typedef char __int8_t;
typedef unsigned char __uint8_t;
typedef short __int16_t;
typedef unsigned short __uint16_t;
typedef int __int32_t;
typedef unsigned int __uint32_t;
typedef long long __int64_t;
typedef unsigned long long __uint64_t;

typedef unsigned long __clock_t;
typedef __uint32_t           __ino_t;
 typedef __int32_t            __ssize_t;/* stat types */
typedef __uint32_t           __dev_t;/* device number */
typedef __uint16_t           __mode_t;
 typedef __uint16_t           __nlink_t;/* link count */
typedef __uint32_t           __uid_t;
 typedef __uint32_t           __gid_t;
 typedef __int32_t            __time_t;
 typedef __int64_t            __blkcnt_t;/* file block count */
typedef __uint32_t           __blksize_t;/* file block size */
typedef __uint32_t           __fflags_t;/* file flags */
typedef __int8_t        __int_fast8_t;
 typedef __uint8_t       __uint_fast8_t;
 typedef __int16_t        __int_fast16_t;
 typedef __uint16_t       __uint_fast16_t;
 typedef __int32_t        __int_fast32_t;
 typedef __uint32_t       __uint_fast32_t;
 typedef __int64_t        __int_fast64_t;
 typedef __uint64_t       __uint_fast64_t;
 typedef __int32_t       __intptr_t;
 typedef __uint32_t       __uintptr_t;
typedef       __uint32_t      __uintfptr_t;
 typedef __uint32_t       __size_t;
 typedef __int64_t        __intmax_t;
 typedef __uint64_t        __uintmax_t;
typedef __int32_t        __ptrdiff_t;
 typedef __uint8_t        __uint_least8_t;
 typedef __uint16_t       __uint_least16_t;
 typedef __uint32_t       __uint_least32_t;
 typedef __uint64_t       __uint_least64_t;
 typedef __int8_t        __int_least8_t;
 typedef __int16_t       __int_least16_t;
 typedef __int32_t       __int_least32_t;
 typedef __int64_t       __int_least64_t;
 typedef int ___wchar_t;

typedef       long            __suseconds_t;  /* microseconds (signed) */
typedef __int32_t __pid_t;  /* process [group] */

#if !defined(__clang__) || !defined(__cplusplus)
typedef __uint_least16_t  __char16_t;
 typedef __uint_least32_t  __char32_t;
#endif
/*
 * mbstate_t is an opaque object to keep conversion state during multibyte
 * stream conversions.
 */
typedef
union {
    char __mbstate8[128];
    __int64_t _mbstateL; /* for alignment */
} __mbstate_t;


#endif /* END _SYS__TYPES_H */
