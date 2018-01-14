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

 $Id: ubixfs.c 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#include <ubixfs/ubixfs.h>
#include <ubixfs/dirCache.h>
#include <vfs/vfs.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/exec.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <lib/kprintf.h>
#include <assert.h>

/* Static defines */
static int ubixfs_loadData(fileDescriptor *fd,char *data,uInt32 size,uInt32 batIndex);

  
static int openFileUbixFS(const char *file, fileDescriptor *fd) {
  //int x = 0;
/* mji  struct directoryEntry *dirEntry = (struct directoryEntry *)kmalloc(0x4000); */
  struct cacheNode * cacheNode = NULL;
  //struct directoryEntry * dirEntry = NULL;
  struct ubixFSInfo *fsInfo = fd->mp->fsInfo;
  

  
/* kprintf("openFileUbixFS(%s), cwd: %s\n", file, _current->oInfo.cwd); */

//if (fsInfo->dirCache == NULL) kprintf("dirCache is null!\n");
  assert(fd);
  assert(fd->mp);
  assert(fd->mp->device);
  assert(fd->mp->device->devInfo);
  assert(fd->mp->device->devInfo->read);
  assert(fsInfo);
  assert(fsInfo->dirCache);
  assert(file);
  
  if ((fd->mode & fileRead) == fileRead) {
    do {
      cacheNode = ubixfs_cacheFind(fsInfo->dirCache,(char *) file);
      if (cacheNode == NULL) return 0;
      if (cacheNode->present == 1) break;
      assert(cacheNode->size);
      if (*cacheNode->size != 0 && cacheNode->info == NULL) {
        //kprintf("caching name(size): %s(%d)\n",cacheNode->name,*cacheNode->size);
        cacheNode->info = kmalloc(UBIXFS_ALIGN(*cacheNode->size));
        fd->size = *cacheNode->size; 
        assert(cacheNode->startCluster);
        ubixfs_loadData(fd,
                        cacheNode->info,
                        *cacheNode->size,
                        *cacheNode->startCluster);
        cacheNode->present = 1;
      } /* if */
    } while(1);

    assert(cacheNode);
    if (cacheNode == NULL) return 0; /* this should be caught above */

    fd->start    = *cacheNode->startCluster;
    fd->size     = *cacheNode->size;
    fd->perms    = *cacheNode->permissions;
    fd->cacheNode = cacheNode; /* Directory Start Sector */
    /*
    if (cacheNode->size != 0x0 && cacheNode->info == NULL) {
      cacheNode->info = kmalloc(UBIXFS_ALIGN(*cacheNode->size));
      ubixfs_loadData(fd,cacheNode->info,cacheNode->size,cacheNode->startCluster);
      cacheNode->present = 0x1;
      }
     */
    return(0x1);
  } 
  else 
    if ((fd->mode & fileWrite) == fileWrite) {
kprintf("Ouch! in filewrite!\n");
#if 0
      fd->start    = dirEntry->startCluster;
      fd->size     = dirEntry->size;
      fd->perms    = dirEntry->permissions;
     // fd->dirBlock = 0x0; /* Directory Start Sector */
#endif
      return(0x1);
    }
    
  return 0;

  }

