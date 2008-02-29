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

#include <isa/fdc.h>
#include <isa/8259.h>
#include <sys/video.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <ubixos/types.h>
#include <ubixos/spinlock.h>
#include <sys/io.h>
#include <sys/dma.h>
#include <sys/device.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <devfs/devfs.h>

static spinLock_t fdcSpinLock = SPIN_LOCK_INITIALIZER;

static volatile bool done = FALSE;
static drvGeom geometry = { dg144Heads,dg144Tracks,dg144Spt };
static bool diskChange = FALSE;
static bool motor = FALSE;
static volatile Int8 fdcTrack = 0xff;
static Int8 sr0 = 0;
static volatile int timeOut = 0;
static Int8 statSize = 0;
static Int8 status[7] = { 0 };

unsigned long tbaddr = 0x80000L;

int fdcInit2(struct device_node *dev) {
  dev->devInfo->size = (1024 * 1450);
  return(0x0);
  }

int fdc_init() {
  struct device_interface *devInfo = (struct device_interface *)kmalloc(sizeof(struct device_interface));
  setVector(floppyIsr, mVec+6, (dInt+dPresent));
  irqEnable(6);
  reset();
  devInfo->major = 0x0;
  devInfo->init  = (void *)&fdcInit2;
  devInfo->read  = fdcRead;
  devInfo->write = fdcWrite;
  devInfo->reset = (void *)reset;
  
  device_add(0,'c',devInfo);
  devfs_makeNode("fd0",'b',0x0,0x0);
  return(0x0);
  }

asm(
  ".globl floppyIsr      \n"
  "floppyIsr:            \n"
  "  pusha               \n"
  "  push %ss            \n"
  "  push %ds            \n"
  "  push %es            \n"
  "  push %fs            \n"
  "  push %gs            \n"
  "  call floppyIsrhndlr \n"
  "  pop %gs             \n"
  "  pop %fs             \n" 
  "  pop %es             \n"
  "  pop %ds             \n"
  "  pop %ss             \n"
  "  popa                \n"
  "  iret                \n"
  );

void floppyIsrhndlr() {
  done = TRUE;
  outportByte(0x20,0x20);
  }

void sendByte(int Int8) {
  volatile int msr;
  int tmo;
  for (tmo=0;tmo<128;tmo++) {
    msr = inportByte(fdcMsr);
    if ((msr & 0xc0) == 0x80) {
      outportByte(fdcData,Int8);
      return;
      }
    inportByte(0x80);
    }
  }

int getByte() {
  volatile int msr;
  int tmo;
  for (tmo=0;tmo<128;tmo++) {
    msr = inportByte(fdcMsr);
    if ((msr & 0xd0) == 0xd0) {
      return inportByte(fdcData);
      }
    inportByte(0x80);
    }
  return(-1);
  }

