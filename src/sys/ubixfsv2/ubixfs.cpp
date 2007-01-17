#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <iostream>

#include "ubixfs.h"
#include "btree.h"

using namespace std;

UbixFS::UbixFS(void) { 
  device = NULL;
  freeBlockList = NULL;
  superBlock = NULL;
  root = NULL;
} // UbixFS::UbixFS


UbixFS::UbixFS(device_t * dev) {
  device = dev;
  freeBlockList = NULL;
  superBlock = NULL;
  root = NULL;
} // UbixFS::UbixFS

void
UbixFS::printSuperBlock(void) {
  printf("superBlock->name........... %s\n", superBlock->name);
  printf("superBlock->magic1......... %X\n", superBlock->magic1);
  printf("superBlock->fsByteOrder.... %d\n", superBlock->fsByteOrder);
  printf("superBlock->blockSize...... %d\n", superBlock->blockSize);
  printf("superBlock->blockShift..... %d\n", superBlock->blockShift);
  printf("superBlock->numBlocks...... %lld\n", superBlock->numBlocks);
  printf("superBlock->usedBlocks..... %lld\n", superBlock->usedBlocks);
  printf("superBlock->batSectors..... %d\n", superBlock->batSectors);
  printf("superBlock->inodeCount..... %d\n", superBlock->inodeCount);
  printf("superBlock->magic2......... %X\n", superBlock->magic2);
  printf("superBlock->blocksPerAG.... %d\n", superBlock->blocksPerAG);
  printf("superBlock->AGShift........ %d\n", superBlock->AGShift);
  printf("superBlock->numAGs......... %d\n", superBlock->numAGs);
  printf("superBlock->lastUsedAG..... %d\n", superBlock->lastUsedAG);
  printf("superBlock->flags.......... %X\n", superBlock->flags);
  printf("superBlock->magic3......... %X\n", superBlock->magic3);
  return;
} // UbixFS::printSuperBlock

int 
UbixFS::vfs_init(void) {
assert(device);
  size_t result;
  cout << "vfs_init()" << endl;
  assert(device);

  if (device == NULL) return -1;
  if (superBlock != NULL) delete superBlock;
  superBlock = new diskSuperBlock;
assert(superBlock);
  if (superBlock == NULL) return -1;

  // read in the superBlock. It's always the last block on the partition 
cout << "reading in superBlock" << endl;
  device->read(device, superBlock, device->sectors-1, 1);
cout << "done" << endl;

  assert(superBlock->magic1 == UBIXFS_MAGIC1);
  assert(superBlock->magic2 == UBIXFS_MAGIC2);
  assert(superBlock->magic3 == UBIXFS_MAGIC3);
  assert(strcmp(superBlock->name, "UbixFS") == 0);
  assert((1 << superBlock->blockShift) == superBlock->blockSize);
  assert((unsigned)(1 << superBlock->AGShift) == superBlock->blocksPerAG);
  assert(superBlock->flags == UBIXFS_CLEAN);

  if (freeBlockList != NULL) delete [] freeBlockList;
  freeBlockList = new signed char[superBlock->batSectors*512];
assert(freeBlockList);
  memset(freeBlockList, 0, superBlock->batSectors*512);

  device->read(device, 
               freeBlockList, 
               device->sectors - superBlock->batSectors-1,
               superBlock->batSectors
              ); // device->read()

  root = new fileDescriptor;
  assert(root);
  memset(root, 0, sizeof(fileDescriptor));
cout << "allocating root dir inode" << endl;
  root->inode = new ubixfsInode;
  memset(root->inode, 0, sizeof(ubixfsInode));
cout << "root dir inode starting sector: " << 
               ((superBlock->rootDir.AG << superBlock->AGShift) 
               + superBlock->rootDir.start) * (superBlock->blockSize / 512)
     << endl;

cout << "reading in root dir inode" << endl;
  
  device->read(device,
               root->inode,
               ((superBlock->rootDir.AG << superBlock->AGShift) 
                 + superBlock->rootDir.start) * (superBlock->blockSize / 512),
               sizeof(ubixfsInode) / 512
              );
cout << "done" << endl;
  ubixfsInode * rootInode = static_cast<ubixfsInode *>(root->inode);
  assert(rootInode);

  /* the bTree constructor now loads in the header */

  rootInode->data.btPtr = new bTree(this, root);
  rootInode->data.btPtr->Info();
  printSuperBlock();
  return 0;
} // UbixFS::init