int writeFileByte(int ch, fileDescriptor *fd, long offset) {

  int blockCount = 0x0,batIndex = 0x0,batIndexPrev = 0x0;
  uInt32 i = 0x0;
  struct directoryEntry *dirEntry = 0x0;
  struct ubixFSInfo *fsInfo = NULL;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->diskLabel);

  batIndexPrev = fd->start;
  fsInfo = fd->mp->fsInfo;

  /* Find Out How Many Blocks Long This File Is */
  blockCount = (offset/4096);
  
  /* Find The Block If It Doesn't Exist We Will Have To Allocate One */
  for (i=0x0; i <= fd->mp->diskLabel->partitions[fd->mp->partition].pBatSize;
       i++) {
    batIndex = fsInfo->blockAllocationTable[batIndexPrev].nextBlock;
    if (batIndex == 0x0) {
      break;
      }
    batIndexPrev = batIndex;
    }

  if ((offset%4096 == 0) && (fd->status == fdRead)) {
      fd->status = fdOpen;
      }    

  /* If batIndex == 0x0 Then We Must Allocate A New Block */
  if (batIndex == 0x0) {
    for (i=1;i < fsInfo->batEntries;i++) {
      if (fsInfo->blockAllocationTable[i].attributes == 0x0) {
        fsInfo->blockAllocationTable[batIndexPrev].nextBlock = i;
        fsInfo->blockAllocationTable[batIndex].nextBlock     = -1;
        batIndex  = i;
        fd->start = i;
        break;
        }
      }
    /* fd->mp->drive->read(fd->mp->drive->driveInfoStruct,diskLabel->partitions[0].pOffset+blockAllocationTable[batIndex].realSector,8,fd->buffer); */
    fd->buffer[offset-(blockCount*4096)] = ch;
    fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[batIndex].realSector,8);
    }
  else {
    if (fd->status != fdRead) {    
      fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[batIndex].realSector,8);
      fd->status = fdRead;
      }
    fd->buffer[offset-(blockCount*4096)] = ch;
    fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[batIndex].realSector,8);
    }    
  if ((uInt32)offset > fd->size) {
    fd->size = offset;
    dirEntry = (struct directoryEntry *)kmalloc(4096);
  /*
    fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,dirEntry,(fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[fd->dirBlock].realSector),8);
  */
    for (i=0x0;i<(4096/sizeof(struct directoryEntry));i++) {
      if ((int)!strcmp(dirEntry[i].fileName,fd->fileName))
        break;
      }  
    dirEntry[i].size = fd->size;
/*
    fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,dirEntry,(fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[fd->dirBlock].realSector),8);
*/
    kfree(dirEntry);
    }
  return(ch);
  }

/* Verified Functions */

    
int readUbixFS(fileDescriptor *fd,char *data,uInt32 offset,long size) {
  int i = 0x0;
  char *buffer = 0x0;
  struct ubixFSInfo *fsInfo = NULL;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->fsInfo);

  fsInfo = fd->mp->fsInfo;
  
  if (fd->cacheNode->present != 1) 
    kpanic("ERROR with cache node\n");
    
  buffer = (char *)fd->cacheNode->info;
  
  for (i=0x0; i<size; i++) {
    if (offset > fd->size) {
      /* Set File EOF If There Is Nothing To Do */
      /* data[i] = '\0'; Is this safe? */
      fd->status = fdEof;
      return(size);
      }
    /* Copy Data From Buffer To Data */
    data[i] = buffer[i + offset];
    }
  /* Return */
  return(size);
  }


/************************************************************************

Function: int writeUbixFS(fileDescriptor *fd,char *data,long offset,long size)
Description: Write Data Into File
Notes:

************************************************************************/
int writeUbixFS(fileDescriptor *fd,char *data,long offset,long size) {
  uInt32 blockOffset    = 0x0;
  uInt32 blockIndex;
  uInt32 blockIndexPrev;
  uInt32 i              = 0x0;
  struct ubixFSInfo *fsInfo = NULL;
  struct directoryEntry *dirEntry = 0x0;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->fsInfo);
  assert(fd->mp->device);
  assert(fd->mp->device->devInfo);

  blockIndex = blockIndexPrev = fd->start;
  fsInfo = fd->mp->fsInfo;

  blockOffset = (offset/0x1000);

  if (fd->size != 0x0) {
    for (i = 0x0;i < blockOffset;i++) {
      blockIndex = fsInfo->blockAllocationTable[blockIndexPrev].nextBlock;
        if ((int)blockIndex == EOBC) {
          blockIndex = getFreeBlocks(1,fd);
          fsInfo->blockAllocationTable[blockIndexPrev].nextBlock = blockIndex;
          fsInfo->blockAllocationTable[blockIndex].nextBlock = EOBC;
          break;
          }
        blockIndexPrev = blockIndex;
      }
    }

  fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[blockIndex].realSector,blockSize);
  for (i = 0x0;i < (uInt32)size;i++) {

    fd->buffer[(offset- (blockOffset *0x1000))] = data[i];
    offset++;

    if (offset%4096 == 0x0) {
      blockOffset++;
      fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[blockIndex].realSector,blockSize);
 
      if (fsInfo->blockAllocationTable[blockIndex].nextBlock == EOBC) {
        blockIndexPrev = blockIndex;
        blockIndex = getFreeBlocks(1,fd);
        fsInfo->blockAllocationTable[blockIndexPrev].nextBlock = blockIndex;
        fsInfo->blockAllocationTable[blockIndex].nextBlock     = EOBC;
        }
      else {
        blockIndex = fsInfo->blockAllocationTable[blockIndex].nextBlock;
        fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[blockIndex].realSector,blockSize);
        }
      }
    }
  fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,fd->buffer,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[blockIndex].realSector,blockSize);

  if ((uInt32)offset > fd->size) {
    fd->size = offset;
    dirEntry = (struct directoryEntry *)kmalloc(4096);
/*
    fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,dirEntry,(fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[fd->dirBlock].realSector),blockSize);
*/
    for (i=0x0;i<(4096/sizeof(struct directoryEntry));i++) {
      if ((int)!strcmp(dirEntry[i].fileName,fd->fileName))
        break;
      }
    dirEntry[i].size = fd->size;
    dirEntry[i].startCluster = fd->start;
/*
    fd->mp->device->devInfo->write(fd->mp->device->devInfo->info,dirEntry,(fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[fd->dirBlock].realSector),blockSize);
*/
    kfree(dirEntry);
    }
  /* Return */
  return(size);
  }

