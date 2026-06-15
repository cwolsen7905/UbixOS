/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ELF basics (the loader uses ELF_TARG_MACH to reject non-native images).
 * Reached via <machine/elf.h>.  Sibling of aarch64/elf.h.
 */
#ifndef _X86_64_ELF_H
#define _X86_64_ELF_H

#include <sys/types.h> /* u_intN_t used by the ELF typedefs */
#include <sys/elf64.h> /* Definitions common to all 64-bit architectures. */

#ifndef __ELF_WORD_SIZE
#define __ELF_WORD_SIZE 64 /* Used by <sys/elf_generic.h> for the Elf_* aliases */
#endif

#include <sys/elf_generic.h> /* Elf_Dyn, Elf_Sym, Elf_Addr, … → Elf64_* */

#define EM_X86_64 62

/* Machine characteristics (mirror i386/elf.h). */
#define ELF_ARCH EM_X86_64
#define ELF_TARG_CLASS ELFCLASS64
#define ELF_TARG_DATA ELFDATA2LSB
#define ELF_TARG_MACH EM_X86_64
#define ELF_TARG_VER 1

#endif /* _X86_64_ELF_H */
