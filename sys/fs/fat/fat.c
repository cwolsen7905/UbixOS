/*****************************************************************************************
 Copyright (c) 2019 The UbixOS Project
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

#include <vfs/vfs.h>
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

int media_read(unsigned long sector, unsigned char *buffer,
    unsigned long sector_count) {
  _mp->device->devInfo->read(_mp->device->devInfo->info, buffer, sector,
      sector_count);

  return 1;
}

int media_write(unsigned long sector, unsigned char *buffer,
    unsigned long sector_count) {
  _mp->device->devInfo->write(_mp->device->devInfo->info, buffer, sector,
      sector_count);

  return 1;
}

int fat_initialize(struct vfs_mountPoint *mp) {
  // Attach media access functions to library
  if (fl_attach_media(media_read, media_write) != FAT_INIT_OK) {
    kprintf("ERROR: Media attach failed\n");
    return (0);
  }

  return (1);
}

int read_fat(fileDescriptor_t *fd, char *data, uInt32 offset, long size) {
  FL_FILE *_file = (FL_FILE*) fd->res;

  kprintf("Reading: %i[%i]\n", size, offset);
  fl_fseek(_file, offset, 0);
  size = fl_fread(data, size, 1, _file);

  kprintf("Read: %i\n", size);

  /* Return */
  return (size);
}

int write_fat() {
  return (0);
}

int open_fat(const char *file, fileDescriptor_t *fd) {
  FL_FILE *_file;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->device);
  assert(fd->mp->device->devInfo);
  assert(fd->mp->device->devInfo->read);
  assert(file);

  kprintf(file);

  _file = fl_fopen(file, "r");
  if (!_file) {
    kprintf("ERROR: Open file failed\n");
    return (0x0);
  }
  else {
    fd->res = _file;
    fd->perms = 0x1;
  }

  return (0x1);
}

int unlink_fat() {
  return (0);
}

int mkdir_fat() {
  return (0);
}

int fat_init() {
  // Initialise File IO Library
  fl_init();

  /* Set up our file system structure */
  struct fileSystem ubixFileSystem = { NULL, /* prev        */
  NULL, /* next        */
  (void*) fat_initialize, /* vfsInitFS   */
  (void*) read_fat, /* vfsRead     */
  (void*) write_fat, /* vfsWrite    */
  (void*) open_fat, /* vfsOpenFile */
  (void*) unlink_fat, /* vfsUnlink   */
  (void*) mkdir_fat, /* vfsMakeDir  */
  NULL, /* vfsRemDir   */
  NULL, /* vfsSync     */
  0xFA /* vfsType     */
  }; /* ubixFileSystem */

  /* Add UbixFS */
  if (vfsRegisterFS(ubixFileSystem) != 0x0) {
    kpanic("Unable To Enable UbixFS");
    return (0x1);
  }

  /* Return */
  return (0x0);
}
