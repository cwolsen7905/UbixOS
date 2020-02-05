/*-
 * Copyright (c) 2002-2018, 2020 The UbixOS Project.
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

#include <sys/io.h>
#include <sys/video.h>
#include <ubixos/spinlock.h>
#include <ubixos/tty.h>

static unsigned char *videoBuffer = (unsigned char *) 0xB8000;
int printColor = defaultColor;

void backSpace() {
  uInt32 bufferOffset = 0x0;
  outportByte(0x3d4, 0x0e);
  bufferOffset = inportByte(0x3d5);
  bufferOffset <<= 0x8; /* Shift Address Left 8 Bits */
  outportByte(0x3d4, 0x0f);
  bufferOffset += inportByte(0x3d5);
  bufferOffset <<= 0x1; /* Shift Address Left 1 Bits */
  videoBuffer[bufferOffset--] = 0x20;
  videoBuffer[bufferOffset--] = printColor;
  videoBuffer[bufferOffset] = 0x20;
  bufferOffset >>= 0x1;
  tty_foreground->tty_x = (bufferOffset & 0xFF);
  tty_foreground->tty_y = (bufferOffset >> 0x8);
  outportByte(0x3D4, 0x0f);
  outportByte(0x3D5, tty_foreground->tty_x);
  outportByte(0x3D4, 0x0e);
  outportByte(0x3D5, tty_foreground->tty_y);
  return;
}

void kprint(char *string) {

  unsigned int bufferOffset = 0x0, character = 0x0, i = 0x0;

  /* Short circuit if we're in tty mode */
  if (NULL != tty_foreground) {
    tty_print(string, tty_find(0));
    return;
  }

  /* We Need To Get The Y Position */
  outportByte(0x3D4, 0x0e);
  bufferOffset = inportByte(0x3D5);
  bufferOffset <<= 8; /* Shift Address Left 8 Bits */
  /* Then We Need To Add The X Position */
  outportByte(0x3D4, 0x0f);
  bufferOffset += inportByte(0x3D5);
  bufferOffset <<= 1; /* Shift Address Left 1 Bits */

  while ((character = *string++)) {
    switch (character) {
      case '\n':
        bufferOffset = (bufferOffset / 160) * 160 + 160;
      break;
      default:
        videoBuffer[bufferOffset++] = character;
        videoBuffer[bufferOffset++] = printColor;
      break;
    } /* switch */
    /* Check To See If We Are Out Of Bounds */
    if (bufferOffset >= 160 * 25) {
      for (i = 0; i < 160 * 24; i++) {
        videoBuffer[i] = videoBuffer[i + 160];
      } /* for */
      for (i = 0; i < 80; i++) {
        videoBuffer[(160 * 24) + (i * 2)] = 0x20;
        videoBuffer[(160 * 24) + (i * 2) + 1] = printColor;
      } /* for */
      bufferOffset -= 160;
    } /* if */
  } /* while */
  bufferOffset >>= 1; /* Set the new cursor position  */
  outportByte(0x3D4, 0x0f);
  outportByte(0x3D5, ((bufferOffset & 0x0ff) & 0xFF));
  outportByte(0x3D4, 0x0e);
  outportByte(0x3D5, ((bufferOffset >> 8) & 0xFF));

  return;
}

void kprint_len(char *string, int len) {

    unsigned int bufferOffset = 0x0, character = 0x0, i = 0x0;
    int x = 0;

    /* Short circuit if we're in tty mode */
    if (NULL != tty_foreground) {
        tty_print(string, tty_find(0));
        return;
    }

    /* We Need To Get The Y Position */
    outportByte(0x3D4, 0x0e);
    bufferOffset = inportByte(0x3D5);
    bufferOffset <<= 8; /* Shift Address Left 8 Bits */
    /* Then We Need To Add The X Position */
    outportByte(0x3D4, 0x0f);
    bufferOffset += inportByte(0x3D5);
    bufferOffset <<= 1; /* Shift Address Left 1 Bits */

    while ((character = *string++)) {
        if (x++ == len)
            break;

        switch (character) {
            case '\n':
                bufferOffset = (bufferOffset / 160) * 160 + 160;
                break;
            default:
                videoBuffer[bufferOffset++] = character;
                videoBuffer[bufferOffset++] = printColor;
                break;
        } /* switch */
        /* Check To See If We Are Out Of Bounds */
        if (bufferOffset >= 160 * 25) {
            for (i = 0; i < 160 * 24; i++) {
                videoBuffer[i] = videoBuffer[i + 160];
            } /* for */
            for (i = 0; i < 80; i++) {
                videoBuffer[(160 * 24) + (i * 2)] = 0x20;
                videoBuffer[(160 * 24) + (i * 2) + 1] = printColor;
            } /* for */
            bufferOffset -= 160;
        } /* if */
    } /* while */

    bufferOffset >>= 1; /* Set the new cursor position  */
    outportByte(0x3D4, 0x0f);
    outportByte(0x3D5, ((bufferOffset & 0x0ff) & 0xFF));
    outportByte(0x3D4, 0x0e);
    outportByte(0x3D5, ((bufferOffset >> 8) & 0xFF));

    return;
}

/* Clears The Screen */
void clearScreen() {
  short i = 0x0;

  for (i = 0x0; i < (80 * 25); i++) {
    videoBuffer[i * 2] = 0x20;
    videoBuffer[i * 2 + 1] = defaultColor;
  }
  outportByte(0x3D4, 0x0f);
  outportByte(0x3D5, 0);
  outportByte(0x3D4, 0x0e);
  outportByte(0x3D5, 0);
}

/***
 END
 ***/
