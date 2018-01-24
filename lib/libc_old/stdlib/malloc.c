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

 $Id: malloc.c 122 2016-01-14 03:45:52Z reddawg $

 *****************************************************************************************/

#include <sys/types.h>
#include <sys/sys.h>
#include <stdio.h>

struct memDescriptor {
    struct memDescriptor *prev;        //4
    struct memDescriptor *next;        //4
    void *baseAddr;    //4
    uint32_t limit;        //4
};

#define MALLOC_ALIGN_SIZE  32
#define MALLOC_ALIGN(size) (size + ((((size) % MALLOC_ALIGN_SIZE) == 0)? 0 : (MALLOC_ALIGN_SIZE - ((size) % MALLOC_ALIGN_SIZE))))

static int insertFreeDesc( struct memDescriptor *freeDesc );

static struct memDescriptor *usedKernDesc = 0x0;
static struct memDescriptor *freeKernDesc = 0x0;
static struct memDescriptor *emptyKernDesc = 0x0;

static void *getEmptyDesc() {
  int i = 0x0;
  struct memDescriptor *tmpDesc = 0x0;

  tmpDesc = emptyKernDesc;

  if ( tmpDesc != 0x0 ) {
    emptyKernDesc = tmpDesc->next;
    if ( emptyKernDesc != 0x0 )
      emptyKernDesc->prev = 0x0;

    tmpDesc->next = 0x0;
    tmpDesc->prev = 0x0;
    return ( tmpDesc );
  }

  if ( ( emptyKernDesc = (struct memDescriptor *) getPage( 0x4 ) ) == 0x0 )
    return ( 0x0 );

  /* zero out the memory so we know there is no garbage */
  memset( emptyKernDesc, 0x0, 0x4000 );

  emptyKernDesc[0].next = &emptyKernDesc[1];

  for ( i = 0x1; i < ( ( 0x4000 / sizeof(struct memDescriptor) ) ) ; i++ ) {
    emptyKernDesc[i].next = &emptyKernDesc[i + 1];
    emptyKernDesc[i].prev = &emptyKernDesc[i - 1];
  }

  tmpDesc = emptyKernDesc;

  emptyKernDesc = tmpDesc->next;
  emptyKernDesc->prev = 0x0;
  tmpDesc->next = 0x0;
  tmpDesc->prev = 0x0;
  return ( tmpDesc );
}

/************************************************************************

 Function: void *kmalloc(uint32_t len)
 Description: Allocate Kernel Memory

 Notes:

 02/17/03 - Do I Still Need To Pass In The Pid?

 ************************************************************************/
void *malloc( uint32_t len ) {
  struct memDescriptor *tmpDesc1 = 0x0;
  struct memDescriptor *tmpDesc2 = 0x0;
  char *buf = 0x0;
  int i = 0x0;

  len = MALLOC_ALIGN( len );

  if ( len == 0x0 ) {
    return ( 0x0 );
  }
  for ( tmpDesc1 = freeKernDesc; tmpDesc1 != 0x0 ; tmpDesc1 = tmpDesc1->next ) {
    if ( tmpDesc1->limit >= len ) {
      if ( tmpDesc1->prev != 0x0 )
        tmpDesc1->prev->next = tmpDesc1->next;
      if ( tmpDesc1->next != 0x0 )
        tmpDesc1->next->prev = tmpDesc1->prev;

      if ( tmpDesc1 == freeKernDesc )
        freeKernDesc = tmpDesc1->next;
      tmpDesc1->prev = 0x0;
      tmpDesc1->next = usedKernDesc;
      if ( usedKernDesc != 0x0 )
        usedKernDesc->prev = tmpDesc1;
      usedKernDesc = tmpDesc1;
      if ( tmpDesc1->limit > ( len + 32 ) ) {
        tmpDesc2 = getEmptyDesc();
        tmpDesc2->limit = tmpDesc1->limit - len;
        tmpDesc1->limit = len;
        tmpDesc2->baseAddr = tmpDesc1->baseAddr + len;
        tmpDesc2->next = 0x0;
        tmpDesc2->prev = 0x0;
        if ( tmpDesc2->limit <= 0x0 )
          insertFreeDesc( tmpDesc2 );
      }
      buf = (char *) tmpDesc1->baseAddr;
      for ( i = 0; i < tmpDesc1->limit ; i++ ) {
        buf[i] = (char) 0x0;
      }
      return ( tmpDesc1->baseAddr );
    }
  }
  tmpDesc1 = getEmptyDesc();
  if ( tmpDesc1 != 0x0 ) {
    tmpDesc1->baseAddr = (struct memDescriptor *) getPage( ( len + 4095 ) / 4096 );
    tmpDesc1->limit = len;
    tmpDesc1->next = usedKernDesc;
    tmpDesc1->prev = 0x0;
    if ( usedKernDesc != 0x0 )
      usedKernDesc->prev = tmpDesc1;
    usedKernDesc = tmpDesc1;

    if ( ( len % 0x1000 ) > 0 ) {
      tmpDesc2 = getEmptyDesc();
      tmpDesc2->baseAddr = tmpDesc1->baseAddr + tmpDesc1->limit;
      tmpDesc2->limit = ( ( ( len + 4095 ) / 4096 ) * 4096 ) - tmpDesc1->limit;
      tmpDesc2->prev = 0x0;
      tmpDesc2->next = 0x0;
      if ( tmpDesc2->limit <= 0x0 )
        insertFreeDesc( tmpDesc2 );
    }
    buf = (char *) tmpDesc1->baseAddr;
    for ( i = 0; i < tmpDesc1->limit ; i++ ) {
      buf[i] = (char) 0x0;
    }

    return ( tmpDesc1->baseAddr );
  }
  //Return Null If Unable To Malloc

  return ( 0x0 );
}

