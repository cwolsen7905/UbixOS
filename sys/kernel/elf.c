/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

int elf_load_file(kTask_t *p, const char *file, uint32_t *addr, uint32_t *entry) {
  int ret = 0;

  int i = 0x0;
  int x = 0x0;
  int numsegs = 0x0;

  uint32_t base_addr = 0x0;
  uint32_t real_base_addr = 0x0;

  Elf32_Ehdr *binaryHeader = 0x0;
  Elf32_Phdr *programHeader = 0x0;

  fileDescriptor *exec_fd = 0x0;

  exec_fd = fopen(file, "r");

  if (exec_fd == 0x0)
    return (-1);

  /* Load the ELF header */
  if ((binaryHeader = (Elf32_Ehdr *) kmalloc(sizeof(Elf32_Ehdr))) == 0x0)
    K_PANIC("malloc failed!");

  fread(binaryHeader, sizeof(Elf32_Ehdr), 1, exec_fd);

  /* Check If App Is A Real Application */
  if ((binaryHeader->e_ident[1] != 'E') && (binaryHeader->e_ident[2] != 'L') && (binaryHeader->e_ident[3] != 'F')) {
    ret = -1;
    goto failed;
  }

  if (binaryHeader->e_type == ET_DYN)
    real_base_addr = *addr;
  else if (binaryHeader->e_type == ET_EXEC)
    real_base_addr = 0x0;
  else {
    ret = -1;
    goto failed;
  }

  /* Load The Program Header(s) */
  if ((programHeader = (Elf32_Phdr *) kmalloc(sizeof(Elf32_Phdr) * binaryHeader->e_phnum)) == 0x0)
    K_PANIC("malloc failed!");

  fseek(exec_fd, binaryHeader->e_phoff, 0);

  fread(programHeader, (sizeof(Elf32_Phdr) * binaryHeader->e_phnum), 1, exec_fd);

  for (numsegs = 0x0, i = 0x0; i < binaryHeader->e_phnum; i++) {
    switch (programHeader[i].p_type) {
      case PT_LOAD:
        /*
         Allocate Memory Im Going To Have To Make This Load Memory With Correct
         Settings so it helps us in the future
         */
        for (x = 0x0; x < (programHeader[i].p_memsz + 0xFFF); x += 0x1000) {

          /* Make readonly and read/write */
          if (vmm_remapPage(vmm_findFreePage(_current->id), ((programHeader[i].p_vaddr & 0xFFFFF000) + x + real_base_addr), PAGE_DEFAULT, _current->id) == 0x0)
            K_PANIC("Error: Remap Page Failed");

          memset((void *) ((programHeader[i].p_vaddr & 0xFFFFF000) + x + real_base_addr), 0x0, 0x1000);
        }

        /* Now Load Section To Memory */
        fseek(exec_fd, programHeader[i].p_offset, 0);
        fread((void *) programHeader[i].p_vaddr + real_base_addr, programHeader[i].p_filesz, 1, exec_fd);

        if ((programHeader[i].p_flags & 0x2) != 0x2) {
          for (x = 0x0; x < (programHeader[i].p_memsz); x += 0x1000) {
            if ((vmm_setPageAttributes((programHeader[i].p_vaddr & 0xFFFFF000) + x + real_base_addr, PAGE_PRESENT | PAGE_USER)) != 0x0)
              K_PANIC("vmm_setPageAttributes failed");
          }
        }
        if (numsegs == 0x0)
          base_addr = programHeader[i].p_vaddr + real_base_addr; //(programHeader[i].p_vaddr & 0xFFFFF000) + real_base_addr;
        numsegs++;
      break;
    }
  }

  *addr = base_addr;

  *entry = binaryHeader->e_entry + real_base_addr;

  failed:

  /* Close The Open File */
  fclose(exec_fd);

  /* Free Binary Header Memory */
  if (binaryHeader != 0x0)
    kfree(binaryHeader);

  /* Free Program Header Memory */
  if (programHeader != 0x0)
    kfree(programHeader);

  return (ret);
}

const struct {
  char *elfTypeName;
  uInt32 id;
} elfType[] = { { "ET_NONE", 0 }, { "ET_REL", 1 }, { "ET_EXEC", 2 }, { "ET_DYN", 3 }, { "ET_CORE", 4 }, { "ET_LOPROC", 0xff00 }, { "ET_HIPROC", 0xffff }, };

const struct {
  char *phTypeName;
  uInt32 id;
} elfPhType[] = { { "PT_NULL", 0 }, { "PT_LOAD", 1 }, { "PT_DYNAMIC", 2 }, { "PT_INTERP", 3 }, { "PT_NOTE", 4 }, { "PT_SHLIB", 5 }, { "PT_PHDR", 6 }, { "PT_LOPROC", 0x70000000 }, { "PT_HIPROC", 0x7fffffff }, };

const struct {
  char *shTypeName;
  uInt32 id;
} elfShType[] = { { "SHT_NULL", 0 }, { "SHT_PROGBITS", 1 }, { "SHT_SYMTAB", 2 }, { "SHT_STRTAB", 3 }, { "SHT_RELA", 4 }, { "SHT_HASH", 5 }, { "SHT_DYNAMIC", 6 }, { "SHT_NOTE", 7 }, { "SHT_NOBITS", 8 }, { "SHT_REL", 9 }, { "SHT_SHLIB", 10 }, { "SHT_DYNSYM", 11 }, };

const struct {
  char *relTypeName;
  uInt32 id;
} elfRelType[] = { { "R_386_NONE", 0 }, { "R_386_32", 1 }, { "R_386_PC32", 2 }, { "R_386_GOT32", 3 }, { "R_386_PLT32", 4 }, { "R_386_COPY", 5 }, { "R_386_GLOB_DAT", 6 }, { "R_386_JMP_SLOT", 7 }, { "R_386_RELATIVE", 8 }, { "R_386_GOTOFF", 9 }, { "R_386_GOTPC", 10 }, };

char *elfGetShType(int shType) {
  return ((char *) elfShType[shType].shTypeName);
}

char *elfGetPhType(int phType) {
  return ((char *) elfPhType[phType].phTypeName);
}

char *elfGetRelType(int relType) {
  return ((char *) elfRelType[relType].relTypeName);
}

/***
 END
 ***/

