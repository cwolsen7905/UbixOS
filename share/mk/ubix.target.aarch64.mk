# (C) 2002-2026 The UbixOS Project
# ubix.target.aarch64.mk — AArch64 (ARMv8-A) target definition.
#
# Sibling of ubix.target.i386.mk.  Owns everything arch-specific about producing
# an AArch64 kernel: the cross toolchain, ISA/ABI compile flags, and the
# linker-script suffix.  Included by Makefile.incl AFTER ubix.platform.mk, so it
# overrides the host-derived x86 cross toolchain (x86_64-elf- / -m32) that
# platform.mk selects by default — that default is correct only for i386/x86_64
# targets, never for aarch64.
#
# Provides:
#   CROSS_PREFIX        aarch64 cross-compiler prefix
#   CROSS_M32           empty (no -m32 on a 64-bit-only ISA)
#   LIBGCC              static libgcc.a for the aarch64 toolchain
#   KERN_TARGET_CFLAGS  ISA/ABI compile flags folded into the kernel CFLAGS
#   LDSCRIPT_SUFFIX     linker-script suffix (sys/compile/ldscript.aarch64)

.if !defined(_UBIX_TARGET_MK)
_UBIX_TARGET_MK = 1

# Cross toolchain.  Hard assignment (not ?=) so it wins over the x86 default
# platform.mk already set.  macOS: `brew install aarch64-elf-gcc
# aarch64-elf-binutils`.  Override CROSS_PREFIX on the command line for a
# different toolchain (e.g. aarch64-linux-gnu-).
CROSS_PREFIX = aarch64-elf-
CROSS_M32    =

# Recompute libgcc for the aarch64 compiler (platform.mk computed it for the
# x86 prefix, which is wrong here).
LIBGCC != ${CROSS_PREFIX}gcc -print-libgcc-file-name 2>/dev/null

# ARMv8-A.  -mgeneral-regs-only: the kernel must not touch the FP/NEON (SIMD)
# register file (it would require enabling/saving FPSIMD state in the trap path
# and context switch — the AArch64 analogue of i386's "no XMM in the kernel").
KERN_TARGET_CFLAGS ?= -march=armv8-a -mgeneral-regs-only

# Linker script: sys/compile/ldscript.aarch64 (QEMU `virt` load address).
LDSCRIPT_SUFFIX    ?= aarch64

.export CROSS_PREFIX CROSS_M32 LIBGCC KERN_TARGET_CFLAGS LDSCRIPT_SUFFIX

.endif # _UBIX_TARGET_MK
