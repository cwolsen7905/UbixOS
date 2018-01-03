/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: ld.c 141 2016-01-17 02:05:18Z reddawg $

 *****************************************************************************************/

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

uInt32 ldEnable() {
  int i = 0x0;
  int x = 0x0;
  int rel = 0x0;
  int sym = 0x0;
  char *newLoc = 0x0;
  char *shStr = 0x0;
  char *dynStr = 0x0;
  uInt32 *reMap = 0x0;
  fileDescriptor *ldFd = 0x0;
  Elf32_Ehdr *binaryHeader = 0x0;
  elfProgramHeader *programHeader = 0x0;
  elfSectionHeader *sectionHeader = 0x0;
  elfDynSym *relSymTab = 0x0;
  elfPltInfo *elfRel = 0x0;

  /* Open our dynamic linker */
  ldFd = fopen("sys:/libexec/ld.so", "rb");

  if (ldFd == 0x0) {
    kprintf("Can not open ld.so\n");
  }

  fseek(ldFd, 0x0, 0x0);
  binaryHeader = (Elf32_Ehdr *) kmalloc(sizeof(Elf32_Ehdr));
  assert(binaryHeader);
  fread(binaryHeader, sizeof(Elf32_Ehdr), 1, ldFd);

  programHeader = (elfProgramHeader *) kmalloc(sizeof(elfProgramHeader) * binaryHeader->e_phnum);
  assert(programHeader);
  fseek(ldFd, binaryHeader->e_phoff, 0);
  fread(programHeader, sizeof(elfSectionHeader), binaryHeader->e_phnum, ldFd);

  sectionHeader = (elfSectionHeader *) kmalloc(sizeof(elfSectionHeader) * binaryHeader->e_shnum);
  assert(sectionHeader);
  fseek(ldFd, binaryHeader->e_shoff, 0);
  fread(sectionHeader, sizeof(elfSectionHeader), binaryHeader->e_shnum, ldFd);

  shStr = (char *) kmalloc(sectionHeader[binaryHeader->e_shstrndx].shSize);
  fseek(ldFd, sectionHeader[binaryHeader->e_shstrndx].shOffset, 0);
  fread(shStr, sectionHeader[binaryHeader->e_shstrndx].shSize, 1, ldFd);

  for (i = 0x0; i < binaryHeader->e_phnum; i++) {
    switch (programHeader[i].phType) {
      case PT_LOAD:
        newLoc = (char *) programHeader[i].phVaddr + LD_START;
        /*
         Allocate Memory Im Going To Have To Make This Load Memory With Correct
         Settings so it helps us in the future
         */
        for (x = 0; x < (programHeader[i].phMemsz); x += 0x1000) {
          /* make r/w or ro */
          if ((vmm_remapPage(vmm_findFreePage(_current->id), ((programHeader[i].phVaddr & 0xFFFFF000) + x + LD_START), PAGE_DEFAULT)) == 0x0)
            K_PANIC("vmmRemapPage: ld");
          memset((void *) ((programHeader[i].phVaddr & 0xFFFFF000) + x + LD_START), 0x0, 0x1000);
        }
        /* Now Load Section To Memory */
        fseek(ldFd, programHeader[i].phOffset, 0x0);
        fread(newLoc, programHeader[i].phFilesz, 1, ldFd);

      break;
      case PT_DYNAMIC:
        /* Now Load Section To Memory */
        fseek(ldFd, programHeader[i].phOffset, 0x0);
        fread(newLoc, programHeader[i].phFilesz, 1, ldFd);
      break;
      case PT_GNU_STACK:
        /* Tells us if the stack should be executable.  Failsafe to executable
         until we add checking */
      break;
      default:
        kprintf("Unhandled Header (kernel) : %08x\n", programHeader[i].phType);
      break;
    }
  }

  for (i = 0x0; i < binaryHeader->e_shnum; i++) {
    switch (sectionHeader[i].shType) {
      case 3:
        if (!strcmp((shStr + sectionHeader[i].shName), ".dynstr")) {
          dynStr = (char *) kmalloc(sectionHeader[i].shSize);
          fseek(ldFd, sectionHeader[i].shOffset, 0x0);
          fread(dynStr, sectionHeader[i].shSize, 1, ldFd);
        }
      break;
      case 9:
        elfRel = (elfPltInfo *) kmalloc(sectionHeader[i].shSize);
        fseek(ldFd, sectionHeader[i].shOffset, 0x0);
        fread(elfRel, sectionHeader[i].shSize, 1, ldFd);

        for (x = 0x0; x < sectionHeader[i].shSize / sizeof(elfPltInfo); x++) {
          rel = ELF32_R_SYM(elfRel[x].pltInfo);
          reMap = (uInt32 *) ((uInt32) LD_START + elfRel[x].pltOffset);
          switch (ELF32_R_TYPE(elfRel[x].pltInfo)) {
            case R_386_32:
              *reMap += ((uInt32) LD_START + relSymTab[rel].dynValue);
            break;
            case R_386_PC32:
              *reMap += ((uInt32) LD_START + relSymTab[rel].dynValue) - (uInt32) reMap;
            break;
            case R_386_RELATIVE:
              *reMap += (uInt32) LD_START;
            break;
            default:
              kprintf("[0x%X][0x%X](%i)[%s]\n", elfRel[x].pltOffset, elfRel[x].pltInfo, rel, elfGetRelType(ELF32_R_TYPE(elfRel[x].pltInfo)));
              kprintf("relTab [%s][0x%X][0x%X]\n", dynStr + relSymTab[rel].dynName, relSymTab[rel].dynValue, relSymTab[rel].dynName);
            break;
          }
        }
        kfree(elfRel);
      break;
      case 11:
        relSymTab = (elfDynSym *) kmalloc(sectionHeader[i].shSize);
        fseek(ldFd, sectionHeader[i].shOffset, 0x0);
        fread(relSymTab, sectionHeader[i].shSize, 1, ldFd);
        sym = i;
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

  return ((uInt32) i);
}
