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
#include <ubixos/sched.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>
#include <sys/elf_load.h>
#include <string.h>

extern char _binary_hello_musl_elf_start[];
extern char _binary_hello_musl_elf_end[];

#define MUSL_STACK_VA 0x100200000UL /* well above the program segments (0x100000000+) */
#define MUSL_STACK_TOP (MUSL_STACK_VA + PAGE_SIZE)
#define INITIAL_FRAME 8 /* u64 slots: argc, argv[0]=NULL, envp[0]=NULL, auxv{0,0}, pad */

/**
 * Load + run the embedded musl ELF with a minimal SysV initial stack.
 */
void aarch64_musl_elf_demo(void)
{
	u_int64_t *l1, entry;
	uintptr_t stack_frame;
	u_int64_t *sp;
	kTask_t *t;
	int i;

	if ((u_int64_t)(_binary_hello_musl_elf_end - _binary_hello_musl_elf_start) < 64)
	{
		kprintf("musl-elf demo: no musl ELF embedded (run 'bmake musl-libc TARGET=aarch64' "
		        "then rebuild the kernel); skipping.\n");
		return;
	}

	kprintf("musl-elf demo: loading a musl-linked aarch64 program...\n");

	l1 = pmap_create_user_space();
	if (elf64_load(_binary_hello_musl_elf_start, l1, &entry) != 0)
	{
		kprintf("musl-elf demo: ELF load failed\n");
		return;
	}

	/* Build the initial stack image (in the frame's identity alias): the SysV
	 * layout __libc_start_main expects at SP — argc, argv[], envp[], auxv[]. */
	stack_frame = vmm_find_free_page(sysID);
	sp = (u_int64_t *)(stack_frame + PAGE_SIZE) - INITIAL_FRAME;
	for (i = 0; i < INITIAL_FRAME; i++)
		sp[i] = 0; /* argc=0, argv[0]=NULL, envp[0]=NULL, auxv{AT_NULL,0} */
	pmap_map_user_page(l1, MUSL_STACK_VA, (u_int64_t)stack_frame, 0);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = MUSL_STACK_TOP - (u_int64_t)INITIAL_FRAME * 8; /* SP points at argc */
	strncpy(t->name, "hellomusl", sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  pid=%d ready (entry=0x%lX); yielding...\n", t->id, entry);
	for (i = 0; i < 128 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();

	kprintf("musl-elf demo: returned (state=%d).\n", t->state);
}