void ubixFSUnlink(char *path,struct vfs_mountPoint *mp) {
  int x=0;
  struct directoryEntry *dirEntry = (struct directoryEntry *)kmalloc(0x1000);
  struct ubixFSInfo *fsInfo = mp->fsInfo;
  
  mp->device->devInfo->read(mp->device->devInfo->info,dirEntry,(mp->diskLabel->partitions[mp->partition].pOffset+fsInfo->blockAllocationTable[fsInfo->rootDir].realSector),8);

  for (x=0;(uInt32)x<(4096/sizeof(struct directoryEntry));x++) {
    if ((int)!strcmp(dirEntry[x].fileName,path)) {
      dirEntry[x].attributes |= typeDeleted;
      dirEntry[x].fileName[0] = '?';
      mp->device->devInfo->write(mp->device->devInfo->info,dirEntry,(mp->diskLabel->partitions[mp->partition].pOffset+fsInfo->blockAllocationTable[fsInfo->rootDir].realSector),8);
      return;
      }
    }
  kprintf("File Not Found\n");
  return;
  }
  
  
/*****************************************************************************************

Function: static
          int ubixfs_loadData(fileDescriptor *fd,char *data,uInt32 size,uInt32 batIndex)

Description: This will load the node data in from the disk

Notes:
  07/23/2004 - This loads complete blocks from disk so it is aligned to 0x1000 not the
               actual file size

*****************************************************************************************/    
static int ubixfs_loadData(fileDescriptor *fd,char *data,uInt32 size,uInt32 batIndex) {
  uInt32 i = 0x0;

  struct ubixFSInfo *fsInfo = NULL;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->fsInfo);

  fsInfo = fd->mp->fsInfo;

  size = UBIXFS_ALIGN(size);
  /* Loop by block size */
  
  for (i=0x0; i<size; i += (UBIXFS_BLOCKSIZE_BYTES)) {
  /* Get next block if we are ready for it */
    if (i != 0x0)
      batIndex = fsInfo->blockAllocationTable[batIndex].nextBlock;
  /* Read data in from media */
    fd->mp->device->devInfo->read(fd->mp->device->devInfo->info,data+i,fd->mp->diskLabel->partitions[fd->mp->partition].pOffset+fsInfo->blockAllocationTable[batIndex].realSector,blockSize);
    }
  /* Return */
  return(0x0);
  }


