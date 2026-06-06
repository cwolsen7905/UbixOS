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
	/* mmap2's offset is a 32-bit page count; mask to the low 32 bits before
	 * scaling so a garbage high half (the ABI only passes 32 bits) does not
	 * leave junk in the byte offset / the VMA's vm_offset. */
	uap->pos = (off_t)(u_int32_t)uap->pos * PAGE_SIZE;
	return sys_mmap(td, uap);
}

/**
 * Write back dirty pages of MAP_SHARED writable file mappings in [base, end) to
 * their backing file, then clear each page's dirty bit.  Pages in anonymous or
 * MAP_PRIVATE mappings are skipped (their changes never reach the file).  The
 * last page is clamped to the backing file's size so a page-rounded mapping
 * never extends the file with zero padding.  Shared by msync and munmap.
 */
static void vmm_writeback_range(u_int32_t base, u_int32_t end)
{
	u_int32_t *pd = (u_int32_t *)PD_BASE_ADDR;
	u_int32_t va;

	for (va = base; va < end; va += PAGE_SIZE)
	{
		vm_map_entry_t *vma = vm_map_lookup(&_current->vm_map, va);
		fileDescriptor_t *bfd;
		u_int32_t pdi, *pt, pte, n, off32;

		if (vma == NULL)
			continue;
		if (!(vma->vm_flags & VM_MAP_FILE) || !(vma->vm_flags & VM_MAP_SHARED) ||
		    !(vma->vm_prot & VM_PROT_WRITE) || vma->vm_vnode == NULL)
			continue;

		pdi = PD_INDEX(va);
		if (!(pd[pdi] & PAGE_PRESENT))
			continue;
		pt = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * pdi));
		pte = pt[PT_INDEX(va)];
		if (!(pte & PAGE_PRESENT) || !(pte & PAGE_DIRTY))
			continue;

		bfd = (fileDescriptor_t *)vma->vm_vnode;
		/* The FS read/write paths truncate the offset to 32 bits, and FAT files
		 * are < 4 GB, so do the clamp in 32 bits — using the full off_t here is
		 * wrong if vm_offset carries garbage high bits from a 32-bit mmap2 pos. */
		off32 = (u_int32_t)(vma->vm_offset + (off_t)(va - vma->vm_start));

		/* Clamp to the backing file's size — never grow it past EOF. */
		if (bfd->size <= off32)
			n = 0;
		else if (bfd->size - off32 >= PAGE_SIZE)
			n = PAGE_SIZE;
		else
			n = bfd->size - off32;

		if (n > 0 && bfd->mp != NULL && bfd->mp->fs != NULL && bfd->mp->fs->vfsWrite != NULL)
			bfd->mp->fs->vfsWrite(bfd, (void *)va, (off_t)off32, n);

		pt[PT_INDEX(va)] = pte & ~PAGE_DIRTY;
		asm volatile("invlpg (%0)" : : "r"(va) : "memory");
	}
}

int sys_munmap(struct thread *td, struct sys_munmap_args *uap)
{
	u_int32_t base = (u_int32_t)uap->addr & ~0xFFFU;
	u_int32_t end = base + round_page(uap->len);

	/* Flush MAP_SHARED writable file pages before tearing the mapping down,
	 * while the VMA and its backing fd still exist. */
	vmm_writeback_range(base, end);

	vm_map_remove(&_current->vm_map, base, end);

	for (u_int32_t va = base; va < end; va += PAGE_SIZE)
	{
		vmm_unmap_page(va, VMM_FREE);
	}

	td->td_retval[0] = 0;
	return (0);
}

/**
 * msync(addr, len, flags) — flush dirty MAP_SHARED writable file pages in the
 * range back to their backing file.  flags (MS_ASYNC/MS_SYNC/MS_INVALIDATE) are
 * accepted but the write is always synchronous (vfsWrite); MS_INVALIDATE is a
 * no-op because there is no separate page cache to discard.
 *
 * @return 0 always (an unmapped range simply writes back nothing).
 */
int sys_msync(struct thread *td, struct sys_msync_args *uap)
{
	u_int32_t base = (u_int32_t)uap->addr & ~0xFFFU;
	u_int32_t end = base + round_page(uap->len);

	vmm_writeback_range(base, end);

	td->td_retval[0] = 0;
	return (0);
}

