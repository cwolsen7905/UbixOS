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

#define EXEC_MAX (1024 * 1024) /* cap on an ELF image we'll load from the FS */

/**
 * Load ELF @image into a new user address space, build the minimal SysV initial
 * stack musl's __libc_start_main expects (argc/argv/envp/auxv incl. AT_PAGESZ),
 * schedule it as @name, and spin the cooperative scheduler until it exits.
 *
 * @return the final task state, or -1 if the ELF failed to load.
 */
int aarch64_run_elf_image(const void *image, const char *name)
{
	u_int64_t *l1, entry;
	uintptr_t stack_frame;
	u_int64_t *sp;
	kTask_t *t;
	int i;

	l1 = pmap_create_user_space();
	if (elf64_load(image, l1, &entry) != 0)
		return (-1);

	stack_frame = vmm_find_free_page(sysID);
	sp = (u_int64_t *)(stack_frame + PAGE_SIZE) - INITIAL_FRAME;
	for (i = 0; i < INITIAL_FRAME; i++)
		sp[i] = 0;
	sp[3] = AT_PAGESZ;
	sp[4] = PAGE_SIZE;
	sp[5] = 0;
	pmap_map_user_page(l1, USER_STACK_VA, (u_int64_t)stack_frame, 0);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = USER_STACK_TOP - (u_int64_t)INITIAL_FRAME * 8; /* SP points at argc */
	strncpy(t->name, name, sizeof(t->name) - 1);
	aarch64_console_setup_fds(&t->td); /* stdin/stdout/stderr -> console */
	sched_ready(t);

	for (i = 0; i < 256 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();
	return (t->state);
}

/**
 * Read the ELF at @path off the VFS into a kernel buffer and run it.  Proves the
 * exec-from-filesystem path end-to-end (the shape `execve` will take).
 */
void aarch64_exec_file(const char *path)
{
	fileDescriptor_t *fd;
	char *buf;
	int sz, n, st;

	kprintf("exec: loading %s from the filesystem...\n", path);
	fd = fopen(path, "r");
	if (fd == NULL)
	{
		kprintf("exec: %s not found\n", path);
		return;
	}

	sz = (int)fd->size;
	if (sz <= 0 || sz > EXEC_MAX)
	{
		kprintf("exec: %s has bad size %d\n", path, sz);
		fclose(fd);
		return;
	}

	buf = (char *)kmalloc((u_int32_t)sz);
	if (buf == NULL)
	{
		fclose(fd);
		return;
	}
	n = (int)fread(buf, 1, (size_t)sz, fd);
	fclose(fd);
	kprintf("exec: read %d/%d bytes of %s; loading + running...\n", n, sz, path);

	st = aarch64_run_elf_image(buf, path);
	kfree(buf);
	kprintf("exec: %s returned (state=%d).\n", path, st);
}
