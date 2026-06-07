/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 ELF basics (stub — relocation types added with the aarch64 loader).
 * Reached via <machine/elf.h>.
 */
#ifndef _AARCH64_ELF_H
#define _AARCH64_ELF_H

#include <sys/types.h> /* u_intN_t used by the ELF typedefs */
#include <sys/elf64.h> /* Definitions common to all 64-bit architectures. */

#ifndef __ELF_WORD_SIZE
#define __ELF_WORD_SIZE 64 /* Used by <sys/elf_generic.h> for the Elf_* aliases */
#endif

#include <sys/elf_generic.h> /* Elf_Dyn, Elf_Sym, Elf_Addr, … → Elf64_* */

#define EM_AARCH64 183

/* Machine characteristics (mirror i386/elf.h). */
#define ELF_ARCH EM_AARCH64
#define ELF_TARG_CLASS ELFCLASS64
#define ELF_TARG_DATA ELFDATA2LSB
#define ELF_TARG_MACH EM_AARCH64
#define ELF_TARG_VER 1

#endif /* _AARCH64_ELF_H */
