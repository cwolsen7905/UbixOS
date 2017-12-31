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

 $Id: types.h 89 2016-01-12 00:20:40Z reddawg $

**************************************************************************************/

#ifndef TYPES_H
#define TYPES_H

#include <wchar.h>



/**** NEW TYPEDEFS ****/
#ifndef _INTPTR_T_DECLARED
typedef __intptr_t      intptr_t;
typedef __uintptr_t     uintptr_t;
#define _INTPTR_T_DECLARED
#endif

#ifndef _OFF_T_DECLARED
typedef __off_t         off_t;
#define _OFF_T_DECLARED
#endif

#ifndef _SSIZE_T_DECLARED
typedef __ssize_t       ssize_t;
#define _SSIZE_T_DECLARED
#endif

#ifndef _UINT64_T_DECLARED
typedef __uint64_t      uInt64;
#define _UINT64_T_DECLARED
#endif

#ifndef _INT32_T_DECLARED
typedef __int32_t       int32_t;
#define _INT32_T_DECLARED
#endif

#ifndef _INT64_T_DECLARED
typedef __int64_t       int64_t;
#define _INT64_T_DECLARED
#endif


/**** END ****/

typedef unsigned char   u_char;
typedef unsigned int    u_int;

typedef unsigned short uShort;
typedef unsigned long  uLong;
typedef unsigned char  uChar;
typedef unsigned int   uInt;

typedef unsigned char  uInt8;
typedef unsigned short uInt16;
typedef unsigned int  uInt32;

typedef long long int quad_t;
typedef unsigned long long int u_quad_t;
 
#ifndef NULL
#define NULL 0x0
#endif

#ifndef NOBOOL
typedef enum { FALSE=0,TRUE=1 } bool;
#endif



#endif
