/*#include <stdio.h>
#include "vfs.h"

DiskFS::DiskFS(const char * filename) {
  diskFile = fopen(filename, "r+");
} // DiskFS::DiskFS

int
DiskFS::write(const void * data, long offset, long size) {
  if (diskFile == NULL) return 1;
  fseek(diskFile, offset, SEEK_SET);
  fwrite(data, size, 1, diskFile);
  return 0;
} // DiskFS::write

int
DiskFS::read(void * data, long offset, long size) {
  if (diskFile == NULL) return 1;
  fseek(diskFile, offset, SEEK_SET);
  fread(data, size, 1, diskFile);
  return 0;
} // DiskFS::read
*/
