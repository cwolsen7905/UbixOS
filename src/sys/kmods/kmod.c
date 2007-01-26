/*****************************************************************************************
 Copyright (c) 2002-2005 The UbixOS Project
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

 $Id$

*****************************************************************************************/

#include <ubixos/types.h>
#include <ubixos/kmod.h>
#include <ubixos/sched.h>
#include <ubixos/elf.h>
#include <ubixos/kpanic.h>
#include <ubixos/lists.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <vfs/vfs.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>

List_t *List = 0x0;

uInt32 kmod_add(const char *kmod_file, const char *name)
{
	uInt32 addr = 0x0;
	Item_t *tmp;
	kmod_t *kmods;

	
	addr = kmod_load(kmod_file);
	if (addr == 0x0)
		return 0x0;

	if(List == 0x0)
	{
		List = InitializeList();
	}

	tmp = CreateItem();
	InsertItemAtFront(List, tmp);
	kmods = kmalloc(sizeof *kmods);
	tmp->data = kmods;
	if(kmods == NULL)
	{
		kprintf("kmod_add: unable to allocate memory!\n");
		return 0x0;
	}
		
	return 0x0;
}

uInt32 kmod_load(const char *kmod_file) {
  int               i             = 0x0;
  int               x             = 0x0;
  int               rel           = 0x0;
  int               sym           = 0x0;
  char             *newLoc        = 0x0; 
  char             *shStr         = 0x0;
  char             *dynStr        = 0x0;
  uInt32           *reMap         = 0x0;
  struct file      *kmod_fd       = 0x0;
  elfHeader        *binaryHeader  = 0x0;
  elfProgramHeader *programHeader = 0x0;
  elfSectionHeader *sectionHeader = 0x0;
  elfDynSym        *relSymTab     = 0x0;
  elfPltInfo       *elfRel        = 0x0;

  /* Open kernel module */
  kmod_fd = (struct fileDescriptorStruct *)kmalloc(sizeof(struct fileDescriptorStruct));
  fopen(kmod_fd,kmod_file,"rb");
  if (kmod_fd == 0x0) {
    kprintf("Can not open %s\n",kmod_file);
      return 0x0;
    }
 
  /* load module header */
  fseek(kmod_fd,0x0,0x0);
  binaryHeader = (elfHeader *)kmalloc(sizeof(elfHeader));
  if(binaryHeader == 0x0)
  {
	kprintf("kmod: out of memory\n");
	return 0x0;
  }
	
  assert(binaryHeader);
  fread(binaryHeader,sizeof(elfHeader),1,kmod_fd);

  programHeader = (elfProgramHeader *)kmalloc(sizeof(elfProgramHeader)*binaryHeader->ePhnum);
  assert(programHeader);
  fseek(kmod_fd,binaryHeader->ePhoff,0);
  fread(programHeader,sizeof(elfSectionHeader),binaryHeader->ePhnum,kmod_fd);

  sectionHeader = (elfSectionHeader *)kmalloc(sizeof(elfSectionHeader)*binaryHeader->eShnum);
  assert(sectionHeader);
  fseek(kmod_fd,binaryHeader->eShoff,0);
  fread(sectionHeader,sizeof(elfSectionHeader),binaryHeader->eShnum,kmod_fd);

  shStr = (char *)kmalloc(sectionHeader[binaryHeader->eShstrndx].shSize);
  fseek(kmod_fd,sectionHeader[binaryHeader->eShstrndx].shOffset,0);
  fread(shStr,sectionHeader[binaryHeader->eShstrndx].shSize,1,kmod_fd);

  for (i=0;i<binaryHeader->ePhnum;i++) {
    switch (programHeader[i].phType) {
      case PT_LOAD:
      case PT_DYNAMIC:
        newLoc = (char *)programHeader[i].phVaddr + LD_START;
        /*
        Allocate Memory Im Going To Have To Make This Load Memory With Correct
        Settings so it helps us in the future
        */
        for (x=0;x < ((programHeader[i].phMemsz)+4095);x += 0x1000) {
          /* make r/w or ro */
          if ((vmm_remapPage(vmmFindFreePage(_current->id),((programHeader[i].phVaddr & 0xFFFFF000) + x + LD_START),PAGE_DEFAULT)) == 0x0) 
	    kpanic("vmmRemapPage: ld\n");
          memset((void *)((programHeader[i].phVaddr & 0xFFFFF000) + x + LD_START),0x0,0x1000);
          }
        /* Now Load Section To Memory */
        fseek(kmod_fd,programHeader[i].phOffset,0x0);
        fread(newLoc,programHeader[i].phFilesz,1,kmod_fd);
        break;
      case PT_GNU_STACK:
        /* Tells us if the stack should be executable.  Failsafe to executable
	   until we add checking */
      	break;
      case PT_PAX_FLAGS:
      	/* Not sure... */
	break;
      default:
        kprintf("Unhandled Header : %08x\n", programHeader[i].phType);
        break;
      }
    }

  for (i=0x0;i<binaryHeader->eShnum;i++) {
    switch (sectionHeader[i].shType) {
     case 3:
        if (!strcmp((shStr + sectionHeader[i].shName),".dynstr")) {
          dynStr = (char *)kmalloc(sectionHeader[i].shSize);
          fseek(kmod_fd,sectionHeader[i].shOffset,0x0);
          fread(dynStr,sectionHeader[i].shSize,1,kmod_fd);
          }
        break;
      case 9:
        elfRel = (elfPltInfo *)kmalloc(sectionHeader[i].shSize);
        fseek(kmod_fd,sectionHeader[i].shOffset,0x0);
        fread(elfRel,sectionHeader[i].shSize,1,kmod_fd);

        for (x=0x0;x<sectionHeader[i].shSize/sizeof(elfPltInfo);x++) {
          rel = ELF32_R_SYM(elfRel[x].pltInfo);
          reMap = (uInt32 *)((uInt32)LD_START + elfRel[x].pltOffset);
          switch (ELF32_R_TYPE(elfRel[x].pltInfo)) {
            case R_386_32:
              *reMap += ((uInt32)LD_START + relSymTab[rel].dynValue);
              break;
            case R_386_PC32:
              *reMap += ((uInt32)LD_START + relSymTab[rel].dynValue) - (uInt32)reMap;
              break;
            case R_386_RELATIVE:
              *reMap += (uInt32)LD_START;
              break;
            default:
              kprintf("[0x%X][0x%X](%i)[%s]\n",elfRel[x].pltOffset,elfRel[x].pltInfo,rel,elfGetRelType(ELF32_R_TYPE(elfRel[x].pltInfo)));
              kprintf("relTab [%s][0x%X][0x%X]\n",dynStr + relSymTab[rel].dynName,relSymTab[rel].dynValue,relSymTab[rel].dynName);
              break;
            }
          }
        kfree(elfRel);
        break;
      case 11:
        relSymTab = (elfDynSym *)kmalloc(sectionHeader[i].shSize);
        fseek(kmod_fd,sectionHeader[i].shOffset,0x0);
        fread(relSymTab,sectionHeader[i].shSize,1,kmod_fd);
        sym = i;
        break;
      }
    }

  i = binaryHeader->eEntry + LD_START;

  kfree(dynStr);
  kfree(shStr);
  kfree(relSymTab);
  kfree(sectionHeader);
  kfree(programHeader);
  kfree(binaryHeader);
  fclose(kmod_fd);

  return((uInt32)i);
  }

/***
 END
 ***/
