/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VMM_PAGING_H_
#define _VMM_PAGING_H_

#include <sys/types.h>
#include <sys/sysproto_posix.h>
#include <sys/thread.h>

#define PAGE_SHIFT      12              // Page Shift
#define PAGE_SIZE       0x1000          // Page Size
#define PAGE_MASK       (PAGE_SIZE-1)   // Page Mask

#define PD_INDEX(v_addr)  (v_addr >> 22)                        // Calc Page Directory Index
#define PD_OFFSET(v_addr) (v_addr >> 0xA)                       // Calc Page Directory OFfset
#define PT_INDEX(v_addr)  ((v_addr >> 12) & 0x03FF)             // Calc Page Table Offset
#define PD_BASE_ADDR2     ((PAGE_SIZE << 0xA) + VMM_KERN_START) // Find Out What This Was For

#define PD_BASE_ADDR      0xC0400000 // Page Directory Addressable Base Address
#define PT_BASE_ADDR      0xC0000000 // Page Table Addressable Base Address

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
#define PAGE_SHARED         0x00000100   /* borrowed from another process; skip freePage on unmap */
#define PAGE_COW            0x00000200
#define PAGE_STACK          0x00000400
#define PAGE_WIRED          0x00000800

/* When PAGE_PRESENT=0, hardware ignores all other bits; bits[31:12] hold the
 * swap slot number and PAGE_SWAPPED marks the PTE as a valid swap reference. */
#define PAGE_SWAPPED        0x00000002
#define PTE_SWAP_SLOT(pte)      ((u_int32_t)(pte) >> 12)
#define PTE_SWAP_ENCODE(slot)   (((u_int32_t)(slot) << 12) | PAGE_SWAPPED)

#define PAGE_DEFAULT        (PAGE_PRESENT|PAGE_WRITE|PAGE_USER)
#define KERNEL_PAGE_DEFAULT (PAGE_PRESENT|PAGE_WRITE)

#define trunc_page(x)   ((x) & ~PAGE_MASK)
#define round_page(x)   (((x) + PAGE_MASK) & ~PAGE_MASK)

#define ctob(x)         ((x)<<PAGE_SHIFT)
#define btoc(x)         (((vm_offset_t)(x)+PAGE_MASK)>>PAGE_SHIFT)

int vmm_clearVirtualPage(u_int32_t pageAddr);

void *vmm_mapFromTask(pidType, void *, u_int32_t);
void *vmm_copyVirtualSpace(pidType);
void *vmm_getFreePage(pidType);
void *vmm_getFreeKernelPage(pidType pid, u_int16_t count);
void *vmm_createVirtualSpace(pidType);
void *vmm_getFreeVirtualPage(pidType, int, int);
void *vmm_reserve_anon_range(pidType pid, int count);

u_int32_t vmm_getPhysicalAddr(u_int32_t);
u_int32_t vmm_getRealAddr(u_int32_t);
int vmm_setPageAttributes(u_int32_t, u_int16_t);
int vmm_remapPage(u_int32_t, u_int32_t, u_int16_t, pidType, int haveLock);
int vmm_remapIOPage(u_int32_t phys, u_int16_t perms, pidType pid);
int vmm_pagingInit();
void *vmm_getFreeMallocPage(u_int16_t count);
//void vmm_pageFault( u_int32_t, u_int32_t, u_int32_t );
void vmm_pageFault(struct trapframe *, u_int32_t);
void _vmm_pageFault();
int mmap(struct thread *, struct sys_mmap_args *);
int obreak(struct thread *, struct obreak_args *);
int munmap(struct thread *, struct sys_munmap_args *);

int vmm_cleanVirtualSpace(u_int32_t);
void *vmm_getFreeVirtualPage(pidType pid, int count, int type);

extern u_int32_t *kernelPageDirectory;

#endif
