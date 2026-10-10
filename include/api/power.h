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
 * api/power.h — restart / shut down the machine from userland.
 *
 * The kernel's reboot(2) (POSIX slot 55, FreeBSD numbering) is root-only and
 * flushes every filesystem first.  ubix_power() calls it directly for root;
 * any other caller (the desktop session runs as the signed-in user) asks init
 * (PID 1, root) over its MPI mailbox to do it — a local desktop user may
 * restart or shut down, as on other desktop systems.
 */

#pragma once

#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/mpi.h> /* no C++ guards of its own */

#define UBIX_POWER_REBOOT 0x0000 /* reset the machine */
#define UBIX_POWER_OFF 0x4000    /* RB_POWEROFF: power the machine off */

#define UBIX_SYS_REBOOT 55         /* POSIX syscall slot (musl's SYS_reboot differs) */
#define INIT_MSG_POWER 0x504F5752u /* 'POWR': init mailbox request, data = struct init_power_req */

	struct init_power_req
	{
		int howto; /* UBIX_POWER_* */
	};

	long syscall(long number, ...);

	/**
	 * Restart (UBIX_POWER_REBOOT) or power off (UBIX_POWER_OFF) the machine.
	 *
	 * @return does not return on success; -1 if the request could not be made.
	 */
	static inline int ubix_power(int howto)
	{
		if (getuid() == 0)
			return (int)syscall(UBIX_SYS_REBOOT, howto);

		mpi_message_t m;
		struct init_power_req *r = (struct init_power_req *)m.data;
		r->howto = howto;
		m.header = INIT_MSG_POWER;
		return mpi_postMessage("init", INIT_MSG_POWER, &m) == 0 ? 0 : -1;
	}

#ifdef __cplusplus
}
#endif
