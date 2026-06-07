/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * clonetest — end-to-end test for the rfork(RFMEM)/clone() thread-create path
 * (kernel sys_rfork + the clone wrapper), independent of TLS/futex.
 *
 * It spawns one thread on a caller-supplied stack and has it write two SHARED
 * globals.  If main() observes those writes, then: the syscall created a
 * runnable context, that context shares main()'s address space (it wrote our
 * globals), and the scheduler ran it — i.e. clone() works.  The thread then
 * exits; main keeps running, which also exercises the "non-last thread exits"
 * address-space refcount (its exit must NOT tear down the shared AS).
 */

#include <stdio.h>

/* Shared with the spawned thread via the common address space. */
volatile int g_ran = 0;
volatile int g_arg = 0;

extern int spawn_thread(int (*fn)(void *), void *stack_top, void *arg);

static char g_tstack[16384];

/* Runs on the new thread's stack, in main()'s address space.  Avoids anything
 * needing TLS (no errno/stdio here) — just touches shared memory and returns
 * (the wrapper turns the return into SYS_exit). */
static int thread_fn(void *arg)
{
	g_arg = (int)(long)arg;
	g_ran = 1;
	return 7;
}

int main(void)
{
	int tid;
	volatile long i;

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("clonetest: spawning a thread in the shared address space...\n");

	tid = spawn_thread(thread_fn, g_tstack + sizeof(g_tstack), (void *)0x1234);
	printf("clonetest: parent: spawn_thread() returned tid=%d\n", tid);

	/* Yield CPU by spinning until the thread sets the shared flag (or we give
	 * up).  Preemptive scheduling should run the thread during this loop. */
	for (i = 0; i < 2000000000L && !g_ran; i++)
		;

	if (g_ran)
		printf("clonetest: PASS -- thread ran in our AS; g_arg=0x%x (expect 0x1234)\n", g_arg);
	else
		printf("clonetest: FAIL -- thread never ran (g_ran still 0)\n");

	printf("clonetest: parent exiting\n");
	return 0;
}
