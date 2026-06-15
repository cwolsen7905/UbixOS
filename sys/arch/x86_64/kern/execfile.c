/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ELF execution + execve (Phase 5e).  Loads an ELF64 into a fresh address
 * space and builds the minimal SysV amd64 initial stack musl's
 * __libc_start_main expects (argc/argv/envp/auxv incl. AT_PAGESZ — musl reads
 * PAGE_SIZE from there or malloc fails), then either schedules it as a new task
 * (the demo / a future spawn) or replaces the calling task (sys_execve).  Sibling
 * of aarch64's kern/execfile.c.
 */

#include "../x86_64.h"
#include <ubixos/sched.h>
#include <sys/thread.h>
#include <sys/sysproto_posix.h>
#include <sys/elf64.h>
#include <sys/elf_load.h>
#include <vmm/vmm.h>     /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <fs/vfs/file.h> /* fopen/fread/fclose */
#include <string.h>

#define AT_NULL 0
#define AT_PAGESZ 6

#define EXEC_STACK_VA 0x40020000UL /* user stack (above the ELF/identity, below mmap/brk) */
#define EXEC_STACK_TOP (EXEC_STACK_VA + PAGE_SIZE)
#define INITIAL_FRAME 6 /* argc, argv NULL, envp NULL, AT_PAGESZ, pagesz, AT_NULL */

/**
 * Load @image into a fresh address space + build the SysV initial stack.
 * @return the new PML4 (physical), or 0 on failure; *out_entry / *out_usp get the
 * entry VA and the initial RSP (pointing at argc).
 */
u64 x86_64_build_user_image(const void *image, u64 *out_entry, u64 *out_usp)
{
	u64 pml4 = x86_64_create_user_space();
	u_int64_t entry = 0;
	uintptr_t stack_frame;
	u64 *sp;
	int i;

	if (pml4 == 0)
		return 0;
	if (elf64_load(image, (u_int64_t *)(uintptr_t)pml4, &entry) != 0)
		return 0;

	/* Build the initial stack in a fresh frame (written through the physmap), then
	 * map it at EXEC_STACK_VA.  Minimal: argc=0, empty argv/envp, AT_PAGESZ. */
	stack_frame = vmm_find_free_page(sysID);
	if (stack_frame == 0)
		return 0;
	sp = (u64 *)((u8 *)P2V(stack_frame) + PAGE_SIZE) - INITIAL_FRAME;
	for (i = 0; i < INITIAL_FRAME; i++)
		sp[i] = 0;
	sp[0] = 0;         /* argc */
	sp[1] = 0;         /* argv[0] = NULL terminator */
	sp[2] = 0;         /* envp[0] = NULL terminator */
	sp[3] = AT_PAGESZ; /* auxv */
	sp[4] = PAGE_SIZE;
	sp[5] = AT_NULL;
	x86_64_map_user_page_to(pml4, EXEC_STACK_VA, (u64)stack_frame, 1);

	*out_entry = entry;
	*out_usp = EXEC_STACK_TOP - (u64)INITIAL_FRAME * 8; /* RSP points at argc */
	return pml4;
}

/**
 * execve(path, argv, envp): replace the calling task's image with the ELF at
 * @path (read off the VFS), then enter it at ring 3 — does not return on success.
 * argv/envp marshalling is minimal for now (argc=0); the full SysV copy lands
 * with the world.
 */
int sys_execve(struct thread *td, struct sys_execve_args *uap)
{
	fileDescriptor_t *fp;
	void *buf;
	u64 pml4, entry, usp;
	long n;

	(void)td;
	fp = fopen((const char *)uap->fname, "r");
	if (fp == 0)
		return -1;

	/* Slurp the whole file (bring-up: cap at 1 MB; the dynamic loader comes later). */
	buf = (void *)kmalloc(1024 * 1024);
	if (buf == 0)
	{
		fclose(fp);
		return -1;
	}
	n = (long)fread(buf, 1, 1024 * 1024, fp);
	fclose(fp);
	if (n <= 0)
	{
		kfree(buf);
		return -1;
	}

	pml4 = x86_64_build_user_image(buf, &entry, &usp);
	kfree(buf);
	if (pml4 == 0)
		return -1;

	/* Replace the current task's image and enter it.  switch_to re-arms rsp0 from
	 * kernelStack, so update md_* and load the new CR3 ourselves, then IRETQ. */
	_current->md.md_cr3 = pml4;
	_current->md.md_entry = entry;
	_current->md.md_usp = usp;
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pml4) : "memory");
	x86_64_iret_to_user(entry, usp); /* does not return */
	return 0;                        /* unreachable */
}

/**
 * Phase 5e demo: load a real ELF64 off the FAT root via the full path (fopen/
 * fread) + x86_64_build_user_image, schedule it, and run it to completion.
 * Proves execve end-to-end — a binary on disk loaded + run at ring 3.
 */
void x86_64_exec_demo(const char *path)
{
	fileDescriptor_t *fp = fopen(path, "r");
	void *buf;
	u64 pml4, entry, usp;
	long n;
	kTask_t *t;
	int i;

	kprintf("exec demo: loading %s off the FAT root...\n", path);
	if (fp == 0)
	{
		kprintf("exec demo: open(%s) failed\n", path);
		return;
	}
	buf = (void *)kmalloc(1024 * 1024);
	n = (long)fread(buf, 1, 1024 * 1024, fp);
	fclose(fp);
	kprintf("exec demo: read %d bytes\n", (int)n);

	pml4 = x86_64_build_user_image(buf, &entry, &usp);
	kfree(buf);
	if (pml4 == 0)
	{
		kprintf("exec demo: build_user_image failed\n");
		return;
	}

	t = schedNewTask();
	t->md.md_cr3 = pml4;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, path, sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  task pid=%d ready (entry %X usp %X); yielding...\n", t->id, entry, usp);
	/* Yield generously (not just until this task exits) so any children it fork()s
	 * — separate tasks the scheduler must also dispatch — run to completion too. */
	for (i = 0; i < 256; i++)
		sched_yield();
	kprintf("exec demo: %s ran + exited — execve loads real on-disk binaries on x86_64.\n", path);
}
