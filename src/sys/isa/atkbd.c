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

 $Id$

*****************************************************************************************/

#include <isa/atkbd.h>
#include <isa/8259.h>
#include <sys/video.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/io.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/types.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <ubixos/tty.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <ubixos/vitals.h>

static int atkbd_scan();

static unsigned int keyMap = 0x0;
static unsigned int ledStatus = 0x0;
static char   stdinBuffer[512];
static uInt16 stdinSize;
static uInt32 controlKeys = 0x0;

static spinLock_t atkbdSpinLock = SPIN_LOCK_INITIALIZER;

static unsigned int keyboardMap[255][8] = {
/*           Ascii,  Shift,   Ctrl,    Alt,    Num,   Caps, Shift Caps, Shift Num */
          {      0,      0,      0,      0,      0,      0,          0,         0},
/* ESC */ {   0x1B,   0x1B,   0x1B,   0x1B,   0x1B,   0x1B,       0x1B,      0x1B},
/* 1,! */ {   0x31,   0x21,      0,      0,   0x31,   0x31,       0x21,      0x21},
/* 2,@ */ {   0x32,   0x40,      0,      0,   0x32,   0x32,       0x40,      0x40},
/* 3,# */ {   0x33,   0x23,      0,      0,   0x33,   0x33,       0x23,      0x23},
/* 4,$ */ {   0x34,   0x24,      0,      0,   0x34,   0x34,       0x24,      0x24},
/* 5,% */ {   0x35,   0x25,      0,      0,   0x35,   0x35,       0x25,      0x25},
/* 6,^ */ {   0x36,   0x5E,      0,      0,   0x36,   0x36,       0x5E,      0x5E},
/* 7,& */ {   0x37,   0x26,      0,      0,   0x37,   0x37,       0x26,      0x26},
/* 8,* */ {   0x38,   0x2A,      0,      0,   0x38,   0x38,       0x2A,      0x2A},
/* 9.( */ {   0x39,   0x28,      0,      0,   0x39,   0x39,       0x28,      0x28},
/* 0,) */ {   0x30,   0x29,      0,      0,   0x30,   0x30,       0x29,      0x29},
/* -,_ */ {   0x2D,   0x5F,      0,      0,   0x2D,   0x2D,       0x5F,      0x5F},
/* =,+ */ {   0x3D,   0x2B,      0,      0,   0x3D,   0x3D,       0x2B,      0x2B},
/*  14 */ {   0x08,   0x08,    0x8,    0x8,   0x08,   0x08,       0x08,      0x08},
/*  15 */ {   0x09,      0,      0,      0,      0,      0,          0,         0},
/*     */ {   0x71,   0x51,      0,      0,      0,      0,          0,         0},
/*     */ {   0x77,   0x57,      0,      0,      0,      0,          0,         0},
/*     */ {   0x65,   0x45,      0,      0,      0,      0,          0,         0},
/*     */ {   0x72,   0x52,      0,      0,      0,      0,          0,         0},
/*     */ {   0x74,   0x54,      0,      0,      0,      0,          0,         0},
/*     */ {   0x79,   0x59,      0,      0,      0,      0,          0,         0},
/*     */ {   0x75,   0x55,      0,      0,      0,      0,          0,         0},
/*     */ {   0x69,   0x49,      0,      0,      0,      0,          0,         0},
/*     */ {   0x6F,   0x4F,      0,      0,      0,      0,          0,         0},
/*     */ {   0x70,   0x50,      0,      0,      0,      0,          0,         0},
/*     */ {   0x5B,   0x7B,      0,      0,      0,      0,          0,         0},
/*     */ {   0x5D,   0x7D,      0,      0,      0,      0,          0,         0},
/*     */ {   0x0A,      0,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0},
/* a,A */ {   0x61,   0x41,   0x41,      0,      0,      0,          0,         0},
/*     */ {   0x73,   0x53,      0,      0,      0,      0,          0,         0},
/*     */ {   0x64,   0x44,      0,      0,      0,      0,          0,         0},
/*     */ {   0x66,   0x46,      0,      0,      0,      0,          0,         0},
/*     */ {   0x67,   0x47,      0,      0,      0,      0,          0,         0},
/*     */ {   0x68,   0x48,      0,      0,      0,      0,          0,         0},
/*     */ {   0x6A,   0x4A,      0,      0,      0,      0,          0,         0},
/*     */ {   0x6B,   0x4B,      0,      0,      0,      0,          0,         0},
/*     */ {   0x6C,   0x4C,      0,      0,      0,      0,          0,         0},
/*     */ {   0x3B,   0x3A,      0,      0,      0,      0,          0,         0},
/*     */ {   0x27,   0x22,      0,      0,      0,      0,          0,         0},
/*     */ {   0x60,   0x7E,      0,      0,      0,      0,          0,         0},
/*     */ {   0x2A,    0x0,      0,      0,      0,      0,          0,         0},
/*     */ {   0x5C,   0x3C,      0,      0,      0,      0,          0,         0},
/*     */ {   0x7A,   0x5A,      0,      0,      0,      0,          0,         0},
/*     */ {   0x78,   0x58,      0,      0,      0,      0,          0,         0},
/* c,C */ {   0x63,   0x43,    0x3,    0x9,      0,      0,          0,         0},
/*     */ {   0x76,   0x56,      0,      0,      0,      0,          0,         0},
/*     */ {   0x62,   0x42,      0,      0,      0,      0,          0,         0},
/*     */ {   0x6E,   0x4E,      0,      0,      0,      0,          0,         0},
/*     */ {   0x6D,   0x4D,      0,      0,      0,      0,          0,         0},
/*     */ {   0x2C,   0x3C,      0,      0,      0,      0,          0,         0},
/*     */ {   0x2E,   0x3E,      0,      0,      0,      0,          0,         0},
/*     */ {   0x2F,   0x3F,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0},
/*     */ {   0x20,      0,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0},
/* F1  */ { 0x3000,      0,      0, 0x3000,      0,      0,          0,         0},
/*     */ { 0x3001,      0,      0, 0x3001,      0,      0,          0,         0},
/*     */ { 0x3002,      0,      0, 0x3002,      0,      0,          0,         0},
/*     */ { 0x3003,      0,      0, 0x3003,      0,      0,          0,         0},
/*     */ { 0x3004,      0,      0, 0x3004,      0,      0,          0,         0},
/*     */ { 0x4000,      0,      0, 0x3005,      0,      0,          0,         0},
/*     */ { 0x4100,      0,      0, 0x3006,      0,      0,          0,         0},
/*     */ { 0x4200,      0,      0, 0x3007,      0,      0,          0,         0},
/*     */ { 0x4300,      0,      0, 0x3008,      0,      0,          0,         0},
/*     */ { 0x4400,      0,      0, 0x3009,      0,      0,          0,         0},
/*     */ {      0,      0,      0, 0x3010,      0,      0,          0,         0},
/*     */ {      0,      0,      0, 0x3011,      0,      0,          0,         0},
/*     */ { 0x4700,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x4800,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x4900,      0,      0,      0,      0,      0,          0,         0},
/*     */ {   0x2D,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x4B00,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x4C00,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x4D00,      0,      0,      0,      0,      0,          0,         0},
/*     */ {   0x2B,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x4F00,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x5000,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x5100,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x5200,      0,      0,      0,      0,      0,          0,         0},
/*     */ { 0x5300,      0,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0},
/*     */ {      0,      0,      0,      0,      0,      0,          0,         0}
  };

