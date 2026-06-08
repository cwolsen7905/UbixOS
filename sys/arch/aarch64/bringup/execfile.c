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
#include <ubixos/wait.h>           /* save_flags/cli/restore_flags */
#include <ubixos/errno.h>          /* ECHILD */
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>
#include <sys/elf_load.h>
#include <sys/elf64.h> /* Elf64_Ehdr — peek the type for the load base */
#include <fs/vfs/file.h>
#include <string.h>

#define USER_STACK_VA 0x100200000UL /* above the program segments (0x100000000+) */
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)
#define INITIAL_FRAME 8 /* u64 slots: argc, argv[0]=NULL, envp[0]=NULL, auxv{6,PAGE_SIZE}{0,0}, pad */
#define AT_PAGESZ 6     /* auxv: aarch64 musl reads PAGE_SIZE from here (else malloc fails) */

#define EXEC_MAX (4 * 1024 * 1024) /* cap on an ELF image we'll load (libc.so ~1 MB) */
#define INITIAL_KSTACK_SIZE 8192   /* kernelStack size (matches schedNewTask kmalloc) */

/* Dynamic-executable layout: a PIE main + the dynamic linker (also PIE) each get
 * their own 1 GB block (user VA must be >= 4 GB — the low 4 GB is the kernel's
 * identity map, see pmap USER_L1_MIN).  brk (0x1C0000000) + mmap (0x200000000)
 * are clear of these. */
#define DYN_MAIN_BASE 0x100000000UL   /* PIE main exe load base   (L1 idx 4) */
#define DYN_INTERP_BASE 0x140000000UL /* dynamic linker load base (L1 idx 5) */
#define DYN_STACK_VA 0x180000000UL    /* initial user stack base  (L1 idx 6) */
#define DYN_STACK_PAGES 64            /* 256 KB stack (busybox et al. need > 1 page) */
#define DYN_STACK_TOP (DYN_STACK_VA + (u_int64_t)DYN_STACK_PAGES * PAGE_SIZE)

/* SysV auxiliary-vector types (Linux/musl ABI — musl's __libc_start_main reads
 * AT_PHDR/PHENT/PHNUM to find the program headers, AT_BASE for the linker's own
 * load address, AT_ENTRY for the main entry, AT_RANDOM for the stack canary). */
#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_BASE 7
#define AT_ENTRY 9
#define AT_RANDOM 25

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
 * Schedule the ELF at @image as @name (a real long-lived task — console fds set
 * up) and turn the calling (boot) thread into the cooperative idle/reaper loop.
 * Unlike run_elf_image this never returns: it is the terminal boot action that
 * hands the machine to the userland init -> login -> shell chain, which drives
 * task switching via the console read's sched_yield (EL0 is IRQ-masked here, so
 * scheduling is cooperative).
 */
static void run_init_image(const void *image, const char *name)
{
	u_int64_t *l1, entry, usp;
	kTask_t *t;

	l1 = build_user_image(image, &entry, &usp);
	if (l1 == NULL)
	{
		kprintf("init: %s failed to load\n", name);
		return;
	}

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, name, sizeof(t->name) - 1);
	aarch64_console_setup_fds(&t->td); /* stdin/stdout/stderr -> console */
	sched_ready(t);

	kprintf("init: %s scheduled as pid %d; entering cooperative idle loop.\n", name, t->id);
	for (;;)
		sched_yield(); /* boot thread is now the idle task; init runs the system */
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
 * Build the SysV initial stack for a dynamically-linked program in @phys_page
 * (the identity-mapped kernel pointer to the stack frame, which is mapped at
 * DYN_STACK_VA in the user space).  Lays down argv[0], a 16-byte AT_RANDOM block,
 * then the argc / argv / envp / auxv vector musl's __libc_start_main + the
 * dynamic linker consume.  All in-stack pointers use the user VA.
 *
 * @return the user-space stack pointer (16-byte aligned, points at argc).
 */
