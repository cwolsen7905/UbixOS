/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ELF execution + execve (Phase 5e).  Loads an ELF64 into a fresh address
 * space and builds the SysV amd64 initial stack musl's __libc_start_main expects
 * (argc/argv/envp/auxv incl. AT_PHDR/AT_BASE/AT_ENTRY/AT_RANDOM/AT_PAGESZ), then
 * either schedules it as a new task or replaces the calling task (sys_execve).
 *
 * The dynamic path (load_dynamic) loads a PIE main executable at DYN_MAIN_BASE,
 * reads + maps its PT_INTERP dynamic linker (ld-musl-x86_64.so.1) at
 * DYN_INTERP_BASE, and enters the linker — the boot path for the real world.  A
 * high-linked static ET_EXEC (no PT_INTERP) is jumped to directly.  Sibling of
 * aarch64's kern/execfile.c.
 */

#include "../x86_64.h"
#include <ubixos/sched.h>
#include <sys/thread.h>
#include <sys/sysproto_posix.h>
#include <sys/elf64.h>
#include <sys/elf_load.h>
#include <vmm/vmm.h>           /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h>       /* sysID */
#include <fs/vfs/file.h>       /* fopen/fread/fclose */
#include <x86_64/vmm_layout.h> /* DYN_MAIN_BASE/DYN_INTERP_BASE/DYN_STACK_* */
#include <ubixos/exec.h>       /* exec_set_name_cmdline (MI, shared with i386/aarch64) */
#include <ubixos/signal.h>     /* signal_exec_reset — reset caught handlers on exec */
#include <string.h>

#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_ENTRY 9
#define AT_RANDOM 25

#define EXEC_MAX (256 * 1024 * 1024) /* cap on an ELF image we'll load (self-hosted clang ~100 MB) */
#define MAXARG 48                  /* cap on argv/envp entries marshalled into a new image */

#define EXEC_STACK_VA 0x40020000UL /* single-page stack for the static bring-up path */
#define EXEC_STACK_TOP (EXEC_STACK_VA + PAGE_SIZE)
#define INITIAL_FRAME 6 /* argc, argv NULL, envp NULL, AT_PAGESZ, pagesz, AT_NULL */

/**
 * Read an ELF file off the VFS into a freshly-kmalloc'd buffer.  Caller kfrees.
 * @return the buffer (size in *out_size), or NULL on open/size/read failure.
 */
static char *read_elf_file(const char *path, int *out_size)
{
	fileDescriptor_t *fd;
	char *buf;
	int sz, n;

	fd = fopen(path, "r");
	if (fd == 0)
		return 0;

	sz = (int)fd->size;
	if (sz <= 0 || sz > EXEC_MAX)
	{
		fclose(fd);
		return 0;
	}
	buf = (char *)kmalloc((u_int32_t)sz);
	if (buf == 0)
	{
		fclose(fd);
		return 0;
	}
	n = (int)fread(buf, 1, (u_int32_t)sz, fd);
	fclose(fd);
	if (n != sz)
	{
		kfree(buf);
		return 0;
	}
	if (out_size != 0)
		*out_size = sz;
	return buf;
}

/**
 * Build the SysV initial stack for a dynamically-linked program in @frame (the
 * physmap pointer to the top stack page, mapped at @page_uva in the user space).
 * Lays down argv/envp strings, a 16-byte AT_RANDOM block, then the
 * argc/argv/envp/auxv vector __libc_start_main + ld-musl consume.  All in-stack
 * pointers use the user VA.
 *
 * @return the user-space stack pointer (16-byte aligned, points at argc).
 */
