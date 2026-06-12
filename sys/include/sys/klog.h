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

#ifndef _SYS_KLOG_H
#define _SYS_KLOG_H

#include <sys/types.h>

/* Log severity levels (matches syslog convention) */
#define KLOG_EMERG 0   /* system unusable */
#define KLOG_ALERT 1   /* action must be taken immediately */
#define KLOG_CRIT 2    /* critical condition */
#define KLOG_ERR 3     /* error condition */
#define KLOG_WARNING 4 /* warning condition */
#define KLOG_NOTICE 5  /* normal but significant */
#define KLOG_INFO 6    /* informational */
#define KLOG_DEBUG 7   /* debug messages */

#define KLOG_MSG_MAX 188   /* max message text (including NUL) */
#define KLOG_RING_SIZE 256 /* number of ring entries (power of two) */

/*
 * One log entry.  Kept small so the ring fits in a few KB.
 * seq is a monotonically increasing generation counter; logd uses it
 * as a cursor to drain only new entries each poll cycle.
 */
struct klog_entry
{
	u_int32_t ke_seq;   /* monotonic sequence number */
	u_int32_t ke_ticks; /* systemVitals->sysTicks at log time */
	u_int32_t ke_time;  /* Unix timestamp (seconds) at log time */
	u_int8_t ke_level;  /* KLOG_* severity */
	char ke_msg[KLOG_MSG_MAX];
} __attribute__((packed));

/* Kernel-internal: push a pre-formatted message into the ring */
void klog_push(u_int8_t level, const char *msg);

/* Primary logging entry point — printf-style, safe from any context */
void klog(u_int8_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Read up to max_entries entries with seq >= start_seq into buf.
 * Returns the number of entries copied.  Used by sys_klog_read. */
int klog_read(struct klog_entry *buf, int max_entries, u_int32_t start_seq);

/* Like klog_read but blocks (leaves the run queue) until an entry with
 * seq >= start_seq exists — lets logd drain without busy-polling. */
int klog_read_wait(struct klog_entry *buf, int max_entries, u_int32_t start_seq);

/* Current highest sequence number + 1 (next seq to be written) */
u_int32_t klog_next_seq(void);

#endif /* _SYS_KLOG_H */
