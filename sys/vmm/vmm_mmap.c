/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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
	uint32_t base = (uint32_t)uap->addr & ~0xFFFU;
	uint32_t end = base + round_page(uap->len);

	for (uint32_t va = base; va < end; va += PAGE_SIZE) {
		vmm_unmapPage(va, VMM_FREE);
}

	td->td_retval[0] = 0;
	return (0);
}

int sys_mmap(struct thread *td, struct sys_mmap_args *uap)
{
	vm_offset_t addr = 0x0;
	char *tmp = 0x0;
	struct file *fd = 0x0;
	int x;

	// kprintf("[%s:%i] mmap(%i-0x%X-%i)", __FILE__, __LINE__, uap->fd, uap->addr, uap->len);

	addr = (vm_offset_t)uap->addr;

	if (uap->fd == -1)
	{
		if (uap->addr != 0x0)
		{
			uint32_t map_base = (uint32_t)uap->addr & 0xFFFFF000;
			if (map_base < VMM_USER_START || map_base + round_page(uap->len) > VMM_USER_END)
			{
				td->td_retval[0] = -1;
				return (EINVAL);
			}
			for (x = 0x0; x < round_page(uap->len); x += 0x1000)
			{
				vmm_unmapPage(map_base + x, VMM_FREE);
				if (vmm_remapPage(vmm_findFreePage(_current->id), map_base + x, PAGE_DEFAULT, _current->id, 0) == 0x0)
					K_PANIC("Remap Page Failed");
			}
			tmp = uap->addr;
			bzero(tmp, uap->len);
			td->td_retval[0] = (uint32_t)tmp;
			return (0x0);
		}

		void *mmap_tmp = vmm_getFreeVirtualPage(_current->id, round_page(uap->len) / 0x1000, VM_TASK);
		td->td_retval[0] = (int)mmap_tmp;
		// kprintf("(tmp5: 0x%X)", td->td_retval[0]);
		bzero(mmap_tmp, uap->len);
		return (0x0); // vmm_getFreeVirtualPage(_current->id, round_page( uap->len ) / 0x1000, VM_THRD));
	}
	else
	{

		getfd(td, &fd, uap->fd);

		if (uap->addr == 0x0) {
			tmp = (char *)vmm_getFreeVirtualPage(_current->id, round_page(uap->len) / 0x1000, VM_TASK);
		} else
		{

			for (x = 0x0; x < round_page(uap->len); x += 0x1000)
			{

				vmm_unmapPage(((uint32_t)uap->addr & 0xFFFFF000) + x, 1);

				/* Make readonly and read/write !!! */
				if (vmm_remapPage(vmm_findFreePage(_current->id), (((uint32_t)uap->addr & 0xFFFFF000) + x), PAGE_DEFAULT, _current->id, 0) == 0x0)
					K_PANIC("Remap Page Failed");
			}
			// kprintf("(tmp1: 0x%X)", tmp);
			tmp = uap->addr;
			// kprintf("(tmp2: 0x%X)", tmp);
		}

		kern_fseek(fd->fd, uap->pos, 0x0);
		fread(tmp, uap->len, 0x1, fd->fd);

		td->td_retval[0] = (uint32_t)tmp;

		if (td->td_retval[0] == (caddr_t)-1) {
			kpanic("MMAP_FAILED");
}
	}
	return (0x0);
}
