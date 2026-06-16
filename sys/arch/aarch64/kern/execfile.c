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
#include <ubixos/exec.h>           /* exec_set_name_cmdline (MI exec helper) */
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

/* User-VA layout (USER_STACK_*, DYN_MAIN_BASE/INTERP_BASE/STACK_*) lives in
 * <aarch64/vmm_layout.h> — the single source of truth shared with kern/syscall.c
 * (MMAP_BASE/BRK_BASE). */
#include <aarch64/vmm_layout.h>

#define INITIAL_FRAME 8 /* u64 slots: argc, argv[0]=NULL, envp[0]=NULL, auxv{6,PAGE_SIZE}{0,0}, pad */
#define AT_PAGESZ 6     /* auxv: aarch64 musl reads PAGE_SIZE from here (else malloc fails) */

#define EXEC_MAX (4 * 1024 * 1024) /* cap on an ELF image we'll load (libc.so ~1 MB) */
#define INITIAL_KSTACK_SIZE 65536  /* 64 KB kernelStack (matches schedNewTask kmalloc + KSTACK_SIZE) */

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

#define MAXARG 48 /* cap on argv/envp entries marshalled into a new image */

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
 * Schedule a static embedded ELF as a long-lived background task (console fds
 * set up) and return immediately — used to start a daemon (e.g. authd) before
 * handing the foreground to login/shell.
 */
void aarch64_spawn_elf_image(const void *image, const char *name)
{
	u_int64_t *l1, entry, usp;
	kTask_t *t;

	l1 = build_user_image(image, &entry, &usp);
	if (l1 == NULL)
	{
		kprintf("spawn: %s failed to load\n", name);
		return;
	}
	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, name, sizeof(t->name) - 1);
	aarch64_console_setup_fds(&t->td);
	sched_ready(t);
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
	/* The boot thread is now purely the idle thread: tag it so sched_account_tick
	 * buckets its ticks as idle (not busy) and procfs reports it at 0% CPU. */
	g_idle_task = _current;

	/* smp-plan M3: the scheduler is up and init is scheduled — create each AP's
	 * per-CPU idle task and release the APs into the shared run queue.  From here
	 * they pull READY tasks alongside the BSP (true SMP). */
	aarch64_smp_release_aps();

	for (;;)
	{
		sched_yield();           /* boot thread is now the idle task; init runs the system */
		__asm__ volatile("wfi"); /* nothing runnable: halt until the next interrupt, not spin */
	}
}

/**
 * Read the ELF at @path off the VFS into a freshly kmalloc'd kernel buffer.
 * Shared by exec_file (new task) and exec_replace (execve): the file is read
 * while the *old* address space is still active, before any TTBR0 switch.
 *
 * @param out_size  receives the byte count read.
 * @return the kernel buffer (caller kfrees), or NULL on any error.
 */
/**
 * Does @path exist on the mounted root?  Used by the boot profile selector
 * (desktop vs base console) to branch on whether the desktop is staged.
 *
 * @return 1 if the file opens, 0 otherwise.
 */
