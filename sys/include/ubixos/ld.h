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

 $Id: ld.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _LD_H
#define _LD_H

#include <sys/types.h>

#define LD_START 0x1000000

uInt32 ldEnable();

#endif

/***
 $Log: ld.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:55  reddawg
 no message

 Revision 1.10  2004/06/17 12:20:32  reddawg
 Try this now solarwind

 Revision 1.9  2004/06/17 02:19:29  reddawg
 Cleaning out dead code

 Revision 1.8  2004/06/16 17:32:14  reddawg
 Removed Dead LD Code now part of ld.so

 Revision 1.7  2004/06/16 17:04:13  reddawg
 ld.so: rest of the commit

 Revision 1.4  2004/06/13 03:05:15  reddawg
 we now have a dynamic linker

 Revision 1.3  2004/06/12 01:27:26  reddawg
 shared objects: yes we almost fully support shared objects

 Revision 1.2  2004/05/21 15:20:00  reddawg
 Cleaned up

 END
 ***/