/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Id: memset.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <string.h>

#define wsize   sizeof(uInt)
#define wmask   (wsize - 1)
#define VAL     c0
#define WIDEVAL c

void *memset(void *dst0, int c0, size_t length) {
  size_t t;
  uInt c;
  uInt8 *dst;

  dst = dst0;

        if (length < 3 * wsize) {
                while (length != 0) {
                        *dst++ = VAL;
                        --length;
                }
                return(dst0);
        }

        if ((c = (uInt8)c0) != 0) {    /* Fill the word. */
                c = (c << 8) | c;       /* u_int is 16 bits. */
                c = (c << 16) | c;      /* u_int is 32 bits. */
        }

        /* Align destination by filling in bytes. */
        if ((t = (long)dst & wmask) != 0) {
                t = wsize - t;
                length -= t;
                do {
                        *dst++ = VAL;
                } while (--t != 0);
        }

        /* Fill words.  Length was >= 2*words so we know t >= 1 here. */
        t = length / wsize;
        do {
                *(u_int *)dst = WIDEVAL;
                dst += wsize;
        } while (--t != 0);

        /* Mop up trailing bytes, if any. */
        t = length & wmask;
        if (t != 0)
                do {
                        *dst++ = VAL;
                } while (--t != 0);
        return(dst0);
  }


/***
 $Log: memset.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:12  reddawg
 no message

 Revision 1.1  2004/07/28 16:20:10  reddawg
 forgot this file

 END
 ***/

