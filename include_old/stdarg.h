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

typedef char *vaList;

/*
#define _vaSize(TYPE) (((sizeof(TYPE) + sizeof(int) -1) / sizeof(int)) * sizeof(int))
#define vaStart(AP, LASTARG) (AP=((vaList)&(LASTARG) + _vaSize(LASTARG)))
#define vaEnd(AP)
#define vaArg(AP, TYPE) (AP += _vaSize(TYPE), *((TYPE *)(AP - _vaSize(TYPE))))
*/

#define vaStart(ap, last) \
        __builtin_va_start((ap), (last))

#define vaArg(ap, type) \
        __builtin_va_arg((ap), type)

#define __va_copy(dest, src) \
        __builtin_va_copy((dest), (src))

#define vaEnd(ap) \
        __builtin_va_end(ap)


#endif
