/*#ifndef VFS_H
#define VFS_H

#include <stdlib.h>
#include <stdio.h>

class FileSystemAbstract {
 protected:
 public: 
  virtual   int read(char *, long, long) = 0;
  virtual   int write(char *, long, long) = 0;
  virtual  ~FileSystemAbstract(void) {};
}; // FileSystemAbstract

class DiskFS : public FileSystemAbstract {
 protected:
   FILE * diskFile;
 public:
            DiskFS(const char *);
  virtual   int write(const void *, long, long);
  virtual   int read(void *, long, long);
  virtual  ~DiskFS(void) { };
}; // DiskFS

#endif // !VFS_H
*/
