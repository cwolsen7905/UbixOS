#include <ubixos/errno.h>
#include <vfs/stat.h>
#include <vfs/vfs.h>

int sys_stat(char *path, struct stat *sb, int flags) {
  int error = 0;
  struct inode *inode;

  //NOTE: Should we verify that the memory is writable?
  kprintf("SYS_STAT {%s}", path);
/*
  switch (flags) {
    case STAT_LSTAT:
      error = namei(path, NULL, STAT_NO_FOLLOW, &inode);
      sb->st_dev = inode->i_dev;
      sb->st_ino = inode->i_ino;
      sb->st_mode = inode->i_mode;
      sb->st_nlink = inode->i_nlink;
      sb->st_uid = inode->i_uid;
      sb->st_gid = inode->i_gid;
      sb->st_rdev = inode->i_rdev;
      sb->st_size = inode->i_size;
      sb->st_atime = inode->i_atime;
      sb->st_mtime = inode->i_mtime;
      sb->st_ctime = inode->i_ctime;
      kprintf("LSTAT(%i): st_ino 0x%X, st_mode: 0x%X, st_uid %i, st_gid %i, st_size: 0x%X", error, sb->st_ino, sb->st_mode, sb->st_uid, sb->st_gid, sb->st_size);
      break;
    default:
      kprintf("STAT ERROR: {%s}", path);
      error = -1;
     break;
  }
*/
  fileDescriptor *fd =  fopen(path, "r");

  if (fd == 0) {
    error = -1;
  }
  else {
  sb->st_dev = 0xDEADBEEF;
  sb->st_ino = fd->ino;
  sb->st_mode = fd->inode.ufs2->di_mode;
  sb->st_nlink = fd->inode.ufs2->di_nlink;
  sb->st_uid = fd->inode.ufs2->di_uid;
  sb->st_gid = fd->inode.ufs2->di_gid;
  sb->st_rdev = 0xBEEFDEAD;
  sb->st_size = fd->inode.ufs2->di_size;
  sb->st_atime = fd->inode.ufs2->di_atime;
  sb->st_mtime = fd->inode.ufs2->di_mtime;
  sb->st_ctime = fd->inode.ufs2->di_ctime;
  kprintf("LSTAT(%i): st_ino 0x%X, st_mode: 0x%X, st_uid %i, st_gid %i, st_size: 0x%X", error, sb->st_ino, sb->st_mode, sb->st_uid, sb->st_gid, sb->st_size);
  fclose(fd);
  }

  return(error);
}


/* Return stat of path do not follow if link return stat of link */
int sys_lstat(struct thread *td, struct sys_lstat_args *args) {
  td->td_retval[0] = sys_stat(args->path, args->sb, STAT_LSTAT);
  return(0x0);
}

