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

#include <vmm/vmm.h>
#include <vmm/mmap.h>
#include <vmm/vm_map.h>
#include <vmm/paging.h>
#include <vmm/vm_filecache.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <lib/kprintf.h>
#include <string.h>
#include <sys/descrip.h>
#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>

int sys_mmap2(struct thread *td, struct sys_mmap_args *uap)
{
	uap->pos = (off_t)uap->pos * PAGE_SIZE;
	return sys_mmap(td, uap);
}

int sys_munmap(struct thread *td, struct sys_munmap_args *uap)
{
	u_int32_t base = (u_int32_t)uap->addr & ~0xFFFU;
	u_int32_t end = base + round_page(uap->len);

	vm_map_remove(&_current->vm_map, base, end);

	for (u_int32_t va = base; va < end; va += PAGE_SIZE)
	{
		vmm_unmap_page(va, VMM_FREE);
	}

	td->td_retval[0] = 0;
	return (0);
}

int sys_mmap(struct thread *td, struct sys_mmap_args *uap)
{
	struct file *fd = NULL;
	int x;

	if (uap->fd == -1)
	{
		if (uap->addr != NULL)
		{
			/* MAP_FIXED anonymous: caller specifies address; unmap any existing
			 * mapping there, record the VMA, and let page faults back pages lazily. */
			u_int32_t map_base = (u_int32_t)uap->addr & 0xFFFFF000;
			u_int32_t map_end = map_base + round_page(uap->len);
			if (map_base < VMM_USER_START || map_end > VMM_USER_END)
			{
				td->td_retval[0] = -1;
				return (EINVAL);
			}
			for (x = 0; x < (int)round_page(uap->len); x += 0x1000)
			{
				vmm_unmap_page(map_base + x, VMM_FREE);
			}
			/* Trim/replace any overlapping VMAs (e.g. the file segment this anon
			 * BSS is mapped over) so the demand-zero lookup resolves here. */
			vm_map_remove(&_current->vm_map, map_base, map_end);
			vm_map_insert(&_current->vm_map, map_base, map_end, VM_PROT_RW, VM_MAP_ANON | VM_MAP_FIXED);
			td->td_retval[0] = (int)(u_int32_t)uap->addr;
			return 0;
		}

		/* Anonymous, no fixed address: reserve a VA range without backing pages. */
		int npages = (int)(round_page(uap->len) / PAGE_SIZE);
		void *mmap_tmp = vmm_reserve_anon_range(_current->id, npages);
		if (mmap_tmp == NULL)
		{
			td->td_retval[0] = -1;
			return (ENOMEM);
		}
		vm_map_insert(&_current->vm_map,
		              (uintptr_t)mmap_tmp,
		              (uintptr_t)mmap_tmp + round_page(uap->len),
		              VM_PROT_RW,
		              VM_MAP_ANON);
		td->td_retval[0] = (int)(uintptr_t)mmap_tmp;
		return 0;
	}
	else
	{
		/*
		 * File-backed mmap (VMM plan Phase 2.2).
		 *
		 * Allocate + read the mapping exactly as the historical path did (one
		 * sequential read into freshly allocated pages — proven correct), then
		 * de-duplicate read-only pages into the shared file-page cache: the
		 * first mapper of a given (file, offset) page publishes its page; later
		 * mappers discard their just-read copy and map the shared physical page
		 * read-only with PAGE_SHARED.  A shared library's text/rodata therefore
		 * ends up as one physical copy across all processes, not one per process.
		 *
		 * Writable pages stay private (historical behaviour); MAP_SHARED
		 * writable is private for now (no write-back — that is msync, future).
		 * File identity is kfd->ino (the FAT start cluster, unique per file —
		 * fd->start is not populated by the FAT driver).
		 */
		fileDescriptor_t *kfd;
		char *tmp;
		u_int32_t npages, base, va, phys, winner, i;
		off_t foff;
		int readonly;

		getfd(td, &fd, uap->fd);
		if (fd == NULL || fd->fd == NULL)
		{
			td->td_retval[0] = -1;
			return (EBADF);
		}
		kfd = fd->fd;
		npages = round_page(uap->len) / PAGE_SIZE;

		/* --- Allocate the VA range and read the file (historical path). --- */
		if (uap->addr == NULL)
		{
			tmp = (char *)vmm_get_free_virtual_page(_current->id, npages, VM_TASK);
		}
		else
		{
			for (i = 0; i < npages; i++)
			{
				vmm_unmap_page(((u_int32_t)uap->addr & 0xFFFFF000) + i * PAGE_SIZE, VMM_KEEP);
				if (vmm_remap_page(vmm_find_free_page(_current->id),
				                   ((u_int32_t)uap->addr & 0xFFFFF000) + i * PAGE_SIZE,
				                   PAGE_DEFAULT,
				                   _current->id,
				                   0) == 0)
					K_PANIC("Remap Page Failed");
			}
			tmp = uap->addr;
		}
		if (tmp == NULL || tmp == (caddr_t)-1)
		{
			td->td_retval[0] = -1;
			return (ENOMEM);
		}
		base = (u_int32_t)tmp;

		kern_fseek(kfd, uap->pos, 0);
		fread(tmp, uap->len, 0x1, kfd);

		/* --- De-duplicate read-only pages into the shared file-page cache. --- */
		readonly = ((uap->prot & VM_PROT_WRITE) == 0) && (kfd->ino != 0);
		if (readonly)
		{
			for (i = 0; i < npages; i++)
			{
				va = base + i * PAGE_SIZE;
				foff = uap->pos + (off_t)i * PAGE_SIZE;

				phys = vm_filecache_lookup_ref(kfd->mp, kfd->ino, foff);
				if (phys != 0)
				{
					/* Already cached — drop our copy, map the shared page. */
					vmm_unmap_page(va, VMM_FREE);
					if (vmm_remap_page(
					        phys, va, PAGE_PRESENT | PAGE_USER | PAGE_SHARED, _current->id, 0) == 0)
						K_PANIC("filecache remap (hit) failed");
					continue;
				}

				/* Publish our freshly read page as the shared copy. */
				phys = vmm_get_physical_addr(va);
				winner = 0;
				if (vm_filecache_insert(kfd->mp, kfd->ino, foff, phys, &winner) == 0)
				{
					/* We own the entry.  The page is already mapped at va (we
					 * just read it); downgrade the existing PTE in place to
					 * shared read-only (same phys) — vmm_remap_page refuses to
					 * overwrite a live mapping, so change the attributes instead.
					 * Teardown then unref's (not frees) it through the cache. */
					vmm_set_page_attributes(va, PAGE_PRESENT | PAGE_USER | PAGE_SHARED);
				}
				else if (winner != 0)
				{
					/* Lost the insert race — adopt the winner, drop ours. */
					vmm_unmap_page(va, VMM_FREE);
					if (vmm_remap_page(
					        winner, va, PAGE_PRESENT | PAGE_USER | PAGE_SHARED, _current->id, 0) ==
					    0)
						K_PANIC("filecache remap (race) failed");
				}
			}
		}

		/*
		 * Record a file-backed VMA (VM_MAP_FILE).  First TRIM any overlapping
		 * VMAs in this range — proper mmap-replace semantics — so that when
		 * rtld later maps a library's anon BSS with MAP_FIXED over the tail of
		 * this file segment, the anon mapping (which also trims) wins the
		 * overlap and demand-zero lookups resolve to the anon VMA, not this
		 * file VMA (the bug fixed in d63d9a8ee, done properly here).
		 *
		 * The VMA owns a private backing fd (re-opened by path) so it survives
		 * the caller closing its fd; the page-fault handler will use it to
		 * demand-read pages once Stage B drops the eager read above.  (Stage A:
		 * pages are still eager, so the VMA is metadata + the trim fix.)
		 */
		vm_map_remove(&_current->vm_map, base, base + npages * PAGE_SIZE);
		{
			fileDescriptor_t *backing = fopen(kfd->fileName, "r");
			vm_map_insert_file(&_current->vm_map,
			                   base,
			                   base + npages * PAGE_SIZE,
			                   uap->prot,
			                   (uap->flags & 0x0001 /* MAP_SHARED */) ? VM_MAP_SHARED : 0,
			                   backing,
			                   uap->pos);
		}

		/* Flush the TLB for the newly mapped range. */
		asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");

		td->td_retval[0] = (int)base;
	}
	return 0;
}
