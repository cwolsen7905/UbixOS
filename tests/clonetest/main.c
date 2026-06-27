/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * clonetest — end-to-end test for the x86_64 rfork(RFMEM)/clone() thread-create
 * path (kernel sys_rfork + the asm wrapper), independent of TLS/futex.
 *
 * Spawns one thread on a caller-supplied stack; the thread writes two SHARED
 * globals.  If main() observes those writes, then the syscall created a runnable
 * context that shares main()'s address space and the scheduler ran it => clone()
 * works.  The thread exits; main keeps running (exercises "non-last thread exits").
 */
#include <unistd.h>
#include <sched.h>
#include <stdio.h>

extern long spawn_thread(int (*fn)(void *), void *stack_top, void *arg);

static volatile int g_ran = 0;
static volatile long g_wrote = 0;
static char g_stack[65536];

static int worker(void *arg)
{
	g_wrote = (long)arg + 42; /* write a SHARED global (proves shared AS) */
	g_ran = 1;
	return 7;
}

int main(void)
{
	long tid;
	int spins;

	write(1, "CLONE-ENTER\n", 12); /* raw syscall, before any musl stdio */
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("CLONE: spawning shared-AS thread...\n");
	tid = spawn_thread(worker, g_stack + sizeof(g_stack), (void *)100);
	printf("CLONE: rfork returned tid=%ld\n", tid);

	for (spins = 0; !g_ran && spins < 2000000; spins++)
		sched_yield();

	printf("CLONE RESULT: ran=%d wrote=%ld (want 142) tid=%ld -> %s\n",
	       g_ran, g_wrote, tid,
	       (tid > 0 && g_ran && g_wrote == 142) ? "PASS" : "FAIL");
	return 0;
}