/************************************************************************

Function: int atkbd_init

Description: This function is used to turn on the keyboard

Notes:

02/20/2004 - Approved for quality

************************************************************************/
int atkbd_init() {
  /* Insert the IDT vector for the keyboard handler */
  setVector(&atkbd_isr, mVec+0x1, dPresent + dInt + dDpl0);

  /* Set the LEDS to their defaults */
  setLED();

  /* Clear Keyboard */
  atkbd_scan();

  /* Turn on the keyboard vector */
  irqEnable(0x1);

  /* Print out information on keyboard */
  kprintf("atkbd0 - Address: [0x%X], Keyboard Buffer: [0x%X], Buffer Size [%i]\n",&atkbd_isr,&stdinBuffer,512);

  /* Return so we know everything went well */
  return(0x0);
  }

/*
 * 2-23-2004 mji  I think the pusha/popa should be pushal/popal
 */

asm(
  ".globl atkbd_isr       \n"
  "atkbd_isr:             \n"
  "  pusha                \n" /* Save all registers           */
  "  push %ss             \n"
  "  push %ds             \n"
  "  push %es             \n"
  "  push %fs             \n"
  "  push %gs             \n"  
  "  call keyboardHandler \n"
  "  mov $0x20,%dx        \n"
  "  mov $0x20,%ax        \n"
  "  outb %al,%dx         \n"
  "  pop %gs              \n"
  "  pop %fs              \n" 
  "  pop %es              \n"
  "  pop %ds              \n"
  "  pop %ss              \n"
  "  popa                 \n"
  "  iret                 \n" /* Exit interrupt                           */
  );

