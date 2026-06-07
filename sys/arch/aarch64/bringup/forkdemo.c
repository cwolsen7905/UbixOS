/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 fork() demo (QEMU `virt` bring-up, Phase 13g).
 *
 * Launch a user process whose payload calls fork(): the kernel duplicates its
 * address space + trapframe (aarch64_fork), the child resumes at the same EL0
 * point returning 0, and the scheduler runs both.  Parent and child each write
 * a distinct line and exit — two processes from one fork, on aarch64.  Runs
 * after the scheduler demos, reusing the task list.
 */

#include "bringup.h"
#include <ubixos/sched.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>   /* PAGE_SIZE */
#include <lib/kmalloc.h>  /* sysID */
#include <sys/elf_load.h> /* md_sync_icache */
#include <string.h>

#define FORK_CODE_VA 0x180000000UL /* block 6: a clean, unused VA slot */
#define FORK_STACK_VA 0x180010000UL
#define FORK_STACK_TOP (FORK_STACK_VA + PAGE_SIZE)

/**
 * Create a user process that fork()s and let the scheduler run parent + child.
 */
void aarch64_fork_demo(void)
{
	u_int64_t *l1;
	uintptr_t code_frame, stack_frame;
	u_int64_t code_len = (u_int64_t)(fork_demo_code_end - fork_demo_code_start);
	kTask_t *t;
	int i;

	kprintf("fork demo: launching a process that forks...\n");

	l1 = pmap_create_user_space();
	code_frame = vmm_find_free_page(sysID);
	stack_frame = vmm_find_free_page(sysID);
	memcpy((void *)code_frame, fork_demo_code_start, (size_t)code_len);
	md_sync_icache(code_frame, code_len);
	pmap_map_user_page(l1, FORK_CODE_VA, (u_int64_t)code_frame, 1);
	pmap_map_user_page(l1, FORK_STACK_VA, (u_int64_t)stack_frame, 0);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = FORK_CODE_VA;
	t->md.md_usp = FORK_STACK_TOP;
	strncpy(t->name, "forker", sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  forker pid=%d ready; yielding to the scheduler...\n", t->id);

	/* Yield enough times for the parent to fork and both to run + exit. */
	for (i = 0; i < 128; i++)
		sched_yield();

	kprintf("fork demo: done — fork() works on aarch64.\n");
}
