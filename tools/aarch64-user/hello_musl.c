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
#include <fcntl.h>

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

	/* File I/O via real syscalls: open + read /proc/meminfo, print it. */
	int fd = open("/proc/meminfo", O_RDONLY);
	if (fd >= 0)
	{
		char b[256];
		ssize_t r = read(fd, b, sizeof(b) - 1);
		if (r > 0)
		{
			write(1, "open+read /proc/meminfo OK:\n", 28);
			write(1, b, (size_t)r);
		}
		else
		{
			write(1, "read(/proc/meminfo) returned <= 0\n", 34);
		}
		close(fd);
	}
	else
	{
		write(1, "open(/proc/meminfo) FAILED\n", 27);
	}
	return 0;
}
