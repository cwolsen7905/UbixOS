/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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
 * Kernel log ring buffer.
 *
 * Design constraints:
 *   - klog_push() must be safe from interrupt context: no locks, no blocking.
 *   - On a uniprocessor kernel the ISR is atomic relative to process context,
 *     so a simple head counter with no explicit lock is correct.
 *   - logd drains via sys_klog_read (syscall 47), using the monotonic seq
 *     field as a cursor — it never has to track ring head/tail itself.
 *   - When the ring is full the oldest entry is silently overwritten.
 */

#include <sys/klog.h>
#include <sys/sysproto.h>
#include <ubixos/vitals.h>
#include <lib/kprintf.h>
#include <string.h>
#include <stdarg.h>

static struct klog_entry klog_ring[KLOG_RING_SIZE];
static volatile uint32_t klog_seq  = 0;   /* next seq to assign */

void
klog_push(uint8_t level, const char *msg)
{
	uint32_t seq  = klog_seq++;
	uint32_t slot = seq & (KLOG_RING_SIZE - 1);
	struct klog_entry *e = &klog_ring[slot];

	e->ke_seq   = seq;
	e->ke_ticks = systemVitals ? systemVitals->sysTicks : 0;
	e->ke_time  = systemVitals ? (systemVitals->timeStart + systemVitals->sysUptime) : 0;
	e->ke_level = level;

	strncpy(e->ke_msg, msg, KLOG_MSG_MAX - 1);
	e->ke_msg[KLOG_MSG_MAX - 1] = '\0';
}

void
klog(uint8_t level, const char *fmt, ...)
{
	char     buf[KLOG_MSG_MAX];
	va_list  ap;
	int      n;

	va_start(ap, fmt);
	n = kvprintf(fmt, NULL, buf, 10, ap);
	va_end(ap);
	if (n < KLOG_MSG_MAX)
		buf[n] = '\0';
	else
		buf[KLOG_MSG_MAX - 1] = '\0';

	klog_push(level, buf);
}

int
klog_read(struct klog_entry *buf, int max_entries, uint32_t start_seq)
{
	uint32_t cur_seq = klog_seq;
	uint32_t oldest;
	int      n = 0;
	uint32_t s;

	if (max_entries <= 0)
		return (0);

	/* Oldest seq still in the ring (may have wrapped) */
	oldest = (cur_seq > KLOG_RING_SIZE) ? (cur_seq - KLOG_RING_SIZE) : 0;

	/* Clamp start_seq so we never read stale overwritten slots */
	if (start_seq < oldest)
		start_seq = oldest;

	for (s = start_seq; s < cur_seq && n < max_entries; s++) {
		uint32_t slot = s & (KLOG_RING_SIZE - 1);
		/* Verify seq matches — entry may have been overwritten mid-read */
		if (klog_ring[slot].ke_seq == s)
			buf[n++] = klog_ring[slot];
	}

	return (n);
}

uint32_t
klog_next_seq(void)
{
	return (klog_seq);
}

int
sys_klog_write(struct thread *td, struct sys_klog_write_args *args)
{
	char buf[KLOG_MSG_MAX];

	if (args->msg == NULL) {
		td->td_retval[0] = -1;
		return (0);
	}
	strncpy(buf, args->msg, KLOG_MSG_MAX - 1);
	buf[KLOG_MSG_MAX - 1] = '\0';
	klog_push(args->level, buf);
	td->td_retval[0] = 0;
	return (0);
}
