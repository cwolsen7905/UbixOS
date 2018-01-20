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
#include <sys/types.h>
#include <lib/kprintf.h>
#include <sys/descrip.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>

/* MrOlsen (2016-01-15) TEMP: Put These somewhere else */
typedef __uint32_t        __vm_size_t; typedef __vm_size_t
vm_size_t;
#define        EINVAL          22              /* Invalid argument */
#define MAP_ALIGNED(n)   ((n) << MAP_ALIGNMENT_SHIFT)
#define MAP_ALIGNMENT_SHIFT     24
#define MAP_ALIGNMENT_MASK      MAP_ALIGNED(0xff)
#define MAP_ALIGNED_SUPER       MAP_ALIGNED(1) /* align on a superpage */
#define        NBBY    8               /* number of bits in a byte */

/* PROTs */
#define PROT_NONE       0x00    /* no permissions */
#define PROT_READ       0x01    /* pages can be read */
#define PROT_WRITE      0x02    /* pages can be written */
#define PROT_EXEC       0x04    /* pages can be executed */

/* FLAGs */

/*
 * Do I Need These?
 */
#define MAP_SHARED      0x0001          /* share changes */
#define MAP_PRIVATE     0x0002          /* changes are private */
#define MAP_FIXED       0x0010 /* map addr must be exactly as requested */
#define MAP_RENAME       0x0020 /* Sun: rename private pages to file */
#define MAP_NORESERVE    0x0040 /* Sun: don't reserve needed swap area */
#define MAP_RESERVED0080 0x0080 /* previously misimplemented MAP_INHERIT */
#define MAP_RESERVED0100 0x0100 /* previously unimplemented MAP_NOEXTEND */
#define MAP_HASSEMAPHORE 0x0200 /* region may contain semaphores */
#define MAP_STACK        0x0400 /* region grows down, like a stack */
#define MAP_NOSYNC       0x0800 /* page to but do not sync underlying file */

/*
 * Mapping type
 */
#define MAP_FILE         0x0000 /* map from file (default) */
#define MAP_ANON         0x1000 /* allocated from memory, swap space */

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
  if (align != 0 && align != MAP_ALIGNED_SUPER && (align >> MAP_ALIGNMENT_SHIFT >= sizeof(void *) * NBBY || align >> MAP_ALIGNMENT_SHIFT < PAGE_SHIFT))
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
  td->td_retval[0] = 0;
  return(0);
};

int sys_mmap(struct thread *td, struct sys_mmap_args *uap) {
  vm_offset_t addr = 0x0;
  char *tmp = 0x0;
  struct file *fd = 0x0;
  int x;

  addr = (vm_offset_t) uap->addr;

  if (uap->fd == -1) {
    if (uap->addr != 0x0) {
      for (x = 0x0; x < round_page(uap->len); x += 0x1000) {
      vmm_unmapPage(((uint32_t)uap->addr & 0xFFFFF000) + x, 1);
        /* Make readonly and read/write !!! */
        if (vmm_remapPage(vmm_findFreePage(_current->id), (((uint32_t)uap->addr & 0xFFFFF000) + x), PAGE_DEFAULT, _current->id, 0) == 0x0)
          K_PANIC("Remap Page Failed");

      }
      tmp = uap->addr;
      bzero(tmp, uap->len);
      td->td_retval[0] = (uint32_t)tmp;
      return (0x0);
    }

    td->td_retval[0] = vmm_getFreeVirtualPage(_current->id, round_page( uap->len ) / 0x1000, VM_TASK);
    bzero(td->td_retval[0], uap->len);
    return (0x0); //vmm_getFreeVirtualPage(_current->id, round_page( uap->len ) / 0x1000, VM_THRD));
  }
  else {
    //kprintf("uap->flags: [0x%X]\n", uap->flags);
    //MrOlsenkprintf("uap->addr:  [0x%X]", uap->addr);
    //MrOlsenkprintf("uap->len:   [0x%X]", uap->len);
    //kprintf("uap->prot:  [0x%X]", uap->prot);
    //MrOlsenkprintf("uap->fd:    [%i]\n", uap->fd);
    //kprintf("uap->pad:   [0x%X]", uap->pad);
    //kprintf("uap->pos:   [0x%X]", uap->pos);
    getfd(td, &fd, uap->fd);
    if (uap->addr == 0x0)
    tmp = (char *) vmm_getFreeVirtualPage(_current->id, round_page(uap->len) / 0x1000, VM_TASK);
    else {
      for (x = 0x0; x < round_page(uap->len); x += 0x1000) {
      vmm_unmapPage(((uint32_t)uap->addr & 0xFFFFF000) + x, 1);
        /* Make readonly and read/write !!! */
        if (vmm_remapPage(vmm_findFreePage(_current->id), (((uint32_t)uap->addr & 0xFFFFF000) + x), PAGE_DEFAULT, _current->id, 0) == 0x0)
          K_PANIC("Remap Page Failed");

      }
      tmp = uap->addr;

 }
    fseek(fd->fd, uap->pos, 0x0);
    fread(tmp, uap->len, 0x1, fd->fd);
    td->td_retval[0] = (uint32_t) tmp;
    if (td->td_retval[0] == (caddr_t)-1)
      kpanic("BALLS");
  }
  return (0x0);
}
