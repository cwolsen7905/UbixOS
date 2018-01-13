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

#ifndef _VFS_FILE_H_
#define _VFS_FILE_H_ 1

#include <sys/types.h>

#include <ubixfs/dirCache.h>
#include <sys/thread.h>
#include <vfs/inode.h>
#include <vfs/mount.h>
#include <ufs/ufs.h>

/* HACK */
int getchar();

#define SEEK_SET 0x0

#define VBLKSHIFT       12
#define VBLKSIZE        (1 << VBLKSHIFT)
#define SBLOCKSIZE      8192
#define DEV_BSHIFT      9               /* log2(DEV_BSIZE) */
#define DEV_BSIZE       (1<<DEV_BSHIFT)

#define MAX_OFILES 256

struct dmadat {
    char blkbuf[VBLKSIZE]; /* filesystem blocks */
    char indbuf[VBLKSIZE]; /* indir blocks */
    char sbbuf[SBLOCKSIZE]; /* superblock */
    char secbuf[DEV_BSIZE]; /* for MBR/disklabel */
};

typedef struct fileDescriptorStruct {
    struct fileDescriptorStruct *prev;
    struct fileDescriptorStruct *next;
    struct vfs_mountPoint *mp;
    uint32_t ino;
    uint16_t status;
    uint16_t mode;
    uint32_t offset;
    uint32_t size;
    uint16_t length;
    uint32_t start;
    char fileName[512];
    char *buffer;
    struct cacheNode *cacheNode;
    uint32_t perms;
    struct dmadat *dmadat;
    int dsk_meta;
    uint32_t resid;
    struct inode inode;
} fileDescriptor;

typedef struct userFileDescriptorStruct {
    struct fileDescriptorStruct *fd;
    uint32_t fdSize;
} userFileDescriptor;

extern fileDescriptor *fdTable;

fileDescriptor *fopen(const char *, const char *);
int fclose(fileDescriptor *);

/* UBU */
struct sys_fwrite_args {
    void *buf;
    size_t nbytes;
    size_t nmemb;
    userFileDescriptor *fd;
};

int unlink(const char *path);
int feof(fileDescriptor *fd);
int fgetc(fileDescriptor *fd);
size_t fread(void *ptr, size_t size, size_t nmemb, fileDescriptor *fd);
size_t fwrite(void *ptr, int size, int nmemb, fileDescriptor *fd);
int fseek(fileDescriptor *, long, int);

int sysFseek(userFileDescriptor *, long, int);

//Good
//int sysChDir(const char *path);
//void chDir(const char *path);
char *verifyDir(const char *path);

/* New Syscall */
int sys_fwrite(struct thread *, struct sys_fwrite_args *);

#endif
