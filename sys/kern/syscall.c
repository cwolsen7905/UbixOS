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

#include <ubixos/syscalls.h>
#include <ubixos/syscall.h>
#include <ubixos/sched.h>
#include <sys/sysproto.h>
#include <fs/ubixfs/ubfs_vfs.h> /* ubfs_vfs_query — native pool-query syscall 67 */
#include <dev/disk.h>           /* disk_query — block-device enumeration syscall 68 */
#include <ubixos/endtask.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <isa/pit.h>
#include <sys/trap.h>
#include <sys/elf.h>
#include <string.h>
#include <lib/kprintf.h>
#include <ubixos/kpanic.h>
/* #include <sde/sde.h> */
#include <vmm/vmm.h>
#include <ubixos/errno.h>

/* Sleep channel + never-true predicate for sys_nanosleep's timed block: the
 * sleeper has no event to wake on, so it sleeps out its per-task callout timeout.
 * A single shared channel address is fine — each task is woken by its own
 * sleep_callout at its own deadline, not by a sched_wakeup_chan. */
static char g_nanosleep_chan;
static int nanosleep_never(void *arg)
{
	(void)arg;
	return (0);
}

void sys_call(struct trapframe *frame)
{
	u_int32_t code = 0x0;
	caddr_t params;

	struct thread *td = &_current->td;

	td->frame = frame;

	int error = 0x0;

	params = (caddr_t)frame->tf_esp + sizeof(int);

	code = frame->tf_eax;

	if (code > totalCalls)
	{
		die_if_kernel("Invalid System uCall", frame, frame->tf_eax);
		kpanic("PID: %i", _current->id);
	}
	else if ((u_int32_t)systemCalls[code].sc_status == SYSCALL_INVALID)
	{
		kprintf("Invalid Call: [%i][0x%X]\n", code, (u_int32_t)systemCalls[code].sc_name);
		frame->tf_eax = -1;
		frame->tf_edx = 0x0;
	}
	else
	{
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->tf_edx;

		if (systemCalls[code].sc_status == SYSCALL_DUMMY)
			kprintf("Syscall->abi: [%i], PID: [%i], Code: %i, Call: %s\n",
			        td->abi,
			        _current->id,
			        frame->tf_eax,
			        systemCalls[code].sc_name);
		/*
		    if (td->abi == ELFOSABI_UBIXOS)
		     error = (int) systemCalls[code].sc_entry( frame->tf_ebx, frame->tf_ecx, frame->tf_edx );
		    else */
		if (td->abi == ELFOSABI_FREEBSD)
			error = (int)systemCalls[code].sc_entry(td, params);
		else
			error = (int)systemCalls[code].sc_entry(td, params);

		if (systemCalls[code].sc_status == SYSCALL_DUMMY)
		{
			kprintf("DUMMY CALL: (%i)\n", code);
			return;
		}

		// kprintf("ERROR: 0x%X",error);
		switch (error)
		{
			case 0:
				frame->tf_eax = td->td_retval[0];
				frame->tf_edx = td->td_retval[1];
				frame->tf_eflags &= ~PSL_C;
				break;
			default:
				frame->tf_eax = td->td_retval[0];
				frame->tf_edx = td->td_retval[1];
				frame->tf_eflags |= PSL_C;
				break;
		}
	}
}

int invalidCall()
{
	int sys_call;

	asm("nop" : "=a"(sys_call) :);

	kprintf("Invalid System Call #[%i], PID: %i\n", sys_call, _current->id);
	return (0);
}

typedef struct _UbixUser UbixUser;
struct _UbixUser
{
	char *username;
	char *password;
	int uid;
	int gid;
	char *home;
	char *shell;
};

int sysAuth(UbixUser *uu)
{
	kprintf("authenticating user %s\n", uu->username);

	/* MrOlsen 2016-01-01 uh?
	 if(uu->username == "root" && uu->password == "user")
	 {
	 uu->uid = 0;
	 uu->gid = 0;
	 }
	 */
	uu->uid = -1;
	uu->gid = -1;
	return (0);
}

int sysPasswd(char *passwd)
{
	kprintf("changing user password for user %d\n", _current->uid);
	return (0);
}

int sysAddModule()
{
	return (0);
}

int sysRmModule()
{
	return (0);
}

int sysGetpid(int *pid)
{
	if (pid)
		*pid = _current->id;
	return (0);
}

int sysExit(int status)
{
	endTask(_current->id);
	return (0x0);
}

int sysCheckPid(int pid, int *ptr)
{
	kTask_t *tmpTask = schedFindTask(pid);
	if ((tmpTask != 0x0) && (ptr != 0x0))
		*ptr = tmpTask->state;
	else
		*ptr = 0x0;
	return (0);
}

/************************************************************************

 Function: int sysGetFreePage();
 Description: Allocs A Page To The Users VM Space
 Notes:

 ************************************************************************/
