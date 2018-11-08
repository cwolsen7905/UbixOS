#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>

int mib[CTL_MAXNAME];
//int mib[2];
size_t len;
char *p;

static int name2oid(const char *name, int *oidp);

int main() {
  printf("SYSCTL\n");


  mib[0] = 1;
  mib[1] = 1;

  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);

  printf("[%s]\n", p); 

  mib[0] = 1;
  mib[1] = 10;

  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);

  printf("[%s]\n", p); 

  mib[0] = 1;
  mib[1] = 2;

  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);

  printf("[%s]\n", p); 

  mib[0] = 1;
  mib[1] = 4;

  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);

  printf("[%s]\n", p); 

  mib[0] = 6;
  mib[1] = 1;

  sysctl(mib, 2, NULL, &len, NULL, 0);
  p = malloc(len);
  sysctl(mib, 2, p, &len, NULL, 0);

  printf("[%s]\n", p); 
  
  size_t j;

  //j = name2oid("vm.overcommit", mib);
  j = name2oid("hw.pagesizes", mib);

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

