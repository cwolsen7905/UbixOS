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

 $Id: thread.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <ubixfs/ubixfs.h>
#include <ubixos/kpanic.h>
#include <vfs/vfs.h>
#include <lib/kprintf.h>

static struct {
  int mounts;
  } ubixFS_Info;

void ubixfs_thread(struct vfs_mountPoint *mp) {
  mpi_message_t myMsg;

  ubixFS_Info.mounts = 0;

  if (mp == 0x0)
    kpanic("bah");

  if (mpi_createMbox("ubixfs") != 0x0) {
    kpanic("Error: Error creating mailbox: ubixfs\n");
    }
  while (1) {
    if (mpi_fetchMessage("ubixfs",&myMsg) == 0x0) {
      switch(myMsg.header) {
        default:
          kprintf("Unhandled Message: %i\n",myMsg.header);
          break;
        }
      }
    }
  }

/***
 $Log: thread.c,v $
 Revision 1.2  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:41  reddawg
 no message

 Revision 1.3  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.2  2004/07/23 09:10:06  reddawg
 ubixfs: cleaned up some functions played with the caching a bit
 vfs:    renamed a bunch of functions
 cleaned up a few misc bugs

 Revision 1.1  2004/06/28 18:12:44  reddawg
 We need these files

 END
 ***/
