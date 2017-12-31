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

 $Id: tty.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _TTY_H
#define _TTY_H

#include <sys/types.h>

#define TTY_MAX_TERMS 5

typedef struct tty_termNode {
  char    *tty_buffer;
  char    *tty_pointer;
  uInt8    tty_colour;
  uInt16   tty_x;
  uInt16   tty_y;
  pidType  owner;
  char     stdin[512];
  int      stdinSize;
  } tty_term;

int tty_init();
int tty_change(uInt16);
tty_term *tty_find(uInt16);
int tty_print(char *,tty_term *);

extern tty_term *tty_foreground;

#endif

/***
 $Log: tty.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:57  reddawg
 no message

 Revision 1.8  2004/09/06 22:11:29  reddawg
 tty: now each tty has a stdin....

 Revision 1.7  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.6  2004/08/09 12:58:05  reddawg
 let me know when you got the surce

 Revision 1.5  2004/08/09 05:40:31  reddawg
 tty: removed current and made a foreground

 Revision 1.4  2004/08/06 22:32:16  reddawg
 Ubix Works Again

 Revision 1.2  2004/08/04 08:17:57  reddawg
 tty: we have primative ttys try f1-f5 so it is easier to use and debug
      ubixos

 Revision 1.1  2004/08/03 21:44:24  reddawg
 ttys

 END
 ***/

