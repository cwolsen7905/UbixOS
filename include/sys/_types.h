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

#ifndef _SYS__TYPES_H_
#define _SYS__TYPES_H_

#include <sys/cdefs.h>
#include <machine/_types.h>

typedef __uint32_t            __ino_t;
typedef __uint32_t            __dev_t;		/* device number */
typedef __uint16_t            __mode_t;
typedef __uint16_t            __nlink_t;	/* link count */
typedef __uint32_t            __uid_t;
typedef __uint32_t            __gid_t;
typedef __int32_t             __time_t;
typedef __int64_t             __blkcnt_t;	/* file block count */
typedef __uint32_t            __blksize_t;	/* file block size */
typedef __uint32_t            __fflags_t;	/* file flags */
typedef int ___wchar_t;

typedef long __suseconds_t; /* microseconds (signed) */
typedef __int32_t	 __pid_t;/* process [group] */

#if !defined(__clang__) || !defined(__cplusplus)
typedef __uint_least16_t   __char16_t;
typedef __uint_least32_t   __char32_t;
#endif

#endif /* !_SYS__TYPES_H_ */
