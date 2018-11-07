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
.text

// void bzero(void *buf, u_int len)
ENTRY(bzero)
        pushl   %edi
        movl    8(%esp),%edi
        movl    12(%esp),%ecx
        xorl    %eax,%eax
        shrl    $2,%ecx
        cld
        rep
        stosl
        movl    12(%esp),%ecx
        andl    $3,%ecx
        rep
        stosb
        popl    %edi
        ret
END(bzero)

// bcopy(src, dst, cnt)
ENTRY(bcopy)
        pushl   %ebp
        movl    %esp,%ebp
        pushl   %esi
        pushl   %edi
        movl    8(%ebp),%esi
        movl    12(%ebp),%edi
        movl    16(%ebp),%ecx

        movl    %edi,%eax
        subl    %esi,%eax
        cmpl    %ecx,%eax                       /* overlapping && src < dst? */
        jb      1f

        shrl    $2,%ecx                         /* copy by 32-bit words */
        cld                                     /* nope, copy forwards */
        rep
        movsl
        movl    16(%ebp),%ecx
        andl    $3,%ecx                         /* any bytes left? */
        rep
        movsb
        popl    %edi
        popl    %esi
        popl    %ebp
        ret

        ALIGN_TEXT
1:
        addl    %ecx,%edi                       /* copy backwards */
        addl    %ecx,%esi
        decl    %edi
        decl    %esi
        andl    $3,%ecx                         /* any fractional bytes? */
        std
        rep
        movsb
        movl    16(%ebp),%ecx                   /* copy remainder by 32-bit words */
        shrl    $2,%ecx
        subl    $3,%esi
        subl    $3,%edi
        rep
        movsl
        popl    %edi
        popl    %esi
        cld
        popl    %ebp
        ret
END(bcopy)

// void *memcpy(const void *dst, const void * src, size_t length)
ENTRY(memcpy)
        pushl   %edi
        pushl   %esi
        movl    12(%esp),%edi
        movl    16(%esp),%esi
        movl    20(%esp),%ecx
        movl    %edi,%eax
        shrl    $2,%ecx                         /* copy by 32-bit words */
        cld                                     /* nope, copy forwards */
        rep
        movsl
        movl    20(%esp),%ecx
        andl    $3,%ecx                         /* any bytes left? */
        rep
        movsb
        popl    %esi
        popl    %edi
        ret
END(memcpy)
