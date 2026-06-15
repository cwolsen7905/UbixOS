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

#ifndef _STDARG_H
#define _STDARG_H

/*
 * va_list is the compiler's builtin variadic-argument type.  On i386 it is
 * pointer-like (historically char *); on aarch64 and x86_64 it is an array/struct
 * type, so the builtin type must be used or __builtin_va_arg rejects it.
 */
#if defined(__aarch64__) || defined(__x86_64__)
typedef __builtin_va_list va_list;
#else
typedef char *va_list;
#endif

/*
 #define __va_size(type) (((sizeof(type) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))
 #define va_start(ap, last) ((ap) = (va_list)&(last) + __va_size(last))
 #define va_arg(ap,type) (*(type *)((ap) += __va_size(type), (ap) - __va_size(type)))
 #define va_end(ap) ( ap = (va_list)0 )
 */

#define va_start(ap, last) \
        __builtin_va_start((ap), (last))

#define va_arg(ap, type) \
        __builtin_va_arg((ap), type)

#define __va_copy(dest, src) \
        __builtin_va_copy((dest), (src))

#define va_end(ap) \
        __builtin_va_end(ap)

int vsprintf(char *buf, const char *fmt, va_list args);

#endif
