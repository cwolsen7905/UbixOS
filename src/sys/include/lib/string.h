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

 $Id: string.h 80 2016-01-11 16:24:44Z reddawg $

 *****************************************************************************************/

#ifndef _STRING_H
#define _STRING_H

#include <sys/types.h>

extern u_char const bcd2bin_data[];
extern u_char const bin2bcd_data[];
extern char const hex2ascii_data[];

#define bcd2bin(bcd)    (bcd2bin_data[bcd])
#define bin2bcd(bin)    (bin2bcd_data[bin])
#define hex2ascii(hex)  (hex2ascii_data[hex])

#define toupper(c) ((c) - 0x20 * (((c) >= 'a') && ((c) <= 'z')))
#define tolower(c) ((c) + 0x20 * (((c) >= 'A') && ((c) <= 'Z')))

#ifdef __cplusplus
extern "C" {
#endif

char * strcpy( char *, const char * );
int strcmp( const char *str1, const char *str2 );
int strncmp( const char * a, const char * b, size_t c );
void *memcpy( const void *dst, const void * src, size_t length );
void *memset( void * dst, int c, size_t length );
int strlen( const char * string );
int memcmp( const void * dst, const void * src, size_t length );
void strncpy( char * dest, const char * src, size_t size );
char *strtok( char *str, const char *sep );
char *strtok_r( char *str, const char *sep, char **last );
char *strstr( const char *s, char *find );

int sprintf( char *buf, const char *fmt, ... );

#ifdef __cplusplus
}
#endif

#endif

/***
 $Log: string.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:41  reddawg
 no message

 Revision 1.5  2004/07/20 19:09:40  flameshadow
 chg: put strcpy() declaration in the right .h file
 add: prototypes for the dirCaching functions in dirCache.h
 chg: renamed dirCaching functions

 Revision 1.4  2004/07/06 23:26:12  reddawg
 Fixed A Compilation Error

 Revision 1.3  2004/06/28 23:12:58  reddawg
 file format now container:/path/to/file

 Revision 1.2  2004/05/21 15:00:27  reddawg
 Cleaned up


 END
 ***/
