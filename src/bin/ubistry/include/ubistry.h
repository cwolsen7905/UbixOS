#include <sys/types.h>

#define MBOX_NAME "ubistry"

struct ubistryKey {
  struct ubistryKey *prev;
  struct ubistryKey *next;
  char               name[128];
  char               value[512];
  };

struct ubistryKey * ubistryFindKey(char *);
int ubistryAddKey(char *,char *);
int ubistryInitMbox(char *);
void ubistryProcessMessages();