static u_int64_t build_dyn_stack(
    uintptr_t phys_page, u_int64_t page_uva, const char *argv0, const elf64_load_info_t *mi, u_int64_t interp_base)
{
	u_int8_t *page = (u_int8_t *)phys_page;
	u_int8_t *p = page + PAGE_SIZE;
	u_int64_t argv0_uva, random_uva;
	u_int64_t *vec;
	size_t slen = strlen(argv0) + 1;
	int k = 0;
#define UVA(ptr) (page_uva + (u_int64_t)((u_int8_t *)(ptr) - page))

	/* argv[0] string + 16 random bytes for the stack canary, near the top. */
	p -= slen;
	memcpy(p, argv0, slen);
	argv0_uva = UVA(p);

	p -= 16;
	memcpy(p, "uBixOS-aarch64!\x01", 16); /* AT_RANDOM: bring-up entropy (not a CSPRNG yet) */
	random_uva = UVA(p);

	/* The vector: argc, argv[0], NULL, envp NULL, then 8 auxv pairs (incl AT_NULL).
	 * 4 + 16 = 20 u64 = 160 bytes (16-aligned), so SP stays 16-aligned. */
	p = (u_int8_t *)((uintptr_t)p & ~(uintptr_t)15);
	p -= 20 * sizeof(u_int64_t);
	p = (u_int8_t *)((uintptr_t)p & ~(uintptr_t)15);
	vec = (u_int64_t *)p;

	vec[k++] = 1;         /* argc */
	vec[k++] = argv0_uva; /* argv[0] */
	vec[k++] = 0;         /* argv terminator */
	vec[k++] = 0;         /* envp terminator */
	vec[k++] = AT_PHDR;
	vec[k++] = mi->phdr_va;
	vec[k++] = AT_PHENT;
	vec[k++] = mi->phentsize;
	vec[k++] = AT_PHNUM;
	/* If the program headers are not in any PT_LOAD (e.g. a static ET_EXEC linked
	 * high, whose headers sit before the first 64 KB-aligned LOAD), AT_PHDR is
	 * unresolved (0); report 0 headers so musl's phdr walk (TLS setup) skips it
	 * rather than dereferencing a NULL AT_PHDR. */
	vec[k++] = (mi->phdr_va != 0) ? mi->phnum : 0;
	vec[k++] = AT_PAGESZ;
	vec[k++] = PAGE_SIZE;
	vec[k++] = AT_BASE;
	vec[k++] = interp_base; /* dynamic linker load base (0 if statically linked) */
	vec[k++] = AT_ENTRY;
	vec[k++] = mi->entry;
	vec[k++] = AT_RANDOM;
	vec[k++] = random_uva;
	vec[k++] = AT_NULL;
	vec[k++] = 0;

	return UVA(vec);
#undef UVA
}

/**
 * Load a (PIE) dynamically-linked program @image into @l1: map the main exe at
 * DYN_MAIN_BASE, read + map its PT_INTERP dynamic linker at DYN_INTERP_BASE,
 * build the SysV/auxv stack, and report the start PC (the linker's entry) + SP.
 * Also handles a high-linked static ET_EXEC (no PT_INTERP -> jump straight to it).
 *
 * @return 0 on success, -1 on failure.
 */
static int load_dynamic(const void *image, u_int64_t *l1, const char *argv0, u_int64_t *out_entry, u_int64_t *out_usp)
{
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
	elf64_load_info_t mi;
	u_int64_t main_base = (eh->e_type == ET_DYN) ? DYN_MAIN_BASE : 0;
	u_int64_t start_entry, interp_base = 0;

	if (elf64_load_at(image, l1, main_base, &mi) != 0)
		return (-1);
	start_entry = mi.entry;

	if (mi.interp_off != 0)
	{
		const char *interp = (const char *)image + mi.interp_off;
		elf64_load_info_t ii;
		char *ibuf;
		int isz;

		kprintf("dyn: interp = %s\n", interp);
		ibuf = read_elf_file(interp, &isz);
		if (ibuf == NULL)
		{
			kprintf("dyn: cannot load interp %s\n", interp);
			return (-1);
		}
		interp_base = DYN_INTERP_BASE;
		if (elf64_load_at(ibuf, l1, interp_base, &ii) != 0)
		{
			kfree(ibuf);
			return (-1);
		}
		kfree(ibuf);
		start_entry = ii.entry; /* enter the dynamic linker, not the main exe */
	}

	/* Map a multi-page stack; the auxv/argv vector lives in the top page. */
	{
		uintptr_t top_frame = 0;
		int i;
		for (i = 0; i < DYN_STACK_PAGES; i++)
		{
			uintptr_t f = vmm_find_free_page(sysID);
			if (f == 0)
				return (-1);
			memset((void *)f, 0, PAGE_SIZE);
			pmap_map_user_page(l1, DYN_STACK_VA + (u_int64_t)i * PAGE_SIZE, (u_int64_t)f, 0);
			if (i == DYN_STACK_PAGES - 1)
				top_frame = f;
		}
		*out_usp = build_dyn_stack(top_frame, DYN_STACK_TOP - PAGE_SIZE, argv0, &mi, interp_base);
	}

	*out_entry = start_entry;
	return (0);
}

/**
 * Load + run a dynamically-linked program off the VFS as a scheduled task, and
 * cooperatively wait for it (bring-up test of the dynamic-linker path).
 */
