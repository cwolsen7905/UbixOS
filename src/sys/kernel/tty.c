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

 $Id: tty.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <ubixos/tty.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/io.h>
#include <string.h>

static tty_term  *terms          = 0x0;
tty_term         *tty_foreground = 0x0;
static struct spinLock tty_spinLock   = SPIN_LOCK_INITIALIZER;

int tty_init() {
  int i = 0x0;

  /* Allocate memory for terminals */
  terms = (tty_term *)kmalloc(sizeof(tty_term)*TTY_MAX_TERMS);
  if (terms == 0x0)
    kpanic("tty_init: Failed to allocate memory. File: %s, Line: %i\n",__FILE__,__LINE__);

  /* Set up all default terminal information */
  for (i = 0;i < TTY_MAX_TERMS;i++) {
    terms[i].tty_buffer = (char *)kmalloc(80*60*2);
    if (terms[i].tty_buffer == 0x0)
      kpanic("tty_init: Failed to allocate buffer memory. File: %s, Line: %i\n",__FILE__,__LINE__);

    terms[i].tty_pointer = terms[i].tty_buffer;   /* Set up tty pointer to internal buffer */
    terms[i].tty_x       = 0x0;                   /* Set up default X position             */
    terms[i].tty_y       = 0x0;                   /* Set up default Y position             */
    terms[i].tty_colour  = 0x0A + i;              /* Set up default tty text colour        */
    }

  /* Read tty0 current position (to migrate from kprintf). */
  outportByte(0x3D4, 0x0e);
  terms[0].tty_y = inportByte(0x3D5);
  outportByte(0x3D4, 0x0f);
  terms[0].tty_x = inportByte(0x3D5);


  /* Set up pointer for the foreground tty */
  tty_foreground = &terms[0];

  /* Set up the foreground ttys information */
  tty_foreground->tty_pointer = (char *)0xB8000;

  /* Return to let kernel know initialization is complete */
  kprintf("tty0 - Initialized\n");

  return(0x0);
  }


 /*
  This will change the specified tty. It ultimately copies the screen
  to the foreground buffer copies the new ttys buffer to the screen and
  adjusts a couple pointers and we are good to go.
 */
int tty_change(uInt16 tty) {

  if (tty > TTY_MAX_TERMS)
    kpanic("Error: Changing to an invalid tty. File: %s, Line: %i\n",__FILE__,__LINE__);

  /* Copy display buffer to tty buffer */
  memcpy(tty_foreground->tty_buffer,(char *)0xB8000,(80*60*2));

  /* Copy new tty buffer to display buffer */
  memcpy((char *)0xB8000,terms[tty].tty_buffer,(80*60*2));

  /*
   Set the tty_pointer to the internal buffer so I can continue 
   writing to what it believes is the screen
  */
  tty_foreground->tty_pointer = tty_foreground->tty_buffer;

  terms[tty].tty_pointer = (char *)0xB8000;

  /* set new foreground tty */
  tty_foreground = &terms[tty];

  /* Adjust cursor when we change consoles */
  outportByte(0x3D4, 0x0F);
  outportByte(0x3D5, tty_foreground->tty_x);
  outportByte(0x3D4, 0x0E);
  outportByte(0x3D5, tty_foreground->tty_y);

  return(0x0);
  }

int tty_print(char *string,tty_term *term) {
  unsigned int    bufferOffset = 0x0, character = 0x0, i = 0x0;
  spinLock(&tty_spinLock);

  /* We Need To Get The Y Position */
  bufferOffset = term->tty_y;
  bufferOffset <<= 8;

  /* Then We Need To Add The X Position */
  bufferOffset += term->tty_x;
  bufferOffset <<= 1;

  while ((character = *string++)) {
    switch (character) {
    case '\n':
      bufferOffset = (bufferOffset / 160) * 160 + 160;
      break;
    default:
      term->tty_pointer[bufferOffset++] = character;
      term->tty_pointer[bufferOffset++] = term->tty_colour;
      break;
    }  /* switch */

    /* Check To See If We Are Out Of Bounds */
    if (bufferOffset >= 160 * 25) {
      for (i = 0; i < 160 * 24; i++) {
        term->tty_pointer[i] = term->tty_pointer[i + 160];
        }
      for (i = 0; i < 80; i++) {
        term->tty_pointer[(160 * 24) + (i * 2)] = 0x20;
        term->tty_pointer[(160 * 24) + (i * 2) + 1] = term->tty_colour;
        }
      bufferOffset -= 160;
      }
    }

  bufferOffset >>= 1;  /* Set the new cursor position  */
  term->tty_x = (bufferOffset &  0xFF);
  term->tty_y = (bufferOffset >> 0x8);

  if (term == tty_foreground) {
    outportByte(0x3D4, 0x0f);
    outportByte(0x3D5, term->tty_x);
    outportByte(0x3D4, 0x0e);
    outportByte(0x3D5, term->tty_y);
    }

  spinUnlock(&tty_spinLock);

  return(0x0);
  }

tty_term *tty_find(uInt16 tty) {
  return(&terms[tty]);
  }

/***
 END
 ***/
