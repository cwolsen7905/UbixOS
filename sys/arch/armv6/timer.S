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
  incl 4(%ecx)             /* Increment sysTicks our 1000ms counter    */                    
  movl 4(%ecx),%eax        /* Increment our sysUptime by 1S if 1000MS  */
  movl $200,%ebx           /* Have Passed                              */
  xor %edx,%edx           
  div %ebx               
  test %edx,%edx          
  jnz next                
  incl 8(%ecx)            
next:                     
  movl 4(%ecx),%eax        /* Test If quantum Has Passed If So Then    */
  movl 12(%ecx),%ebx       /* We Can CALL sched                        */
  xor %edx,%edx           
  div %ebx                
  test %edx,%edx          
  jnz done                
  call sched              
done:                     
  popa                      /* Restore Registers                        */
  iret                    

/***
 END
 ***/

