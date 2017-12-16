/*****************************************************************************************
 Copyright (c) 2002, 2016 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: createvirtualspace.c 232 2016-01-24 01:47:12Z reddawg $

 *****************************************************************************************/

#include <vmm/vmm.h>

/************************************************************************

 Function: void *vmmCreateVirtualSpace(pid_t);
 Description: Creates A Virtual Space For A New Task
 Notes:

 07/30/02 - This Is Going To Create A New VM Space However Its Going To
 Share The Same Top 1GB Space With The Kernels VM And Lower
 1MB Of VM Space With The Kernel

 07/30/02 - Note This Is Going To Get The Top 1Gig And Lower 1MB Region
 From The Currently Loaded Page Directory This Is Safe Because
 All VM Spaces Will Share These Regions

 07/30/02 - Note I Realized A Mistake The First Page Table Will Need To Be
 A Copy But The Page Tables For The Top 1GB Will Not Reason For
 This Is That We Just Share The First 1MB In The First Page Table
 So We Will Just Share Physical Pages.

 08/02/02 - Added Passing Of pid_t pid For Better Tracking Of Who Has Which
 Set Of Pages

 ************************************************************************/
void *vmmCreateVirtualSpace( pid_t pid ) {
  void *newPageDirectoryAddress = 0x0;
  uInt32 *parentPageDirectory = 0x0, *newPageDirectory = 0x0;
  uInt32 *parentPageTable = 0x0, *newPageTable = 0x0;
  int x = 0;

  /* Set Address Of Parent Page Directory */
  parentPageDirectory = (uInt32 *) PD_BASE_ADDR;

  /* Allocate A New Page For The New Page Directory */

  newPageDirectory = (uInt32 *) vmmGetFreePage( pid );

  /* Set newPageDirectoryAddress To The Newly Created Page Directories Page */

  newPageDirectoryAddress = (void *) vmm_getPhysicalAddr( (uInt32) newPageDirectory );

  /* First Set Up A Flushed Page Directory */
  for ( x = 0; x < PD_ENTRIES; x++ ) {
    newPageDirectory[x] = (uInt32) 0x0;
  }

  /* Map The Top Kernel (APPROX 1GB) Region Of The VM Space */
  for ( x = PD_INDEX( VMM_KERN_START ); x < PD_ENTRIES; x++ ) {
    newPageDirectory[x] = parentPageDirectory[x];
  }

  /*
   * Allocate A New Page For The The First Page Table Where We Will Map The
   * Lower Region
   */

  newPageTable = (uInt32 *) vmmGetFreePage( pid );

  /* Flush The Page From Garbage In Memory */
  for ( x = 0; x < PD_ENTRIES; x++ ) {
    newPageTable[x] = (uInt32) 0x0;
  }

  /* Map This Into The Page Directory */
  newPageDirectory[0] = (vmm_getPhysicalAddr( (uInt32) newPageTable ) | PAGE_DEFAULT);

  /* Set Address Of Parents Page Table */
  parentPageTable = (uInt32 *) PT_BASE_ADDR;

  /* Map The First 1MB Worth Of Pages */
  for ( x = 0; x < (PD_ENTRIES / 4); x++ ) {
    newPageTable[x] = parentPageTable[x];
  }

  /* Unmap Page From Virtual Space */
  vmmUnmapPage( (uInt32) newPageTable, 1 );

  /*
   *
   * Map Page Directory Into VM Space
   * First Page After Page Tables
   * This must be mapped into the page directory before we map all 1024 page directories into the memory space
   */
  newPageTable = (uInt32 *) vmmGetFreePage( pid );

  newPageDirectory[PD_INDEX( PD_BASE_ADDR )] = (u_int32_t)( vmm_getPhysicalAddr( (uInt32) newPageTable ) | PAGE_DEFAULT );

  newPageTable[0] = (u_int32_t)( (u_int32_t)( newPageDirectoryAddress ) | PAGE_DEFAULT );
  //MrOlsen 2017-12-15 kprintf( "PD3: %i - 0x%X - 0x%X\n", PD_INDEX( PD_BASE_ADDR ), newPageDirectoryAddress, newPageTable[0] );

  vmmUnmapPage( (uInt32) newPageTable, 1 );

  /*
   *
   * Map Page Tables Into VM Space
   * The First Page Table (4MB) Maps To All Page Directories
   *
   */

  newPageTable = (uInt32 *) vmmGetFreePage( pid );

  newPageDirectory[PD_INDEX( PT_BASE_ADDR )] = (u_int32_t)( vmm_getPhysicalAddr( (uInt32) newPageTable ) | PAGE_DEFAULT );

  /* Flush The Page From Garbage In Memory */
  for ( x = 0; x < PD_ENTRIES; x++ )
    newPageTable[x] = (uInt32) 0x0;

  for ( x = 0; x < PD_ENTRIES; x++ )
    newPageTable[x] = newPageDirectory[x];

  /* Unmap Page From Virtual Space */
  vmmUnmapPage( (uInt32) newPageTable, 1 );

  /* Now We Are Done With The Page Directory So Lets Unmap That Too */

  vmmUnmapPage( (uInt32) newPageDirectory, 1 );
  /* Return Physical Address Of Page Directory */
  return (newPageDirectoryAddress);
}

/***
 END
 ***/

