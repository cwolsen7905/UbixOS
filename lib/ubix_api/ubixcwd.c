/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

/*
 * ubix_getcwd - UbixOS-native getcwd returning the full VFS path including
 * mountpoint prefix (e.g. "sys:/bin").  Uses native syscall 41 (int $0x81).
 *
 * Implemented as a bare asm block (no C function prolog) so that the kernel's
 * syscall handler can read buf and size directly from [esp+4] and [esp+8].
 * Returns buf on success, NULL on error (eax != 0 from kernel).
 */
#ifdef __aarch64__
/* buf in x0, size in x1.  Native syscall 41 returns 0 on success (in x0); on
 * success hand back the original buf, otherwise NULL. */
asm(".text                          \n"
    ".globl ubix_getcwd             \n"
    ".type  ubix_getcwd, %function  \n"
    "ubix_getcwd:                   \n"
    "  mov x9, x0                   \n" /* save buf */
    "  mov x8, #(41 | 0x8000)       \n"
    "  svc #0                       \n"
    "  cbnz x0, 1f                  \n" /* kernel ret != 0 -> error */
    "  mov x0, x9                   \n" /* return buf */
    "  ret                          \n"
    "1:                             \n"
    "  mov x0, #0                   \n" /* return NULL */
    "  ret                          \n");
#elif defined(__x86_64__)
/* x86-64 SysV ABI: buf in %rdi, size in %rsi; the int $0x81 native path reads
 * the args from those registers (C-ABI order).  Save buf in a call-clobbered
 * register the kernel preserves across the trap, and hand it back on success.
 * (The i386 version reads buf from 4(%esp) — wrong here: that is a 32-bit ESP
 * access that truncates the 64-bit stack pointer and faults.) */
asm(".text                          \n"
    ".globl ubix_getcwd             \n"
    ".type  ubix_getcwd, @function  \n"
    "ubix_getcwd:                   \n"
    "  movq %rdi, %r9               \n" /* save buf */
    "  movl $41, %eax               \n"
    "  int  $0x81                   \n"
    "  testl %eax, %eax             \n"
    "  jnz  ubix_getcwd_err         \n"
    "  movq %r9, %rax               \n" /* return buf */
    "  ret                          \n"
    "ubix_getcwd_err:               \n"
    "  xorl %eax, %eax              \n" /* return NULL */
    "  ret                          \n");
#else
asm(".text                          \n"
    ".globl ubix_getcwd             \n"
    ".type  ubix_getcwd, @function  \n"
    "ubix_getcwd:                   \n"
    "  movl $41, %eax               \n"
    "  int  $0x81                   \n"
    "  testl %eax, %eax             \n"
    "  jnz  ubix_getcwd_err         \n"
    "  movl 4(%esp), %eax           \n" /* return buf */
    "  ret                          \n"
    "ubix_getcwd_err:               \n"
    "  xorl %eax, %eax              \n" /* return NULL */
    "  ret                          \n");
#endif
