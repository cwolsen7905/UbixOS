#include <dirent.h>

static int _readdir(DIR *dir);

asm(
  ".globl _readdir\n"
  "_readdir:\n"
  "movl $300,%eax\n"
  "int $0x80\n"
  "ret\n"
  );

struct dirent *readdir(DIR *dir) {
  if (dir == 0x0 || dir->dd_handle == 0x0)
    return (0x0);
  if (_readdir(dir) != 0)
    return (0x0);
  return (&dir->dd_ent);
}
