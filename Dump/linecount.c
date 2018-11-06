#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
  int fd     = 0x0;
  int curcnt = 0x0;
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

  curcnt = 0;
  maxcnt = 0;
  printf("File opened...\n");

  while (read(fd,&c,1) != 0x0) {
    if (c == 0x0C) {
      printf("linefeed found!\n\tcurcnt = %i\n\tmaxcnt = %i\n", curcnt, maxcnt);
      if (curcnt > maxcnt)
        maxcnt = curcnt;
      curcnt = 0;
      }
    else if (c == 0x0D) {
      read(fd,&c,1);
      if (c = 0x0A)
        curcnt++;
      printf("DDD");
      }
    }
  close(fd);
  printf("Max line count is: %i\n", maxcnt);
  return(0x0);
  }
