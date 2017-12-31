/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Log: sde.cc,v $
 Revision 1.1.1.1  2006/06/01 12:46:17  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:37  reddawg
 no message

 Revision 1.3  2004/08/21 23:47:50  reddawg
 *** empty log message ***

 Revision 1.2  2004/05/19 04:07:43  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.1.1.1  2004/04/15 12:07:17  reddawg
 UbixOS v1.0

 Revision 1.12  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license



 $Id: sde.cc 89 2016-01-12 00:20:40Z reddawg $

*****************************************************************************************/

extern "C" {
  #include <ubixos/types.h>
  #include <sys/video.h>
  #include <vmm/paging.h>
  #include <lib/string.h>
  #include <lib/kprintf.h>
  #include <lib/kmalloc.h>
  #include <ubixos/sched.h>
  #include <sde/sde.h>
}
#include <objgfx40/objgfx40.h>

struct sdeWindows *windows = 0x0;

extern "C" void sysSDE(uInt32 cmd,void *ptr) {
  ogSurface *newBuf = 0x0;
  ogSurface *oldBuf = (ogSurface *)ptr;
  struct sdeWindows *tmp = 0x0;
  
  for (tmp=windows;tmp;tmp=tmp->next) {
    if (tmp->pid == (int)_current->id)
      break;
    }
    
  if (tmp != 0x0) {
    while (tmp->status != windowReady)
      asm("hlt");
      //sched_yield();
    }
  else if (tmp == 0x0 && cmd != registerWindow) {
    kprintf("Invalid Window\n");
    return;
    }

  switch (cmd) {
    case drawWindow:
      tmp->status = drawWindow;
      while (tmp->status != windowReady) {
        //sched();
        //asm("hlt");
        }
      break;
    case killWindow:
      tmp->status = killWindow;
      break;
    case registerWindow:
      if (oldBuf->buffer != 0x0) {
        newBuf                     = new ogSurface();
        newBuf->version            = oldBuf->version;
        newBuf->buffer             = oldBuf->buffer;
        newBuf->owner              = oldBuf->owner;
        newBuf->lineOfs            = oldBuf->lineOfs;
        newBuf->pal                = oldBuf->pal;
        newBuf->attributes         = oldBuf->attributes;
        newBuf->xRes               = oldBuf->xRes;
        newBuf->yRes               = oldBuf->yRes;
        newBuf->maxX               = oldBuf->maxX;
        newBuf->maxY               = oldBuf->maxY;
        newBuf->bSize              = oldBuf->bSize;
        newBuf->lSize              = oldBuf->lSize; 
        newBuf->BPP                = oldBuf->BPP;  
        newBuf->bytesPerPix        = oldBuf->bytesPerPix;
        newBuf->pixFmtID           = oldBuf->pixFmtID;
        newBuf->redFieldPosition   = oldBuf->redFieldPosition;
        newBuf->greenFieldPosition = oldBuf->greenFieldPosition;
        newBuf->blueFieldPosition  = oldBuf->blueFieldPosition;
        newBuf->alphaFieldPosition = oldBuf->alphaFieldPosition;
        newBuf->redShifter         = oldBuf->redShifter;
        newBuf->greenShifter       = oldBuf->greenShifter;
        newBuf->blueShifter        = oldBuf->blueShifter;
        newBuf->alphaShifter       = oldBuf->alphaShifter;
        newBuf->lastError          = oldBuf->lastError;
        newBuf->dataState          = oldBuf->dataState;
        tmp = (struct sdeWindows *)kmalloc(sizeof(struct sdeWindows));
        tmp->buf    = newBuf;
        tmp->status = registerWindow;
        tmp->pid    = _current->id;
        tmp->prev   = 0x0;
        if (windows == 0x0) {
          windows   = tmp;
          tmp->next = 0x0;
          }
        else {
          tmp->next = windows;
          windows->prev = tmp;
          windows = tmp;
          }
        }
      else {
        kprintf("Invalid Window\n");
        }
      break;
    default:
      kprintf("Invalid SDE Command [0x%X]\n",ptr);
      break;
    }
  return;
  }

/***
 END
 ***/