/*****************************************************************************************

Function: int ubixfs_initialize()

Description: This will initialize a mount point it loads the BAT and Caches the rootDir

Notes:

*****************************************************************************************/  
int ubixfs_initialize(struct vfs_mountPoint *mp) {
  struct ubixFSInfo *fsInfo = 0x0;

  assert(mp);
  assert(mp->diskLabel);
  assert(mp->diskLabel->partitions);

  mp->fsInfo = (struct ubixFSInfo *)kmalloc(sizeof(struct ubixFSInfo));
  assert(mp->fsInfo);
  
  fsInfo = mp->fsInfo;
  fsInfo->rootDir = 0x0; /* Root directory is always 0x0 on the UbixFS */
  
  /*
   Check the disk label to ensure this is an UbixFS partition
  */
  if ((mp->diskLabel->magicNum == UBIXDISKMAGIC) &&  (mp->diskLabel->magicNum2 == UBIXDISKMAGIC)) {
    
    /* Allocate memory for BAT */
    fsInfo->blockAllocationTable = (struct blockAllocationTableEntry *)kmalloc(mp->diskLabel->partitions[mp->partition].pBatSize * 512);
    assert(fsInfo->blockAllocationTable);

    /* Set up the amount of BAT entries */
    fsInfo->batEntries = (mp->diskLabel->partitions[mp->partition].pBatSize*512) / sizeof(struct blockAllocationTableEntry);

    /* Read the BAT to memory */
    assert(mp->device->devInfo->read);
    mp->device->devInfo->read(mp->device->devInfo->info, 
                             fsInfo->blockAllocationTable,
                             mp->diskLabel->partitions[mp->partition].pOffset,
                             mp->diskLabel->partitions[mp->partition].pBatSize);
    
    /* Set up root directory cache */
    fsInfo->dirCache = ubixfs_cacheNew("/");
    assert(fsInfo->dirCache);
    fsInfo->dirCache->info = (struct directoryEntry *)kmalloc(0x4000);  /* allocate root dir */
    fsInfo->dirCache->present = 1;
    fsInfo->dirCache->size = kmalloc(sizeof(fsInfo->dirCache->size));
    fsInfo->dirCache->startCluster = kmalloc(sizeof(fsInfo->dirCache->startCluster));
    fsInfo->dirCache->attributes = kmalloc(sizeof(fsInfo->dirCache->attributes));
    fsInfo->dirCache->permissions = kmalloc(sizeof(fsInfo->dirCache->permissions));

    *fsInfo->dirCache->size = 0x4000;
    *fsInfo->dirCache->startCluster = fsInfo->rootDir;
    
    assert(fsInfo->dirCache->info);
    /* Read root dir in from disk it is always 0x4000 bytes long */
    mp->device->devInfo->read(mp->device->devInfo->info,
                              fsInfo->dirCache->info,
                              (mp->diskLabel->partitions[mp->partition].pOffset+fsInfo->blockAllocationTable[fsInfo->rootDir].realSector),
                              0x4000 / 512);

    /* Start our ubixfs_thread to manage the mount point */
    /*
    UBU disable for now
    execThread(ubixfs_Thread,(uInt32)(kmalloc(0x2000)+0x2000),0x0);
    */
    kprintf("  Offset: [%i], Partition: [%i]\n",
    mp->diskLabel->partitions[mp->partition].pOffset,mp->partition);
    kprintf("UbixFS Initialized\n");
    return(0x1);
    }
    
  kprintf("Not a valid UbixFS disk.\n");
  /* Return */
  return(0x0);
  }
  
/*****************************************************************************************

Function: int ubixfs_init()

Description: This is the master initialization for the Ubix File System it will make the
             OS UbixFS aware.
             It does not in any way shape or form connect a mount point an medium that is
             upto the ubixfs_initialize() function

Notes:

*****************************************************************************************/  
int ubixfs_init() {
  /* Set up our file system structure */
  struct fileSystem ubixFileSystem = 
   {NULL,                         /* prev        */
    NULL,                         /* next        */
    (void *)ubixfs_initialize,    /* vfsInitFS   */
    (void *)readUbixFS,           /* vfsRead     */
    (void *)writeUbixFS,          /* vfsWrite    */
    (void *)openFileUbixFS,       /* vfsOpenFile */
    (void *)ubixFSUnlink,         /* vfsUnlink   */
    (void *)ubixFSmkDir,          /* vfsMakeDir  */
    NULL,                         /* vfsRemDir   */
    NULL,                         /* vfsSync     */
    0                             /* vfsType     */
   }; /* ubixFileSystem */

  /* Add UbixFS */
  if (vfsRegisterFS(ubixFileSystem) != 0x0) {
    kpanic("Unable To Enable UbixFS");
    return(0x1);
    }

  /* Return */
  return(0x0);
  }

