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
 * i386 virtual address-space layout (per process).  Canonical reference:
 * docs/architecture/i386-page-directory-map.md.  The low 4 MB is identity
 * mapped in every space; the kernel half (>= VMM_KERN_START, PDEs 770-1023) is
 * shared and re-synced across spaces on fork / create / share_region.
 *
 *   0x00000000 .. 0x003FFFFF   identity map (PDE 0): IVT/BIOS/VGA, kernel TSS
 *                              (0x4200), AP GDT (0x20000), v86 stub; kernel
 *                              image loads at 0x300000
 *   ..0x007FEFFF               "kernel code" window (VMM_KERN_CODE_*)
 *   0x007FF000                 per-process LDT (LDT[1] = user TLS, %gs=0x0F)
 *   0x00800000 .. 0xBFFFFFFF   user space (VMM_USER_*); stack top = STACK_ADDR
 *   0xC0000000 / 0xC0400000    recursive page-table / page-directory self-maps
 *   0xC0800000 .. 0xFDFFFFFF   kernel space: kmalloc heap, kernel pages
 *                              (PDEs 770-1015; the heap-allocation range)
 *   0xFE000000 .. 0xFFFFFFFF   reserved kernel high range (PDEs 1016-1023);
 *                              LAPIC MMIO at 0xFEE00000 (PDE 1019) lives here
 *
 * Note: the kernel-PD *sync* covers PDEs 770-1023 (so the LAPIC is reachable
 * from any address space), while kmalloc *allocation* stays within 770-1015.
 */

#define STACK_ADDR 0xBFFFFFFF                /* default user stack top (grows down) */

#define VMM_MMAP_ADDR_PMODE  VMM_KERN_START  /* where the page-frame bitmap is remapped */
#define VMM_MMAP_ADDR_RMODE  0x101000        /* legacy pre-paging bitmap staging addr   */

#define VMM_KERN_CODE_START 0x00000000
#define VMM_KERN_CODE_END   0x007FEFFF

#define VMM_USER_LDT   0x007FF000             /* per-process LDT page (user TLS) */

#define VMM_USER_START 0x00800000
#define VMM_USER_END   0xBFFFFFFF

#define VMM_PAGE_DIRS  0xC0000000             /* recursive page-table self-map  */
#define VMM_PAGE_DIR   0xC0400000             /* recursive page-directory self-map */

#define VMM_KERN_START 0xC0800000             /* kernel space start (PDE 770), shared */
#define VMM_KERN_END   0xFDFFFFFF             /* kmalloc-heap top (PDE 1015)          */

#define VMM_KERN_STACK_START 0xFE000000       /* reserved kernel high range (PDE 1016) */
#define VMM_KERN_STACK_END   0xFFFFFFFF       /* .. PDE 1023; holds LAPIC MMIO @0xFEE00000 */

/* Per-process transient window used to map another task's page directory while
 * forking / sharing a region (then unmapped).  Placed in the USER half on
 * purpose so the temporary mapping stays private to the acting process rather
 * than leaking into the shared kernel half.  Must not overlap a live mapping. */
#define VMM_CHILD_PD_WINDOW  0x5A00000

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
    u_int32_t vmm_find_free_page(pidType pid);
    int free_page(u_int32_t pageAddr);
    int adjust_cow_counter(u_int32_t baseAddr, int adjustment);
    void vmm_free_process_pages(pidType pid);

    int vmm_alloc_page_table(u_int32_t, pidType);
    void vmm_unmap_page(u_int32_t, unmapFlags_t);
    void vmm_unmap_pages(void*, u_int32_t, unmapFlags_t);
    int vmm_free_virtual_page(u_int32_t addr);
    uintptr_t vmm_share_region(uintptr_t vaddr, size_t size, pidType dst_pid);

#ifdef __cplusplus
}
#endif

#endif // _VMM_VMM_H
