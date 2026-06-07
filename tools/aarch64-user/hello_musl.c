/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A musl-linked AArch64 user program (uses libc, not raw syscalls).  Built
 * static against the aarch64 musl and run via the kernel ELF loader — exercises
 * the full crt + __libc_start_main + libc path, driving the kernel syscall
 * surface a real program needs.
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	static const char msg[] = "Hello from a musl-linked aarch64 program!\n";
	write(1, msg, sizeof(msg) - 1);

	char *p = malloc(256);
	if (p)
	{
		memset(p, 'A', 255);
		p[255] = '\0';
		write(1, "malloc(256) OK\n", 15);
		free(p);
	}
	else
	{
		write(1, "malloc(256) FAILED\n", 19);
	}
	return 0;
}
