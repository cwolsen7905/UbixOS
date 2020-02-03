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
#include <lib/kprintf.h>
#include <sys/descrip.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>

int freebsd6_mmap(struct thread *td, struct freebsd6_mmap_args *uap) {
    vm_size_t size, pageoff;
    off_t pos;
    int align, flags, error;
    vm_offset_t addr;

    int i;

    error = 0;

    size = uap->len;
    flags = uap->flags;
    pos = uap->pos;

    kprintf("uap->flags: [0x%X]\n", uap->flags);
    kprintf("uap->addr:  [0x%X]\n", uap->addr);
    kprintf("uap->len:   [0x%X]\n", uap->len);
    kprintf("uap->prot:  [0x%X]\n", uap->prot);
    kprintf("uap->fd:    [%i]\n", uap->fd);
    kprintf("uap->pad:   [0x%X]\n", uap->pad);
    kprintf("uap->pos:   [0x%X]\n", uap->pos);

    if ((uap->flags & MAP_ANON) != 0)
        pos = 0;

    /*
     * Align the file position to a page boundary,
     * and save its page offset component.
     */

    pageoff = (pos & PAGE_MASK);
    pos -= pageoff;

    /* Adjust size for rounding (on both ends). */
    size += pageoff;
    size = (vm_size_t) round_page(size);

    /* Ensure alignment is at least a page and fits in a pointer. */
    align = flags & MAP_ALIGNMENT_MASK;
    if (align != 0 && align != MAP_ALIGNED_SUPER && (align >> MAP_ALIGNMENT_SHIFT >= sizeof(void*) * NBBY || align >> MAP_ALIGNMENT_SHIFT < PAGE_SHIFT))
        return (EINVAL);

    if (flags & MAP_FIXED) {
        kprintf("FIXED NOT SUPPORTED YET");
        return (EINVAL);
    }
    else {
        /* At Some Point I Need To  Proc Lock Incase It's Threaded */
        /* MrOlsen (2016-01-15) Temporary comment out
         if ( addr == 0 || (addr >= round_page( (vm_offset_t) vms->vm_taddr ) && addr < round_page( (vm_offset_t) vms->vm_daddr + lim_max( td->td_proc, RLIMIT_DATA ) )) )
         addr = round_page( (vm_offset_t) vms->vm_daddr + lim_max( td->td_proc, RLIMIT_DATA ) );
         */
    }

    if (flags & MAP_ANON) {
        /*
         * Mapping blank space is trivial.
         */

        /*
         handle = NULL;
         handle_type = OBJT_DEFAULT;
         maxprot = VM_PROT_ALL;
         cap_maxprot = VM_PROT_ALL;
         */
        for (i = addr; i < (addr + size); i += 0x1000) {
            if (vmm_remapPage(vmm_findFreePage(_current->id), i, PAGE_DEFAULT, _current->id, 0) == 0x0)
                K_PANIC("remap Failed");
        }
        kprintf("td->vm_dsize should be adjust but isn't");
    }
    else {
        /* Mapping File */
        kprintf("File Mapping Not Supported Yet");
        return (EINVAL);
    }

    return (0x0);
}

int sys_munmap(struct thread *td, struct sys_munmap_args *uap) {
    //TEMP
    //kprintf("[%s:%i] munmap", __FILE__, __LINE__);
    td->td_retval[0] = 0;
    return (0);
}


int sys_mmap(struct thread *td, struct sys_mmap_args *uap) {
    vm_offset_t addr = 0x0;
    char *tmp = 0x0;
    struct file *fd = 0x0;
    int x;

    kprintf("[%s:%i] mmap(%i-0x%X-%i)", __FILE__, __LINE__, uap->fd, uap->addr, uap->len);

    addr = (vm_offset_t) uap->addr;

    if (uap->fd == -1) {
        if (uap->addr != 0x0) {
            for (x = 0x0; x < round_page(uap->len); x += 0x1000) {
                vmm_unmapPage(((uint32_t) uap->addr & 0xFFFFF000) + x, VMM_FREE);
                /* Make readonly and read/write !!! */
                if (vmm_remapPage(vmm_findFreePage(_current->id), (((uint32_t) uap->addr & 0xFFFFF000) + x), PAGE_DEFAULT, _current->id, 0) == 0x0)
                    K_PANIC("Remap Page Failed");

            }
            tmp = uap->addr;
            bzero(tmp, uap->len);
            td->td_retval[0] = (uint32_t) tmp;
            return (0x0);
        }

        td->td_retval[0] = vmm_getFreeVirtualPage(_current->id, round_page( uap->len ) / 0x1000, VM_TASK);
        bzero(td->td_retval[0], uap->len);
        return (0x0);  //vmm_getFreeVirtualPage(_current->id, round_page( uap->len ) / 0x1000, VM_THRD));
    }
    else {

        getfd(td, &fd, uap->fd);

        if (uap->addr == 0x0)
            tmp = (char*) vmm_getFreeVirtualPage(_current->id, round_page(uap->len) / 0x1000, VM_TASK);
        else {

            for (x = 0x0; x < round_page(uap->len); x += 0x1000) {

                vmm_unmapPage(((uint32_t) uap->addr & 0xFFFFF000) + x, 1);

                /* Make readonly and read/write !!! */
                if (vmm_remapPage(vmm_findFreePage(_current->id), (((uint32_t) uap->addr & 0xFFFFF000) + x), PAGE_DEFAULT, _current->id, 0) == 0x0)
                    K_PANIC("Remap Page Failed");

            }

            tmp = uap->addr;
            kprintf("(tmp: 0x%X)", tmp);

        }

        kern_fseek(fd->fd, uap->pos, 0x0);
        fread(tmp, uap->len, 0x1, fd->fd);

        td->td_retval[0] = (uint32_t) tmp;

        if (td->td_retval[0] == (caddr_t) -1)
            kpanic("MMAP_FAILED");
    }
    return (0x0);
}
