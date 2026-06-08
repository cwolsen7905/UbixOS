/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A musl-linked AArch64 "init" stand-in that exercises the process-chain
 * primitives: fork() a child, have the child execve(/bin/child), and wait4() to
 * reap it.  This proves fork + execve(path) + wait4/reap end-to-end on aarch64 —
 * the mechanism the real init -> login -> shell chain rides on.  Built static
 * against the aarch64 musl and run by the kernel ELF loader.
 */
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(void)
{
	static const char banner[] = "uBixOS aarch64 init: fork/execve/wait4 demo\n";
	write(1, banner, sizeof(banner) - 1);

	pid_t pid = fork();
	if (pid == 0)
	{
		/* Child: replace ourselves with /bin/child via execve. */
		static const char m[] = "init: child here, execve(/bin/child)...\n";
		write(1, m, sizeof(m) - 1);
		char *argv[] = {"/bin/child", 0};
		char *envp[] = {0};
		execve("/bin/child", argv, envp);
		static const char e[] = "init: execve FAILED\n";
		write(2, e, sizeof(e) - 1);
		_exit(127);
	}
	else if (pid > 0)
	{
		/* Parent: block until the child exits, then report. */
		static const char w[] = "init: forked child, waiting...\n";
		write(1, w, sizeof(w) - 1);
		int status = 0;
		pid_t r = waitpid(pid, &status, 0);
		if (r > 0)
		{
			static const char ok[] = "init: child reaped OK\n";
			write(1, ok, sizeof(ok) - 1);
		}
		else
		{
			static const char bad[] = "init: waitpid FAILED\n";
			write(2, bad, sizeof(bad) - 1);
		}
	}
	else
	{
		static const char f[] = "init: fork FAILED\n";
		write(2, f, sizeof(f) - 1);
	}

	static const char done[] = "init: exiting.\n";
	write(1, done, sizeof(done) - 1);
	return 0;
}
