/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * vfstest — checks VFS edge cases: zero-length read, O_APPEND on every write,
 * ".." in paths, faccessat, and that the VFS lookup cache follows namespace
 * changes.  Prints one PASS/FAIL line per check and exits
 * non-zero if any check failed.
 *
 * "vfstest --long N" and "vfstest --many N" are the re-exec targets of the
 * execve argument checks: they exit 0 if their argv arrived intact.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <stdlib.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>

#define NMAPS 64
#define MAPSZ 8192

static int g_fail;
static char *g_maps[2][NMAPS]; /* check_thread_mmap: each thread's regions */

static void check(const char *what, int ok)
{
	printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		g_fail = 1;
}

/**
 * Re-exec target: verify the argv the parent built in check_execve_args.
 *
 * @return 0 if it arrived intact.
 */
static int argv_target(int argc, char **argv)
{
	int n = atoi(argv[2]), i;
	char want[16];

	if (strcmp(argv[1], "--long") == 0)
		return (argc == 4 && (int)strlen(argv[3]) == n && argv[3][n - 1] == 'Z') ? 0 : 1;
	if (argc != n + 3)
		return (1);
	for (i = 0; i < n; i++)
	{
		snprintf(want, sizeof(want), "a%d", i);
		if (strcmp(argv[3 + i], want) != 0)
			return (1);
	}
	return (0);
}

/**
 * Fork, execve @path with @av in the child, and wait.
 *
 * @return the child's exit status, or 100 + errno if execve itself failed.
 */
