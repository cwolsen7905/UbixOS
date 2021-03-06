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

 #include <i386/asmacros.h>


ENTRY(memset)
        pushl   %edi
        pushl   %ebx
        movl    12(%esp),%edi
        movzbl  16(%esp),%eax           /* unsigned char, zero extend */
        movl    20(%esp),%ecx
        pushl   %edi                    /* push address of buffer */

        cld                             /* set fill direction forward */

        /*
         * if the string is too short, it's really not worth the overhead
         * of aligning to word boundries, etc.  So we jump to a plain
         * unaligned set.
         */
        cmpl    $0x0f,%ecx
        jle     L1

        movb    %al,%ah                 /* copy char to all bytes in word */
        movl    %eax,%edx
        sall    $16,%eax
        orl     %edx,%eax

        movl    %edi,%edx               /* compute misalignment */
        negl    %edx
        andl    $3,%edx
        movl    %ecx,%ebx
        subl    %edx,%ebx

        movl    %edx,%ecx               /* set until word aligned */
        rep
        stosb

        movl    %ebx,%ecx
        shrl    $2,%ecx                 /* set by words */
        rep
        stosl

        movl    %ebx,%ecx               /* set remainder by bytes */
        andl    $3,%ecx
L1:     rep
        stosb

        popl    %eax                    /* pop address of buffer */
        popl    %ebx
        popl    %edi
        ret
END(memset)
