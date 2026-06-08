/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A minimal static "login" for the aarch64 bring-up: prompt for a username +
 * password, check them against the built-in default (root / user — the same
 * credentials the i386 image ships), and on success execve the shell.  Reading
 * /etc/userdb + disabling password echo are follow-ups (no termios on aarch64
 * yet).  Built static against the aarch64 musl, run by the kernel ELF loader.
 */
#include <unistd.h>
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

int main(void)
{
	for (;;)
	{
		char user[64], pass[64];

		write(1, "\nuBixOS aarch64 login: ", 23);
		if (read_line(user, sizeof(user)) < 0)
			return 0; /* EOF -> let init respawn us */

		write(1, "Password: ", 10);
		if (read_line(pass, sizeof(pass)) < 0)
			return 0;

		if (strcmp(user, "root") == 0 && strcmp(pass, "user") == 0)
		{
			static const char ok[] = "\nWelcome to uBixOS (aarch64).\n";
			write(1, ok, sizeof(ok) - 1);
			char *argv[] = {"/bin/sh", 0};
			char *envp[] = {0};
			execve("/bin/sh", argv, envp);
			static const char e[] = "login: cannot exec /bin/sh\n";
			write(2, e, sizeof(e) - 1);
			return 127;
		}

		static const char bad[] = "Login incorrect\n";
		write(1, bad, sizeof(bad) - 1);
	}
	return 0;
}
