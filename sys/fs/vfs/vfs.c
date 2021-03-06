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

#include <vfs/vfs.h>
#include <ubixos/vitals.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>
#include <sys/descrip.h>

/************************************************************************

Function: void vfs_init();

Description: This Function Initializes The VFS Layer

Notes:

02/20/2004 - Approved for quality

************************************************************************/
int vfs_init() {
  /* Set up default fileSystems list */
  systemVitals->fileSystems = 0x0;

  /* Print information */
  kprintf("vfs0: loaded at address: [0x%X]\n",systemVitals->fileSystems);

  /* Return so we know things went well */
  return(0x0);
  }

struct fileSystem *vfsFindFS(int vfsType) {
  struct fileSystem *tmp = 0x0;

  /* Search For File System */
  for (tmp=systemVitals->fileSystems;tmp;tmp=tmp->next) {
    /* If Found Return File System */
    if (tmp->vfsType == vfsType) {
      return(tmp);
      }
    }

  /* If FS Not Found Return NULL */
  return(0x0);
  }

/*!
 * \brief register a file system
 *
 * This registers a new filesystem into the vfs which is referenced when trying to mount a device
 *
 * \param newFS pointer to fileSystem structure
 */
int vfsRegisterFS(struct fileSystem newFS) {
/*
int vfsType,
void *vfsInitFS,
void *vfsRead,
void *vfsWrite,
void *vfsOpenFile,
void *vfsUnlink,
void *vfsMakeDir,
void *vfsRemDir,
void *vfsSync) {
 */
  struct fileSystem *tmpFs = 0x0;

  if (vfsFindFS(newFS.vfsType) != 0x0) {
    kprintf("FS Is already Registered\n");
    return(0x1);
    }

  /* Allocate Memory */
  tmpFs = (struct fileSystem *)kmalloc(sizeof(struct fileSystem));
  if (tmpFs == NULL) {
	kprintf("vfsRegisterFS: memory allocation failed\n");
    /* Memory Allocation Failed */
    return(0x1);
    }

  /* Set Up FS Defaults */

/* 2004 7-16-2004 mji
 * Old method:
 * tmpFs->vfsType       = newFS.vfsType;
 * tmpFs->vfsInitFS     = newFS.vfsInitFS;
 * tmpFs->vfsRead       = newFS.vfsRead;
 * tmpFs->vfsWrite      = newFS.vfsWrite;
 * tmpFs->vfsOpenFile   = newFS.vfsOpenFile;
 * tmpFs->vfsUnlink     = newFS.vfsUnlink;
 * tmpFs->vfsMakeDir    = newFS.vfsMakeDir;
 * tmpFs->vfsRemDir     = newFS.vfsRemDir;
 * tmpFs->vfsSync       = newFS.vfsSync;
 */
 /* new method: */

  memcpy(tmpFs, &newFS, sizeof(struct fileSystem));
  if (!systemVitals->fileSystems) {
    tmpFs->prev               = 0x0;
    tmpFs->next               = 0x0;
    systemVitals->fileSystems = tmpFs;
    }
  else {
    tmpFs->prev                     = 0x0;
    tmpFs->next                     = systemVitals->fileSystems;
    systemVitals->fileSystems->prev = tmpFs;
    systemVitals->fileSystems       = tmpFs;
    }

  return(0x0);
  }
