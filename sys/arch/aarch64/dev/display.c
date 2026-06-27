/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 display syscalls: the framebuffer half of the desktop platform.
 *
 * i386's equivalents live in sys/kern/fb.c and map the VESA linear framebuffer
 * (a live PCI scanout).  aarch64 has no VESA: the framebuffer is the contiguous
 * guest-RAM buffer owned by virtio_gpu.c (B8G8R8X8, the same layout objGFX
 * expects), and presenting a frame is an explicit virtio-gpu transfer+flush
 * rather than a live write.  So:
 *
 *   sys_mapfb    maps the virtio-gpu framebuffer's physical pages into the
 *                calling compositor's address space at a fixed user VA.
 *   sys_fbpresent pushes the current framebuffer contents to the host display.
 *
 * These define the same symbols the shared native syscall table (sys/kern/
 * syscalls.c) references; the i386 build resolves them from fb.c instead.
 */

#include <sys/types.h>
#include <sys/fb.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <vmm/paging.h> /* PAGE_SIZE */
#include <ubixos/sched.h>
#include <ubixos/tty.h> /* pty_alloc / tty_inject_user / tty_snapshot / tty_resize */
#include <lib/kprintf.h>
#include <lib/kconsole.h> /* kconsole_suspend_primary on fb claim */
#include "bringup.h"      /* AARCH64_PHYS_OF — virtio_gpu_fb is a physmap VA */

/* Per-process anonymous-mmap region base (matches MMAP_BASE in syscall.c); the
 * client's shared window buffers are bump-allocated from the same region. */
#define USER_MMAP_BASE 0x200000000UL

/* virtio-gpu driver state + present entry point (sys/arch/aarch64/dev/virtio_gpu.c). */
extern u_int8_t *virtio_gpu_fb;
extern u_int32_t virtio_gpu_width;
extern u_int32_t virtio_gpu_height;
extern u_int32_t virtio_gpu_pitch;
void virtio_gpu_flush(void);

/* pmap mappers (sys/arch/aarch64/vmm/pmap.c). */
int pmap_map_user_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, int executable);
int pmap_map_user_page_wired(u_int64_t *l1, u_int64_t va, u_int64_t pa); /* RW, fork-shared, never freed */
u_int64_t pmap_extract(u_int64_t *l1, u_int64_t va);

/*
 * Fixed user VA window for the mapped framebuffer.  Block 12 (0x300000000) is
 * clear of every region the aarch64 user-AS layout uses: the PIE main image
 * (4 GB), the ld.so interp (5 GB), the stack (6 GB), the brk heap (block 7) and
 * the anonymous-mmap region (block 8, which grows upward) — see the VA-layout
 * constants in sys/arch/aarch64/kern/syscall.c.
 */
#define FB_USER_BASE 0x300000000UL

/**
 * sys_mapfb (native 43) — map the virtio-gpu framebuffer into the caller.
 *
 * Maps the framebuffer's contiguous physical pages into the calling process's
 * address space at FB_USER_BASE (EL0 RW, non-executable) and fills *args->info
 * with the base VA + geometry.  The caller (the views compositor) then composites
 * into that buffer and calls sys_fbpresent() to display each frame.
 *
 * @return 0 on success, -1 if the GPU isn't initialised.
 */
