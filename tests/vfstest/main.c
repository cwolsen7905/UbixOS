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
#include <sys/wait.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/stat.h>

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

	/* "#!" scripts: the kernel runs the interpreter with its argument, the
	 * script path and the original arguments. */
	{
		int p[2], st = -1;
		pid_t pid;

		unlink("/tmp/vfstest.sh");
		fd = open("/tmp/vfstest.sh", O_WRONLY | O_CREAT | O_TRUNC, 0755);
		write(fd, "#!/bin/echo hello\n", 18);
		close(fd);
		pipe(p);
		pid = fork();
		if (pid == 0)
		{
			char *av[] = {"/tmp/vfstest.sh", "arg1", NULL};
			char *ev[] = {NULL};
			dup2(p[1], 1);
			close(p[0]);
			close(p[1]);
			execve("/tmp/vfstest.sh", av, ev);
			_exit(127);
		}
		close(p[1]);
		memset(buf, 0, sizeof(buf));
		n = read(p[0], buf, sizeof(buf) - 1);
		close(p[0]);
		waitpid(pid, &st, 0);
		check("#! script runs interpreter", strcmp(buf, "hello /tmp/vfstest.sh arg1\n") == 0);
		check("#! script exit status 0", WIFEXITED(st) && WEXITSTATUS(st) == 0);
	}

	/* MAP_SHARED + PROT_WRITE file mappings reach the file on munmap, msync and
	 * exit (ld.lld writes its output this way). */
	{
		static const char *how[3] = {"munmap", "msync", "exit"};
		int k;

		for (k = 0; k < 3; k++)
		{
			char path[64], got[16];
			char *m;
			pid_t pid;
			int st;

			snprintf(path, sizeof(path), "/tmp/vfstest.map%d", k);
			unlink(path);
			fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
			memset(buf, '.', sizeof(buf));
			for (n = 0; n < 128; n++) /* 8 KB of '.' */
				write(fd, buf, sizeof(buf));
			pid = (k == 2) ? fork() : 0;
			if (pid == 0)
			{
				m = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				if (m != MAP_FAILED)
				{
					memcpy(m + 5000, "mapped!", 7);
					if (k == 0)
						munmap(m, 8192);
					else if (k == 1)
						msync(m, 8192, MS_SYNC);
				}
				if (k == 2)
					_exit(0); /* no munmap: exit must flush */
			}
			else
			{
				waitpid(pid, &st, 0);
			}
			close(fd);
			fd = open(path, O_RDONLY);
			lseek(fd, 5000, SEEK_SET);
			memset(got, 0, sizeof(got));
			read(fd, got, 7);
			close(fd);
			snprintf(buf, sizeof(buf), "MAP_SHARED write-back on %s", how[k]);
			check(buf, memcmp(got, "mapped!", 7) == 0);
		}
	}

	/* readdir returns every entry of a directory bigger than one getdents
	 * buffer (an entry at the buffer boundary used to be dropped). */
	{
		DIR *dp;
		struct dirent *de;
		int i, seen = 0;
		char path[64];

		mkdir("/tmp/vfstest.dir", 0755);
		for (i = 0; i < 300; i++)
		{
			snprintf(path, sizeof(path), "/tmp/vfstest.dir/entry-with-a-longish-name-%03d.c", i);
			fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd >= 0)
				close(fd);
		}
		dp = opendir("/tmp/vfstest.dir");
		while (dp != NULL && (de = readdir(dp)) != NULL)
			if (de->d_name[0] != '.')
				seen++;
		if (dp != NULL)
			closedir(dp);
		printf("  readdir saw %d of 300\n", seen);
		check("readdir returns every entry of a large directory", seen == 300);
	}

	/* An unlinked file stays usable through an open fd until the last close
	 * (mkstemp + unlink: bmake -j job scripts, compiler temporaries). */
	{
		int u = open("/tmp/vfstest.unl", O_RDWR | O_CREAT | O_TRUNC, 0644);
		check("open scratch file", u >= 0);
		check("unlink while open", unlink("/tmp/vfstest.unl") == 0);
		check("gone from the directory", access("/tmp/vfstest.unl", F_OK) != 0);
		check("write after unlink", write(u, "still here", 10) == 10);
		lseek(u, 0, SEEK_SET);
		memset(buf, 0, sizeof(buf));
		check("read after unlink", read(u, buf, 10) == 10 && strcmp(buf, "still here") == 0);
		close(u);
	}

	printf("vfstest: %s\n", g_fail ? "FAILED" : "all passed");
	return g_fail;
}