int
UbixFS::vfs_format(device_t * dev) {
  cout << "vfs_format()" << endl;
  char sector[512];
  uInt32 blocks, batSect, batSize;
  if (dev == NULL) return -1; 

  // zero out the sector
  memset(&sector, 0x0, sizeof(sector));
 
  // fill the drive in with zeroed out sectors
  cout << "dev->sectors: " << dev->sectors << endl;
  cout << "clearing device...";

  for (unsigned int i = 0; i < dev->sectors; i++) {
    dev->write(dev, &sector, i, 1);
  } // for i

  cout << "done" << endl;

  // allocate a new superBlock and clear it

  diskSuperBlock *sb = new diskSuperBlock;
  if (sb == NULL) return -1;
  memset(sb, 0, sizeof(diskSuperBlock));

  // dev->sectors is the number of 512 byte sectors

  blocks = (dev->sectors-1) / 8;     // 4k blocks
  batSize = (dev->sectors-1) % 8;    // remainder
  
  // compute the BAT size

  while ((batSize * 4096) < blocks) {
    batSize += 8;
    --blocks;
  } // while
 
  // batSize is in sectors
  batSect = blocks * 8;

  strcpy(sb->name, "UbixFS");
  sb->magic1 = UBIXFS_MAGIC1;
  sb->fsByteOrder = 0;
  sb->blockSize = 4096;
  sb->blockShift = 12;
  sb->numBlocks = blocks;
  sb->usedBlocks = 2;  // root dir takes two blocks (inode + bTree header)
  sb->inodeCount = 1;
  sb->inodeSize = 4096;
  sb->magic2 = UBIXFS_MAGIC2;
  sb->blocksPerAG = 2048;
  sb->AGShift = 11;
  sb->numAGs = (sb->numBlocks+2047) / 2048;
  sb->lastUsedAG = 0;

  // the BAT exists outside our official block count, so no 
  // entries in the BAT need to be set for it
  sb->batSectors = batSize;

  sb->flags = 0x434C454E; // CLEN
  sb->logBlocks.AG = 0;
  sb->logBlocks.start = 0;
  sb->logBlocks.len = 0;
  sb->logStart = 0;
  sb->logEnd = 0;
  sb->magic3 = UBIXFS_MAGIC3;
  sb->indicies.AG = 0;
  sb->indicies.start = 0;
  sb->indicies.len = 0;

  sb->rootDir.AG = 0;
  sb->rootDir.start = 0;
  sb->rootDir.len = 1;

  // write out the superBlock

  dev->write(dev, sb, dev->sectors-1, 1);

  // mark the first two 4k blocks used
  memset(&sector, 0, sizeof(sector));
  sector[0] = (1 << 7) | (1 << 6);

  // write out the first sector of the BAT 

  dev->write(dev, &sector, batSect, 1);
  // clear the rest
  sector[0] = 0;

  // write out the rest of the BAT

  for (unsigned int i = 1; i < batSize; i++) {
    dev->write(dev, &sector, (batSect)+i, 1);
  } // for i

  /* allocate part of the root dir */

  // sanity checks
  assert(sb->blockSize);
  assert((unsigned)sb->blockSize >= sizeof(bTreeHeader));

  bTreeHeader * bth = new bTreeHeader;
  assert(bth);
  memset(bth, 0, sizeof(bTreeHeader));

  bth->firstDeleted = -1;
  bth->firstNodeOffset = -1;
  bth->treeDepth = 1;
  bth->treeWidth = 0;
  bth->treeLeafCount = 0;

  /* create the root dir inode here */

  ubixfsInode * inode = new ubixfsInode;
  assert(inode);
  if (inode == NULL) return -1;
  memset(inode, 0, sizeof(ubixfsInode));

  inode->magic1 = UBIXFS_INODE_MAGIC;

  // inodes point to themselves
  inode->inodeNum.AG = 0;
  inode->inodeNum.start = 0;
  inode->inodeNum.len = 1;

  // root dir has no parent directory
  inode->parent.iAddr.AG = 0;
  inode->parent.iAddr.start = 0;
  inode->parent.iAddr.len = 0;

  /* this is part of the root dir structure (the bTreeHeader) */
  inode->blocks.direct[0].AG = 0;
  inode->blocks.direct[0].start = 1;
  inode->blocks.direct[0].len = 1;
   
  inode->blocks.maxDirectRange = sizeof(bTreeHeader);
  inode->blocks.size = sizeof(bTreeHeader);

  strcpy(inode->name, "/");
  inode->uid = getuid();
  inode->gid = getgid();
  // inode->mode
  inode->flags = INODE_DIRECTORY;
  // inode->createTime
  // inode->lastModifiedTime
  // inode->type

  inode->attributes.AG = 0;
  inode->attributes.start = 0;
  inode->attributes.len = 0;

  inode->inodeSize = sb->inodeSize;

  /*
   * next and prev are used in memory to hold pointers to the next/prev
   * inodes in this dir.  On disk they may have another value, but for
   * now they should be set to null.
   */

  inode->next.offset = 0;
  inode->prev.offset = 0;

  // write out the "root" dir inode

  dev->write(dev, 
             inode, 
             ((inode->inodeNum.AG << sb->AGShift) +
              inode->inodeNum.start) * (sb->blockSize / 512),
             sb->inodeSize / 512
            ); // dev->write

  // write out the "root" dir

  dev->write(dev, 
             bth, 
             ((inode->blocks.direct[0].AG << sb->AGShift) + 
              inode->blocks.direct[0].start) * (sb->blockSize / 512),
             sb->blockSize / 512
            ); // dev->write

  delete inode;
  delete bth;
  delete sb;
  cout << "format complete" << endl;
  return 0;
} // UbixFS::vfs_format

