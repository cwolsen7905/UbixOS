/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 display syscalls — the framebuffer half of the desktop platform.
 *
 * The framebuffer is the Bochs/std-VGA linear framebuffer (dev/vga_fb.c), a live
 * scanout exactly like the i386 VESA LFB.  So:
 *   sys_mapfb     maps the LFB's physical BAR pages into the compositor at a fixed
 *                 user VA (wired + cache-disabled — it is device MMIO).
 *   sys_fbpresent is a no-op (the LFB is live; writes appear immediately).
 *   sys_shareregion maps the caller's pages into another process (a window's
 *                 shared backing buffer the compositor hands its client).
 *
 * These define the same symbols the native syscall table (sys/kern/syscalls.c)
 * references; i386 resolves them from fb.c, aarch64 from dev/display.c, x86_64
 * here.  Sibling of aarch64's dev/display.c.
 */

#include "../x86_64.h"
#include <sys/types.h>
#include <sys/fb.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <vmm/vmm.h> /* PAGE_SIZE */
#include <ubixos/sched.h>
#include <lib/kprintf.h>

/* std-VGA LFB state (dev/vga_fb.c). */
extern u64 x86_64_fb_phys;
extern u32 x86_64_fb_width;
extern u32 x86_64_fb_height;
extern u32 x86_64_fb_pitch;

/* Page mappers (kern/usermode.c). */
void x86_64_map_fb_page(uintptr_t pml4_phys, u64 va, u64 phys);
void x86_64_map_user_page_wired(uintptr_t pml4_phys, u64 va, u64 phys);
u64 x86_64_user_extract(uintptr_t pml4_phys, u64 va);

/*
 * Fixed user VA window for the mapped framebuffer: block 12 (0x300000000, 12 GB),
 * clear of every region the x86_64 user-AS layout uses (ELF 1 GB, DYN_MAIN 4 GB,
 * interp 5 GB, stack 6 GB, brk 7 GB, mmap 8 GB — see x86_64/vmm_layout.h).
 */
#define FB_USER_BASE 0x300000000UL

/* Anonymous-mmap region base (matches MMAP_BASE) — shared window buffers bump
 * from the destination's mmap cursor. */
#define USER_MMAP_BASE 0x200000000UL

/**
 * sys_mapfb (native 43) — map the std-VGA LFB into the caller (the views
 * compositor) at FB_USER_BASE and fill *args->info with the base VA + geometry.
 * @return 0 on success, -1 if the framebuffer isn't initialised.
 */
int sys_mapfb(struct thread *td, struct sys_mapfb_args *args)
{
	struct fb_info *out = args->info;
	uintptr_t pml4 = (uintptr_t)_current->md.md_cr3;
	u64 pa, va, end;

	if (x86_64_fb_phys == 0 || x86_64_fb_width == 0 || x86_64_fb_height == 0)
	{
		kprintf("sys_mapfb: framebuffer not initialised\n");
		td->td_retval[0] = -1;
		return (-1);
	}

	pa = x86_64_fb_phys;
	end = pa + (u64)x86_64_fb_pitch * x86_64_fb_height;
	for (va = FB_USER_BASE; pa < end; pa += PAGE_SIZE, va += PAGE_SIZE)
		x86_64_map_fb_page(pml4, va, pa);

	out->base = (void *)FB_USER_BASE;
	out->width = x86_64_fb_width;
	out->height = x86_64_fb_height;
	out->pitch = (u_int16_t)x86_64_fb_pitch;
	out->bpp = 32;

	kprintf("sys_mapfb: pid %d mapped LFB pa=0x%X va=0x300000000 %ux%u pitch=%u\n",
	        _current->id,
	        (u32)x86_64_fb_phys,
	        x86_64_fb_width,
	        x86_64_fb_height,
	        x86_64_fb_pitch);

	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_fbpresent (native 54) — present the framebuffer.  The std-VGA LFB is the
 * live scanout, so there is nothing to push (matches i386).
 */
int sys_fbpresent(struct thread *td, struct sys_fbpresent_args *args)
{
	(void)args;
	x86_64_fb_present();
	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_shareregion (native 45) — map the caller's pages [vaddr, vaddr+size) into
 * process @dst_pid, returning the destination VA in *out_vaddr.  The compositor
 * uses this to hand a window's shared backing buffer to its client.  Both ends are
 * wired so a later fork on either side shares (never copies) the frames.
 * @return 0 on success, -1 if the destination is gone or a page is unmapped.
 */
int sys_shareregion(struct thread *td, struct sys_shareregion_args *args)
{
	kTask_t *dst = schedFindTask(args->dst_pid);
	uintptr_t src_pml4 = (uintptr_t)_current->md.md_cr3;
	uintptr_t dst_pml4;
	u64 base = (u64)(uintptr_t)args->vaddr;
	u64 n, i, dst_va;

	if (dst == 0 || args->vaddr == 0 || args->size == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	dst_pml4 = (uintptr_t)dst->md.md_cr3;

	if (dst->md.md_mmap_next == 0)
		dst->md.md_mmap_next = USER_MMAP_BASE;
	dst_va = dst->md.md_mmap_next;

	n = (args->size + PAGE_SIZE - 1) / PAGE_SIZE;
	for (i = 0; i < n; i++)
	{
		u64 pa = x86_64_user_extract(src_pml4, base + i * PAGE_SIZE);
		if (pa == 0)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		/* Wire both ends: the frame is now in two address spaces, so a fork on
		 * either side must share (not COW-copy) it, else the two stop seeing each
		 * other's writes. */
		x86_64_map_user_page_wired(src_pml4, base + i * PAGE_SIZE, pa);
		x86_64_map_user_page_wired(dst_pml4, dst_va + i * PAGE_SIZE, pa);
	}
	dst->md.md_mmap_next += n * PAGE_SIZE;

	if (args->out_vaddr != 0)
		*args->out_vaddr = (uintptr_t)dst_va;
	td->td_retval[0] = 0;
	return (0);
}
