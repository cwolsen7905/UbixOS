#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
  int fd     = 0x0;
  int curcnt = 0x1;
  int maxcnt = 0x0;
  char c;

  if (argc < 2) {
    printf("Error: No file specified\n");
    exit(0x0);
    }

  fd = open(argv[1],O_RDONLY);

  if (fd == 0x0) {
    printf("Error: Not a valid file: [%s]\n",argv[1]);
    exit(0x0);
    }

  printf("File opened...\n");

  while (read(fd,&c,1) != 0x0) {
    if (c == 0x0C) {
      if (curcnt > maxcnt)
        maxcnt = curcnt;
      curcnt = 1;
      }
    else if (c == 0x0A)
      curcnt++;
    }
  close(fd);
  printf("Max line count is: %i\n", maxcnt);
  return(0x0);
  }
