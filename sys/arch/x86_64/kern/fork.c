/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 fork(2) (Phase 5e).  Creates a child task that shares the parent's user
 * pages copy-on-write (PTE_COW + the MI COW refcount; the first writer of either
 * side faults into x86_64_cow_fault for a private copy) and a duplicate of the
 * fork-syscall trapframe (with rax = 0 so the child's fork returns 0).  Wired
 * pages (framebuffer / shared window buffers) are shared verbatim, MMIO is
 * skipped.  The child's kernel stack is seeded so
 * its first dispatch lands in ret_from_fork (syscall_entry.S) — the shared
 * pop-and-SYSRETQ tail — resuming it in ring 3 at the fork return point.  Sibling
 * of aarch64's aarch64_fork (kern/proc.c).
 */

#include "../x86_64.h"
#include <ubixos/sched.h>
#include <sys/thread.h>
#include <vmm/vmm.h>     /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>

#define PTE_P 0x1
#define PTE_RW 0x2
#define PTE_US 0x4
#define PTE_PS 0x80      /* 2 MB page (skip — kernel identity, never copied) */
#define PTE_WIRED 0x200  /* available bit 9: share verbatim across fork (no copy) */
#define PTE_COW 0x400    /* available bit 10: copy-on-write (frame shared read-only) */
#define PTE_SHARED 0x800 /* available bit 11: shared file-cache page (vm_filecache-owned) */
#define PTE_ADDR_MASK (~0xFFFUL)
#define FRAME_SLOTS 7 /* 6 callee-saved + return address (matches cpu_switch.S) */

extern void ret_from_fork(void);
extern void x86_64_map_user_page_wired(uintptr_t pml4_phys, u64 va, u64 phys);
extern void x86_64_map_user_page_cow(u64 pml4_phys, u64 va, u64 phys);
extern void x86_64_map_user_page_shared(u64 pml4_phys, u64 va, u64 phys);
extern void x86_64_tlb_shootdown(u64 pml4_phys, u64 va);
extern int adjust_cow_counter(uintptr_t base_addr, int adjustment);
extern int vm_filecache_ref_phys(u_int32_t phys);
extern u_int32_t numPages; /* RAM page count; frame >= numPages => MMIO (vmm.h) */

/**
 * Copy-on-write the parent's USER pages (the private PDPT[1..] region under
 * PML4[0], i.e. VAs >= 1 GB) into a fresh address space: both spaces reference the
 * same frames read-only until one writes.  The kernel low-1 GB identity (PDPT[0]) +
 * the kernel higher half are shared by create_user_space; only the user leaves are
 * re-mapped.  @return the child PML4 (physical), or 0.
 */
static u64 x86_64_fork_copy(u64 parent_pml4)
{
	u64 child = x86_64_create_user_space();
	u64 *ppdpt, *ppd, *ppt;
	unsigned pi, di, ti;

	if (child == 0)
		return 0;

	ppdpt = (u64 *)P2V(((u64 *)P2V(parent_pml4))[0] & PTE_ADDR_MASK);
	for (pi = 1; pi < 512; pi++) /* skip PDPT[0] (shared kernel identity) */
	{
		if ((ppdpt[pi] & PTE_P) == 0)
			continue;
		ppd = (u64 *)P2V(ppdpt[pi] & PTE_ADDR_MASK);
		for (di = 0; di < 512; di++)
		{
			if ((ppd[di] & PTE_P) == 0 || (ppd[di] & PTE_PS))
				continue;
			ppt = (u64 *)P2V(ppd[di] & PTE_ADDR_MASK);
			for (ti = 0; ti < 512; ti++)
			{
				u64 pte = ppt[ti];
				u64 va, phys;
				if ((pte & PTE_P) == 0 || (pte & PTE_US) == 0)
					continue;
				va = ((u64)pi << 30) | ((u64)di << 21) | ((u64)ti << 12);
				phys = pte & PTE_ADDR_MASK;

				/* MMIO (frame >= RAM, e.g. the framebuffer BAR): the child does NOT
				 * inherit device memory — only the process that mapped it (views) uses
				 * the live scanout, and an MMIO frame has no physmap (P2V) entry to
				 * copy from.  Skip it. */
				if ((phys >> 12) >= numPages)
					continue;

				/* Wired (PTE_WIRED) pages — a shared window buffer — are SHARED
				 * verbatim, not copied: a copy would split the buffer off its other
				 * end (compositor <-> client).  Map the same physical frame. */
				if (pte & PTE_WIRED)
				{
					x86_64_map_user_page_wired(child, va, phys);
					continue;
				}

				/* Shared file-cache page (a library's text/rodata): the child maps the
				 * same frame read-only and takes its own cache reference — no COW, a
				 * writer of either side copies out via x86_64_cow_fault's PTE_SHARED
				 * path. */
				if (pte & PTE_SHARED)
				{
					x86_64_map_user_page_shared(child, va, phys);
					vm_filecache_ref_phys((u_int32_t)phys);
					continue;
				}

				/* Copy-on-write: share the frame read-only in BOTH parent and child;
				 * the first writer faults into x86_64_cow_fault for a private copy.
				 * Mark the parent RO+COW the first time the frame is shared, then bump
				 * the COW refcount by 2 (parent + child) — or by 1 if it was already
				 * COW from an earlier fork.  Matches i386/aarch64's COW convention. */
				{
					int already = (pte & PTE_COW) != 0;
					if (!already)
					{
						ppt[ti] = (pte & ~(u64)PTE_RW) | PTE_COW; /* parent: RO + COW */
						/* SMP: a sibling thread of a multi-threaded parent may be running
						 * this same cr3 on another CPU with the page still cached RW —
						 * shoot it down so its next write COW-faults instead of scribbling
						 * the now-shared frame. */
						x86_64_tlb_shootdown(parent_pml4, va);
					}
					x86_64_map_user_page_cow(child, va, phys); /* child: RO + COW, same frame */
					adjust_cow_counter((uintptr_t)phys, already ? 1 : 2);
				}
			}
		}
	}
	return child;
}

/**
 * Common fork/clone body: COW-copy the caller's address space, copy the fd table +
 * inherit context, and resume the child at the caller's return point with rax = 0.
 * @child_sp is the child's user stack: the caller's own RSP for fork() (the copied
 * stack), or the caller-supplied clone stack for clone() (so musl clone.s's child
 * path runs the thread function).
 *
 * @return the child's pid to the caller, or -1 on failure.
 */
static long x86_64_fork_common(struct x86_64_trapframe *parent_tf, u64 child_sp)
{
	u64 child_pml4 = x86_64_fork_copy(_current->md.md_cr3);
	kTask_t *child;
	u8 *top;
	struct x86_64_trapframe *ctf;
	u64 *ctx;
	int i;

	if (child_pml4 == 0)
		return -1;

	child = schedNewTask();
	child->md.md_cr3 = child_pml4;
	child->md.md_usp = child_sp; /* fork: copied parent SP; clone: caller's clone stack */
	child->md.md_entry = 0;      /* unused — the trapframe drives the return */
	/* Inherit the parent's TLS base + heap/mmap cursors: the child runs the parent's
	 * image (same code/data, so the same FS.base-relative TLS) until it execve's. */
	child->md.md_fsbase = _current->md.md_fsbase;
	child->md.md_mmap_next = _current->md.md_mmap_next;
	child->md.md_brk = _current->md.md_brk;
	child->parent = _current;
	_current->children++;

	/* Deep-copy the open-file table and inherit cwd / pgrp / controlling tty /
	 * creds / QoS, then reset the child's signal state — via the MI fork helpers
	 * (kern/fork.c), exactly as i386 and aarch64 do.  A shallow o_files *pointer*
	 * copy is unsafe: the child would share the parent's struct file objects, so
	 * when an ancestor (e.g. vlogin) closes its fds or exits and kfree()s them, the
	 * child is left holding dangling pointers and faults (#GP) on its next read/
	 * close.  fork_copy_fdtable gives the child its own copies. */
	fork_copy_fdtable(child, &_current->td);
	proc_fork_inherit_context(child);
	proc_fork_signal_init(child, &_current->td);

	/* Child kernel stack: a copy of the parent trapframe at the top (rax = 0), and
	 * below it a ctx frame whose return address is ret_from_fork. */
	top = (u8 *)child->kernelStack + 65536; /* KSTACK_SIZE (schedNewTask) */
	ctf = (struct x86_64_trapframe *)(top - sizeof(struct x86_64_trapframe));
	memcpy(ctf, parent_tf, sizeof(struct x86_64_trapframe));
	ctf->rax = 0;        /* the child's fork()/clone() returns 0 */
	ctf->rsp = child_sp; /* fork: unchanged (copied SP); clone: the clone stack */

	ctx = (u64 *)((u8 *)ctf - FRAME_SLOTS * 8);
	for (i = 0; i < FRAME_SLOTS; i++)
		ctx[i] = 0;
	ctx[FRAME_SLOTS - 1] = (u64)(uintptr_t)ret_from_fork; /* RET target after the 6 pops */

	child->md.md_kstack = (u64)(uintptr_t)ctx; /* set => sched_ready skips md_setup_initial_frame */
	sched_ready(child);

	return child->id;
}

/**
 * fork(2): COW-copy the caller's address space; the child resumes at the fork
 * return point with rax = 0 on its (copied) stack.
 */
long x86_64_fork(struct x86_64_trapframe *parent_tf)
{
	return x86_64_fork_common(parent_tf, parent_tf->rsp);
}

/**
 * clone() with a private address space (musl posix_spawn: CLONE_VM|CLONE_VFORK,
 * no CLONE_THREAD/CLONE_FILES).  Unlike x86_64_rfork (which shares the AS + fds for
 * pthreads), the child gets a COW copy + a copied fd table on the caller-supplied
 * clone stack (rsi), so its file-actions + execve never disturb the parent — the
 * posix_spawn child sets up fds then execve()s.
 */
long x86_64_clone(struct x86_64_trapframe *parent_tf)
{
	return x86_64_fork_common(parent_tf, parent_tf->rsi /* clone stack */);
}

/**
 * rfork(RFMEM) — thread create: a new task SHARING the caller's address space
 * (same CR3, not a copy), running on a caller-supplied user stack, resuming at the
 * caller's rfork() return point with rax = 0 — so musl's clone.s child path runs
 * the thread function.  It joins the caller's thread group (tgid), so endTask frees
 * the shared address space only when the last task of the group exits.
 *
 * SysV args (musl clone.s -> rfork, FreeBSD slot 251): rdi = flags (informational;
 * v1 always shares the AS), rsi = child user-stack top, rdx = TLS base (0 if none).
 *
 * @return the new thread's tid to the caller; the new thread itself sees 0.
 */
long x86_64_rfork(struct x86_64_trapframe *parent_tf)
{
	u64 child_stack = parent_tf->rsi;
	u64 tls = parent_tf->rdx;
	kTask_t *child;
	u8 *top;
	struct x86_64_trapframe *ctf;
	u64 *ctx;
	int i;

	child = schedNewTask();

	/* SHARE the caller's address space (same PML4) — the defining difference from
	 * fork(), which copies it. */
	child->md.md_cr3 = _current->md.md_cr3;
	child->md.md_usp = child_stack;
	child->md.md_entry = 0; /* unused — the trapframe drives the return */
	child->md.md_fsbase = tls != 0 ? tls : _current->md.md_fsbase;
	child->md.md_mmap_next = _current->md.md_mmap_next;
	child->md.md_brk = _current->md.md_brk;
	child->parent = _current;
	_current->children++;

	/* Same thread group => shared AS; endTask's sched_tgid_others_alive() check
	 * frees the address space only when the LAST thread of the group exits. */
	child->tgid = _current->tgid;

	/* Shallow-share the fd table: threads share open files.  Free the placeholder
	 * fds schedNewTask() made for 0-2, then point at the caller's file objects.
	 * (x86_64 uses the MI endTask, which does not close/free fds, so a non-last
	 * thread exiting cannot drop a shared file out from under its siblings.) */
	for (i = 0; i < O_FILES; i++)
	{
		if (child->td.o_files[i] != NULL)
		{
			kfree(child->td.o_files[i]);
			child->td.o_files[i] = NULL;
		}
		child->td.o_files[i] = _current->td.o_files[i];
	}

	proc_fork_inherit_context(child);
	proc_fork_signal_init(child, &_current->td);

	/* Child kernel stack: the caller's trapframe with rax = 0 (the child's rfork
	 * return), its own user stack + a fresh frame base; below it a ctx frame whose
	 * return address is ret_from_fork (the shared pop-and-SYSRETQ tail to ring 3). */
	top = (u8 *)child->kernelStack + 65536; /* KSTACK_SIZE (schedNewTask) */
	ctf = (struct x86_64_trapframe *)(top - sizeof(struct x86_64_trapframe));
	memcpy(ctf, parent_tf, sizeof(struct x86_64_trapframe));
	ctf->rax = 0;           /* the new thread sees rfork() == 0 */
	ctf->rsp = child_stack; /* its own user stack */
	ctf->rbp = 0;           /* fresh frame base */

	ctx = (u64 *)((u8 *)ctf - FRAME_SLOTS * 8);
	for (i = 0; i < FRAME_SLOTS; i++)
		ctx[i] = 0;
	ctx[FRAME_SLOTS - 1] = (u64)(uintptr_t)ret_from_fork;

	child->md.md_kstack = (u64)(uintptr_t)ctx;
	sched_ready(child);

	return child->id;
}
