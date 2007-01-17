/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

*****************************************************************************************/

#ifndef _TYPES_H
#define _TYPES_H

#include <sys/_types.h>

#ifndef NULL
#define NULL 0x0
#endif

typedef unsigned char  uInt8;
typedef unsigned short uInt16;
typedef unsigned int   uInt32;
typedef unsigned int   uInt;
typedef char Int8;
typedef short Int16;
typedef long Int32;

typedef __uint8_t       u_int8_t;       /* unsigned integrals (deprecated) */
typedef __uint16_t      u_int16_t;
typedef __uint32_t      u_int32_t;
typedef __uint64_t      u_int64_t;
//typedef long long int quad_t;
typedef __uint64_t      quad_t;

typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned int    u_int;
typedef unsigned long   u_long;


typedef int pidType;

typedef int  pid_t;
typedef int size_t; /* standart */

#ifndef NOBOOL
#ifndef __cplusplus
typedef enum { FALSE=0,TRUE=1 } bool;
#endif
#endif

#ifndef _INO_T_DECLARED
typedef __ino_t         ino_t;          /* inode number */
#define _INO_T_DECLARED
#endif

#ifndef _INT8_T_DECLARED
typedef __int8_t        int8_t;
#define _INT8_T_DECLARED
#endif

#ifndef _INT16_T_DECLARED
typedef __int16_t       int16_t;
#define _INT16_T_DECLARED
#endif

#ifndef _INT32_T_DECLARED
typedef __int32_t       int32_t;
#define _INT32_T_DECLARED
#endif

#ifndef _INT64_T_DECLARED
typedef __int64_t       int64_t;
#define _INT64_T_DECLARED
#endif

typedef __ssize_t       ssize_t;
typedef char *          caddr_t;
typedef __int64_t       off_t;
typedef __uint32_t      vm_offset_t;

typedef __uid_t         uid_t;          /* user id */
typedef __gid_t         gid_t;          /* group id */
typedef __blkcnt_t      blkcnt_t;
typedef __blksize_t     blksize_t;
typedef __fflags_t      fflags_t;

#ifndef _TIME_T_DECLARED
typedef __time_t        time_t;
#define _TIME_T_DECLARED
#endif


#endif

/***
 END
 ***/

