#include <sys/types.h>


void *getDrives() {
  uInt32 ptr = 0x0;
  asm(
    "int %0\n"
    : : "i" (0x80),"a" (45),"b" (&ptr)
    );
  return((void *)ptr);
  }
