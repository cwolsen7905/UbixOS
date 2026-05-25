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
#include <sys/types.h>
#include <sys/errno.h>
#include <lib/kprintf.h>
#include <string.h>
#include <sys/descrip.h>
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
	vm_offset_t addr = 0;
	char *tmp = NULL;
	struct file *fd = NULL;
	int x;

	// kprintf("[%s:%i] mmap(%i-0x%X-%i)", __FILE__, __LINE__, uap->fd, uap->addr, uap->len);

	addr = (vm_offset_t)uap->addr;

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

		getfd(td, &fd, uap->fd);

		if (uap->addr == NULL)
		{
			tmp = (char *)vmm_get_free_virtual_page(_current->id, round_page(uap->len) / 0x1000, VM_TASK);
		}
		else
		{

			for (x = 0; x < round_page(uap->len); x += 0x1000)
			{

				vmm_unmap_page(((u_int32_t)uap->addr & 0xFFFFF000) + x, VMM_KEEP);

				/* Make readonly and read/write !!! */
				if (vmm_remap_page(vmm_find_free_page(_current->id),
				                  (((u_int32_t)uap->addr & 0xFFFFF000) + x),
				                  PAGE_DEFAULT,
				                  _current->id,
				                  0) == 0)
				{
					K_PANIC("Remap Page Failed");
				}
			}
			// kprintf("(tmp1: 0x%X)", tmp);
			tmp = uap->addr;
			// kprintf("(tmp2: 0x%X)", tmp);
		}

		kern_fseek(fd->fd, uap->pos, 0);
		fread(tmp, uap->len, 0x1, fd->fd);

		td->td_retval[0] = (int)(uintptr_t)tmp;

		if (tmp == (caddr_t)-1)
		{
			kpanic("MMAP_FAILED");
		}
	}
	return 0;
}
