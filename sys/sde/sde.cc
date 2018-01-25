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

extern "C" {
#include <sys/types.h>
#include <sys/video.h>
#include <ubixos/sched.h>
#include <ubixos/vitals.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>
}

#include <sde/sde.h>
#include <sde/ogDisplay_UbixOS.h>
#include <objgfx40/objgfx40.h>

struct sdeWindows *windows = 0x0;

extern "C" int sysSDE(struct thread *td, struct sys_sde_args *args) {

  ogSurface *newBuf = 0x0;
  ogSurface *oldBuf = (ogSurface *)args->ptr;
  ogSurface *screen = 0x0;

  struct sdeWindows *tmp = 0x0;

  screen = (ogDisplay_UbixOS *) systemVitals->screen;

  for (tmp = windows; tmp; tmp = tmp->next) {
    if (tmp->pid == (int) _current->id)
      break;
  }

  if (tmp != 0x0) {
    while (tmp->status != windowReady)
      asm("hlt");
  }
  else if (tmp == 0x0 && args->cmd != registerWindow) {
    if (args->cmd == drawWindow) {
      //screen->ogCopyBuf(screen->ogGetMaxX() - oldBuf->ogGetMaxX(), screen->ogGetMaxY() - oldBuf->ogGetMaxY(), *oldBuf, 0, 0, oldBuf->ogGetMaxX(), oldBuf->ogGetMaxY());
    }

    //kprintf("Invalid Window\n");
    td->td_retval[0] = -1;
    return(-1);
  }

  switch (args->cmd) {
    case drawWindow:
      /*
      screen->ogCopyBuf(screen->ogGetMaxX() - oldBuf->ogGetMaxX(), screen->ogGetMaxY() - oldBuf->ogGetMaxY(), *oldBuf, 0, 0, oldBuf->ogGetMaxX(), oldBuf->ogGetMaxY());
      kprintf("Draw Window\n");
      while(1) asm("nop");
      */

      tmp->status = drawWindow;
      while (tmp->status != windowReady) {
        asm("nop");
        //asm("hlt");
      }
    break;
    case killWindow:
      tmp->status = killWindow;
    break;
    case registerWindow:
      if (oldBuf->buffer != 0x0) {
        newBuf = new ogSurface();
        newBuf->version = oldBuf->version;
        newBuf->buffer = oldBuf->buffer;
        newBuf->owner = oldBuf->owner;
        newBuf->lineOfs = oldBuf->lineOfs;
        newBuf->pal = oldBuf->pal;
        newBuf->attributes = oldBuf->attributes;
        newBuf->xRes = oldBuf->xRes;
        newBuf->yRes = oldBuf->yRes;
        newBuf->maxX = oldBuf->maxX;
        newBuf->maxY = oldBuf->maxY;
        newBuf->bSize = oldBuf->bSize;
        newBuf->lSize = oldBuf->lSize;
        newBuf->BPP = oldBuf->BPP;
        newBuf->bytesPerPix = oldBuf->bytesPerPix;
        newBuf->pixFmtID = oldBuf->pixFmtID;
        newBuf->redFieldPosition = oldBuf->redFieldPosition;
        newBuf->greenFieldPosition = oldBuf->greenFieldPosition;
        newBuf->blueFieldPosition = oldBuf->blueFieldPosition;
        newBuf->alphaFieldPosition = oldBuf->alphaFieldPosition;
        newBuf->redShifter = oldBuf->redShifter;
        newBuf->greenShifter = oldBuf->greenShifter;
        newBuf->blueShifter = oldBuf->blueShifter;
        newBuf->alphaShifter = oldBuf->alphaShifter;
        newBuf->lastError = oldBuf->lastError;
        newBuf->dataState = oldBuf->dataState;
        tmp = (struct sdeWindows *) kmalloc(sizeof(struct sdeWindows));
        tmp->buf = newBuf;
        tmp->status = registerWindow;
        tmp->pid = _current->id;
        tmp->prev = 0x0;
        if (windows == 0x0) {
          windows = tmp;
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
      kprintf("Invalid SDE Command [0x%X]\n", args->ptr);
    break;
  }

  td->td_retval[0] = 0;
  return(0);
}

/***
 END
 ***/

