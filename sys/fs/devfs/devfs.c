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

#include <fs/devfs/devfs.h>
#include <fs/vfs/vfs.h>
#include <sys/bus.h>
#include <sys/types.h>
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
static int devfs_open(char *file, fileDescriptor_t *fd) {
  struct devfs_info *fsInfo = fd->mp->fsInfo;
  struct devfs_devices *tmpDev = 0x0;
  struct ubx_device *device = 0x0;

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
          device = ubx_device_find(tmpDev->devMajor, tmpDev->devMinor);
          fd->start = (uint32_t)(uintptr_t) tmpDev; /* MrOlsen (2016-01-19) FIX: I Don't Understand This */
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
 Function: int readDevFS(fileDescriptor_t *fd,char *data,long offset,long size)
 Description: Read File Into Data
 Notes:
 */
static int devfs_read(fileDescriptor_t *fd, char *data, off_t offset, long size) {
  int i = 0x0, x = 0x0;
  uInt32 sectors = 0x0;
  uInt16 diff = 0x0;
  struct ubx_device *device = 0x0;
  struct devfs_devices *tmpDev = (void *) fd->start;

  if (tmpDev == (struct devfs_devices *)-1) {
    kprintf("Hi Ubie [%i]!!!\n", size);
    for (i = 0; i < size; i++) {
      data[i] = 'a';
      fd->buffer[i] = 'a';
    }
    data[size - 1] = '\n';
    return (size);
  }

  /* Pseudo-devices: null reads return 0 bytes; zero reads return NUL bytes */
  if (tmpDev->devType == 'p') {
    if (tmpDev->devMinor == 0) /* null */
      return (0);
    if (tmpDev->devMinor == 1) { /* zero */
      memset(data, 0, size);
      return (size);
    }
    return (0);
  }

  device = ubx_device_find(tmpDev->devMajor, tmpDev->devMinor);

  sectors = ((size + 511) / 512);
  diff = (offset - ((offset / 512) * 512));

  for (i = 0x0; i < sectors; i++) {
    device->dev_blk_ops->read(device, i + (offset / 512), 1, fd->buffer);
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

 Function: int writeDevFS(fileDescriptor_t *fd,char *data,long offset,long size)
 Description: Write Data Into File
 Notes:

 ************************************************************************/
static int devfs_write(fileDescriptor_t *fd, char *data, long offset, long size) {
  int i = 0x0, x = 0x0;
  struct ubx_device *device = 0x0;
  struct devfs_devices *tmpDev = (void *) fd->start;

  /* Pseudo-devices: silently discard all writes */
  if (tmpDev->devType == 'p')
    return (size);

  device = ubx_device_find(tmpDev->devMajor, tmpDev->devMinor);

  /* Character device with a write hook (e.g. /dev/audio) */
  if (device != 0x0 && device->dev_char_write != 0x0)
    return device->dev_char_write(device, data, (int)size);
  for (i = 0x0; i < ((size + 511) / 512); i++) {
    device->dev_blk_ops->read(device, i + (offset / 512), 1, fd->buffer);
    for (x = 0x0; ((x < 512) && ((x + (i * 512)) < size)); x++) {
      fd->buffer[x] = data[x];
    }
    device->dev_blk_ops->write(device, i + (offset / 512), 1, fd->buffer);
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
  snprintf(tmpDev->devName, sizeof(tmpDev->devName), "%s", name);
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

static int devfs_opendir(const char *path, kDIR_t *dir) {
  struct vfs_mountPoint *mp = dir->mp;
  struct devfs_info *fsInfo = mp->fsInfo;

  (void)path;
  spinLock(&devfsSpinLock);
  dir->dirHandle = fsInfo->deviceList;
  spinUnlock(&devfsSpinLock);
  return (0x1);
}

static int devfs_readdir(kDIR_t *dir, struct kdirent *ent) {
  struct devfs_devices *dev = (struct devfs_devices *)dir->dirHandle;

  if (dev == 0x0)
    return (-1);

  ent->d_ino  = (uint32_t)(uintptr_t)dev;
  ent->d_type = KDT_REG;
  strncpy(ent->d_name, dev->devName, sizeof(ent->d_name) - 1);
  ent->d_name[sizeof(ent->d_name) - 1] = '\0';

  dir->dirHandle = dev->next;
  return (0x0);
}

static int devfs_closedir(kDIR_t *dir) {
  dir->dirHandle = 0x0;
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
  1,    /* vfsType     */
  devfs_opendir,
  devfs_readdir,
  devfs_closedir,
  }; /* devFS */

  if (vfsRegisterFS(devFS) != 0x0) {
    //sysErr(systemErr,"Unable To Enable DevFS");
    return (0x1);
  }
  /* Mount our devfs this will build the devfs container node */
  vfs_mount(0x0, 0x0, 0x0, 0x1, "devfs", "rw"); // Mount Device File System

  /* Pseudo-devices always present, major=0 minor=0/1 */
  devfs_makeNode("null", 'p', 0, 0);
  devfs_makeNode("zero", 'p', 0, 1);

  /* Return */
  return (0x0);
}
