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

ENTRY(memcmp)
        pushl   %edi
        pushl   %esi
        movl    12(%esp),%edi
        movl    16(%esp),%esi
        cld                             /* set compare direction forward */

        movl    20(%esp),%ecx           /* compare by words */
        shrl    $2,%ecx
        repe
        cmpsl
        jne     L5                      /* do we match so far? */

        movl    20(%esp),%ecx           /* compare remainder by bytes */
        andl    $3,%ecx
        repe
        cmpsb
        jne     L6                      /* do we match? */

        xorl    %eax,%eax               /* we match, return zero        */
        popl    %esi
        popl    %edi
        ret

L5:     movl    $4,%ecx                 /* We know that one of the next */
        subl    %ecx,%edi               /* four pairs of bytes do not   */
        subl    %ecx,%esi               /* match.                       */
        repe
        cmpsb
L6:     movzbl  -1(%edi),%eax           /* Perform unsigned comparison  */
        movzbl  -1(%esi),%edx
        subl    %edx,%eax
        popl    %esi
        popl    %edi
        ret
END(memcmp)
