#include <stdio.h>
#include <stdlib.h>
#include "ld.h"

#define DT_PLTGOT 3

ldLibrary *ldAddLibrary(const char *lib) {
  int        i        = 0x0;
  int        x        = 0x0;
  int        rel      = 0x0;
  uint32_t    *reMap    = 0x0;
  char      *newLoc   = 0x0;
  FILE      *linkerFd = 0x0;
  char       tmpFile[1024];
  ldLibrary *tmpLib   = 0x0;
  elfDynamic *dynp    = 0x0;
  uint32_t     *tmp     = 0x0;

  if ((tmpLib = (ldLibrary *)malloc(sizeof(ldLibrary))) == 0x0) {
    printf("malloc failed: tmpLib\n");
    exit(0x1);
    }
  if (tmpLib->output == 0x0) {
    /* Hack because we have no ld path set */
    sprintf(tmpFile,"sys:/lib/%s",lib);
    linkerFd = fopen(tmpFile,"rb");
    if (linkerFd->fd == 0x0) {
      printf("Could not open library: %s\n",lib);
      exit(-1);
      }
    //if ((tmpLib->output = (char *)malloc((linkerFd->size+0x4000))) == 0x0) {
    //if ((tmpLib->output = (char *)malloc(0x111000)) == 0x0) {
    //if ((tmpLib->output = (char *)getPage((0x111000/0x1000),2)) == 0x0) {
    if ((tmpLib->output = (char *)getPage(((linkerFd->size+0x4000)/0x1000),2)) == 0x0) {
      printf("malloc failed: tmpLib->output\n");
      exit(0x1);
      }
    sprintf(tmpLib->name,lib);
    }

  printf("Base: {0x%X}[%i]\n",tmpLib->output, __LINE__);
  if (tmpLib->linkerHeader == 0x0) {
    fseek(linkerFd,0x0,0x0);
    if ((tmpLib->linkerHeader = (elfHeader *)malloc(sizeof(elfHeader))) == 0x0) {
      printf("malloc failed: tmpLib->linkerHeader\n");
      exit(0x1);
      }
    fread(tmpLib->linkerHeader,sizeof(elfHeader),1,linkerFd);
    }
  if (tmpLib->linkerProgramHeader == 0x0) {
    if ((tmpLib->linkerProgramHeader = (elfProgramHeader *)malloc(sizeof(elfProgramHeader)*tmpLib->linkerHeader->ePhnum)) == 0x0) {
      printf("malloc failed: tmpLib->linkerProgramHeader\n");
      exit(0x1);
      }
    fseek(linkerFd,tmpLib->linkerHeader->ePhoff,0);
    fread(tmpLib->linkerProgramHeader,sizeof(elfProgramHeader),tmpLib->linkerHeader->ePhnum,linkerFd);

    for (i=0;i<tmpLib->linkerHeader->ePhnum;i++) {
      switch (tmpLib->linkerProgramHeader[i].phType) {
        case PT_LOAD:
          newLoc = (char *)(tmpLib->linkerProgramHeader[i].phVaddr + (uint32_t)tmpLib->output);
          fseek(linkerFd,tmpLib->linkerProgramHeader[i].phOffset,0);
          fread(newLoc,tmpLib->linkerProgramHeader[i].phFilesz,1,linkerFd);
          break;
        case PT_DYNAMIC:
          dynp = (elfDynamic *)(tmpLib->linkerProgramHeader[i].phVaddr + (uint32_t)tmpLib->output);
          printf("dynp: 0x%X:0x%X:0x%X", dynp, tmpLib->linkerProgramHeader[i].phVaddr, tmpLib->output);
          for (;dynp->dynVal != 0x0;dynp++) {
           switch (dynp->dynVal) {
             case DT_PLTGOT:
                tmp = (void *)((uint32_t)tmpLib->output + dynp->dynPtr);
                tmp[1] = 0xDEAD;
                tmp[2] = 0xBEEF;
                break;
             default:
               printf("dV: %i", dynp->dynVal);
                  break;
           }
           }
          asm("nop");
          break;
        case PT_TLS:
            tmpLib->tlsindex = 1;
            tmpLib->tlssize = tmpLib->linkerProgramHeader[i].phMemsz;//ph->p_memsz;
            tmpLib->tlsalign = tmpLib->linkerProgramHeader[i].phAlign;//ph->p_align;
            tmpLib->tlsinitsize = tmpLib->linkerProgramHeader[i].phFilesz;//ph->p_filesz;
            tmpLib->tlsinit = (void*)(tmpLib->linkerProgramHeader[i].phVaddr + (uint32_t)tmpLib->output);
            printf("TLS: 0x%X, 0x%X, 0x%X, 0x%X", tmpLib->tlssize, tmpLib->tlsinitsize, tmpLib->tlsinit, tmpLib->tlsinit - (uint32_t)tmpLib->output);
/*
          newLoc = (char *)tmpLib->linkerProgramHeader[i].phVaddr + (uint32_t)tmpLib->output;
          fseek(linkerFd,tmpLib->linkerProgramHeader[i].phOffset,0);
          fread(newLoc,tmpLib->linkerProgramHeader[i].phFilesz,1,linkerFd);
*/
          break;
	case PT_GNU_STACK:
          /* Tells us if the stack should be executable.  Failsafe to executable until we add checking */
          printf("NOT DEF1\n");
      	  break;
      	case PT_PAX_FLAGS:
      		/* Not sure... */
          printf("NOT DEF2\n");
	  break;
        default:
          printf("Unhandled Header (ld.so) : %08x\n", tmpLib->linkerProgramHeader[i].phType);
          break;
        }
      }
    }

  if (tmpLib->linkerSectionHeader == 0x0) {
    if ((tmpLib->linkerSectionHeader = (elfSectionHeader *)malloc(sizeof(elfSectionHeader)*tmpLib->linkerHeader->eShnum)) == 0x0) {
      printf("malloc failed: tmpLib->linkerSectionHeader\n");
      exit(0x1);
      }
    fseek(linkerFd,tmpLib->linkerHeader->eShoff,0);
    fread(tmpLib->linkerSectionHeader,sizeof(elfSectionHeader),tmpLib->linkerHeader->eShnum,linkerFd);

  if (tmpLib->linkerShStr == 0x0) {
    tmpLib->linkerShStr = (char *)malloc(tmpLib->linkerSectionHeader[tmpLib->linkerHeader->eShstrndx].shSize);
    fseek(linkerFd,tmpLib->linkerSectionHeader[tmpLib->linkerHeader->eShstrndx].shOffset,0);
    fread(tmpLib->linkerShStr,tmpLib->linkerSectionHeader[tmpLib->linkerHeader->eShstrndx].shSize,1,linkerFd);
    }

  for (i = 0x0;i < tmpLib->linkerHeader->eShnum;i++) {
    switch (tmpLib->linkerSectionHeader[i].shType) {
     case 3:
        if (!strcmp((tmpLib->linkerShStr + tmpLib->linkerSectionHeader[i].shName),".dynstr")) {
          if (tmpLib->linkerDynStr == 0x0) {
            if ((tmpLib->linkerDynStr = (char *)malloc(tmpLib->linkerSectionHeader[i].shSize)) == 0x0) {
	      printf("malloc failed: tmpLib->linkerDynStr\n");
	      exit(0x1);
	      }
            fseek(linkerFd,tmpLib->linkerSectionHeader[i].shOffset,0);
            fread(tmpLib->linkerDynStr,tmpLib->linkerSectionHeader[i].shSize,1,linkerFd);
            }
          }
        break;
      case 9:
        if ((tmpLib->linkerElfRel = (elfPltInfo *)malloc(tmpLib->linkerSectionHeader[i].shSize)) == 0x0) {
	  printf("malloc failed: tmpLib->linkerElfRel\n");
	  exit(0x1);
	  }
        fseek(linkerFd,tmpLib->linkerSectionHeader[i].shOffset,0x0);
        fread(tmpLib->linkerElfRel,tmpLib->linkerSectionHeader[i].shSize,1,linkerFd);

        for (x=0x0;x<tmpLib->linkerSectionHeader[i].shSize/sizeof(elfPltInfo);x++) {
          rel = ELF32_R_SYM(tmpLib->linkerElfRel[x].pltInfo);
          reMap = (uint32_t *)((uint32_t)tmpLib->output + tmpLib->linkerElfRel[x].pltOffset);
          switch (ELF32_R_TYPE(tmpLib->linkerElfRel[x].pltInfo)) {
            case R_386_32:
            case R_386_TLS_TPOFF:
            case R_386_TLS_TPOFF32:
            case R_386_TLS_DTPMOD32:
            case R_386_TLS_DTPOFF32:
              *reMap += ((uint32_t)tmpLib->output + tmpLib->linkerRelSymTab[rel].dynValue);
              *reMap += ((uint32_t)tmpLib->output + tmpLib->linkerRelSymTab[rel].dynValue) - (uint32_t)reMap;
              break;
            case R_386_PC32:
              *reMap += ((uint32_t)tmpLib->output + tmpLib->linkerRelSymTab[rel].dynValue) - (uint32_t)reMap;
              break;
            case R_386_RELATIVE:
              *reMap += (uint32_t)tmpLib->output;
              break;
            case R_386_JMP_SLOT:
              *reMap += (uint32_t)tmpLib->output;
              break;
            case R_386_GLOB_DAT:
              *reMap = ((uint32_t)tmpLib->output + tmpLib->linkerRelSymTab[rel].dynValue);
              break;
            default:
              printf("Unhandled sym: [0x%X]\n", ELF32_R_TYPE(tmpLib->linkerElfRel[x].pltInfo));
              while (1);
              break;
            }
          }
        free(tmpLib->linkerElfRel);
        break;
      case 11:
        if (tmpLib->linkerRelSymTab == 0x0) {
          tmpLib->linkerRelSymTab = (elfDynSym *)malloc(tmpLib->linkerSectionHeader[i].shSize);
          fseek(linkerFd,tmpLib->linkerSectionHeader[i].shOffset,0);
          fread(tmpLib->linkerRelSymTab,tmpLib->linkerSectionHeader[i].shSize,1,linkerFd);
          tmpLib->sym = i;
          }
        break;
      default:
        printf("[SHTYPE: 0x%X]", tmpLib->linkerSectionHeader[i].shType);
        break;
      }
    }
  }
  if (libs != 0x0)
    libs->prev    = tmpLib;

  tmpLib->prev  = 0x0;
  tmpLib->next  = libs;
  libs          = tmpLib;

  if (linkerFd) {
    fclose(linkerFd);
    }
  return(tmpLib);
  }
