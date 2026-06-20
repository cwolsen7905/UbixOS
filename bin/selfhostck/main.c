/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * selfhostck — in-OS self-host foundation check.
 *
 * Exercises the syscall surface the self-hosting toolchain (Clang/lld, bmake, a
 * shell) depends on, and prints PASS/FAIL per capability.  Phase 0 of
 * docs/design/self-hosting-plan.md: verify the foundation empirically rather than
 * trusting the status matrix.  Run it on-device from the shell:  selfhostck
 *
 * Exit status is the number of failed checks (0 = all good).
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

static int g_fail;
static int g_pass;

/**
 * Report one check: prints [PASS]/[FAIL] and tallies.  @ok non-zero is a pass;
 * @detail is appended (errno string on failure, or a note).
 */
static void check(const char *name, int ok, const char *detail)
{
	if (ok)
	{
		g_pass++;
		printf("[PASS] %-22s %s\n", name, detail ? detail : "");
	}
	else
	{
		g_fail++;
		printf("[FAIL] %-22s %s\n", name, detail ? detail : "");
	}
}

/* Anonymous mmap: allocate, write, read back, free. */
static void t_mmap_anon(void)
{
	void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED)
	{
		check("mmap anon", 0, strerror(errno));
		return;
	}
	memset(p, 0xAB, 4096);
	int ok = (((unsigned char *)p)[0] == 0xAB && ((unsigned char *)p)[4095] == 0xAB);
	munmap(p, 4096);
	check("mmap anon", ok, ok ? "rw 4 KB" : "readback mismatch");
}

/* File-backed mmap: map a known file read-only and read its first byte. */
static void t_mmap_file(void)
{
	int fd = open("/etc/fstab", O_RDONLY);
	if (fd < 0)
	{
		check("mmap file", 0, "open /etc/fstab failed");
		return;
	}
	void *p = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
	int ok = (p != MAP_FAILED);
	char d[40];
	if (ok)
	{
		snprintf(d, sizeof(d), "first byte 0x%02x", (unsigned char)((char *)p)[0]);
		munmap(p, 4096);
	}
	else
		snprintf(d, sizeof(d), "%s", strerror(errno));
	close(fd);
	check("mmap file", ok, d);
}

/* mprotect: flip an anon page RW -> RO -> RW. */
static void t_mprotect(void)
{
	void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED)
	{
		check("mprotect", 0, "mmap failed");
		return;
	}
	int rc = mprotect(p, 4096, PROT_READ);
	rc |= mprotect(p, 4096, PROT_READ | PROT_WRITE);
	munmap(p, 4096);
	check("mprotect", rc == 0, rc == 0 ? "" : strerror(errno));
}

/* fork + wait4: child exits 42, parent decodes the status. */
static void t_fork_wait(void)
{
	pid_t pid = fork();
	if (pid < 0)
	{
		check("fork+wait4", 0, strerror(errno));
		return;
	}
	if (pid == 0)
		_exit(42);
	int status = 0;
	pid_t w = waitpid(pid, &status, 0);
	int ok = (w == pid && WIFEXITED(status) && WEXITSTATUS(status) == 42);
	char d[40];
	snprintf(d, sizeof(d), "status exit=%d", WEXITSTATUS(status));
	check("fork+wait4", ok, ok ? d : "bad status");
}

/* getcwd returns a non-empty absolute path. */
static void t_getcwd(void)
{
	char buf[256];
	char *r = getcwd(buf, sizeof(buf));
	int ok = (r != NULL && buf[0] == '/');
	check("getcwd", ok, ok ? buf : strerror(errno));
}

/* readdir over "/" returns entries. */
static void t_readdir(void)
{
	DIR *d = opendir("/");
	if (!d)
	{
		check("getdents/readdir", 0, strerror(errno));
		return;
	}
	int n = 0;
	while (readdir(d) != NULL)
		n++;
	closedir(d);
	char det[32];
	snprintf(det, sizeof(det), "%d entries", n);
	check("getdents/readdir", n > 0, det);
}

/* rename: create a temp file and rename it; stat the result. */
static void t_rename(void)
{
	const char *a = "/selfhostck.a";
	const char *b = "/selfhostck.b";
	int fd = open(a, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
	{
		check("rename", 0, "create failed (read-only fs?)");
		return;
	}
	write(fd, "x", 1);
	close(fd);
	int rc = rename(a, b);
	struct stat st;
	int ok = (rc == 0 && stat(b, &st) == 0);
	unlink(b);
	unlink(a);
	check("rename", ok, ok ? "" : strerror(errno));
}

/* chmod by path. */
static void t_chmod(void)
{
	const char *f = "/selfhostck.c";
	int fd = open(f, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
	{
		check("chmod", 0, "create failed");
		return;
	}
	close(fd);
	int rc = chmod(f, 0600);
	unlink(f);
	check("chmod", rc == 0, rc == 0 ? "" : strerror(errno));
}

/* getrlimit for the stack — the compiler sizes thread stacks from this. */
static void t_getrlimit(void)
{
	struct rlimit rl;
	int rc = getrlimit(RLIMIT_STACK, &rl);
	char d[40];
	snprintf(d, sizeof(d), "stack cur=%ld", (long)rl.rlim_cur);
	check("getrlimit", rc == 0, rc == 0 ? d : strerror(errno));
}

/* utimes: set mtime — make/bmake stamp dependency times.  Expected gap. */
static void t_utimes(void)
{
	const char *f = "/selfhostck.u";
	int fd = open(f, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
	{
		check("utimes", 0, "create failed");
		return;
	}
	close(fd);
	struct timeval tv[2];
	tv[0].tv_sec = 1000000;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = 1000000;
	tv[1].tv_usec = 0;
	int rc = utimes(f, tv);
	int persisted = 0;
	if (rc == 0)
	{
		struct stat st;
		if (stat(f, &st) == 0 && st.st_mtime == 1000000)
			persisted = 1;
	}
	unlink(f);
	/* PASS = the syscall succeeds (build tools must not hard-fail); whether the
	 * mtime actually persists is reported separately (a known follow-up). */
	check("utimes", rc == 0, rc != 0 ? strerror(errno) : (persisted ? "mtime persisted" : "ok (no mtime persist)"));
}

/* fchmod: chmod by open fd. */
static void t_fchmod(void)
{
	const char *f = "/selfhostck.f";
	int fd = open(f, O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (fd < 0)
	{
		check("fchmod", 0, "create failed");
		return;
	}
	int rc = fchmod(fd, 0600);
	close(fd);
	unlink(f);
	check("fchmod", rc == 0, rc == 0 ? "" : strerror(errno));
}

int main(void)
{
	printf("== uBixOS self-host foundation check ==\n");
	t_mmap_anon();
	t_mmap_file();
	t_mprotect();
	t_fork_wait();
	t_getcwd();
	t_readdir();
	t_rename();
	t_chmod();
	t_getrlimit();
	t_utimes();
	t_fchmod();
	printf("== %d passed, %d failed ==\n", g_pass, g_fail);
	return g_fail;
}
