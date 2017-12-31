#include <iostream>
#include <vector>
#include <stdlib.h>
#include "vfs.h"
#include "btree.h"
#include "ubixfs.h"
#include "device.h"
#include "ramdrive.h"
using namespace std;

int
main(void) {

  device_t * ramDrive = dev_ramDrive();
  UbixFS * fs = new UbixFS(ramDrive);
  fs->vfs_format(ramDrive);
  fs->vfs_init(); 
  fs->vfs_mkdir("/testdir", 0);
  fs->vfs_stop();
  dev_ramDestroy();

#if 0
  int i = 0;
  ubixfsInode * inode = (ubixfsInode *)malloc(sizeof(ubixfsInode));
  memset(inode, 0, sizeof(ubixfsInode));
  strcpy(inode -> name, "50");
  bTree * tree = new bTree(".", inode);

  for (i = 0; i < 100; i++) {
//  while (tree->Verify()) {
//    if (i%1000 == 0) cout << "-_- i = "<<i<<" -_-" << endl;
    inode = (ubixfsInode *)malloc(sizeof(ubixfsInode));
    if (inode == NULL) break;
    memset(inode, 0, sizeof(ubixfsInode));
    for (int k = 0; k < (random() % 100)+5; k++) {
//    for (int k = 0; k < 100; k++) {
      inode->name[k] = (char)((random() % 26)+'a');
    } // for k
//     tree->Insert(inode);
    if (!tree->Insert(inode->name, inode)) cout << "Insert(" << inode->name << ") failed" << endl;
//    ++i;
  } // for i
//  cout << "i made it to: " << i << endl;

  i = 0;
  ubixfsInode * tmpInode = tmpInode = tree->GetFirstNode();
  if (tmpInode == NULL) cout << "GetFirstNode() returns null" << endl;
  while (tmpInode != NULL) {
    //cout << "node[" << i++ << "]: " << tmpInode->name << endl;
    cout << tmpInode->name << endl;
    tmpInode = tmpInode->next.iPtr;
  } // while


//  tree->Info();
  tree->Save("tree.dat");
  free(inode);
  delete tree;
#endif
  cout << "sizeof(bNode): " << sizeof(struct bNode) << endl;
  cout << "sizeof(ubixfsInode): " << sizeof(struct ubixfsInode) << endl;
  cout << "sizeof(diskSuperBlock): " << sizeof(struct diskSuperBlock) << endl;
  cout << "sizeof(bTreeHeader): " << sizeof(struct bTreeHeader) << endl;
  return 0;
}
