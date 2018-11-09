#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char *buf = (char *)malloc(1024);

  printf("__error: 0x%X\n", (u_int32_t)__error());
  printf("errno: %i\n", errno);
  printf("readlink: %i\n", readlink("/etc/malloc.conf", buf, 1024));
  printf("buf: [%s]\n", buf);
  printf("__error: 0x%X\n", (u_int32_t)__error());
  printf("errno: %i\n", errno);

  return(0);
}
