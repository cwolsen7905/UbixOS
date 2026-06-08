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
#include <lib/kprintf.h>

/* virtio-gpu driver state + present entry point (sys/arch/aarch64/dev/virtio_gpu.c). */
extern u_int8_t *virtio_gpu_fb;
extern u_int32_t virtio_gpu_width;
extern u_int32_t virtio_gpu_height;
extern u_int32_t virtio_gpu_pitch;
void virtio_gpu_flush(void);

/* pmap user-page mapper (sys/arch/aarch64/vmm/pmap.c). */
int pmap_map_user_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, int executable);

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
		kprintf("sys_mapfb: virtio-gpu not initialised\n");
		td->td_retval[0] = -1;
		return (-1);
	}

	pa = (u_int64_t)(uintptr_t)virtio_gpu_fb;
	end = pa + (u_int64_t)virtio_gpu_pitch * virtio_gpu_height;
	for (va = FB_USER_BASE; pa < end; pa += PAGE_SIZE, va += PAGE_SIZE)
		pmap_map_user_page(l1, va, pa, 0);

	out->base = (void *)FB_USER_BASE;
	out->width = virtio_gpu_width;
	out->height = virtio_gpu_height;
	out->pitch = (u_int16_t)virtio_gpu_pitch;
	out->bpp = 32;

	kprintf("sys_mapfb: pid %d mapped fb pa=0x%lx va=0x%lx %ux%u pitch=%u\n",
	        _current->id,
	        (u_int64_t)(uintptr_t)virtio_gpu_fb,
	        (u_int64_t)FB_USER_BASE,
	        virtio_gpu_width,
	        virtio_gpu_height,
	        virtio_gpu_pitch);

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
