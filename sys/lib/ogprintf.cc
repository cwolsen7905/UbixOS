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

#include <objgfx40/objgfx40.h>
#include <objgfx40/ogFont.h>
#include <sde/ogDisplay_UbixOS.h>

#include <ubixos/vitals.h>

  static int screenRow = 0x0;
  static int screenCol = 0x1;

  int ogPrintf(char *ch) {
    int i = 0x0;
    int bufHeight;

    ogSurface *screen = (ogDisplay_UbixOS *) systemVitals->screen;
    ogBitFont *font = (ogBitFont *) systemVitals->font;

    char *s = 0;
    s = ch;

    while ('\0' != s[i]) {
      switch (s[i]) {
        case '\t':
          screenCol += 3;
        break;
        case '\b':
          if (screenCol > 0)
            --screenCol;
        case '\n':
          screenCol = 0;

          bufHeight = ((screen->ogGetMaxY() + 1) / font->GetHeight()) - 1;
          if (screenRow < bufHeight)
            ++screenRow;
          else {
            screen->ogCopyBuf(0, 0, *screen, 0, font->GetHeight(), screen->ogGetMaxX(), screen->ogGetMaxY());
            screen->ogFillRect(0, bufHeight * font->GetHeight() + 1, screen->ogGetMaxX(), screen->ogGetMaxY(), screen->ogPack(122, 140, 163));
          }
        break;
        default:
          font->PutChar(*screen, screenCol * font->GetWidth(), screenRow * font->GetHeight(), s[i]);
        break;
      } /* switch */

      ++screenCol;
      ++i;

    } /* while */

    return 0;
  } // ogPrintf

}
