/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/mpi.h>
#include <sys/wait.h>
#include <api/ubix.h>

#define INITD_PATH "/etc/init.d"
#define INITD_MAXSVCS 64
#define INITD_PATHMAX 256

/* Candidate system-console primaries.  init runs ONE program on the single
 * boot/system console (slot 0) — the console-first, graphical-optional model has
 * no switchable text VTs, so there is no getty-per-VT (the retired ttyd).  The
 * desktop profile stages /bin/views (the compositor); the base/headless profile
 * does not, and falls back to the text-console login.  init picks by what the
 * image staged (console_primary()), so the SAME init runs on every arch +
 * profile — the kernel no longer hand-launches the desktop. */
#define CONSOLE_DESKTOP "/bin/views"
#define CONSOLE_BASE "/bin/login"

static char *envp_login[6] = {
    "HOME=/",
    "PWD=/",
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "USER=root",
    "GROUP=admin",
    NULL,
};

/*
 * Choose the console primary: the desktop compositor if it was staged (desktop
 * profile), else the text-console login (base/headless profile).
 */
static const char *console_primary(void)
{
	int tries;

	/* The desktop image stages /bin/views.  A single fopen can transiently miss it
	 * when the filesystem read loses the virtio-blk SMP race (the same race behind
	 * the intermittent "failed-first-login"), which would wrongly drop a desktop
	 * image to the text console.  Retry briefly so one flaky read doesn't cost the
	 * whole graphical session. */
	for (tries = 0; tries < 20; tries++)
	{
		FILE *f = fopen(CONSOLE_DESKTOP, "r");
		if (f != NULL)
		{
			fclose(f);
			return (CONSOLE_DESKTOP);
		}
		usleep(50000); /* 50 ms — let the FS settle, then retry */
	}
	return (CONSOLE_BASE);
}

/*
 * Scan INITD_PATH for service definition files.  Each file contains one
 * line: the full VFS path of the binary to exec.  Files are started in
 * lexicographic order, so a numeric prefix (10-logd, 20-views …) controls
 * the launch sequence.  Lines beginning with '#' and empty lines are
 * skipped.  Missing or unreadable files are silently ignored.
 */
static void start_initd_services(void)
{
	DIR *d;
	struct dirent *ent;
	char *names[INITD_MAXSVCS];
	int nnames = 0, i, j;
	char path[INITD_PATHMAX];
	char execpath[INITD_PATHMAX];
	char *tmp;
	FILE *f;
	char *argv_svc[2];

	d = opendir(INITD_PATH);
	if (d == NULL)
		return;

	while ((ent = readdir(d)) != NULL && nnames < INITD_MAXSVCS)
	{
		if (ent->d_name[0] == '.')
			continue;
		names[nnames] = strdup(ent->d_name);
		if (names[nnames] != NULL)
			nnames++;
	}
	closedir(d);

	/* bubble sort — numeric prefixes (10-, 20-, …) set launch order */
	for (i = 0; i < nnames - 1; i++)
	{
		for (j = i + 1; j < nnames; j++)
		{
			if (strcmp(names[i], names[j]) > 0)
			{
				tmp = names[i];
				names[i] = names[j];
				names[j] = tmp;
			}
		}
	}

	for (i = 0; i < nnames; i++)
	{
		snprintf(path, sizeof(path), "%s/%s", INITD_PATH, names[i]);
		free(names[i]);

		f = fopen(path, "r");
		if (f == NULL)
			continue;
		if (fgets(execpath, sizeof(execpath), f) == NULL)
		{
			fclose(f);
			continue;
		}
		fclose(f);

		execpath[strcspn(execpath, "\r\n")] = '\0';
		if (execpath[0] == '\0' || execpath[0] == '#')
			continue;

		printf("init: starting %s\n", execpath);
		fflush(stdout);

		if (fork() == 0)
		{
			argv_svc[0] = execpath;
			argv_svc[1] = NULL;
			execve(execpath, argv_svc, envp_login);
			printf("init: failed to exec %s\n", execpath);
			exit(0);
		}
	}
}

/*
 * Console supervisor (runs as a forked child of init): claim the system console
 * (settty is a no-op on aarch64 — fds 0/1/2 are already wired to the console) and
 * respawn @prog forever, so logout re-prompts, the way the retired ttyd's getty
 * loop did — for a single console, no switchable VTs.  Never returns.
 */
static void console_supervisor(const char *prog)
{
	char *av[2];

	if (settty(0) != 0)
	{
		printf("init: settty(0) failed\n");
		exit(1);
	}

	av[0] = (char *)prog;
	av[1] = NULL;
	printf("init: console = %s\n", prog);
	for (;;)
	{
		int p = fork();
		if (p == 0)
		{
			execve(prog, av, envp_login);
			printf("init: failed to exec %s\n", prog);
			exit(1);
		}
		if (p < 0)
		{
			printf("init: fork for %s failed\n", prog);
			exit(1);
		}
		waitpid(p, NULL, 0);
	}
}

/*
 * Launch the system-console primary and keep it alive in a supervised child, so
 * init can return to its idle/reaper loop.
 *
 * In the desktop profile we ALSO run a text login on the serial console (PL011),
 * so the serial line is an interactive shell *alongside* the GUI — the
 * X-on-tty7 + getty-on-ttyS0 model.  The two never fight: the serial login owns
 * fd 0/1/2 (the PL011 console fileops), while the compositor uses the framebuffer
 * (sys_mapfb) and virtio keyboard, ignoring its console fds.  This keeps the
 * serial console maintained (e.g. for `bmake run-claude`).  The base/headless
 * profile already IS a serial login, so it needs no second one.
 */
static void start_console(void)
{
	const char *primary = console_primary();

	if (strcmp(primary, CONSOLE_DESKTOP) == 0)
	{
		if (fork() == 0)
		{
			console_supervisor(CONSOLE_BASE); /* /bin/login on the serial console */
			exit(0);                          /* unreachable */
		}
	}

	if (fork() == 0)
	{
		console_supervisor(primary); /* desktop compositor (or login for base) */
		exit(0);                     /* unreachable */
	}

	/* init (parent) returns to its idle/reaper loop. */
}

int main(int argc, char **argv, char **envp)
{
	mpi_message_t myMsg;

	/* Create a mailbox for this task */
	if (mpi_createMbox("init") != 0x0)
	{
		printf("Error: Failed to creating mail box: init\n");
		exit(0x1);
	}

	/* Make sure we have superuser permissions if not exit */
	if ((getuid() != 0x0) || (getgid() != 0x0))
	{
		printf("Error: This program must be run by root.\n");
		exit(0x1);
	}

	printf("Initializing UbixOS\n");

	start_initd_services();

	/* Launch the system-console primary (the desktop compositor) on slot 0.
	 * The old multi-VT ttyd / etc/ttys is retired — one system console, no
	 * switchable text VTs.  (ubistry, authd, … are started via /etc/init.d.) */
	start_console();

	/* Idle / reaper loop.  Block on the init mailbox instead of busy-polling; the
	 * 1 s timeout (100 ticks @ 100 Hz) keeps PID 1 ticking over as a safety hedge,
	 * though service reaping is handled by the per-service supervisor children,
	 * not here. */
	for (;;)
	{
		if (mpi_waitMessage("init", &myMsg, 100) == 0x0)
		{
			/* reserved for future init control messages */
		}
	}

	return (0x0);
}

/***
 END
 ***/
