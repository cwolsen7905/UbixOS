/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Log: sample_driver.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:07  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:13:57  reddawg
 no message

 Revision 1.2  2004/04/27 20:49:22  reddawg
 Update Sample Driver

 Revision 1.1  2004/04/27 12:04:24  reddawg
 Sample driver for isa interface



 $Id: sample_driver.c 88 2016-01-12 00:11:29Z reddawg $

*****************************************************************************************/

/********************************************************
 This sample driver is good for an ISA device you can
 either probe or hand set irq and ioAddr information if 
 ********************************************************/

#include <sys/device.h>

struct devInfo {
  int irq;
  int ioAddr;
  }

static struct devInfo sampleInfo;

void sampleRegister() {
  int sampleMajor = 0; /* Major ID */
  int sampleMinor = 0; /* Minor ID */
  
  deviceAdd(sampleMajor,sampleMinor,"c",sampleRead,sampleWrite,sampleReset,sampleInit,sampleIoctl,sampleStop,sampleStart,sampleStandby,sampleInfo)
  }


int sampleRead(struct deviceNode *dev,void *ptr,uInt32 offset,uInt32 length) {
  /* Read From Device */
  return(0);
  }

int sampleWrite(struct deviceNode *dev,void *ptr,uInt32 offset,uInt32 length) {
  /* WritE to Device */
  return(0);
  }

int sampleReset(struct deviceNode *dev) {
  /* Device Reset */
  return(0);
  }

int sampleInit(struct deviceNode *dev) {
  /* Device Initialization                            */
  /* Also Set Device Size If IT Is A Character Device */
  dev->size = 1024;
  return(0);
  }

int sampleIoctl(struct deviceNode *dev) {
  /* Device IO Control */
  return(0);
  }

int sampleStop(struct deviceNode *dev) {
  /* Stop this device */
  return(0);
  }

int sampleStart(struct deviceNode *dev) {
  /* Start This Device */
  return(0);
  } 

int sampleStandby(struct deviceNode *dev) {
  /* Do all things needed to allow this device to go into standby mode */
  return(0); 
  }