void *
UbixFS::vfs_mknod(const char *path, mode_t mode) {
  return mknod(path, 0, mode);  // <- this probably isn't correct
} // UbixFS::vfs_mknod

int
UbixFS::vfs_open(const char * filename, fileDescriptor * fd, int flags, ...) {
  if (filename == NULL || fd == NULL) return -1;
  flags = flags;
  fd->inode = NULL;
  fd->offset = 0;
  fd->size = 0;
  // look up the file here
  return 0;
} // UbixFS::vfs_open

size_t
UbixFS::vfs_read(fileDescriptor * fd, void * data, off_t offset, size_t size) {
  
  unsigned int i;
  off_t sum, startingBlock;
  size_t runSize, sectorCount, totalSize, bSize; // blockSize
  ubixfsInode * inode = NULL;

  if (fd == NULL || data == NULL) return ~0;

  if (size == 0) return 0; // don't fail if size is 0?
   
  assert(device);
  assert(superBlock);

  inode = static_cast<ubixfsInode *>(fd->inode);

  assert(inode);

  bSize = superBlock->blockSize;

  totalSize = sum = i = 0;
  
  while (size > 0) {

    /*
     * place check here to see which set of blocks we're looking through
     */

    // scan through direct blocks
    do {
      if (offset >= sum && offset < sum + inode->blocks.direct[i].len * bSize)          break;

      sum += inode->blocks.direct[i++].len * bSize;
     
    } while (i < NUM_DIRECT_BLOCKS);

    startingBlock = (inode->blocks.direct[i].AG << superBlock->AGShift) + 
                     inode->blocks.direct[i].start + ((offset - sum) / bSize);

    runSize = inode->blocks.direct[i].len * bSize;

    // startingBlock is in 4k blocks
    startingBlock *= (bSize / 512);
    // startingBlock is now in sectors

    if (runSize >= size) {
      runSize = size;
      size = 0;
    } else { 
      size -= runSize;
    } // else

    sectorCount = runSize / 512;
  
    cout << "device->read(device, data, " << startingBlock << ", ";
    cout << sectorCount << ");" << endl; 

    device->read(device, data, startingBlock, sectorCount);

    (uInt8 *)data += runSize;
    totalSize += runSize;
  } // while  
  return totalSize;
} // UbixFS::vfs_read

