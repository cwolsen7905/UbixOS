/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UBIXFS_UBIXFS_H
#define _UBIXFS_UBIXFS_H

#include <sys/types.h>
#include <sys/vfs/vfs.h>
#include <sys/device.h>
#include <mpi/mpi.h>
#include <fs/ubixfs/dirCache.h>

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
    char dirName[256];
    char *dirCache;
    uInt32 dirBlock;
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
    struct ubixPartitions {  //the partition table
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
    uInt32 startCluster;   //Starting Cluster Of File
    uInt32 size;           //Size Of File
    uInt32 creationDate;  //Date Created
    uInt32 lastModified;  //Date Last Modified
    uInt32 uid;           //UID Of Owner
    uInt32 gid;           //GID Of Owner
    uInt16 attributes;    //Files Attributes
    uInt16 permissions;   //Files Permissions
    char fileName[256]; //File Name
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
};
/* ubixFSInfo */

int readFile(char *file);
int writeFileByte(int ch, fileDescriptor_t *fd, long offset);
//int openFileUbixFS(char *file,fileDescriptor_t *fd);
int getFreeBlocks(int count, fileDescriptor_t *fd);
//extern struct ubixDiskLabel *diskLabel;

//Good Functions
//void initUbixFS(struct mountPoints *mp);

int readUbixFS(fileDescriptor_t *fd, char *data, uInt32, long size);
int writeUbixFS(fileDescriptor_t *fd, char *data, long offset, long size);
void syncBat(struct vfs_mountPoint *mp);
int freeBlocks(int block, fileDescriptor_t *fd);
int addDirEntry(struct directoryEntry *dir, fileDescriptor_t *fd);
void ubixFSUnlink(char *path, struct vfs_mountPoint *mp);
int ubixFSmkDir(char *dir, fileDescriptor_t *fd);

int ubixfs_init();
int ubixfs_initialize();
void ubixfs_thread();

#endif /* END _UBIXFS_UBIXFS_H */
