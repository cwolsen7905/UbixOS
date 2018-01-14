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

 $Id: directory.c 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#include <ubixfs/ubixfs.h>
#include <vfs/file.h>
#include <vfs/mount.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

static dirList_t dirList = 0x0;

dirList_t 
ubixFSLoadDir(char *data) {
  dirList_t tmpDir = 0x0;

  tmpDir = (dirList_t)kmalloc(sizeof(struct directoryList));

  sprintf(tmpDir->dirName,"%s",data);

  if (0x0 == dirList) {
    dirList = tmpDir;
    }
  else {
    tmpDir->next = dirList;
    tmpDir->prev = 0x0;
    dirList->prev = tmpDir;
    dirList       = tmpDir;
    }

  if (!strcmp(":",data)) {
    tmpDir->dirCache = (char *)kmalloc(0x4000);
    }

  return(tmpDir);
  }

int 
addDirEntry(struct directoryEntry *dir,fileDescriptor_t *fd) {
  uInt32 i = 0x0;
  uInt32 entries = 0x0;
  struct directoryEntry *tmp = 0x0;

  tmp = (struct directoryEntry *)kmalloc(fd->size);

  readUbixFS(fd,(char *)tmp,fd->offset,fd->size);
  entries = fd->size/sizeof(struct directoryEntry);
  for (i=0;(tmp[i].attributes != 0x0) && (i < entries);i++);

  if (i == entries) {
    tmp = (struct directoryEntry *)kmalloc(0x1000);
    i = 0x0;
    }
  else {
    fd->offset = 0x0;
    }
  memcpy(&tmp[i],dir,sizeof(struct directoryEntry));
  
  if (writeUbixFS(fd,(char *)tmp,fd->offset,fd->size) == 0x0) {
    kprintf("Error Creating Directory\n");
    }
  
  return(0x0);
  }

int 
ubixFSmkDir(char *directory,fileDescriptor_t *fd) {
  int block = 0x0;

  struct directoryEntry *dir   = 0x0;
  struct directoryEntry *entry = 0x0;
  struct ubixFSInfo *fsInfo    = fd->mp->fsInfo;

  //kprintf("Creating Directory: %s",directory);
  
  block = getFreeBlocks(1,fd);
  if (block != 0x0) {
    dir   = (struct directoryEntry *)kmalloc(UBIXFS_BLOCKSIZE_BYTES);
    entry = (struct directoryEntry *)kmalloc(sizeof(struct directoryEntry));

    entry->startCluster = block;
    entry->size         = UBIXFS_BLOCKSIZE_BYTES;
    entry->attributes   = typeDirectory;
    entry->permissions  = 0xEAA;
    sprintf(entry->fileName, directory);

    //dir->attributes = typeDirectory;
    //sprintf(dir->fileName,"Test Entry");
    
    fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,
                       dir,
                       fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[block].realSector,
                       blockSize);
    addDirEntry(entry,fd);
    kfree(dir);
    kfree(entry);
    }

  return(0x0);
  }

/***
 $Log: directory.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:41  reddawg
 no message

 Revision 1.12  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.11  2004/08/01 17:58:39  flameshadow
 chg: fixed string allocation bug in ubixfs_cacheNew()

 Revision 1.10  2004/07/23 09:10:06  reddawg
 ubixfs: cleaned up some functions played with the caching a bit
 vfs:    renamed a bunch of functions
 cleaned up a few misc bugs

 Revision 1.9  2004/07/16 20:17:29  flameshadow
 chg: broke the ufs stuff
 chg: changed vfsRegisterFS() to accept a fileSystem struct
 chg: modified calls to vfsRegisterFS() to pass fileSystem struct

 Revision 1.8  2004/06/29 03:59:47  reddawg
 Fixed a few issues with subdirectories they are working much better now

 Revision 1.7  2004/06/28 23:12:58  reddawg
 file format now container:/path/to/file

 Revision 1.6  2004/06/04 13:20:22  reddawg
 ubixFSmkDir(): played with it a bit to see if it still worked

 Revision 1.5  2004/06/04 10:19:42  reddawg
 notes: we compile again, thank g-d anyways i was about to cry

 Revision 1.4  2004/05/19 15:20:06  reddawg
 Fixed reference problems due to changes in drive subsystem

 Revision 1.3  2004/05/19 04:07:43  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.2  2004/04/28 02:22:55  reddawg
 This is a fiarly large commit but we are starting to use new driver model
 all around

 Revision 1.1.1.1  2004/04/15 12:07:07  reddawg
 UbixOS v1.0

 Revision 1.6  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
