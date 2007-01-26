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

#include <sys/types.h>
#include <fcntl.h>

pid_t old_fork() {
  volatile pid_t pid = 0x0;
  asm volatile(
    "pushl %%eax\n"
    "pushl %%ecx\n"
    "movl $4,%%eax\n"
    "push %%ss\n"
    "movl %%esp,%%ecx\n"
    "sub $4,%%ecx\n"
    "pushl %%ecx\n"
    "int $0x80  \n"
    "popl %%esp\n"
    "pop %%ax\n"
    "popl %%ecx\n"
    "popl %%eax\n"
    :
    : "b" (&pid)
    );
  return(pid);
  }
  
/***
 $Log$
 Revision 1.1.1.1  2007/01/17 03:30:21  reddawg
 UbixOS

 Revision 1.6  2004/08/25 22:02:41  reddawg
 task switching - We now are using software switching to be consistant with the rest of the world because all of this open source freaks gave me a hard time about something I liked. There doesn't seem to be any gain in performance but it is working fine and flawlessly

 Revision 1.5  2004/08/24 23:33:45  reddawg
 Fixed

 Revision 1.4  2004/08/02 18:50:13  reddawg
 Updates to make some variable volatile to make work with gcc 3.3. However there are still some issues but we have not caused new issues with gcc 2.95

 Revision 1.3  2004/08/01 20:14:18  reddawg
 Fixens

 Revision 1.2  2004/08/01 19:59:19  reddawg
 *** empty log message ***

 END
 ***/


