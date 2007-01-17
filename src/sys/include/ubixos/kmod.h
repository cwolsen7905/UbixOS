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

#ifndef _KMOD_H
#define _KMOD_H

#include <ubixos/types.h>

#define LD_START 0x1000000

typedef struct kmod_struct {
  struct kmod_struct *next;
  struct kmod_struct *prev;
  uInt16              id;
  uInt16              refs;
  uInt32              address;
  char                name[128];
} kmod_t;
  

uInt32 kmod_load(const char *);
uInt32 kmod_add(const char *, const char *name);

#endif

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:55  reddawg
 no message

 Revision 1.5  2005/08/04 20:35:19  fsdfs

 various updates. mostly kprints, tabbing code to look cleaner

 Revision 1.4  2004/09/26 20:46:13  reddawg
 ok time for bed added kmod_add keeps modules listed now

 Revision 1.3  2004/09/26 20:40:51  reddawg
 Added baseAddr to the kmod_t

 Revision 1.2  2004/09/26 20:39:19  reddawg
 Added kmod struct type kmod_t

 Revision 1.1  2004/09/20 07:33:10  reddawg
 Start of kernel modules will make it much more flexable - These modules can be either in kernel threads or system services...

 END
 ***/
