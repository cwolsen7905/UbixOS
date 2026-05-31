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

#include <sys/fb.h>
#include <sys/sysproto.h>
#include <sys/klog.h>
#include <sys/thread.h>
#include <vmm/paging.h>
#include <vmm/vmm.h>
#include <lib/vesa.h>
#include <lib/kprintf.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <isa/rs232.h>
#include <isa/mouse.h>
#include <isa/kbd.h>

int sys_mapfb(struct thread *td, struct sys_mapfb_args *args)
{
	struct fb_info *out = args->info;
	u_int32_t paddr, end, addr, vaddr;
	pidType pid = _current->id;

	if (!vesa_fb_paddr || !vesa_pitch || !vesa_width || !vesa_height || !vesa_bpp)
	{
		kprintf("sys_mapfb: VESA not initialised — send MPI 0x82 to system first\n");
		td->td_retval[0] = -1;
		return (-1);
	}

	/*
	 * Map framebuffer physical pages into the calling process at a fixed
	 * user-space virtual address.  The LFB physical address (e.g. 0xFD000000)
	 * is above the 3 GB kernel/user split so we cannot use it as a virtual
	 * address — map it at 0x10000000 instead, which is safely in user space.
	 */
	paddr = vesa_fb_paddr & ~0xFFFU;
	end = vesa_fb_paddr + (u_int32_t)vesa_pitch * vesa_height;
	vaddr = 0x10000000U;

	for (addr = paddr; addr < end; addr += 0x1000, vaddr += 0x1000)
		vmm_remap_page(addr, vaddr, PAGE_DEFAULT | PAGE_CACHE_DISABLED, pid, 0);

	out->base = (void *)0x10000000U;
	out->width = vesa_width;
	out->height = vesa_height;
	out->pitch = vesa_pitch;
	out->bpp = vesa_bpp;

	kprintf("sys_mapfb: pid %d mapped fb paddr=0x%X vaddr=0x10000000 %dx%d bpp=%d\n",
	        pid,
	        vesa_fb_paddr,
	        vesa_width,
	        vesa_height,
	        vesa_bpp);

	/* Signal that a GUI compositor now owns the keyboard ring. */
	kbd_gui_mode = 1;

	td->td_retval[0] = 0;
	return (0);
}

int sys_shareregion(struct thread *td, struct sys_shareregion_args *args)
{
	uintptr_t client_vaddr;

	client_vaddr = vmm_share_region((uintptr_t)args->vaddr, (size_t)args->size, (pidType)args->dst_pid);

	if (client_vaddr == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	*args->out_vaddr = (u_int32_t)client_vaddr;
	td->td_retval[0] = 0;
	return (0);
}

int sys_getmouse(struct thread *td, struct sys_getmouse_args *args)
{
	mouse_event_t *out = args->ev;
	mouse_event_t ev;

	if (mouse_getEvent(&ev) != 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	out->dx = ev.dx;
	out->dy = ev.dy;
	out->buttons = ev.buttons;

	td->td_retval[0] = 0;
	return (0);
}

int sys_getkbd(struct thread *td, struct sys_getkbd_args *args)
{
	kbd_event_t *out = args->ev;
	kbd_event_t ev;

	if (kbd_getEvent(&ev) != 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	out->keycode = ev.keycode;
	out->pressed = ev.pressed;

	td->td_retval[0] = 0;
	return (0);
}

int sys_klog_read(struct thread *td, struct sys_klog_read_args *args)
{
	int n;

	if (args->buf == NULL || args->max_entries <= 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	n = klog_read(args->buf, args->max_entries, args->start_seq);
	td->td_retval[0] = n;
	return (0);
}

/*
 * sys_settty (slot 48) — claim the serial TTY as the calling process's
 * controlling terminal.  Analogous to FreeBSD getty calling login_tty() after
 * opening /dev/ttyu0: it sets _current->term and switches the COM1 ISR into
 * ring-drain mode so the serial port is no longer forwarded to tty_foreground.
 *
 * slot: tty_term[] index to use as the serial TTY (convention: 1).
 */
int sys_settty(struct thread *td, struct sys_settty_args *args)
{
	tty_term *t;

	if (args->slot < 0 || args->slot >= TTY_MAX_TERMS)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	t = tty_find(args->slot);
	/* Slots 0-3: VGA virtual consoles.  Slot 4+: serial. */
	t->t_type = (args->slot <= 3) ? TTY_TYPE_VGA : TTY_TYPE_SERIAL;
	t->t_echo = 1;
	t->t_raw = 0;
	t->stdinSize = 0;
	t->t_linelen = 0;

	_current->term = t;
	if (args->slot > 3)
		serial_claimed = 1;

	td->td_retval[0] = 0;
	return (0);
}

/*
 * sys_ptyalloc (slot 55) — allocate a pseudo-terminal slot from the pty pool.
 * Returns the slot index, or -1 if the pool is exhausted.
 */
int sys_ptyalloc(struct thread *td, struct sys_ptyalloc_args *args)
{
	(void)args;
	td->td_retval[0] = pty_alloc();
	return (0);
}

/*
 * sys_ptyfree (slot 56) — release a pseudo-terminal slot back to the pool.
 */
int sys_ptyfree(struct thread *td, struct sys_ptyfree_args *args)
{
	pty_free(args->slot);
	td->td_retval[0] = 0;
	return (0);
}

/*
 * sys_ptyinject (slot 57) — push a userland keystroke buffer through a pty's
 * line discipline (keyboard → shell stdin).  Returns bytes injected or -1.
 */
int sys_ptyinject(struct thread *td, struct sys_ptyinject_args *args)
{
	td->td_retval[0] = tty_inject_user(args->slot, args->buf, args->n);
	return (0);
}

/*
 * sys_ptysnap (slot 58) — copy a pty's 80x25 cell grid + cursor to userland.
 * Returns 0 on success, -1 for an invalid slot.
 */
int sys_ptysnap(struct thread *td, struct sys_ptysnap_args *args)
{
	td->td_retval[0] = tty_snapshot(args->slot, args->dst, args->x, args->y);
	return (0);
}
