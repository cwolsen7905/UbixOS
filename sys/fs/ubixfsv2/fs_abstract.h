/*#ifndef FSABSTRACT_H
#define FSABSTRACT_H

#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <device.h>
#include "file.h"

class vfs_abstract {
 protected:
  vfs_abstract * prev;
  vfs_abstract * next;
  device_t * device;
 public: 
  // File I/O
  virtual int     vfs_open(const char *, fileDescriptor *,int,...) { return -1; };
  virtual int     vfs_close(fileDescriptor *) { return -1; };
  virtual size_t  vfs_read(fileDescriptor *, void *, off_t, size_t) 
                  { return 0; };
  virtual size_t  vfs_write(fileDescriptor *, void *, off_t, size_t) 
                  { return 0; };

  // Dir I/O
  virtual int     vfs_opendir(DIR *,const char *) { return -1; };
  virtual int     vfs_closedir(DIR *) { return -1; };
  virtual int     vfs_mkdir(const char *, mode_t) { return -1; };
  virtual int     vfs_rmdir(const char *) { return -1; };
  virtual int     vfs_readdir(DIR *,struct dirent *) { return -1; };

  // FS Functions
  virtual int     vfs_init(void) { return -1; };
  virtual int     vfs_format(device_t *) { return -1; };
  virtual void *  vfs_mknod(const char *, mode_t) { return NULL; };
  virtual int     vfs_purge(void) { return -1; };
  virtual int     vfs_stop(void) { return -1; };
  virtual int     vfs_sync(void) { return -1; };

  // Misc Functions
  virtual int     vfs_unlink(const char *) { return -1; };
  virtual int     vfs_rename(const char *,const char *) { return -1; };

  virtual     ~vfs_abstract(void) { };
}; // vfs_FS

#endif*/ // !FSABSTRACT_H
