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
  /* TLS information */
  int tlsindex;               /* Index in DTV for this module */
  void *tlsinit;              /* Base address of TLS init block */
  size_t tlsinitsize;         /* Size of TLS init block for this module */
  size_t tlssize;             /* Size of TLS block for this module */
  size_t tlsoffset;           /* Offset of static TLS block for this module */
  size_t tlsalign;            /* Alignment of static TLS block */

  } ldLibrary;

extern ldLibrary *libs;
extern int       lib_c;
extern int       lib_s[10];

uint32_t ldFindFunc(const char *,const char *);
ldLibrary *ldFindLibrary(const char *);
ldLibrary *ldAddLibrary(const char *);

/***
 END
 ***/

