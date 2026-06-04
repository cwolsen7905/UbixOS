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
 * Callout (one-shot timer) subsystem.
 *
 * A callout fires fn(arg) once, `ticks` scheduler ticks after it is armed.
 * Callouts live on a single list sorted by expiry, so the per-tick cost is
 * O(1) (only the head is inspected) — replacing the previous O(n)-every-tick
 * scan of the whole task list for timed sleepers.
 *
 * Locking: the callout list is protected by schedulerSpinLock.  Every entry
 * point here MUST be called with that lock held and interrupts disabled (the
 * scheduler already runs in that state).  callout_run_expired() invokes fn()
 * with the lock still held, so callbacks must be lock-safe (e.g. the timed-
 * sleep waker just re-enqueues a task via rq_enqueue_locked()).  Heavier work
 * should hand off to a thread rather than run in the callback.
 */

#ifndef _UBIXOS_CALLOUT_H
#define _UBIXOS_CALLOUT_H

#include <sys/types.h>

struct callout {
	u_int32_t       expiry;  /* sysTicks at which to fire (valid when armed) */
	void          (*fn)(void *arg);
	void           *arg;
	struct callout *next;    /* expiry-sorted singly-linked list */
	u_int8_t        armed;
};

/** One-time initialization of a callout (zeroes it). */
void callout_init(struct callout *c);

/**
 * Arm (or re-arm) c to fire fn(arg) `ticks` ticks from now.  Re-arming an
 * already-armed callout cancels the prior arming.  ticks==0 fires on the next
 * callout_run_expired().  Caller holds schedulerSpinLock.
 */
void callout_reset(struct callout *c, u_int32_t ticks, void (*fn)(void *arg), void *arg);

/** Cancel c if armed (no-op otherwise).  Caller holds schedulerSpinLock. */
void callout_stop(struct callout *c);

/**
 * Fire every callout whose expiry has elapsed (expiry <= now), in order.
 * Called once per scheduler tick with schedulerSpinLock held.
 */
void callout_run_expired(u_int32_t now);

#endif /* _UBIXOS_CALLOUT_H */
