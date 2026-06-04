# (C) 2002-2026 The UbixOS Project
# ubix.target.i386.mk — i386 (IA-32) target definition.
#
# Owns the ISA/ABI facts about producing an i386 kernel binary, kept separate
# from host detection (ubix.platform.mk, which only answers "what toolchain
# does this build host have").  A sibling ubix.target.<arch>.mk (x86_64,
# aarch64, …) provides the same variables for another target, so adding an
# architecture is "write one target file + an arch dir + a linker script"
# rather than editing the shared CFLAGS.
#
# Included by Makefile.incl after ubix.platform.mk.
#
# Provides:
#   KERN_TARGET_CFLAGS  ISA/ABI compile flags folded into the kernel CFLAGS
#   LDSCRIPT_SUFFIX     linker-script suffix (sys/compile/ldscript.<suffix>)

.if !defined(_UBIX_TARGET_MK)
_UBIX_TARGET_MK = 1

# -m32 (CROSS_M32) is host-dependent — empty on a native i386 host, -m32 when
# the host toolchain is 64-bit — so it is supplied by ubix.platform.mk and
# folded in here.  -mno-sse/-sse2/-mmx/-3dnow: the kernel never sets
# CR4.OSFXSR, so the compiler must never emit XMM/MMX instructions (they would
# #UD).  These are the i386-specific facts that used to live inline in the
# shared kernel CFLAGS.
KERN_TARGET_CFLAGS ?= ${CROSS_M32} -mno-sse -mno-sse2 -mno-mmx -mno-3dnow

# Linker script: sys/compile/ldscript.i386.  Kept as a separate knob from
# _ARCH so a target could share another target's link layout if needed.
LDSCRIPT_SUFFIX    ?= i386

.export KERN_TARGET_CFLAGS LDSCRIPT_SUFFIX

.endif # _UBIX_TARGET_MK