int aarch64_file_exists(const char *path)
{
	fileDescriptor_t *fd = fopen(path, "r");

	if (fd == NULL)
		return (0);
	fclose(fd);
	return (1);
}

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
static u_int64_t build_dyn_stack(uintptr_t phys_page,
                                 u_int64_t page_uva,
                                 char **argv,
                                 int argc,
                                 char **envp,
                                 int envc,
                                 const elf64_load_info_t *mi,
                                 u_int64_t interp_base,
                                 u_int64_t limit)
{
	u_int8_t *page = (u_int8_t *)phys_page;
	u_int8_t *p = page + PAGE_SIZE;
	u_int64_t argv_uva[MAXARG], envp_uva[MAXARG], random_uva;
	u_int64_t *vec;
	int i, k = 0, nvec;
	size_t need;
#define UVA(ptr) (page_uva + (u_int64_t)((u_int8_t *)(ptr) - page))

	/* The argv/env strings + the AT_RANDOM block + the argc/argv/envp/auxv vector are
	 * laid down below the top of the stack, descending from @phys_page's top.  @limit
	 * is how far down that may go (the reserved, physically-contiguous arg region);
	 * size it up front and bail cleanly on overflow rather than scribbling past the
	 * region into adjacent kernel memory (a huge environment must fail the exec). */
	nvec = 1 + argc + 1 + envc + 1 + 16;
	need = 16 + 32 /* AT_RANDOM + two 16-byte alignment slacks */ + (size_t)nvec * sizeof(u_int64_t);
	for (i = 0; i < argc; i++)
		need += strlen(argv[i]) + 1;
	for (i = 0; i < envc; i++)
		need += strlen(envp[i]) + 1;
	if (need > limit)
	{
		kprintf("execve: argv+env (%u bytes) exceeds the %u-byte stack arg region\n",
		        (u_int32_t)need,
		        (u_int32_t)limit);
		return (0);
	}

	/* argv + envp strings, then 16 random bytes for the stack canary, near top. */
	for (i = 0; i < argc; i++)
	{
		size_t l = strlen(argv[i]) + 1;
		p -= l;
		memcpy(p, argv[i], l);
		argv_uva[i] = UVA(p);
	}
	for (i = 0; i < envc; i++)
	{
		size_t l = strlen(envp[i]) + 1;
		p -= l;
		memcpy(p, envp[i], l);
		envp_uva[i] = UVA(p);
	}

	p -= 16;
	memcpy(p, "uBixOS-aarch64!\x01", 16); /* AT_RANDOM: bring-up entropy (not a CSPRNG yet) */
	random_uva = UVA(p);

	/* Vector: argc, argv[..], NULL, envp[..], NULL, then 8 auxv pairs.  Align the
	 * base down to 16 so SP is 16-byte aligned at the program entry. */
	nvec = 1 + argc + 1 + envc + 1 + 16;
	p = (u_int8_t *)((uintptr_t)p & ~(uintptr_t)15);
	p -= (u_int64_t)nvec * sizeof(u_int64_t);
	p = (u_int8_t *)((uintptr_t)p & ~(uintptr_t)15);
	vec = (u_int64_t *)p;

	vec[k++] = (u_int64_t)argc;
	for (i = 0; i < argc; i++)
		vec[k++] = argv_uva[i];
	vec[k++] = 0; /* argv terminator */
	for (i = 0; i < envc; i++)
		vec[k++] = envp_uva[i];
	vec[k++] = 0; /* envp terminator */
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
static int load_dynamic(const void *image,
                        u_int64_t *l1,
                        char **argv,
                        int argc,
                        char **envp,
                        int envc,
                        u_int64_t *out_entry,
                        u_int64_t *out_usp)
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

	/* Allocate the whole stack as one *contiguous* run of frames.  The argv/env/auxv
	 * vector is laid down from the top descending; contiguous frames keep both the
	 * (identity-mapped) kernel view and the user VA linear, so the vector can span
	 * multiple pages (the pointer math in build_dyn_stack stays valid across page
	 * boundaries) instead of being capped at the single top page.  A contiguous-alloc
	 * failure fails the exec cleanly rather than corrupting. */
	{
		uintptr_t stack_base;
		int i;

		stack_base = vmm_find_free_pages_contig(DYN_STACK_PAGES, sysID);
		if (stack_base == 0)
			return (-1);
		memset((void *)stack_base, 0, (size_t)DYN_STACK_PAGES * PAGE_SIZE);
		for (i = 0; i < DYN_STACK_PAGES; i++)
			pmap_map_user_page(l1,
			                   DYN_STACK_VA + (u_int64_t)i * PAGE_SIZE,
			                   (u_int64_t)(stack_base + (uintptr_t)i * PAGE_SIZE),
			                   0);

		/* build_dyn_stack writes top-down from the topmost page.  Reserve the lower
		 * half for runtime growth; the argv/env/auxv vector may use the top half. */
		*out_usp = build_dyn_stack(stack_base + (uintptr_t)(DYN_STACK_PAGES - 1) * PAGE_SIZE,
		                           DYN_STACK_TOP - PAGE_SIZE,
		                           argv,
		                           argc,
		                           envp,
		                           envc,
		                           &mi,
		                           interp_base,
		                           (u_int64_t)(DYN_STACK_PAGES / 2) * PAGE_SIZE);
		if (*out_usp == 0)
			return (-1);
	}

	*out_entry = start_entry;
	return (0);
}

/**
 * Load a dynamically-linked program off the VFS, build its image + SysV stack,
 * and schedule it as a console task with argv[0]=@path.
 *
 * @return the scheduled task, or NULL on failure.
 */
static kTask_t *spawn_dynamic(const char *path)
{
	char *buf;
	int sz;
	u_int64_t *l1, entry, usp;
	kTask_t *t;
	char *argv[1];

	kprintf("dyn: loading %s (dynamic)...\n", path);
	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("dyn: %s not loadable\n", path);
		return (NULL);
	}

	l1 = pmap_create_user_space();
	argv[0] = (char *)path;
	if (load_dynamic(buf, l1, argv, 1, NULL, 0, &entry, &usp) != 0)
	{
		kfree(buf);
		kprintf("dyn: %s failed to load\n", path);
		return (NULL);
	}
	kfree(buf);

	t = schedNewTask();
	t->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, path, sizeof(t->name) - 1);
	aarch64_console_setup_fds(&t->td);
	sched_ready(t);
	return (t);
}

