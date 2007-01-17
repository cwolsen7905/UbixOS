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

#ifndef _TSS_H
#define _TSS_H

#include <ubixos/types.h>

struct tssStruct {
  short back_link;
  short back_link_reserved;
  long  esp0;
  short ss0;
  short ss0_reserved;
  long  esp1;
  short ss1;
  short ss1_reserved;
  long  esp2;
  short ss2;
  short ss2_reserved;
  long  cr3;
  long  eip;
  long  eflags;
  long  eax,ecx,edx,ebx;
  long  esp;
  long  ebp;
  long  esi;
  long  edi;
  short es;
  short es_reserved;
  short cs;
  short cs_reserved;
  short ss;
  short ss_reserved;
  short ds;
  short ds_reserved;
  short fs;
  short fs_reserved;
  short gs;
  short gs_reserved;
  short ldt;
  short ldt_reserved;
  short trace_bitmap;
  short io_map;
  char  io_space[8192];
  };

struct i387Struct {
  long cwd;
  long swd;
  long twd;
  long fip;
  long fcs;
  long foo;
  long fos;
  long st_space[20];   /* 8*10 bytes for each FP-reg = 80 bytes */
  };

struct i386_frame {
  uInt32 gs;
  uInt32 fs;
  uInt32 es;
  uInt32 ds;
  uInt32 ss;
  uInt32 edi;
  uInt32 esi;
  uInt32 ebp;
  uInt32 esp;
  uInt32 ebx;
  uInt32 edx;
  uInt32 ecx;
  uInt32 eax;
  /*
  uInt32 vector;
  uInt32 error_code;
  */
  uInt32 eip;
  uInt32 cs;
  uInt32 flags;
  uInt32 user_esp;
  uInt32 user_ss;
  };

#endif

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:15  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:53  reddawg
 no message

 Revision 1.6  2004/07/27 07:42:29  reddawg
 *burp*

 Revision 1.5  2004/07/27 07:40:41  reddawg
 does it compile now?

 Revision 1.4  2004/07/27 07:27:50  reddawg
 chg: I was fooled thought we failed but it was a casting issue

 Revision 1.3  2004/07/22 20:53:07  reddawg
 atkbd: fixed problem

 Revision 1.2  2004/05/21 15:12:17  reddawg
 Cleaned up


 END
 ***/
