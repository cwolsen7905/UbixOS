/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * pipetest — deterministic, non-interactive pipe(2) exercise.
 *
 * Verifies the FD_TYPE_PIPE read/write/close path end-to-end: create a pipe,
 * fork, the child writes a known payload into the write end and exits, the
 * parent drains the read end and checks the bytes + the EOF (0-byte read after
 * the writer closes).  Prints a single PASS/FAIL line so it can be scored from
 * the serial log of a headless QEMU run — the coverage the boot smoke-test
 * lacks for the fileops refactor (path B).
 *
 * Run: pipetest
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static const char payload[] = "uBixOS-pipe-payload-0123456789";

int main(void)
{
	int fds[2];
	pid_t pid;

	printf("pipetest: start\n");

	if (pipe(fds) != 0)
	{
		printf("pipetest: FAIL — pipe() failed\n");
		return 1;
	}

	pid = fork();
	if (pid < 0)
	{
		printf("pipetest: FAIL — fork() failed\n");
		return 1;
	}

	if (pid == 0)
	{
		/* Child: write the payload, then close + exit so the parent sees EOF. */
		close(fds[0]);
		ssize_t w = write(fds[1], payload, sizeof(payload) - 1);
		close(fds[1]);
		_exit(w == (ssize_t)(sizeof(payload) - 1) ? 0 : 1);
	}

	/* Parent: drain the read end and compare. */
	close(fds[1]);

	char buf[64];
	size_t got = 0;
	for (;;)
	{
		ssize_t r = read(fds[0], buf + got, sizeof(buf) - 1 - got);
		if (r < 0)
		{
			printf("pipetest: FAIL — read() returned %ld\n", (long)r);
			close(fds[0]);
			return 1;
		}
		if (r == 0)
			break; /* EOF: writer closed */
		got += (size_t)r;
		if (got >= sizeof(buf) - 1)
			break;
	}
	buf[got] = '\0';
	close(fds[0]);
	waitpid(pid, NULL, 0);

	if (got == sizeof(payload) - 1 && memcmp(buf, payload, got) == 0)
	{
		printf("pipetest: PASS — %zu bytes round-tripped through the pipe\n", got);
		return 0;
	}

	printf("pipetest: FAIL — got %zu bytes: \"%s\"\n", got, buf);
	return 1;
}
