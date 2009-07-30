/*****************************************************************************************
 Copyright (c) 2002-2004, 2009 The UbixOS Project
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

#ifndef _VMM_H
#define _VMM_H

#include <vmm/paging.h>
#include <ubixos/types.h>
#include <ubixos/sched.h>

#define vmmID       -3
#define vmmMemoryMapAddr 0xE6667000

typedef struct {
  uInt32  pageAddr;
  uInt16 status;
  uInt16 reserved;
  pid_t  pid;
  int    cowCounter;
  } mMap;

/* Externs */
extern int numPages;
extern mMap *vmmMemoryMap;
extern u_int32_t *kernelPageDirectory;

/* Callable Functions */
void      *vmm_getFreeVirtualPage_old(pidType,int,int);
void      *vmm_getFreeVirtualPage(pidType pid,int count,int type,u_int32_t startAddr); /* Returns free virtual page */
void      *vmm_getFreeKernelPage(pidType pid,u_int32_t count);
void      *vmm_getFreeMallocPage(u_int16_t count);
u_int32_t vmm_findFreePage(pidType pid);
int        vmm_cleanVirtualSpace(u_int32_t addr);
int        vmm_zeroVirtualPage(u_int32_t pageAddr);
void       vmm_unmapPages(u_int32_t addr,u_int32_t count,u_int16_t flags);
void      *vmm_mapFromTask(pidType pid,u_int32_t baseAddr,u_int32_t size);
void      *vmm_copyVirtualSpace(pidType);
void      *vmmCreateVirtualSpace(pidType);
u_int32_t  vmm_getPhysicalAddr(u_int32_t);
void       vmm_setPageAttributes(u_int32_t,u_int16_t);
int        vmm_remapPage(u_int32_t,u_int32_t,u_int16_t);
void vmm_freePage(u_int32_t pageAddr);
void vmm_adjustCowCounter(u_int32_t pageAddr,int adjustment);
void vmm_freeProcessPages(kTask_t *tmpTask);

/* Interrupt Functions */
void vmm_pageFault(u_int32_t,u_int32_t,u_int32_t);
void _vmm_pageFault();

/* Initialization Functions */
int vmm_pagingInit();
int vmm_init();
int vmm_memMapInit();
int vmm_countMemory();

#endif

/***
 $Log$
 Revision 1.4  2009/07/09 04:01:15  reddawg
 More Sanity Checks

 Revision 1.3  2009/07/08 21:20:13  reddawg
 Getting There


 END
 ***/