/***
 $Log: ubixfs.c,v $
 Revision 1.2  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:42  reddawg
 no message

 Revision 1.44  2004/08/26 22:51:19  reddawg
 TCA touched me :( i think he likes men....


 sched.h:        kTask_t added parentPid
 endtask.c:     fixed term back to parentPid
 exec.c:          cleaned warnings
 fork.c:            fixed term to childPid
 sched.c:         clean up for dead tasks
 systemtask.c: clean up dead tasks
 kmalloc.c:       cleaned up warnings
 udp.c:            cleaned up warnings
 bot.c:             cleaned up warnings
 shell.c:           cleaned up warnings
 tcpdump.c:     took a dump
 hd.c:             cleaned up warnings
 ubixfs.c:        stopped prning debug info

 Revision 1.43  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.42  2004/08/09 12:58:05  reddawg
 let me know when you got the surce

 Revision 1.41  2004/08/01 17:58:39  flameshadow
 chg: fixed string allocation bug in ubixfs_cacheNew()

 Revision 1.40  2004/07/28 17:07:29  flameshadow
 chg: re-added moving cached nodes to the front of the list when found
 add: added an assert() in ubixfs.c

 Revision 1.39  2004/07/27 19:24:31  flameshadow
 chg: reduced the number of debugging statements in the kernel.

 Revision 1.38  2004/07/27 12:02:01  reddawg
 chg: fixed marks bug readFile did a lookup which is why it looked like it was loopping so much

 Revision 1.37  2004/07/27 09:05:43  flameshadow
 chg: fixed file not found bug. Still can't find looping issue

 Revision 1.36  2004/07/27 04:05:20  flameshadow
 chg: kinda fixed it. Added bunches of debug info

 Revision 1.35  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.34  2004/07/24 23:04:44  reddawg
 Changes... mark let me know if you fault at pid 185 when you type stress

 Revision 1.33  2004/07/23 09:10:06  reddawg
 ubixfs: cleaned up some functions played with the caching a bit
 vfs:    renamed a bunch of functions
 cleaned up a few misc bugs

 Revision 1.32  2004/07/22 23:01:51  reddawg
 Ok checking in before I sleep

 Revision 1.31  2004/07/22 22:37:03  reddawg
 Caching is working now the FS is extremely fast but needs to be optimized to do 32bit copies over 8bit

 Revision 1.30  2004/07/22 19:01:59  flameshadow
 chg: more directory and file caching

 Revision 1.29  2004/07/22 16:34:32  flameshadow
 add: file and dir caching kinda work

 Revision 1.28  2004/07/21 22:07:18  flameshadow
 chg: renamed caching functions (again)

 Revision 1.27  2004/07/21 21:08:05  flameshadow
 add: added provisions for file caching

 Revision 1.26  2004/07/20 23:21:31  flameshadow
 syncing

 Revision 1.25  2004/07/20 21:39:53  reddawg
 ubixfs: does propper caching now problem was you did not seek realSector however the os starts but I am getting a segfault could be from anything haven't looked into it right quick

 Revision 1.24  2004/07/20 21:38:08  flameshadow
 try now


 Revision 1.23  2004/07/20 21:35:09  reddawg
 Let me commit before we start to overlap

 Revision 1.22  2004/07/20 21:28:16  flameshadow
 oops

 Revision 1.20  2004/07/20 19:36:49  reddawg
 UBU Tags

 Revision 1.19  2004/07/20 18:09:37  flameshadow
 add: directory caching related stuff

 Revision 1.18  2004/07/17 03:21:34  flameshadow
 chg: cleaned up code; added assert() statements

 Revision 1.15  2004/07/14 12:21:49  reddawg
 ubixfs: enableUbixFs to ubixfs_init
 Changed Startup Routines

 Revision 1.14  2004/06/28 23:12:58  reddawg
 file format now container:/path/to/file

 Revision 1.13  2004/06/28 18:12:44  reddawg
 We need these files

 Revision 1.12  2004/06/28 11:57:58  reddawg
 Fixing Up Filesystem

 Revision 1.10  2004/06/04 13:20:22  reddawg
 ubixFSmkDir(): played with it a bit to see if it still worked

 Revision 1.9  2004/06/04 10:19:42  reddawg
 notes: we compile again, thank g-d anyways i was about to cry

 Revision 1.8  2004/06/01 00:04:53  reddawg
 Try now mark

 Revision 1.7  2004/05/19 15:20:06  reddawg
 Fixed reference problems due to changes in drive subsystem

 Revision 1.6  2004/05/19 04:07:43  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.5  2004/04/29 15:45:19  reddawg
 Fixed some bugs so now the automade images will work correctly

 Revision 1.4  2004/04/28 21:10:40  reddawg
 Lots Of changes to make it work with existing os

 Revision 1.3  2004/04/28 13:33:09  reddawg
 Overhaul to ubixfs and boot loader and MBR to work well with our schema
 now BAT and dir and file entries are all offset 64Sectors from the start of the partition

 Revision 1.2  2004/04/28 02:22:55  reddawg
 This is a fiarly large commit but we are starting to use new driver model
 all around

 Revision 1.1.1.1  2004/04/15 12:07:08  reddawg
 UbixOS v1.0

 Revision 1.31  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/

