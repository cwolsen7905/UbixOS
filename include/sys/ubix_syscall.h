/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * UBIX_NATIVE_THUNK — emit a bare userland thunk for a UbixOS-native syscall
 * (the int $0x81 table).  The thunk does no argument marshalling: the kernel
 * reads the arguments where the platform C ABI already put them (i386: the user
 * stack; aarch64: x0..x5), so the body is just "set the syscall number, trap,
 * return".  This centralizes the one machine-dependent line so the per-syscall
 * wrappers in lib/ubix_api stay architecture-agnostic.
 *
 *   UBIX_NATIVE_THUNK(name, num)   defines a global function `name` that
 *                                  invokes native syscall `num`.
 *
 * On aarch64 the number is OR'd with NATIVE_FLAG (0x8000) — the kernel SVC
 * dispatcher uses that bit to select the native table (see arch/aarch64 syscall
 * dispatch); on i386 the int $0x81 vector already selects it.
 */
#ifndef _UBIX_SYSCALL_H
#define _UBIX_SYSCALL_H

#ifdef __aarch64__
#define UBIX_NATIVE_THUNK(name, num)                                                                                   \
	asm(".text\n"                                                                                                  \
	    ".globl " #name "\n"                                                                                       \
	    ".type  " #name ", %function\n" #name ":\n"                                                                \
	    "  mov x8, #((" #num ") | 0x8000)\n"                                                                       \
	    "  svc #0\n"                                                                                               \
	    "  ret\n")
#else
#define UBIX_NATIVE_THUNK(name, num)                                                                                   \
	asm(".text\n"                                                                                                  \
	    ".globl " #name "\n"                                                                                       \
	    ".type  " #name ", @function\n" #name ":\n"                                                                \
	    "  movl $" #num ", %eax\n"                                                                                 \
	    "  int  $0x81\n"                                                                                           \
	    "  ret\n")
#endif

#endif /* _UBIX_SYSCALL_H */
