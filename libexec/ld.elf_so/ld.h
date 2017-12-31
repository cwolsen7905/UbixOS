#include <sys/types.h>
#include <string.h>
#include "elf.h"

typedef struct ldLibrary_s {
  struct ldLibrary_s *next;
  struct ldLibrary_s *prev;
  char              name[256];       
  elfHeader        *linkerHeader;
  elfSectionHeader *linkerSectionHeader;
  elfProgramHeader *linkerProgramHeader;
  elfDynSym        *linkerRelSymTab;
  elfPltInfo       *linkerElfRel;
  char             *linkerShStr;
  char             *linkerDynStr;
  char             *output;
  int               sym;
  } ldLibrary;

extern ldLibrary *libs;
extern int       lib_c;
extern int       lib_s[10];

uInt32 ldFindFunc(const char *,const char *);
ldLibrary *ldFindLibrary(const char *);
ldLibrary *ldAddLibrary(const char *);

/***
 END
 ***/

