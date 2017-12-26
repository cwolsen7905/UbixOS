/*-
 * Copyright (c) 2002-2004, 2016, 2017 The UbixOS Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of
 * conditions, the following disclaimer and the list of authors.  Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions, the following
 * disclaimer and the list of authors in the documentation and/or other materials provided
 * with the distribution. Neither the name of the UbixOS Project nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VMM_PAGING_H_
#define _VMM_PAGING_H_

#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/thread.h>

#define PAGE_SHIFT      12              // Page Shift
#define PAGE_SIZE       0x1000          // Page Size
#define PAGE_MASK       (PAGE_SIZE-1)   // Page Mask

#define PD_INDEX(v_addr)  (v_addr >> 22)                        // Calc Page Directory Index
#define PD_OFFSET(v_addr) (v_addr >> 0xA)                       // Calc Page Directory OFfset
#define PT_INDEX(v_addr)  ((v_addr >> 12) & 0x03FF)             // Calc Page Table Offset
#define PD_BASE_ADDR2     ((PAGE_SIZE << 0xA) + VMM_KERN_START) // Find Out What This Was For

#define PD_BASE_ADDR      0xC0400000 //Page Directory Base Address
#define PT_BASE_ADDR      0xC0000000 // Page Table Base Address

#define PD_ENTRIES        (PAGE_SIZE/4) //Return Page Directory Entries
#define PT_ENTRIES        (PAGE_SIZE/4) //Return Page Table Entries

#define VM_THRD           0 // Thread
#define VM_TASK           1 // Task


// Page Flags
#define PAGE_PRESENT        0x00000001
#define PAGE_WRITE          0x00000002
#define PAGE_USER           0x00000004
#define PAGE_WRITE_THROUGH  0x00000008
#define PAGE_CACHE_DISABLED 0x00000010
#define PAGE_ACCESSED       0x00000020
#define PAGE_DIRTY          0x00000040
#define PAGE_GLOBAL         0x00000080
#define PAGE_               0x00000100
#define PAGE_COW            0x00000200
#define PAGE_STACK          0x00000400
#define PAGE_WIRED          0x00000800

#define PAGE_DEFAULT        (PAGE_PRESENT|PAGE_WRITE|PAGE_USER)
#define KERNEL_PAGE_DEFAULT (PAGE_PRESENT|PAGE_WRITE)

#define trunc_page(x)   ((x) & ~PAGE_MASK)
#define round_page(x)   (((x) + PAGE_MASK) & ~PAGE_MASK)

#define ctob(x)         ((x)<<PAGE_SHIFT)
#define btoc(x)         (((vm_offset_t)(x)+PAGE_MASK)>>PAGE_SHIFT)


int vmmClearVirtualPage(uInt32 pageAddr);

void vmmUnmapPage(uInt32, int);
void vmmUnmapPages(void *, uInt32);
void *vmmMapFromTask(pidType, void *, uInt32);
void *vmmCopyVirtualSpace(pidType);
void *vmmGetFreePage(pidType);
void *vmmGetFreeKernelPage(pidType pid, uInt16 count);
void *vmmCreateVirtualSpace(pidType);
void *vmmGetFreeVirtualPage(pidType, int, int);

uint32_t vmm_getPhysicalAddr(uint32_t);
uint32_t vmm_getRealAddr(uint32_t);
int vmm_setPageAttributes(uInt32, uInt16);
int vmm_remapPage(uInt32, uInt32, uInt16);
int vmm_pagingInit();
void *vmm_getFreeMallocPage(uInt16 count);
//void vmm_pageFault( uInt32, uInt32, uInt32 );
void vmm_pageFault(struct trapframe *, uint32_t);
void _vmm_pageFault();
int mmap(struct thread *, struct sys_mmap_args *);
int obreak(struct thread *, struct obreak_args *);
int munmap(struct thread *, struct munmap_args *);

int vmm_cleanVirtualSpace(uint32_t);

extern uInt32 *kernelPageDirectory;

#endif
