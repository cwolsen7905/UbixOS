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

 $Id: pci.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _PCI_H
#define _PCI_H

#include <sys/types.h>


struct pciConfig {
  uint16_t vendorId;
  uint16_t deviceId;

  uint16_t command;
  uint16_t status;

  uint8_t  revisionId;
  uint8_t  interface;
  uint8_t  subClass;
  uint8_t  baseClass;

  uint8_t  cacheLineSize;
  uint8_t  latencyTimer;
  uint8_t  headerType;
  uint8_t  bist;

  uint32_t bar[6];

  uint32_t cisPointer;
  uint16_t subsysVendorID;
  uint16_t subsystemID;
  uint32_t epromAddr;
  uint16_t capabilites;
  uint16_t res1;
  uint32_t res2;
  uint8_t intLine;
  uint8_t intPin;
  uint8_t minGrant;
  uint8_t maxLatency;

  /* device info */
  //uint8_t  bus;
  //uint8_t  dev;
  //uint8_t  func;
  //uint8_t  irq;
  //uint8_t irqLine;

  /* base registers */
  //uInt32 base[6];
  //uInt32 size[6];

  //uint16_t subsysVendor;
  //uint16_t subsys;
  
  };

struct confadd {
  uint8_t reg:8;
  uint8_t func:3;
  uint8_t dev:5;
  uint8_t bus:8;
  uint8_t rsvd:7;
  uint8_t enable:1;
  };

#define countof(a)     (sizeof(a) / sizeof(a[0]))
  
int pci_init();


bool pciProbe(int bus,int dev,int func,struct pciConfig *cfg);
uInt32 pciRead(int bus, int dev, int func, int reg, int bytes);
void pciWrite(int bus,int dev,int func,int reg,uInt32 v,int bytes);

#endif

/***
 $Log: pci.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:51  reddawg
 no message

 Revision 1.3  2004/08/20 16:49:11  reddawg
 PCI Updates - More to follow as PCI system gets revamped

 Revision 1.2  2004/05/21 15:05:07  reddawg
 Cleaned up


 END
 ***/