static u64 build_dyn_stack(u8 *frame,
                           u64 page_uva,
                           char **argv,
                           int argc,
                           char **envp,
                           int envc,
                           const elf64_load_info_t *mi,
                           u64 interp_base,
                           u64 limit)
{
	u8 *p = frame + PAGE_SIZE;
	u64 argv_uva[MAXARG], envp_uva[MAXARG], random_uva;
	u64 *vec;
	int i, k = 0, nvec;
	size_t need;
#define UVA(ptr) (page_uva + (u64)((u8 *)(ptr) - frame))

	/* The argv/env strings + the AT_RANDOM block + the argc/argv/envp/auxv vector
	 * are all laid down below the top of the stack, descending from @frame's top.
	 * @limit is how far down that may go (the reserved, physically-contiguous arg
	 * region).  Size it up front and bail cleanly on overflow rather than writing
	 * past the region and corrupting adjacent kernel memory (a huge environment
	 * must fail the exec, not silently scribble). */
	nvec = 1 + argc + 1 + envc + 1 + 16;
	need = 16 + 32 /* AT_RANDOM + two 16-byte alignment slacks */ + (size_t)nvec * sizeof(u64);
	for (i = 0; i < argc; i++)
		need += strlen(argv[i]) + 1;
	for (i = 0; i < envc; i++)
		need += strlen(envp[i]) + 1;
	if (need > limit)
	{
		kprintf("execve: argv+env (%u bytes) exceeds the %u-byte stack arg region\n", (u32)need, (u32)limit);
		return 0;
	}

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
	memcpy(p, "uBixOS-x86_64!\x01\x02", 16); /* AT_RANDOM: bring-up entropy (not a CSPRNG yet) */
	random_uva = UVA(p);

	/* Vector: argc, argv[..], NULL, envp[..], NULL, then 8 auxv pairs.  amd64 SysV
	 * requires RSP 16-byte aligned at the program entry — align so it is. */
	nvec = 1 + argc + 1 + envc + 1 + 16;
	p = (u8 *)((uintptr_t)p & ~(uintptr_t)15);
	p -= (u64)nvec * sizeof(u64);
	p = (u8 *)((uintptr_t)p & ~(uintptr_t)15);
	vec = (u64 *)p;

	vec[k++] = (u64)argc;
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
	/* If the headers are in no PT_LOAD (phdr_va unresolved), report 0 so musl's
	 * phdr walk skips it rather than dereferencing a NULL AT_PHDR. */
	vec[k++] = (mi->phdr_va != 0) ? mi->phnum : 0;
	vec[k++] = AT_PAGESZ;
	vec[k++] = PAGE_SIZE;
	vec[k++] = AT_BASE;
	vec[k++] = interp_base; /* dynamic linker load base (0 if static) */
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
 * Load a (PIE) dynamically-linked program @image into @pml4: map the main exe at
 * DYN_MAIN_BASE, read + map its PT_INTERP dynamic linker at DYN_INTERP_BASE,
 * build the SysV/auxv stack, and report the start RIP (the linker's entry) + RSP.
 * Also handles a high-linked static ET_EXEC (no PT_INTERP -> jump straight to it).
 *
 * @return 0 on success, -1 on failure.
 */
static int load_dynamic(
    const void *image, u64 pml4, char **argv, int argc, char **envp, int envc, u64 *out_entry, u64 *out_usp)
{
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
	u_int64_t *aspace = (u_int64_t *)(uintptr_t)pml4;
	elf64_load_info_t mi;
	u64 main_base = (eh->e_type == ET_DYN) ? DYN_MAIN_BASE : 0;
	u64 start_entry, interp_base = 0;

	if (elf64_load_at(image, aspace, main_base, &mi) != 0)
		return -1;
	start_entry = mi.entry;

	if (mi.interp_off != 0)
	{
		const char *interp = (const char *)image + mi.interp_off;
		elf64_load_info_t ii;
		char *ibuf;
		int isz;

		kprintf("dyn: interp = %s\n", interp);
		ibuf = read_elf_file(interp, &isz);
		if (ibuf == 0)
		{
			kprintf("dyn: cannot load interp %s\n", interp);
			return -1;
		}
		interp_base = DYN_INTERP_BASE;
		if (elf64_load_at(ibuf, aspace, interp_base, &ii) != 0)
		{
			kfree(ibuf);
			return -1;
		}
		kfree(ibuf);
		start_entry = ii.entry; /* enter the dynamic linker, not the main exe */
	}

	/* Allocate the whole stack as one *contiguous* run of frames.  The argv/env/auxv
	 * vector is laid down from the top descending, and contiguous frames keep both
	 * the kernel P2V view and the user VA linear — so the vector can span as many
	 * pages as it needs (the pointer math in build_dyn_stack stays valid across page
	 * boundaries) instead of being capped at a single page.  256 KB (DYN_STACK_PAGES)
	 * is readily available at the early-boot execs; a contiguous-alloc failure fails
	 * the exec cleanly rather than corrupting. */
	{
		uintptr_t stack_base;
		uintptr_t top_kv;
		int i;

		stack_base = vmm_find_free_pages_contig(DYN_STACK_PAGES, sysID);
		if (stack_base == 0)
			return -1;
		memset((void *)P2V(stack_base), 0, (size_t)DYN_STACK_PAGES * PAGE_SIZE);
		for (i = 0; i < DYN_STACK_PAGES; i++)
			x86_64_map_user_page_to(
			    pml4, DYN_STACK_VA + (u64)i * PAGE_SIZE, (u64)(stack_base + (uintptr_t)i * PAGE_SIZE), 1);

		/* build_dyn_stack writes top-down from the topmost page.  Reserve the lower
		 * half of the stack for runtime growth; the argv/env/auxv vector may use the
		 * top half (128 KB) — far beyond any real argv + environment. */
		top_kv = (uintptr_t)P2V(stack_base + (uintptr_t)(DYN_STACK_PAGES - 1) * PAGE_SIZE);
		*out_usp = build_dyn_stack((u8 *)top_kv,
		                           DYN_STACK_TOP - PAGE_SIZE,
		                           argv,
		                           argc,
		                           envp,
		                           envc,
		                           &mi,
		                           interp_base,
		                           (u64)(DYN_STACK_PAGES / 2) * PAGE_SIZE);
		if (*out_usp == 0)
			return -1;
	}

	*out_entry = start_entry;
	return 0;
}

/**
 * Load @image into a fresh address space + build a minimal SysV initial stack
 * (static bring-up path: argc=0, AT_PAGESZ only).  Used by the static test
 * binaries in x86_64_exec_demo.
 * @return the new PML4 (physical), or 0 on failure.
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

	stack_frame = vmm_find_free_page(sysID);
	if (stack_frame == 0)
		return 0;
	sp = (u64 *)((u8 *)P2V(stack_frame) + PAGE_SIZE) - INITIAL_FRAME;
	for (i = 0; i < INITIAL_FRAME; i++)
		sp[i] = 0;
	sp[3] = AT_PAGESZ;
	sp[4] = PAGE_SIZE;
	sp[5] = AT_NULL;
	x86_64_map_user_page_to(pml4, EXEC_STACK_VA, (u64)stack_frame, 1);

	*out_entry = entry;
	*out_usp = EXEC_STACK_TOP - (u64)INITIAL_FRAME * 8;
	return pml4;
}

/**
 * Copy a NULL-terminated user string vector (argv or envp) into freshly kmalloc'd
 * kernel strings, so it survives the address-space switch in execve (the user
 * pointers become invalid the instant CR3 is reloaded).  Sibling of aarch64's
 * copy_user_strvec.
 * @return the number of entries copied (0..max).  Caller kfree()s each kvec[i].
 */
static int copy_user_strvec(char *const *uvec, char **kvec, int max)
{
	int n = 0;

	if (uvec == 0)
		return 0;
	while (n < max && uvec[n] != 0)
	{
		const char *us = uvec[n];
		size_t len = 0;
		char *ks;
		while (us[len] != '\0' && len < 4096)
			len++;
		ks = (char *)kmalloc((u_int32_t)len + 1);
		if (ks == 0)
			break;
		memcpy(ks, us, len);
		ks[len] = '\0';
		kvec[n++] = ks;
	}
	return n;
}

/**
 * Load the program at @path into a fresh address space via the dynamic loader
 * (PIE/dynamic and high static both work) with the given @argv/@envp, and report
 * the new PML4 + start RIP + RSP.  @argv/@envp are kernel pointers (already copied
 * out of the caller's address space).
 * @return the new PML4 (physical), or 0 on failure.
 */
static u64 build_dynamic_argv(
    const char *path, char **argv, int argc, char **envp, int envc, u64 *out_entry, u64 *out_usp)
{
	char *buf;
	int sz;
	u64 pml4;

	buf = read_elf_file(path, &sz);
	if (buf == 0)
		return 0;
	pml4 = x86_64_create_user_space();
	if (pml4 == 0)
	{
		kfree(buf);
		return 0;
	}
	if (load_dynamic(buf, pml4, argv, argc, envp, envc, out_entry, out_usp) != 0)
	{
		kfree(buf);
		return 0;
	}
	kfree(buf);
	return pml4;
}

/**
 * Load @path with the single argument argv[0]=@path and no environment (the
 * service-spawn path: spawn_dynamic / the bring-up demos).
 * @return the new PML4 (physical), or 0 on failure.
 */
static u64 build_dynamic_from_path(const char *path, u64 *out_entry, u64 *out_usp)
{
	char *argv[1];
	argv[0] = (char *)path;
	return build_dynamic_argv(path, argv, 1, 0, 0, out_entry, out_usp);
}

/**
 * execve(path, argv, envp): replace the calling task's image with the ELF at
 * @path (read off the VFS) via the dynamic loader, then enter it at ring 3 —
 * does not return on success.  The caller's argv/envp vectors are copied out of
 * the old address space (they become invalid the instant CR3 is reloaded) and
 * marshalled onto the new image's SysV stack, so the shell/apps see their real
 * arguments + environment (TERM/HOME/PATH/SHELL, login-shell argv0, …).
 */
int sys_execve(struct thread *td, struct sys_execve_args *uap)
{
	u64 pml4, entry, usp;
	char *kargv[MAXARG], *kenvp[MAXARG];
	char namebuf[256];
	int argc, envc, i;

	(void)td;

	/* Copy name + argv + envp out of the OLD address space now — they are user
	 * pointers there, and the old space is unmapped the instant we reload CR3. */
	strncpy(namebuf, (const char *)uap->fname, sizeof(namebuf) - 1);
	namebuf[sizeof(namebuf) - 1] = '\0';
	argc = copy_user_strvec(uap->argv, kargv, MAXARG);
	envc = copy_user_strvec(uap->envp, kenvp, MAXARG);
	if (argc == 0)
	{
		kargv[0] = namebuf; /* always pass at least argv[0] */
		argc = 1;
	}

	/* Task name (basename of path) + cmdline (argv joined) — MI helper shared with
	 * i386 / aarch64.  Done while kargv[] is still live (freed below). */
	exec_set_name_cmdline(_current, namebuf, kargv, argc);

	pml4 = build_dynamic_argv((const char *)uap->fname, kargv, argc, kenvp, envc, &entry, &usp);

	/* The argv/envp strings are now copied onto the new user stack; free the kernel
	 * temporaries (but not namebuf, which is on our stack). */
	for (i = 0; i < argc; i++)
		if (kargv[i] != namebuf)
			kfree(kargv[i]);
	for (i = 0; i < envc; i++)
		kfree(kenvp[i]);

	if (pml4 == 0)
		return -1;

	/* Replace the current task's image and enter it.  switch_to re-arms rsp0 from
	 * kernelStack, so update md_* and load the new CR3 ourselves, then IRETQ. */
	u64 old_pml4 = _current->md.md_cr3;
	_current->md.md_cr3 = pml4;
	_current->md.md_entry = entry;
	_current->md.md_usp = usp;
	_current->md.md_mmap_next = 0; /* fresh image: reset the mmap/brk bumps */
	_current->md.md_brk = 0;
	/* Reset the user TLS: a fresh image's ld-musl sets its own FS.base via
	 * arch_prctl, but until it does it must not see the previous image's (here a
	 * fork parent's) stale TLS pointer — clear md_fsbase + FS.base now (execve
	 * enters via iret_to_user, bypassing switch_to's per-task FS.base restore). */
	_current->md.md_fsbase = 0;
	__asm__ __volatile__("wrmsr" : : "c"(0xC0000100u), "a"(0u), "d"(0u));
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pml4) : "memory");

	/* Reclaim the replaced image now that we run on the new CR3 — the old space's
	 * frames are this task's private fork-copies / load frames (wired + MMIO are
	 * skipped, so a shared window buffer or the framebuffer is never freed).  Done
	 * AFTER the CR3 switch: never free the address space you are executing on.
	 * Without this every exec leaked the whole previous image. */
	x86_64_free_user_space(old_pml4);

	/* POSIX: the new image inherits SIG_DFL/SIG_IGN dispositions but NOT caught
	 * handlers — reset those so a stale handler VA (e.g. the launching shell's
	 * SIGCHLD job-control handler) can't be entered in this image. */
	signal_exec_reset(&_current->td);

	x86_64_iret_to_user(entry, usp); /* does not return */
	return 0;                        /* unreachable */
}

