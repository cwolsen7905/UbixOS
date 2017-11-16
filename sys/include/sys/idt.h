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

 $Id: idt.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _IDT_H
#define _IDT_H

#include <sys/types.h>
#include <sys/gdt.h>

#define EFLAG_TF         0x100
#define EFLAG_IF         0x200
#define EFLAG_IOPL3      0x3000
#define EFLAG_VM         0x20000

int idt_init();
void setVector(void *handler, unsigned char interrupt, unsigned short controlMajor);
void setTaskVector(uInt8 interrupt,uInt16 controlMajor,uInt8 selector);
void intNull();

void _int0();
void _int1();
void _int2();
void _int3();
void _int4();
void _int5();
void _int6();
void _int7();
void _int8();
void _int9();
void _int10();
void _int11();
void _int12();
void _int13();
void timerInt();

#endif

/***
 $Log: idt.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:15  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:52  reddawg
 no message

 Revision 1.5  2004/09/07 21:54:38  reddawg
 ok reverted back to old scheduling for now....

 Revision 1.3  2004/07/09 13:16:41  reddawg
 idt: idtInit to idt_init
 Adjusted Startup Routines

 Revision 1.2  2004/05/21 15:12:17  reddawg
 Cleaned up


 END
 ***/