/* madvise behaviour values (match musl/Linux/FreeBSD). */
#define MADV_DONTNEED 4
#define MADV_FREE 8

/**
 * madvise(addr, len, behav).  Only DONTNEED/FREE have an effect: drop the
 * resident pages of the range so they are reclaimed now, keeping the VMA so a
 * later access re-faults (anon → demand-zero, file → demand-read).  Dirty
 * MAP_SHARED file pages are written back first so no un-msync'd data is lost.
 * Only pages covered by a registered VMA are dropped — never bare heap memory
 * that has no demand-fault backing.  All other advices are accepted hints
 * (there is no read-ahead engine to tune).
 *
 * @return 0 always.
 */
int sys_madvise(struct thread *td, struct sys_madvise_args *uap)
{
	u_int32_t base = (u_int32_t)uap->addr & ~0xFFFU;
	u_int32_t end = base + round_page(uap->len);
	u_int32_t va;

	if (uap->behav != MADV_DONTNEED && uap->behav != MADV_FREE)
	{
		td->td_retval[0] = 0;
		return (0);
	}

	vmm_writeback_range(base, end);

	for (va = base; va < end; va += PAGE_SIZE)
	{
		if (vm_map_lookup(&_current->vm_map, va) == NULL)
			continue; /* not a demand-faultable mapping — leave it resident */
		vmm_unmap_page(va, VMM_FREE);
	}

	asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
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
		 * File-backed mmap (VMM plan Phase 2.2) — DEMAND-PAGED.
		 *
		 * Open a private backing fd (re-opened by path so it survives the caller
		 * closing its own fd) and record a VM_MAP_FILE VMA covering the range;
		 * map NO pages now.  The page-fault handler reads each page from the
		 * backing fd on first touch — read-only pages shared through the
		 * file-page cache (one physical copy across processes), writable pages
		 * private.  Trimming overlapping VMAs first gives proper MAP_FIXED
		 * replace semantics (rtld maps a library's anon BSS over the file
		 * segment's tail).
		 *
		 * If a backing fd cannot be opened, fall back to the historical EAGER
		 * read + de-dup so the mapping still works.  File identity for the cache
		 * is kfd->ino (the FAT start cluster — fd->start is not populated).
		 */
		fileDescriptor_t *kfd, *backing;
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
		backing = fopen(kfd->fileName, "r");

		/* --- DEMAND mode: reserve VA, map nothing, record the VMA. --- */
		if (backing != NULL)
		{
			if (uap->addr == NULL)
			{
				void *r = vmm_reserve_anon_range(_current->id, (int)npages);
				if (r == NULL)
				{
					fclose(backing);
					td->td_retval[0] = -1;
					return (ENOMEM);
				}
				base = (u_int32_t)r;
			}
			else
			{
				base = (u_int32_t)uap->addr & 0xFFFFF000;
				for (i = 0; i < npages; i++)
					vmm_unmap_page(base + i * PAGE_SIZE, VMM_FREE);
			}
			vm_map_remove(&_current->vm_map, base, base + npages * PAGE_SIZE);
			vm_map_insert_file(&_current->vm_map,
			                   base,
			                   base + npages * PAGE_SIZE,
			                   uap->prot,
			                   (uap->flags & 0x0001 /* MAP_SHARED */) ? VM_MAP_SHARED : 0,
			                   backing,
			                   uap->pos);
			asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
			td->td_retval[0] = (int)base;
			return 0;
		}

		/* --- Eager fallback (no backing fd): allocate + read + de-dup. --- */
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

		/* Pages are present (eager), so they never fault — record a backing-less
		 * file VMA after trimming overlaps so MAP_FIXED replace semantics hold. */
		vm_map_remove(&_current->vm_map, base, base + npages * PAGE_SIZE);
		vm_map_insert_file(&_current->vm_map,
		                   base,
		                   base + npages * PAGE_SIZE,
		                   uap->prot,
		                   (uap->flags & 0x0001 /* MAP_SHARED */) ? VM_MAP_SHARED : 0,
		                   NULL,
		                   uap->pos);

		/* Flush the TLB for the newly mapped range. */
		asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");

		td->td_retval[0] = (int)base;
	}
	return 0;
}
