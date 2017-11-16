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

 $Id: device.h 102 2016-01-12 03:59:34Z reddawg $

 *****************************************************************************************/

#ifndef _DEVICE_H
#define _DEVICE_H

#include <sys/types.h>

struct device_node {
  struct device_node *prev;
  struct device_node *next;
  struct device_interface *devInfo;
  struct device_resource *devRec;
  char type;
  int minor;
};

struct device_resource {
  uInt8 irq;
};

struct device_interface {
  uInt8 initialized;
  uInt32 size;
  int major;
  void *info;
  int (*read)( void *, void *, uInt32, uInt32 );
  int (*write)( void *, void *, uInt32, uInt32 );
  void (*reset)( void * );
  int (*init)( void * );
  void (*ioctl)( void * );
  void (*stop)( void * );
  void (*start)( void * );
  void (*standby)( void * );
};

int device_add( int, char, struct device_interface * );
struct device_node *device_find( int major, int minor );
int device_remove( struct device_node * );
#endif

/***
 $Log: device.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:15  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:51  reddawg
 no message

 Revision 1.14  2004/08/15 00:33:02  reddawg
 Wow the ide driver works again

 Revision 1.13  2004/08/14 21:56:44  reddawg
 Added initialized byte to the device system to make it easy to add child devices which use parent hardware.

 Revision 1.12  2004/07/21 10:02:09  reddawg
 devfs: renamed functions
 device system: renamed functions
 fdc: fixed a few potential bugs and cleaned up some unused variables
 strol: fixed definition
 endtask: made it print out freepage debug info
 kmalloc: fixed a huge memory leak we had some unhandled descriptor insertion so some descriptors were lost
 ld: fixed a pointer conversion
 file: cleaned up a few unused variables
 sched: broke task deletion
 kprintf: fixed ogPrintf definition

 Revision 1.11  2004/05/22 02:40:04  ionix


 fixed typo in device.h and initialized previous in device.c :)

 Revision 1.10  2004/05/22 02:34:03  ionix


 Added proto

 Revision 1.9  2004/05/21 15:12:17  reddawg
 Cleaned up


 END
 ***/

