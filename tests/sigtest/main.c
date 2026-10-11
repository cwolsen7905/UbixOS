/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * sigtest — checks the signal ABI between musl and the kernel, and wait()
 * exit statuses.  Prints one PASS/FAIL line per check and exits non-zero
 * if any check failed.
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static volatile int g_hits;
static volatile int g_signo;

static int g_fail;

static char g_altstack[65536];
static volatile int g_alt_on;    /* handler's frame was inside g_altstack */
static volatile int g_alt_flags; /* ss_flags sigaltstack reported inside the handler */
static volatile int g_alt_eperm; /* changing the stack while on it failed with EPERM */

static void check(const char *what, int ok)
{
	printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		g_fail = 1;
}

static void on_usr1(int sig, siginfo_t *si, void *ctx)
{
	(void)sig;
	(void)ctx;
	g_hits++;
	g_signo = si ? si->si_signo : -1;
}

/**
 * SIGUSR2 handler for the sigaltstack checks: records whether it runs on
 * g_altstack, what sigaltstack reports from inside it, and that the stack
 * cannot be changed while in use.
 */
static void on_usr2(int sig)
{
	char here;
	stack_t q, n;

	(void)sig;
	g_alt_on = ((uintptr_t)&here >= (uintptr_t)g_altstack &&
	            (uintptr_t)&here < (uintptr_t)g_altstack + sizeof(g_altstack));
	g_alt_flags = (sigaltstack(NULL, &q) == 0) ? q.ss_flags : -1;
	memset(&n, 0, sizeof(n));
	n.ss_sp = g_altstack;
	n.ss_size = sizeof(g_altstack);
	g_alt_eperm = (sigaltstack(&n, NULL) == -1 && errno == EPERM);
}

/**
 * sigaltstack: initial state, error paths, delivery on the alternate stack only
 * with SA_ONSTACK, inheritance across fork, and disabling.
 */
static void check_sigaltstack(void)
{
	struct sigaction sa;
	stack_t ss, o;
	pid_t pid;
	int st;

	memset(&o, 0, sizeof(o));
	check("sigaltstack initially disabled", sigaltstack(NULL, &o) == 0 && (o.ss_flags & SS_DISABLE));
	errno = 0;
	check("sigaltstack bad oss -> EFAULT", sigaltstack(NULL, (stack_t *)8) == -1 && errno == EFAULT);
	memset(&ss, 0, sizeof(ss));
	ss.ss_sp = g_altstack;
	ss.ss_size = 512;
	errno = 0;
	check("sigaltstack tiny -> ENOMEM", sigaltstack(&ss, NULL) == -1 && errno == ENOMEM);

	ss.ss_size = sizeof(g_altstack);
	check("sigaltstack set", sigaltstack(&ss, NULL) == 0);
	memset(&o, 0, sizeof(o));
	check("sigaltstack query",
	      sigaltstack(NULL, &o) == 0 && o.ss_sp == g_altstack && o.ss_size == sizeof(g_altstack) &&
	          o.ss_flags == 0);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_usr2;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR2, &sa, NULL);
	g_alt_on = -1;
	raise(SIGUSR2);
	check("no SA_ONSTACK: normal stack", g_alt_on == 0);

	sa.sa_flags = SA_ONSTACK;
	sigaction(SIGUSR2, &sa, NULL);
	g_alt_on = -1;
	raise(SIGUSR2);
	check("SA_ONSTACK: alternate stack", g_alt_on == 1);
	check("SS_ONSTACK reported in handler", g_alt_flags == SS_ONSTACK);
	check("change while on it -> EPERM", g_alt_eperm == 1);
	memset(&o, 0, sizeof(o));
	check("off the stack after return", sigaltstack(NULL, &o) == 0 && o.ss_flags == 0);

	pid = fork();
	if (pid == 0)
	{
		memset(&o, 0, sizeof(o));
		_exit(sigaltstack(NULL, &o) == 0 && o.ss_sp == g_altstack ? 0 : 1);
	}
	st = -1;
	check("fork inherits sigaltstack", waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);

	memset(&ss, 0, sizeof(ss));
	ss.ss_flags = SS_DISABLE;
	check("sigaltstack disable", sigaltstack(&ss, NULL) == 0);
	memset(&o, 0, sizeof(o));
	check("disabled after disable", sigaltstack(NULL, &o) == 0 && (o.ss_flags & SS_DISABLE));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigaction(SIGUSR2, &sa, NULL);
}

int main(void)
{
	struct sigaction sa, old;
	sigset_t set, cur;
	unsigned char guard[64];
	pid_t pid;
	int st;

	/* SA_SIGINFO handler that must survive repeated delivery: the kernel
	 * once read musl's SA_SIGINFO (4) as its own SA_RESETHAND, so the second
	 * signal killed the process. */
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = on_usr1;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	check("sigaction install", sigaction(SIGUSR1, &sa, NULL) == 0);
	raise(SIGUSR1);
	raise(SIGUSR1);
	check("SA_SIGINFO handler runs twice", g_hits == 2);
	check("siginfo si_signo", g_signo == SIGUSR1);

	memset(&old, 0, sizeof(old));
	check("sigaction query", sigaction(SIGUSR1, NULL, &old) == 0);
	check("queried handler", old.sa_sigaction == on_usr1);
	check("queried SA_SIGINFO", (old.sa_flags & SA_SIGINFO) != 0);

	/* sigprocmask must write exactly musl's 8-byte set; the bytes around the
	 * caller's buffers must survive. */
	memset(guard, 0xA5, sizeof(guard));
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);
	check("block SIGUSR2", sigprocmask(SIG_BLOCK, &set, NULL) == 0);
	check("query mask", sigprocmask(SIG_BLOCK, NULL, &cur) == 0);
	check("SIGUSR2 is blocked", sigismember(&cur, SIGUSR2) == 1);
	check("SIGUSR1 is not blocked", sigismember(&cur, SIGUSR1) == 0);
	{
		int i, ok = 1;
		for (i = 0; i < (int)sizeof(guard); i++)
			if (guard[i] != 0xA5)
				ok = 0;
		check("stack guard intact", ok);
	}
	sigprocmask(SIG_UNBLOCK, &set, NULL);

	/* wait() statuses. */
	pid = fork();
	if (pid == 0)
		_exit(3);
	st = -1;
	check("waitpid exit child", waitpid(pid, &st, 0) == pid);
	check("WIFEXITED && status 3", WIFEXITED(st) && WEXITSTATUS(st) == 3);

	pid = fork();
	if (pid == 0)
	{
		volatile int *bad = (volatile int *)8;
		*bad = 1; /* SIGSEGV */
		_exit(0);
	}
	st = -1;
	check("waitpid faulting child", waitpid(pid, &st, 0) == pid);
	check("WIFSIGNALED && SIGSEGV", WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV);

	check_sigaltstack();

	printf("sigtest: %s\n", g_fail ? "FAILED" : "all passed");
	return g_fail;
}