bool fdcRw(int block,Int8 *blockBuffer,bool read,unsigned long numSectors) {
  int head = 0x0,track = 0x0,sector = 0x0,tries= 0x0, copyCount = 0x0;
  unsigned char *p_tbaddr = (unsigned char *)0x80000;
  unsigned char *p_blockbuff = (unsigned char *)blockBuffer;
  //kprintf("Block: [%i]\n",block);
  block2Hts(block,&head,&track,&sector);
  motorOn();
  if (!read && blockBuffer) {
    /* copy data from data buffer into track buffer */
    for (copyCount=0; copyCount<(numSectors*512); copyCount++) {
      *p_tbaddr = *p_blockbuff;
      p_blockbuff++;
      p_tbaddr++;
      }
    }
  for (tries = 0;tries < 3;tries++) {
    if (inportByte(fdcDir) & 0x80) {
      diskChange = TRUE;
      seek(1);  /* clear "disk change" status */
      recalibrate();
      motorOff();
      kprint("FDC: Disk change detected. Trying again.\n");
      return fdcRw(block, blockBuffer, read, numSectors);
      }
    if (!seek(track)) {
      motorOff();
      kprintf("FDC: Error seeking to track [%i]\n",block);
      return FALSE;
      }
    outportByte(fdcCcr,0);
    if (read) {
      dmaXfer(2,tbaddr,numSectors*512,FALSE);
      sendByte(cmdRead);
      }
    else {
      dmaXfer(2,tbaddr,numSectors*512,TRUE);
      sendByte(cmdWrite);
      }
    sendByte(head << 2);
    sendByte(track);
    sendByte(head);
    sendByte(sector);
    sendByte(2);               /* 512 Int8s/sector */
    sendByte(geometry.spt);
    if (geometry.spt == dg144Spt) {
      sendByte(dg144Gap3rw);  /* gap 3 size for 1.44M read/write */
      }
    else {
      sendByte(dg168Gap3rw);  /* gap 3 size for 1.68M read/write */
      }
    sendByte(0xff);            /* DTL = unused */
    if (!waitFdc(TRUE)) {
      kprint("Timed out, trying operation again after reset()\n");
      reset();
      return fdcRw(block, blockBuffer, read, numSectors);
      }
    if ((status[0] & 0xc0) == 0) break;   /* worked! outta here! */
    recalibrate();  /* oops, try again... */
    }
  motorOff();
  if (read && blockBuffer) {
    p_blockbuff = (unsigned char *)blockBuffer;
    p_tbaddr = (unsigned char *) 0x80000;
    for (copyCount=0x0; copyCount<(numSectors*512); copyCount++) {
      *p_blockbuff = *p_tbaddr;
      p_blockbuff++;
      p_tbaddr++;
      }
    }
  return (tries != 3);
  }

void block2Hts(int block,int *head,int *track,int *sector) {
  *head = (block % (geometry.spt * geometry.heads)) / (geometry.spt);
  *track = block / (geometry.spt * geometry.heads);
  *sector = block % geometry.spt + 1;
  }

void motorOn(void) {
  if (motor == FALSE) {
    outportByte(fdcDor,0x1c);
    motor = TRUE;
    }
  }

void motorOff(void) {
  if (motor == TRUE) {
    //outportByte(fdcDor,0x0);
    //outportByte(fdcDor,0x0C);
    motor = FALSE;
    }
  }

bool seek(int track) {
  if (fdcTrack == track) {
    return(TRUE);
    }
  sendByte(cmdSeek);
  sendByte(0);
  sendByte(track);
  if (!waitFdc(TRUE)) {
    kprintf("wait fdc failed\n");
    return(FALSE);
    }
  if ((sr0 != 0x20) || (fdcTrack != track)) {
    return(FALSE);
    }
  else {
    return(TRUE);
    }
  }

bool readBlock(int block,Int8 *blockBuffer, unsigned long numSectors) {
  int result = 0x0,loop = 0x0;
  if (numSectors > 1) {
    for (loop=0; loop<numSectors; loop++) {
      result = fdcRw(block+loop, blockBuffer+(loop*512), TRUE, 1);
      }
    return result;
    }
  return fdcRw(block,blockBuffer,TRUE,numSectors);
  }

bool writeBlock(int block,Int8 *blockBuffer, unsigned long numSectors) {
  return fdcRw(block,blockBuffer,FALSE, numSectors);
  }

bool waitFdc(bool sensei) {
  timeOut = 50000;
  while (!done && timeOut);
  statSize = 0;
  while ((statSize < 7) && (inportByte(fdcMsr) & (1<<4))) {
    status[(int)statSize++] = getByte();
    }
  if (sensei) {
    sendByte(cmdSensei);
    sr0 = getByte();
    fdcTrack = getByte();
    }
  done = FALSE;
  if (!timeOut) {
    if (inportByte(fdcDir) & 0x80) {
      diskChange = TRUE;
      }
    return(FALSE);
    }
  else {
    return(TRUE);
    }
  }

