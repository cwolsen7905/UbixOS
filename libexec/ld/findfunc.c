#include <stdio.h>
#include <stdlib.h>
#include "ld.h"

uint32_t ldFindFunc(const char *func,const char *lib) {
  int        i        = 0x0;
  int        x        = 0x0;
  uint32_t    *funcPtr  = 0x0;
  ldLibrary *libPtr   = 0x0;

  printf("ldFindFunc: func=\"%s\" lib_c=%d\n", func, lib_c);
  for (x = 0; x < lib_c;x++) {
    printf("ldFindFunc: searching lib \"%s\"\n", lib + lib_s[x]);
    libPtr = ldFindLibrary(lib + lib_s[x]);
    if (libPtr == 0x0) {
      libPtr = ldAddLibrary(lib + lib_s[x]);
      }

    if (libPtr == 0x0) {
      printf("ldFindFunc: ldAddLibrary failed for \"%s\"\n", lib + lib_s[x]);
      continue;
      }

    int nsyms = (int)(libPtr->linkerSectionHeader[libPtr->sym].shSize/sizeof(elfDynSym));
    printf("ldFindFunc: library has %d symbols, sym=%d\n", nsyms, libPtr->sym);
    for (i=0x0;i<nsyms;i++) {
      const char *symname = libPtr->linkerDynStr + libPtr->linkerRelSymTab[i].dynName;
      if (!strcmp(func, symname)) {
        funcPtr = (uint32_t *)((uint32_t)(libPtr->linkerRelSymTab[i].dynValue) + (uint32_t)libPtr->output);
        printf("ldFindFunc: found \"%s\" at 0x%X (val=0x%X base=0x%X)\n",
               func, (uint32_t)funcPtr, libPtr->linkerRelSymTab[i].dynValue, (uint32_t)libPtr->output);
        return((uint32_t)funcPtr);
        }
      }
    printf("ldFindFunc: symbol \"%s\" not found in library\n", func);
    }

  printf("ERROR COULDN'T FIND FUNCTION: %s\n",func);
  return(0x0);
  }

