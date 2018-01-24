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
  
    for (i=0x0;i<libPtr->linkerSectionHeader[libPtr->sym].shSize/sizeof(elfDynSym);i++) {
      if (!strcmp(func,(libPtr->linkerDynStr + libPtr->linkerRelSymTab[i].dynName))) {
        funcPtr = (uint32_t *)((uint32_t)(libPtr->linkerRelSymTab[i].dynValue) + (uint32_t)libPtr->output);
        if (funcPtr == 0x0) {
            printf("[%s:0x%X]\n",func,funcPtr,*funcPtr);
          }
        return((uint32_t)funcPtr);
        break;
        }
      }
    }

  printf("ERROR COULDN'T FIND FUNCTION: %s:%s\n",func,lib);
  return(0x0);
  }

