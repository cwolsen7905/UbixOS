/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A musl-linked AArch64 user program (uses libc, not raw syscalls).  Built
 * static against the aarch64 musl and run via the kernel ELF loader — exercises
 * the full crt + __libc_start_main + libc path, driving the kernel syscall
 * surface a real program needs.
 */
#include <unistd.h>

int main(void)
{
	static const char msg[] = "Hello from a musl-linked aarch64 program!\n";
	write(1, msg, sizeof(msg) - 1);
	return 0;
}
