/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ubixos/errno.h>
#include <sys/sysproto.h>
#include <sys/sysproto_posix.h>
#include <fs/vfs/stat.h>
#include <fs/vfs/file.h>
#include <lib/kprintf.h>
#include <sys/descrip.h>
#include <string.h>

int _sys_stat(char *path, struct stat *sb, int flags) {
  int error = 0;
  //struct inode *inode;

  //MrOlsen kprintf("SYS_STAT {%s}", path);

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
   //MrOlsen kprintf("LSTAT(%i)=st_ino 0x%X, st_mode=0x%X, st_uid %i, st_gid %i, st_size=0x%X", error, sb->st_ino, sb->st_mode, sb->st_uid, sb->st_gid, sb->st_size);
   break;
   default:
   kprintf("STAT ERROR={%s}", path);
   error = -1;
   break;
   }
   */

  fileDescriptor_t *fd = fopen(path, "rb");
  //MrOlsen kprintf("FD=0x%X", fd);

  if (fd == 0) {
    error = -1;
  }
  else {
    sb->st_dev = 0xDEADBEEF;
    sb->st_ino = fd->ino;
    sb->st_mode = fd->inode.u.ufs2_i.di_mode;
    sb->st_nlink = fd->inode.u.ufs2_i.di_nlink;
    sb->st_uid = fd->inode.u.ufs2_i.di_uid;
    sb->st_gid = fd->inode.u.ufs2_i.di_gid;
    sb->st_rdev = 0xBEEFDEAD;
    sb->st_size = fd->inode.u.ufs2_i.di_size;
    sb->st_atime = fd->inode.u.ufs2_i.di_atime;
    sb->st_mtime = fd->inode.u.ufs2_i.di_mtime;
    sb->st_ctime = fd->inode.u.ufs2_i.di_ctime;

    fclose(fd);
  }

  return (error);
}

int sys_fstat(struct thread *td, struct sys_fstat_args *args) {
  int error = 0;

  struct file *fdd = 0x0;
  fileDescriptor_t *fd = 0x0;

  getfd(td, &fdd, args->fd);

  if (fdd == 0x0 || fdd->fd == 0x0) {
    td->td_retval[0] = EBADF;
    return (EBADF);
  }

  fd = fdd->fd;

  if (fd->res != 0x0) {
    args->sb->st_dev = 0xDEADBEEF;
    args->sb->st_ino = fd->ino;
    args->sb->st_rdev = 0xBEEFDEAD;
    args->sb->st_size = fd->size;
    args->sb->st_uid = 0;
    args->sb->st_gid = 0;
  }
  else {
    args->sb->st_dev = 0xDEADBEEF;
    args->sb->st_ino = fd->ino;
    args->sb->st_mode = fd->inode.u.ufs2_i.di_mode;
    args->sb->st_nlink = fd->inode.u.ufs2_i.di_nlink;
    args->sb->st_uid = fd->inode.u.ufs2_i.di_uid;
    args->sb->st_gid = fd->inode.u.ufs2_i.di_gid;
    args->sb->st_rdev = 0xBEEFDEAD;
    args->sb->st_size = fd->inode.u.ufs2_i.di_size;
    args->sb->st_atime = fd->inode.u.ufs2_i.di_atime;
    args->sb->st_mtime = fd->inode.u.ufs2_i.di_mtime;
    args->sb->st_ctime = fd->inode.u.ufs2_i.di_ctime;
  }

  td->td_retval[0] = error;
  return (error);
}

