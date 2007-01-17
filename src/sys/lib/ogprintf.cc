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

 $Id$

*****************************************************************************************/

#include <objgfx40/objgfx40.h>
#include <objgfx40/ogFont.h>
#include <sde/ogDisplay_UbixOS.h>

extern "C" {

#include <ubixos/vitals.h>


static int screenRow  = 0x0;
static int screenCol  = 0x1;
  
int ogPrintf(char *s) {
  int i = 0x0;
  int bufHeight;
  ogSurface *screen = (ogDisplay_UbixOS *)systemVitals->screen;
  ogBitFont *font   = (ogBitFont *)systemVitals->font; 


  while ('\0' != s[i]) {
    switch (s[i]) {
    case '\t':
      screenCol += 3;
      break;
    case '\b':
      if (screenCol > 0) --screenCol;
    case '\n':
      screenCol = 0;

      bufHeight = ((screen->ogGetMaxY()+1) / font->GetHeight())-1;
      if (screenRow < bufHeight) 
        ++screenRow;
      else {
        screen->ogCopyBuf(0, 0,
                          *screen,
                          0, font->GetHeight(),
                          screen->ogGetMaxX(), screen->ogGetMaxY());
        screen->ogFillRect(0, bufHeight * font->GetHeight()+1,
                           screen->ogGetMaxX(), screen->ogGetMaxY(),
                           screen->ogPack(122, 140, 163));
      }
      break;
    default:
      font->PutChar(*screen,
                    screenCol * font->GetWidth(),
                    screenRow * font->GetHeight(),
                    s[i]);
      break;
    }                           /* switch */
    ++screenCol;
    ++i;
  } /* while */

  return 0;
} // ogPrintf

}

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:13  reddawg
 no message

 Revision 1.7  2004/07/20 22:58:33  reddawg
 retiring to the laptop for the night must sync in work to resume from there

 Revision 1.6  2004/07/17 14:24:22  reddawg
 compile: changes to the way we link the kernel should prevent future errors

 Revision 1.5  2004/05/23 23:30:34  reddawg
 Fixens

 Revision 1.4  2004/05/23 01:40:19  reddawg
 Spinlock

 Revision 1.3  2004/05/19 17:09:50  flameshadow
 chg: Undid previous renaming. This now restores me as the EOOUIAD.

 Revision 1.2  2004/04/26 12:56:01  reddawg
 Made src/sys/sde Copy and Make the ogPixelFormat.cpp

 Revision 1.1.1.1  2004/04/15 12:07:10  reddawg
 UbixOS v1.0

 Revision 1.19  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
