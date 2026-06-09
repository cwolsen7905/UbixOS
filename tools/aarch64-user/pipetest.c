/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pipetest — aarch64 bring-up smoke test for pipe(2)/fork(2).  Creates a pipe,
 * forks a child that writes a token into the write end, and verifies the parent
 * reads it back from the read end.  Prints PASS/FAIL to stdout (the console).
 * Built static against the aarch64 musl, run by the kernel ELF loader (the
 * embedded boot path) or from a shell once on disk.
 */
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define TOKEN "PIPE_OK"
#define TOKEN_LEN 7

/**
 * Write a NUL-terminated string to stdout (the console).
 */
static void out(const char *s)
{
	write(1, s, strlen(s));
}

/**
 * Run one pipe+fork round-trip over a ready pair of pipe fds @pfd.
 *
 * @return 0 on PASS, 1 on FAIL (message already printed).
 */
static int roundtrip(int pfd[2], const char *label)
{
	pid_t pid;
	char buf[16];
	int n = 0, r, st;

	pid = fork();
	if (pid == 0)
	{
		close(pfd[0]);
		write(pfd[1], TOKEN, TOKEN_LEN);
		close(pfd[1]);
		_exit(0);
	}
	if (pid < 0)
	{
		out("pipetest: FAIL — fork() failed\n");
		return (1);
	}

	close(pfd[1]);
	while (n < TOKEN_LEN && (r = (int)read(pfd[0], buf + n, (size_t)(TOKEN_LEN - n))) > 0)
		n += r;
	buf[n] = '\0';
	close(pfd[0]);
	waitpid(pid, &st, 0);

	if (n == TOKEN_LEN && strcmp(buf, TOKEN) == 0)
	{
		out("pipetest: PASS — ");
		out(label);
		out("\n");
		return (0);
	}
	out("pipetest: FAIL — ");
	out(label);
	out(" round-trip wrong: got '");
	out(buf);
	out("'\n");
	return (1);
}

int main(void)
{
	int pfd[2];
	int rc = 0;

	if (pipe(pfd) != 0)
	{
		out("pipetest: FAIL — pipe() returned nonzero\n");
		return (1);
	}
	rc |= roundtrip(pfd, "pipe(2) round-trip");

	if (pipe2(pfd, 0) != 0)
	{
		out("pipetest: FAIL — pipe2() returned nonzero\n");
		return (1);
	}
	rc |= roundtrip(pfd, "pipe2(2) round-trip");

	return (rc);
}

