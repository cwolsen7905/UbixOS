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

#ifndef _UBIXOS_TTY_H
#define _UBIXOS_TTY_H

#include <sys/types.h>

#define TTY_MAX_TERMS 5

/* tty_setmode cmd values */
#define TTY_SETRAW   0  /* val 1 = raw, 0 = canonical */
#define TTY_SETECHO  1  /* val 1 = echo on, 0 = echo off */

typedef struct tty_termNode {
    char *tty_buffer;
    char *tty_pointer;
    uint8_t tty_colour;
    uint16_t tty_x;
    uint16_t tty_y;
    pidType owner;
    char stdin[512];
    int stdinSize;
    /* Line discipline */
    char t_linebuf[512]; /* canonical input buffer (ISR fills until Enter) */
    int  t_linelen;      /* chars currently in t_linebuf */
    uint8_t t_echo;      /* 1 = echo input to terminal (default) */
    uint8_t t_raw;       /* 1 = raw mode: bypass line discipline */
} tty_term;

int tty_init();
int tty_change(uInt16);
tty_term *tty_find(uInt16);
int tty_print(char *, tty_term *);

extern tty_term *tty_foreground;

#endif
