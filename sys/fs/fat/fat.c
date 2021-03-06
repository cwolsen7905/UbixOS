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

 $Id: ubixfs.c 79 2016-01-11 16:21:27Z reddawg $

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

int media_read(unsigned long sector, unsigned char *buffer, unsigned long sector_count) {
  _mp->device->devInfo->read(_mp->device->devInfo->info, buffer, sector, sector_count);

  return 1;
}

int media_write(unsigned long sector, unsigned char *buffer, unsigned long sector_count) {
  _mp->device->devInfo->write(_mp->device->devInfo->info, buffer, sector, sector_count);

  return 1;
}

int fat_initialize(struct vfs_mountPoint *mp) {
  FL_FILE *file;
  _mp = mp;

  // Attach media access functions to library
  if (fl_attach_media(media_read, media_write) != FAT_INIT_OK) {
    kprintf("ERROR: Media attach failed\n");
    return (0);
  }

  // List root directory
  //fl_listdirectory("/bin/");

  // Delete File
  //if (fl_remove("/file.bin") < 0)
  //  kprintf("ERROR: Delete file failed\n");

  // Create File
  file = fl_fopen("/file.bin", "w");
  unsigned char data[] = {
      'a',
      '\n',
      'b',
      '\n',
      'c',
      '\n' };
  if (file) {
    // Write some data
    if (fl_fwrite(data, 1, sizeof(data), file) != sizeof(data))
      kprintf("ERROR: Write file failed\n");
  }
  else
    kprintf("ERROR: Create file failed\n");

  // Close file
  fl_fclose(file);

  return (1);
}

int read_fat(fileDescriptor_t *fd, char *data, off_t offset, long size) {
  FL_FILE *_file = (FL_FILE*) fd->res;

  if (size == 0) {
    kprintf("ZERO: %s", _file->filename);

        return (0);
  }

  //kprintf("[Offset(%s): %ld:%ld]", _file->filename, offset, size);
  if (fl_fseek(_file, offset & 0xFFFFFFFF, 0) != 0)
    kprintf("SEEK FAILED!");

  size = fl_fread(data, size, 1, _file);
    if (size > 0)
        fd->offset += size;
    /*
    else
        kprintf("[%s:%i] read_fat(0) FAILED!");
     */

  /* Return */
  return (size);
}

int write_fat(fileDescriptor_t *fd, char *data, off_t offset, long size) {
  FL_FILE *_file = (FL_FILE*) fd->res;

  kprintf("Writing: %i[%i]\n", size, offset);
  // XXX this is not supposed to happen fl_fseek(_file, offset, 0);

  if (fl_fwrite(data, 1, size, _file) != size)
    kprintf("ERROR: Write file failed\n");

  kprintf("Wrote: %i\n", size);

  /* Return */
  return (size);
}

int open_fat(const char *file, fileDescriptor_t *fd) {
  FL_FILE *_file = 0x0;

  assert(fd);
  assert(fd->mp);
  assert(fd->mp->device);
  assert(fd->mp->device->devInfo);
  assert(fd->mp->device->devInfo->read);
  assert(file);

    //kprintf("File: %s, ", file);
  //kprintf("Mode: 0x%X\n", fd->mode);

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

int mkdir_fat() {
    kprintf("[%s:%i] mkdir_fat");
  return (0);
}

int fat_init() {
  // Initialise File IO Library
  fl_init();

  /* Set up our file system structure */
  struct fileSystem ubixFileSystem = {
      NULL, /* prev        */
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
