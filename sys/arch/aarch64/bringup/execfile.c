/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * exec-from-file (aarch64 bring-up): load an ELF *from a filesystem path* (not an
 * embedded blob) into a fresh user address space and run it.  This is the piece
 * the bootstrap needs — mount a root fs, then exec /bin/init off it.  The ELF
 * loader (elf64_load) and the SysV initial-stack setup are shared with the
 * embedded-program demos via aarch64_run_elf_image().
 */
#include "bringup.h"
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h> /* taskList, pid_hash_remove */
#include <ubixos/errno.h>          /* ECHILD */
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>
#include <sys/elf_load.h>
#include <fs/vfs/file.h>
#include <string.h>

#define USER_STACK_VA 0x100200000UL /* above the program segments (0x100000000+) */
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)
#define INITIAL_FRAME 8 /* u64 slots: argc, argv[0]=NULL, envp[0]=NULL, auxv{6,PAGE_SIZE}{0,0}, pad */
#define AT_PAGESZ 6     /* auxv: aarch64 musl reads PAGE_SIZE from here (else malloc fails) */

#define EXEC_MAX (1024 * 1024)   /* cap on an ELF image we'll load from the FS */
#define INITIAL_KSTACK_SIZE 8192 /* kernelStack size (matches schedNewTask kmalloc) */

/**
 * Load ELF @image into a fresh user address space and build the minimal SysV
 * initial stack musl's __libc_start_main expects (argc/argv/envp/auxv incl.
 * AT_PAGESZ — aarch64 musl reads PAGE_SIZE from there or malloc fails).  Shared
 * by run_elf_image (new task) and exec_replace (current task), so both lay out
 * the user image identically.
 *
 * @param out_entry  receives the ELF entry VA.
 * @param out_usp    receives the initial EL0 stack pointer (points at argc).
 * @return the new L1 page-table root, or NULL if the ELF failed to load.
 */
static u_int64_t *build_user_image(const void *image, u_int64_t *out_entry, u_int64_t *out_usp)
{
	u_int64_t *l1, entry;
	uintptr_t stack_frame;
	u_int64_t *sp;
	int i;

	l1 = pmap_create_user_space();
	if (elf64_load(image, l1, &entry) != 0)
		return (NULL);

	stack_frame = vmm_find_free_page(sysID);
	sp = (u_int64_t *)(stack_frame + PAGE_SIZE) - INITIAL_FRAME;
	for (i = 0; i < INITIAL_FRAME; i++)
		sp[i] = 0;
	sp[3] = AT_PAGESZ;
	sp[4] = PAGE_SIZE;
	sp[5] = 0;
	pmap_map_user_page(l1, USER_STACK_VA, (u_int64_t)stack_frame, 0);

	*out_entry = entry;
	*out_usp = USER_STACK_TOP - (u_int64_t)INITIAL_FRAME * 8; /* SP points at argc */
	return (l1);
}

/**
 * Load ELF @image into a new user address space, schedule it as @name, and spin
 * the cooperative scheduler until it exits.
 *
 * @return the final task state, or -1 if the ELF failed to load.
 */
int aarch64_run_elf_image(const void *image, const char *name)
{
	u_int64_t *l1, entry, usp;
	kTask_t *t;
	int i;

	l1 = build_user_image(image, &entry, &usp);
	if (l1 == NULL)
		return (-1);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, name, sizeof(t->name) - 1);
	aarch64_console_setup_fds(&t->td); /* stdin/stdout/stderr -> console */
	sched_ready(t);

	for (i = 0; i < 256 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();
	return (t->state);
}

/**
 * Read the ELF at @path off the VFS into a freshly kmalloc'd kernel buffer.
 * Shared by exec_file (new task) and exec_replace (execve): the file is read
 * while the *old* address space is still active, before any TTBR0 switch.
 *
 * @param out_size  receives the byte count read.
 * @return the kernel buffer (caller kfrees), or NULL on any error.
 */
static char *read_elf_file(const char *path, int *out_size)
{
	fileDescriptor_t *fd;
	char *buf;
	int sz, n;

	fd = fopen(path, "r");
	if (fd == NULL)
		return (NULL);

	sz = (int)fd->size;
	if (sz <= 0 || sz > EXEC_MAX)
	{
		fclose(fd);
		return (NULL);
	}

	buf = (char *)kmalloc((u_int32_t)sz);
	if (buf == NULL)
	{
		fclose(fd);
		return (NULL);
	}
	n = (int)fread(buf, 1, (size_t)sz, fd);
	fclose(fd);
	if (n != sz)
	{
		kfree(buf);
		return (NULL);
	}

	*out_size = sz;
	return (buf);
}

