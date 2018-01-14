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

#include <devfs/devfs.h>
#include <vfs/vfs.h>
#include <sys/device.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <lib/kprintf.h>

/* Spinlock for devfs we should start converting to sem/mutex */
static struct spinLock devfsSpinLock = SPIN_LOCK_INITIALIZER;

/* Length of dev list */
static int devfs_len = 0x0;

/**
 This is the initialized called by the vfs system when enabling devfs
 basically it allocates memory for the devfs module
 */
static void devfs_initialize(struct vfs_mountPoint *mp) {
  struct devfs_info *fsInfo = 0x0;

  /* Allocate memory for the fsInfo */
  if ((mp->fsInfo = (struct devfs_info *) kmalloc(sizeof(struct devfs_info))) == 0x0)
    K_PANIC("devfs: failed to allocate memor\n");

  fsInfo = mp->fsInfo;
  fsInfo->deviceList = 0x0;

  /* Return */
  return;
}

/**
 This is the open routine called by the vfs when a fopen or open is called within the user or kernel space
 file - this is the file node
 fd   - this is the file descriptor

 This format will be changing down the road
 */
static int devfs_open(char *file, fileDescriptor *fd) {
  struct devfs_info *fsInfo = fd->mp->fsInfo;
  struct devfs_devices *tmpDev = 0x0;
  struct device_node *device = 0x0;

  spinLock(&devfsSpinLock);

  if (strcmp(file, "/") == 0x0) {
    fd->start = -1;
    fd->size = devfs_len;
    spinUnlock(&devfsSpinLock);
    return (0x1);
  }
  if (file[0] == '/')
    file++;
  for (tmpDev = fsInfo->deviceList; tmpDev != 0x0; tmpDev = tmpDev->next) {
    if (strcmp(tmpDev->devName, file) == 0x0) {
      switch ((fd->mode & 0x3)) {
        case 0:
        case 1:
          device = device_find(tmpDev->devMajor, tmpDev->devMinor);
          fd->start = (uint32_t *) tmpDev; /* MrOlsen (2016-01-19) FIX: I Don't Understand This */
          fd->size = device->devInfo->size;
        break;
        default:
          kprintf("Invalid File Mode\n");
          spinUnlock(&devfsSpinLock);
          return (-1);
        break;
      }
      spinUnlock(&devfsSpinLock);
      return (0x1);
    }
  }
  spinUnlock(&devfsSpinLock);
  return (0x0);
}

/**
 Function: int readDevFS(fileDescriptor *fd,char *data,long offset,long size)
 Description: Read File Into Data
 Notes:
 */
static int devfs_read(fileDescriptor *fd, char *data, long offset, long size) {
  int i = 0x0, x = 0x0;
  uInt32 sectors = 0x0;
  uInt16 diff = 0x0;
  struct device_node *device = 0x0;
  struct devfs_devices *tmpDev = (void *) fd->start;

  if (tmpDev == -1) {
    kprintf("Hi Ubie [%i]!!!\n", size);
    for (i = 0; i < size; i++) {
      data[i] = 'a';
      fd->buffer[i] = 'a';
    }
    data[size - 1] = '\n';
    return (size);
  }

  device = device_find(tmpDev->devMajor, tmpDev->devMinor);

  sectors = ((size + 511) / 512);
  diff = (offset - ((offset / 512) * 512));

  for (i = 0x0; i < sectors; i++) {
    device->devInfo->read(device->devInfo->info, fd->buffer, i + (offset / 512), 1);
    for (x = 0x0; x < (size - (i * 512)); x++) {
      if (diff > 0) {
        data[x] = fd->buffer[x + diff];
      }
      else {
        data[x] = fd->buffer[x];
      }
    }
    diff = 0x0;
    data += 512;
  }

  return (size);
}

/************************************************************************

 Function: int writeDevFS(fileDescriptor *fd,char *data,long offset,long size)
 Description: Write Data Into File
 Notes:

 ************************************************************************/
static int devfs_write(fileDescriptor *fd, char *data, long offset, long size) {
  int i = 0x0, x = 0x0;
  struct device_node *device = 0x0;
  struct devfs_devices *tmpDev = (void *) fd->start;

  device = device_find(tmpDev->devMajor, tmpDev->devMinor);
  for (i = 0x0; i < ((size + 511) / 512); i++) {
    device->devInfo->read(device->devInfo->info, fd->buffer, i + (offset / 512), 1);
    for (x = 0x0; ((x < 512) && ((x + (i * 512)) < size)); x++) {
      fd->buffer[x] = data[x];
    }
    device->devInfo->write(device->devInfo->info, fd->buffer, i + (offset / 512), 1);
    data += 512;
  }
  return (size);
}

int devfs_makeNode(char *name, uInt8 type, uInt16 major, uInt16 minor) {
  struct vfs_mountPoint *mp = 0x0;
  struct devfs_info *fsInfo = 0x0;
  struct devfs_devices *tmpDev = 0x0;

  spinLock(&devfsSpinLock);

  mp = vfs_findMount("devfs");

  if (mp == 0x0) {
    kprintf("Error: Can't Find Mount Point\n");
    spinUnlock(&devfsSpinLock);
    return (-1);
  }

  fsInfo = mp->fsInfo;

  tmpDev = (struct devfs_devices *) kmalloc(sizeof(struct devfs_devices));

  tmpDev->devType = type;
  tmpDev->devMajor = major;
  tmpDev->devMinor = minor;
  sprintf(tmpDev->devName, name);
  devfs_len += strlen(name) + 1;

  tmpDev->next = fsInfo->deviceList;
  tmpDev->prev = 0x0;
  if (fsInfo->deviceList != 0x0) {
    fsInfo->deviceList->prev = tmpDev;
  }

  fsInfo->deviceList = tmpDev;

  spinUnlock(&devfsSpinLock);
  return (0x0);
}

int devfs_init() {
  /* Build our devfs struct */
  struct fileSystem devFS = { NULL, /* prev        */
  NULL, /* next        */
  (void *) devfs_initialize, /* vfsInitFS   */
  (void *) devfs_read, /* vfsRead     */
  (void *) devfs_write, /* vfsWrite    */
  (void *) devfs_open, /* vfsOpenFile */
  NULL, /* vfsUnlink   */
  NULL, /* vfsMakeDir  */
  NULL, /* vfsRemDir   */
  NULL, /* vfsSync     */
  1 /* vfsType     */
  }; /* devFS */

  if (vfsRegisterFS(devFS) != 0x0) {
    //sysErr(systemErr,"Unable To Enable DevFS");
    return (0x1);
  }
  /* Mount our devfs this will build the devfs container node */
  vfs_mount(0x0, 0x0, 0x0, 0x1, "devfs", "rw"); // Mount Device File System

  /* Return */
  return (0x0);
}
