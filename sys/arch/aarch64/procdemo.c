/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 scheduled-process demo (QEMU `virt` bring-up, Phase 13f).
 *
 * The payoff of the kernel-primitive stack: a real user process the SCHEDULER
 * dispatches, not a one-shot enter_el0 demo.  Build a fresh address space, map
 * the EL0 payload + a stack, create a kTask_t pointing at it (md_ttbr0/md_entry/
 * md_usp), and sched_ready() it.  When sched() picks it, switch_to swaps to its
 * address space and the user_trampoline ERETs to EL0; the program writes via the
 * write syscall and exit() terminates the task, after which the scheduler
 * returns to this (kernel) context.  Runs after the cooperative scheduler demo,
 * reusing its task list.
 */

#include "bringup.h"
#include <ubixos/sched.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>   /* PAGE_SIZE */
#include <lib/kmalloc.h>  /* sysID */
#include <sys/elf_load.h> /* md_sync_icache */
#include <string.h>

#define PROC_CODE_VA 0x140000000UL /* block 5: a clean, unused VA slot */
#define PROC_STACK_VA 0x140010000UL
#define PROC_STACK_TOP (PROC_STACK_VA + PAGE_SIZE)

/**
 * Create a user process from the EL0 payload and let the scheduler run it.
 */
void aarch64_proc_demo(void)
{
	u_int64_t *l1;
	uintptr_t code_frame, stack_frame;
	u_int64_t code_len = (u_int64_t)(user_demo_code_end - user_demo_code_start);
	kTask_t *t;
	int i;

	kprintf("proc demo: scheduling a real user process...\n");

	/* Fresh address space with the payload mapped EL0-executable + a stack. */
	l1 = pmap_create_user_space();
	code_frame = vmm_find_free_page(sysID);
	stack_frame = vmm_find_free_page(sysID);
	memcpy((void *)code_frame, user_demo_code_start, (size_t)code_len);
	md_sync_icache(code_frame, code_len);
	pmap_map_user_page(l1, PROC_CODE_VA, (u_int64_t)code_frame, 1);
	pmap_map_user_page(l1, PROC_STACK_VA, (u_int64_t)stack_frame, 0);

	/* A task that runs at EL0 in that address space. */
	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = PROC_CODE_VA;
	t->md.md_usp = PROC_STACK_TOP;
	strncpy(t->name, "el0proc", sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  task pid=%d ready; yielding to the scheduler...\n", t->id);

	/* Yield until the scheduler has run the process to completion. */
	for (i = 0; i < 64 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();

	kprintf("proc demo: user process (pid=%d) ran + exited via the scheduler — "
	        "scheduled processes work on aarch64.\n",
	        t->id);
}
