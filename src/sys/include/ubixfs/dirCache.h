#ifndef DIRCACHE_H
#define DIRCACHE_H

/* #include "ubixfs.h" */
#include <ubixos/types.h>

struct cacheNode {
  char * name;
  struct cacheNode * prev;
  struct cacheNode * next;
  struct cacheNode * parent;
  struct cacheNode * fileListHead;
  struct cacheNode * fileListTail;
  void             * info; 
  int              * size;
  int                present;
  int                dirty;
  uInt32           * startCluster;
  uInt16           * attributes;
  uInt16           * permissions;
}; /* cacheNode */

struct cacheNode * ubixfs_cacheFind(struct cacheNode *, char *);
struct cacheNode * ubixfs_cacheNew(const char *);
void ubixfs_cacheDelete(struct cacheNode **);
struct cacheNode * ubixfs_cacheAdd(struct cacheNode *, struct cacheNode *);

#endif /* !DIRCACHE_H */
