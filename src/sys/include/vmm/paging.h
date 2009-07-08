/*****************************************************************************************
 Copyright (c) 2002-2004,2009 The UbixOS Project
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

#ifndef _PAGING_H
#define _PAGING_H

#include <ubixos/types.h>
#include <sys/sysproto.h>
#include <sys/thread.h>

#define VM_THRD             0
#define VM_TASK             1

#define pageLength          0x00000400
#define PAGE_TABLES_BASE_ADDR 0xBFC00000 /* Base address of page tables in virtual area 3GB - 4MB */ 
#define PARENT_PAGEDIR_ADDR   0x100000   /* Address at which the page directory is stored */

#define PAGE_COW            0x00000200
#define PAGE_STACK          0x00000400
#define PAGE_WIRED          0x00000600
#define PAGE_PRESENT        0x00000001
#define PAGE_WRITE          0x00000002
#define PAGE_USER           0x00000004
#define PAGE_DEFAULT        (PAGE_PRESENT|PAGE_WRITE|PAGE_USER)
#define KERNEL_PAGE_DEFAULT (PAGE_PRESENT|PAGE_WRITE)

#define PAGE_SHIFT          12              /* LOG2(PAGE_SIZE) */
#define PAGE_SIZE           (1<<PAGE_SHIFT) /* bytes/page */
#define PAGE_MASK           (PAGE_SIZE-1)
#define PAGE_ENTRIES        (PAGE_SIZE/4)

#define PTI(x)              ((x >> 12) & 0x3FF)
#define PDI(x)              (x >> 22)
#define trunc_page(x)       ((x) & ~PAGE_MASK)
#define round_page(x)       (((x) + PAGE_MASK) & ~PAGE_MASK)
#define ctob(x)             ((x)<<PAGE_SHIFT)
#define btoc(x)             (((vm_offset_t)(x)+PAGE_MASK)>>PAGE_SHIFT)


int   vmm_zeroVirtualPage(u_int32_t pageAddr);
void  vmm_unmapPages(u_int32_t addr,u_int32_t count,u_int16_t flags);
void *vmm_mapFromTask(pidType pid,u_int32_t baseAddr,u_int32_t size);
void *vmmCopyVirtualSpace(pidType);
void *vmmCreateVirtualSpace(pidType);
void *vmmGetFreeVirtualPage(pidType,int,int);

void *vmm_getFreeKernelPage(pidType pid,u_int32_t count);
u_int32_t vmm_getPhysicalAddr(u_int32_t);
void    vmm_setPageAttributes(u_int32_t,u_int16_t);
int    vmm_remapPage(u_int32_t,u_int32_t,u_int16_t);
int    vmm_pagingInit();
void  *vmm_getFreeMallocPage(u_int16_t count);
void   vmm_pageFault(u_int32_t,u_int32_t,u_int32_t);
void  _vmm_pageFault();
int mmap(struct thread *,struct mmap_args *);
int obreak(struct thread *,struct obreak_args *);
int munmap(struct thread *,struct munmap_args *);


extern u_int32_t *kernelPageDirectory;

#endif

/***
 END
 ***/

