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

#include <lib/kconsole.h>
#include <ubixos/spinlock.h>
#include <ubixos/wait.h>

/*
 * The registered sink list and the primary-suspend flag.  Registration happens
 * once at boot; emit is the hot path.  Under SMP two CPUs can kprintf at once, so
 * kconsole_emit serialises on g_console_lock with IRQs disabled (an ISR on the same
 * CPU may also kprintf — IRQ-off prevents a non-recursive self-deadlock) so each
 * line is emitted atomically instead of interleaving characters on the serial port.
 */
static struct kconsole *g_console_list = 0;
static int g_primary_suspended = 0;
static struct spinLock g_console_lock = SPIN_LOCK_INITIALIZER;

/*
 * Console locking is engaged only once SMP is active (kconsole_enable_mp, called
 * just before the APs are released).  Early boot is single-CPU AND runs before the
 * MMU is enabled — and the spinlock's LDXR/STXR exclusive monitor is undefined on
 * pre-MMU memory: TCG tolerates it but HVF hangs the very first kprintf ("boot OK")
 * before the MMU comes up.  So before SMP, kconsole_emit emits unlocked (no other CPU
 * exists to race it); after, it serialises.
 */
static volatile int g_console_mp = 0;

/** Engage console locking (call once SMP is up + MMU on, before releasing APs). */
void kconsole_enable_mp(void)
{
	g_console_mp = 1;
}

/**
 * Register a console sink.
 *
 * Idempotent: a sink already on the list is ignored, so an arch that re-runs its
 * console init does not double-emit.  Sinks are appended so registration order
 * (serial first, then the primary) is the emit order.
 */
void kconsole_register(struct kconsole *kc)
{
	struct kconsole **pp;

	if (kc == 0)
		return;

	for (pp = &g_console_list; *pp != 0; pp = &(*pp)->next)
	{
		if (*pp == kc)
			return; /* already registered */
	}
	kc->next = 0;
	*pp = kc;
}

/**
 * Walk every registered sink, emitting the string one character at a time.
 *
 * A primary, suspendable sink is skipped while the primary console is
 * suspended (the compositor owns the screen); serial and other always-on sinks
 * continue to receive output.
 */
void kconsole_emit(const char *s)
{
	struct kconsole *kc;
	const char *p;
	u_int32_t flags;

	if (s == 0)
		return;

	/* Before SMP is up, emit unlocked: single-CPU, and the lock's atomics are unsafe
	 * pre-MMU (see g_console_mp note).  Once SMP is active, serialise the whole string
	 * with IRQs disabled so concurrent kprintf()s don't interleave character-by-char. */
	if (!g_console_mp)
	{
		for (kc = g_console_list; kc != 0; kc = kc->next)
		{
			if (g_primary_suspended &&
			    (kc->flags & (KC_PRIMARY | KC_SUSPENDABLE)) == (KC_PRIMARY | KC_SUSPENDABLE))
				continue;
			if (kc->putc == 0)
				continue;
			for (p = s; *p != '\0'; p++)
				kc->putc((int)(unsigned char)*p);
		}
		return;
	}

	save_flags(flags);
	cli();
	spinLock(&g_console_lock);
	for (kc = g_console_list; kc != 0; kc = kc->next)
	{
		if (g_primary_suspended && (kc->flags & (KC_PRIMARY | KC_SUSPENDABLE)) == (KC_PRIMARY | KC_SUSPENDABLE))
			continue;
		if (kc->putc == 0)
			continue;
		for (p = s; *p != '\0'; p++)
			kc->putc((int)(unsigned char)*p);
	}
	spinUnlock(&g_console_lock);
	restore_flags(flags);
}

/**
 * Suspend the primary visible console (the compositor is claiming the screen).
 * Serial and other always-on sinks are unaffected.
 */
void kconsole_suspend_primary(void)
{
	g_primary_suspended = 1;
}

/**
 * Resume the primary visible console (logout, or a panic reclaiming the
 * display).
 */
void kconsole_resume_primary(void)
{
	g_primary_suspended = 0;
}
