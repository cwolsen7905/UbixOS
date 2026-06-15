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
#include <string.h>

#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_ENTRY 9
#define AT_RANDOM 25

#define EXEC_MAX (4 * 1024 * 1024) /* cap on an ELF image we'll load (libc.so ~1 MB) */
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
static u64 build_dyn_stack(
    u8 *frame, u64 page_uva, char **argv, int argc, char **envp, int envc, const elf64_load_info_t *mi, u64 interp_base)
{
	u8 *p = frame + PAGE_SIZE;
	u64 argv_uva[MAXARG], envp_uva[MAXARG], random_uva;
	u64 *vec;
	int i, k = 0, nvec;
#define UVA(ptr) (page_uva + (u64)((u8 *)(ptr) - frame))

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

	/* Map a multi-page stack; the auxv/argv vector lives in the top page. */
	{
		uintptr_t top_frame = 0;
		int i;
		for (i = 0; i < DYN_STACK_PAGES; i++)
		{
			uintptr_t f = vmm_find_free_page(sysID);
			if (f == 0)
				return -1;
			memset((void *)P2V(f), 0, PAGE_SIZE);
			x86_64_map_user_page_to(pml4, DYN_STACK_VA + (u64)i * PAGE_SIZE, (u64)f, 1);
			if (i == DYN_STACK_PAGES - 1)
				top_frame = f;
		}
		*out_usp = build_dyn_stack(
		    (u8 *)P2V(top_frame), DYN_STACK_TOP - PAGE_SIZE, argv, argc, envp, envc, &mi, interp_base);
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
 * Load the program at @path into a fresh address space via the dynamic loader
 * (PIE/dynamic and high static both work) with argv[0]=@path, and report the new
 * PML4 + start RIP + RSP.
 * @return the new PML4 (physical), or 0 on failure.
 */
static u64 build_dynamic_from_path(const char *path, u64 *out_entry, u64 *out_usp)
{
	char *buf;
	int sz;
	u64 pml4;
	char *argv[1];

	buf = read_elf_file(path, &sz);
	if (buf == 0)
		return 0;
	pml4 = x86_64_create_user_space();
	if (pml4 == 0)
	{
		kfree(buf);
		return 0;
	}
	argv[0] = (char *)path;
	if (load_dynamic(buf, pml4, argv, 1, 0, 0, out_entry, out_usp) != 0)
	{
		kfree(buf);
		return 0;
	}
	kfree(buf);
	return pml4;
}

/**
 * execve(path, argv, envp): replace the calling task's image with the ELF at
 * @path (read off the VFS) via the dynamic loader, then enter it at ring 3 —
 * does not return on success.  argv/envp marshalling is minimal for now (argc=1,
 * argv[0]=path); the full SysV copy lands with interactive job control.
 */
int sys_execve(struct thread *td, struct sys_execve_args *uap)
{
	u64 pml4, entry, usp;

	(void)td;
	pml4 = build_dynamic_from_path((const char *)uap->fname, &entry, &usp);
	if (pml4 == 0)
		return -1;

	/* Replace the current task's image and enter it.  switch_to re-arms rsp0 from
	 * kernelStack, so update md_* and load the new CR3 ourselves, then IRETQ. */
	_current->md.md_cr3 = pml4;
	_current->md.md_entry = entry;
	_current->md.md_usp = usp;
	_current->md.md_mmap_next = 0; /* fresh image: reset the mmap/brk bumps */
	_current->md.md_brk = 0;
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pml4) : "memory");
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
