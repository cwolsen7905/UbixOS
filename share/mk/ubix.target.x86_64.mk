# (C) 2002-2026 The UbixOS Project
# ubix.target.x86_64.mk — x86-64 (AMD64, long mode) target definition.
#
# Sibling of ubix.target.i386.mk / ubix.target.aarch64.mk.  x86_64 shares the
# i386 toolchain (x86_64-elf-) but builds NATIVE 64-bit (no -m32) — long mode,
# LP64.  The 3.0 forward target that replaces i386; reuses most of the i386 MD
# code widened to 64-bit (drivers, PCI/ISA) with a new long-mode boot, 4-level
# paging, SYSCALL/SYSRET and 64-bit interrupt frames.
#
# Provides:
#   CROSS_PREFIX        x86_64 cross-compiler prefix (same toolchain as i386)
#   CROSS_M32           empty (native 64-bit — the whole point vs i386's -m32)
#   LIBGCC              static libgcc.a for the x86_64 toolchain
#   KERN_TARGET_CFLAGS  ISA/ABI compile flags folded into the kernel CFLAGS
#   LDSCRIPT_SUFFIX     linker-script suffix (sys/compile/ldscript.x86_64)

.if !defined(_UBIX_TARGET_MK)
_UBIX_TARGET_MK = 1

# Same cross toolchain as i386, but no -m32 — build for long mode.
CROSS_PREFIX = x86_64-elf-
CROSS_M32    =

LIBGCC != ${CROSS_PREFIX}gcc -print-libgcc-file-name 2>/dev/null

# Kernel ISA/ABI flags.  Like i386 the kernel touches no FP/vector unit (it would
# require saving XMM/x87 in the trap path), so disable SSE/MMX.  -mno-red-zone is
# mandatory in a kernel: interrupts clobber the 128-byte red zone the SysV AMD64
# ABI lets leaf functions use below %rsp.  -mcmodel=large makes no assumption
# about where the kernel is linked (bring-up loads low; a higher-half move later
# stays valid).
KERN_TARGET_CFLAGS ?= -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -mno-red-zone -mcmodel=large

# Linker script: sys/compile/ldscript.x86_64.
LDSCRIPT_SUFFIX    ?= x86_64

.export CROSS_PREFIX CROSS_M32 LIBGCC KERN_TARGET_CFLAGS LDSCRIPT_SUFFIX

.endif # _UBIX_TARGET_MK