int sys_fstatat(struct thread *td, struct sys_fstatat_args *args) {
  int error = 0;

  struct file *fdd = 0x0;
  fileDescriptor_t *fd = 0x0;
  struct stat *sb = 0x0;
  int uP = 0x0;

  //kprintf("FSTATAT(%i:%s:%s)", args->fd, args->path, args->flag);

  if (strlen(args->path) > 0) {
    fd = fopen(args->path, "rb");
    uP = 1;
  }
  else {
    getfd(td, &fdd, args->fd);
    if (fdd == 0 || fdd->fd == 0x0) {
      error = -1;
    }
    else {
      fd = fdd->fd;
    }
  }

  if (fd == 0x0) {
    error = -1;
  }
  else {
    sb = args->buf;
    sb->st_dev = 0xDEADBEEF;
    sb->st_ino = fd->ino;
    sb->st_mode = fd->inode.u.ufs2_i.di_mode;
    sb->st_nlink = fd->inode.u.ufs2_i.di_nlink;
    sb->st_uid = fd->inode.u.ufs2_i.di_uid;
    sb->st_gid = fd->inode.u.ufs2_i.di_gid;
    sb->st_rdev = 0xBEEFDEAD;
    sb->st_size = fd->inode.u.ufs2_i.di_size;
    sb->st_atime = fd->inode.u.ufs2_i.di_atime;
    sb->st_mtime = fd->inode.u.ufs2_i.di_mtime;
    sb->st_ctime = fd->inode.u.ufs2_i.di_ctime;

    if (sb->st_mode == 0)
      sb->st_mode = 0x81ED; /* S_IFREG | 0755 */
    if (sb->st_nlink == 0)
      sb->st_nlink = 1;
    if (sb->st_size == 0 && fd->size != 0)
      sb->st_size = (off_t)fd->size;

    if (uP == 1)
      fclose(fd);
  }

  td->td_retval[0] = error;
  return (error);
}

int sys_fstatfs(struct thread *td, struct sys_fstatfs_args *args) {
  int error = 0;

  struct file *fd = 0x0;

  getfd(td, &fd, args->fd);

  if (fd == 0) {
    error = -1;
  }
  else {
    /*
     args->buf->st_dev = 0xDEADBEEF;
     args->buf->st_ino = fd->fd->ino;
     args->buf->st_mode = fd->fd->inode.u.ufs2_i.di_mode;
     args->buf->st_nlink = fd->fd->inode.u.ufs2_i.di_nlink;
     args->buf->st_uid = fd->fd->inode.u.ufs2_i.di_uid;
     args->buf->st_gid = fd->fd->inode.u.ufs2_i.di_gid;
     args->buf->st_rdev = 0xBEEFDEAD;
     args->buf->st_size = fd->fd->inode.u.ufs2_i.di_size;
     args->buf->st_atime = fd->fd->inode.u.ufs2_i.di_atime;
     args->buf->st_mtime = fd->fd->inode.u.ufs2_i.di_mtime;
     args->buf->st_ctime = fd->fd->inode.u.ufs2_i.di_ctime;
     //MrOlsen kprintf("LSTAT(%i)=st_ino 0x%X, st_mode=0x%X, st_uid %i, st_gid %i, st_size=0x%X", error, args->buf->st_ino, args->buf->st_mode, args->buf->st_uid, args->buf->st_gid, args->buf->st_size);
     //fclose(fd);
     */

    args->buf->f_version = 0x20030518;
    args->buf->f_type = 0x35;
    args->buf->f_flags = 0x1000;
    args->buf->f_bsize = 0x1000;
    args->buf->f_iosize = 0x8000;
    args->buf->f_blocks = 0x7bf9d;
    args->buf->f_bfree = 0x73c0c;
    args->buf->f_bavail = 0x69d5c;
    args->buf->f_files = 0x3fffe;
    args->buf->f_ffree = 0x3d3f0;
    args->buf->f_syncwrites = 0x75232;
    args->buf->f_asyncwrites = 0x2b804;
    args->buf->f_syncreads = 0x2edea;
    args->buf->f_asyncreads = 0x6182;
    args->buf->f_namemax = 0xFF;
    args->buf->f_owner = 0x0;
    args->buf->f_fsid.val[0] = 0x5A31F4F0;
    args->buf->f_fsid.val[1] = 0x65E7C98F;
//args->buf->f_charspare= {""};
    sprintf(args->buf->f_fstypename, "ufs");
    sprintf(args->buf->f_mntfromname, "/dev/ada0s1a");
    sprintf(args->buf->f_mntonname, "/");

  }

  td->td_retval[0] = error;
  return (error);

}

