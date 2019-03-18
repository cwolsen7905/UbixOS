#include <string.h>
#include "ld.h"

ldLibrary *ldFindLibrary(const char *lib) {
  ldLibrary *tmpLibs = 0x0;

  for (tmpLibs = libs;tmpLibs != 0x0;tmpLibs = tmpLibs->next) {
    if (!strcmp(tmpLibs->name,lib)) {
      return(tmpLibs);
      }
    }
  return(0x0);
  }
