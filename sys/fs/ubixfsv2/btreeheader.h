/*#ifndef BTREEHEADER_H
#define BTREEHEADER_H

typedef struct bTreeHeader {
  u_int32_t treeDepth;
  u_int32_t treeWidth;
  u_int32_t treeLeafCount;
  off_t  firstNodeOffset; // used when tree is on disk
  off_t  firstDeleted;    // used to point to an empty node
  char paddington[4068];
} bTreeHeader; // bTreeHeader

#endif*/ /* !BTREEHEADER_H */
