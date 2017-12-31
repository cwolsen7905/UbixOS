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

 $Id: ramdrive.cpp 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "device.h"


static char *ram_data = 0x0;

#define RAM_DRIVE_SIZE 1024*1024*100

static 
int 
ramDrive_read(device_t *dev,void *ptr,off_t offset,size_t length) {
  char *data = ram_data + (offset*512);
  assert(ram_data);
  assert(offset+length <= dev->sectors); 
  memcpy(ptr, data, length * 512);
  
  return(length);
}
  
static 
int  
ramDrive_write(device_t *dev,void *ptr,off_t offset,size_t length) {
  char *data = ram_data + (offset*512);
  assert(ram_data);

  assert(offset+length <= dev->sectors); 
  
  memcpy(data, ptr, length * 512);
  
  return(length);
}
  
device_t *
dev_ramDrive(void) {
  device_t *ramDrive = 0x0;
  
  FILE *ramDisk;
  
  ramDisk = fopen("./ram.dsk","rb");

  ramDrive = (device_t *)malloc(sizeof(device_t));
  
  ram_data = (char *)malloc(RAM_DRIVE_SIZE);
  
  if (ramDisk != 0x0) {
    printf("Loaded Ram Disk\n");
    fread(ram_data,RAM_DRIVE_SIZE,1,ramDisk);
    fclose(ramDisk);
    }
  
  ramDrive->major = 0x1;
  ramDrive->sectors = RAM_DRIVE_SIZE / 512;
  
  ramDrive->read = ramDrive_read;
  ramDrive->write = ramDrive_write;
  
  printf("leaving dev_ramDrive()\n");
  return(ramDrive);

} // dev_ramDrive
  
int 
dev_ramDestroy(void) {
  printf("dev_ramDestroy()\n");
  FILE *ramDisk;
  
  ramDisk = fopen("./ram.dsk","wb");
  
  fwrite(ram_data, RAM_DRIVE_SIZE ,1,ramDisk);
  
  fclose(ramDisk);
  
  printf("Saved Ram Disk\n");
  
  return(0x0);
} // dev_ramDestroy

/***
 $Log: ramdrive.cpp,v $
 Revision 1.1.1.1  2006/06/01 12:46:15  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:45  reddawg
 no message

 Revision 1.5  2004/09/20 00:53:04  flameshadow
 chg: UbixFS::vfs_read() now works for direct block extents
 chg: UbixFS::vfs_read() returns ~0 on error, or the size read in on success
 chg: UbixFS::root member is now a fileDescriptor
 chg: UbixFS::vfs_init() creates a file descriptor for the root dir
 chg: UbixFS::vfs_format() now sets all values in the bTreeHeader before writing
 chg: UbixFS::vfs_format() sets the inode magic number
 chg: device_t::read(device_t *, void *, off_t, size_t)
 chg: device_t::write(device_t *, void *, off_t, size_t)
 chg: vfs_abstract::vfs_read(fileDescriptor *, void *, off_t, size_t)
 chg: vfs_abstract::vfs_write(fileDescriptor *, void *, off_t, size_t)
 chg: ramDrive_read(device_t *dev,void *ptr,off_t offset,size_t length)
 chg: ramDrive_write(device_t *dev,void *ptr,off_t offset,size_t length)

 Revision 1.4  2004/09/13 20:10:11  flameshadow
 chg: UbixFS::vfs_format() works
 chg: UbixFS::vfs_init() works

 Revision 1.3  2004/09/13 15:48:29  flameshadow
 chg: oops, the ramDrive is 100MB, not 512MB

 Revision 1.2  2004/09/13 15:21:26  flameshadow
 add: ramdrive.h
 chg: renamed device_t.size to sectors
 chg: made #define for size of ramdisk
 chg: calculated sectors of ramdisk and stored in the device_t struct

 Revision 1.1  2004/09/13 14:37:49  flameshadow
 add: ramdrive.cpp
 chg: added ramdrive to the makefile
 chg: finished root dir inode initialization and saving in UbixFS::format()

 Revision 1.8  2004/08/30 13:29:08  reddawg
 Fixed ramdisk to write/read 512 blocks

 Revision 1.7  2004/08/13 22:06:54  reddawg
 UbixFS: fixed the test shell

 Revision 1.6  2004/08/13 21:47:49  reddawg
 Fixed

 Revision 1.5  2004/08/13 17:25:06  reddawg
 Testing better

 Revision 1.4  2004/08/13 17:07:32  reddawg
 Works

 Revision 1.3  2004/08/13 16:58:57  reddawg
 test

 Revision 1.2  2004/08/13 16:54:14  reddawg
 fixed

 Revision 1.1  2004/08/13 16:51:55  reddawg
 Start of ubixfs shell

 END
 ***/