static int run_exec(const char *path, char **av)
{
	char *ev[] = {"VFSTEST=1", NULL};
	pid_t pid;
	int st = -1;

	pid = fork();
	if (pid == 0)
	{
		execve(path, av, ev);
		_exit(100 + errno);
	}
	waitpid(pid, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/** SIGUSR1 handler for the poll-interrupt check: only has to exist. */
static void on_usr1(int sig)
{
	(void)sig;
}

/**
 * poll/select on pipes: readiness from data and EOF, timeouts, the ready-fd
 * count, and EINTR from a caught signal (bmake -j depends on all of these).
 */
static void check_poll_pipes(void)
{
	struct pollfd pf[2];
	struct sigaction sa;
	pid_t pid;
	fd_set rs;
	struct timeval tv;
	int p[2], q[2], r;

	if (pipe(p) != 0 || pipe(q) != 0)
	{
		check("poll test pipes", 0);
		return;
	}
	pf[0].fd = p[0];
	pf[0].events = POLLIN;
	check("poll empty pipe times out", poll(pf, 1, 50) == 0 && pf[0].revents == 0);

	write(p[1], "x", 1);
	check("poll pipe with data -> POLLIN", poll(pf, 1, 1000) == 1 && (pf[0].revents & POLLIN));

	pf[1].fd = p[1];
	pf[1].events = POLLOUT;
	check("poll counts ready fds", poll(pf, 2, 1000) == 2 && (pf[1].revents & POLLOUT));

	close(q[1]);
	pf[0].fd = q[0];
	check("poll pipe at EOF -> POLLHUP", poll(pf, 1, 1000) == 1 && (pf[0].revents & POLLHUP));

	FD_ZERO(&rs);
	FD_SET(q[0], &rs);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	check("select pipe at EOF", select(q[0] + 1, &rs, NULL, NULL, &tv) == 1 && FD_ISSET(q[0], &rs));

	/* A caught signal ends a wait with EINTR: a child signals us. */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_usr1;
	sigaction(SIGUSR1, &sa, NULL);
	read(p[0], &r, 1); /* drain: the pipe is empty again */
	pf[0].fd = p[0];
	pid = fork();
	if (pid == 0)
	{
		usleep(200000);
		kill(getppid(), SIGUSR1);
		_exit(0);
	}
	errno = 0;
	r = poll(pf, 1, 5000);
	check("poll interrupted by a signal -> EINTR", r == -1 && errno == EINTR);
	waitpid(pid, NULL, 0);
	sa.sa_handler = SIG_DFL;
	sigaction(SIGUSR1, &sa, NULL);

	close(p[0]);
	close(p[1]);
	close(q[0]);
}

/**
 * mmap NMAPS regions for slot @arg and fill each with the slot's pattern byte.
 * @return NULL.
 */
static void *mapper(void *arg)
{
	int slot = (int)(long)arg;
	int i;

	for (i = 0; i < NMAPS; i++)
	{
		char *m = mmap(NULL, MAPSZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

		g_maps[slot][i] = (m == MAP_FAILED) ? NULL : m;
		if (m != MAP_FAILED)
			memset(m, 'A' + slot, MAPSZ);
	}
	return NULL;
}

/**
 * Two threads mmap concurrently; every region must be distinct and intact.
 * @return 1 on success.
 */
static int check_thread_mmap(void)
{
	pthread_t t;
	int s, i, j, k;

	if (pthread_create(&t, NULL, mapper, (void *)1L) != 0)
		return 0;
	mapper((void *)0L);
	pthread_join(t, NULL);

	for (s = 0; s < 2; s++)
		for (i = 0; i < NMAPS; i++)
		{
			if (g_maps[s][i] == NULL)
				return 0;
			for (k = 0; k < MAPSZ; k += 512)
				if (g_maps[s][i][k] != 'A' + s)
					return 0;
			for (j = 0; j < NMAPS; j++) /* no overlap with the other thread's maps */
			{
				char *o = g_maps[1 - s][j];
				if (o != NULL && o < g_maps[s][i] + MAPSZ && g_maps[s][i] < o + MAPSZ)
					return 0;
			}
		}
	return 1;
}

/**
 * Open /etc/passwd in a thread and return its fd.
 * @return the fd, cast to a pointer.
 */
static void *opener(void *arg)
{
	(void)arg;
	return (void *)(long)open("/etc/passwd", O_RDONLY);
}

/**
 * Threads share one fd table: an fd a thread opens is usable in the main
 * thread, and a file the main thread closes is closed for the thread too.
 * Run in a child so the child's exit (last thread closing the table) is
 * checked as well; each thread used to have its own copy of the table, and
 * that exit freed the closed file a second time (kernel double-free).
 * @return 1 on success.
 */
static int check_thread_fds(void)
{
	pid_t pid;
	int st = -1;

	pid = fork();
	if (pid == 0)
	{
		pthread_t t;
		void *r = NULL;
		char c;
		int fd, ok;

		fd = open("/etc/passwd", O_RDONLY);
		if (fd < 0 || pthread_create(&t, NULL, opener, NULL) != 0)
			_exit(2);
		pthread_join(t, &r);
		close(fd);
		ok = (long)r >= 0 && read((int)(long)r, &c, 1) == 1;
		_exit(ok ? 0 : 1);
	}
	return pid > 0 && waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/** @return MemFree from /proc/meminfo in kB, or -1 if it can't be read. */
static long mem_free_kb(void)
{
	char line[128];
	long kb = -1;
	FILE *f = fopen("/proc/meminfo", "r");

	if (f == NULL)
		return -1;
	while (fgets(line, sizeof(line), f) != NULL)
	{
		if (sscanf(line, "MemFree: %ld", &kb) == 1)
			break;
	}
	fclose(f);
	return kb;
}

/**
 * mmap(MAP_FIXED) over a page that is already mapped replaces it without
 * leaking the old frame: 2000 remaps of one page must not use up ~8 MB.
 */
static void check_mmap_fixed_reuse(void)
{
	enum
	{
		REMAPS = 2000
	};
	long before, after;
	char *p;
	int i;

	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p == MAP_FAILED)
	{
		check("mmap a page to remap", 0);
		return;
	}
	before = mem_free_kb();
	for (i = 0; i < REMAPS; i++)
	{
		if (mmap(p, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) != p)
			break;
		p[0] = 1;
	}
	after = mem_free_kb();
	check("mmap MAP_FIXED over a mapped page succeeds", i == REMAPS);
	check("mmap MAP_FIXED over a mapped page frees the old frame",
	      before > 0 && after > 0 && before - after < 2048);
	munmap(p, 4096);
}

/**
 * File modes: open(O_CREAT) gives a new file the requested mode less the umask,
 * chmod and fchmod change it, rename keeps it, and O_CREAT|O_EXCL refuses an
 * existing file.
 */
static void check_modes(void)
{
	const char *p = "/tmp/vfstest.mode";
	struct stat st;
	int fd;

	unlink(p);
	fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	check("open O_CREAT 0755", fd >= 0);
	check("new file gets mode 0755", fstat(fd, &st) == 0 && (st.st_mode & 07777) == 0755);
	check("fchmod 0600", fchmod(fd, 0600) == 0 && fstat(fd, &st) == 0 && (st.st_mode & 07777) == 0600);
	close(fd);
	check("fchmod mode persists", stat(p, &st) == 0 && (st.st_mode & 07777) == 0600);
	check("chmod 0751", chmod(p, 0751) == 0 && stat(p, &st) == 0 && (st.st_mode & 07777) == 0751);
	check("chmod keeps the file type", S_ISREG(st.st_mode));
	errno = 0;
	fd = open(p, O_WRONLY | O_CREAT | O_EXCL, 0644);
	check("O_CREAT|O_EXCL on an existing file -> EEXIST", fd < 0 && errno == EEXIST);
	if (fd >= 0)
		close(fd);
	/* rename moves the file, mode and contents, over an existing target. */
	fd = open("/tmp/vfstest.mode2", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0)
	{
		write(fd, "old", 3);
		close(fd);
	}
	fd = open(p, O_WRONLY | O_APPEND);
	if (fd >= 0)
	{
		write(fd, "new!", 4);
		close(fd);
	}
	check("rename over an existing file", rename(p, "/tmp/vfstest.mode2") == 0);
	check("rename removes the source", access(p, F_OK) < 0);
	check("rename keeps the mode",
	      stat("/tmp/vfstest.mode2", &st) == 0 && (st.st_mode & 07777) == 0751 && st.st_size == 4);
	unlink("/tmp/vfstest.mode2");
	errno = 0;
	check("chmod missing file -> ENOENT", chmod(p, 0644) < 0 && errno == ENOENT);
}

/**
 * umask is per process: it returns the old mask, open(O_CREAT) clears its bits,
 * and a forked child inherits it without changing the parent's.
 */
static void check_umask(void)
{
	const char *p = "/tmp/vfstest.umask";
	struct stat st;
	mode_t old;
	pid_t pid;
	int fd;
	int status = 0;

	old = umask(077);
	check("umask returns the old mask", umask(077) == 077);
	unlink(p);
	fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	check("umask 077 makes a new 0666 file 0600", fd >= 0 && fstat(fd, &st) == 0 && (st.st_mode & 07777) == 0600);
	if (fd >= 0)
		close(fd);
	unlink(p);
	pid = fork();
	if (pid == 0)
		_exit(umask(0) == 077 ? 0 : 1);
	check("fork child inherits the umask",
	      pid > 0 && waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
	check("child's umask change leaves the parent's", umask(old) == 077);
}

/**
 * A newly written file is stamped with the current wall-clock time, not the
 * time the system booted.
 */
static void check_mtime_now(void)
{
	const char *p = "/tmp/vfstest.mtime";
	struct stat st;
	time_t now;
	int fd;

	unlink(p);
	fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0)
	{
		write(fd, "t", 1);
		close(fd);
	}
	now = time(NULL);
	check("new file mtime is the current time",
	      stat(p, &st) == 0 && st.st_mtime <= now + 2 && st.st_mtime >= now - 2);
	unlink(p);
}

/**
 * execve passes long and numerous arguments whole, and refuses an oversized
 * or unreadable argv with an error instead of truncating it.
 */
static void check_execve_args(const char *self)
{
	enum
	{
		LONG_ARG = 20000,
		MANY_ARGS = 1000,
		TOO_BIG = 200 * 1024
	};
	char **av = malloc((MANY_ARGS + 4) * sizeof(char *));
	char *big = malloc(TOO_BIG + 1);
	char num[16];
	int i;

	if (av == NULL || big == NULL)
	{
		check("execve test buffers", 0);
		return;
	}
	memset(big, 'x', TOO_BIG);

	big[LONG_ARG - 1] = 'Z';
	big[LONG_ARG] = '\0';
	snprintf(num, sizeof(num), "%d", LONG_ARG);
	av[0] = (char *)self;
	av[1] = "--long";
	av[2] = num;
	av[3] = big;
	av[4] = NULL;
	check("execve passes a 20000-byte argument whole", run_exec(self, av) == 0);

	snprintf(num, sizeof(num), "%d", MANY_ARGS);
	av[1] = "--many";
	for (i = 0; i < MANY_ARGS; i++)
	{
		av[3 + i] = malloc(16);
		snprintf(av[3 + i], 16, "a%d", i);
	}
	av[3 + MANY_ARGS] = NULL;
	check("execve passes 1000 arguments", run_exec(self, av) == 0);
	for (i = 0; i < MANY_ARGS; i++)
		free(av[3 + i]);

	memset(big, 'x', TOO_BIG);
	big[TOO_BIG] = '\0';
	av[1] = big;
	av[2] = NULL;
	check("execve over ARG_MAX -> E2BIG", run_exec(self, av) == 100 + E2BIG);
	check("execve bad argv -> EFAULT", run_exec(self, (char **)8) == 100 + EFAULT);

	free(big);
	free(av);
}

/**
 * Run every check, or act as the execve re-exec target (see argv_target).
 *
 * @return 0 if every check passed.
 */
/**
 * errno after a readlink() of @path, or 0 if it succeeded (it was a symlink).
 */
static int readlink_errno(const char *path)
{
	char t[64];

	errno = 0;
	return (readlink(path, t, sizeof(t)) >= 0 ? 0 : errno);
}

/**
 * Nanoseconds on the monotonic clock.
 */
static long long now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((long long)ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

/**
 * The VFS lookup cache (W-039) remembers "nothing here" and "here, not a
 * symlink" per path.  Every namespace change must make those answers current:
 * create, unlink, symlink (including a dangling one), mkdir, rmdir and a
 * directory rename.
 */
static void check_lookup_cache(void)
{
	char dir[64], p[96], q[96], sub[96], sub2[96], x[128], x2[128], t[64];
	struct stat st;
	int fd;

	snprintf(dir, sizeof(dir), "/tmp/vnc.%d", (int)getpid());
	mkdir(dir, 0755);
	snprintf(p, sizeof(p), "%s/f", dir);
	snprintf(q, sizeof(q), "%s/target", dir);

	/* Missing: ENOENT from every probe, asked twice (the second is cached). */
	check("lookup cache: readlink missing is ENOENT", readlink_errno(p) == ENOENT);
	check("lookup cache: readlink missing again is ENOENT", readlink_errno(p) == ENOENT);
	check("lookup cache: stat missing fails", stat(p, &st) != 0 && errno == ENOENT);
	check("lookup cache: access missing fails", access(p, F_OK) != 0);
	check("lookup cache: open missing fails", open(p, O_RDONLY) < 0);

	/* Create: the "missing" answer must go. */
	fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	check("lookup cache: create", fd >= 0);
	if (fd >= 0)
		close(fd);
	check("lookup cache: stat after create", stat(p, &st) == 0);
	check("lookup cache: access after create", access(p, F_OK) == 0);
	fd = open(p, O_RDONLY);
	check("lookup cache: open after create", fd >= 0);
	if (fd >= 0)
		close(fd);
	check("lookup cache: readlink regular file is EINVAL", readlink_errno(p) == EINVAL);
	check("lookup cache: readlink regular file again is EINVAL", readlink_errno(p) == EINVAL);

	/* Unlink: the "exists, not a link" answer must go. */
	check("lookup cache: unlink", unlink(p) == 0);
	check("lookup cache: readlink after unlink is ENOENT", readlink_errno(p) == ENOENT);
	check("lookup cache: stat after unlink fails", stat(p, &st) != 0);
	check("lookup cache: access after unlink fails", access(p, F_OK) != 0);

	/* A dangling symlink is not "missing": stat fails, readlink still reads it,
	 * and creating its target makes it resolve. */
	check("lookup cache: symlink over a removed name", symlink("target", p) == 0);
	check("lookup cache: stat of dangling link fails", stat(p, &st) != 0);
	check("lookup cache: access of dangling link fails", access(p, F_OK) != 0);
	memset(t, 0, sizeof(t));
	check("lookup cache: readlink of dangling link",
	      readlink(p, t, sizeof(t) - 1) == 6 && strcmp(t, "target") == 0);
	fd = open(q, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0)
		close(fd);
	check("lookup cache: access through link after target created", access(p, F_OK) == 0);
	check("lookup cache: stat through link after target created", stat(p, &st) == 0);
	unlink(p);
	unlink(q);

	/* mkdir of a name known to be missing; rmdir of one known to exist. */
	snprintf(sub, sizeof(sub), "%s/sub", dir);
	check("lookup cache: readlink missing dir is ENOENT", readlink_errno(sub) == ENOENT);
	check("lookup cache: mkdir", mkdir(sub, 0755) == 0);
	check("lookup cache: readlink new dir is EINVAL", readlink_errno(sub) == EINVAL);
	check("lookup cache: stat new dir is a directory", stat(sub, &st) == 0 && S_ISDIR(st.st_mode));

	/* Rename a directory: names below the old path vanish, the new ones exist. */
	snprintf(x, sizeof(x), "%s/x", sub);
	snprintf(sub2, sizeof(sub2), "%s/sub2", dir);
	snprintf(x2, sizeof(x2), "%s/x", sub2);
	fd = open(x, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0)
		close(fd);
	check("lookup cache: readlink file in dir is EINVAL", readlink_errno(x) == EINVAL);
	check("lookup cache: readlink in renamed-to dir is ENOENT before", readlink_errno(x2) == ENOENT);
	check("lookup cache: rename dir", rename(sub, sub2) == 0);
	check("lookup cache: old path gone after rename", readlink_errno(x) == ENOENT && stat(x, &st) != 0);
	check("lookup cache: new path there after rename", readlink_errno(x2) == EINVAL && stat(x2, &st) == 0);
	unlink(x2);
	check("lookup cache: rmdir", rmdir(sub2) == 0);
	check("lookup cache: readlink after rmdir is ENOENT", readlink_errno(sub2) == ENOENT);
	check("lookup cache: opendir after rmdir fails", opendir(sub2) == NULL);
	rmdir(dir);
}

/**
 * readlink() of an existing file that is not a symlink (what realpath() does for
 * every component) costs about as much as a trivial syscall, not a filesystem
 * walk.  Before the lookup cache each call made up to three full path lookups
 * under the global VFS lock, about 20,000 times the cost of getppid(); with it
 * the ratio is about 13 on both arches.  Compares the faster of five rounds of
 * each, so a busy host affects both sides.
 */
static void check_readlink_cost(void)
{
	const char *path = "/usr/share/mk/sys.mk";
	long long best_rl = -1, best_pp = -1;
	int round, i, ok = 1;

	if (access(path, F_OK) != 0)
		path = "/etc/passwd";
	for (round = 0; round < 5; round++)
	{
		long long t0 = now_ns();
		for (i = 0; i < 200; i++)
			if (readlink_errno(path) != EINVAL)
				ok = 0;
		long long t1 = now_ns();
		for (i = 0; i < 200; i++)
			(void)getppid();
		long long t2 = now_ns();
		if (best_rl < 0 || t1 - t0 < best_rl)
			best_rl = t1 - t0;
		if (best_pp < 0 || t2 - t1 < best_pp)
			best_pp = t2 - t1;
	}
	if (best_pp <= 0)
		best_pp = 1;
	printf("readlink(%s) x200: %lld us, getppid() x200: %lld us, ratio %lld\n",
	       path,
	       best_rl / 1000,
	       best_pp / 1000,
	       best_rl / best_pp);
	check("readlink of a non-link answers EINVAL", ok);
	check("readlink of a non-link costs under 100 getppid() calls", best_rl < 100 * best_pp);
}

int main(int argc, char **argv)
{
	char buf[64];
	char cwd[256];
	int fd, a, b;
	ssize_t n;

	if (argc >= 3 && (strcmp(argv[1], "--long") == 0 || strcmp(argv[1], "--many") == 0))
		return (argv_target(argc, argv));
	/* Started by check_execve_args without a target mode: an execve that should
	 * have failed ran.  Report it rather than rerunning every check (and
	 * re-execing again, forever). */
	if (getenv("VFSTEST") != NULL)
	{
		printf("FAIL: unexpected re-exec, argc %d argv[1] %.16s\n", argc, argc > 1 ? argv[1] : "(none)");
		fflush(stdout);
		return (99);
	}

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
	check("getcwd is /usr",
	      getcwd(cwd, sizeof(cwd)) != NULL && (strcmp(cwd, "/usr") == 0 || strcmp(cwd, "/usr/") == 0));
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

	/* Calls whose musl numbers used to be Linux's and landed on unrelated
	 * FreeBSD slots (pwrite64 181 was setgid, fadvise64_64 272 getdents,
	 * times 43 getegid; getppid's slot pointed at getpid). */
	{
		gid_t g0 = getgid();
		int p = open("/tmp/vfstest.pw", O_RDWR | O_CREAT | O_TRUNC, 0644);
		pid_t me = getpid(), pid;
		struct tms t;
		int st;

		check("open pwrite file", p >= 0);
		check("write base", write(p, "aaaaaaaa", 8) == 8);
		check("pwrite at offset 2", pwrite(p, "XY", 2, 2) == 2);
		check("pwrite leaves the offset", lseek(p, 0, SEEK_CUR) == 8);
		check("pwrite leaves the gid", getgid() == g0);
		memset(buf, 0, sizeof(buf));
		check("pwrite data in place", pread(p, buf, 8, 0) == 8 && strcmp(buf, "aaXYaaaa") == 0);
		check("posix_fadvise ok", posix_fadvise(p, 0, 0, POSIX_FADV_SEQUENTIAL) == 0);
		check("posix_fadvise bad fd", posix_fadvise(-1, 0, 0, POSIX_FADV_NORMAL) == EBADF);
		close(p);
		unlink("/tmp/vfstest.pw");

		pid = fork();
		if (pid == 0)
			_exit(getppid() == me ? 0 : 1);
		st = -1;
		check("getppid is the parent", waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
		check("times() returns uptime ticks", times(&t) != (clock_t)-1 && times(&t) > 0);
		check("times() leaves the gid", getgid() == g0);
	}

	/* statvfs: musl used Linux's (path, size, buf) form, so the kernel wrote
	 * its struct to address 0x78. */
	{
		struct statvfs v;
		int f;

		memset(&v, 0, sizeof(v));
		check("statvfs / (directory)", statvfs("/", &v) == 0 && v.f_bsize == 4096 && v.f_namemax == 255);
		check("statvfs file", statvfs("/etc/passwd", &v) == 0 && v.f_blocks > 0);
		errno = 0;
		check("statvfs missing -> ENOENT", statvfs("/etc/nope-nope", &v) == -1 && errno == ENOENT);
		f = open("/etc/passwd", O_RDONLY);
		memset(&v, 0, sizeof(v));
		check("fstatvfs", f >= 0 && fstatvfs(f, &v) == 0 && v.f_bsize == 4096);
		close(f);
	}

	/* A directory fd inherited across fork and dup shares one kernel handle;
	 * the exiting child and the parent's close each freed it (kfree
	 * double-free panic during the in-OS world build). */
	{
		DIR *dp;
		int d, dd, st, ok = 0;
		pid_t pid;

		d = open("/etc", O_RDONLY | O_DIRECTORY);
		dd = dup(d);
		pid = fork();
		if (pid == 0)
			_exit(0);
		waitpid(pid, &st, 0);
		close(dd);
		dp = d >= 0 ? fdopendir(d) : NULL;
		if (dp != NULL)
		{
			ok = readdir(dp) != NULL;
			closedir(dp);
		}
		check("directory fd shared with an exited child and a dup", ok);
	}

	check_modes();
	check_umask();
	check_mtime_now();
	check_lookup_cache();
	check_readlink_cost();
	check_mmap_fixed_reuse();
	check_poll_pipes();
	check("two threads' mmaps are distinct and intact", check_thread_mmap());
	check("threads share one fd table", check_thread_fds());
	check_execve_args(argv[0][0] == '/' ? argv[0] : "/usr/tests/vfstest");

	printf("vfstest: %s\n", g_fail ? "FAILED" : "all passed");
	return g_fail;
}
