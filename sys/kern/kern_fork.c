/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-independent fork helpers shared by the per-arch fork paths
 * (i386 sys_fork in arch/i386/kern/fork.c, aarch64_fork in
 * arch/aarch64/kern/proc.c).  The address-space copy and the trap/context
 * frame setup stay arch-specific; the open-file-table duplication below is
 * neutral and was previously copy-pasted into both.  Part of the Phase 5
 * convergence work (docs/design/console-and-arch-convergence-plan.md).
 */

#include <ubixos/sched.h>
#include <sys/thread.h>
#include <sys/descrip.h>
#include <sys/pipe.h>
#include <lib/kmalloc.h>
#include <string.h>

/**
 * Duplicate the parent thread's open-file table into a freshly-created child
 * task (the POSIX fork() semantics: the child gets independent fd slots that
 * reference copies of the parent's open files).
 *
 * schedNewTask() pre-seeds placeholder file structs in slots 0-2; those are
 * freed first so every inherited fd is a true copy.  Each parent fd is deep
 * copied: the struct file, its underlying fileDescriptor_t (if any), and that
 * descriptor's 4 KB buffer.  Pipe ends are shared across the fork, so the
 * matching read/write refcount on the shared pipeInfo is bumped.
 */
void fork_copy_fdtable(kTask_t *child, struct thread *ptd)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (child->td.o_files[i] != 0x0)
		{
			kfree(child->td.o_files[i]);
			child->td.o_files[i] = 0x0;
		}
	}

	for (i = 0; i < O_FILES; i++)
	{
		struct file *parent_f = (struct file *)ptd->o_files[i];
		struct file *child_f;

		if (parent_f == 0x0)
			continue;

		child_f = (struct file *)kmalloc(sizeof(struct file));
		if (child_f == 0x0)
			continue;
		memcpy(child_f, parent_f, sizeof(struct file));
		child->td.o_files[i] = (void *)child_f;

		if (parent_f->fd != 0x0)
		{
			child_f->fd = (fileDescriptor_t *)kmalloc(sizeof(fileDescriptor_t));
			if (child_f->fd != 0x0)
			{
				memcpy(child_f->fd, parent_f->fd, sizeof(fileDescriptor_t));
				if (parent_f->fd->buffer != 0x0)
				{
					child_f->fd->buffer = kmalloc(4096);
					if (child_f->fd->buffer != 0x0)
						memcpy(child_f->fd->buffer, parent_f->fd->buffer, 4096);
				}
			}
		}

		/* Pipes share their pipeInfo across processes; bump the matching
		 * refcount so neither end is freed while the child still references it. */
		if (parent_f->fd_type == FD_TYPE_PIPE && parent_f->data != 0x0)
		{
			struct pipeInfo *pi = (struct pipeInfo *)parent_f->data;
			if (parent_f->pipe_end == PIPE_END_READ)
				pi->rfdCNT++;
			else if (parent_f->pipe_end == PIPE_END_WRITE)
				pi->wfdCNT++;
		}
	}
}
