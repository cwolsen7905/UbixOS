/*****************************************************************************************
 Copyright (c) 2002-2026 The UbixOS Project
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

 *****************************************************************************************/

#include <fs/vfs/vfs.h>
#include <sys/bus.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/exec.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <lib/kprintf.h>
#include <assert.h>

#include "fat_filelib.h"
#include "fat_access.h"

fileDescriptor_t *_fd;
struct vfs_mountPoint *_mp;

/*
 * FAT library mutex — serializes all fl_* calls.
 *
 * fat_io_lib has no internal locking; concurrent callers corrupt its sector
 * cache.  On a uniprocessor kernel the simplest correct primitive is an
 * atomic test-and-set flag with sched_yield() in the spin loop: a contending
 * task yields the CPU so the holder can finish and release, avoiding the
 * deadlock that a cpu_relax() spinlock would cause when the PIT fires and
 * switches tasks mid-hold.
 *
 * fat_acquire / fat_release must not be called from interrupt context.
 */
static volatile int fat_busy = 0;

static void
fat_acquire(void)
{
	while (__sync_lock_test_and_set(&fat_busy, 1))
		sched_yield();
}

static void
fat_release(void)
{
	__sync_lock_release(&fat_busy);
}

int media_read(unsigned long sector, unsigned char *buffer, unsigned long sector_count) {
  _mp->device->dev_blk_ops->read(_mp->device, sector, sector_count, buffer);
  return 1;
}

int media_write(unsigned long sector, unsigned char *buffer, unsigned long sector_count) {
  _mp->device->dev_blk_ops->write(_mp->device, sector, sector_count, buffer);
  return 1;
}

int fat_initialize(struct vfs_mountPoint *mp) {
  FL_FILE *file;
  _mp = mp;

  if (fl_attach_media(media_read, media_write) != FAT_INIT_OK) {
    kprintf("ERROR: Media attach failed\n");
    return (0);
  }

  /* Create a test file to verify write path on init */
  file = fl_fopen("/file.bin", "w");
  unsigned char data[] = { 'a', '\n', 'b', '\n', 'c', '\n' };
  if (file) {
    if (fl_fwrite(data, 1, sizeof(data), file) != sizeof(data))
      kprintf("ERROR: Write file failed\n");
  }
  else
    kprintf("ERROR: Create file failed\n");

  fl_fclose(file);

  return (1);
}

int read_fat(fileDescriptor_t *fd, char *data, off_t offset, long size) {
  FL_FILE *_file = (FL_FILE*) fd->res;
  long ret;

  if (size == 0) {
    kprintf("ZERO: %s", _file->filename);
    return (0);
  }

  fat_acquire();
  if (fl_fseek(_file, offset & 0xFFFFFFFF, 0) != 0)
    kprintf("SEEK FAILED!");
  ret = fl_fread(data, size, 1, _file);
  fat_release();

  return (ret);
}

int write_fat(fileDescriptor_t *fd, char *data, off_t offset, long size) {
  FL_FILE *_file = (FL_FILE*) fd->res;

  fat_acquire();
  if (fl_fwrite(data, 1, size, _file) != size)
    kprintf("ERROR: Write file failed\n");
  fl_fflush(_file);
  fat_release();

  return (size);
}

int open_fat(const char *file, fileDescriptor_t *fd) {
  FL_FILE *_file = 0x0;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->device);
  assert(fd->mp->device->dev_blk_ops);
  assert(fd->mp->device->dev_blk_ops->read);
  assert(file);

  fat_acquire();
  if ((fd->mode & 0x1) == 0x1) {
    _file = fl_fopen(file, "r");
  }
  else if ((fd->mode & 0x2) == 0x2) {
    if ((fd->mode & 0x8) == 0x8) {
      _file = fl_fopen(file, "a");
    }
    else {
      _file = fl_fopen(file, "w");
    }
  }
  else
    kprintf("Invalid Mode?");
  fat_release();

  if (!_file) {
    return (0x0);
  }
  else {
    fd->offset = 0;
    fd->res = _file;
    fd->perms = 0x1;
    fd->size = _file->filelength;
    fd->ino = _file->startcluster;
  }

  return (0x1);
}

int unlink_fat() {
    kprintf("[%s:%i] unlink_fat");
  return (0);
}

int mkdir_fat(char *path, void *fd) {
  int ret;
  fat_acquire();
  ret = fl_createdirectory(path);
  fat_release();
  return ret;
}

int rmdir_fat(char *path, void *fd) {
  int ret;
  fat_acquire();
  ret = fl_remove(path);
  fat_release();
  return ret;
}

int fat_opendir(const char *path, kDIR_t *dir) {
    FL_DIR *fldir = (FL_DIR *) kmalloc(sizeof(FL_DIR));
    if (fldir == 0x0)
        return (0x0);
    fat_acquire();
    if (fl_opendir(path, fldir) == 0x0) {
        fat_release();
        kfree(fldir);
        return (0x0);
    }
    fat_release();
    dir->dirHandle = fldir;
    return (0x1);
}

int fat_readdir(kDIR_t *dir, struct kdirent *ent) {
    fl_dirent fent;
    int ret;
    fat_acquire();
    ret = fl_readdir((FL_DIR *)dir->dirHandle, &fent);
    fat_release();
    if (ret != 0)
        return (-1);
    ent->d_ino = fent.cluster;
    ent->d_type = fent.is_dir ? KDT_DIR : KDT_REG;
    strncpy(ent->d_name, fent.filename, 255);
    ent->d_name[255] = '\0';
    return (0);
}

int fat_closedir(kDIR_t *dir) {
    if (dir->dirHandle != 0x0) {
        fat_acquire();
        fl_closedir((FL_DIR *)dir->dirHandle);
        fat_release();
        kfree(dir->dirHandle);
        dir->dirHandle = 0x0;
    }
    return (0);
}

int fat_init() {
  fl_init();

  struct fileSystem ubixFileSystem = {
      NULL,
      NULL,
      (void*) fat_initialize,
      (void*) read_fat,
      (void*) write_fat,
      (void*) open_fat,
      (void*) unlink_fat,
      (void*) mkdir_fat,
      (void*) rmdir_fat,
      NULL,
      0xFA,
      (void*) fat_opendir,
      (void*) fat_readdir,
      (void*) fat_closedir
  };

  if (vfsRegisterFS(ubixFileSystem) != 0x0) {
    kpanic("Unable To Enable UbixFS");
    return (0x1);
  }

  return (0x0);
}
