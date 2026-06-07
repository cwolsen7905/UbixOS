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

#ifndef _VMM_VMM_H
#define _VMM_VMM_H

#include <sys/types.h>
#include <vmm/paging.h>
#include <vmm/mmap.h>

#ifdef __cplusplus
extern "C" {
#endif

#define memAvail     1
#define memNotavail  2
#define vmmID       -3

/*
 * The virtual address-space layout constants (VMM_USER_*, VMM_KERN_*,
 * STACK_ADDR, VMM_CHILD_PD_WINDOW, PD_BASE_ADDR, …) are machine-dependent and
 * now live in <machine/vmm_layout.h> (-> <i386/vmm_layout.h>), so a 64-bit / ARM
 * port can supply its own.  See docs/architecture/i386-page-directory-map.md.
 */
#include <machine/vmm_layout.h>

    extern struct spinLock pdSpinLock;

    typedef struct {
            u_int32_t pageAddr;
            u_int16_t status;
            u_int16_t reserved;
            pid_t pid;
            int cowCounter;
    } vmm_page_info_t;

    typedef enum {
        VMM_FREE = 0,
        VMM_KEEP = 1
    } unmapFlags_t;

    extern u_int32_t numPages;
    extern vmm_page_info_t *vmmMemoryMap;
    extern u_int32_t vmm_bitmap_phys;

    int vmm_init();
    int vmm_mem_map_init();
    u_int32_t count_memory();
    uintptr_t vmm_find_free_page(pidType pid);
    int free_page(uintptr_t pageAddr);
    int adjust_cow_counter(uintptr_t baseAddr, int adjustment);
    void vmm_share_ref(uintptr_t phys);
    void vmm_free_process_pages(pidType pid);
    u_int32_t vmm_audit_orphan_pages(void);

    int vmm_alloc_page_table(u_int32_t, pidType);
    void vmm_unmap_page(u_int32_t, unmapFlags_t);
    void vmm_unmap_pages(void*, u_int32_t, unmapFlags_t);
    int vmm_free_virtual_page(u_int32_t addr);
    uintptr_t vmm_share_region(uintptr_t vaddr, size_t size, pidType dst_pid);

#ifdef __cplusplus
}
#endif

#endif // _VMM_VMM_H
