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

#include <ubixos/ld.h>
#include <ubixos/sched.h>
#include <sys/elf.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <vfs/vfs.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>

uint32_t ldEnable() {
  int i = 0x0;
  int x = 0x0;
  int rel = 0x0;
  int sym = 0x0;
  char *newLoc = 0x0;
  char *shStr = 0x0;
  char *dynStr = 0x0;
  uint32_t *reMap = 0x0;
  fileDescriptor_t *ldFd = 0x0;
  Elf_Ehdr *binaryHeader = 0x0;
  Elf_Phdr *programHeader = 0x0;
  Elf_Shdr *sectionHeader = 0x0;
  Elf_Sym *relSymTab = 0x0;
  Elf_Rel *elfRel = 0x0;
  Elf_Rela *elfRela = 0x0;
  Elf_Addr addr;

  /* Open our dynamic linker */
  ldFd = fopen("sys:/libexec/ld.so", "rb");

  if (ldFd == 0x0) {
    kprintf("Can not open ld.so\n");
  }

  fseek(ldFd, 0x0, 0x0);
  binaryHeader = (Elf32_Ehdr *) kmalloc(sizeof(Elf32_Ehdr));

  assert(binaryHeader);
  fread(binaryHeader, sizeof(Elf32_Ehdr), 1, ldFd);

  programHeader = (Elf_Phdr *) kmalloc(sizeof(Elf_Phdr) * binaryHeader->e_phnum);
  assert(programHeader);

  fseek(ldFd, binaryHeader->e_phoff, 0);
  fread(programHeader, sizeof(Elf_Shdr), binaryHeader->e_phnum, ldFd);

  sectionHeader = (Elf_Shdr *) kmalloc(sizeof(Elf_Shdr) * binaryHeader->e_shnum);
  assert(sectionHeader);
  fseek(ldFd, binaryHeader->e_shoff, 0);
  fread(sectionHeader, sizeof(Elf_Shdr), binaryHeader->e_shnum, ldFd);

  shStr = (char *) kmalloc(sectionHeader[binaryHeader->e_shstrndx].sh_size);
  fseek(ldFd, sectionHeader[binaryHeader->e_shstrndx].sh_offset, 0);
  fread(shStr, sectionHeader[binaryHeader->e_shstrndx].sh_size, 1, ldFd);

  for (i = 0x0; i < binaryHeader->e_phnum; i++) {
    switch (programHeader[i].p_type) {
      case PT_LOAD:
        newLoc = (char *) programHeader[i].p_vaddr + LD_START;
        /*
         Allocate Memory Im Going To Have To Make This Load Memory With Correct
         Settings so it helps us in the future
         */
        for (x = 0; x < (programHeader[i].p_memsz); x += 0x1000) {
          /* make r/w or ro */
          if ((vmm_remapPage(vmm_findFreePage(_current->id), ((programHeader[i].p_vaddr & 0xFFFFF000) + x + LD_START), PAGE_DEFAULT, _current->id, 0)) == 0x0)
            K_PANIC("vmmRemapPage: ld");
          memset((void *) ((programHeader[i].p_vaddr & 0xFFFFF000) + x + LD_START), 0x0, 0x1000);
        }
        /* Now Load Section To Memory */
        fseek(ldFd, programHeader[i].p_offset, 0x0);
        fread(newLoc, programHeader[i].p_filesz, 1, ldFd);

      break;
      case PT_DYNAMIC:
        /* Now Load Section To Memory */
        //fseek(ldFd, programHeader[i].p_offset, 0x0);
        //fread(newLoc, programHeader[i].p_filesz, 1, ldFd);
      break;
      case PT_GNU_STACK:
        /* Tells us if the stack should be executable.  Failsafe to executable
         until we add checking */
      break;
      default:
        kprintf("Unhandled Header (kernel) : %08x\n", programHeader[i].p_type);
      break;
    }
  }

  for (i = 0x0; i < binaryHeader->e_shnum; i++) {
    switch (sectionHeader[i].sh_type) {
      case SHT_STRTAB:
        if (!strcmp((shStr + sectionHeader[i].sh_name), ".dynstr")) {
          dynStr = (char *) kmalloc(sectionHeader[i].sh_size);
          //fseek(ldFd, sectionHeader[i].sh_offset, 0x0);
          //fread(dynStr, sectionHeader[i].sh_size, 1, ldFd);
        }
      break;
      case SHT_REL:
        elfRel = (Elf_Rel *) kmalloc(sectionHeader[i].sh_size);
        //fseek(ldFd, sectionHeader[i].sh_offset, 0x0);
        //fread(elfRel, sectionHeader[i].sh_size, 1, ldFd);

        for (x = 0x0; x < sectionHeader[i].sh_size / sizeof(Elf_Rel); x++) {
          rel = ELF32_R_SYM(elfRel[x].r_info);
          reMap = (uint32_t *) ((uint32_t) LD_START + elfRel[x].r_offset);
          switch (ELF32_R_TYPE(elfRel[x].r_info)) {
            case R_386_32:
              *reMap += ((uint32_t) LD_START + relSymTab[rel].st_value);
            break;
            case R_386_PC32:
              *reMap += ((uint32_t) LD_START + relSymTab[rel].st_value) - (uint32_t) reMap;
            break;
            case R_386_RELATIVE:
              *reMap += (uint32_t) LD_START;
            break;
            case R_386_NONE:
            break;
            default:
              kprintf("[0x%X][0x%X](%i)[%s]\n", elfRel[x].r_offset, elfRel[x].r_info, rel, elfGetRelType(ELF32_R_TYPE(elfRel[x].r_info)));
              kprintf("relTab [%s][0x%X][0x%X]\n", dynStr + relSymTab[rel].st_name, relSymTab[rel].st_value, relSymTab[rel].st_name);
            break;
          }
        }
        kfree(elfRel);
      break;
      case SHT_DYNSYM:
        relSymTab = (Elf_Sym *) kmalloc(sectionHeader[i].sh_size);
        fseek(ldFd, sectionHeader[i].sh_offset, 0x0);
        fread(relSymTab, sectionHeader[i].sh_size, 1, ldFd);
        sym = i;
      break;
      case SHT_PROGBITS:
        //kprintf("PROGBITS");
        break;
      case SHT_HASH:
        //kprintf("HASH");
        break;
      case SHT_DYNAMIC:
        //kprintf("DYNAMIC");
        break;
      case SHT_SYMTAB:
        //kprintf("SYMTAB");
        break;
      default:
        kprintf("INvalid: %i]", sectionHeader[i].sh_type);
      break;
    }
  }

  i = binaryHeader->e_entry + LD_START;

  kfree(dynStr);
  kfree(shStr);
  kfree(relSymTab);
  kfree(sectionHeader);
  kfree(programHeader);
  kfree(binaryHeader);
  fclose(ldFd);

  return ((uint32_t) i);
}
