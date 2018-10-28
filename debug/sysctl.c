#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>

int mib[2];
size_t len;
char *p;

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
  
}
