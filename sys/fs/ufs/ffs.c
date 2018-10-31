#include <sys/vfs/vfs.h>
#include <fs/ufs/ufs.h>
#include <fs/ufs/ffs.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/kpanic.h>
#include <string.h>
#include <sys/buf.h>


int ffs_read(fileDescriptor_t *fd,char *data,uInt32 offset,long size) {
  struct fs  *fs;

  fs = (struct fs *)fd->dmadat->sbbuf;

  if (offset < fd->size && offset >= fs->fs_maxfilesize) {
    //return (EOVERFLOW);
    return(-1);
    }

  kprintf("Reading File w/ New Function [0x%X]\n",fs->fs_maxfilesize);
  return(0x0);
  }
