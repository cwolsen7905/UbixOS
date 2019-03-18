// http://www.cs.msstate.edu/~cs2314/global/BTreeAnimation/algorithm.html

/*

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <assert.h>
#include "btree.h"
#include "ubixfs.h"

using namespace std;
#define VERIFY(x, y, z, n) if ((x) != (y)) { cout << "verify " << z << " failed" << endl; PrintWholeTree(); }

bTree::bTree(UbixFS * filesystem, fileDescriptor * myfd) {
  size_t result = 0;

  root = NULL;
  tag = 0;
  fs = filesystem;
  fd = myfd;
  header = new bTreeHeader;
  assert(header);
  memset(header, 0, sizeof(bTreeHeader));
  assert(fs);
  result = fs->vfs_read(fd, header, 0, sizeof(bTreeHeader));
  assert(result == sizeof(bTreeHeader));

  // If there are any files in this dir, load the first node of the b+tree
  if (header->treeLeafCount != 0) {
    assert(header->firstNodeOffset != 0);
    root = new bNode;
    assert(root);
    result = fs->vfs_read(fd, root, header->firstNodeOffset, sizeof(bNode));
    assert(result == sizeof(bNode));
  } // if 

} // bTree::bTree

bTree::bTree(const char * key, ubixfsInode * inode) {
// once the FS and the bTree are interfaced, this should go away
  root = NULL;
  tag = 0;
  header = new bTreeHeader;
  assert(header);
  memset(header, 0, sizeof(bTreeHeader));
  header->treeDepth = 1;
  header->treeWidth = 0;
  header->treeLeafCount = 0;
  header->firstDeleted = -1;
  header->firstNodeOffset = sizeof(bTreeHeader);

  if (inode == NULL) return;
  root = allocEmptyNode();
  if (root == NULL) return;
  root->used = 1;
  root->parent.bPtr = NULL;
  root->leaf = true;
  root->childCount[1] = 1;

// cout << "---Creating " << inode->name << "@" << inode << endl;
  strncpy(root->keys[0], key, B_MAX_NAME_LENGTH);
  // insert pointer to data page to the right of the data
  root->head[1].iPtr = inode;
  root->tail[1].iPtr = inode;

  root->present[1] = true;
  if (inode != NULL) {
    inode->next.bPtr = inode->prev.bPtr = NULL;
  } // if
  return;
} // bTree:bTree

bool
bTree::Insert(const char * key, ubixfsInode * inode) {
  bNode * bnode = root;
  ubixfsInode * tmpInode = NULL;
  unsigned int curSlot = 0;

  if (inode == NULL) return false;
 
  // note: this code is right out of the constructor
  if (root == NULL) {
    if (header == NULL) header = new bTreeHeader;
    assert(header);
    memset(header, 0, sizeof(bTreeHeader));
    header->treeDepth = 1;
    header->treeWidth = 0;
    header->treeLeafCount = 0;
    header->firstDeleted = -1;
    header->firstNodeOffset = sizeof(bTreeHeader);

    root = allocEmptyNode();
    assert(root);
    if (root == NULL) return false;
    
    root->used = 1;
    root->parent.bPtr = NULL;
    root->leaf = true;
    root->childCount[1] = 1;

    strncpy(root->keys[0], key, B_MAX_NAME_LENGTH);
    // insert pointer to data page to the right of the data
    root->head[1].iPtr = inode;
    root->tail[1].iPtr = inode;

    root->present[1] = true;
    inode->next.iPtr = inode->prev.iPtr = NULL;
    return true;
  } // if
    
  tmpInode = Find(key);
  if (tmpInode != NULL) return false;
//  PrintWholeTree();
// cout << "Insert(" << key << ")" << endl;
//Info(bnode);
  ++header->treeLeafCount;
  //Find the leaf node the inode goes into

  assert(bnode->used);
// cout << "---Inserting " << inode->name << " @ " << inode << endl; 
  while (bnode != NULL && !bnode->leaf) {
    if (strcmp(key, bnode->keys[0]) < 0) {
      bnode = bnode->head[0].bPtr;
    } else {
      if (strcmp(key, bnode->keys[bnode->used-1]) >= 0) {
        bnode = bnode->head[bnode->used].bPtr;
      } else {
        for (unsigned int i = 1; i < bnode->used; i++) {
          if (strcmp(key, bnode->keys[i]) < 0) {
            bnode = bnode->head[i].bPtr;
            break;
          } // if
        } // for i
      } // else
    }
  } // while


assert(bnode);
if (bnode->leaf != true) cout << "leafnode!=true" << endl;
assert(inode);

  if (strcmp(key, bnode->keys[curSlot = 0]) < 0)
    tmpInode = bnode->head[curSlot].iPtr;
  else
    if (strcmp(key, bnode->keys[(curSlot = bnode->used)-1]) >= 0)
      tmpInode = bnode->head[bnode->used].iPtr;
    else {
      for (curSlot = 1; curSlot < bnode->used; curSlot++) {
        if (strcmp(key, bnode->keys[curSlot]) < 0) {
          tmpInode = bnode->head[curSlot].iPtr;
          break;
        } // if
      } // for curSlot
      tmpInode = bnode->head[curSlot].iPtr;
    } // else


  if (tmpInode == NULL) {
    // This is the first node in this leaf
    bnode->head[curSlot].iPtr = bnode->tail[curSlot].iPtr = inode;
    bnode->present[curSlot] = true;

    if (curSlot == 0) {

      if (bnode->head[1].iPtr != NULL)  {
        ubixfsInode * iptr = bnode->head[1].iPtr;
        inode->prev.iPtr = iptr->prev.iPtr;
        inode->next.iPtr = iptr;
        iptr->prev.iPtr = inode;
        if (inode->prev.iPtr != NULL) 
          inode->prev.iPtr->next.iPtr = inode;
      } else {
        inode->next.iPtr = inode->prev.iPtr = NULL;
      } // else

    } else {
      ++bnode->used; 
    } // else

  } else {
    // Add node to leaf page. Scan through to find where it goes.

    if (strcmp(key, bnode->head[curSlot].iPtr->name) < 0)
    {

      inode->next.iPtr = bnode->head[curSlot].iPtr;
      inode->prev.iPtr = inode->next.iPtr->prev.iPtr;
      inode->next.iPtr->prev.iPtr = inode;
      if (inode->prev.iPtr != NULL) inode->prev.iPtr->next.iPtr = inode;
      bnode->head[curSlot].iPtr = inode;

    } else {

      if (strcmp(key, bnode->tail[curSlot].iPtr->name) > 0) {

        inode->prev.iPtr = bnode->tail[curSlot].iPtr;
        inode->next.iPtr = inode->prev.iPtr->next.iPtr;
        inode->prev.iPtr->next.iPtr = inode;

        if (inode->next.iPtr != NULL) inode->next.iPtr->prev.iPtr = inode;
        bnode->tail[curSlot].iPtr = inode;

      } else {


        ubixfsInode * tmpInode = bnode->head[curSlot].iPtr;
        for (unsigned int i = 0; i < bnode->childCount[curSlot]; i++) {
          if (strcmp(key, tmpInode->name) < 0) {
            inode->next.iPtr = tmpInode;
            inode->prev.iPtr = tmpInode->prev.iPtr;
            inode->next.iPtr->prev.iPtr = inode;
            inode->prev.iPtr->next.iPtr = inode;
            break;
          } // if
          tmpInode = tmpInode->next.iPtr; 
        } // for i

      } // else

    } // else

  } // else



  if (++bnode->childCount[curSlot] == B_MAX_CHILD_COUNT) {

// cout << "---- before split ----" << endl;
// Info(bnode);

    if (curSlot != bnode->used) {
      int shift = bnode->used - curSlot +1;

      memmove(&bnode->head[curSlot+1],
              &bnode->head[curSlot],
              sizeof(bnode->head[0]) * shift);
      memmove(&bnode->tail[curSlot+1],
              &bnode->tail[curSlot],
              sizeof(bnode->tail[0]) * shift);
      memmove(&bnode->present[curSlot+1],
              &bnode->present[curSlot],
              sizeof(bnode->present[0]) * shift);
      memmove(&bnode->childCount[curSlot+1],
              &bnode->childCount[curSlot],
              sizeof(bnode->childCount[0]) * shift);

      memmove(&bnode->keys[curSlot+1],
              &bnode->keys[curSlot],
              sizeof(bnode->keys[0]) * (shift-1));
      memset(bnode->keys[curSlot], 0, B_MAX_NAME_LENGTH);
    } else {
      bnode->head[curSlot+1] = bnode->head[curSlot];
      bnode->tail[curSlot+1] = bnode->tail[curSlot];
      bnode->childCount[curSlot+1] = bnode->childCount[curSlot];
      bnode->present[curSlot+1] = bnode->present[curSlot];
    } // else

    ubixfsInode * tmpInode = bnode->head[curSlot].iPtr;

    for (unsigned int i = 0; i < (B_MAX_CHILD_COUNT+1) >> 1; i++) {
      assert(tmpInode);
      tmpInode = tmpInode->next.iPtr;
    } // for i

    strncpy(bnode->keys[curSlot], tmpInode->name, B_MAX_NAME_LENGTH);
    bnode->head[curSlot+1].iPtr = tmpInode;
    bnode->tail[curSlot].iPtr = tmpInode->prev.iPtr;
    bnode->childCount[curSlot] = (B_MAX_CHILD_COUNT+1) >> 1;
    bnode->childCount[curSlot+1] -= bnode->childCount[curSlot];
    bnode->present[curSlot] = true;
    ++header->treeWidth;
    if (++bnode->used == B_MAX_KEYS) splitNode(bnode);

  } // if leaf is full
// Info(bnode);
  return true;
} // bTree::Insert

void 
bTree::splitNode(bNode * oldNode) {
  ubixfsInode * tmpInode = NULL;
  assert(oldNode);
  if (oldNode == NULL) return;
  if (oldNode->used != B_MAX_KEYS) return;

  bNode * newNode = allocEmptyNode();
  if (newNode == NULL) return;

  unsigned int shift = B_MAX_KEYS >> 1;
  unsigned int splitLoc = B_MAX_KEYS - shift;
  ++ shift;
// cout << "oldNode before split: " << endl;
// Info(oldNode);
// cout << "splitLoc: " << splitLoc << endl;
// cout << "shift: " << shift << endl;

  newNode->used = oldNode->used = B_MAX_KEYS >> 1;
  newNode->parent.bPtr = oldNode->parent.bPtr;
  newNode->leaf = oldNode->leaf;

// cout << "newNode->used: " << newNode->used << endl;
// cout << "oldNode->used: " << oldNode->used << endl;

  memcpy(&newNode->keys[0],
         &oldNode->keys[splitLoc],
         sizeof(newNode->keys[0]) * (shift-1));
  
  memset(&oldNode->keys[splitLoc], 0, sizeof(newNode->keys[0]) * (shift-1));

  memcpy(&newNode->present[0],
         &oldNode->present[splitLoc],
         sizeof(newNode->present[0]) * shift);

  memset(&oldNode->present[splitLoc], 0, sizeof(newNode->present[0]) * shift);

  memcpy(&newNode->head[0],
         &oldNode->head[splitLoc],
         sizeof(newNode->head[0]) * shift);

  memset(&oldNode->head[splitLoc], 0, 
         sizeof(newNode->head[0]) * shift);

  memcpy(&newNode->tail[0],
         &oldNode->tail[splitLoc],
         sizeof(newNode->tail[0]) * shift);

  memset(&oldNode->tail[splitLoc], 0, 
         sizeof(newNode->tail[0]) * shift);

  memcpy(&newNode->childCount[0],
         &oldNode->childCount[splitLoc],
         sizeof(newNode->childCount[0]) * shift);

  memset(&oldNode->childCount[splitLoc], 0, 
         sizeof(newNode->childCount[0]) * shift);

  if (!newNode->leaf) {
    for (unsigned int i = 0; i <= newNode->used; i++) {
      newNode->head[i].bPtr->parent.bPtr = newNode;
    } // for i
  } // if newNode isn't a leaf

  tmpInode = GetFirstNode(newNode);
  assert(tmpInode);

  if (oldNode == root) {
    // allocate a new root node
    ++header->treeDepth;
    root = allocEmptyNode();
    oldNode->parent.bPtr = root;
    newNode->parent.bPtr = root;
 //   strncpy(root->keys[0], newNode->keys[0], B_MAX_NAME_LENGTH);
    strncpy(root->keys[0], tmpInode->name, B_MAX_NAME_LENGTH);
    root->head[0].bPtr = oldNode;
    root->tail[0].bPtr = root->tail[1].bPtr = NULL;
    root->head[1].bPtr = newNode;
    root->used = 1;
    root->leaf = false;
    root->present[0] = root->present[1] = true;
    root->childCount[0] = root->childCount[1] = 0;
//    root->childCount[0] = oldNode->used;
//    root->childCount[1] = newNode->used;

// cout << "parent" << endl;
// Info(newNode->parent);
// cout << "oldNode" << endl;
// Info(oldNode);
// cout << "-----" << endl;
// cout << "newNode" << endl;
// Info(newNode);
// cout << "-----" << endl;

  } else {
    insertNode(newNode->parent.bPtr, tmpInode->name, newNode);
   // if (oldNode->parent->used == B_MAX_KEYS) splitNode(oldNode->parent);
  } // else
  return;
} // bTree::splitNode

void
bTree::insertNode(bNode * node, const char * key, bNode * headPtr) {
  unsigned int curSlot = 0;
  if (node == NULL || key == NULL) return;

  if (strcmp(key, node->keys[node->used-1]) >= 0){
    curSlot = node->used;
    memset(node->keys[curSlot], 0, B_MAX_NAME_LENGTH);
    strncpy(node->keys[curSlot], key, B_MAX_NAME_LENGTH);
    node->head[curSlot+1].bPtr = headPtr;
    node->tail[curSlot+1].bPtr = NULL;
    node->present[curSlot+1] = true;
    node->childCount[node->used] = 0; // maybe?

  } else {

    for (curSlot = 0; curSlot < node->used; curSlot++) {
      if (strcmp(key, node->keys[curSlot]) < 0) break;
    } // for 


    *//*
     * note that there is one more item for everything but keys
     * So, make the shift count +1 and just subtract it from the key shift
     * later
     *//*
    int shift = node->used - curSlot +1;

    memmove(&node->head[curSlot+1],
            &node->head[curSlot],
            sizeof(node->head[0]) * shift);
    memmove(&node->tail[curSlot+1],
            &node->tail[curSlot],
            sizeof(node->tail[0]) * shift);
    memmove(&node->present[curSlot+1],
            &node->present[curSlot],
            sizeof(node->present[0]) * shift);
    memmove(&node->childCount[curSlot+1],
            &node->childCount[curSlot],
            sizeof(node->childCount[0]) * shift);

    memmove(&node->keys[curSlot+1],
            &node->keys[curSlot],
            sizeof(node->keys[0]) * (shift-1));
    
    memset(node->keys[curSlot], 0, B_MAX_NAME_LENGTH);
    strncpy(node->keys[curSlot], key, B_MAX_NAME_LENGTH);
    node->head[curSlot+1].bPtr = headPtr;
    node->tail[curSlot+1].bPtr = NULL;
    node->present[curSlot+1] = true;
//    node->childCount[node->used] = ?;
  } // else 
  if (++node->used == B_MAX_KEYS) splitNode(node); 
  return;
} // bTree::insertNode

bNode *
bTree::allocEmptyNode(void) {
  bNode * newNode = new bNode;

  memset(newNode, 0, sizeof(bNode));
  newNode->magic1 = B_NODE_MAGIC_1;
  newNode->magic2 = B_NODE_MAGIC_2;
  newNode->parent.bPtr = NULL;
  newNode->tag = ++tag; // this will start at 1 (0 is the header node)
  return newNode;
} // bTree::allocEmptyNode

void
bTree::Info(const bNode * node) {
  ubixfsInode * inode = NULL;
  if (node == NULL || root == NULL) return;
  cout <<  node << " | " << node->parent.bPtr << endl;
  for (unsigned int i = 0; i <= node->used; i++) {
    inode = node->head[i].iPtr;
//    cout << "(" << node->childCount[i] << ")";
    for (unsigned int k = 0; k < node->childCount[i]; k++) {
      cout << "[" << inode->name << "]";
      inode = inode->next.iPtr;
    } // for k
    if (i != node->used) cout << " {" << node->keys[i] << "} ";
  } // for i
  cout << endl;
  return;
#if 0
  for (unsigned int i = 0; i < node->used; i++) {
    cout << "keys[" << i << "]: " << node->keys[i] << "  ";
  } // for i
  cout << endl;
  cout << "node->used: " << node->used << endl;
  cout << "leaf: " << node->leaf << endl;
  for (unsigned int i = 0; i <= node->used; i++) {
    inode = (ubixfsInode *)node->head[i];
cout << "node->childCount["<<i<<"]: " << node->childCount[i] << endl;
    for (unsigned int j = 0; j < node->childCount[i]; j++) {
      assert(inode);
      cout << "[" << i << "].[" << j << "]->" << inode->name << endl;
      inode = inode->next;
    } // for j
  } // for i
#endif
} // bTree::Info

void
bTree::Info(void) {
  ubixfsInode * inode = NULL;

  cout << "tree depth: " << header->treeDepth << endl;
  cout << "tree width: " << header->treeWidth << endl;
  cout << "tree leaf count: " << header->treeLeafCount << endl;
  cout << "tag: " << tag << endl;

  if (root == NULL) return;

  for (unsigned int i = 0; i <= root->used; i++) {
    cout << "CC[" << i << "]: " << root->childCount[i] << "  ";
  } // for i

  cout << endl;
  for (unsigned int i = 0; i <= root->used; i++) {
    cout << "CH[" << i << "]: " << root->head[i].bPtr << "  ";
  } // for i

  cout << endl;
  for (unsigned int i = 0; i <=root->used; i++) {
    cout << "CT[" << i << "]: " << root->tail[i].bPtr << "  ";
  } // for i
  cout << endl;
  for (unsigned int i = 0; i < root->used; i++) {
    cout << "keys[" << i << "]: " << root->keys[i] << "  ";
  } // for i
  cout << endl;

cout << "root->used: " << root->used << endl;
    for (unsigned int i = 0; i <= root->used; i++) {
      inode = root->head[i].iPtr;
cout << "root->childCount["<<i<<"]: " << root->childCount[i] << endl;
    if (root->leaf) {
cout << "root contains leaf node" << endl;
      for (unsigned int j = 0; j < root->childCount[i]; j++) {
        assert(inode);
        cout << "[" << i << "].[" << j << "]->" << inode->name << endl;
        inode = inode->next.iPtr;
      } // for j
    } // if root->leaf
  } // for i
} // bTree::Info

void
bTree::Print(void) {
  ubixfsInode * node = GetFirstNode();
  while (node != NULL) {
    cout << node->name << endl;
    node = node->next.iPtr;
  }
} // bTree::Print

ubixfsInode *
bTree::Find(const char * key) {

*//*
  ubixfsInode * tmp = GetFirstNode();
  while (tmp!=NULL) {
    if (strcmp(tmp->name, key) == 0) return tmp;
    tmp = tmp->next.iPtr;
  }
  return NULL;
*//*
  return treeSearch(root, key);
} // bTree::Find

ubixfsInode *
bTree::inodeSearch(ubixfsInode * inode, const char * key) {
  if (inode == NULL || key == NULL) return NULL;
  int result = strcmp(inode->name, key);
  if (result == 0) return inode;

  if (result < 0) {
    inode = inode->next.iPtr;
    while (inode != NULL && ((result = strcmp(inode->name, key)) < 0)) {
      inode = inode->next.iPtr;
    } // while
  } else {
    inode = inode->prev.iPtr;
    while (inode != NULL && ((result = strcmp(inode->name, key)) > 0)) {
      inode = inode->prev.iPtr;
    } // while
  } // else
  return (result == 0 ? inode : NULL);
} // bTree::inodeSearch

ubixfsInode *
bTree::treeSearch(bNode * bnode, const char * key) {

  if (bnode == NULL || key == NULL || bnode->used == 0) return NULL;
 
  if (bnode->leaf) 
    return inodeSearch(GetFirstNode(bnode), key);

  if (strcmp(key, bnode->keys[0]) < 0) {
    return treeSearch(bnode->head[0].bPtr, key);
  } // if

  if (strcmp(key, bnode->keys[bnode->used-1]) >= 0) {
    return treeSearch(bnode->head[bnode->used].bPtr, key);
  } // if

  for (unsigned int i = 1; i < bnode->used; i++) {
    if (strcmp(key, bnode->keys[i]) < 0) {
      return treeSearch(bnode->head[i].bPtr, key);
    } // if
  } // for i

  return NULL;
} // bTree::treeSearch

ubixfsInode * 
bTree::GetFirstNode(void) {
  return GetFirstNode(root);
} // bTree::GetFirstNode

ubixfsInode *
bTree::GetFirstNode(bNode * node) {
  bNode * tmpNode = node;

  if (tmpNode == NULL) return NULL;

  while (!tmpNode->leaf) {
    for (unsigned int i = 0; i < tmpNode->used; i++) {
      if (tmpNode->head[i].bPtr != NULL) {
        tmpNode = tmpNode->head[i].bPtr;
        break;
      }  // if
    } // for i
  } // while

  for (unsigned int i = 0; i < tmpNode->used; i++) {
    if (tmpNode->head[i].iPtr != NULL) return tmpNode->head[i].iPtr;
  } // for i
  return NULL;
} // bTree::GetFirstNode

bNode *
bTree::findLeafNode(bNode * node, const char * key) {
  assert(node);
  assert(key);
  if (node == NULL || key == NULL) return NULL;
  assert(node->used);
  if (node->leaf) return node;

  if (strcmp(key, node->keys[0]) < 0) 
    return findLeafNode(node->head[0].bPtr, key);

  if (strcmp(key, node->keys[node->used-1]) >= 0) 
    return findLeafNode(node->head[node->used].bPtr, key);
  
  for (unsigned int i = 1; i < node->used; i++) {
    if (strcmp(key, node->keys[i]) < 0) 
      return findLeafNode(node->head[i].bPtr, key);
  } // for i
  
  return NULL;
} // bTree::findLeafNode

void
bTree::saveNode(FILE * fd, bNode * node, void * tmpPtr) {
 
  bNode * ptr = (bNode *)tmpPtr;
  assert(tmpPtr);
  assert(fd);
  assert(node);
  cout << "writing tag: " << node->tag << endl;

  memcpy(tmpPtr, node, sizeof(bNode));  

  if (node->parent.bPtr != NULL)
    ptr->parent.offset = node->parent.bPtr->tag * sizeof(bNode);
  else
    ptr->parent.offset = 0;

  for (unsigned int i = 0; i <= node->used; i++) {
    bNode * bPtr = node->head[i].bPtr;

    if (bPtr != NULL)
      ptr->head[i].offset = bPtr->tag * sizeof(bNode);
    else
      ptr->head[i].offset = ~0;
    ptr->present[i] = false;
  } // for i
  
  if (node->leaf) {

    for (unsigned int i = 0; i <= node->used; i++) {
  //    ubixfsInode * inode = node->head[i].iPtr;
  // mji    if (inode != NULL) tmp->head[i] = inode->
    } // for i
  } else {

    for (unsigned int i = 0; i <= node->used; i++) {

      if (node->head[i].bPtr != NULL) saveNode(fd, node->head[i].bPtr, tmpPtr);

    } // for i

  } // else

  return;
} // bTree::saveNode 

bool 
bTree::Save(const char * filename) {
  ubixfsInode * uPtr = NULL;
  if (filename == NULL) return false;
  FILE * fd = NULL;
  if ((fd = fopen(filename, "wb+")) == NULL) return false;

cout << "tags: " << tag << endl;
  lseek(fileno(fd), tag * sizeof(bNode), SEEK_END);

  header->firstNodeOffset = sizeof(bNode);
  header->firstDeleted = -1;
  void * tmpPtr = malloc(sizeof(bNode));
  assert(tmpPtr);
  uPtr = (ubixfsInode *)tmpPtr;
  memset(tmpPtr, 0, sizeof(bNode));
  fwrite(header, sizeof(bTreeHeader), 1, fd); 
  saveNode(fd, root, tmpPtr);
  
  fclose(fd);
  free(tmpPtr);
  return true;
} // bTree::Save

bool
bTree::Load(const char * filename) {
  if (filename == NULL) return false;
  return true;
} // bTree::Load

bool
bTree::Delete(const char * key) {

  if (key == NULL) return false;
  return true;
} // bTree::Delete

bool
bTree::Verify(void) {
  ubixfsInode * node = GetFirstNode();
  if (node == NULL) return true;
 
  while (node != NULL) {
    ubixfsInode * next = node->next.iPtr;
    if (next != NULL) {
  //  cout << node->name << "::" << node->next->name << ":::" << strcmp(node->name, node->next->name) << endl;
      if (strcmp(node->name, next->name) > 0) return false;
    }
    node = next;
  } // while
  return true;
} // bTree::Verify

void 
bTree::Print(bNode * node) {
  if (node == NULL) return;
  Info(node);
  if (!node->leaf)
    for (unsigned int i = 0; i <= node->used; i++) {
      Print(node->head[i].bPtr);
    } // for i
} // bTree::Print

void
bTree::PrintWholeTree(void) {
  Print(root);
} // bTree::PrintWholeTree;

bTree::~bTree(void) {
  cout << "tree depth: " << header->treeDepth << endl;
  cout << "tree width: " << header->treeWidth << endl;
  cout << "tree leaf count: " << header->treeLeafCount << endl;
} // bTree::~bTree

*/