void recalibrate(void) {
  motorOn();
  sendByte(cmdRecal);
  sendByte(0);
  waitFdc(TRUE);
  motorOff();
  }

void reset(void) {
  outportByte(fdcDor,0);
  motor = FALSE;
  outportByte(fdcDor,0x0c);
  done = TRUE;
  waitFdc(TRUE);
  sendByte(cmdSpecify);
  sendByte(0xdf);
  sendByte(0x02);
  seek(1);
  recalibrate();
  diskChange = FALSE;
  return;
  }

void fdcRead(void *info,void *baseAddr,uInt32 startSector,uInt32 sectorCount) {
  spinLock(&fdcSpinLock);
  readBlock(startSector,baseAddr,sectorCount);
  spinUnlock(&fdcSpinLock);
  return;
  }
void fdcWrite(void *info,void *baseAddr,uInt32 startSector,uInt32 sectorCount){
  writeBlock(startSector,baseAddr,sectorCount);
  return;
  }

/***

 $Log$
 Revision 1.1.1.1  2007/01/17 03:31:51  reddawg
 UbixOS

 Revision 1.1.1.1  2006/06/01 12:46:12  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:01  reddawg
 no message

 Revision 1.24  2004/09/07 21:54:38  reddawg
 ok reverted back to old scheduling for now....

 Revision 1.23  2004/09/06 15:13:25  reddawg
 Last commit before FreeBSD 6.0

 Revision 1.22  2004/08/21 20:06:28  reddawg
 ok check out exec.c

 Revision 1.21  2004/08/15 16:47:49  reddawg
 Fixed

 Revision 1.20  2004/08/15 00:33:02  reddawg
 Wow the ide driver works again

 Revision 1.19  2004/08/01 20:40:45  reddawg
 Net related fixes

 Revision 1.18  2004/07/29 21:32:16  reddawg
 My quick lunchs breaks worth of updates....

 Revision 1.17  2004/07/27 08:03:36  reddawg
 chg: stopped push all these extra registers I can't find a good reason as to why I was doing it

 Revision 1.16  2004/07/22 17:32:25  reddawg
 I broke it hopefully

 Revision 1.15  2004/07/21 10:02:09  reddawg
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

 Revision 1.14  2004/07/17 02:38:31  reddawg
 Fixed a few problems

 Revision 1.13  2004/07/14 12:42:46  reddawg
 fdc: fdcInit to fdc_init
 Changed Startup Routines

 Revision 1.12  2004/06/04 10:19:42  reddawg
 notes: we compile again, thank g-d anyways i was about to cry

 Revision 1.11  2004/05/20 22:51:09  reddawg
 Cleaned Up Warnings

 Revision 1.10  2004/05/19 23:36:52  reddawg
 Bug Fixes

 Revision 1.9  2004/05/19 15:31:27  reddawg
 Fixed up the rest of the references

 Revision 1.8  2004/05/19 15:26:33  reddawg
 Fixed reference issues due to changes in driver subsystem

 Revision 1.7  2004/05/10 02:23:24  reddawg
 Minor Changes To Source Code To Prepare It For Open Source Release

 Revision 1.6  2004/04/30 14:16:04  reddawg
 Fixed all the datatypes to be consistant uInt8,uInt16,uInt32,Int8,Int16,Int32

 Revision 1.5  2004/04/29 15:29:20  reddawg
 Fixed All Running Issues

 Revision 1.4  2004/04/28 02:22:54  reddawg
 This is a fiarly large commit but we are starting to use new driver model
 all around

 Revision 1.3  2004/04/26 22:22:33  reddawg
 DevFS now uses correct size of device

 Revision 1.2  2004/04/22 21:20:05  reddawg
 FDC now adds drives to the devfs

 Revision 1.1.1.1  2004/04/15 12:07:09  reddawg
 UbixOS v1.0

 Revision 1.6  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/