/**
 * execve(@path): replace the *current* task's image with the ELF at @path and
 * restart it at EL0 — the real exec, as opposed to run_elf_image's new task.
 *
 * Reads the file (path + VFS I/O run in the old address space) into a kernel
 * buffer, builds the new image in a fresh address space, repoints the current
 * task's md state at it, switches TTBR0, then aarch64_exec_to_el0()s into the
 * new entry — which does not return.  The old address space's user pages leak
 * for now (a pmap_destroy is the follow-up); the kernel stack is reused.
 *
 * argv/envp are not yet marshalled onto the new stack (the initial frame is the
 * minimal argc=0 + AT_PAGESZ auxv); programs that read arguments come with the
 * dynamic-linker work.
 *
 * @return -1 on any failure to load (on success it does not return).
 */
int aarch64_exec_replace(const char *path)
{
	char *buf;
	int sz;
	u_int64_t *l1, entry, usp, kstack_top;

	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("execve: %s not loadable\n", path);
		return (-1);
	}

	l1 = build_user_image(buf, &entry, &usp);
	kfree(buf);
	if (l1 == NULL)
		return (-1);

	/* Repoint the current task at the new image, then make it live.  Set the
	 * name *before* pmap_switch — @path is a user pointer in the old address
	 * space, which becomes unmapped the instant we load the new TTBR0. */
	strncpy(_current->name, path, sizeof(_current->name) - 1);
	_current->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	_current->md.md_entry = entry;
	_current->md.md_usp = usp;
	_current->md.md_mmap_next = 0; /* fresh mmap/brk regions for the new image */
	_current->md.md_brk = 0;
	pmap_switch(l1);

	kstack_top = (u_int64_t)(uintptr_t)((u_int8_t *)_current->kernelStack + INITIAL_KSTACK_SIZE);
	aarch64_exec_to_el0(entry, usp, kstack_top); /* does not return */
	return (-1);                                 /* unreachable */
}

/**
 * wait4(@want_pid, @status): cooperative reap.  Scan for a child of the current
 * task (any child if @want_pid == -1) that has exited (ZOMBIE/DEAD); collect it
 * (splice from taskList, free) and return its pid.  If a matching child exists
 * but is still running, sched_yield() and retry — EL0 tasks run cooperatively
 * here (IRQ-masked), so yielding lets the child run to exit.
 *
 * @return the reaped child's pid, or -ECHILD if there is no such child.
 */
int aarch64_wait4(int want_pid, int *status)
{
	kTask_t *t;
	int have_child;

	for (;;)
	{
		have_child = 0;
		for (t = taskList; t != NULL; t = t->next)
		{
			if (t->parent != _current)
				continue;
			if (want_pid != -1 && (int)t->id != want_pid)
				continue;
			have_child = 1;
			if (t->state == DEAD || t->state == ZOMBIE)
			{
				int pid = (int)t->id;

				if (status != NULL)
					*status = 0; /* exit code not yet propagated; report 0 */
				if (t->prev != NULL)
					t->prev->next = t->next;
				else
					taskList = t->next;
				if (t->next != NULL)
					t->next->prev = t->prev;
				pid_hash_remove(t);
				sched_addDelTask(t);
				if (_current->children > 0)
					_current->children--;
				return (pid);
			}
		}
		if (!have_child)
			return (-ECHILD);
		sched_yield(); /* let the still-running child make progress */
	}
}

/**
 * Read the ELF at @path off the VFS into a kernel buffer and run it.  Proves the
 * exec-from-filesystem path end-to-end (the shape `execve` will take).
 */
void aarch64_exec_file(const char *path)
{
	char *buf;
	int sz, st;

	kprintf("exec: loading %s from the filesystem...\n", path);
	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("exec: %s not loadable\n", path);
		return;
	}
	kprintf("exec: read %d bytes of %s; loading + running...\n", sz, path);

	st = aarch64_run_elf_image(buf, path);
	kfree(buf);
	kprintf("exec: %s returned (state=%d).\n", path, st);
}
