#include <ubixos/errno.h>
#include <vfs/stat.h>
#include <vfs/vfs.h>

int sys_stat(char *path, struct stat *sb, int flags) {
  int error = 0;
  struct inode *inode;

  //NOTE: Should we verify that the memory is writable?
  kprintf("SYS_STAT {%s}", path);

  switch (flags) {
    case STAT_LSTAT:
      namei(path, NULL, STAT_NO_FOLLOW, &inode);
      sb.st_dev = inode->i_dev;
      sb.st_ino = inode->i_ino;
      sb.st_mode = inode->i_mode;
      sb.st_nlink = inode->i_nlink;
      sb.st_uid = inode->i_uid;
      sb.st_gid = inode->i_gid;
      sb.st_rdev = inode->i_rdev;
      sb.st_size = inode->i_size;
      sb.st_atime = inode->i_atime;
      sb.st_mtime = inode->i_mtime;
      sb.st_ctime = inode->i_ctime;
      break;
    default:
      kprintf("STAT ERROR: {%s}", path);
      error = -1;
     break;
  }

  sb->st_dev = 0x5E;
  sb->st_ino = 0x3003;
  sb->st_mode = 0x41FF;
  sb->st_nlink = 0x2;
  sb->st_uid = 0x0;
  sb->st_gid = 0x0;
  sb->st_rdev = 0x7FF3;
  sb->st_size = 0xFFFFEB70;

  return(error);
}


/* Return stat of path do not follow if link return stat of link */
int sys_lstat(struct thread *td, struct sys_lstat_args *args) {
  td->td_retval[0] = sys_stat(args->path, args->sb, STAT_LSTAT);
  return(0x0);
}

