/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * ttyd — terminal daemon (UbixOS equivalent of FreeBSD getty).
 *
 * Reads /etc/ttys.  For each enabled entry it forks a child that
 * claims the named terminal slot, then loops: fork the configured
 * program (login, vlogin, shell, …), wait for it to exit, restart.
 *
 * /etc/ttys format (whitespace-delimited, # comments):
 *   <name>    <program>   <on|off>
 *
 * name → slot mapping:
 *   console   0   VGA text console
 *   com1      1   COM1 RS-232 serial
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <api/ubix.h>

#define TTYS_PATH "/etc/ttys"
#define MAX_TTYS 8

static char *envp_login[] = {
    "HOME=/", "PWD=/", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "USER=root", "GROUP=admin", "TERM=vt100", NULL};

/**
 * envp_login - default environment passed to login shells
 *
 * This array provides a minimal environment for spawned login programs
 * (for example `login` or `vlogin`). It is passed to `execve()` and
 * must be NULL-terminated.
 */

static int name_to_slot(const char *name)
{
	/* Virtual VGA consoles: ttyv0..ttyv3 → slots 0-3 */
	if (strcmp(name, "ttyv0") == 0)
		return 0;
	if (strcmp(name, "ttyv1") == 0)
		return 1;
	if (strcmp(name, "ttyv2") == 0)
		return 2;
	if (strcmp(name, "ttyv3") == 0)
		return 3;
	/* Serial consoles: com1 → slot 4, com2 → slot 5 */
	if (strcmp(name, "com1") == 0)
		return 4;
	if (strcmp(name, "com2") == 0)
		return 5;
	return -1;
}

/**
 * name_to_slot - map a tty name to a console slot index
 * @name: tty device name (e.g. "ttyv0", "com1")
 *
 * Returns the integer slot index for the given tty name, or `-1` if the
 * name is unknown. Virtual VGA consoles `ttyv0..ttyv3` map to slots 0..3;
 * serial consoles `com1`/`com2` map to slots 4/5.
 */

/* Getty loop — runs in a forked child, never returns normally. */
static void getty_loop(int slot, const char *name, const char *program)
{
	char *argv[2] = {(char *)program, NULL};
	int is_serial = (slot > 3);

	if (settty(slot) != 0)
	{
		printf("ttyd: settty(%d) failed\r\n", slot);
		exit(1);
	}

	if (is_serial)
		printf("\r\nttyd: %s ready (%s)\r\n", name, program);
	else
		printf("\nttyd: %s ready (%s)\n", name, program);

	for (;;)
	{
		int pid = fork();
		if (pid == 0)
		{
			execve(program, argv, envp_login);
			printf("ttyd: exec %s failed (slot %d)\r\n", program, slot);
			exit(1);
		}
		if (pid < 0)
		{
			printf("ttyd: fork failed (slot %d)\r\n", slot);
			exit(1);
		}
		waitpid(pid, NULL, 0);
	}
}

/**
 * getty_loop - persistent getty loop for a single terminal slot
 * @slot: numeric tty slot claimed by this process
 * @name: textual tty name as listed in /etc/ttys
 * @program: program to exec for login (path)
 *
 * This function runs in a forked child and never returns under normal
 * operation. It claims the terminal via `settty(slot)`, announces readiness,
 * and then continually forks and execs the configured `program`. When the
 * child process exits the loop restarts the program.
 */

int main(int argc, char *argv[])
{
	FILE *f;
	char line[256];
	char name[64], program[128], status[16];
	int spawned = 0;

	f = fopen(TTYS_PATH, "r");
	if (f == NULL)
	{
		printf("ttyd: cannot open %s\n", TTYS_PATH);
		return (1);
	}

	while (fgets(line, sizeof(line), f) != NULL)
	{
		/* strip trailing newline */
		line[strcspn(line, "\r\n")] = '\0';

		/* skip blank lines and comments */
		if (line[0] == '\0' || line[0] == '#')
			continue;

		if (sscanf(line, "%63s %127s %15s", name, program, status) != 3)
			continue;

		if (strcmp(status, "on") != 0)
			continue;

		int slot = name_to_slot(name);
		if (slot < 0)
		{
			printf("ttyd: unknown terminal '%s', skipping\n", name);
			continue;
		}

		/* Last entry becomes this process; all others are forked. */
		/* We fork for every entry and let the parent exit after all
		 * children are spawned — each child owns its own getty loop. */
		int pid = fork();
		if (pid == 0)
		{
			fclose(f);
			getty_loop(slot, name, program);
			/* not reached */
			exit(1);
		}
		if (pid < 0)
		{
			printf("ttyd: fork failed for %s\n", name);
		}
		else
		{
			spawned++;
		}
	}

	fclose(f);

	if (spawned == 0)
	{
		printf("ttyd: no terminals configured in %s\n", TTYS_PATH);
		return (1);
	}

	/* Parent idles — children own the getty loops. */
	for (;;)
		sched_yield();

	return (0);
}

/**
 * main - ttyd daemon entry point
 * @argc: argument count (unused)
 * @argv: argument vector (unused)
 *
 * Reads `TTYS_PATH` (`/etc/ttys`) and for each enabled entry forks a child
 * which owns a getty loop for that terminal. The parent process spawns all
 * children and then idles; each child runs `getty_loop()` and manages its
 * own login program. Returns non-zero on startup failure or if no terminals
 * are configured.
 */
