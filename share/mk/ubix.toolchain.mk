# (C) 2002-2026 The UbixOS Project
#
# ubix.toolchain.mk — the one place that names the compiler and binutils.
#
# Cross builds (macOS / Linux / FreeBSD hosts) use the ${CROSS_PREFIX} GNU
# toolchain as before.  A build running *on* uBixOS (uname -s == UBIX) uses the
# native tools: cc / c++ (clang, or tcc for C), ar / ranlib / nm / objcopy
# (LLVM), and ld.lld.  Override any of them on the command line as usual.
#
# Included by Makefile.incl after the platform and target files.

.if !defined(_UBIX_TOOLCHAIN_MK)
_UBIX_TOOLCHAIN_MK = 1

.if defined(UBIX_NATIVE)
UBIX_CC      ?= cc
UBIX_CXX     ?= c++
UBIX_AS      ?= cc
UBIX_AR      ?= ar
UBIX_LD      ?= ld.lld
UBIX_NM      ?= nm
UBIX_OBJCOPY ?= objcopy
UBIX_RANLIB  ?= ranlib
.else
UBIX_CC      ?= ${CROSS_PREFIX}gcc
UBIX_CXX     ?= ${CROSS_PREFIX}g++
UBIX_AS      ?= ${CROSS_PREFIX}as
UBIX_AR      ?= ${CROSS_PREFIX}ar
UBIX_LD      ?= ${CROSS_PREFIX}ld
UBIX_NM      ?= ${CROSS_PREFIX}nm
UBIX_OBJCOPY ?= ${CROSS_PREFIX}objcopy
UBIX_RANLIB  ?= ${CROSS_PREFIX}ranlib
.endif

.export UBIX_CC UBIX_CXX UBIX_AS UBIX_AR UBIX_LD UBIX_NM UBIX_OBJCOPY UBIX_RANLIB
.endif # _UBIX_TOOLCHAIN_MK
