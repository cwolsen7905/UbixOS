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
#include <ubixos/tty.h>
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

		/* lwIP sockets are a single global table; the child now aliases the
		 * same socket, so refcount it — else the first process to close()
		 * destroys the socket out from under the other (breaks forking network
		 * servers like sshd: parent closes the accepted socket after fork). */
		if (parent_f->fd_type == FD_TYPE_SOCKET)
			socket_fork_ref(parent_f->socket);

		/* Likewise a posix_openpt master: dropbear's session child inherits the
		 * master and close()s it; without a ref the pty would be torn down (SIGHUP +
		 * freed) out from under the parent's relay.  The pool slot is in fd->data. */
		if (parent_f->fd_type == FD_TYPE_PTMASTER)
			pty_master_fork_ref((int)(uintptr_t)parent_f->data);
	}
}

/**
 * Inherit the parent (@_current) process context into a forked @child: the QoS
 * floor, cwd, parent pid, process group / session, controlling terminal,
 * attached terminal, and credentials.  The arch-neutral field copies the two
 * fork paths shared.  Pure field assignments with no address-space dependency,
 * so it is safe to call before or after the arch's address-space copy.
 *
 * The child starts at its QoS floor (base_priority), not the parent's possibly
 * boosted current priority.  If the parent owns its attached terminal, the
 * ownership transfers to the child (mirrors the i386 fork's tty-owner handoff).
 */
void proc_fork_inherit_context(kTask_t *child)
{
	child->base_priority = _current->base_priority;
	child->priority = _current->base_priority;

	memcpy(child->oInfo.cwd, _current->oInfo.cwd, sizeof(child->oInfo.cwd));
	child->ppid = _current->id;
	child->pgrp = _current->pgrp;
	child->sid = _current->sid;
	child->ct_tty = _current->ct_tty;

	child->term = _current->term;
	if (_current->term != NULL && _current->term->owner == _current->id)
		_current->term->owner = child->id;

	child->uid = _current->uid;
	child->gid = _current->gid;
}

/**
 * Initialise a forked @child's signal-delivery state from the parent thread
 * @ptd: POSIX requires the child start with no pending signals, but it inherits
 * the parent's signal dispositions (sigact) and signal mask.
 *
 * MUST be called AFTER the arch's address-space copy: on i386 the COW walk can
 * transiently double-map the physical page backing sig_pending (kernel heap and
 * vmm_get_free_kernel_page share the VMM_KERN_START..VMM_KERN_END range),
 * overwriting it with garbage — so the clean slate has to be established last.
 */
void proc_fork_signal_init(kTask_t *child, struct thread *ptd)
{
	child->td.sig_pending = 0;
	memset(child->td.sig_code, 0, sizeof(child->td.sig_code));
	memset(child->td.sig_extra, 0, sizeof(child->td.sig_extra));
	memcpy(child->td.sigact, ptd->sigact, sizeof(child->td.sigact));
	memcpy(&child->td.sigmask, &ptd->sigmask, sizeof(child->td.sigmask));
}
