/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * vfstest — checks VFS edge cases: zero-length read, O_APPEND on every write,
 * ".." in paths and faccessat.  Prints one PASS/FAIL line per check and exits
 * non-zero if any check failed.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

static void check(const char *what, int ok)
{
	printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		g_fail = 1;
}

int main(void)
{
	char buf[64];
	char cwd[256];
	int fd, a, b;
	ssize_t n;

	/* read(fd, buf, 0) reads nothing and leaves the buffer alone. */
	fd = open("/etc/passwd", O_RDONLY);
	check("open /etc/passwd", fd >= 0);
	memset(buf, 'Z', sizeof(buf));
	n = read(fd, buf, 0);
	check("read of 0 bytes returns 0", n == 0);
	check("read of 0 bytes leaves buffer", buf[0] == 'Z');
	check("offset unchanged after 0-byte read", lseek(fd, 0, SEEK_CUR) == 0);
	close(fd);

	/* O_APPEND writes land at the end, including after a reopen.  (Two
	 * O_APPEND descriptors open at once each cache the size at open, so
	 * they do not yet see each other's writes — a known VFS limitation.) */
	unlink("/tmp/vfstest.app");
	a = open("/tmp/vfstest.app", O_WRONLY | O_CREAT | O_APPEND, 0644);
	check("open O_APPEND", a >= 0);
	write(a, "one\n", 4);
	write(a, "two\n", 4);
	close(a);
	b = open("/tmp/vfstest.app", O_WRONLY | O_APPEND);
	write(b, "three\n", 6);
	close(b);
	fd = open("/tmp/vfstest.app", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	check("O_APPEND appends across reopen", n == 14 && strcmp(buf, "one\ntwo\nthree\n") == 0);

	/* ".." folds against the previous component. */
	check("chdir /usr/bin", chdir("/usr/bin") == 0);
	check("chdir ..", chdir("..") == 0);
	check("getcwd is /usr", getcwd(cwd, sizeof(cwd)) != NULL && (strcmp(cwd, "/usr") == 0 || strcmp(cwd, "/usr/") == 0));
	fd = open("../etc/passwd", O_RDONLY);
	check("open ../etc/passwd from /usr", fd >= 0);
	if (fd >= 0)
		close(fd);
	fd = open("/usr/bin/../../etc/./passwd", O_RDONLY);
	check("open /usr/bin/../../etc/./passwd", fd >= 0);
	if (fd >= 0)
		close(fd);
	chdir("/");

	/* faccessat with AT_FDCWD. */
	check("faccessat existing", faccessat(AT_FDCWD, "/etc/passwd", F_OK, 0) == 0);
	check("faccessat missing", faccessat(AT_FDCWD, "/etc/nope-nope", F_OK, 0) != 0);
	check("access existing", access("/etc/passwd", F_OK) == 0);

	printf("vfstest: %s\n", g_fail ? "FAILED" : "all passed");
	return g_fail;
}
