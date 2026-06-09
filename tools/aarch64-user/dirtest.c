/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * dirtest — aarch64 bring-up test for the directory-reading syscalls that `ls`
 * uses (opendir/readdir = open + getdents, and stat = statx).  These are the
 * pointer-arg POSIX calls a prior Phase-3 trial prune routed through the shared
 * table, where they tripped HVF's `isv` assertion.  Running this in the no-disk
 * boot path lets the prune be bisected headlessly: it prints the entry count and
 * one stat result, or faults at the syscall that breaks.
 */
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

static void out(const char *s)
{
	write(1, s, strlen(s));
}

int main(void)
{
	DIR *d;
	struct dirent *e;
	struct stat st;
	int n = 0;
	char line[128];

	out("dirtest: opendir(\"/bin\")\n");
	d = opendir("/bin");
	if (d == NULL)
	{
		out("dirtest: FAIL — opendir failed\n");
		return (1);
	}

	while ((e = readdir(d)) != NULL) /* readdir -> getdents */
		n++;
	closedir(d);

	snprintf(line, sizeof(line), "dirtest: read %d entries\n", n);
	out(line);

	if (stat("/bin/cat", &st) == 0) /* stat -> statx */
		snprintf(line, sizeof(line), "dirtest: stat /bin/cat size=%ld ok\n", (long)st.st_size);
	else
		snprintf(line, sizeof(line), "dirtest: stat /bin/cat FAILED\n");
	out(line);

	out("dirtest: PASS\n");
	return (0);
}
