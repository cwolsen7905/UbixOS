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
#include <ubixos/sched_internal.h> /* taskList — orphan reparenting */
#include <ubixos/wait.h>           /* save_flags/cli/restore_flags */
#include <ubixos/callout.h>        /* callout_stop */
#include <sys/sysproto_posix.h>    /* sys_close */
#include <machine/signal.h>        /* SIGCHLD */

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

	/* Same program until it execs: inherit its name, command line and path
	 * (ps/procfs showed a forked child with an empty name). */
	memcpy(child->name, _current->name, sizeof(child->name));
	memcpy(child->cmdline, _current->cmdline, sizeof(child->cmdline));
	memcpy(child->exe_path, _current->exe_path, sizeof(child->exe_path));
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

/**
 * Exit-time cleanup shared by every architecture's endTask(), run by the dying
 * task @t (== _current) on its own kernel stack.
 *
 * - Cancels its timed-sleep callout, which lives inside the task struct the
 *   reaper is about to free (a killed sleeper would leave it on the callout list).
 * - When @t is the last task of its thread group: closes every open descriptor,
 *   so pipe writer counts drop (readers see EOF) and file structs are released.
 *   Earlier threads only drop their slots — siblings share the o_files objects.
 * - Hands its children to init (pid 1): once @t is reaped its struct is freed,
 *   and the scheduler dereferences a child's ->parent when that child exits.
 *
 * @param last_of_group  non-zero if no other task of @t's tgid is alive.
 */
void proc_exit_cleanup(kTask_t *t, int last_of_group)
{
	kTask_t *init, *c;
	u_int32_t flags;
	int fd;

	callout_stop(&t->sleep_callout);

	for (fd = 0; fd < O_FILES; fd++)
	{
		if (t->td.o_files[fd] == NULL)
			continue;
		if (last_of_group)
		{
			struct sys_close_args ca;
			ca.fd = fd;
			sys_close(&t->td, &ca);
		}
		t->td.o_files[fd] = NULL;
	}

	init = schedFindTask(1);
	if (init == t)
		init = NULL;
	save_flags(flags);
	cli();
	for (c = taskList; c != NULL; c = c->next)
	{
		if (c->parent != t || c == t)
			continue;
		c->parent = init;
		c->ppid = (init != NULL) ? init->id : 0;
		if (init != NULL)
		{
			init->children++;
			/* An already-exited orphan needs init to reap it. */
			if (c->state == ZOMBIE || c->state == DEAD)
				init->td.sig_pending |= (1u << (SIGCHLD - 1));
		}
	}
	restore_flags(flags);
}