int sys_statfs(struct thread *td, struct sys_statfs_args *args) {
  int error = 0;
  fileDescriptor_t *fd = 0x0;

  fd = fopen(args->path, "rb");

  if (fd == 0) {
    error = -1;
  }
  else {
    /*
     args->buf->st_dev = 0xDEADBEEF;
     args->buf->st_ino = fd->fd->ino;
     args->buf->st_mode = fd->fd->inode.u.ufs2_i.di_mode;
     args->buf->st_nlink = fd->fd->inode.u.ufs2_i.di_nlink;
     args->buf->st_uid = fd->fd->inode.u.ufs2_i.di_uid;
     args->buf->st_gid = fd->fd->inode.u.ufs2_i.di_gid;
     args->buf->st_rdev = 0xBEEFDEAD;
     args->buf->st_size = fd->fd->inode.u.ufs2_i.di_size;
     args->buf->st_atime = fd->fd->inode.u.ufs2_i.di_atime;
     args->buf->st_mtime = fd->fd->inode.u.ufs2_i.di_mtime;
     args->buf->st_ctime = fd->fd->inode.u.ufs2_i.di_ctime;
     //MrOlsen kprintf("LSTAT(%i)=st_ino 0x%X, st_mode=0x%X, st_uid %i, st_gid %i, st_size=0x%X", error, args->buf->st_ino, args->buf->st_mode, args->buf->st_uid, args->buf->st_gid, args->buf->st_size);
     //fclose(fd);
     */

//kprintf("sFS: %s", args->path);
    args->buf->f_version = 0x20030518;
    args->buf->f_type = 0x35;
    args->buf->f_flags = 0x1000;
    args->buf->f_bsize = 0x1000;
    args->buf->f_iosize = 0x8000;
    args->buf->f_blocks = 0x7bf9d;
    args->buf->f_bfree = 0x73c0c;
    args->buf->f_bavail = 0x69d5c;
    args->buf->f_files = 0x3fffe;
    args->buf->f_ffree = 0x3d3f0;
    args->buf->f_syncwrites = 0x75232;
    args->buf->f_asyncwrites = 0x2b804;
    args->buf->f_syncreads = 0x2edea;
    args->buf->f_asyncreads = 0x6182;
    args->buf->f_namemax = 0xFF;
    args->buf->f_owner = 0x0;
    args->buf->f_fsid.val[0] = 0x5A31F4F0;
    args->buf->f_fsid.val[1] = 0x65E7C98F;
//args->buf->f_charspare= {""};
    sprintf(args->buf->f_fstypename, "ufs");
    sprintf(args->buf->f_mntfromname, "/dev/ada0s1a");
    sprintf(args->buf->f_mntonname, "/");

  }

  td->td_retval[0] = error;
  return (error);

}

/* Return stat of path do not follow if link return stat of link */
int sys_lstat(struct thread *td, struct sys_lstat_args *args) {
  td->td_retval[0] = _sys_stat(args->path, args->sb, STAT_LSTAT);
  return (0x0);
}

int sys_stat(struct thread *td, struct sys_stat_args *args) {
  td->td_retval[0] = _sys_stat(args->path, args->ub, STAT_LSTAT);
  return (0x0);
}

/*
 * sys_statx — Linux statx(2) compatibility (slot 383).
 *
 * musl uses statx as the backing for fstat/stat.  We handle the two
 * common call patterns:
 *   AT_EMPTY_PATH set, path==""  → fstat(dirfd)
 *   AT_FDCWD or path given       → stat(path)
 *
 * The struct statx is zeroed first so musl's conversion to struct stat
 * sees consistent values for fields we don't populate.
 */
