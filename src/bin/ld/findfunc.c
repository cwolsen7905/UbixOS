#include <stdio.h>
#include <stdlib.h>
#include "ld.h"

uInt32 ldFindFunc(const char *func,const char *lib) {
  int        i        = 0x0;
  int        x        = 0x0;
  uInt32    *funcPtr  = 0x0;
  ldLibrary *libPtr   = 0x0;

  for (x = 0; x < lib_c;x++) {
    libPtr = ldFindLibrary(lib + lib_s[x]); 
    if (libPtr == 0x0) {
      //printf("[%s][%s]\n",func,lib);
      libPtr = ldAddLibrary(lib + lib_s[x]);
      }

    //printf("str: [0x%X]\n",libPtr->linkerDynStr);
    //printf("func: [%s]",func);

    for (i=0x0;i<libPtr->linkerSectionHeader[libPtr->sym].shSize/sizeof(elfDynSym);i++) {
 //        printf("Func: [%s]\n",func); 
      if (!strcmp(func,(libPtr->linkerDynStr + libPtr->linkerRelSymTab[i].dynName))) {
        funcPtr = (uInt32 *)((uInt32)(libPtr->linkerRelSymTab[i].dynValue) + (uInt32)libPtr->output);
        if (funcPtr == 0x0) {
            printf("[%s:0x%X]\n",func,(uInt32)funcPtr);
          }
        return((uInt32)funcPtr);
        break;
        }
      }
    }

  printf("ERROR COULDN'T FIND FUNCTION: %s:%s\n",func,lib);
  return(0x0);
  }