void aarch64_run_dynamic(const char *path)
{
	char *buf;
	int sz, i;
	u_int64_t *l1, entry, usp;
	kTask_t *t;

	kprintf("dyn: loading %s (dynamic)...\n", path);
	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("dyn: %s not loadable\n", path);
		return;
	}

	l1 = pmap_create_user_space();
	if (load_dynamic(buf, l1, path, &entry, &usp) != 0)
	{
		kfree(buf);
		kprintf("dyn: %s failed to load\n", path);
		return;
	}
	kfree(buf);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, path, sizeof(t->name) - 1);
	aarch64_console_setup_fds(&t->td);
	sched_ready(t);

	for (i = 0; i < 200000 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();
	kprintf("dyn: %s returned (state=%d).\n", path, t->state);
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
 * Uses the dynamic loader (load_dynamic), so it handles both PIE/dynamic and
 * static images — the real world is dynamically linked, so the shell execs PIE
 * binaries through here.  argv/envp marshalling is the minimal argv[0]=path.
 *
 * @return -1 on any failure to load (on success it does not return).
 */
int aarch64_exec_replace(const char *path)
{
	char *buf;
	int sz;
	u_int64_t *l1, entry, usp, kstack_top;
	char namebuf[256];

	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("execve: %s not loadable\n", path);
		return (-1);
	}

	/* Copy the name out of the old address space now — @path is a user pointer
	 * there and the old AS is unmapped the instant we switch TTBR0. */
	strncpy(namebuf, path, sizeof(namebuf) - 1);
	namebuf[sizeof(namebuf) - 1] = '\0';

	l1 = pmap_create_user_space();
	if (load_dynamic(buf, l1, namebuf, &entry, &usp) != 0)
	{
		kfree(buf);
		kprintf("execve: %s failed to load\n", path);
		return (-1);
	}
	kfree(buf);

	/* Repoint the current task at the new image, then make it live. */
	strncpy(_current->name, namebuf, sizeof(_current->name) - 1);
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
 * Scan the current task's children for an exited one (any if @want_pid == -1);
 * if found, collect it (splice from taskList, move to the del list, decrement
 * the child count) and return it.  Runs with IRQs masked so it is atomic with
 * respect to the timer-driven sched() reaper (which may transition a child
 * ZOMBIE->DEAD + notify us concurrently on a different scheduling quantum).
 *
 * @param have_child  out: set non-zero if a matching child exists at all (used
 *                    to distinguish "no such child" -> ECHILD from "not yet").
 * @return the reaped child, or NULL if none has exited yet.
 */
static kTask_t *find_and_reap_child(int want_pid, int *have_child)
{
	u_int32_t flags;
	kTask_t *t, *found = NULL;

	*have_child = 0;
	save_flags(flags);
	cli();
	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->parent != _current)
			continue;
		if (want_pid != -1 && (int)t->id != want_pid)
			continue;
		*have_child = 1;
		if (t->state == DEAD || t->state == ZOMBIE)
		{
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
			found = t;
			break;
		}
	}
	restore_flags(flags);
	return (found);
}

/**
 * wait4(@want_pid, @status): block until a child (any if @want_pid == -1) exits,
 * then reap it.  Mirrors the generic sys_wait4 blocking protocol: sleep in the
 * WAIT state (off the run queue) so the timer-driven sched() reaper wakes us
 * (WAIT->READY) when a child goes ZOMBIE — busy-yielding instead would leave us
 * runnable and the reaper's wake would double-enqueue us, corrupting the run
 * queue under preemption.  The re-scan after sched_sleep closes the lost-wakeup
 * window (child exited between our scan and the sleep).
 *
 * @return the reaped child's pid, or -ECHILD if there is no such child.
 */
int aarch64_wait4(int want_pid, int *status)
{
	for (;;)
	{
		int have_child = 0;
		kTask_t *child = find_and_reap_child(want_pid, &have_child);
		if (child != NULL)
		{
			/* Guard against pidStatus(pid) (ubix_api), which shares SVC #7 with
			 * wait4 but passes only x0 — x1 (status) is garbage; only write a
			 * pointer that is in the user VA range. */
			if (status != NULL && (uintptr_t)status >= 0x100000000UL)
				*status = 0; /* exit code not yet propagated; report 0 */
			return ((int)child->id);
		}
		if (!have_child)
			return (-ECHILD);

		/* Block until a child exits.  Re-check after sleeping (the reaper may
		 * have flagged the child between our scan and the sleep). */
		sched_sleep(_current, WAIT);
		child = find_and_reap_child(want_pid, &have_child);
		if (child != NULL)
		{
			sched_wakeup(_current);
			/* Guard against pidStatus(pid) (ubix_api), which shares SVC #7 with
			 * wait4 but passes only x0 — x1 (status) is garbage; only write a
			 * pointer that is in the user VA range. */
			if (status != NULL && (uintptr_t)status >= 0x100000000UL)
				*status = 0;
			return ((int)child->id);
		}
		sched_yield();
		sched_wakeup(_current);
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

/**
 * Read the ELF at @path off the VFS and run it as the system's init: schedule
 * it, then become the cooperative idle loop (never returns).  The terminal boot
 * action — init forks login, login execs the shell, all off the ramfs root.
 */
void aarch64_run_init(const char *path)
{
	char *buf;
	int sz;

	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("init: %s not loadable\n", path);
		return;
	}
	run_init_image(buf, path);
	kfree(buf); /* only reached if the image failed to load */
}
