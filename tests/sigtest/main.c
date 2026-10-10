/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * sigtest — checks the signal ABI between musl and the kernel, and wait()
 * exit statuses.  Prints one PASS/FAIL line per check and exits non-zero
 * if any check failed.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static volatile int g_hits;
static volatile int g_signo;

static int g_fail;

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

	printf("sigtest: %s\n", g_fail ? "FAILED" : "all passed");
	return g_fail;
}
