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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include "./include/ubistry.h"

int main(int argc,char **argv) {
  //FILE *pidFile;

  if (fork() != 0x0) {
    exit(0x0);
    }

  ubistryInitMbox(MBOX_NAME);

  ubistryAddKey("Ubu","Creator Of UbixOS");
  ubistryAddKey("TCA","The GUI GUY!!!");

  while (1) {
    ubistryProcessMessages();
    sched_yield();
    }

  exit(0x0);
  }

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:14:04  reddawg
 no message

 Revision 1.10  2004/09/08 23:19:58  reddawg
 hmm

 Revision 1.9  2004/08/02 18:50:13  reddawg
 Updates to make some variable volatile to make work with gcc 3.3. However there are still some issues but we have not caused new issues with gcc 2.95

 Revision 1.8  2004/07/17 16:43:10  reddawg
 shell: fixed stress testing
 ubistry: fixed some segfaults
 spinlock: added assert()

 Revision 1.7  2004/06/17 15:10:55  reddawg
 Fixed Some Global Variables

 Revision 1.6  2004/06/10 13:08:00  reddawg
 Minor Bug Fixes

 Revision 1.5  2004/06/01 01:30:43  reddawg
 No more warnings and organized make files

 Revision 1.4  2004/05/26 15:41:20  reddawg
 ubistry: added command 666 which will restart the registry server also added
          command 51 to add a key format key,value

 Revision 1.3  2004/05/26 13:10:39  reddawg
 ubistry: added two functions
          ubistryFindKey(char *name) <- Will find key with specified name
          ubistryAddKey(char *name,char *value) <-> Will add key with specified
            name and value

 Revision 1.2  2004/05/26 12:16:02  reddawg
 ubistry: now runs as a deamon

 Revision 1.1  2004/05/26 12:09:12  reddawg
 ubistry: this is the frame work for the ubix registry system more to come
          over the next few days

 END
 ***/