static int atkbd_scan() {
  int code = 0x0;
  int val  = 0x0;
  
  code = inportByte(0x60);
  val  = inportByte(0x61);
  
  outportByte(0x61,val | 0x80);
  outportByte(0x61,val);
  
  return(code);
  }

void keyboardHandler() {
  int key = 0x0;

  if (!spinTryLock(&atkbdSpinLock))
        return;

  key = atkbd_scan();
  
  if (key > 255) 
    return;

  /* Control Key */
  if (key == 0x1D && !(controlKeys & controlKey)) {
    controlKeys |= controlKey;
    }
  if (key == 0x80 + 0x1D) {
    controlKeys &= (0xFF - controlKey);
    }
  /* ALT Key */
  if (key == 0x38 && !(controlKeys & altKey)) {
    controlKeys |= altKey;
    }
  if (key == 0x80 + 0x38) {
    controlKeys &= (0xFF - altKey);
    }
  /* Shift Key */
  if ((key == 0x2A || key == 0x36) && !(controlKeys & shiftKey)) {
    controlKeys |= shiftKey;
    }
  if ((key == 0x80 + 0x2A) || (key == 0x80 + 0x36)) {
    controlKeys &= (0xFF - shiftKey);
    }
  /* Caps Lock */
  if (key == 0x3A) {
    ledStatus ^= ledCapslock;
    setLED();
    }
  /* Num Lock */
  if (key == 0x45) {
    ledStatus ^= ledNumlock;
    setLED();
    }
  /* Scroll Lock */
  if (key == 0x46) {
    ledStatus ^= ledScrolllock;
    setLED();
    }
  /* Pick Which Key Map */
  if (controlKeys == 0) { keyMap = 0; }
  else if (controlKeys == 1) { keyMap = 1; }
  else if (controlKeys == 2) { keyMap = 2; }
  else if (controlKeys == 4) { keyMap = 3; }
  /* If Key Is Not Null Add It To Handler */
  if (((uInt)(keyboardMap[key][keyMap]) > 0) && ((uInt32)(keyboardMap[key][keyMap]) < 0xFF)) {
    switch ((uInt32)keyboardMap[key][keyMap]) {
      case 8:
        backSpace();
	if (tty_foreground == 0x0) {
          stdinBuffer[stdinSize] = keyboardMap[key][keyMap];
          stdinSize++;
	  }
	else {
	  tty_foreground->stdin[tty_foreground->stdinSize] = keyboardMap[key][keyMap];
	  tty_foreground->stdinSize++;
	  }
        break;
      case 0x3:
        //if (tty_foreground != 0x0)
        //  endTask(tty_foreground->owner);
        K_PANIC("CTRL-C pressed\n");
        kprintf("FreePages: [0x%X]\n",systemVitals->freePages);
        break;
      case 0x9:
        kprintf("REBOOTING");
        while(inportByte(0x64) & 0x02);
        outportByte(0x64, 0xFE);
        break;
      default:
        if (tty_foreground == 0x0) {
          stdinBuffer[stdinSize] = keyboardMap[key][keyMap];
          stdinSize++;
	  }
	else {
	  tty_foreground->stdin[tty_foreground->stdinSize] = keyboardMap[key][keyMap];
	  tty_foreground->stdinSize++;
	  }
        break;
      }
    }
  else {
    switch ((keyboardMap[key][keyMap] >> 8)) {
      case 0x30:
        tty_change(keyboardMap[key][keyMap] & 0xFF);
        //kprintf("Changing Consoles[0x%X:0x%X]\n",_current->id,_current);
        break;
      default:
        break;
      }
    }

  /* Return */
  spinUnlock(&atkbdSpinLock);
  return;
  }

void setLED() {
  outportByte(0x60, 0xED);
  while(inportByte(0x64) & 2);
  outportByte(0x60, ledStatus);
  while(inportByte(0x64) & 2);
  }

