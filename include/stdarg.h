/**************************************************************************************
 Copyright (c) 2002 The UbixOS Project
 All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, the following disclaimer and the list of authors.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, the following disclaimer and the list of authors
in the documentation and/or other materials provided with the distribution. Neither the name of the UbixOS Project nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: stdarg.h 111 2016-01-13 05:50:42Z reddawg $

**************************************************************************************/

#ifndef _STDARG_H
#define _STDARG_H

#ifdef __TINYC__
/* TCC i386: va_list is char * */
typedef char *va_list;
typedef char *vaList;

#define va_start(ap, last) \
    ((ap) = ((char *)&(last)) + ((sizeof(last)+3)&~3))
#define va_arg(ap, type) \
    ((ap) += (sizeof(type)+3)&~3, *(type *)((ap) - ((sizeof(type)+3)&~3)))
#define va_end(ap)        ((void)0)
#define va_copy(dst, src) ((dst) = (src))

#else
/* GCC / Clang */
typedef __builtin_va_list va_list;
typedef __builtin_va_list vaList;

#define va_start(ap, last)	__builtin_va_start((ap), (last))
#define va_arg(ap, type)	__builtin_va_arg((ap), type)
#define va_end(ap)		__builtin_va_end(ap)
#define va_copy(dst, src)	__builtin_va_copy((dst), (src))
#endif

/* UbixOS legacy aliases */
#define vaStart  va_start
#define vaArg    va_arg
#define vaEnd    va_end
#define __va_copy va_copy

#endif /* _STDARG_H */