size_t
UbixFS::vfs_write(fileDescriptor * fd, void * data, off_t offset, size_t size) {
  // char * sector[512];  // used to pad
  unsigned int i, whichBlocks;
  off_t sum, startingBlock, EORPos, maxRange;
  size_t runSize, runRemainder, sectorCount, totalSize, bSize; // blockSize
  ubixfsInode * inode = NULL;
  blockRun br;

  if (fd == NULL || data == NULL) return ~0;

  if (size == 0) return 0; // don't fail if size is 0?

  assert(device);
  assert(superBlock);

  inode = static_cast<ubixfsInode *>(fd->inode);

  assert(inode);

  bSize = superBlock->blockSize;
  assert(bSize != 0);

  totalSize = sum = i = whichBlocks = 0;

  EORPos = offset + size;  // compute End Of Run Position
  maxRange = inode->blocks.maxDirectRange;

  if (inode->blocks.maxIndirectRange > maxRange) {
    maxRange = inode->blocks.maxIndirectRange;
    whichBlocks = 1;
  }

  if (inode->blocks.maxDoubleIndirectRange > maxRange) {
    maxRange = inode->blocks.maxDoubleIndirectRange;
    whichBlocks = 2;
  } 
  
  if (EORPos > maxRange) {
    /* 
     * The offset+size is greater than the size of the file, so we need to
     * extend out the file. Scan through the direct blocks (FIX LATER)
     * to find out where we need to extend
     */
    switch (whichBlocks) {
      case 0:
        while (i < NUM_DIRECT_BLOCKS && inode->blocks.direct[i].len != 0) ++i;
        // i holds which direct block we're going to add to
        break;
      case 1:
      case 2:
        assert(false);  // UNFINISHED
        break;
      default:
        assert(false);  // sanity check
    } // switch

    /*
     * NOTE: it's possible that if we scan through to find where the
     * run goes, we might be able to extend the previous block extent.
     * This will require that we set up br.start to be where we'd like to
     * start looking through the free block list, and then modifying
     * getFreeBlock() to honour that.
     */

    br.AG = inode->inodeNum.AG;        // request a sane allocation group
    br.start = 0;                      // getFreeBlock() will ignore this

    /*
     * The length that we need is determined by how much extra slack we 
     * already have in the pre-allocated blocks.
     * e.g. (assumes 4k blocks)
     * bSize = 4096
     * maxRange = 4096
     * size = 3000
     * offset = 3000
     * size = 4000
     * [--- data block ---][--- data block ---]  <---- blocks on disk
     * <-file data->                             <---- actual file size
     *              <----->                      <---- slack
     * [  data block size ]                      <---- maxRange
     *              |                            <---- offset
     *              *****************            <---- new data
     *
     * In the above example, you'd need at least one more block to write
     * out the data.  
     * ((offset + size) - maxRange + (bSize-1)) / bSize
     * ((3000 + 4000) - 4096 + (4095)) / 4096 == 1 (rounded down)
     * And then we expand it by a little extra so we don't have to keep 
     * looking for more blocks. Currently we use 32k of slack (or 8 blocks)
     */

    br.len = ((EORPos - maxRange + (bSize-1)) / bSize);

    if (br.len < 8) br.len = 8;  // we allocate 32k if the file needs to grow

    br = getFreeBlock(br);
    assert(br.len > 0);
    switch (whichBlocks) {
      case 0:
        inode->blocks.direct[i] = br;
        inode->blocks.maxDirectRange += br.len * bSize;
        break;
      case 1:
        assert(false); // UNFINISHED
        inode->blocks.maxIndirectRange += br.len * bSize;
        break;
      case 2:
        assert(false); // UNFINISHED
        inode->blocks.maxDoubleIndirectRange += br.len * bSize;
        break;
      default:   
        assert(false); // sanity check
    } // switch

    inode->blocks.size = EORPos;
  } // if


  runRemainder = size % 512;
  size -= runRemainder;

  totalSize = sum = i = 0;

  while (size > 0) {

    /*
     * place check here to see which set of blocks we're looking through
     */

    // scan through direct blocks
    do {
      if (offset >= sum && offset < sum + inode->blocks.direct[i].len * bSize)
        break;

      sum += inode->blocks.direct[i++].len * bSize;

    } while (i < NUM_DIRECT_BLOCKS);

    startingBlock = (inode->blocks.direct[i].AG << superBlock->AGShift) +
                     inode->blocks.direct[i].start + ((offset - sum) / bSize);

    runSize = inode->blocks.direct[i].len * bSize;

    // startingBlock is in 4k blocks
    startingBlock *= (bSize / 512);
    // startingBlock is now in sectors

    if (runSize >= size) {
      runSize = size;
      size = 0;
    } else {
      size -= runSize;
    } // else

    sectorCount = runSize / 512;

    cout << "device->write(device, data, " << startingBlock << ", ";
    cout << sectorCount << ");" << endl;

    device->write(device, data, startingBlock, sectorCount);

    (uInt8 *)data += runSize;
    totalSize += runSize;
  } // while

  assert(runRemainder != 0);  // UNFINISHED
  return totalSize;
} // UbixFS::vfs_write

