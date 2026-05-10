#include <dirent.h>
#include <stdlib.h>

static int _opendir(const char *path, DIR *dir);

asm(
  ".globl _opendir\n"
  "_opendir:\n"
  "movl $299,%eax\n"
  "int $0x80\n"
  "ret\n"
  );

DIR *opendir(const char *path) {
  DIR *dir = malloc(sizeof(DIR));
  if (dir == 0x0)
    return (0x0);
  dir->dd_handle = 0x0;
  _opendir(path, dir);
  if (dir->dd_handle == 0x0) {
    free(dir);
    return (0x0);
  }
  return (dir);
}
