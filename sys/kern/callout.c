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
 * Callout (one-shot timer) subsystem — expiry-sorted list.
 *
 * See <ubixos/callout.h> for the contract.  All entry points run with
 * schedulerSpinLock held (the scheduler owns the list), so this file takes no
 * locks of its own.  The list is kept sorted by expiry so callout_run_expired()
 * only walks the prefix that has actually elapsed.
 *
 * Expiry uses sysTicks (32-bit, wraps ~248 days at 200 Hz); all comparisons are
 * signed-difference so wrap is handled.
 */

#include <ubixos/callout.h>
#include <ubixos/vitals.h>

/* Head of the expiry-sorted list (soonest first).  Protected by
 * schedulerSpinLock; only ever touched with that lock held. */
static struct callout *g_callout_head = NULL;

/**
 * Remove c from the list if present.  Internal helper; caller holds the lock.
 */
static void callout_unlink(struct callout *c)
{
	struct callout **pp = &g_callout_head;

	while (*pp != NULL)
	{
		if (*pp == c)
		{
			*pp = c->next;
			c->next = NULL;
			return;
		}
		pp = &(*pp)->next;
	}
}

void callout_init(struct callout *c)
{
	c->expiry = 0;
	c->fn = NULL;
	c->arg = NULL;
	c->next = NULL;
	c->armed = 0;
}

void callout_reset(struct callout *c, u_int32_t ticks, void (*fn)(void *arg), void *arg)
{
	struct callout **pp;
	u_int32_t expiry = systemVitals->sysTicks + ticks;

	/* Cancel any prior arming so the list never contains c twice. */
	if (c->armed)
		callout_unlink(c);

	c->expiry = expiry;
	c->fn = fn;
	c->arg = arg;
	c->armed = 1;

	/* Insert keeping the list sorted by expiry (signed compare for wrap). */
	pp = &g_callout_head;
	while (*pp != NULL && (int32_t)((*pp)->expiry - expiry) <= 0)
		pp = &(*pp)->next;
	c->next = *pp;
	*pp = c;
}

void callout_stop(struct callout *c)
{
	if (c->armed)
	{
		callout_unlink(c);
		c->armed = 0;
	}
}

void callout_run_expired(u_int32_t now)
{
	/*
	 * The list is expiry-sorted, so once we hit a not-yet-due entry we are
	 * done.  Detach each due callout (mark it disarmed) before invoking fn so
	 * the callback may safely re-arm or free it.
	 */
	while (g_callout_head != NULL && (int32_t)(now - g_callout_head->expiry) >= 0)
	{
		struct callout *c = g_callout_head;
		void (*fn)(void *) = c->fn;
		void *arg = c->arg;

		g_callout_head = c->next;
		c->next = NULL;
		c->armed = 0;

		if (fn != NULL)
			fn(arg);
	}
}
