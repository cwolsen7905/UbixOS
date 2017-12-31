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

 $Id: ubixfs.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _UBIXFS_H
#define _UBIXFS_H

#include <sys/types.h>
#include <vfs/vfs.h>
#include <sys/device.h>
#include <mpi/mpi.h>
#include <ubixfs/dirCache.h>


#define UBIXFS_BLOCKSIZE_BYTES     blockSize*512
#define UBIXFS_ALIGN(size) (size + ((((size) % (UBIXFS_BLOCKSIZE_BYTES)) == 0)? 0 : ((UBIXFS_BLOCKSIZE_BYTES) - ((size) % (UBIXFS_BLOCKSIZE_BYTES)))))

#define UBIXDISKMAGIC     ((uInt32)0x45) /* The disk magic number */
#define MAXUBIXPARTITIONS 16
#define blockSize         8


#define EOBC              -1


#define typeFile      1
#define typeContainer 2
#define typeDirectory 4
#define typeDeleted   8

/* Start */
struct directoryList {
  char                  dirName[256];
  char                 *dirCache;
  uInt32                dirBlock;
  struct directoryList *next;
  struct directoryList *prev;
  };

typedef struct directoryList * dirList_t;

dirList_t ubixFSLoadDir(char *);
/* End   */

//Partition Information
struct ubixDiskLabel {
  uInt32 magicNum;
  uInt32 magicNum2;
  uInt16 driveType;
  uInt16 numPartitions;
  struct  ubixPartitions {  //the partition table
    uInt32 pSize;            //number of sectors in partition
    uInt32 pOffset;          //starting sector
    uInt32 pFsSize;          //filesystem basic fragment size
    uInt32 pBatSize;         //BAT size
    uInt8 pFsType;          //filesystem type, see below
    uInt8 pFrag;            //filesystem fragments per block
    } partitions[MAXUBIXPARTITIONS];
  };


struct partitionInformation {
  uInt32 size;                 //Size In Sectors
  uInt32 startSector;          //Base Sector Of Partition
  uInt32 blockAllocationTable; //Base Sector Of BAT
  uInt32 rootDirectory;        //Base Sector Of Root Directory
  };

//Block Allocation Table Entry
struct blockAllocationTableEntry {
  long attributes; //Block Attributes
  long realSector; //Real Sector  
  long nextBlock;  //Sector Of Next Block
  long reserved;   //Reserved
  };

//UbixFS Directory Entry
struct directoryEntry {
  uInt32  startCluster;   //Starting Cluster Of File
  uInt32  size;           //Size Of File
  uInt32  creationDate;  //Date Created
  uInt32  lastModified;  //Date Last Modified
  uInt32  uid;           //UID Of Owner
  uInt32  gid;           //GID Of Owner
  uInt16 attributes;    //Files Attributes
  uInt16 permissions;   //Files Permissions
  char   fileName[256]; //File Name
  };

struct bootSect {
  uInt8 jmp[4];
  uInt8 id[6];
  uInt16 version;
  uInt16 tmp;
  uInt16 fsStart;
  uInt16 tmp2;
  uInt32 krnl_start;
  uInt BytesPerSector;
  uInt SectersPerTrack;
  uInt TotalHeads;
  uInt32 TotalSectors;
  uInt8 code[479];
  };  

struct ubixFSInfo {
  struct blockAllocationTableEntry *blockAllocationTable;
  struct cacheNode * dirCache;
  uInt32 batEntries;
  uInt32 rootDir;
}; /* ubixFSInfo */

int readFile(char *file);
int writeFileByte(int ch,fileDescriptor *fd,long offset);
//int openFileUbixFS(char *file,fileDescriptor *fd);
int getFreeBlocks(int count,fileDescriptor *fd);
//extern struct ubixDiskLabel *diskLabel;

//Good Functions
//void initUbixFS(struct mountPoints *mp);

int readUbixFS(fileDescriptor *fd,char *data,uInt32,long size);
int writeUbixFS(fileDescriptor *fd,char *data,long offset,long size);
void syncBat(struct vfs_mountPoint *mp);
int freeBlocks(int block,fileDescriptor *fd);
int addDirEntry(struct directoryEntry *dir,fileDescriptor *fd);
void ubixFSUnlink(char *path,struct vfs_mountPoint *mp);
int ubixFSmkDir(char *dir,fileDescriptor *fd);

int  ubixfs_init();
int  ubixfs_initialize();
void ubixfs_thread();


#endif

/***
 $Log: ubixfs.h,v $
 Revision 1.2  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:53  reddawg
 no message

 Revision 1.21  2004/09/14 20:57:01  reddawg
 Bug fixes: macro problem over opt a multiply

 Revision 1.20  2004/08/01 17:58:39  flameshadow
 chg: fixed string allocation bug in ubixfs_cacheNew()

 Revision 1.19  2004/07/27 12:02:01  reddawg
 chg: fixed marks bug readFile did a lookup which is why it looked like it was loopping so much

 Revision 1.18  2004/07/23 09:10:06  reddawg
 ubixfs: cleaned up some functions played with the caching a bit
 vfs:    renamed a bunch of functions
 cleaned up a few misc bugs

 Revision 1.17  2004/07/22 22:37:03  reddawg
 Caching is working now the FS is extremely fast but needs to be optimized to do 32bit copies over 8bit

 Revision 1.16  2004/07/20 21:28:16  flameshadow
 oops

 Revision 1.14  2004/07/20 19:36:49  reddawg
 UBU Tags

 Revision 1.13  2004/07/14 12:21:49  reddawg
 ubixfs: enableUbixFs to ubixfs_init
 Changed Startup Routines

 END
 ***/