/**
 * Load + run a dynamically-linked program off the VFS as a scheduled task, and
 * cooperatively wait for it (bring-up test of the dynamic-linker path).
 */
void aarch64_run_dynamic(const char *path)
{
	kTask_t *t = spawn_dynamic(path);
	int i;

	if (t == NULL)
		return;
	for (i = 0; i < 200000 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();
	kprintf("dyn: %s returned (state=%d).\n", path, t->state);
}

/**
 * Schedule a dynamically-linked program off the VFS as a background daemon and
 * return immediately (no wait) — e.g. start /bin/authd before running login.
 */
void aarch64_spawn_dynamic(const char *path)
{
	(void)spawn_dynamic(path);
}

/**
 * Run a dynamically-linked program off the VFS as the system: schedule it, then
 * turn the boot thread into the cooperative idle/reaper loop (never returns).
 * Used to hand the machine to a disk-backed shell / init.
 */
void aarch64_run_dynamic_init(const char *path)
{
	kTask_t *t = spawn_dynamic(path);

	if (t == NULL)
		return;
	kprintf("dyn: %s is now the system (pid %d); idle loop.\n", path, t->id);
	/* The boot thread is now purely the idle thread: tag it so sched_account_tick
	 * buckets its ticks as idle (not busy) and procfs reports it at 0% CPU. */
	g_idle_task = _current;

	/* smp-plan M3: scheduler up + init scheduled — create each AP's per-CPU idle
	 * task and release the APs into the shared run queue (true SMP). */
	aarch64_smp_release_aps();

	for (;;)
	{
		sched_yield(); /* run any ready task */
		/* Nothing was runnable (sched_yield returned to the idle thread): halt the
		 * CPU until the next interrupt instead of spinning.  IRQs are unmasked, so
		 * the 100 Hz timer (and any device IRQ / expiring callout that makes a task
		 * runnable) wakes us; the loop then reschedules.  Without this the idle
		 * thread pegged a core at 100% once every service learned to block. */
		__asm__ volatile("wfi");
	}
}

/**
 * Copy a NULL-terminated user vector of user strings (argv/envp) into freshly
 * kmalloc'd kernel strings.  Run in the caller's (old) address space before any
 * TTBR0 switch, so the user pointers are valid.  Caller kfrees each kvec[i].
 *
 * @return the number of entries copied (0..max).
 */
static int copy_user_strvec(char *const *uvec, char **kvec, int max)
{
	int n = 0;

	if (uvec == NULL)
		return 0;
	while (n < max && uvec[n] != NULL)
	{
		const char *us = uvec[n];
		size_t len = 0;
		char *ks;
		while (us[len] != '\0' && len < 4096)
			len++;
		ks = (char *)kmalloc((u_int32_t)len + 1);
		if (ks == NULL)
			break;
		memcpy(ks, us, len);
		ks[len] = '\0';
		kvec[n++] = ks;
	}
	return n;
}

/**
 * execve(@path, @uargv, @uenvp): replace the *current* task's image with the ELF
 * at @path and restart it at EL0.  argv/envp are user vectors, copied to kernel
 * memory before the address-space switch and marshalled onto the new stack.
 *
 * Reads the file + builds the new image in a fresh address space (via the
 * dynamic loader, so PIE/dynamic and static both work), repoints the current
 * task, switches TTBR0, then aarch64_exec_to_el0()s into the new entry — which
 * does not return.  The old address space's user pages leak for now.
 *
 * @return -1 on any failure to load (on success it does not return).
 */
int aarch64_exec_replace(const char *path, char *const *uargv, char *const *uenvp)
{
	char *buf;
	int sz, argc, envc, i;
	u_int64_t *l1, entry, usp, kstack_top, old_ttbr0;
	char namebuf[256];
	char *kargv[MAXARG], *kenvp[MAXARG];

	buf = read_elf_file(path, &sz);
	if (buf == NULL)
	{
		kprintf("execve: %s not loadable\n", path);
		return (-1);
	}

	/* Copy name + argv + envp out of the old address space now — they are user
	 * pointers there and the old AS is unmapped the instant we switch TTBR0. */
	strncpy(namebuf, path, sizeof(namebuf) - 1);
	namebuf[sizeof(namebuf) - 1] = '\0';
	argc = copy_user_strvec(uargv, kargv, MAXARG);
	envc = copy_user_strvec(uenvp, kenvp, MAXARG);
	if (argc == 0)
	{
		kargv[0] = namebuf; /* always pass at least argv[0] */
		argc = 1;
	}

	/* Task name (basename of path) + cmdline (argv joined) — MI helper shared
	 * with i386 sys_exec.  Done here while kargv[] is still live (it is freed
	 * below once load_dynamic has copied it onto the new user stack). */
	exec_set_name_cmdline(_current, namebuf, kargv, argc);

	l1 = pmap_create_user_space();
	if (load_dynamic(buf, l1, kargv, argc, kenvp, envc, &entry, &usp) != 0)
	{
		kfree(buf);
		kprintf("execve: %s failed to load\n", path);
		return (-1);
	}
	kfree(buf);
	/* The argv/envp strings are now copied onto the new user stack; free the
	 * kernel temporaries (but not namebuf, which is on our stack). */
	for (i = 0; i < argc; i++)
		if (kargv[i] != namebuf)
			kfree(kargv[i]);
	for (i = 0; i < envc; i++)
		kfree(kenvp[i]);

	/* Repoint the current task at the new image, then make it live.  (name +
	 * cmdline were already set by exec_set_name_cmdline above.) */
	old_ttbr0 = _current->md.md_ttbr0;
	_current->md.md_ttbr0 = (u_int64_t)(uintptr_t)l1;
	_current->md.md_entry = entry;
	_current->md.md_usp = usp;
	_current->md.md_mmap_next = 0; /* fresh mmap/brk regions for the new image */
	_current->md.md_brk = 0;
	pmap_switch(l1);

	/* Reclaim the replaced image's user pages now that we run on the new TTBR0
	 * (the old space's frames are this process's private fork-copies / load
	 * frames — free_page is MMIO/COW-safe).  Without this every exec leaked the
	 * whole old address space. */
	pmap_free_user_space((u_int64_t *)(uintptr_t)old_ttbr0);

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
 * wait4(@want_pid, @status, @options): reap a child (any if @want_pid == -1).
 * With WNOHANG set in @options, return 0 immediately if no child has exited yet
 * (waitpid(..., WNOHANG) — used by the GUI terminal to poll its shell without
 * blocking its render loop).  Otherwise block: sleep in the WAIT state (off the
 * run queue) so the timer-driven sched() reaper wakes us (WAIT->READY) when a
 * child goes ZOMBIE — busy-yielding instead would leave us runnable and the
 * reaper's wake would double-enqueue us, corrupting the run queue under
 * preemption.  The re-scan after sched_sleep closes the lost-wakeup window.
 *
 * @return the reaped child's pid, 0 if WNOHANG and nothing exited, or -ECHILD.
 */
/**
 * Scan the current task's children for one that has STOPPED (Ctrl-Z / SIGSTOP)
 * and whose stop has not yet been reported to wait4 (t_stopped_sig != 0).  Does
 * NOT reap — a stopped child is collected later, once it continues and exits.
 *
 * @return the stopped child, or NULL if none has a pending (unreported) stop.
 */
static kTask_t *find_stopped_child(int want_pid)
{
	u_int32_t flags;
	kTask_t *t, *found = NULL;

	save_flags(flags);
	cli();
	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->parent != _current)
			continue;
		if (want_pid != -1 && (int)t->id != want_pid)
			continue;
		if (t->state == STOPPED && t->t_stopped_sig != 0)
		{
			found = t;
			break;
		}
	}
	restore_flags(flags);
	return (found);
}

