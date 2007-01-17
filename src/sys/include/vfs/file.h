/**************************************************************************************
 Copyright (c) 2002 The UbixOS Project
 All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, the following disclaimer and the list of authors.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, the following disclaimer and the list of authors
in the documentation and/or other materials provided with the distribution. Neither the name of the UbixOS Project nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

**************************************************************************************/

#ifndef _FILE_H
#define _FILE_H

#include <ubixos/types.h>
#include <ubixfs/dirCache.h>
#include <vfs/mount.h>

#define SEEK_SET 0x0

#define VBLKSHIFT       12
#define VBLKSIZE        (1 << VBLKSHIFT)
#define SBLOCKSIZE      8192
#define      DEV_BSHIFT      9               /* log2(DEV_BSIZE) */
#define      DEV_BSIZE       (1<<DEV_BSHIFT)

struct dmadat {
  char blkbuf[VBLKSIZE];  /* filesystem blocks */
  char indbuf[VBLKSIZE];  /* indir blocks */
  char sbbuf[SBLOCKSIZE]; /* superblock */
  char secbuf[DEV_BSIZE]; /* for MBR/disklabel */
  };

typedef struct fileDescriptorStruct {
  struct fileDescriptorStruct *prev;
  struct fileDescriptorStruct *next;
  struct vfs_mountPoint       *mp;
  uInt16                       status;
  uInt16                       mode;
  uInt32                       offset;
  uInt32                       size;
  uInt16                       length;
  uInt32                       start;
  char                         fileName[512];
  char                        *buffer;
  uInt32                       ino;
  struct cacheNode            *cacheNode;
  uInt32                       perms;
  struct dmadat               *dmadat;
  int                          dsk_meta;
  uInt32                       resid;
  } fileDescriptor;


typedef struct userFileDescriptorStruct {
  struct fileDescriptorStruct *fd;
  uInt32                       fdSize;
  } userFileDescriptor;

extern fileDescriptor *fdTable;

fileDescriptor *fopen(const char *,const char *);
int             fclose(fileDescriptor *);

/* UBU */


int unlink(const char *path);
int feof(fileDescriptor *fd);
int fgetc(fileDescriptor *fd);
size_t fread(void *ptr,size_t size,size_t nmemb,fileDescriptor *fd);
size_t fwrite(void *ptr,int size,int nmemb,fileDescriptor *fd);
int fseek(fileDescriptor *,long,int);

void sysFseek(userFileDescriptor *,long,int);

//Good
void sysChDir(const char *path);
void chDir(const char *path);
char *verifyDir(const char *path);

#endif
