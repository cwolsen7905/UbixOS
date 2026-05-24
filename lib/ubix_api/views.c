/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <api/ubix.h>
#include <string.h>

/* Forward-declare only what we need to avoid pulling sys/types.h
 * through sys/mpi.h in the -nostdinc ubix_api build. */
#define MESSAGE_LENGTH 248
struct _mpi_msg { unsigned int header; unsigned int pid; char data[MESSAGE_LENGTH]; struct _mpi_msg *next; };
extern int mpi_postMessage(const char *, unsigned int, struct _mpi_msg *);

/*
 * views_running — test whether the views compositor mailbox exists.
 *
 * Sends a DISPLAY_QUERY (type 8) with an empty reply field.
 * mpi_postMessage returns non-zero when the mailbox does not exist.
 * Views will attempt to reply to the empty reply field, fail silently,
 * and continue — no crash or visible side-effect.
 *
 * Returns 1 if views is running, 0 if not.
 */
int
views_running(void)
{
	struct _mpi_msg msg;
	memset(&msg, 0, sizeof(msg));
	msg.header = 8; /* DISPLAY_QUERY */
	return mpi_postMessage("views", 8, &msg) == 0;
}
