/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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
#include <fs/vfs/vfs.h>
#include <sys/bus.h>
#include <mpi/mpi.h>
#include <fs/ubixfs/dir_cache.h>

#define UBIXFS_BLOCKSIZE_BYTES     blockSize*512
#define UBIXFS_ALIGN(size) (size + ((((size) % (UBIXFS_BLOCKSIZE_BYTES)) == 0)? 0 : ((UBIXFS_BLOCKSIZE_BYTES) - ((size) % (UBIXFS_BLOCKSIZE_BYTES)))))

#define UBIXDISKMAGIC     ((u_int32_t)0x45) /* The disk magic number */
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
    u_int32_t dirBlock;
    struct directoryList *next;
    struct directoryList *prev;
};

typedef struct directoryList * dirList_t;

dirList_t ubixFSLoadDir(char *);
/* End   */

//Partition Information
struct ubixDiskLabel {
    u_int32_t magicNum;
    u_int32_t magicNum2;
    u_int16_t driveType;
    u_int16_t numPartitions;
    struct ubixPartitions {  //the partition table
        u_int32_t pSize;            //number of sectors in partition
        u_int32_t pOffset;          //starting sector
        u_int32_t pFsSize;          //filesystem basic fragment size
        u_int32_t pBatSize;         //BAT size
        u_int8_t pFsType;          //filesystem type, see below
        u_int8_t pFrag;            //filesystem fragments per block
    } partitions[MAXUBIXPARTITIONS];
};

struct partitionInformation {
    u_int32_t size;                 //Size In Sectors
    u_int32_t startSector;          //Base Sector Of Partition
    u_int32_t blockAllocationTable; //Base Sector Of BAT
    u_int32_t rootDirectory;        //Base Sector Of Root Directory
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
    u_int32_t startCluster;   //Starting Cluster Of File
    u_int32_t size;           //Size Of File
    u_int32_t creationDate;  //Date Created
    u_int32_t lastModified;  //Date Last Modified
    u_int32_t uid;           //UID Of Owner
    u_int32_t gid;           //GID Of Owner
    u_int16_t attributes;    //Files Attributes
    u_int16_t permissions;   //Files Permissions
    char fileName[256]; //File Name
};

struct bootSect {
    u_int8_t jmp[4];
    u_int8_t id[6];
    u_int16_t version;
    u_int16_t tmp;
    u_int16_t fsStart;
    u_int16_t tmp2;
    u_int32_t krnl_start;
    u_int32_t BytesPerSector;
    u_int32_t SectersPerTrack;
    u_int32_t TotalHeads;
    u_int32_t TotalSectors;
    u_int8_t code[479];
};

struct ubixFSInfo {
    struct blockAllocationTableEntry *blockAllocationTable;
    struct cacheNode * dirCache;
    u_int32_t batEntries;
    u_int32_t rootDir;
};
/* ubixFSInfo */

int readFile(char *file);
int writeFileByte(int ch, fileDescriptor_t *fd, long offset);
//int openFileUbixFS(char *file,fileDescriptor_t *fd);
int getFreeBlocks(int count, fileDescriptor_t *fd);
//extern struct ubixDiskLabel *diskLabel;

//Good Functions
//void initUbixFS(struct mountPoints *mp);

int readUbixFS(fileDescriptor_t *fd, char *data, off_t, long size);
int writeUbixFS(fileDescriptor_t *fd, char *data, long offset, long size);
void syncBat(struct vfs_mountPoint *mp);
int freeBlocks(int block, fileDescriptor_t *fd);
int addDirEntry(struct directoryEntry *dir, fileDescriptor_t *fd);
void ubixFSUnlink(char *path, struct vfs_mountPoint *mp);
int ubixFSmkDir(char *dir, fileDescriptor_t *fd);

int ubixfs_init();
int ubixfs_initialize(struct vfs_mountPoint *mp);
void ubixfs_thread(struct vfs_mountPoint *mp);

#endif /* END _UBIXFS_UBIXFS_H */
