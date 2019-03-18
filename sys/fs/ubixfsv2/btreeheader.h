/*#ifndef BTREEHEADER_H
#define BTREEHEADER_H

typedef struct bTreeHeader {
  uInt32 treeDepth;
  uInt32 treeWidth;
  uInt32 treeLeafCount;
  off_t  firstNodeOffset; // used when tree is on disk
  off_t  firstDeleted;    // used to point to an empty node
  char paddington[4068];
} bTreeHeader; // bTreeHeader

#endif*/ /* !BTREEHEADER_H */
