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

 $Id$

*****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "ld.h"

ldLibrary *libs = 0x0;
int        lib_c               = 0x0;
int        lib_s[10];
int        binarySym           = 0x0;

static elfHeader        *binaryHeader        = 0x0;
elfSectionHeader *binarySectionHeader = 0x0;
static char             *binaryShStr         = 0x0;
char             *binaryDynStr        = 0x0;
elfDynSym        *binaryRelSymTab     = 0x0;
static Elf32_Dyn        *binaryElf32_Dyn     = 0x0;
static elfPltInfo       *binaryElfRelDyn     = 0x0;
static elfPltInfo       *binaryElfRel        = 0x0;

uInt32 ld(uInt32 got2,uInt32 entry) {
  int  i             = 0x0;
  int  x             = 0x0;
  int  y             = 0x0;
  int  rel           = 0x0;
  int  relDyn        = 0x0;
  uInt32 *reMap      = 0x0;
  FILE *binaryFd     = 0x0;

  if (binaryHeader == 0x0) {
    binaryFd = malloc(sizeof(FILE));
    binaryFd->fd = (uInt32)got2;
    fseek(binaryFd,0x0,0x0);
    binaryHeader = (elfHeader *)malloc(sizeof(elfHeader));
    fread(binaryHeader,sizeof(elfHeader),1,binaryFd);
    }

  if (binarySectionHeader == 0x0) {
    binarySectionHeader = (elfSectionHeader *)malloc(sizeof(elfSectionHeader)*binaryHeader->eShnum);
    fseek(binaryFd,binaryHeader->eShoff,0);
    fread(binarySectionHeader,sizeof(elfSectionHeader),binaryHeader->eShnum,binaryFd);

  if (binaryShStr == 0x0) {
    binaryShStr = (char *)malloc(binarySectionHeader[binaryHeader->eShstrndx].shSize);
    fseek(binaryFd,binarySectionHeader[binaryHeader->eShstrndx].shOffset,0);
    fread(binaryShStr,binarySectionHeader[binaryHeader->eShstrndx].shSize,1,binaryFd);
    }

    for (i=0x0;i<binaryHeader->eShnum;i++) {
      switch (binarySectionHeader[i].shType) {
        case 3:
          if (!strcmp((binaryShStr + binarySectionHeader[i].shName),".dynstr")) {
            if (binaryDynStr == 0x0) {
              binaryDynStr = (char *)malloc(binarySectionHeader[i].shSize);
              fseek(binaryFd,binarySectionHeader[i].shOffset,0);
              fread(binaryDynStr,binarySectionHeader[i].shSize,1,binaryFd);
              }
            }
          break;
        case SHT_DYNAMIC:
          binaryElf32_Dyn = (Elf32_Dyn *)malloc(binarySectionHeader[i].shSize);
          fseek(binaryFd,binarySectionHeader[i].shOffset,0);
          fread(binaryElf32_Dyn,binarySectionHeader[i].shSize,1,binaryFd);
          for (x = 0;x < binarySectionHeader[i].shSize / sizeof(Elf32_Dyn);x++) {
            if (binaryElf32_Dyn[x].d_tag == 1) {
              lib_s[lib_c] = binaryElf32_Dyn[x].d_un.d_ptr;
              lib_c++;
              }
       }
          break;
        case SHT_REL:
  //        if (!strcmp(binaryShStr + binarySectionHeader[i].shName,".rel.dyn"))
    //        relDyn = i;
      //    else
            rel = i;
          break;
        case 11:
          if (binaryRelSymTab == 0x0) {
            binaryRelSymTab = (elfDynSym *)malloc(binarySectionHeader[i].shSize);
            fseek(binaryFd,binarySectionHeader[i].shOffset,0);
            fread(binaryRelSymTab,binarySectionHeader[i].shSize,1,binaryFd);
            binarySym = i;
            }
          break;
        }
      }
    }

/*
  if ((binaryElfRelDyn == 0x0) && (relDyn != 0)) {
    binaryElfRelDyn = (elfPltInfo *)malloc(binarySectionHeader[i].shSize);
    fseek(binaryFd,binarySectionHeader[relDyn].shOffset,0x0);
    fread(binaryElfRelDyn,binarySectionHeader[relDyn].shSize,1,binaryFd);

    for (x = 0;x < binarySectionHeader[relDyn].shSize / sizeof(elfPltInfo);x++) {
      switch (ELF32_R_TYPE(binaryElfRelDyn[x].pltInfo)) {
         case R_386_COPY:
            printf("COPY");
            reMap = (uInt32 *)binaryElfRelDyn[x].pltOffset;
            *reMap = 0x1;
            break;
         default:
            //printf("UNHANDLED THING");
            break;
         }
      printf("y: [%i:0x%X]",y,binaryElfRelDyn[x].pltOffset);
      }
    }
*/

  if (binaryElfRel == 0x0) {
    binaryElfRel = (elfPltInfo *)malloc(binarySectionHeader[rel].shSize);
    fseek(binaryFd,binarySectionHeader[rel].shOffset,0x0);
    fread(binaryElfRel,binarySectionHeader[rel].shSize,1,binaryFd);
    }


  i = (entry/sizeof(elfPltInfo));
  x = ELF32_R_SYM(binaryElfRel[i].pltInfo);
  reMap = (uInt32 *)binaryElfRel[i].pltOffset;
  *reMap = ldFindFunc(binaryDynStr + binaryRelSymTab[x].dynName,binaryDynStr);
//printf("\nld(%s:0x%X)",binaryDynStr + binaryRelSymTab[x].dynName,*reMap);
  //*reMap = ldFindFunc(binaryDynStr + binaryRelSymTab[x].dynName,(char *)(binaryDynStr + 1));


  if (binaryFd) {
    fclose(binaryFd);
    }

  return(*reMap);
  }

/***
 END
 ***/
