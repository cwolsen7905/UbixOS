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
#ifndef _VFS_VFS_H
#define _VFS_VFS_H

#include <sys/types.h>
#include <vfs/file.h>
#include <vfs/mount.h>
#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <net/net.h>
#include <ubixos/wait.h>

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define maxFd   32
#define fdAvail 1
#define fdOpen  2
#define fdRead  3
#define fdEof   4

#define fileRead    0x0001
#define fileWrite   0x0002
#define fileBinary  0x0004
#define fileAppend  0x0008

/*!
 \brief filesSystem Structure

 not sure if we should allow function to point to NULL
 */
struct fileSystem {
    struct fileSystem *prev;
    struct fileSystem *next;
    int (*vfsInitFS)(void*); /*!< pointer to inialization routine */
    int (*vfsRead)(void*, char*, off_t, long); /*!< pointer to read routine */
    int (*vfsWrite)(void*, char*, off_t, long); /*!< pointer to write routine */
    int (*vfsOpenFile)(void*, void*); /*!< pointer to openfile routine */
    int (*vfsUnlink)(char*, void*); /*!< pointer to unlink routine */
    int (*vfsMakeDir)(char*, void*); /*!< pointer to makedir routine */
    int (*vfsRemDir)(char*); /*!< pointer to remdir routine */
    int (*vfsSync)(void); /*!< pointer to sync routine */
    int vfsType; /*!< vfs type id */
};

struct inode_operations {
    struct file_operations *default_file_ops;
    int (*create)(struct inode*, const char*, int, int, struct inode**);
    int (*lookup)(struct inode*, const char*, int, struct inode**);
    int (*link)(struct inode*, struct inode*, const char*, int);
    int (*unlink)(struct inode*, const char*, int);
    int (*symlink)(struct inode*, const char*, int, const char*);
    int (*mkdir)(struct inode*, const char*, int, int);
    int (*rmdir)(struct inode*, const char*, int);
    int (*mknod)(struct inode*, const char*, int, int, int);
    int (*rename)(struct inode*, const char*, int, struct inode*, const char*, int);
    int (*readlink)(struct inode*, char*, int);
    int (*follow_link)(struct inode*, struct inode*, int, int, struct inode**);
    int (*bmap)(struct inode*, int);
    void (*truncate)(struct inode*);
    int (*permission)(struct inode*, int);
};

/* VFS Functions */
int vfs_init();
int vfsRegisterFS(struct fileSystem);
struct fileSystem* vfs_findFS(int);

struct super_operations {
    void (*read_inode)(struct inode*);
    int (*notify_change)(int flags, struct inode*);
    void (*write_inode)(struct inode*);
    void (*put_inode)(struct inode*);
    void (*put_super)(struct super_block*);
    void (*write_super)(struct super_block*);
    void (*statfs)(struct super_block*, struct statfs*);
    int (*remount_fs)(struct super_block*, int*, char*);
};

struct super_block {
    __dev_t s_dev;
    unsigned long s_blocksize;
    unsigned char s_blocksize_bits;
    unsigned char s_lock;
    unsigned char s_rd_only;
    unsigned char s_dirt;
    struct super_operations *s_op;
    unsigned long s_flags;
    unsigned long s_magic;
    unsigned long s_time;
    struct inode *s_covered;
    struct inode *s_mounted;
    struct wait_queue *s_wait;
    union {
        struct msdos_sb_info msdos_sb;
        /*
         struct fs ufs1_sb;
         struct fs ufs2_sb;
         */
    } u;
};

#endif
