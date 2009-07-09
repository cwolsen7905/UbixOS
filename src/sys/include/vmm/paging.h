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

#define pageLength            0x00000400
#define PAGE_TABLES_BASE_ADDR 0xBFC00000 /* Base address of page tables in virtual area 3GB - 4MB */ 
#define PARENT_PAGEDIR_ADDR   0x100000   /* Address at which the page directory is stored */
#define PAGE_COW              0x00000200
#define PAGE_STACK            0x00000400
#define PAGE_WIRED            0x00000600
#define PAGE_PRESENT          0x00000001
#define PAGE_WRITE            0x00000002
#define PAGE_USER             0x00000004
#define PAGE_DEFAULT        (PAGE_PRESENT|PAGE_WRITE|PAGE_USER)
#define KERNEL_PAGE_DEFAULT (PAGE_PRESENT|PAGE_WRITE)

#define PAGE_SHIFT          12              /* LOG2(PAGE_SIZE) */
#define PAGE_SIZE           (1<<PAGE_SHIFT) /* bytes/page */
#define PAGE_ENTRIES        (PAGE_SIZE/4)   /* Entries in a page */
#define PAGE_MASK           (PAGE_SIZE-1)   /* Unset all but the 0xFFF Bits */
#define PAGE_UNMASK         0xFFFFF000      /* Unset the 0xFFF Bits */
#define PAGE_AVAILABLE      0x1             /* Set the page to be available */
#define PAGE_UNAVAILABLE    0x2             /* Set the page to be in use */

#define PAGE_KERNEL_ENTRY   0x300           /* First kernel page entry */
#define PAGE_DIR_SPACE      0x2FF           /* Virtual Area For Pages 4MB */

#define PTI(x)              ((x >> 12) & 0x3FF) /* Return page directory index of address */
#define PDI(x)              (x >> 22)           /* Return page table index of address     */
#define trunc_page(x)       ((x) & ~PAGE_MASK)
#define round_page(x)       (((x) + PAGE_MASK) & ~PAGE_MASK)
#define ctob(x)             ((x)<<PAGE_SHIFT)
#define btoc(x)             (((vm_offset_t)(x)+PAGE_MASK)>>PAGE_SHIFT)


int mmap(struct thread *,struct mmap_args *);
int obreak(struct thread *,struct obreak_args *);
int munmap(struct thread *,struct munmap_args *);

#endif

/***
 END
 ***/