int sysGetFreePage(struct thread *td, u_int32_t *count)
{

	td->td_retval[0] = (int)vmm_get_free_virtual_page(_current->id, *count, VM_THRD);
	return (0);
	// return(vmm_get_free_virtual_page(_current->id, *count, VM_TASK));
}

int sysGetDrives(u_int32_t *ptr)
{
	if (ptr)
		*ptr = 0x0; //(u_int32_t)devices;
	return (0);
}

int sysGetUptime(u_int32_t *ptr)
{
	if (ptr)
		*ptr = systemVitals->sysTicks;
	return (0);
}

int sysGetTime(u_int32_t *ptr)
{
	if (ptr)
		*ptr = systemVitals->sysUptime + systemVitals->timeStart;
	return (0);
}

int sys_getvfscwd(struct thread *td, struct sys_getvfscwd_args *args)
{
	if (args->buf && args->size > 0)
	{
		strncpy(args->buf, _current->oInfo.cwd, args->size - 1);
		args->buf[args->size - 1] = '\0';
	}
	td->td_retval[0] = 0;
	return (0);
}

/**
 * Native syscall 67 — fill the userland buffer with mounted UbixFS pool records
 * (the in-OS `ubpool`/`ubfs` commands).  @args->buf is a struct ubix_pool_info[]
 * (<api/ubfs_pool.h>); the driver enumerates the VFS mount list.
 *
 * @return 0; the pool count is returned in td_retval[0].
 */
int sys_ubfs_query(struct thread *td, struct sys_ubfs_query_args *args)
{
	td->td_retval[0] = ubfs_vfs_query(args->buf, (int)args->max);
	return (0);
}

int sys_disk_query(struct thread *td, struct sys_disk_query_args *args)
{
	td->td_retval[0] = disk_query(args->buf, (int)args->max);
	return (0);
}

int sys_getcwd(struct thread *td, struct sys_getcwd_args *args)
{
	const char *cwd = _current->oInfo.cwd;
	size_t len = strlen(cwd) + 1;

	if (args->buf)
	{
		if (len > args->size)
		{
			td->td_retval[0] = -1;
			return (ERANGE);
		}
		memcpy((char *)args->buf, cwd, len);
	}

	td->td_retval[0] = (int)len;
	return (0);
}

int sys_sched_yield(struct thread *td, void *args)
{
	sched_yield();
	return (0);
}

/* nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
 * FreeBSD POSIX slot 240.  rqtp/rmtp are user pointers — layout:
 *   long tv_sec  (offset 0)
 *   long tv_nsec (offset 4)
 * PIT_TIMER ticks/sec = 200; each tick = 5 ms.
 */
int sys_nanosleep(struct thread *td, void *args)
{
	u_int32_t *params = (u_int32_t *)args;
	const char *rqtp = (const char *)params[0]; /* struct timespec * */
	char *rmtp = (char *)params[1];

	if (!rqtp)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	/* uBixOS's musl is _REDIR_TIME64: struct timespec on i386 is
	 * { int64_t tv_sec; long tv_nsec; } — tv_nsec is at byte offset 8, NOT 4, and
	 * musl always converts to this 64-bit layout before issuing SYS_nanosleep.
	 * Reading tv_nsec as the second 32-bit word (the old code) picked up the high
	 * word of tv_sec (always 0), so every nanosleep computed 0 ticks and returned
	 * immediately — turning every pacing nap into a busy spin (e.g. the aural
	 * mixer at ~90% CPU under TCG).  Read the fields at their real offsets. */
	long long tv_sec = *(const long long *)(rqtp + 0);
	long tv_nsec = *(const long *)(rqtp + 8);
	if (tv_sec < 0 || tv_nsec < 0 || tv_nsec >= 1000000000L)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	/* Convert requested time to PIT ticks (round up). */
	u_int32_t ticks = (u_int32_t)(tv_sec * PIT_TIMER) +
	                  (u_int32_t)((tv_nsec + (1000000000L / PIT_TIMER) - 1) / (1000000000L / PIT_TIMER));

	/* Sleep descheduled for the duration instead of busy-yielding: the old
	 * sched_yield spin kept the caller permanently runnable.  A per-task callout
	 * wakes us at the deadline; ticks==0 (sub-tick request) just returns. */
	if (ticks > 0)
		sched_wait_event_timeout(&g_nanosleep_chan, nanosleep_never, NULL, ticks);

	if (rmtp)
		memset(rmtp, 0, sizeof(long long) + sizeof(long)); /* 64-bit tv_sec(8) + tv_nsec(4) */
	td->td_retval[0] = 0;
	return (0);
}

int sysStartSDE()
{
	int i = 0x0;
	for (i = 0; i < 1400; i++)
	{
		asm("hlt");
	}
	// execThread(sdeThread,0x2000),0x0);
	for (i = 0; i < 1400; i++)
	{
		asm("hlt");
	}
	return (0);
}
