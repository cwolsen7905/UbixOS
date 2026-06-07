/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Minimal freestanding AArch64 user program — no libc, raw SVC syscalls.
 *
 * Built static/no-pie (-Ttext at a fixed user VA), embedded into the kernel and
 * run via the ELF64 loader.  This proves the real toolchain -> ELF -> kernel
 * loader -> syscall-ABI pipeline (the userland-port spike) before musl is ported.
 * Syscall numbers follow the FreeBSD ABI the aarch64 dispatcher uses
 * (write=4, exit=1).
 */

static long my_write(int fd, const char *buf, unsigned long len)
{
	register long x8 __asm__("x8") = 4; /* SYS_write */
	register long x0 __asm__("x0") = fd;
	register long x1 __asm__("x1") = (long)buf;
	register long x2 __asm__("x2") = (long)len;
	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
	return x0;
}

static void my_exit(int code)
{
	register long x8 __asm__("x8") = 1; /* SYS_exit */
	register long x0 __asm__("x0") = code;
	__asm__ volatile("svc #0" : : "r"(x8), "r"(x0) : "memory");
	__builtin_unreachable();
}

void _start(void)
{
	static const char msg[] = "Hello from a COMPILED aarch64 user ELF (kernel ELF loader + SVC ABI)!\n";

	my_write(1, msg, sizeof(msg) - 1);
	my_exit(0);
}
