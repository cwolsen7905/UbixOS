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

 $Id: cpu.h 207 2016-01-23 19:43:49Z reddawg $

 *****************************************************************************************/

#ifndef _CPU_H_
#define _CPU_H_

#include <sys/types.h>

/*
 * 386 processor status longword.
 */
#define PSL_C           0x00000001      /* carry bit */
#define PSL_PF          0x00000004      /* parity bit */
#define PSL_AF          0x00000010      /* bcd carry bit */
#define PSL_Z           0x00000040      /* zero bit */
#define PSL_N           0x00000080      /* negative bit */
#define PSL_T           0x00000100      /* trace enable bit */
#define PSL_I           0x00000200      /* interrupt enable bit */
#define PSL_D           0x00000400      /* string instruction direction bit */
#define PSL_V           0x00000800      /* overflow bit */
#define PSL_IOPL        0x00003000      /* i/o privilege level */
#define PSL_NT          0x00004000      /* nested task bit */
#define PSL_RF          0x00010000      /* resume flag bit */
#define PSL_VM          0x00020000      /* virtual 8086 mode bit */
#define PSL_AC          0x00040000      /* alignment checking */
#define PSL_VIF         0x00080000      /* virtual interrupt enable */
#define PSL_VIP         0x00100000      /* virtual interrupt pending */
#define PSL_ID          0x00200000      /* identification bit */

/* Read Contents Of CR0 */
static __inline u_int rcr0( void ) {
  u_int data;
  __asm __volatile("movl %%cr0,%0" : "=r" (data));
  return (data);
}

/* Write To CR0 */
static __inline void load_cr0( u_int data ) {
  __asm __volatile("movl %0,%%cr0" : : "r" (data));
}

static __inline u_int rcr2( void ) {
  u_int data;

  __asm __volatile("movl %%cr2,%0" : "=r" (data));
  return (data);
}

static __inline u_int rcr3( void ) {
  u_int data;

  __asm __volatile("movl %%cr3,%0" : "=r" (data));
  return (data);
}

static __inline void load_cr3( u_int data ) {
  __asm __volatile("movl %0,%%cr3" : : "r" (data) : "memory");
}

static __inline u_int rcr4( void ) {
  u_int data;

  __asm __volatile("movl %%cr4,%0" : "=r" (data));
  return (data);
}

static __inline void load_cr4( u_int data ) {
  __asm __volatile("movl %0,%%cr4" : : "r" (data));
}

static __inline void invlpg( u_int addr ) {
  __asm __volatile("invlpg %0" : : "m" (*(char *)addr) : "memory");
}

#endif

/***
 END
 ***/
