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
  fileDescriptor *fd =  fopen(path, "rb");
  kprintf("FD: 0x%X", fd);

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

