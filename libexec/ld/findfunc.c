#include <stdio.h>
#include <stdlib.h>
#include "ld.h"

uint32_t ldFindFunc(const char *func,const char *lib) {
  int        i        = 0x0;
  int        x        = 0x0;
  uint32_t    *funcPtr  = 0x0;
  ldLibrary *libPtr   = 0x0;

  for (x = 0; x < lib_c;x++) {
    libPtr = ldFindLibrary(lib + lib_s[x]);
    if (libPtr == 0x0) {
      libPtr = ldAddLibrary(lib + lib_s[x]);
      }

    if (libPtr == 0x0)
      continue;

    int nsyms = (int)(libPtr->linkerSectionHeader[libPtr->sym].shSize/sizeof(elfDynSym));
    for (i=0x0;i<nsyms;i++) {
      const char *symname = libPtr->linkerDynStr + libPtr->linkerRelSymTab[i].dynName;
      if (!strcmp(func, symname)) {
        funcPtr = (uint32_t *)((uint32_t)(libPtr->linkerRelSymTab[i].dynValue) + (uint32_t)libPtr->output);
        return((uint32_t)funcPtr);
        }
      }
    }

  printf("ERROR COULDN'T FIND FUNCTION: %s\n",func);
  return(0x0);
  }

