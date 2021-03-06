#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>

int mib[CTL_MAXNAME];
size_t len;
char *p;

static int name2oid(const char *name, int *oidp);

int main() {
  printf("SYSCTL\n");

  int i;
  u_int32_t *tI;
  int *sI;


  mib[0] = 2;//1;
  mib[1] = 134516811;//18;

  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);

  printf("[len: %i]\n", len); 

  if (len == 4) {
    sI = p;
    printf("[%i]\n", sI[0]);
  }
  else
    printf("[%s]\n", p); 

  size_t j;

  j = name2oid("kern.ngroups", mib);
  //j = name2oid("hw.pagesizes", mib);

  printf("j:[%i]\n", j);

  for (int i = 0; i < j; i++)
    printf("[%i]", mib[i]);

  printf("\n");
}

static int name2oid(const char *name, int *oidp)
{
        int oid[2];
        int i;
        size_t j;

        oid[0] = 0;
        oid[1] = 3;

        j = CTL_MAXNAME * sizeof(int);
        i = sysctl(oid, 2, oidp, &j, name, strlen(name));
        if (i < 0)
                return (i);
        j /= sizeof(int);
        return (j);
}

