/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
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

 $Id$

*****************************************************************************************/

#include <vmm/vmm.h>
#include <sys/kern_sysctl.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <string.h>

static spinLock_t cvsSpinLock = SPIN_LOCK_INITIALIZER;

/************************************************************************

Function: void *vmmCopyVirtualSpace(pidType pid);

Description: Creates A Copy Of A Virtual Space And Set All NON Kernel
             Space To COW For A Fork This Will Also Alter The Parents
             VM Space To Make That COW As Well

Notes:

08/02/02 - Added Passing Of pidType pid So We Can Better Keep Track Of
           Which Task Has Which Physical Pages

************************************************************************/
void           *
vmmCopyVirtualSpace(pidType pid)
{
  void           *newPageDirectoryAddress = 0x0;
  uInt32         *parentPageDirectory = 0x0, *newPageDirectory = 0x0;
  uInt32         *parentPageTable = 0x0, *newPageTable = 0x0;
  uInt32         *parentStackPage = 0x0, *newStackPage = 0x0;
  uInt16          x = 0, i = 0, s = 0;

  spinLock(&cvsSpinLock);
  
  /* Set Address Of Parent Page Directory */
  parentPageDirectory = (uInt32 *) parentPageDirAddr;
  /* Allocate A New Page For The New Page Directory */
  if ((newPageDirectory = (uInt32 *) vmmGetFreeKernelPage(pid,1)) == 0x0)
    kpanic("Error: newPageDirectory == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);
    
  /* Set newPageDirectoryAddress To The Newly Created Page Directories Page */
  newPageDirectoryAddress = (void *)vmm_getPhysicalAddr((uInt32) newPageDirectory);
  
  /* First Set Up A Flushed Page Directory */
  memset(newPageDirectory,0x0,0x1000);

  /* Map The Top 1GB Region Of The VM Space */
  for (x = 768; x < pageEntries; x++) {
    newPageDirectory[x] = parentPageDirectory[x];
    }
    
  /*
   * Now For The Fun Stuff For Page Tables 1-766 We Must Map These And Set
   * The Permissions On Every Mapped Pages To COW This Will Conserve Memory
   * Because The Two VM Spaces Will Be Sharing Some Pages
   */
  for (x = 0x1; x <= 766; x++) {
    /* If Page Table Exists Map It */
    if (parentPageDirectory[x] != 0) {
      /* Set Parent  To Propper Page Table */
      parentPageTable = (uInt32 *) (tablesBaseAddress + (0x1000 * x));
      /* Allocate A New Page Table */
      if ((newPageTable = (uInt32 *) vmmGetFreeKernelPage(pid,1)) == 0x0)
        kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);

      /* Set Parent And New Pages To COW */
      for (i = 0; i < pageEntries; i++) {
      
	/* If Page Is Mapped */
	if ((parentPageTable[i] & 0xFFFFF000) != 0x0) {
	  /* Check To See If Its A Stack Page */
	  if (((uInt32) parentPageTable[i] & PAGE_STACK) == PAGE_STACK) {
	    /* Alloc A New Page For This Stack Page */
	    if ((newStackPage = (uInt32 *) vmmGetFreeKernelPage(pid,1)) == 0x0)
	      kpanic("Error: newStackPage == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);
	      
	    /* Set Pointer To Parents Stack Page */
	    parentStackPage = (uInt32 *) (((1024 * 4096) * x) + (4096 * i));
	    
	    /* Copy The Tack Byte For Byte (I Should Find A Faster Way) */
	    for (s = 0x0; s < pageEntries; s++) {
	      newStackPage[s] = parentStackPage[s];
	      }
	    /* Insert New Stack Into Page Table */
	    newPageTable[i] = (vmm_getPhysicalAddr((uInt32) newStackPage) | PAGE_DEFAULT | PAGE_STACK);
	    /* Unmap From Kernel Space */
	    vmmUnmapPage((uInt32) newStackPage, 1);
  	    }
          else {
	    /* Set Page To COW In Parent And Child Space */
	    newPageTable[i] = (((uInt32) parentPageTable[i] & 0xFFFFF000) | (PAGE_DEFAULT | PAGE_COW));
	    /* Increment The COW Counter For This Page */
	    if (((uInt32) parentPageTable[i] & PAGE_COW) == PAGE_COW) {
	      adjustCowCounter(((uInt32) parentPageTable[i] & 0xFFFFF000), 1);
	      }
            else {
	      adjustCowCounter(((uInt32) parentPageTable[i] & 0xFFFFF000), 2);
	      parentPageTable[i] = newPageTable[i];
	      }
	    }
	  }
        else {
	  newPageTable[i] = (uInt32) 0x0;
	  }
        }

      /* Put New Page Table Into New Page Directory */
      newPageDirectory[x] = (vmm_getPhysicalAddr((uInt32) newPageTable) | PAGE_DEFAULT);
      /* Unmap Page From Kernel Space But Keep It Marked As Not Avail */
      vmmUnmapPage((uInt32) newPageTable, 1);
    } else {
      newPageDirectory[x] = (uInt32) 0x0;
    }
  }
  /*
   * Allocate A New Page For The The First Page Table Where We Will Map The
   * Lower Region
   */
  if ((newPageTable = (uInt32 *) vmmGetFreeKernelPage(pid,1)) == 0x0)
    kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);
    
  /* Flush The Page From Garbage In Memory */
  memset(newPageTable,0x0,0x1000);

  /* Map This Into The Page Directory */
  newPageDirectory[0] = (vmm_getPhysicalAddr((uInt32) newPageTable) | PAGE_DEFAULT);
  /* Set Address Of Parents Page Table */
  parentPageTable = (uInt32 *) tablesBaseAddress;
  /* Map The First 1MB Worth Of Pages */
  for (x = 0; x < (pageEntries / 4); x++) {
    newPageTable[x] = parentPageTable[x];
  }
  /* Map The Next 3MB Worth Of Pages But Make Them COW */
  for (x = (pageEntries / 4) + 1; x < pageEntries; x++) {
    /* If Page Is Avaiable Map It */
    if ((parentPageTable[x] & 0xFFFFF000) != 0x0) {
      /* Set Pages To COW */
      newPageTable[x] = (((uInt32) parentPageTable[x] & 0xFFFFF000) | (PAGE_DEFAULT | PAGE_COW));
      /* Increment The COW Counter For This Page */
      if (((uInt32) parentPageTable[x] & PAGE_COW) == PAGE_COW) {
	adjustCowCounter(((uInt32) parentPageTable[x] & 0xFFFFF000), 1);
      } else {
	adjustCowCounter(((uInt32) parentPageTable[x] & 0xFFFFF000), 2);
	parentPageTable[x] = newPageTable[x];
      }
    } else {
      newPageTable[x] = (uInt32) 0x0;
    }
  }
  /* Set Virtual Mapping For Page Directory */
  newPageTable[256] = (vmm_getPhysicalAddr((uInt32) newPageDirectory) | PAGE_DEFAULT);

  /*
   * Now The Fun Stuff Build The Initial Virtual Page Space So We Don't Have
   * To Worry About Mapping Them In Later How Ever I'm Concerned This May
   * Become A Security Issue
   */
  /* First Lets Unmap The Previously Allocated Page Table */
  vmmUnmapPage((uInt32) newPageTable, 1);
  /* Allocate A New Page Table */
  if ((newPageTable = (uInt32 *) vmmGetFreeKernelPage(pid,1)) == 0x0)
    kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);
  /* First Set Our Page Directory To Contain This */
  newPageDirectory[767] = vmm_getPhysicalAddr((uInt32) newPageTable) | PAGE_DEFAULT;
  /* Now Lets Build The Page Table */
  for (x = 0; x < pageEntries; x++) {
    newPageTable[x] = newPageDirectory[x];
  }
  /* Now We Are Done So Lets Unmap This Page */
  vmmUnmapPage((uInt32) newPageTable, 1);
  /* Now We Are Done With The Page Directory So Lets Unmap That Too */
  vmmUnmapPage((uInt32) newPageDirectory, 1);
  
  spinUnlock(&cvsSpinLock);
  
  /* Return Physical Address Of Page Directory */
  return (newPageDirectoryAddress);
}

/***
 $Log$
 Revision 1.4  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.3  2006/12/01 05:12:35  reddawg
 We're almost there... :)

 Revision 1.2  2006/11/06 19:10:12  reddawg
 Lots Of Updates... Still having issues with brk();

 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:49  reddawg
 no message

 Revision 1.7  2004/07/28 15:05:43  reddawg
 Major:
   Pages now have strict security enforcement.
   Many null dereferences have been resolved.
   When apps loaded permissions set for pages rw and ro

 Revision 1.6  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.5  2004/07/25 06:04:00  reddawg
 Last of my fixes for the morning

 Revision 1.4  2004/07/20 22:29:55  reddawg
 assert: remade assert

 Revision 1.3  2004/07/19 01:58:12  reddawg
 vmmCopyVirtualSpace: cleaned up one full page memory leak we were still using old sysID over pid

 Revision 1.2  2004/06/15 12:35:05  reddawg
 Cleaned Up

 Revision 1.1.1.1  2004/04/15 12:06:51  reddawg
 UbixOS v1.0

 Revision 1.14  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
