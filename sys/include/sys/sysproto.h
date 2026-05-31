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

#ifndef _SYS_SYSPROTO_H
#define _SYS_SYSPROTO_H

#include <sys/thread.h>
#include <sys/fb.h>
#include <sys/klog.h>

#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? 0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define PADL_(t) 0
#define PADR_(t) PAD_(t)
#else
#define PADL_(t) PAD_(t)
#define PADR_(t) 0
#endif

// Protos
struct sys_mpiCreateMbox_args
{
	char name_l_[PADL_(char *)];
	char *name;
	char name_r_[PADR_(char *)];
};

struct sys_mpiDestroyMbox_args
{
	char name_l_[PADL_(char *)];
	char *name;
	char name_r_[PADR_(char *)];
};

struct sys_mpiFetchMessage_args
{
	char name_l_[PADL_(char *)];
	char *name;
	char name_r_[PADR_(char *)];
	char msg_l_[PADL_(const void *)];
	const void *msg;
	char msg_r_[PADR_(const void *)];
};

struct sys_mpiPostMessage_args
{
	char name_l_[PADL_(char *)];
	char *name;
	char name_r_[PADR_(char *)];
	char type_l_[PADL_(u_int32_t)];
	u_int32_t type;
	char type_r_[PADR_(u_int32_t)];
	char msg_l_[PADL_(const void *)];
	const void *msg;
	char msg_r_[PADR_(const void *)];
};

struct sys_getvfscwd_args
{
	char buf_l_[PADL_(char *)];
	char *buf;
	char buf_r_[PADR_(char *)];
	char size_l_[PADL_(u_int32_t)];
	u_int32_t size;
	char size_r_[PADR_(u_int32_t)];
};

struct sys_pidStatus_args
{
	char pid_l_[PADL_(int)];
	int pid;
	char pid_r_[PADR_(int)];
};

// Func Defs
int sys_invalid(struct thread *, void *);
int sys_pidStatus(struct thread *, struct sys_pidStatus_args *);
int sys_mpiCreateMbox(struct thread *, struct sys_mpiCreateMbox_args *);
int sys_mpiDestroyMbox(struct thread *, struct sys_mpiDestroyMbox_args *);
int sys_mpiFetchMessage(struct thread *, struct sys_mpiFetchMessage_args *);
int sys_mpiPostMessage(struct thread *, struct sys_mpiPostMessage_args *);
int sys_getvfscwd(struct thread *, struct sys_getvfscwd_args *);

struct sys_ttyctrl_args
{
	int cmd;
	int val;
};
int sys_ttyctrl(struct thread *, struct sys_ttyctrl_args *);

struct sys_mapfb_args
{
	struct fb_info *info;
};
int sys_mapfb(struct thread *, struct sys_mapfb_args *);

struct sys_getmouse_args
{
	struct mouse_event *ev;
};
int sys_getmouse(struct thread *, struct sys_getmouse_args *);

struct sys_getkbd_args
{
	struct kbd_event *ev;
};
int sys_getkbd(struct thread *, struct sys_getkbd_args *);

struct sys_shareregion_args
{
	pid_t dst_pid;
	void *vaddr;
	u_int32_t size;
	u_int32_t *out_vaddr;
};
int sys_shareregion(struct thread *, struct sys_shareregion_args *);

struct sys_klog_read_args
{
	struct klog_entry *buf; /* userspace array to fill */
	int max_entries;
	u_int32_t start_seq; /* read entries with seq >= start_seq */
};
int sys_klog_read(struct thread *, struct sys_klog_read_args *);

struct sys_klog_write_args
{
	u_int8_t level;  /* KLOG_* severity */
	const char *msg; /* NUL-terminated message string */
};
int sys_klog_write(struct thread *, struct sys_klog_write_args *);

/* sys_settty (slot 48) — claim the serial TTY and set it as the process's terminal.
 * Analogous to FreeBSD getty opening /dev/ttyu0 and calling login_tty().
 * slot: index into the kernel tty_term[] table (1 = serial TTY). */
struct sys_settty_args
{
	char slot_l_[PADL_(int)];
	int slot;
	char slot_r_[PADR_(int)];
};
int sys_settty(struct thread *, struct sys_settty_args *);

/* Pseudo-terminal pool syscalls (slots 55-58) — used by graphical terminal
 * apps to run an interactive shell on a real kernel tty. */
struct sys_ptyalloc_args
{
	register_t dummy;
};
int sys_ptyalloc(struct thread *, struct sys_ptyalloc_args *);

struct sys_ptyfree_args
{
	char slot_l_[PADL_(int)];
	int slot;
	char slot_r_[PADR_(int)];
};
int sys_ptyfree(struct thread *, struct sys_ptyfree_args *);

struct sys_ptyinject_args
{
	char slot_l_[PADL_(int)];
	int slot;
	char slot_r_[PADR_(int)];
	char buf_l_[PADL_(const char *)];
	const char *buf;
	char buf_r_[PADR_(const char *)];
	char n_l_[PADL_(int)];
	int n;
	char n_r_[PADR_(int)];
};
int sys_ptyinject(struct thread *, struct sys_ptyinject_args *);

struct sys_ptysnap_args
{
	char slot_l_[PADL_(int)];
	int slot;
	char slot_r_[PADR_(int)];
	char dst_l_[PADL_(void *)];
	void *dst;
	char dst_r_[PADR_(void *)];
	char x_l_[PADL_(u_int16_t *)];
	u_int16_t *x;
	char x_r_[PADR_(u_int16_t *)];
	char y_l_[PADL_(u_int16_t *)];
	u_int16_t *y;
	char y_r_[PADR_(u_int16_t *)];
};
int sys_ptysnap(struct thread *, struct sys_ptysnap_args *);

struct sys_net_configure_args
{
	char cfg_l_[PADL_(const void *)];
	const void *cfg; /* user pointer to struct net_config */
	char cfg_r_[PADR_(const void *)];
};
int sys_net_configure(struct thread *, struct sys_net_configure_args *);

#endif
