#ifndef UBIXFS_H
#define UBIXFS_H

#include <sys/types.h>
#include <unistd.h>
#include "fsAbstract.h"
#include "types.h"
#include "file.h"

#define INODE_IN_USE      0x00000001
#define INODE_DIRECTORY   0x00000002
#define ATTR_INODE        0x00000004
#define INODE_LOGGED      0x00000008
#define INODE_DELETED     0x00000010
#define PERMANENT_FLAGS   0x0000ffff
#define INODE_NO_CACHE    0x00010000
#define INODE_WAS_WRITTEN 0x00020000
#define NO_TRANSACTION    0x00040000

#define NUM_DIRECT_BLOCKS 64
#define MAX_FILENAME_LENGTH 256

#define UBIXFS_MAGIC1 0xA0A0A0A
#define UBIXFS_MAGIC2 0xB0B0B0B
#define UBIXFS_MAGIC3 0xC0C0C0C
#define UBIXFS_INODE_MAGIC 0x3bbe0ad9

/* befs magic numbers
#define SUPER_BLOCK_MAGIC1 0x42465331 // BFS1
#define SUPER_BLOCK_MAGIC2 0xdd121031
#define SUPER_BLOCK_MAGIC3 0x15b6830e
 */
#define UBIXFS_CLEAN 0x434C454E  // CLEN
#define UBIXFS_DIRTY 0x44495254  // DIRT


typedef struct blockRun {
  int            AG               __attribute__ ((packed));
  unsigned short start            __attribute__ ((packed));
  unsigned short len              __attribute__ ((packed));
} inodeAddr;

struct bNode;
struct ubixfsInode;
class bTree;

typedef union uPtr {
  inodeAddr iAddr;
  bNode * bPtr;
  bTree * btPtr;
  ubixfsInode * iPtr;
  void * vPtr;
  off_t offset;
};

typedef struct diskSuperBlock {
  char      name[32]      __attribute__ ((packed));
  int32     magic1        __attribute__ ((packed));
  int32     fsByteOrder   __attribute__ ((packed));

// blockSize on disk (4096 for UbixFS v2)
  int32     blockSize     __attribute__ ((packed));

// number of bits needed to shift a block number to get a byte address
  uInt32     blockShift    __attribute__ ((packed));

  off_t     numBlocks     __attribute__ ((packed));
  off_t     usedBlocks    __attribute__ ((packed));

// BlockAllocationTable
  uInt32    batSectors    __attribute__ ((packed));

  uInt32    inodeCount    __attribute__ ((packed));
  uInt32    inodeSize     __attribute__ ((packed));
  uInt32    magic2        __attribute__ ((packed));
  uInt32    blocksPerAG   __attribute__ ((packed));
  uInt32    AGShift       __attribute__ ((packed));
  uInt32    numAGs        __attribute__ ((packed));
  uInt32    lastUsedAG    __attribute__ ((packed));
// flags tells whether the FS is clean (0x434C454E) or dirty (0x44495954)
  int32     flags         __attribute__ ((packed));

// journal information
  blockRun  logBlocks     __attribute__ ((packed));
  off_t     logStart      __attribute__ ((packed));
  off_t     logEnd        __attribute__ ((packed));

  int32     magic3        __attribute__ ((packed));

// root dir of the SYS container
  inodeAddr rootDir       __attribute__ ((packed));

// indicies
  inodeAddr indicies      __attribute__ ((packed));

  char      pad[368]      __attribute__ ((packed));

} diskSuperBlock;

typedef struct dataStream {
  struct blockRun direct[NUM_DIRECT_BLOCKS] __attribute__ ((packed));
  off_t           maxDirectRange            __attribute__ ((packed));
  struct blockRun indirect                  __attribute__ ((packed));
  off_t           maxIndirectRange          __attribute__ ((packed));
  struct blockRun double_indirect           __attribute__ ((packed));
  off_t           maxDoubleIndirectRange    __attribute__ ((packed));
  off_t           size                      __attribute__ ((packed));
} dataStream;

typedef struct ubixfsInode {
  int32       magic1                     __attribute__ ((packed));
  inodeAddr   inodeNum                   __attribute__ ((packed));
  char        name[MAX_FILENAME_LENGTH]  __attribute__ ((packed));
  uid_t       uid                        __attribute__ ((packed));
  gid_t       gid                        __attribute__ ((packed));
  int32       mode                       __attribute__ ((packed));
  int32       flags                      __attribute__ ((packed));
 // uInt64      createTime                 __attribute__ ((packed));
 // uInt64      lastModifiedTime           __attribute__ (packed));
  inodeAddr   attributes                 __attribute__ ((packed));
  uInt32      type                       __attribute__ ((packed));
  uInt32      inodeSize                  __attribute__ ((packed));
  uPtr        parent                     __attribute__ ((packed));
  uPtr        next                       __attribute__ ((packed));
  uPtr        prev                       __attribute__ ((packed));
  uPtr        data                       __attribute__ ((packed));
  dataStream  blocks                     __attribute__ ((packed));
  uInt32      refCount                   __attribute__ ((packed));
  char        smallData[3200]            __attribute__ ((packed));
} ubixfsInode;

class UbixFS : public vfs_abstract {
 protected:
  signed char *    freeBlockList;
  diskSuperBlock * superBlock;
  fileDescriptor * root;

  blockRun         getFreeBlock(blockRun);
  blockRun         getFreeBlock(uInt32);
  blockRun         getFreeBlock(void);
  blockRun         get8FreeBlocks(uInt32);
  uInt32           getNextAG(void);
  void *           mknod(const char *, ubixfsInode *, mode_t);
  void             printSuperBlock(void);
  void             printFreeBlockList(uInt32);
  void             setFreeBlock(blockRun);
 public:
                   UbixFS(void);
                   UbixFS(device_t *);
  virtual int      vfs_init(void);
  virtual int      vfs_format(device_t *);
  virtual void *   vfs_mknod(const char *, mode_t);
  virtual int      vfs_mkdir(const char *, mode_t);
  virtual int      vfs_open(const char *, fileDescriptor *, int, ...);
  virtual size_t   vfs_read(fileDescriptor *, void *, off_t, size_t);
  virtual size_t   vfs_write(fileDescriptor *, void *, off_t, size_t);
  virtual int      vfs_sync(void);
  virtual int      vfs_stop(void); 
  virtual         ~UbixFS(void); 
  friend class bTree;
}; // UbixFS

#endif // !UBIXFS_H
