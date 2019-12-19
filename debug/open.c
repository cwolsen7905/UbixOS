#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int fd = 0;

  fd = open("./open.test.file", DWR);

   printf("open/fd: %i\n", fd);

  write(fd, "TEST\n", 5);

  return(0);
}
