#ifndef BTREE_H
#define BTREE_H

#include <stdio.h>

#include "ubixfs.h"
#include "btreeheader.h"
#include "file.h"

#define B_NODE_MAGIC_1 0xDEADBEEF
#define B_NODE_MAGIC_2 0x1900BABE

#define B_MAX_KEYS 15
#define B_MAX_NAME_LENGTH 240
#define B_MAX_CHILD_COUNT 4

// if any of these structs change they have to be updated in the format
// utility too

typedef struct bNode { 
  uInt32  magic1                          __attribute__ ((packed));
  uInt32  used                            __attribute__ ((packed));
  uPtr    parent                          __attribute__ ((packed));
  uInt64  tag                             __attribute__ ((packed));
  char    keys[B_MAX_KEYS][B_MAX_NAME_LENGTH]      __attribute__ ((packed));
  bool    present[B_MAX_KEYS+1]           __attribute__ ((packed));
  uPtr    head[B_MAX_KEYS+1]              __attribute__ ((packed));
  uPtr    tail[B_MAX_KEYS+1]              __attribute__ ((packed));
  uInt32   childCount[B_MAX_KEYS+1]        __attribute__ ((packed));
  uInt32  magic2                          __attribute__ ((packed));
  bool    leaf                            __attribute__ ((packed));
  char reserved[131] __attribute__ ((packed));
} bNode; // bNode

struct ubixfsInode;

class bTree {
 protected:
  bNode          * root;
  UbixFS         * fs;
  bTreeHeader    * header;
  fileDescriptor * fd;
  uInt32           tag;
  ubixfsInode    * treeSearch(bNode *, const char *);
  ubixfsInode    * inodeSearch(ubixfsInode *, const char *);
  void             splitNode(bNode *);
  bNode          * allocEmptyNode(void);
  void             insertNode(bNode *, const char *, bNode *);
  bNode          * findLeafNode(bNode *, const char *);
  void             Print(bNode *);
  void             saveNode(FILE *, bNode *, void *);
 public:
                   bTree(const char *, ubixfsInode *);
                   bTree(UbixFS *, fileDescriptor *);
  ubixfsInode    * Find(const char *);
  ubixfsInode    * GetFirstNode(void);
  ubixfsInode    * GetFirstNode(bNode *);
  bool             Delete(const char *);
  void             Info(void);
  void             Info(const bNode *);
  bool             Insert(const char *, ubixfsInode *);
  bool             Save(const char *);
  bool             Load(const char *);
  void             Print(void);
  void             PrintWholeTree(void);
  bool             Verify(void);
                  ~bTree(void);
  friend class UbixFS;
}; // bTree
#endif // !BTREE_H
