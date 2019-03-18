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

#ifndef _UBIXFS_H
#define _UBIXFS_H

typedef unsigned long uLong;
typedef unsigned short uShort;
typedef unsigned char uChar;
typedef unsigned int uInt;

//Partition Information
struct partitionInformation {
    uLong size;                 //Size In Sectors
    uLong startSector;          //Base Sector Of Partition
    uLong blockAllocationTable; //Base Sector Of BAT
    uLong rootDirectory;        //Base Sector Of Root Directory
    uLong rootDirectorySize; /* Size In Sectors Of Root Directory */
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
    uLong startCluster;  //Starting Cluster Of File
    uLong size;          //Size Of File
    uLong creationDate;  //Date Created
    uLong lastModified;  //Date Last Modified
    uLong uid;           //UID Of Owner
    uLong gid;           //GID Of Owner
    uShort attributes;    //Files Attributes
    uShort permissions;   //Files Permissions
    char fileName[256]; //File Name
};

struct bootSect {
    uChar jmp[4];
    uChar id[6];
    uShort version;
    uShort tmp;
    uShort fsStart;
    uShort tmp2;
    uLong krnl_start;
    uInt BytesPerSector;
    uInt SectersPerTrack;
    uInt TotalHeads;
    uLong TotalSectors;
    uChar code[479];
};

#endif
