/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 ELF-exec demo (QEMU `virt` bring-up).
 *
 * Exercises the full exec path with the generic loader (sys/kern/elf64_load.c):
 * wrap the position-independent EL0 payload (el0.S) in a minimal in-memory
 * ELF64 image — standing in for a binary read from a file — create a fresh
 * address space, elf64_load() it, give it a stack, switch TTBR0 and run it at
 * EL0.  The program writes via the write syscall and exits.  Throwaway
 * scaffolding; the real path will read the ELF from the filesystem.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <vmm/paging.h>  /* PAGE_SIZE */
#include <lib/kmalloc.h> /* kmalloc, sysID */
#include <sys/elf_load.h>
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <machine/elf.h> /* ELF_TARG_MACH */
#include <string.h>

#define USER_ELF_VA 0x100000000UL /* block 4 (4 GB): a clean, unused VA slot */
#define USER_STACK_VA 0x100010000UL
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000UL

/**
 * Build a minimal single-PT_LOAD ELF64 image around the EL0 payload, linked at
 * USER_ELF_VA.  Returns a kmalloc'd image (header page + code).
 */
static void *synthesize_elf(void)
{
	u_int64_t code_len = (u_int64_t)(user_demo_code_end - user_demo_code_start);
	u_int8_t *buf = kmalloc((u_int32_t)(PAGE_SIZE + code_len));
	Elf64_Ehdr *eh = (Elf64_Ehdr *)buf;
	Elf64_Phdr *ph = (Elf64_Phdr *)(buf + sizeof(Elf64_Ehdr));

	memset(buf, 0, PAGE_SIZE);

	eh->e_ident[EI_MAG0] = ELFMAG0;
	eh->e_ident[EI_MAG1] = ELFMAG1;
	eh->e_ident[EI_MAG2] = ELFMAG2;
	eh->e_ident[EI_MAG3] = ELFMAG3;
	eh->e_ident[EI_CLASS] = ELFCLASS64;
	eh->e_ident[EI_DATA] = ELFDATA2LSB;
	eh->e_ident[EI_VERSION] = EV_CURRENT;
	eh->e_type = ET_EXEC;
	eh->e_machine = ELF_TARG_MACH;
	eh->e_version = EV_CURRENT;
	eh->e_entry = USER_ELF_VA;
	eh->e_phoff = sizeof(Elf64_Ehdr);
	eh->e_ehsize = sizeof(Elf64_Ehdr);
	eh->e_phentsize = sizeof(Elf64_Phdr);
	eh->e_phnum = 1;

	ph->p_type = PT_LOAD;
	ph->p_flags = PF_R | PF_X;
	ph->p_offset = PAGE_SIZE;
	ph->p_vaddr = USER_ELF_VA;
	ph->p_paddr = USER_ELF_VA;
	ph->p_filesz = code_len;
	ph->p_memsz = code_len;
	ph->p_align = PAGE_SIZE;

	memcpy(buf + PAGE_SIZE, user_demo_code_start, (size_t)code_len);
	return buf;
}

/**
 * Load the synthesized ELF into a fresh address space and run it at EL0.
 */
void aarch64_elf_demo(void)
{
	u_int64_t ttbr0, *kernel_l1, *l1, entry;
	uintptr_t stack_frame;
	void *elf;

	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	kernel_l1 = (u_int64_t *)(uintptr_t)(ttbr0 & PTE_ADDR_MASK);

	kprintf("elf demo: loading + running an ELF64 binary...\n");

	elf = synthesize_elf();
	l1 = pmap_create_user_space();
	if (elf64_load(elf, l1, &entry) != 0)
	{
		kprintf("elf demo: load failed\n");
		return;
	}

	stack_frame = vmm_find_free_page(sysID);
	pmap_map_user_page(l1, USER_STACK_VA, (u_int64_t)stack_frame, 0);

	kprintf("  loaded; entry=0x%lX — entering EL0\n", entry);
	pmap_switch(l1);
	aarch64_enter_el0(entry, USER_STACK_TOP);
	pmap_switch(kernel_l1); /* restore the kernel address space */

	kprintf("elf demo: ELF process ran + exited — exec path works on aarch64.\n");
}
