/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * detachtest — detached-thread teardown test (Task E2b).
 *
 * Detached threads have no joiner, so on exit each frees its OWN stack via
 * musl's __unmapself -> the kernel thread_exit_unmap syscall (unmap own stack +
 * exit in one trap), and the kernel's CLONE_CHILD_CLEARTID handling must release
 * musl's thread-list lock.  If either is wrong this either deadlocks (the next
 * pthread_create can't take the thread-list lock) or faults.
 *
 * We spawn detached threads in WAVES: each wave can only make progress if the
 * previous wave's detached threads exited cleanly and released the lock.  PASS =
 * all waves complete (no deadlock/crash) and the mutex-guarded counter is exact.
 */

#include <stdio.h>
#include <pthread.h>

#define WAVES 5
#define PER_WAVE 6
#define NINC 5000
#define TOTAL (WAVES * PER_WAVE)

static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static volatile long g_counter;
static volatile int g_done; /* threads that finished their work */

static void *worker(void *arg)
{
	int i;
	(void)arg;

	for (i = 0; i < NINC; i++) {
		pthread_mutex_lock(&g_m);
		g_counter++;
		pthread_mutex_unlock(&g_m);
	}

	pthread_mutex_lock(&g_m);
	g_done++;
	pthread_mutex_unlock(&g_m);
	return NULL; /* detached -> __pthread_exit -> __unmapself */
}

int main(void)
{
	pthread_attr_t attr;
	int wave, i;
	volatile long spin;
	long expected = (long)TOTAL * NINC;

	setvbuf(stdout, NULL, _IONBF, 0);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	printf("detachtest: %d waves x %d detached threads (self-teardown via __unmapself)...\n", WAVES, PER_WAVE);

	for (wave = 0; wave < WAVES; wave++) {
		int target = (wave + 1) * PER_WAVE;

		for (i = 0; i < PER_WAVE; i++) {
			pthread_t t;
			if (pthread_create(&t, &attr, worker, NULL) != 0) {
				printf("detachtest: FAIL -- pthread_create failed (wave %d) -- likely tl-lock deadlock\n", wave);
				return 1;
			}
		}

		/* Wait for this wave's workers to finish their counting.  If a prior
		 * detached exit failed to release the thread-list lock, the creates
		 * above would have hung — reaching here proves they didn't. */
		for (spin = 0; spin < 2000000000L && g_done < target; spin++)
			;
		if (g_done < target) {
			printf("detachtest: FAIL -- wave %d stalled (g_done=%d, want %d)\n", wave, g_done, target);
			return 1;
		}
		printf("  wave %d ok: %d/%d threads done, counter=%ld\n", wave, g_done, TOTAL, g_counter);
	}

	/* Let the last wave's threads finish self-unmapping. */
	for (spin = 0; spin < 200000000L; spin++)
		;

	pthread_attr_destroy(&attr);

	if (g_counter != expected)
		printf("detachtest: FAIL -- counter=%ld expected %ld\n", g_counter, expected);
	else
		printf("detachtest: PASS -- %d detached threads across %d waves; counter=%ld\n", TOTAL, WAVES, g_counter);

	return 0;
}
