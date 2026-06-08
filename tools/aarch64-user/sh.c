/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A minimal static shell for the aarch64 bring-up: print a prompt, read a line,
 * and either run a builtin (help, exit) or fork + execve /bin/<command> and
 * wait for it.  No argv parsing, pipes, or redirection yet — just enough to
 * prove an interactive shell that launches real programs over fork/execve/wait4.
 * Built static against the aarch64 musl, run by the kernel ELF loader.
 */
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/**
 * Read a line from stdin into @buf (NUL-terminated, trailing CR/LF stripped).
 *
 * @return the line length, or -1 on EOF/error.
 */
static int read_line(char *buf, int cap)
{
	int n = (int)read(0, buf, (size_t)cap - 1);
	if (n <= 0)
		return -1;
	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
		n--;
	buf[n] = '\0';
	return n;
}

/**
 * Fork a child that execve's /bin/@cmd, and wait for it.
 */
static void run_command(const char *cmd)
{
	char path[128];

	strcpy(path, "/bin/");
	strncat(path, cmd, sizeof(path) - 6);

	pid_t pid = fork();
	if (pid == 0)
	{
		char *argv[] = {path, 0};
		char *envp[] = {0};
		execve(path, argv, envp);
		static const char e[] = "sh: command not found: ";
		write(2, e, sizeof(e) - 1);
		write(2, cmd, strlen(cmd));
		write(2, "\n", 1);
		_exit(127);
	}
	if (pid < 0)
	{
		static const char f[] = "sh: fork failed\n";
		write(2, f, sizeof(f) - 1);
		return;
	}
	int status = 0;
	waitpid(pid, &status, 0);
}

int main(void)
{
	static const char banner[] = "uBixOS aarch64 shell — builtins: help, exit.\n";
	write(1, banner, sizeof(banner) - 1);

	for (;;)
	{
		char line[128];

		write(1, "# ", 2);
		if (read_line(line, sizeof(line)) < 0)
			return 0; /* EOF -> exit, login respawns */
		if (line[0] == '\0')
			continue;

		if (strcmp(line, "exit") == 0)
			return 0;
		if (strcmp(line, "help") == 0)
		{
			static const char h[] = "builtins: help, exit. anything else runs /bin/<word>.\n";
			write(1, h, sizeof(h) - 1);
			continue;
		}

		run_command(line);
	}
	return 0;
}
