/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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
#include <sys/descrip.h> /* struct file + g_dev_char_ioctl hook (path B) */
#include <sys/types.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <ubixos/vitals.h>
#include <ubixos/random.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <lib/kprintf.h>

/* Spinlock for devfs we should start converting to sem/mutex */
static struct spinLock devfsSpinLock = SPIN_LOCK_INITIALIZER;

/* Length of dev list */
static int devfs_len = 0x0;

/*
 * Pre-mount queue — device registrations that arrive before /dev is mounted
 * are buffered here and replayed by devfs_initialize() at mount time.
 * This allows the kernel's vfs_mount() call to be removed from devfs_init()
 * so that /dev can be mounted by automountd from fstab instead.
 */
#define DEVFS_QUEUE_MAX 64
struct devfs_queued {
	char   name[32];
	u_int8_t  type;
	u_int16_t major;
	u_int16_t minor;
};
static struct devfs_queued devfs_queue[DEVFS_QUEUE_MAX];
static int                 devfs_queue_n  = 0;
static int                 devfs_mounted  = 0;

/**
 This is the initialized called by the vfs system when enabling devfs
 basically it allocates memory for the devfs module
 */
static int devfs_initialize(struct vfs_mountPoint *mp) {
  struct devfs_info *fsInfo = 0x0;
  int i;

  /* Allocate memory for the fsInfo */
  if ((mp->fsInfo = (struct devfs_info *) kmalloc(sizeof(struct devfs_info))) == 0x0)
    K_PANIC("devfs: failed to allocate memor\n");

  fsInfo = mp->fsInfo;
  fsInfo->deviceList = 0x0;

  /* Mark mounted and replay any device registrations that arrived early. */
  devfs_mounted = 1;
  for (i = 0; i < devfs_queue_n; i++)
    devfs_makeNode(devfs_queue[i].name, devfs_queue[i].type,
                   devfs_queue[i].major, devfs_queue[i].minor);
  devfs_queue_n = 0;
  return (1);
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
      fd->start = (u_int32_t)(uintptr_t) tmpDev;
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
  u_int32_t sectors = 0x0;
  u_int16_t diff = 0x0;
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

  /* Pseudo-devices */
  if (tmpDev->devType == 'p') {
    if (tmpDev->devMinor == 0) /* null — reads return EOF */
      return (0);
    if (tmpDev->devMinor == 1) { /* zero */
      memset(data, 0, size);
      return (size);
    }
    if (tmpDev->devMinor == 3 || tmpDev->devMinor == 4) { /* random / urandom */
      krandom_bytes(data, size);
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
static int devfs_write(fileDescriptor_t *fd, char *data, off_t offset, long size) {
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

int devfs_makeNode(char *name, u_int8_t type, u_int16_t major, u_int16_t minor) {
  struct vfs_mountPoint *mp = 0x0;
  struct devfs_info *fsInfo = 0x0;
  struct devfs_devices *tmpDev = 0x0;

  spinLock(&devfsSpinLock);

  /* If /dev is not yet mounted, buffer for replay in devfs_initialize(). */
  if (!devfs_mounted) {
    if (devfs_queue_n < DEVFS_QUEUE_MAX) {
      strncpy(devfs_queue[devfs_queue_n].name, name,
              sizeof(devfs_queue[0].name) - 1);
      devfs_queue[devfs_queue_n].type  = type;
      devfs_queue[devfs_queue_n].major = major;
      devfs_queue[devfs_queue_n].minor = minor;
      devfs_queue_n++;
    }
    spinUnlock(&devfsSpinLock);
    return (0);
  }

  mp = vfs_findMount("/dev");

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

  ent->d_ino  = (u_int32_t)(uintptr_t)dev;
  ent->d_type = 2; /* DT_CHR — character device */
  strncpy(ent->d_name, dev->devName, sizeof(ent->d_name) - 1);
  ent->d_name[sizeof(ent->d_name) - 1] = '\0';

  dir->dirHandle = dev->next;
  return (0x0);
}

static int devfs_closedir(kDIR_t *dir) {
  dir->dirHandle = 0x0;
  return (0x0);
}

/*
 * devfs_char_ioctl — g_dev_char_ioctl hook (path B fileops): route an ioctl on a
 * devfs char-device fd to the driver's dev_char_ioctl.  Keeps the device-model
 * lookup out of the generic sys_ioctl (sys/kern/descrip.c).
 *
 * @return the driver result, or -1 if @fp is not a devfs char device or the
 *         driver does not implement an ioctl op.
 */
static int devfs_char_ioctl(struct file *fp, u_int32_t com, void *data) {
  if (fp != NULL && fp->fd != NULL && fp->fd->mp != NULL && fp->fd->mp->fs->vfsType == VFS_TYPE_DEVFS) {
    struct devfs_devices *node = (struct devfs_devices *)(uintptr_t)fp->fd->start;
    struct ubx_device *dev = ubx_device_find(node->devMajor, node->devMinor);
    if (dev != NULL && dev->dev_char_ioctl != NULL)
      return (dev->dev_char_ioctl(dev, com, data));
  }
  return (-1);
}

int devfs_init() {
  /* Route char-device ioctls (path B): the generic sys_ioctl calls this hook. */
  g_dev_char_ioctl = devfs_char_ioctl;

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
    return (0x1);
  }

  /* Queue pseudo-devices — replayed by devfs_initialize() when automountd
   * mounts /dev via fstab.  No vfs_mount() call here. */
  devfs_makeNode("null",    'p', 0, 0);
  devfs_makeNode("zero",    'p', 0, 1);
  devfs_makeNode("tty",     'p', 0, 2);
  devfs_makeNode("random",  'p', 0, 3);
  devfs_makeNode("urandom", 'p', 0, 4);
  devfs_makeNode("console", 'p', 0, 5);
  devfs_makeNode("stdin",   'p', 0, 6);
  devfs_makeNode("stdout",  'p', 0, 7);
  devfs_makeNode("stderr",  'p', 0, 8);

  return (0x0);
}