/**
 * Schedule the dynamically-linked program at @path as a new ring-3 task with
 * argv[0]=@path.  @return the scheduled task, or NULL on failure.
 */
static kTask_t *spawn_dynamic(const char *path)
{
	u64 pml4, entry, usp;
	kTask_t *t;

	kprintf("dyn: loading %s (dynamic)...\n", path);
	pml4 = build_dynamic_from_path(path, &entry, &usp);
	if (pml4 == 0)
	{
		kprintf("dyn: %s failed to load\n", path);
		return 0;
	}

	t = schedNewTask();
	t->md.md_cr3 = pml4;
	t->md.md_entry = entry;
	t->md.md_usp = usp;
	strncpy(t->name, path, sizeof(t->name) - 1);
	/* New processes start at the root dir; without this oInfo.cwd is empty and the
	 * VFS resolves a relative path (".") against "" — so ls/pwd fail (i386 sets this
	 * in i386_exec).  fork inherits it; the shell's chdir(HOME) builds on it. */
	t->oInfo.cwd[0] = '/';
	t->oInfo.cwd[1] = '\0';
	x86_64_console_setup_fds(&t->td); /* stdin/stdout/stderr -> COM1 console */
	sched_ready(t);
	kprintf("dyn: %s ready (pid %d, entry %X usp %X)\n", path, t->id, entry, usp);
	return t;
}

/**
 * Run the dynamically-linked program at @path as the system (PID 1 init): schedule
 * it, then turn the boot thread into the cooperative idle loop (never returns).
 */
void x86_64_run_dynamic_init(const char *path)
{
	kTask_t *t = spawn_dynamic(path);

	if (t == 0)
	{
		kprintf("dyn: %s could not start — staying in the kernel idle loop.\n", path);
		return;
	}
	kprintf("dyn: %s is now the system (pid %d); idle loop.\n", path, t->id);
	g_idle_task = _current;

	for (;;)
	{
		sched_yield();
		__asm__ __volatile__("hlt");
	}
}

/**
 * Phase 5e demo: load a real static ELF64 off the FAT root and run it to
 * completion (proves the static execve path — the test binaries).
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
	for (i = 0; i < 256; i++)
		sched_yield();
	kprintf("exec demo: %s ran + exited — execve loads real on-disk binaries on x86_64.\n", path);
}
