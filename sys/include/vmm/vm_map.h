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

#ifndef _VMM_VM_MAP_H_
#define _VMM_VM_MAP_H_

#include <sys/types.h>
#include <lib/rbtree.h>

/* vm_prot flags — match POSIX mmap PROT_* values intentionally. */
#define VM_PROT_NONE   0x00
#define VM_PROT_READ   0x01
#define VM_PROT_WRITE  0x02
#define VM_PROT_EXEC   0x04
#define VM_PROT_RW     (VM_PROT_READ | VM_PROT_WRITE)
#define VM_PROT_RWX    (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXEC)

/* vm_flags — type and sharing. */
#define VM_MAP_ANON    0x01  /* anonymous (not file-backed) */
#define VM_MAP_FIXED   0x02  /* MAP_FIXED — address was caller-specified */
#define VM_MAP_SHARED  0x04  /* MAP_SHARED */

/*
 * One virtual memory area.  The rb node is the first member so a plain
 * cast between (struct rb_node *) and (vm_map_entry_t *) is valid.
 *
 * vm_start is inclusive, vm_end is exclusive (POSIX convention).
 * vm_vnode / vm_offset are reserved for Phase 2 file-backed mmap.
 */
typedef struct vm_map_entry {
	struct rb_node  rb;         /* intrusive RB node — MUST be first */
	uintptr_t       vm_start;   /* first byte of range (page-aligned) */
	uintptr_t       vm_end;     /* first byte past range (page-aligned) */
	u_int32_t        vm_prot;    /* VM_PROT_* */
	u_int32_t        vm_flags;   /* VM_MAP_* */
	void           *vm_vnode;   /* NULL for anonymous; vnode ptr for file-backed (Phase 2) */
	off_t           vm_offset;  /* offset into vm_vnode (Phase 2) */
} vm_map_entry_t;

/* Per-process VMA tree. */
typedef struct vm_map {
	struct rb_root  vm_root;
	u_int32_t        vm_nentries;
} vm_map_t;

#define VM_MAP_INIT  { RB_ROOT_INIT, 0 }

/* ------------------------------------------------------------------ */
/* vm_map operations                                                    */
/* ------------------------------------------------------------------ */

/*
 * vm_map_insert — add a new VMA covering [start, end) with @prot/@flags.
 * Allocates a vm_map_entry_t via kmalloc.  Returns 0 on success, -1 on OOM.
 * Does not check for overlaps — caller must ensure [start,end) is free.
 */
int vm_map_insert(vm_map_t *map, uintptr_t start, uintptr_t end,
    u_int32_t prot, u_int32_t flags);

/*
 * vm_map_remove — remove and free all VMAs that overlap [start, end).
 * Partial overlaps are trimmed rather than removed entirely.
 */
void vm_map_remove(vm_map_t *map, uintptr_t start, uintptr_t end);

/*
 * vm_map_lookup — find the VMA containing @addr (vm_start <= addr < vm_end).
 * Returns NULL if no VMA covers @addr.
 */
vm_map_entry_t *vm_map_lookup(vm_map_t *map, uintptr_t addr);

/*
 * vm_map_free — remove and free all entries.  Called on exec/exit.
 */
void vm_map_free(vm_map_t *map);

/*
 * vm_map_copy — deep-copy @src into @dst (used by fork).
 * Returns 0 on success, -1 if any kmalloc fails (dst is partially filled).
 */
int vm_map_copy(vm_map_t *dst, const vm_map_t *src);

#endif /* _VMM_VM_MAP_H_ */
