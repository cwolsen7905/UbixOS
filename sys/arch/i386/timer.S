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

.globl timerInt
.text
.code32
timerInt:                 
  pusha                    /* Save all of the registers                */
  mov $0x20,%dx            /* The Following Sends Our EOI To The MPIC  */
  mov $0x20,%ax           
  outb %al,%dx
  movl systemVitals,%ecx   /* Put Location Of System Vitals Into ECX   */
  incl (%ecx)             /* Increment sysTicks our 1000ms counter    */                    
  movl (%ecx),%eax        /* Increment our sysUptime by 1S if 1000MS  */
  movl $200,%ebx           /* Have Passed                              */
  xor %edx,%edx           
  div %ebx               
  test %edx,%edx          
  jnz next                
  incl 4(%ecx)            
next:                     
  movl (%ecx),%eax        /* Test If quantum Has Passed If So Then    */
  movl 8(%ecx),%ebx       /* We Can CALL sched                        */
  test %ebx,%ebx          /* Guard: skip if quantum == 0              */
  jz done
  xor %edx,%edx
  div %ebx
  test %edx,%edx
  jnz done
/*
push %ds
push %es
push %fs
push %gs
mov $0x10,%eax
mov %eax, %ds
mov %eax, %es
mov %eax, %fs
cld
*/
/* push %esp */
  call sched
/* add $0x4,%esp */ /* Remove Stack Pointer From Stack */
  /* AST: if we interrupted ring-3 code, check for pending signals.
   * After pusha the CPU-pushed CS is at [ESP+36]; RPL bits 0-1 == 3 for
   * user mode (CS=0x1B).  Kernel interrupts (CS=0x08) have RPL=0 → skip. */
  testb $3, 36(%esp)
  jz done
  call signal_ast_check
done:
  popa                      /* Restore Registers                        */
  iret
