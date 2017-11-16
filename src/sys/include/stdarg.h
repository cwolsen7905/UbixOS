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

 $Id: stdarg.h 75 2016-01-11 06:15:17Z reddawg $

 *****************************************************************************************/

#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;

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

int vsprintf( char *buf, const char *fmt, va_list args );

#endif

/***
 $Log: stdarg.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:38  reddawg
 no message

 Revision 1.2  2004/05/21 15:22:35  reddawg
 Cleaned up


 END
 ***/
