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
 * ubistry — the UbixOS registry daemon.  Owns the hierarchical settings tree in
 * RAM, loads/persists it to /var/db/ubistry.db, and serves GET/SET/ENUM/DEL
 * requests over the "ubistry" MPI mailbox.  Started early at boot by init via
 * /etc/init.d/15-ubistry.
 */

#include <stdlib.h>
#include <sched.h>
#include <api/ubix.h>
#include <ubistry/ubistry.h>
#include "db.h"
#include "persist.h"
#include "message.h"

/* Loop iterations a pending change waits before being flushed to disk — long
 * enough to coalesce bursts of SETs, short enough to persist promptly. */
#define FLUSH_TICKS 256

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	ub_root(); /* materialise the root container */

	if (persist_load(UBISTRY_DB) != 0)
		ulogf(ULOG_WARNING, "ubistry: %s not found — starting empty", UBISTRY_DB);

	/* Create the serving mailbox LAST, so a client (or the boot path) that sees
	 * the "ubistry" mailbox exist knows the registry is fully loaded and ready —
	 * no window where a GET races an unpopulated db. */
	if (ubistry_init_mbox(UBISTRY_MBOX) != 0)
		exit(1);

	ulogf(ULOG_INFO, "ubistry: ready (db %s)", UBISTRY_DB);

	ubistry_process_messages(); /* drain anything queued during startup */
	for (;;)
	{
		/* Block on the mailbox instead of busy-polling.  When the db is clean,
		 * sleep indefinitely until a request arrives.  When dirty, sleep only up
		 * to FLUSH_TICKS — if that elapses with no further request, persist the
		 * pending changes (write-behind) and go back to an indefinite wait. */
		if (persist_dirty())
		{
			if (!ubistry_wait_and_process(FLUSH_TICKS))
				persist_save(UBISTRY_DB); /* quiet for FLUSH_TICKS while dirty -> flush */
		}
		else
		{
			ubistry_wait_and_process(0); /* clean: block until a request */
		}
	}

	return (0);
}
