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
 * logd — UbixOS kernel log daemon.
 *
 * Drains the kernel log ring via sys_klog_read (native syscall 47) and
 * appends formatted entries to sys:/var/log/messages.
 *
 * Design:
 *   - Runs as a long-lived service launched early by init.
 *   - Tracks the last sequence number it consumed; on each poll cycle it
 *     calls sys_klog_read for any new entries since that seq.
 *   - Yields between polls to avoid burning CPU when the log is quiet.
 *   - Can be killed and restarted without losing already-written entries;
 *     entries buffered in the kernel ring during the gap are drained on
 *     the next start (up to KLOG_RING_SIZE entries).
 *   - Flushes to disk after each batch so a crash doesn't lose messages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>

/* Match the kernel definition exactly */
#define KLOG_MSG_MAX   188
#define KLOG_RING_SIZE 256

#define KLOG_EMERG    0
#define KLOG_ALERT    1
#define KLOG_CRIT     2
#define KLOG_ERR      3
#define KLOG_WARNING  4
#define KLOG_NOTICE   5
#define KLOG_INFO     6
#define KLOG_DEBUG    7

struct klog_entry {
	uint32_t  ke_seq;
	uint32_t  ke_ticks;
	uint8_t   ke_level;
	char      ke_msg[KLOG_MSG_MAX];
} __attribute__((packed));

/* Native syscall 47 — sys_klog_read */
static int sys_klog_read(struct klog_entry *buf, int max, uint32_t start_seq)
{
	int ret;
	__asm__ volatile (
		"movl $47, %%eax\n"
		"int  $0x81\n"
		: "=a"(ret)
		: "b"(buf), "c"(max), "d"(start_seq)
		: "memory"
	);
	return ret;
}

/* Native syscall 49 — sys_klog_write */
static void sys_klog_write(uint8_t level, const char *msg)
{
	__asm__ volatile (
		"movl $49, %%eax\n"
		"int  $0x81\n"
		:
		: "b"((uint32_t)level), "c"(msg)
		: "eax", "memory"
	);
}

#define LOG_PATH    "sys:/var/log/messages"
#define BATCH_SIZE  32

static const char *level_name(uint8_t level)
{
	switch (level) {
	case KLOG_EMERG:   return "EMERG";
	case KLOG_ALERT:   return "ALERT";
	case KLOG_CRIT:    return "CRIT";
	case KLOG_ERR:     return "ERR";
	case KLOG_WARNING: return "WARN";
	case KLOG_NOTICE:  return "NOTICE";
	case KLOG_INFO:    return "INFO";
	case KLOG_DEBUG:   return "DEBUG";
	default:           return "?";
	}
}

int main(int argc, char **argv)
{
	struct klog_entry batch[BATCH_SIZE];
	uint32_t          next_seq = 0;
	FILE             *log;
	int               n, i;

	(void)argc; (void)argv;

	sys_klog_write(KLOG_NOTICE, "logd: starting");

	log = fopen(LOG_PATH, "a");
	if (log == NULL) {
		/* Can't open log file — try to create the directory path */
		log = fopen(LOG_PATH, "w");
		if (log == NULL) {
			sys_klog_write(KLOG_ERR, "logd: failed to open " LOG_PATH);
			for (;;)
				sched_yield();
		}
	}

	sys_klog_write(KLOG_NOTICE, "logd: log file open");
	fprintf(log, "--- logd started ---\n");
	fflush(log);

	for (;;) {
		n = sys_klog_read(batch, BATCH_SIZE, next_seq);

		for (i = 0; i < n; i++) {
			struct klog_entry *e = &batch[i];
			fprintf(log, "[%8u] <%s> %s\n",
			    e->ke_ticks, level_name(e->ke_level), e->ke_msg);
			if (e->ke_seq >= next_seq)
				next_seq = e->ke_seq + 1;
		}

		if (n > 0)
			fflush(log);

		sched_yield();
	}

	return 0;
}
