/*
 * tee — duplicate standard input to standard output and to files.
 *
 *   tee [-ai] [FILE...]
 *
 * Copies standard input to standard output and to every FILE.  -a appends to
 * the files instead of truncating them; -i ignores SIGINT.  A FILE that cannot
 * be opened is reported and skipped; the exit status is then non-zero.
 * Written from the POSIX/SUS specification.
 *
 * (C) 2002-2026 The UbixOS Project
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Write a whole buffer to a descriptor, retrying on short writes and EINTR.
 *
 * @return 0 on success, -1 on error (errno set).
 */
static int write_all(int fd, const char *buf, size_t len)
{
	while (len > 0)
	{
		ssize_t n = write(fd, buf, len);

		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		buf += n;
		len -= (size_t)n;
	}
	return 0;
}

/**
 * Program entry: parse -a/-i, open the files, and copy stdin to all of them.
 *
 * @return 0 on success, 1 on any open/read/write error or bad usage.
 */
int main(int argc, char **argv)
{
	int append = 0, status = 0, opt, nfd = 0, i;
	int *fds;
	char buf[4096];
	ssize_t n;

	while ((opt = getopt(argc, argv, "ai")) != -1)
	{
		switch (opt)
		{
			case 'a':
				append = 1;
				break;
			case 'i':
				signal(SIGINT, SIG_IGN);
				break;
			default:
				fprintf(stderr, "usage: tee [-ai] [file ...]\n");
				return 1;
		}
	}
	argc -= optind;
	argv += optind;

	fds = malloc((size_t)(argc + 1) * sizeof(*fds));
	if (fds == NULL)
	{
		fprintf(stderr, "tee: out of memory\n");
		return 1;
	}
	for (i = 0; i < argc; i++)
	{
		int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
		int fd = open(argv[i], flags, 0666);

		if (fd < 0)
		{
			fprintf(stderr, "tee: %s: %s\n", argv[i], strerror(errno));
			status = 1;
			continue;
		}
		fds[nfd++] = fd;
	}

	while ((n = read(STDIN_FILENO, buf, sizeof(buf))) != 0)
	{
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			fprintf(stderr, "tee: read: %s\n", strerror(errno));
			status = 1;
			break;
		}
		if (write_all(STDOUT_FILENO, buf, (size_t)n) < 0)
		{
			fprintf(stderr, "tee: stdout: %s\n", strerror(errno));
			status = 1;
		}
		for (i = 0; i < nfd; i++)
		{
			if (fds[i] >= 0 && write_all(fds[i], buf, (size_t)n) < 0)
			{
				fprintf(stderr, "tee: write error: %s\n", strerror(errno));
				close(fds[i]);
				fds[i] = -1;
				status = 1;
			}
		}
	}
	for (i = 0; i < nfd; i++)
		if (fds[i] >= 0)
			close(fds[i]);
	return status;
}