int sys_statx(struct thread *td, struct sys_statx_args *args)
{
  struct statx *stx = args->stx;
  int error = 0;

  if (stx == 0x0) {
    td->td_retval[0] = EFAULT;
    return (EFAULT);
  }

  memset(stx, 0, sizeof(*stx));

  if ((args->flags & AT_EMPTY_PATH) &&
      (args->path == 0x0 || args->path[0] == '\0')) {
    /* fstat(dirfd) path */
    struct file *fdd = 0x0;
    fileDescriptor_t *fd = 0x0;

    getfd(td, &fdd, args->dirfd);
    if (fdd == 0x0) {
      td->td_retval[0] = EBADF;
      return (EBADF);
    }

    /* TTY placeholder fds have fd==NULL; return character-device stats so
     * isatty() works correctly. */
    if (fdd->fd == 0x0) {
      stx->stx_mask      = args->mask & STATX_BASIC_STATS;
      stx->stx_blksize   = 512;
      stx->stx_nlink     = 1;
      stx->stx_uid       = 0;
      stx->stx_gid       = 0;
      stx->stx_mode      = 0020620; /* S_IFCHR | 0620 */
      stx->stx_ino       = (uint64_t)(uintptr_t)args->dirfd + 1;
      stx->stx_size      = 0;
      stx->stx_blocks    = 0;
      stx->stx_dev_major = 5;
      stx->stx_dev_minor = 0;
      stx->stx_rdev_major = 5;
      stx->stx_rdev_minor = 0;
      td->td_retval[0] = 0;
      return (0);
    }
    fd = fdd->fd;

    stx->stx_mask       = args->mask & STATX_BASIC_STATS;
    stx->stx_blksize    = 512;
    stx->stx_nlink      = fd->inode.u.ufs2_i.di_nlink ? fd->inode.u.ufs2_i.di_nlink : 1;
    stx->stx_uid        = fd->inode.u.ufs2_i.di_uid;
    stx->stx_gid        = fd->inode.u.ufs2_i.di_gid;
    stx->stx_mode       = fd->res != 0x0 ? 0100644 : fd->inode.u.ufs2_i.di_mode;
    if ((stx->stx_mode & 0170000) == 0)
      stx->stx_mode |= 0100644; /* FAT: no type bits → regular file 644 */
    stx->stx_ino        = fd->ino;
    stx->stx_size       = fd->size;
    stx->stx_blocks     = (fd->size + 511) / 512;
    stx->stx_dev_major  = 1;
    stx->stx_dev_minor  = 1;
  } else {
    /* stat(path) path */
    const char *path = args->path;
    fileDescriptor_t *fd = 0x0;
    kDIR_t *dir = 0x0;

    if (path == 0x0 || path[0] == '\0') {
      td->td_retval[0] = ENOENT;
      return (ENOENT);
    }

    fd = fopen(path, "rb");
    if (fd == 0x0) {
      /* fopen rejects directories; try vfs_opendir to detect them. */
      dir = vfs_opendir(path);
      if (dir == 0x0) {
        kprintf("statx: ENOENT path=%s\n", path ? path : "(null)");
        td->td_retval[0] = ENOENT;
        return (ENOENT);
      }
      vfs_closedir(dir);

      stx->stx_mask      = args->mask & STATX_BASIC_STATS;
      stx->stx_blksize   = 512;
      stx->stx_nlink     = 2;
      stx->stx_uid       = 0;
      stx->stx_gid       = 0;
      stx->stx_mode      = 0040755; /* S_IFDIR | 0755 */
      stx->stx_ino       = 1;
      stx->stx_size      = 0;
      stx->stx_blocks    = 0;
      stx->stx_dev_major = 1;
      stx->stx_dev_minor = 1;
    } else {
      stx->stx_mask       = args->mask & STATX_BASIC_STATS;
      stx->stx_blksize    = 512;
      stx->stx_nlink      = fd->inode.u.ufs2_i.di_nlink ? fd->inode.u.ufs2_i.di_nlink : 1;
      stx->stx_uid        = fd->inode.u.ufs2_i.di_uid;
      stx->stx_gid        = fd->inode.u.ufs2_i.di_gid;
      stx->stx_mode       = fd->res != 0x0 ? 0100644 : fd->inode.u.ufs2_i.di_mode;
      if ((stx->stx_mode & 0170000) == 0)
        stx->stx_mode |= 0100755; /* FAT: no type bits → regular file 755 */
      kprintf("statx: OK path=%s mode=0%o\n", path, stx->stx_mode);
      stx->stx_ino        = fd->ino ? fd->ino : (uint64_t)(uintptr_t)fd;
      stx->stx_size       = fd->size;
      stx->stx_blocks     = (fd->size + 511) / 512;
      stx->stx_dev_major  = 1;
      stx->stx_dev_minor  = 1;
      fclose(fd);
    }
  }

  td->td_retval[0] = error;
  return (error);
}
