/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * pwd(1) — write the absolute pathname of the current working directory to
 * stdout.  The kernel has no symbolic links, so -L and -P are equivalent and
 * the options are accepted but ignored.
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	char buf[1024];

	(void)argc;
	(void)argv;

	if (getcwd(buf, sizeof(buf)) == NULL)
	{
		perror("pwd");
		return (1);
	}

	puts(buf);
	return (0);
}
