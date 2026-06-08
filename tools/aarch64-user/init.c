/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A minimal static "init" (PID-1 stand-in) for the aarch64 bring-up: fork
 * /bin/login, wait for it, and respawn it when it exits — the classic init
 * loop, proving the real boot chain (init -> login -> shell) on aarch64 without
 * the i386 init's MPI machinery.  Built static against the aarch64 musl and run
 * by the kernel ELF loader off the ramfs root.
 */
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(void)
{
	static const char banner[] = "\nuBixOS aarch64 init (PID-1 stand-in) starting.\n";
	write(1, banner, sizeof(banner) - 1);

	for (;;)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			char *argv[] = {"/bin/login", 0};
			char *envp[] = {0};
			execve("/bin/login", argv, envp);
			static const char e[] = "init: cannot exec /bin/login\n";
			write(2, e, sizeof(e) - 1);
			_exit(127);
		}
		if (pid < 0)
		{
			static const char f[] = "init: fork failed\n";
			write(2, f, sizeof(f) - 1);
			return 1;
		}

		int status = 0;
		waitpid(pid, &status, 0);
		static const char r[] = "init: login exited; respawning.\n";
		write(1, r, sizeof(r) - 1);
	}
	return 0;
}
