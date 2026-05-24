/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/elf.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <lib/kmalloc.h>
#include <vmm/vmm.h>
#include <lib/kprintf.h>
#include <string.h>

/* Program header type (p_type) id → name table. */
static const struct
{
	char *phTypeName;
	uInt32 id;
} g_elf_ph_type[] = {
    {"PT_NULL", 0},
    {"PT_LOAD", 1},
    {"PT_DYNAMIC", 2},
    {"PT_INTERP", 3},
    {"PT_NOTE", 4},
    {"PT_SHLIB", 5},
    {"PT_PHDR", 6},
    {"PT_LOPROC", 0x70000000},
    {"PT_HIPROC", 0x7fffffff},
};

/* Section header type (sh_type) id → name table. */
static const struct
{
	char *shTypeName;
	uInt32 id;
} g_elf_sh_type[] = {
    {"SHT_NULL", 0},
    {"SHT_PROGBITS", 1},
    {"SHT_SYMTAB", 2},
    {"SHT_STRTAB", 3},
    {"SHT_RELA", 4},
    {"SHT_HASH", 5},
    {"SHT_DYNAMIC", 6},
    {"SHT_NOTE", 7},
    {"SHT_NOBITS", 8},
    {"SHT_REL", 9},
    {"SHT_SHLIB", 10},
    {"SHT_DYNSYM", 11},
};

/* Relocation type (r_info type) id → name table. */
static const struct
{
	char *relTypeName;
	uInt32 id;
} g_elf_rel_type[] = {
    {"R_386_NONE", 0},
    {"R_386_32", 1},
    {"R_386_PC32", 2},
    {"R_386_GOT32", 3},
    {"R_386_PLT32", 4},
    {"R_386_COPY", 5},
    {"R_386_GLOB_DAT", 6},
    {"R_386_JMP_SLOT", 7},
    {"R_386_RELATIVE", 8},
    {"R_386_GOTOFF", 9},
    {"R_386_GOTPC", 10},
};

/**
 * Load an ELF executable into a task's address space.
 *
 * Allocates pages via the VMM and copies each PT_LOAD segment from the file.
 * ET_DYN images are rebased to *addr on entry; ET_EXEC images load at their
 * fixed virtual address (real_base = 0).
 *
 * @param addr   In: base address hint for ET_DYN. Out: VA of first PT_LOAD segment.
 * @param entry  Out: program entry point virtual address.
 * @return 0 on success, -1 if the file cannot be opened or is not a supported ELF type.
 */
int elf_load_file(kTask_t *p, const char *file, uint32_t *addr, uint32_t *entry)
{
	int ret = 0;
	int i = 0x0;
	int x = 0x0;
	int numsegs = 0x0;

	uint32_t base_addr = 0x0;
	uint32_t real_base_addr = 0x0;

	Elf32_Ehdr *binary_header = 0x0;
	Elf32_Phdr *program_header = 0x0;
	fileDescriptor_t *exec_fd = 0x0;

	exec_fd = fopen(file, "r");

	if (exec_fd == 0x0)
	{
		return (-1);
	}

	/* Load the ELF header */
	if ((binary_header = (Elf32_Ehdr *)kmalloc(sizeof(Elf32_Ehdr))) == 0x0)
	{
		K_PANIC("malloc failed!");
	}

	fread(binary_header, sizeof(Elf32_Ehdr), 1, exec_fd);

	/* Check If App Is A Real Application */
	if ((binary_header->e_ident[1] != 'E') || (binary_header->e_ident[2] != 'L') ||
	    (binary_header->e_ident[3] != 'F'))
	{
		ret = -1;
		goto failed;
	}

	if (binary_header->e_type == ET_DYN)
	{
		real_base_addr = *addr;
	}
	else if (binary_header->e_type == ET_EXEC)
	{
		real_base_addr = 0x0;
	}
	else
	{
		ret = -1;
		goto failed;
	}

	/* Load The Program Header(s) */
	if ((program_header = (Elf32_Phdr *)kmalloc(sizeof(Elf32_Phdr) * binary_header->e_phnum)) == 0x0)
	{
		K_PANIC("malloc failed!");
	}

	kern_fseek(exec_fd, binary_header->e_phoff, 0);

	fread(program_header, (sizeof(Elf32_Phdr) * binary_header->e_phnum), 1, exec_fd);

	for (numsegs = 0x0, i = 0x0; i < binary_header->e_phnum; i++)
	{
		switch (program_header[i].p_type)
		{
			case PT_LOAD:
				for (x = 0x0; x < (program_header[i].p_memsz + 0xFFF); x += 0x1000)
				{
					if (vmm_remapPage(
					        vmm_findFreePage(_current->id),
					        ((program_header[i].p_vaddr & 0xFFFFF000) + x + real_base_addr),
					        PAGE_DEFAULT,
					        _current->id,
					        0x0) == 0x0)
					{
						K_PANIC("Error: Remap Page Failed");
					}

					memset((void *)((program_header[i].p_vaddr & 0xFFFFF000) + x + real_base_addr),
					       0x0,
					       0x1000);
				}

				kern_fseek(exec_fd, program_header[i].p_offset, 0);
				fread((void *)program_header[i].p_vaddr + real_base_addr,
				      program_header[i].p_filesz,
				      1,
				      exec_fd);

				if ((program_header[i].p_flags & 0x2) != 0x2)
				{
					for (x = 0x0; x < (program_header[i].p_memsz); x += 0x1000)
					{
						if ((vmm_setPageAttributes((program_header[i].p_vaddr & 0xFFFFF000) +
						                               x + real_base_addr,
						                           PAGE_PRESENT | PAGE_USER)) != 0x0)
						{
							K_PANIC("vmm_setPageAttributes failed");
						}
					}
				}
				if (numsegs == 0x0)
				{
					base_addr = program_header[i].p_vaddr + real_base_addr;
				}
				numsegs++;
				break;
		}
	}

	*addr = base_addr;
	*entry = binary_header->e_entry + real_base_addr;

failed:
	fclose(exec_fd);

	if (binary_header != 0x0)
	{
		kfree(binary_header);
	}

	if (program_header != 0x0)
	{
		kfree(program_header);
	}

	return (ret);
}

/**
 * Return the human-readable name for an ELF section header type.
 *
 * @param sh_type  Must be a valid index into g_elf_sh_type[]; no bounds check is performed.
 * @return String name (e.g. "SHT_PROGBITS").
 */
char *elfGetShType(int sh_type)
{
	return ((char *)g_elf_sh_type[sh_type].shTypeName);
}

/**
 * Return the human-readable name for an ELF program header type.
 *
 * @param ph_type  Must be a valid index into g_elf_ph_type[]; no bounds check is performed.
 * @return String name (e.g. "PT_LOAD").
 */
char *elfGetPhType(int ph_type)
{
	return ((char *)g_elf_ph_type[ph_type].phTypeName);
}

/**
 * Return the human-readable name for an ELF relocation type.
 *
 * @param rel_type  Must be a valid index into g_elf_rel_type[]; no bounds check is performed.
 * @return String name (e.g. "R_386_32").
 */
char *elfGetRelType(int rel_type)
{
	return ((char *)g_elf_rel_type[rel_type].relTypeName);
}
