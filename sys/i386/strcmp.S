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

 #include <sys/i386/asmacros.h>

ENTRY(strcmp)
        movl    0x04(%esp),%eax
        movl    0x08(%esp),%edx
        jmp     L2                      /* Jump into the loop! */

        .align  2,0x90
L1:     incl    %eax
        incl    %edx
L2:     movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        je      L3
        cmpb    %cl,(%edx)
        je      L1
        .align 2, 0x90
L3:     movzbl  (%eax),%eax             /* unsigned comparison */
        movzbl  (%edx),%edx
        subl    %edx,%eax
        ret
END(strcmp)
