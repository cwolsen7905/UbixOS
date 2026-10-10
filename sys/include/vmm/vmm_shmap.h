/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * MAP_SHARED write-back registry (sys/vmm/vmm_shmap.c), aarch64 + x86_64.
 */

#ifndef _VMM_VMM_SHMAP_H_
#define _VMM_VMM_SHMAP_H_

#include <sys/types.h>
#include <ubixos/sched.h>

/* Record a MAP_SHARED + PROT_WRITE file mapping of @pid at @va. */
void vmm_shmap_register(pidType pid, uintptr_t va, size_t len, off_t foff, const char *path);
/* munmap: write back and forget mappings overlapping [@va, @va+@len). */
void vmm_shmap_unmap(pidType pid, uintptr_t va, size_t len);
/* msync: write back mappings overlapping [@va, @va+@len), keep them. */
void vmm_shmap_sync(pidType pid, uintptr_t va, size_t len);
/* exit/exec: write back and forget all of @pid's mappings (call before the
 * address space is torn down). */
void vmm_shmap_exit(pidType pid);

/* Arch: kernel-readable pointer to the current task's page at user @va, or
 * NULL if it is not mapped. */
void *md_user_page_kva(uintptr_t va);

#endif /* _VMM_VMM_SHMAP_H_ */