/* Temp */
unsigned char getch() {
  uInt8   retKey   = 0x0;
  uInt32  i        = 0x0;

  /*
  if ((stdinSize <= 0) && (tty_foreground == 0x0)) {
    sched_yield();
    }
  if ((tty_foreground != 0x0) && (tty_foreground->stdinSize <= 0x0)) {
    sched_yield();
    }
  */

  /*
  if (!spinTryLock(&atkbdSpinLock))
        return(0x0);
*/

  if (tty_foreground == 0x0) {
    if (stdinSize == 0x0) {
    //  spinUnlock(&atkbdSpinLock);
      return(0x0);
      }
  
    retKey = stdinBuffer[0];
    stdinSize--;

    for (i=0x0;i<stdinSize;i++) {
      stdinBuffer[i] = stdinBuffer[i+0x1];
      }
    }
  else {
    if (tty_foreground->stdinSize == 0x0) {
   //   spinUnlock(&atkbdSpinLock);
      return(0x0);
      }
  
    retKey = tty_foreground->stdin[0];
    tty_foreground->stdinSize--;

    for (i=0x0;i<tty_foreground->stdinSize;i++) {
      tty_foreground->stdin[i] = tty_foreground->stdin[i+0x1];
      }
    }
  //spinUnlock(&atkbdSpinLock);
  return(retKey);
  }

/***

 $Log$
 Revision 1.1.1.1  2007/01/17 03:31:51  reddawg
 UbixOS

 Revision 1.5  2006/12/05 17:01:15  reddawg
 Modified kpanic

 Revision 1.4  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.3  2006/12/01 05:12:35  reddawg
 We're almost there... :)

 Revision 1.2  2006/10/19 17:52:17  reddawg
 Working On Userland

 Revision 1.1.1.1  2006/06/01 12:46:12  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:01  reddawg
 no message

 Revision 1.29  2004/09/11 21:38:00  reddawg
 Fixed a few problems

 Revision 1.28  2004/09/08 23:19:58  reddawg
 hmm

 Revision 1.27  2004/09/07 21:54:38  reddawg
 ok reverted back to old scheduling for now....

 Revision 1.26  2004/09/06 22:18:52  reddawg
 ok bed time

 Revision 1.25  2004/09/06 22:11:29  reddawg
 tty: now each tty has a stdin....

 Revision 1.24  2004/09/06 15:13:25  reddawg
 Last commit before FreeBSD 6.0

 Revision 1.23  2004/08/21 23:47:50  reddawg
 *** empty log message ***

 Revision 1.22  2004/08/09 12:58:05  reddawg
 let me know when you got the surce

 Revision 1.21  2004/08/06 22:32:16  reddawg
 Ubix Works Again

 Revision 1.19  2004/08/03 18:31:19  reddawg
 virtual terms

 Revision 1.18  2004/07/29 21:32:16  reddawg
 My quick lunchs breaks worth of updates....

 Revision 1.17  2004/07/28 18:45:39  reddawg
 movement of files

 Revision 1.16  2004/07/28 17:07:25  reddawg
 MPI: moved the syscalls

 Revision 1.15  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.14  2004/07/25 05:32:58  reddawg
 fixed

 Revision 1.13  2004/07/25 05:24:39  reddawg
 atkbd: removed sti... does it still miss keys

 Revision 1.12  2004/07/24 20:00:51  reddawg
 Lots of changes to the vmm subsystem.... Page faults have been adjust to now be blocking on a per thread basis not system wide. This has resulted in no more deadlocks.. also the addition of per thread locking has removed segfaults as a result of COW in which two tasks fault the same COW page and try to modify it.

 Revision 1.11  2004/07/24 15:12:56  reddawg
 Now I'm current

 Revision 1.10  2004/07/23 17:49:58  reddawg
 atkbd: adjust the timing issue on the driver hopefully it will work fine now

 Revision 1.9  2004/07/23 17:37:35  reddawg
 Fix

 Revision 1.8  2004/07/23 09:10:06  reddawg
 ubixfs: cleaned up some functions played with the caching a bit
 vfs:    renamed a bunch of functions
 cleaned up a few misc bugs

 Revision 1.7  2004/07/22 20:53:07  reddawg
 atkbd: fixed problem

 Revision 1.6  2004/07/09 13:34:51  reddawg
 keyboard: keyboardInit to atkbd_init
 Adjusted initialization routines

 Revision 1.5  2004/06/17 14:49:14  reddawg
 atkbd: converted some variables to static

 Revision 1.4  2004/06/04 10:19:42  reddawg
 notes: we compile again, thank g-d anyways i was about to cry

 Revision 1.3  2004/05/19 04:07:42  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.2  2004/05/10 02:23:24  reddawg
 Minor Changes To Source Code To Prepare It For Open Source Release

 Revision 1.1.1.1  2004/04/15 12:07:09  reddawg
 UbixOS v1.0

 Revision 1.19  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/

