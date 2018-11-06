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

#ifndef _LIB_KMALLOC_H
#define _LIB_KMALLOC_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define sysID      -2
#define MALLOC_ALIGN_SIZE  32
#define MALLOC_ALIGN(size) (size + ((((size) % (MALLOC_ALIGN_SIZE)) == 0)? 0 : ((MALLOC_ALIGN_SIZE) - ((size) % (MALLOC_ALIGN_SIZE)))))

  struct memDescriptor {
      struct memDescriptor *prev;        //4
      struct memDescriptor *next;        //4
      void *baseAddr;    //4
      uInt32 limit;        //4
      /*uInt8                status;       //1  */
      /*char                 reserved[11]; //11 */
  };

  void kfree(void *baseAddr);
  void *kmalloc(uInt32 len);

#ifdef __cplusplus
}
#endif

#endif

/***
 $Log: kmalloc.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:40  reddawg
 no message

 Revision 1.7  2004/09/14 20:57:01  reddawg
 Bug fixes: macro problem over opt a multiply

 Revision 1.6  2004/07/21 10:02:09  reddawg
 devfs: renamed functions
 device system: renamed functions
 fdc: fixed a few potential bugs and cleaned up some unused variables
 strol: fixed definition
 endtask: made it print out freepage debug info
 kmalloc: fixed a huge memory leak we had some unhandled descriptor insertion so some descriptors were lost
 ld: fixed a pointer conversion
 file: cleaned up a few unused variables
 sched: broke task deletion
 kprintf: fixed ogPrintf definition

 Revision 1.5  2004/07/19 02:08:27  reddawg
 Cleaned out the rest of debuging code also temporarily disabled the ip stack to improve boot time

 Revision 1.4  2004/07/18 05:24:15  reddawg
 Fixens

 Revision 1.3  2004/05/21 15:00:27  reddawg
 Cleaned up

 END
 ***/
