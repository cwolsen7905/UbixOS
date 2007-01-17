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

 $Id$

*****************************************************************************************/

#ifndef _MOUNT_H
#define _MOUNT_H

#include <ubixos/types.h>

#define MFSNAMELEN      16              /* length of type name including null */
#define MNAMELEN        88              /* size of on/from name bufs */

typedef struct fsid { int32_t val[2]; } fsid_t;        /* filesystem id type */

struct statfs {
  u_int32_t f_version;             /* structure version number */
  u_int32_t f_type;                /* type of filesystem */
  u_int64_t f_flags;               /* copy of mount exported flags */
  u_int64_t f_bsize;               /* filesystem fragment size */
  u_int64_t f_iosize;              /* optimal transfer block size */
  u_int64_t f_blocks;              /* total data blocks in filesystem */
  u_int64_t f_bfree;               /* free blocks in filesystem */
  int64_t  f_bavail;              /* free blocks avail to non-superuser */
  u_int64_t f_files;               /* total file nodes in filesystem */
  int64_t  f_ffree;               /* free nodes avail to non-superuser */
  u_int64_t f_syncwrites;          /* count of sync writes since mount */
  u_int64_t f_asyncwrites;         /* count of async writes since mount */
  u_int64_t f_syncreads;           /* count of sync reads since mount */
  u_int64_t f_asyncreads;          /* count of async reads since mount */
  u_int64_t f_spare[10];           /* unused spare */
  u_int32_t f_namemax;             /* maximum filename length */
  uid_t     f_owner;              /* user that mounted the filesystem */
  fsid_t    f_fsid;               /* filesystem id */
  char      f_charspare[80];          /* spare string space */
  char      f_fstypename[MFSNAMELEN]; /* filesystem type name */
  char      f_mntfromname[MNAMELEN];  /* mounted filesystem */
  char      f_mntonname[MNAMELEN];    /* directory on which mounted */
  };

struct vfs_mountPoint {
  struct vfs_mountPoint   *prev;
  struct vfs_mountPoint   *next;
  struct fileSystem       *fs;
  struct device_node      *device;
  struct ubixDiskLabel   *diskLabel;
  void               *fsInfo;
  int                partition;
  char               mountPoint[1024];
  char               perms;
  };

int vfs_mount(int major,int minor,int partition,int fsType,char *mountPoint,char *perms);
int vfs_addMount(struct vfs_mountPoint *mp);
struct vfs_mountPoint *vfs_findMount(char *mountPoint);

#endif

/***
 END
 ***/
