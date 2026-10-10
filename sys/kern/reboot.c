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

/*
 * reboot(2), shared by every architecture: permission check and disk sync here;
 * the actual power-off / reset is the per-arch machine_power() hook.
 */

#include <sys/types.h>
#include <sys/shutdown.h>
#include <sys/thread.h>
#include <lib/kprintf.h>
#include <fs/vfs/vfs.h>
#include <ubixos/sched.h>
#include <ubixos/errno.h>


/**
 * reboot(2), FreeBSD numbering (POSIX slot 55): flush every filesystem, then
 * power off (howto has RB_POWEROFF or RB_HALT) or reset.  Root only.
 *
 * uap points at the raw argument words: [0] = howto.  musl's Linux-style
 * reboot() uses another syscall number (88, taken by getFreePage here), so
 * userland calls this slot directly — see include/api/power.h.
 *
 * @return EPERM for a non-root caller; does not return otherwise.
 */
int sys_reboot(struct thread *td, void *uap)
{
	register_t *args = (register_t *)uap;
	int howto = (int)args[0];

	if (_current->uid != 0)
	{
		td->td_retval[0] = -EPERM;
		return (EPERM);
	}

	int off = (howto & (RB_POWEROFF | RB_HALT)) != 0;
	kprintf("system: %s requested by pid %d; syncing disks...\n", off ? "power-off" : "reboot", (int)_current->id);
	if (vfs_sync_all() != 0)
		kprintf("system: warning: a filesystem failed to sync\n");
	kprintf("system: %s\n", off ? "powering off" : "rebooting");
	machine_power(off);
	return (0); /* not reached */
}
