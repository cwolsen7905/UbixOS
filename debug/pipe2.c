#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MSGSIZE 32


int main(int argc, char **argv) {
  int p[2];
  int pid;
  int j;

  char inbuf[MSGSIZE];

  if (pipe(p) == -1) {
    printf("pipe call error");
    exit(1);
  }

  for (j = 0; j < 2; j++)
    printf("p[%i]: %i\n", j, p[j]);

  switch (pid = fork()) {
    case 0:
      close(p[0]);
      write(p[1], "TEST 1", MSGSIZE);
      write(p[1], "TEST 2", MSGSIZE);
      write(p[1], "TEST 3", MSGSIZE);
      break;
    default:
      close(p[1]);
      for (j=0;j<3;j++) {
        read(p[0], inbuf, MSGSIZE);
        printf("Parent: %s\n", inbuf);
      }
  }

  return(0);
}
