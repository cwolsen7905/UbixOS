/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * pthreadtest — first end-to-end musl pthreads test (Task E, milestone 1).
 *
 * Exercises the whole stack built in Tasks A-D through the real libc API:
 *   - pthread_create  -> __clone -> rfork (new context, shared AS)
 *   - per-thread TLS  -> CLONE_SETTLS install + cpu_switch restore (errno etc.)
 *   - pthread_mutex   -> futex WAIT/WAKE
 *   - pthread_join    -> detach_state futex + CLONE_CHILD_CLEARTID lock release
 *
 * N threads each increment a shared counter NINC times under a mutex; main joins
 * them and checks the total is exactly N*NINC (no lost updates => the mutex, and
 * thus futex, works) and that each join returns the thread's own value.
 */

#include <stdio.h>
#include <pthread.h>

#define NTHREADS 4
#define NINC 20000

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile long g_counter = 0;

static void *worker(void *arg)
{
	int id = (int)(long)arg;
	int i;

	/* pthread_self() differs per thread — visual proof these are distinct
	 * contexts with their own TLS. */
	printf("  [thread %d] started (self=%p)\n", id, (void *)pthread_self());

	for (i = 0; i < NINC; i++) {
		pthread_mutex_lock(&g_mutex);
		g_counter++;
		pthread_mutex_unlock(&g_mutex);

		/* Progress beat at each quarter — interleaving across threads is the
		 * visual that they really run concurrently.  The counter read here is
		 * an intentional lock-free peek, just for display. */
		if ((i + 1) % (NINC / 4) == 0)
			printf("  [thread %d] %d/%d increments done, shared counter ~%ld\n",
			       id, i + 1, NINC, g_counter);
	}

	printf("  [thread %d] finished\n", id);
	return (void *)(long)(id + 100);
}

int main(void)
{
	pthread_t t[NTHREADS];
	int i;
	int ok = 1;
	long expected = (long)NTHREADS * NINC;

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("pthreadtest: creating %d threads, %d mutex-guarded increments each...\n", NTHREADS, NINC);

	for (i = 0; i < NTHREADS; i++) {
		if (pthread_create(&t[i], NULL, worker, (void *)(long)i) != 0) {
			printf("pthreadtest: FAIL -- pthread_create %d failed\n", i);
			return 1;
		}
	}

	for (i = 0; i < NTHREADS; i++) {
		void *ret = NULL;
		pthread_join(t[i], &ret);
		if ((int)(long)ret != i + 100) {
			ok = 0;
			printf("  join %d: ret=%d expected %d\n", i, (int)(long)ret, i + 100);
		}
	}

	if (!ok)
		printf("pthreadtest: FAIL -- wrong join return value(s)\n");
	else if (g_counter != expected)
		printf("pthreadtest: FAIL -- counter=%ld expected %ld (lost updates -> mutex/futex broken)\n",
		       g_counter, expected);
	else
		printf("pthreadtest: PASS -- %d threads joined; mutex-protected counter=%ld\n", NTHREADS, g_counter);

	return 0;
}