#define WAIT4_WNOHANG 1   /* POSIX WNOHANG */
#define WAIT4_WUNTRACED 2 /* POSIX WUNTRACED — also report stopped children */
int aarch64_wait4(int want_pid, int *status, int options)
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

		/* WUNTRACED: report a child that stopped (Ctrl-Z / SIGSTOP) without
		 * reaping it, so a job-control shell (tcsh) learns its foreground job
		 * suspended and can reclaim the controlling terminal.  The stop is
		 * edge-triggered: t_stopped_sig is cleared once reported, and a later
		 * SIGCONT resumes the child via its STOPPED state.  Without this, wait4
		 * never returns for a stopped child and the terminal wedges. */
		if (options & WAIT4_WUNTRACED)
		{
			kTask_t *st = find_stopped_child(want_pid);
			if (st != NULL)
			{
				if (status != NULL && (uintptr_t)status >= 0x100000000UL)
					*status = (st->t_stopped_sig << 8) | 0x7f; /* W_STOPCODE(sig) */
				st->t_stopped_sig = 0;
				return ((int)st->id);
			}
		}

		if (!have_child)
			return (-ECHILD);

		/* Non-blocking poll: a live (un-exited) child exists but the caller does
		 * not want to wait.  Report "nothing reaped yet". */
		if (options & WAIT4_WNOHANG)
			return (0);

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
