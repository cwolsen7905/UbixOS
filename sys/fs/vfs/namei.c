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

#include <sys/types.h>
#include <ubixos/errno.h>
#include <sys/vfs/vfs.h>
#include <ubixos/sched.h>

int follow_link(struct inode * dir, struct inode * inode, int flag, int mode, struct inode ** res_inode) {

  if (!dir || !inode) {
    iput(dir);
    iput(inode);
    *res_inode = NULL;
    return -ENOENT;
  }

  if (!inode->i_op || !inode->i_op->follow_link) {
    iput(dir);
    *res_inode = inode;
    return 0;
  }

  return inode->i_op->follow_link(dir, inode, flag, mode, res_inode);
}

int permission(struct inode * inode, int mask) {
  int mode = inode->i_mode;

  if (inode->i_op && inode->i_op->permission)
    return inode->i_op->permission(inode, mask);
  else if (_current->euid == inode->i_uid)
    mode >>= 6;
  else if (in_group_p(inode->i_gid))
    mode >>= 3;

  if (((mode & mask & 0007) == mask)) // || suser())
    return 1;

  return 0;
}

int lookup(struct inode * dir, const char * name, int len, struct inode ** result) {
  struct super_block * sb;
  int perm;

  *result = NULL;

  if (!dir)
    return -ENOENT;

  /* check permissions before traversing mount-points */
  perm = permission(dir, MAY_EXEC);

  if (len == 2 && name[0] == '.' && name[1] == '.') {

#ifdef _IGNORE
    if (dir == _current->root) {
      *result = dir;
      return 0;
    }
    else if ((sb = dir->i_sb) && (dir == sb->s_mounted)) {
      sb = dir->i_sb;
      iput(dir);
      dir = sb->s_covered;
      if (!dir)
        return -ENOENT;
      dir->i_count++;
    }
#endif

  }

  if (!dir->i_op || !dir->i_op->lookup) {
    iput(dir);
    return -ENOTDIR;
  }

  if (!perm) {
    iput(dir);
    return -EACCES;
  }

  if (!len) {
    *result = dir;
    return 0;
  }

  return dir->i_op->lookup(dir, name, len, result);
}

static int dir_namei(const char * pathname, int * namelen, const char ** name, struct inode * base, struct inode ** res_inode) {
  char c;
  const char *thisname;
  int len;
  int error;
  struct inode *inode;

  *res_inode = NULL;

#ifdef _IGNORE
  if (!base) {
    base = _current->pwd;
    base->i_count++;
  }
#endif

  if (!base) {
    kprintf("BASE == NULL");
  }

  if ((c = *pathname) == '/') {
    iput(base);
#ifdef _IGNORE
    base = _current->root;
#endif
    pathname++;
    base->i_count++;
  }

  while (1) {
    thisname = pathname;
    for (len = 0; (c = *(pathname++)) && (c != '/'); len++)
      /* nothing */;
    if (!c)
      break;
    base->i_count++;
    error = lookup(base, thisname, len, &inode);
    if (error) {
      iput(base);
      return error;
    }
    error = follow_link(base, inode, 0, 0, &base);
    if (error)
      return error;
  }
  if (!base->i_op || !base->i_op->lookup) {
    iput(base);
    return -ENOTDIR;
  }
  *name = thisname;
  *namelen = len;
  *res_inode = base;
  return 0;
}

int namei(const char * pathname, struct inode * base, int follow_links, struct inode ** res_inode) {
  const char *basename;
  int namelen, error;
  struct inode *inode;

  *res_inode = NULL;

  error = dir_namei(pathname, &namelen, &basename, base, &base);
  if (error)
    return error;

  base->i_count++; /* lookup uses up base */

  error = lookup(base, basename, namelen, &inode);

  if (error) {
    iput(base);
    return error;
  }

  if (follow_links) {
    error = follow_link(base, inode, 0, 0, &inode);
    if (error)
      return error;
  }
  else
    iput(base);

  *res_inode = inode;

  return 0;
}
