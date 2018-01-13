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

#include <sys/types.h>

#define	DOSPTYP_UBX	 0x2A	/* UbixFS partition type */
#define UBIXDISKMAGIC    ((uInt32)0x45) /* The disk magic number */
#define MAXUBIXPARTITIONS 16
#define UBIXFSMAGIC      ((uInt32)0x69) /* The File System Magic Number */

struct ubixDiskLabel {
    uInt32 magicNum;
    uInt32 magicNum2;
    uInt16 driveType;
    uInt16 numPartitions;
    struct ubixPartitions { /* the partition table */
        uInt32 p_size; /* number of sectors in partition */
        uInt32 p_offset; /* starting sector */
        uInt32 p_fsize; /* filesystem basic fragment size */
        uInt32 p_bsize; /* BAT size */
        uInt8 p_fstype; /* filesystem type, see below */
        uInt8 p_frag; /* filesystem fragments per block */
    } partitions[MAXUBIXPARTITIONS];
};

//Block Allocation Table Entry
struct blockAllocationTableEntry {
    long attributes; //Block Attributes
    long realSector; //Real Sector
    long nextBlock;  //Sector Of Next Block
    long reserved;   //Reserved
};

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

/***
 $Log: ubixfs.h,v $
 Revision 1.2  2006/10/26 23:52:26  reddawg
 Fixens

 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:13:59  reddawg
 no message

 Revision 1.2  2004/05/24 13:54:52  reddawg
 Clean Up


 END
 ***/
