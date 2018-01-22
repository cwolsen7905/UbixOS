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
#include <ubixos/sched.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/vitals.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/bioscall.h>
}

#include <sde/sde.h>
#include <objgfx40/objgfx40.h>
#include <sde/ogDisplay_UbixOS.h>
#include <objgfx40/ogFont.h>

extern "C" void sdeThread() {
  ogSurface *screen = new ogDisplay_UbixOS();


  struct sdeWindows *tmp = 0x0;
  ogSurface *buf = 0x0;
  ogBitFont * font = new ogBitFont();


  font->Load("/var/fonts/ROM8X8.DPF", 0);
  font->SetFGColor(255, 255, 255, 255);
  font->SetBGColor(0, 0, 0, 255);

  printOff = 0x1;

  screen->ogCreate(800, 600, OG_PIXFMT_16BPP);
  screen->ogClear(screen->ogPack(122, 140, 163));

  systemVitals->screen = screen;
  systemVitals->font = font;

  ogprintOff = (int) 0x0;

  screen->ogSetAntiAliasing(false);

  //ogSurface::RawLine(100, 100, 200, 200, 0xDEADBEEF)
  screen->RawLine(100, 100, 200, 200, 0xDEADBEEF);

  while (1) {
    for (tmp = windows; tmp; tmp = tmp->next) {
      switch (tmp->status) {
        case registerWindow:
          buf = (ogSurface *) tmp->buf;
          buf->buffer = (void *) vmm_mapFromTask(tmp->pid, buf->buffer, buf->bSize);
          if (buf->buffer == 0x0) {
            kprintf("Error: buf->buffer\n");
            while (1)
              asm("nop");
          }
          buf->lineOfs = (uInt32 *) vmm_mapFromTask(tmp->pid, buf->lineOfs, buf->lSize);
          if (buf->lineOfs == 0x0) {
            kprintf("Error: buf->lineOfs\n");
            while (1)
              ;
          }
          tmp->status = windowReady;
          //kprintf("Window Registered!\n");
        break;
        case drawWindow:
          buf = (ogSurface *) tmp->buf;
          screen->ogCopyBuf(screen->ogGetMaxX() - buf->ogGetMaxX(), screen->ogGetMaxY() - buf->ogGetMaxY(), *buf, 0, 0, buf->ogGetMaxX(), buf->ogGetMaxY());
          tmp->status = windowReady;
          //kprintf("Draw Window Routines Here\n");
        break;
        case killWindow:
          //kprintf("Killed Window\n");
          if (tmp->next != 0x0) {
            tmp->next->prev = tmp->prev;
            if (tmp->prev != 0x0)
              tmp->prev->next = tmp->next;
          }
          else if (tmp->prev != 0x0) {
            tmp->prev->next = tmp->next;
            if (tmp->next != 0x0)
              tmp->next->prev = tmp->prev;
          }
          else {
            windows = 0x0;
          }
          vmm_unmapPages(buf->buffer, buf->bSize, VMM_KEEP);
          vmm_unmapPages(buf->lineOfs, buf->lSize, VMM_KEEP);
          //kfree(tmp->buf);
          kfree(tmp);
          tmp = 0x0;
        break;
        default:
          sched_yield();
        break;
      }
    }
  }
}
