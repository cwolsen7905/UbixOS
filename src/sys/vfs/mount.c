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

#include <vfs/mount.h>
#include <ubixos/vitals.h>
#include <ubixos/kpanic.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <lib/string.h>
#include <sys/device.h>

/************************************************************************

Function: mount(int driveId,int partition,int fsType,char *mountPoint,char *perms)

Description: mount adds a mount point and returns 0 if successful 1 if it fails

Notes:

************************************************************************/
int vfs_mount(int major,int minor,int partition,int vfsType,char *mountPoint,char *perms) {
  struct vfs_mountPoint *mp     = 0x0;
  struct device_node  *device = 0x0;

  /* Allocate Memory For Mount Point */
  if ((mp = (struct vfs_mountPoint *)kmalloc(sizeof(struct vfs_mountPoint))) == NULL)
    kprintf("vfs_mount: failed to allocate mp\n");

  /* Copy Mount Point Into Buffer */
  sprintf(mp->mountPoint,mountPoint);
  
  /* Set Pointer To Physical Drive */
  device = device_find(major,minor);

  /* Set Up Mp Defaults */
  mp->device    = device;
  mp->fs        = vfsFindFS(vfsType);
  mp->partition = partition;
  mp->perms     = *perms;

  if (mp->fs == 0x0) {
    /* sysErr(systemErr,"File System Type: %i Not Found\n",fsType); */
    kprintf("File System Type: %i Not Found\n",vfsType);
    return(0x1);
    }
  /*What is this for? 10/6/2006 */
/*
  if (device != 0x0) {
    mp->diskLabel = (struct ubixDiskLabel *)kmalloc(512);
    mp->device->devInfo->read(mp->device->devInfo->info,mp->diskLabel,1,1);
    kprintf("READING SECTOR");
    }
*/

  /* Add Mountpoint If It Fails Free And Return */
  if (vfs_addMount(mp) != 0x0) {
    kfree(mp);
    return(0x1);
    }

  /* Initialize The File System If It Fails Return */
  if (mp->fs->vfsInitFS(mp) == 0x0) {
    kfree(mp);
    return(0x1);
    }
  /* Return */
  return(0x0);
  }

/************************************************************************

Function: vfs_addMount(struct vfs_mountPoint *mp)

Description: This function adds a mount point to the system

Notes:

************************************************************************/
int vfs_addMount(struct vfs_mountPoint  *mp) {

  /* If There Are No Existing Mounts Make It The First */
  if (systemVitals->mountPoints == 0x0) {
    mp->prev               = 0x0;
    mp->next               = 0x0;
    systemVitals->mountPoints = mp;
    }
  else {
    mp->next                        = systemVitals->mountPoints;
    systemVitals->mountPoints->prev = mp;
    mp->prev                        = 0x0;
    systemVitals->mountPoints       = mp;
    }
  /* Return */
  return(0x0);
  }

/************************************************************************

Function: vfs_findMount(char *mountPoint)

Description: This function finds a particular mount point

Notes:

************************************************************************/
struct vfs_mountPoint *vfs_findMount(char *mountPoint) {
  struct vfs_mountPoint *tmpMp = 0x0;

  for (tmpMp=systemVitals->mountPoints;tmpMp;tmpMp=tmpMp->next) {
    if (strcmp(tmpMp->mountPoint,mountPoint) == 0x0) {
      return(tmpMp);
      }
    }
  /* Return NULL If Mount Point Not Found */
  return NULL;
  }

/***
 END
 ***/

