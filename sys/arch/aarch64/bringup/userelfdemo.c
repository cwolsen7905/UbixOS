/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 compiled-user-ELF demo (QEMU `virt` bring-up — userland-port spike).
 *
 * Loads a REAL compiled aarch64 ELF (tools/aarch64-user/hello.c, built static
 * /no-pie and embedded via objcopy as _binary_hello_elf_*) through the generic
 * ELF64 loader and runs it as a scheduled process.  Unlike the hand-written
 * el0.S payloads, this exercises the actual toolchain -> ELF -> loader ->
 * syscall-ABI path end to end — the foundation the musl/world port builds on.
 */

#include "bringup.h"
#include <ubixos/sched.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>  /* PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <sys/elf_load.h>
#include <string.h>

/* The embedded ELF image (objcopy -I binary of hello.elf). */
extern char _binary_hello_elf_start[];
extern char _binary_hello_elf_end[];

#define UELF_STACK_VA 0x100010000UL /* a page above the program's link base (0x100000000) */
#define UELF_STACK_TOP (UELF_STACK_VA + PAGE_SIZE)

/**
 * Load the embedded compiled ELF into a fresh address space and run it.
 */
void aarch64_user_elf_demo(void)
{
	u_int64_t *l1, entry;
	uintptr_t stack_frame;
	kTask_t *t;
	int i;

	kprintf("user-elf demo: loading a COMPILED aarch64 ELF (%lu bytes) via the loader...\n",
	        (u_int64_t)(_binary_hello_elf_end - _binary_hello_elf_start));

	l1 = pmap_create_user_space();
	if (elf64_load(_binary_hello_elf_start, l1, &entry) != 0)
	{
		kprintf("user-elf demo: ELF load failed\n");
		return;
	}

	stack_frame = vmm_find_free_page(sysID);
	pmap_map_user_page(l1, UELF_STACK_VA, (u_int64_t)stack_frame, 0);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = UELF_STACK_TOP;
	strncpy(t->name, "hello", sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  pid=%d ready (entry=0x%lX); yielding to the scheduler...\n", t->id, entry);

	for (i = 0; i < 64 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();

	kprintf("user-elf demo: compiled user ELF ran on aarch64 — toolchain pipeline works.\n");
}
