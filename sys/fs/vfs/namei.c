#include <ubixos/errno.h>
#include <vfs/vfs.h>

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
    if (dir == _current->root) {
      *result = dir;
      return 0;
    } else if ((sb = dir->i_sb) && (dir == sb->s_mounted)) {
      sb = dir->i_sb;
      iput(dir);
      dir = sb->s_covered;
      if (!dir)
        return -ENOENT;
      dir->i_count++;
    }
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
  const char * thisname;
  int len, error;
  struct inode * inode;

  *res_inode = NULL;
  if (!base) {
    base = _current->pwd;
    base->i_count++;
  }
  if ((c = *pathname) == '/') {
    iput(base);
    base = _current->root;
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
  } else
    iput(base);

  *res_inode = inode;

  return 0;
}