/************************************************************************

 Function: void kfree(void *baseAddr)
 Description: This Will Find The Descriptor And Free It

 Notes:

 02/17/03 - I need To Make It Join Descriptors

 ************************************************************************/
void free( void *baseAddr ) {
  struct memDescriptor *tmpDesc1 = 0x0;

  for ( tmpDesc1 = usedKernDesc; tmpDesc1 != 0x0 ; tmpDesc1 = tmpDesc1->next ) {
    if ( tmpDesc1->baseAddr == baseAddr ) {

      if ( usedKernDesc == tmpDesc1 )
        usedKernDesc = tmpDesc1->next;

      if ( tmpDesc1->prev != 0x0 )
        tmpDesc1->prev->next = tmpDesc1->next;

      if ( tmpDesc1->next != 0x0 )
        tmpDesc1->next->prev = tmpDesc1->prev;

      tmpDesc1->next = 0x0;
      tmpDesc1->prev = 0x0;

      insertFreeDesc( tmpDesc1 );

      //mergeMemBlocks();

      return;
    }
  }
  printf( "Kernel: Error Freeing Descriptor! [0x%X]\n", (uint32_t) baseAddr );

  return;
}

/************************************************************************

 Function: void insertFreeDesc(struct memDescriptor *freeDesc)
 Description: This Function Inserts A Free Descriptor On The List Which Is
 Kept In Size Order

 Notes:

 02/17/03 - This Was Inspired By TCA's Great Wisdom -
 "[20:20:59] <TCA> You should just insert it in order"

 ************************************************************************/
static int insertFreeDesc( struct memDescriptor *freeDesc ) {
  struct memDescriptor *tmpDesc = 0x0;

  if ( freeDesc->limit <= 0x0 ) {
    printf( "Inserting Descriptor with no limit\n" );
    while ( 1 )
      ;
  }

  if ( freeKernDesc != 0x0 ) {
    /*
     */
    freeDesc->next = freeKernDesc;
    freeDesc->prev = 0x0;
    freeKernDesc->prev = freeDesc;
    freeKernDesc = freeDesc;
    /*
     */
    /*
     for (tmpDesc = freeKernDesc;tmpDesc != 0x0;tmpDesc = tmpDesc->next) {
     if (freeDesc->limit <= tmpDesc->limit) {
     freeDesc->prev = tmpDesc->prev;
     tmpDesc->prev  = freeDesc;
     freeDesc->next = tmpDesc;

     if (tmpDesc == freeKernDesc)
     freeKernDesc = freeDesc;
     return(0x0);
     }
     if (tmpDesc->next == 0x0) {
     tmpDesc->next = freeDesc;
     freeDesc->prev = tmpDesc;
     freeDesc->next = 0x0;
     return(0x0);
     }
     }
     kpanic("didnt Insert\n");
     */
    return ( 0x0 );
  }
  else {
    freeDesc->prev = 0x0;
    freeDesc->next = 0x0;
    freeKernDesc = freeDesc;
    return ( 0x0 );
  }

  return ( 0x1 );
}

/***
 $Log: malloc.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:10  reddawg
 ubix2

 Revision 1.5  2006/06/01 03:58:33  reddawg
 wondering about this stuff here

 Revision 1.4  2005/10/21 20:57:35  reddawg
 Ubix Is Back In A Working Condition

 Revision 1.3  2005/10/21 20:07:07  reddawg
 Work has resumed

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:22:57  reddawg
 no message

 Revision 1.7  2004/10/02 12:58:38  reddawg
 weird...

 Revision 1.6  2004/09/29 22:30:52  reddawg
 hmm permission error

 Revision 1.5  2004/07/28 00:17:05  reddawg
 Major:
 Disconnected page 0x0 from the system... Unfortunately this broke many things
 all of which have been fixed. This was good because nothing deferences NULL
 any more.

 Things affected:
 malloc,kmalloc,getfreepage,getfreevirtualpage,pagefault,fork,exec,ld,ld.so,exec,file

 Revision 1.4  2004/06/17 14:18:41  reddawg
 Fixed some potential problems

 Revision 1.3  2004/05/22 02:32:41  reddawg
 Fixed a typo

 Revision 1.2  2004/05/20 23:10:46  reddawg
 New Userland Malloc

 Revision 1.4  2004/05/20 22:54:02  reddawg
 Cleaned Up Warrnings

 Revision 1.3  2004/05/19 04:07:43  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.2  2004/04/20 00:53:16  reddawg
 Works

 Revision 1.1.1.1  2004/04/15 12:07:10  reddawg
 UbixOS v1.0

 Revision 1.21  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