int sys_mapfb(struct thread *td, struct sys_mapfb_args *args)
{
	struct fb_info *out = args->info;
	u_int64_t *l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;
	u_int64_t pa, va, end;

	if (virtio_gpu_fb == 0 || virtio_gpu_width == 0 || virtio_gpu_height == 0)
	{
		/* The compositor retries sys_mapfb every tick until the GPU is ready, so
		 * on a headless run (no virtio-gpu device) this fires forever.  Log the
		 * first failure and then every 256th, so serial.log shows the condition
		 * without being buried under thousands of identical lines. */
		static u_int32_t g_mapfb_fail_count;
		if ((g_mapfb_fail_count++ & 0xFF) == 0)
			kprintf("sys_mapfb: virtio-gpu not initialised (x%u)\n", g_mapfb_fail_count);
		td->td_retval[0] = -1;
		return (-1);
	}

	pa = (u_int64_t)AARCH64_PHYS_OF((uintptr_t)virtio_gpu_fb); /* fb kernel ptr is a physmap VA */
	end = pa + (u_int64_t)virtio_gpu_pitch * virtio_gpu_height;
	/* Wired, not a plain user page: the framebuffer is RAM-backed and owned by the
	 * GPU, so a fork (the compositor forks vlogin) must NOT copy-on-write it — that
	 * would silently hand the compositor a private copy the scanout never sees. */
	for (va = FB_USER_BASE; pa < end; pa += PAGE_SIZE, va += PAGE_SIZE)
		pmap_map_user_page_wired(l1, va, pa);

	out->base = (void *)FB_USER_BASE;
	out->width = virtio_gpu_width;
	out->height = virtio_gpu_height;
	out->pitch = (u_int16_t)virtio_gpu_pitch;
	out->bpp = 32;

	kprintf("sys_mapfb: pid %d mapped fb pa=0x%lx va=0x%lx %ux%u pitch=%u\n",
	        _current->id,
	        (u_int64_t)AARCH64_PHYS_OF((uintptr_t)virtio_gpu_fb),
	        (u_int64_t)FB_USER_BASE,
	        virtio_gpu_width,
	        virtio_gpu_height,
	        virtio_gpu_pitch);

	/* The compositor now owns the scanout — stop the framebuffer console from
	 * drawing over it (the always-on serial sink keeps logging). */
	kconsole_suspend_primary();

	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_fbpresent (native 54) — present the framebuffer to the host display via a
 * virtio-gpu TRANSFER_TO_HOST_2D + RESOURCE_FLUSH.  Called by the compositor
 * once per composited frame.
 */
int sys_fbpresent(struct thread *td, struct sys_fbpresent_args *args)
{
	(void)args;
	virtio_gpu_flush();
	td->td_retval[0] = 0;
	return (0);
}

/*
 * Pseudo-terminal syscalls (native 55-58, 61) for the GUI terminal.  The pty
 * pool + VT100 engine are arch-neutral (sys/posix/tty.c, now compiled for
 * aarch64); these thin handlers mirror the i386 ones in sys/kern/fb.c.  i386
 * links its copies from fb.c; aarch64 links these.
 */

/** sys_ptyalloc (55) — allocate a pty slot; returns the slot or -1. */
int sys_ptyalloc(struct thread *td, struct sys_ptyalloc_args *uap)
{
	(void)uap;
	td->td_retval[0] = pty_alloc();
	return (0);
}

/** sys_ptyfree (56) — release a pty slot. */
int sys_ptyfree(struct thread *td, struct sys_ptyfree_args *uap)
{
	pty_free(uap->slot);
	td->td_retval[0] = 0;
	return (0);
}

/** sys_ptyinject (57) — push a keystroke buffer through a pty's line discipline. */
int sys_ptyinject(struct thread *td, struct sys_ptyinject_args *uap)
{
	td->td_retval[0] = tty_inject_user(uap->slot, uap->buf, uap->n);
	return (0);
}

/** sys_ptysnap (58) — copy a pty's cell grid + cursor to userland. */
int sys_ptysnap(struct thread *td, struct sys_ptysnap_args *uap)
{
	td->td_retval[0] = tty_snapshot(uap->slot, uap->dst, uap->x, uap->y);
	return (0);
}

/** sys_ptyresize (61) — set a pty's grid to cols x rows + update its winsize. */
int sys_ptyresize(struct thread *td, struct sys_ptyresize_args *uap)
{
	td->td_retval[0] = tty_resize(uap->slot, uap->cols, uap->rows);
	return (0);
}

/**
 * sys_shareregion (native 45) — map the caller's pages [vaddr, vaddr+size) into
 * process @dst_pid's address space, returning the destination VA in *out_vaddr.
 *
 * The compositor uses this to hand a window's shared backing buffer to its
 * client.  Unlike the i386 vmm_share_region (CR3 switch + PT self-map, 32-bit),
 * aarch64 maps cross-address-space directly: all RAM is identity-mapped, so the
 * caller's frames (pmap_extract from its L1) are mapped into the client's L1
 * (pmap_map_user_page) with no TTBR0 switch.  The client VA is bump-allocated
 * from its mmap region.  No COW refcount yet — aarch64 leaks user pages on exit,
 * so the shared frames outlive both ends (matches the current port's model).
 *
 * @return 0 on success, -1 if the destination is gone or a page is unmapped.
 */
int sys_shareregion(struct thread *td, struct sys_shareregion_args *args)
{
	kTask_t *dst = schedFindTask(args->dst_pid);
	u_int64_t *src_l1 = (u_int64_t *)(uintptr_t)_current->md.md_ttbr0;
	u_int64_t *dst_l1;
	u_int64_t base = (u_int64_t)(uintptr_t)args->vaddr;
	u_int64_t n, i, dst_va;

	if (dst == 0 || args->vaddr == 0 || args->size == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	dst_l1 = (u_int64_t *)(uintptr_t)dst->md.md_ttbr0;

	if (dst->md.md_mmap_next == 0)
		dst->md.md_mmap_next = USER_MMAP_BASE;
	dst_va = dst->md.md_mmap_next;

	n = (args->size + PAGE_SIZE - 1) / PAGE_SIZE;
	for (i = 0; i < n; i++)
	{
		u_int64_t pa = pmap_extract(src_l1, base + i * PAGE_SIZE);
		if (pa == 0)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		/* Wire BOTH ends of the shared window buffer.  The frame is now mapped in
		 * two address spaces; if either the compositor or the client forks, a COW
		 * would split one side off the shared frame and the two would stop seeing
		 * each other's writes.  Re-map the source side wired too (it was a plain
		 * user page), so neither end can be COW-broken. */
		pmap_map_user_page_wired(src_l1, base + i * PAGE_SIZE, pa);
		pmap_map_user_page_wired(dst_l1, dst_va + i * PAGE_SIZE, pa);
	}
	dst->md.md_mmap_next += n * PAGE_SIZE;

	if (args->out_vaddr != 0)
		*args->out_vaddr = (uintptr_t)dst_va;
	td->td_retval[0] = 0;
	return (0);
}
