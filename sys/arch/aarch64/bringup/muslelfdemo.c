/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 musl-linked program demo (userland-port spike U2).
 *
 * Loads a static musl-linked ELF (tools/aarch64-user/hello_musl.c) and runs it
 * with a minimal initial stack (argc/argv/envp/auxv) so musl's
 * __libc_start_main can start.  Reveals which syscalls musl startup + the
 * program actually need from the kernel.  Throwaway spike scaffolding.
 */

#include "bringup.h"

extern char _binary_hello_musl_elf_start[];
extern char _binary_hello_musl_elf_end[];

/**
 * Load + run the embedded musl ELF (via the shared aarch64_run_elf_image()).
 */
void aarch64_musl_elf_demo(void)
{
	int st;

	if ((u_int64_t)(_binary_hello_musl_elf_end - _binary_hello_musl_elf_start) < 64)
	{
		kprintf("musl-elf demo: no musl ELF embedded (run 'bmake musl-libc TARGET=aarch64' "
		        "then rebuild the kernel); skipping.\n");
		return;
	}

	kprintf("musl-elf demo: loading a musl-linked aarch64 program...\n");
	st = aarch64_run_elf_image(_binary_hello_musl_elf_start, "hellomusl");
	if (st < 0)
		kprintf("musl-elf demo: ELF load failed\n");
	else
		kprintf("musl-elf demo: returned (state=%d).\n", st);
}
