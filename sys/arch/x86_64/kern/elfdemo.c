/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ELF64-loader demo (Phase 5d step C).  Proves the arch-neutral ELF64
 * loader (sys/kern/elf64_load.c) runs on x86_64 via the md hooks
 * (md_map_user_page / md_sync_icache).  It synthesizes a minimal static ET_EXEC
 * around the proven ring-3 payload (so it needs no on-disk binary yet — the musl
 * world is 5e), loads it into a fresh address space at the standard amd64 base
 * (0x400000, now in the clean user low half), and runs it via the scheduler.
 * Sibling of aarch64's bringup/elfdemo.c.  Throwaway once execve loads real
 * binaries off the FAT root.
 */

#include "../x86_64.h"
#include <ubixos/sched.h>
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <sys/elf_load.h>
#include <vmm/vmm.h>     /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>

#define ELF_LOAD_VA 0x400000UL /* amd64 ET_EXEC link base */
#define ELF_STACK_VA 0x500000UL
#define ELF_STACK_TOP (ELF_STACK_VA + PAGE_SIZE)

/* Use the syscall-instruction payload so the demo proves the ELF loader AND the
 * SYSCALL/SYSRET fast path together. */
extern char x86_64_user_demo_sc_start[];
extern char x86_64_user_demo_sc_end[];

static u8 g_elf_image[1024];

/**
 * Build a minimal static ET_EXEC: ELF header + one PT_LOAD program header + the
 * ring-3 payload, linked at ELF_LOAD_VA.  @return the image size in bytes.
 */
static unsigned long synth_elf(void)
{
	Elf64_Ehdr *eh = (Elf64_Ehdr *)g_elf_image;
	Elf64_Phdr *ph = (Elf64_Phdr *)(g_elf_image + sizeof(Elf64_Ehdr));
	unsigned long code_off = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
	unsigned long code_len = (unsigned long)(x86_64_user_demo_sc_end - x86_64_user_demo_sc_start);
	unsigned long total = code_off + code_len;

	memset(g_elf_image, 0, sizeof(g_elf_image));
	memcpy(g_elf_image + code_off, x86_64_user_demo_sc_start, code_len);

	eh->e_ident[EI_MAG0] = ELFMAG0;
	eh->e_ident[EI_MAG1] = ELFMAG1;
	eh->e_ident[EI_MAG2] = ELFMAG2;
	eh->e_ident[EI_MAG3] = ELFMAG3;
	eh->e_ident[EI_CLASS] = ELFCLASS64;
	eh->e_ident[EI_DATA] = ELFDATA2LSB;
	eh->e_ident[EI_VERSION] = EV_CURRENT;
	eh->e_type = ET_EXEC;
	eh->e_machine = EM_X86_64;
	eh->e_version = EV_CURRENT;
	eh->e_entry = ELF_LOAD_VA + code_off; /* payload runs at its in-memory VA */
	eh->e_phoff = sizeof(Elf64_Ehdr);
	eh->e_ehsize = sizeof(Elf64_Ehdr);
	eh->e_phentsize = sizeof(Elf64_Phdr);
	eh->e_phnum = 1;

	ph->p_type = PT_LOAD;
	ph->p_flags = PF_R | PF_W | PF_X;
	ph->p_offset = 0;
	ph->p_vaddr = ELF_LOAD_VA;
	ph->p_paddr = ELF_LOAD_VA;
	ph->p_filesz = total;
	ph->p_memsz = total;
	ph->p_align = PAGE_SIZE;

	return total;
}

/**
 * Load the synthesized ELF64 into a fresh address space and run it as a scheduled
 * ring-3 task — exercising the MI ELF64 loader end to end on x86_64.
 */
void x86_64_elf_demo(void)
{
	u64 pml4 = x86_64_create_user_space();
	uintptr_t stack_phys = vmm_find_free_page(sysID);
	u_int64_t entry = 0; /* elf64_load uses u_int64_t* (distinct from the bring-up u64) */
	kTask_t *t;
	int i;

	kprintf("elf demo: loading a synthesized static ELF64 via the MI loader...\n");
	(void)synth_elf();

	if (elf64_load(g_elf_image, (u_int64_t *)(uintptr_t)pml4, &entry) != 0)
	{
		kprintf("elf demo: elf64_load failed\n");
		return;
	}
	x86_64_map_user_page_to(pml4, ELF_STACK_VA, (u64)stack_phys, 1);

	t = schedNewTask();
	t->md.md_cr3 = pml4;
	t->md.md_entry = entry;
	t->md.md_usp = ELF_STACK_TOP;
	strncpy(t->name, "elf64proc", sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  task pid=%d ready (entry %X); yielding...\n", t->id, entry);
	for (i = 0; i < 64 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();

	kprintf("elf demo: ELF64 process (pid=%d) ran + exited — the MI ELF loader works on x86_64.\n", t->id);
}
