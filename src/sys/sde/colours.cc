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

 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:36  reddawg
 no message

 Revision 1.2  2004/05/19 17:09:50  flameshadow
 chg: Undid previous renaming. This now restores me as the EOOUIAD.

 Revision 1.1.1.1  2004/04/15 12:07:16  reddawg
 UbixOS v1.0

 Revision 1.23  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license



 $Id$

*****************************************************************************************/

#include <sde/sde.h>
#include <sde/ogDisplay_UbixOS.h>
#include <objgfx40/objgfx40.h>

extern "C" {
  #include <sys/video.h>
  #include <vmm/paging.h>
  #include <lib/string.h>
  #include <lib/kprintf.h>
  #include <lib/kmalloc.h>
  #include <ubixos/sched.h>
  #include <ubixos/vitals.h>
  }

extern "C" {
void sdeTestThread() {
//  uInt32 count, i = 0x0;
  uInt8 r, g, b;
  ogSurface *screen = 0x0;
  ogPoint2d points[4];
  ogRGBA8 colours[4];
  r = g = b = 0;
  while (screen == 0x0) {
    screen  = (ogDisplay_UbixOS *)systemVitals->screen;
    }
  while (!screen->ogAvail());

  points[0].x = screen->ogGetMaxX() - 150;
  points[0].y = 0;
  points[1].x = screen->ogGetMaxX();
  points[1].y = 0;
  points[2].x = screen->ogGetMaxX();
  points[2].y = 150;
  points[3].x = screen->ogGetMaxX() - 250;
  points[3].y = 250;

  colours[0].red = 255;
  colours[0].green = 0;
  colours[0].blue = 0;
  colours[0].alpha = 255;
  colours[1].red = 0;
  colours[1].green = 255;
  colours[1].blue = 128;
  colours[1].alpha = 255;
  colours[2].red = 128;
  colours[2].green = 255;
  colours[2].blue = 128;
  colours[2].alpha = 255;
  colours[3].red = 63;
  colours[3].green = 63;
  colours[3].blue = 63;
  colours[3].alpha = 255;
  screen->ogSetAntiAliasing(true);

  while (true) {
#if 0
    for (count = 150; count > 0; count--) {
      screen->Line(screen->GetMaxX() / 2, screen->GetMaxY() / 2,
                     screen->GetMaxX(), count*8,
                     screen->RGB(r, g, b));
      screen->FillCircle(screen->GetMaxX() - 50, 50, count, 
                           screen->RGB(r, g, b));
                         
//      screen->FillRect(screen->GetMaxX() - 50 - count, count, 
//                     screen->GetMaxX() - count, 100 - count,
//                     screen->RGB(r, g, b));
      r -= 8;
      g += 8;
      b -= 8;
    } // for
#endif
  screen->ogFillGouraudPolygon(4, points, colours);
  //kprintf("colours(0)[0x%X]\n",colours[0]);
  colours[0].red   -= 8;
  colours[0].green += 8;
  colours[0].blue -= 8;
  colours[1].red  += 8;
  colours[1].green += 8;
  colours[1].blue -= 8;
  colours[2].red += 8;
  colours[2].green -= 8;
  colours[2].blue += 8;
  colours[3].red += 8;
  colours[3].green += 8;
  colours[3].blue += 8;

  } // while

} // sdeTestThread

}

/***
 END
 ***/

