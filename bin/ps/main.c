/*
 * ps — list processes from /proc.
 *
 * Reads /proc/<pid>/status (fields Name, Pid, PPid, State) for every numeric
 * entry under /proc and prints one line per process:
 *
 *   PID   PPID  STATE  NAME
 *
 * (C) 2002-2026 The UbixOS Project
 */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct proc {
	long pid;
	long ppid;
	char state[24];
	char name[64];
};

/**
 * Parse /proc/<pid>/status into @p p.
 *
 * @return 0 on success, -1 if the process vanished or the file is unreadable.
 */
static int read_status(const char *pid, struct proc *p)
{
	char path[64], line[256];
	FILE *fp;

	snprintf(path, sizeof(path), "/proc/%s/status", pid);
	fp = fopen(path, "r");
	if (!fp)
		return -1;
	memset(p, 0, sizeof(*p));
	p->pid = strtol(pid, NULL, 10);
	while (fgets(line, sizeof(line), fp)) {
		char *v = strchr(line, ':');

		if (!v)
			continue;
		*v++ = '\0';
		while (*v == ' ' || *v == '\t')
			v++;
		v[strcspn(v, "\n")] = '\0';
		if (!strcmp(line, "Name"))
			snprintf(p->name, sizeof(p->name), "%s", v);
		else if (!strcmp(line, "Pid"))
			p->pid = strtol(v, NULL, 10);
		else if (!strcmp(line, "PPid"))
			p->ppid = strtol(v, NULL, 10);
		else if (!strcmp(line, "State"))
			snprintf(p->state, sizeof(p->state), "%s", v);
	}
	fclose(fp);
	return 0;
}

/** qsort comparator: ascending PID. */
static int by_pid(const void *a, const void *b)
{
	long x = ((const struct proc *)a)->pid, y = ((const struct proc *)b)->pid;

	return (x > y) - (x < y);
}

int main(void)
{
	struct proc *procs = NULL;
	size_t n = 0, cap = 0;
	struct dirent *de;
	DIR *d = opendir("/proc");

	if (!d) {
		perror("ps: /proc");
		return 1;
	}
	while ((de = readdir(d))) {
		struct proc p;

		if (!isdigit((unsigned char)de->d_name[0]))
			continue;
		if (read_status(de->d_name, &p) < 0)
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 32;
			procs = realloc(procs, cap * sizeof(*procs));
			if (!procs)
				return 1;
		}
		procs[n++] = p;
	}
	closedir(d);

	if (n)
		qsort(procs, n, sizeof(*procs), by_pid);
	printf("%5s %5s %-9s %s\n", "PID", "PPID", "STATE", "NAME");
	for (size_t i = 0; i < n; i++)
		printf("%5ld %5ld %-9s %s\n", procs[i].pid, procs[i].ppid, procs[i].state, procs[i].name);
	free(procs);
	return 0;
}
