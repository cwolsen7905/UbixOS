/*
#ifndef FILE_H
#define FILE_H

#include "ubixfs.h"

typedef struct fileDescriptor {
  struct fileDescriptor * prev;
  struct fileDescriptor * next;
  void * inode;
  off_t  offset;
  size_t size;
} fileDescriptor;

#endif *//* !FILE_H */
