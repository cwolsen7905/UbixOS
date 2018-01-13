/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./ubixfs.h"

#define typeFile      1
#define typeContainer 2
#define typeDirectory 4

//Michelle My Bell

struct blockAllocationTableEntry *BAT = 0x0;

int findFreeBlock(int cBlock, int size) {
  int i = 0x0;
  for (i = 1; i < size; i++) {
    if (BAT[i].attributes == 0x0) {
      if (cBlock != -1) {
        BAT[cBlock].nextBlock = i;
      }
      BAT[i].attributes = 1;
      return (i);
    }
  }
  return (0x0);
}

int main(int argc, char **argv) {
  FILE *driveFd, *fileFd;
  int startSector = 0x0, size = 0x0, structSize = 0x0, i = 0x0, x = 0x4, block = 0x0, batSize = 0x0;
  int blockCount = 0x0, file = 0x0;
  unsigned long counter = 0x0;
  struct directoryEntry *dirEntry = 0x0;
  struct partitionInformation *partInfo = 0x0;
  if (argc <= 4) {
    printf("Error:\nformat start size files....\n");
    exit(1);
  }
  driveFd = fopen(argv[3], "wb");
  startSector = atoi(argv[1]) - 1;
  size = atoi(argv[2]) * 512;
  structSize = sizeof(struct blockAllocationTableEntry);
  structSize *= (size / 4096);
  batSize = (((structSize + 511) / 512) * 512);
  BAT = (struct blockAllocationTableEntry *) malloc(structSize);
  dirEntry = (struct directoryEntry *) malloc(0x4000);
  memset(dirEntry, 0x0, 0x4000);
  partInfo = (struct partitionInformation *) malloc(512);
  partInfo->size = (size / 512);
  partInfo->startSector = (startSector + 1);
  partInfo->blockAllocationTable = (startSector + 1);
  partInfo->rootDirectory = ((startSector + 1) + (batSize / 512));
  partInfo->rootDirectorySize = 0x4000 / 512;
  fseek(driveFd, (startSector * 512), 0);
  fwrite(partInfo, 512, 1, driveFd);
  startSector++;
  dirEntry[0].attributes = typeDirectory;
  dirEntry[0].permissions = 0xEAA;
  dirEntry[0].gid = 0x0;
  dirEntry[0].uid = 0x0;
  dirEntry[0].startCluster = 0x0;
  dirEntry[0].size = 0x4000;
  sprintf(dirEntry[0].fileName, "/");
  dirEntry[1].attributes = typeDirectory;
  dirEntry[1].permissions = 0xEAA;
  dirEntry[1].gid = 0x0;
  dirEntry[1].uid = 0x0;
  dirEntry[1].startCluster = 0x0;
  dirEntry[1].size = 0x4000;
  sprintf(dirEntry[1].fileName, "..");
  BAT[0].nextBlock = 0x1;
  BAT[0].attributes = 1;
  BAT[0].realSector = (batSize / 512);
  BAT[1].nextBlock = 0x2;
  BAT[1].attributes = 1;
  BAT[1].realSector = ((batSize / 512) + 1 * 8);
  BAT[2].nextBlock = 0x3;
  BAT[2].attributes = 1;
  BAT[2].realSector = ((batSize / 512) + 2 * 8);
  BAT[3].nextBlock = -1;
  BAT[3].attributes = 1;
  BAT[3].realSector = ((batSize / 512) + 3 * 8);

  for (i = 4; i < (size / 4096); i++) {
    BAT[i].nextBlock = -1;
    BAT[i].attributes = 0x0;
    BAT[i].realSector = ((batSize / 512) + (i * 8));
    BAT[i].reserved = 0x0;
  }
  file = 0x2;
  while (x < argc) {
    counter = 0x0;
    blockCount = 0x0;
    fileFd = fopen(argv[x], "rb");
    block = findFreeBlock(-1, (size / 4096));
    dirEntry[file].startCluster = block;
    dirEntry[file].attributes = typeFile;
    dirEntry[file].permissions = atoi(argv[x + 1]);
    rewind(driveFd);
    /* fseek(driveFd,((BAT[startSector].realSector * 512) + ((batSize/512)+(BAT[block].realSector*4096))),0); */
    fseek(driveFd, ((startSector + BAT[block].realSector) * 512), 0);
    while (!feof(fileFd)) {
      if (4096 == (counter - (blockCount * 4096))) {
        block = findFreeBlock(block, (size / 4096));
        printf("Block: [%i][%s]\n", block, argv[x]);
        rewind(driveFd);
        /* fseek(driveFd,((startSector * 512) + ((batSize/512)+(BAT[block].realSector*4096))),0); */
        fseek(driveFd, ((startSector + BAT[block].realSector) * 512), 0);
        blockCount++;
      }
      fputc(fgetc(fileFd), driveFd);
      counter++;
    }
    i = 0;
    while ((counter + i) % 512) {
      fputc(0x0, driveFd);
      i++;
    }
    fclose(fileFd);
    dirEntry[file].size = (counter - 1);
    dirEntry[file].uid = 0x0;
    dirEntry[file].gid = 0x0;
    sprintf(dirEntry[file].fileName, "%s", argv[x]);
    x += 2;
    file++;
  }
  dirEntry[1].attributes = typeDirectory;
  dirEntry[1].permissions = 0xEAA;
  dirEntry[1].gid = 0x0;
  dirEntry[1].uid = 0x0;
  dirEntry[1].startCluster = 0x0;
  dirEntry[1].size = 0x4000;
  sprintf(dirEntry[1].fileName, "fakeDir");
  rewind(driveFd);
  fseek(driveFd, (long) (((startSector) * 512) + batSize), 0);
  fwrite(dirEntry, 0x4000, 1, driveFd);
  rewind(driveFd);
  fseek(driveFd, ((startSector) * 512), 0);
  if (fwrite(BAT, batSize, 1, driveFd) >= 1) {
    /*
     *    printf("size         [%i]\n",partInfo->size);
     *    printf("startSector: [%i]\n",partInfo->startSector);
     *    printf("bat:         [%i]\n",partInfo->blockAllocationTable);
     *    printf("rootDir:     [%i]\n",partInfo->rootDirectory);
     *    printf("sizeof:      [%i]\n",sizeof(struct blockAllocationTableEntry));
     *    printf("size:        [%i]\n",(size/4096));
     */
    printf("Formatted!\n");
  }
  else {
    printf("Error Formatting!!\n");
  }
  fclose(driveFd);
  return (0);
}
