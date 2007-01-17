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

#ifndef _VFS_H
#define _VFS_H

#include <ubixos/types.h>
#include <vfs/file.h>
#include <vfs/mount.h>
#include <sys/sysproto.h>
#include <sys/thread.h>

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
  int               (*vfsInitFS)(void *); /*!< pointer to inialization routine */
  int               (*vfsRead)(void *,char *,long,long); /*!< pointer to read routine */
  int               (*vfsWrite)(void *,char *,long,long); /*!< pointer to write routine */
  int               (*vfsOpenFile)(void *,void *); /*!< pointer to openfile routine */
  int               (*vfsUnlink)(char *,void *); /*!< pointer to unlink routine */
  int               (*vfsMakeDir)(char *,void *); /*!< pointer to makedir routine */
  int               (*vfsRemDir)(char *); /*!< pointer to remdir routine */
  int               (*vfsSync)(void); /*!< pointer to sync routine */
  int               vfsType; /*!< vfs type id */
  };


/* VFS Functions */
int vfs_init();
int vfsRegisterFS(struct fileSystem);
struct fileSystem *vfs_findFS(int);

#endif

/***
 END
 ***/
