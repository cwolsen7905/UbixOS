/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * tlstest — end-to-end test for per-thread thread-local storage (Task C).
 *
 * Two worker threads share one address space (and therefore the single LDT[1]
 * descriptor that user %gs = 0xF selects).  Each installs its OWN TLS base via
 * set_thread_area(), writes a unique magic through %gs:0, then spins re-reading
 * %gs:0.  A correct kernel re-installs the resuming thread's base in cpu_switch
 * on every context switch, so each thread always reads back its own magic.  If
 * the bases bled across switches (one shared LDT[1] base) a thread would read
 * its sibling's magic and the test FAILs.
 *
 * A start barrier guarantees both threads have set their bases and are running
 * concurrently, so the check loop genuinely spans context switches between them.
 */

#include <stdio.h>

extern int spawn_thread(int (*fn)(void *), void *stack_top, void *arg);
extern void set_tls(void *base);
extern int read_gs0(void);
extern void write_gs0(int v);

#define NITER 2000000

static char g_stack_a[16384];
static char g_stack_b[16384];

/* Each thread's private TLS block; %gs:0 maps to offset 0 after set_tls(). */
static char g_tls_a[256];
static char g_tls_b[256];

volatile int g_ready_a = 0, g_ready_b = 0;
volatile int g_done_a = 0, g_done_b = 0;
volatile int g_fail_a = 0, g_fail_b = 0;

/*
 * Body shared by both workers: install our TLS base, publish our magic, wait
 * until BOTH workers are running (so switches interleave), then verify our %gs:0
 * keeps returning our own magic across many context switches.
 */
static int worker(char *tls, int magic, volatile int *ready, volatile int *done, volatile int *fail)
{
	volatile long i;

	set_tls(tls);
	write_gs0(magic);
	*ready = 1;

	/* Barrier: don't start checking until the sibling has also set its base. */
	while (!(g_ready_a && g_ready_b))
		;

	for (i = 0; i < NITER; i++) {
		if (read_gs0() != magic) {
			*fail = 1;
			break;
		}
	}

	*done = 1;
	return 0;
}

static int thread_a(void *arg)
{
	(void)arg;
	return worker(g_tls_a, 0x0000A5A5, &g_ready_a, &g_done_a, &g_fail_a);
}

static int thread_b(void *arg)
{
	(void)arg;
	return worker(g_tls_b, 0x00005B5B, &g_ready_b, &g_done_b, &g_fail_b);
}

int main(void)
{
	volatile long i;

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("tlstest: two threads, each with its own %%gs TLS base...\n");

	spawn_thread(thread_a, g_stack_a + sizeof(g_stack_a), (void *)0);
	spawn_thread(thread_b, g_stack_b + sizeof(g_stack_b), (void *)0);

	for (i = 0; i < 2000000000L && !(g_done_a && g_done_b); i++)
		;

	if (!g_done_a || !g_done_b)
		printf("tlstest: FAIL -- threads did not finish (done a=%d b=%d)\n", g_done_a, g_done_b);
	else if (g_fail_a || g_fail_b)
		printf("tlstest: FAIL -- TLS bled across threads (fail a=%d b=%d)\n", g_fail_a, g_fail_b);
	else
		printf("tlstest: PASS -- each thread kept its own TLS across context switches\n");

	printf("tlstest: parent exiting\n");
	return 0;
}
