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
#include <vfs/stat.h>
#include <vfs/file.h>
#include <lib/kprintf.h>
#include <sys/descrip.h>

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

  fileDescriptor_t *fd =  fopen(path, "rb");
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
  //MrOlsen kprintf("LSTAT(%i)=st_ino 0x%X, st_mode=0x%X, st_uid %i, st_gid %i, st_size=0x%X", error, sb->st_ino, sb->st_mode, sb->st_uid, sb->st_gid, sb->st_size);
  fclose(fd);
  }

  return(error);
}


int sys_fstat(struct thread *td, struct sys_fstat_args *args) {
  int error = 0;

  struct file *fdd = 0x0;
  fileDescriptor_t *fd = 0x0;

  getfd(td, &fdd, args->fd);

  fd = fdd->fd;

  if (fdd == 0 || fdd->fd == 0x0) {
    error = -1;
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
  //MrOlsen kprintf("FSTAT(%i)=st_ino 0x%X, st_mode=0x%X, st_uid %i, st_gid %i, st_size=0x%X:0x%X", args->fd, args->sb->st_ino, args->sb->st_mode, args->sb->st_uid, args->sb->st_gid, args->sb->st_size, fd->size);
  }

  td->td_retval[0] = error;
  return(error);
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

args->buf->f_version=0x20030518;
args->buf->f_type=0x35;
args->buf->f_flags=0x1000;
args->buf->f_bsize=0x1000;
args->buf->f_iosize=0x8000;
args->buf->f_blocks=0x7bf9d;
args->buf->f_bfree=0x73c0c;
args->buf->f_bavail=0x69d5c;
args->buf->f_files=0x3fffe;
args->buf->f_ffree=0x3d3f0;
args->buf->f_syncwrites=0x75232;
args->buf->f_asyncwrites=0x2b804;
args->buf->f_syncreads=0x2edea;
args->buf->f_asyncreads=0x6182;
args->buf->f_namemax=0xFF;
args->buf->f_owner=0x0;
args->buf->f_fsid.val[0]=0x5A31F4F0;
args->buf->f_fsid.val[1]=0x65E7C98F;
//args->buf->f_charspare= {""};
sprintf(args->buf->f_fstypename, "ufs");
sprintf(args->buf->f_mntfromname, "/dev/ada0s1a");
sprintf(args->buf->f_mntonname,"/");


  }

  td->td_retval[0] = error;
  return(error);

  
}

/* Return stat of path do not follow if link return stat of link */
int sys_lstat(struct thread *td, struct sys_lstat_args *args) {
  td->td_retval[0] = _sys_stat(args->path, args->sb, STAT_LSTAT);
  return(0x0);
}

int sys_stat(struct thread *td, struct sys_stat_args *args) {
  td->td_retval[0] = _sys_stat(args->path, args->ub, STAT_LSTAT);
  return(0x0);
}
