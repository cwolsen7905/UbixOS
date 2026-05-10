#include <dirent.h>
#include <stdlib.h>

static int _closedir(DIR *dir);

asm(
  ".globl _closedir\n"
  "_closedir:\n"
  "movl $301,%eax\n"
  "int $0x80\n"
  "ret\n"
  );

int closedir(DIR *dir) {
  if (dir == 0x0)
    return (-1);
  _closedir(dir);
  free(dir);
  return (0);
}
