/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * condtest — pthread condition-variable test (Task E2a).
 *
 * A bounded-buffer producer/consumer: the producer blocks on `not_full` when the
 * ring is full, the consumer blocks on `not_empty` when it is empty, each signals
 * the other.  This exercises pthread_cond_wait/signal on BOTH sides, which run on
 * the kernel futex (FUTEX_WAIT/-timeout + the REQUEUE-as-WAKE path).  PASS
 * requires every produced item to be consumed exactly once (checksum match),
 * proving no lost wakeups and correct mutex/cond handoff.
 */

#include <stdio.h>
#include <pthread.h>

#define CAP 8
#define NITEMS 100000

static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_not_full = PTHREAD_COND_INITIALIZER;

static int g_buf[CAP];
static int g_head, g_tail, g_count;
static long long g_sum_produced, g_sum_consumed; /* i386 long is 32-bit; sum ~5e9 needs 64-bit */

static void *producer(void *arg)
{
	int i;
	(void)arg;

	for (i = 0; i < NITEMS; i++) {
		pthread_mutex_lock(&g_m);
		while (g_count == CAP)
			pthread_cond_wait(&g_not_full, &g_m);
		g_buf[g_tail] = i;
		g_tail = (g_tail + 1) % CAP;
		g_count++;
		g_sum_produced += i;
		pthread_cond_signal(&g_not_empty);
		pthread_mutex_unlock(&g_m);
	}
	return NULL;
}

static void *consumer(void *arg)
{
	int i, v;
	(void)arg;

	for (i = 0; i < NITEMS; i++) {
		pthread_mutex_lock(&g_m);
		while (g_count == 0)
			pthread_cond_wait(&g_not_empty, &g_m);
		v = g_buf[g_head];
		g_head = (g_head + 1) % CAP;
		g_count--;
		g_sum_consumed += v;
		pthread_cond_signal(&g_not_full);
		pthread_mutex_unlock(&g_m);
	}
	return NULL;
}

int main(void)
{
	pthread_t p, c;
	long long expected = (long long)NITEMS * (NITEMS - 1) / 2;

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("condtest: bounded-buffer producer/consumer, %d items, ring=%d...\n", NITEMS, CAP);

	pthread_create(&p, NULL, producer, NULL);
	pthread_create(&c, NULL, consumer, NULL);
	pthread_join(p, NULL);
	pthread_join(c, NULL);

	if (g_count != 0)
		printf("condtest: FAIL -- ring not drained (count=%d)\n", g_count);
	else if (g_sum_produced != expected || g_sum_consumed != expected)
		printf("condtest: FAIL -- checksum mismatch (produced=%lld consumed=%lld expected=%lld)\n",
		       g_sum_produced, g_sum_consumed, expected);
	else
		printf("condtest: PASS -- %d items through cond-synced ring; checksum=%lld\n", NITEMS, g_sum_consumed);

	return 0;
}
