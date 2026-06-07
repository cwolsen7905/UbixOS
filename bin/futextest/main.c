/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * futextest — end-to-end test for the futex syscall (Task D).
 *
 * A worker thread does a single FUTEX_WAIT on a shared word; main verifies the
 * thread actually BLOCKS (does not return) until main flips the word and issues
 * FUTEX_WAKE.  This distinguishes a real blocking futex from the old no-op stub
 * (which returned immediately, so the worker would have set g_woke at once).
 *
 * Sequence:
 *   worker: g_ready = 1; futex_wait(&g_futex, 0); g_woke = 1
 *   main:   wait for g_ready; confirm g_woke still 0 after a delay (worker is
 *           blocked in the kernel); then g_futex = 1; FUTEX_WAKE; expect g_woke.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

extern int spawn_thread(int (*fn)(void *), void *stack_top, void *arg);

static char g_stack[16384];

volatile int g_futex = 0;
volatile int g_ready = 0;
volatile int g_woke = 0;
volatile int g_wait_ret = 123;

static int futex(volatile int *uaddr, int op, int val, void *timeout)
{
	return (int)syscall(SYS_futex, uaddr, op, val, timeout);
}

static int worker(void *arg)
{
	(void)arg;
	g_ready = 1;
	g_wait_ret = futex(&g_futex, FUTEX_WAIT, 0, NULL); /* block while *uaddr==0 */
	g_woke = 1;
	return 0;
}

int main(void)
{
	volatile long i;
	int blocked_before_wake;

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("futextest: worker will FUTEX_WAIT; main will FUTEX_WAKE it...\n");

	spawn_thread(worker, g_stack + sizeof(g_stack), (void *)0);

	/* Wait until the worker is about to (or has) entered FUTEX_WAIT. */
	for (i = 0; i < 2000000000L && !g_ready; i++)
		;

	/* Give the worker ample time to enter the kernel and block.  A real
	 * blocking futex leaves g_woke == 0 here; a no-op stub would have let the
	 * worker fall straight through and set g_woke. */
	for (i = 0; i < 50000000L; i++)
		;
	blocked_before_wake = (g_woke == 0);

	/* Release the worker: change the value, then wake. */
	g_futex = 1;
	futex(&g_futex, FUTEX_WAKE, 1, NULL);

	/* The worker should now return from FUTEX_WAIT and set g_woke. */
	for (i = 0; i < 2000000000L && !g_woke; i++)
		;

	if (!g_woke)
		printf("futextest: FAIL -- worker never woke (futex_wait did not return)\n");
	else if (!blocked_before_wake)
		printf("futextest: FAIL -- worker did not block in FUTEX_WAIT (no-op futex?)\n");
	else
		printf("futextest: PASS -- worker blocked in FUTEX_WAIT and woke on FUTEX_WAKE (ret=%d)\n", g_wait_ret);

	printf("futextest: parent exiting\n");
	return 0;
}