int
UbixFS::vfs_stop(void) {
  if (vfs_sync() != 0) return -1;

  // you must delete the root dir first, in case it needs to
  // still write anything out

  if (root != NULL) {
    ubixfsInode * rootInode = static_cast<ubixfsInode *>(root->inode);
    delete rootInode->data.btPtr; 
    delete rootInode;
    root->inode = NULL;
    
  } // if

  delete root;
  delete [] freeBlockList;
  delete superBlock;

  freeBlockList = NULL;
  superBlock = NULL; 
  root = NULL;

  /* 
   * The device isn't null at this point, allowing for people to restart
   * the mount point. Or, alternatively, to blow things up.
   */
  
  return 0;
} // UbixFS::vfs_stop

int
UbixFS::vfs_sync(void) {
  if (device == NULL || superBlock == NULL || freeBlockList == NULL) return -1;
  device->write(device, 
                freeBlockList, 
                device->sectors - superBlock->batSectors - 1, 
                superBlock->batSectors
               );
  device->write(device, superBlock, device->sectors-1, 1);
  return 0;
} // UbixFS::vfs_sync

void
UbixFS::setFreeBlock(blockRun ibr) {
  signed char * ptr;
  
  if (superBlock == NULL || freeBlockList == NULL) return;
  if (ibr.len == 0) return;
  ptr = freeBlockList + ((ibr.AG << superBlock->AGShift) >> 3);
  ptr += ibr.start >> 3;

  if (ibr.start % 8 != 0) {
    
    ibr.len -= ibr.start % 8;
  } // if

} // UbixFS::setFreeBlock

blockRun
UbixFS::getFreeBlock(blockRun ibr) {
  signed char * ptr;
  signed char * holdPtr;
  int32 count, holdCount;

  blockRun obr = {0, 0, 0};  // output block run

  // Check to make sure none of these are null
  if (device == NULL || freeBlockList == NULL || superBlock == NULL) return obr;

  if (ibr.len == 0) return obr;

  if (ibr.len > superBlock->numBlocks) return obr;

  if (ibr.len == 1) return getFreeBlock(ibr.AG);
  /*
   * count is the block from the base of the list.
   * Since we're given a specific AG to look through, we start the count at
   * AG << AGShift, where AGShift is the shift value of the number of blocks
   * in an AG
   */

  count = (ibr.AG << superBlock->AGShift);

  /*
   * The freeBlockList is a bit map of the free/used blocks.
   * Used = on bit
   * Unused = off bit
   * There are 8 bits per byte (hopefully) and so we have to divide the count
   * by 8 to get our starting byte offset to look from
   */

  ptr = freeBlockList + (count >> 3);

rescan:
  // look for the first free 8 blocks (this may create holes)
  while (*ptr != 0) {
    ++ptr;
    count += 8;
    if (count+8 > superBlock->numBlocks) {
      ptr = freeBlockList;
      count = 0;
    } // if 
  } // while *ptr != 0

  holdPtr = ptr;
  holdCount = count;

  for (unsigned short i = 0; i < ((ibr.len+7) / 8); i++) {
    ++ptr;
    count += 8;
    if (count+8 > superBlock->numBlocks) {
      ptr = freeBlockList;
      count = 0;
      goto rescan;
    } // if
    if (*ptr != 0) goto rescan;
  } // for i

  // we have found a range of blocks that work for us

  obr.AG = holdCount / superBlock->blocksPerAG;
  obr.start = holdCount % superBlock->blocksPerAG;
  obr.len = ibr.len;

  for (unsigned short i = 0; i < (ibr.len / 8); i++) {
    *holdPtr = -1;
    ++holdPtr;
  } // for

  if (ibr.len % 8 != 0) *holdPtr = (-1 << (8-(ibr.len % 8)));

  superBlock->usedBlocks += ibr.len;   // increment the number of used blocks
  return obr;
} // UbixFS::getFreeBlock

