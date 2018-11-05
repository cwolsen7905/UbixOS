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

 $Log: block.c,v $
 Revision 1.2  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:39  reddawg
 no message

 Revision 1.6  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.5  2004/07/23 09:10:06  reddawg
 ubixfs: cleaned up some functions played with the caching a bit
 vfs:    renamed a bunch of functions
 cleaned up a few misc bugs

 Revision 1.4  2004/06/04 10:19:42  reddawg
 notes: we compile again, thank g-d anyways i was about to cry

 Revision 1.3  2004/05/19 15:20:06  reddawg
 Fixed reference problems due to changes in drive subsystem

 Revision 1.2  2004/04/28 02:22:55  reddawg
 This is a fiarly large commit but we are starting to use new driver model
 all around

 Revision 1.1.1.1  2004/04/15 12:07:07  reddawg
 UbixOS v1.0

 Revision 1.3  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license



 $Id: block.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <fs/ubixfs/ubixfs.h>
#include <sys/vfs/file.h>
#include <sys/vfs/mount.h>


void syncBat(struct vfs_mountPoint *mp) {
  struct ubixFSInfo *fsInfo = mp->fsInfo;
  mp->device->devInfo->write(mp->device->devInfo->info,fsInfo->blockAllocationTable,mp->diskLabel->partitions[mp->partition].pOffset,mp->diskLabel->partitions[mp->partition].pBatSize);
  }

int freeBlocks(int block,fileDescriptor_t *fd) {
  int i = block;
  
  struct ubixFSInfo *fsInfo = fd->mp->fsInfo;

  while (i != 0x0) {
    block = fsInfo->blockAllocationTable[i].nextBlock;
        
    fsInfo->blockAllocationTable[i].attributes = 0x0;
    fsInfo->blockAllocationTable[i].nextBlock  = 0x0;
    
    i = block;
    }
  syncBat(fd->mp);
  return(i);
  }
  
int getFreeBlocks(int count,fileDescriptor_t *fd) {
  uInt32 i = 0x0;
  uInt32 x = 0x0;
  
  struct ubixFSInfo *fsInfo = fd->mp->fsInfo;

  getBlocks:
  for (i=1;i < fsInfo->batEntries;i++) {
    if (fsInfo->blockAllocationTable[i].attributes == 0x0) {
      for (x = 1; x < (uInt32)count; x++) {
        if (fsInfo->blockAllocationTable[i + x].attributes != 0x0) {
          goto getBlocks;
          }
        }
      for (x = i; x < i+count;x++) {
        fsInfo->blockAllocationTable[x].attributes = 0x1;
        if ((x+1) == (i+count)) {
          fsInfo->blockAllocationTable[x].nextBlock = -1;
          }
        else {
          fsInfo->blockAllocationTable[x].nextBlock  = x+1;
          }
        }
      syncBat(fd->mp);
      return(i);
      }
    }
  return(0x0);
  }

/***
 END
 ***/

