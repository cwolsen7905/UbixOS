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
 *
 * mpi_rpc.c — request/reply helpers over MPI with envelope correlation.
 *
 * MPI's transport is FIFO-ordered and SMP-safe, but a reply mailbox shared by a
 * process can hold a stale reply from an earlier (timed-out or pipelined) request;
 * FIFO then hands that wrong reply to the next waiter.  These helpers correlate a
 * reply to its request via the kernel-stamped envelope (msg_id / in_reply_to), so
 * the answer a caller gets is provably the answer to ITS call — the generic fix that
 * every service can use instead of hand-rolling a per-protocol sequence field.
 */

#include <sched.h>
#include <sys/mpi.h>

/* Mismatched (stale/crossed) replies discarded before giving up.  A correlated reply
 * normally arrives first or second; this only bounds a pathological storm. */
#define MPI_CALL_MAX_STALE 64

/**
 * Send @req to @dest and block for the reply that answers exactly this call.
 *
 * The caller owns @reply_mbox (create it once, e.g. mpi_createMbox) and must put that
 * name wherever the destination protocol expects the reply address (typically a field
 * in @req->data) so the server knows where to answer.  The kernel stamps @req->msg_id
 * on post; this waits for a message on @reply_mbox whose in_reply_to matches it,
 * discarding any non-matching (stale) reply.
 *
 * @param want_header expected reply header (a second guard alongside the id).
 * @param timeout     per-wait blocking timeout in scheduler ticks (0 = block forever).
 * @return 0 on success (@reply filled), -1 if the daemon is unreachable or it times out.
 */
int mpi_call(const char *dest,
             const char *reply_mbox,
             mpi_message_t *req,
             uint32_t want_header,
             mpi_message_t *reply,
             uint32_t timeout)
{
	uint32_t id;
	int stale = 0;

	if (dest == 0 || reply_mbox == 0 || req == 0 || reply == 0)
		return (-1);

	req->in_reply_to = 0; /* this is a request, not a reply */
	if (mpi_postMessage((char *)dest, req->header, req) != 0)
		return (-1); /* no such mailbox — daemon not up; caller falls back */
	id = req->msg_id;    /* the kernel wrote the assigned id back into req */

	for (;;)
	{
		if (mpi_waitMessage((char *)reply_mbox, reply, timeout) != 0)
			return (-1); /* timed out with no message */
		if (reply->in_reply_to == id && (want_header == 0 || reply->header == want_header))
			return (0);
		if (++stale > MPI_CALL_MAX_STALE)
			return (-1); /* give up rather than loop forever on crossed traffic */
	}
}

/**
 * Reply to a received request: post @rsp to @reply_mbox tagged as the answer to @req.
 *
 * Sets @rsp->in_reply_to to the request's msg_id so the requester's mpi_call() matches
 * it.  @reply_mbox is the address the requester advertised (from @req's payload).
 *
 * @return 0 on success, non-zero if the reply mailbox does not exist.
 */
int mpi_reply(const char *reply_mbox, mpi_message_t *req, uint32_t header, mpi_message_t *rsp)
{
	if (reply_mbox == 0 || req == 0 || rsp == 0)
		return (-1);
	rsp->in_reply_to = req->msg_id;
	rsp->header = header;
	return (mpi_postMessage((char *)reply_mbox, header, rsp));
}