blockRun
UbixFS::getFreeBlock(uInt32 AG) {
  // AG == AllocationGroup
  blockRun br;
  signed char * ptr;
  int32 count;
  int32 subCount = 128;

  br.AG = 0;
  br.start = 0;
  br.len = 0;
  // Check to make sure neither of these are null
  if (device == NULL || freeBlockList == NULL || superBlock == NULL) return br;

  // Are there any blocks available?
  if (superBlock->numBlocks == superBlock->usedBlocks) return br;

  /* 
   * count is the block from the base of the list.
   * Since we're given a specific AG to look through, we start the count at
   * AG << AGShift, where AGShift is the shift value of the number of blocks
   * in an AG 
   */

  count = (AG << superBlock->AGShift);

  /*
   * The freeBlockList is a bit map of the free/used blocks. 
   * Used = on bit
   * Unused = off bit
   * There are 8 bits per byte (hopefully) and so we have to divide the count
   * by 8 to get our starting byte offset to look from
   */

  ptr = freeBlockList + (count >> 3);

  // Scan through the freeBlockList 

rescan:
  while (*ptr == -1) { 
    ++ptr;
    count += 8;
    if (count+8 > superBlock->numBlocks) break;
  } // while *ptr == -1

  subCount = 128;

  do {
    if ((*ptr & subCount) == 0) break;
    subCount >>= 1;
    ++count;
    if (count == superBlock->numBlocks) {
      count = 0;
      ptr = freeBlockList;
      goto rescan;
    } // if
  } while(subCount > 1);

  *ptr |= subCount;           // mark this block as used
  ++superBlock->usedBlocks;   // increment the number of used blocks

  br.AG = count / superBlock->blocksPerAG; 
  br.start = count % superBlock->blocksPerAG;
  br.len = 1;
  return br;               // return the allocated block number
} // Ubixfs::getFreeBlock

uInt32
UbixFS::getNextAG(void) {

  if (superBlock->lastUsedAG == superBlock->numAGs) 
    superBlock->lastUsedAG = 0;
  else
    superBlock->lastUsedAG++;
  return superBlock->lastUsedAG;

} // UbixFS::getNextAG

/*
 * UbixFS::getFreeBlock(void)
 * upon success returns a free block based on the next AG after the lastUsedAG
 * failure returns -1
 */

blockRun
UbixFS::getFreeBlock(void) {
  return getFreeBlock(getNextAG());
} // UbixFS::getFreeBlock

blockRun
UbixFS::get8FreeBlocks(uInt32 AG) {
  // AG == AllocationGroup
  blockRun br;
  signed char * ptr;
  signed char * endPtr;
  int32 count;

  br.AG = 0;
  br.start = 0;
  br.len = 0;

  if (device == NULL || freeBlockList == NULL || superBlock == NULL) return br;

  // Are there any blocks available?
  if (superBlock->usedBlocks+8 > superBlock->numBlocks) return br;

  /*
   * count is the block from the base of the list.
   * Since we're given a specific AG to look through, we start the count at
   * AG << AGShift, where AGShift is the shift value of the number of blocks
   * in an AG
   */

  count = (AG << superBlock->AGShift);

  ptr = freeBlockList + (count >> 3);
  
  endPtr = freeBlockList + (superBlock->numBlocks >> 3);

  bool secondTime = false;
  while (*ptr != 0) {
    ++ptr;
    count += 8;
    if (ptr == endPtr) {
      if (secondTime) 
        return br; 
      else 
        secondTime = true;

      count = 0;
      ptr = freeBlockList;      
    } // if
  } // while 

  *ptr = -1;   // mark 8 blocks as taken

  br.AG = count / superBlock->blocksPerAG;
  br.start = count % superBlock->blocksPerAG;
  br.len = 8;
  return br;
} // UbixFS::get8FreeBlocks

void *
UbixFS::mknod(const char *filename, ubixfsInode * parent, mode_t mode) {
  ubixfsInode * inode = NULL;

  inode = new ubixfsInode;
  assert(inode);
  if (inode == NULL) return NULL;
  memset(inode, 0, sizeof(ubixfsInode));

  inode->magic1 = UBIXFS_INODE_MAGIC;

  /* 
   * in retrospect.. I'm not sure why parent would be null.. only the
   * root directory would have a null parent, but that's manually allocated
   * in vfs_format()
   */

  if (parent == NULL) {
    inode->inodeNum = getFreeBlock();
    inode->parent.iAddr.AG = 0;
    inode->parent.iAddr.start = 0;
    inode->parent.iAddr.len = 0;
  } else {
    inode->inodeNum = getFreeBlock(parent->inodeNum.AG);
    inode->parent.iAddr = parent->inodeNum;
  } // else
   
  strncpy(inode->name, filename, MAX_FILENAME_LENGTH);

  inode->uid = getuid();
  inode->gid = getgid();
  // inode->mode
  inode->flags = mode;
  // inode->createTime
  // inode->lastModifiedTime
  inode->inodeSize = superBlock->inodeSize;

  inode->attributes.AG = 0;
  inode->attributes.start = 0;
  inode->attributes.len = 0;

  // inode->type 

  /*
   * next and prev are used in memory to hold pointers to the next/prev
   * inodes in this dir.  On disk they may have another value, but for
   * now they should be set to null.
   */

  inode->next.offset = 0;
  inode->prev.offset = 0;
  inode->refCount = 0;
  ++superBlock->inodeCount;
  return inode;
} // UbixFS::mknod

int
UbixFS::vfs_mkdir(const char * path, mode_t mode) {
  char name[MAX_FILENAME_LENGTH];
  unsigned int start, end, len, nameStart;
  ubixfsInode * dir = static_cast<ubixfsInode *>(root->inode);
  ubixfsInode * inode = NULL;

  assert(path);                     // bad input: vfs_mkdir(NULL);
  assert(*path);                    // bad input: vfs_mkdir("");

  memset(&name, 0, sizeof(name));
  // find the dir name
  len = strlen(path);

  assert(path[0] == '/');           // bad input: vfs isn't doing its job
  assert(len > 1);                  // bad input: mkdir /

  // remove trailing "/" if present
  if (path[len-1] == '/') --len;

  assert(len > 1);                  // bad input: mkdir //

  nameStart = len-1;

  assert(path[nameStart] != '/');   // bad input: mkdir /a//

  /*
   * we're guaranteed by the assert() above that there is 
   * at least one "/" before our location. If you remove the assert 
   * you might need to make sure nameStart stays above 0 in the following
   * while
   */

  while (path[nameStart] != '/') --nameStart;
  ++nameStart;
  assert(len - nameStart > 0);

  /* e.g.
   *   v--------------------- start
   *      v------------------ end
   *                  v------ nameStart
   *  /usr/local/data/dirname/  <--- ignores trailing /
   *  <---------23----------> len
   */

  start = end = 1;   // skip leading /
  while (end < nameStart) {
    do { ++end; } while(path[end] != '/');

    assert(end-start+1 < sizeof(name));
    // this is probably wrong:
    strncpy(name, &path[start], end-start+1);
    cout << name << endl;
    dir = dir->data.btPtr->Find(name);
    assert(dir);
    assert(dir->flags & INODE_DIRECTORY == INODE_DIRECTORY);
    start = ++end;
  }

  strncpy(name, &path[nameStart], len - nameStart);
  inode = (ubixfsInode *)mknod(name, dir, mode | INODE_DIRECTORY);

  /* 
   * keep in mind that the reason for passing in the name is because
   * we thought about allowing key names to be different from inode 
   * names. In retrospect, I don't think that's a good idea since a dir
   * listing will print the actual dir name instead of . and ..
   * Thus: the first parameter of btPtr->Insert() may go away.
   */

  assert(dir->data.btPtr->Insert(inode->name, inode));

  return 0;
} // UbixFS::vfs_mkdir

void 
UbixFS::printFreeBlockList(uInt32 AG) {
  unsigned int j;
  if (superBlock == NULL || freeBlockList == NULL) return;
  printf("AG = %d\n", AG);
  for (unsigned int i = 0; i < superBlock->blocksPerAG / 8; i++) {
    j = 128;
    signed char foo = freeBlockList[(AG << superBlock->AGShift)+i];
    while (j > 0) {
      if ((foo & j) == j) 
        printf("1");
      else
        printf("0");    
      j >>= 1;
 
    }
  } // for i
  printf("\n");
  return;
} // UbixFS::printFreeBlockList

UbixFS::~UbixFS(void) {
  delete [] freeBlockList;
  return;
}
