# (C) 2002-2026 The UbixOS Project
# ubix.musl.vars.mk — Shared musl/libcxx path variables.
#
# Included by all ubix.musl.*.mk files.  Provides:
#   MUSL_SRC       source tree root
#   MUSL_OBJ       build output root (from OBJ_DIR)
#   MUSL_LIB       compiled lib dir (crt1.o, libc.a, libc.so)
#   MUSL_BASE_INC  -I flags for musl headers (no ${SRCTOP}/include)
#   LIBCXX_SRC     LLVM libc++ source root
#   LIBCXX_INC     -I flags for libc++ and libcxxabi headers

.if !defined(_UBIX_MUSL_VARS_MK)
_UBIX_MUSL_VARS_MK = 1

MUSL_SRC   ?= ${SRCTOP}/contrib/musl
MUSL_OBJ   ?= ${OBJ_DIR}/obj/musl
MUSL_LIB   ?= ${MUSL_OBJ}/lib
LIBCXX_SRC ?= ${SRCTOP}/contrib/libcxx

# Architecture (passed down from the top-level Makefile via WMAKE).  Default to
# i386 so a stray invocation without _ARCH still behaves as before.
_ARCH ?= i386

# Per-arch link/ABI knobs derived from _ARCH.  World .mk files reference these
# instead of hardcoding i386, so adding an arch is one edit here:
#   MUSL_LDEMULATION    ld -m emulation
#   MUSL_LDSO           runtime dynamic-linker path on the target
#   MUSL_ARCH_INC       musl per-arch header include
#   MUSL_LIBGCC_COMPAT  hand-built 32-bit libgcc shim (i386 only; empty elsewhere)
.if ${_ARCH} == "aarch64"
MUSL_LDEMULATION   ?= aarch64elf
MUSL_LDSO          ?= /lib/ld-musl-aarch64.so.1
MUSL_ARCH_INC      ?= -I${MUSL_SRC}/arch/aarch64
MUSL_LIBGCC_COMPAT ?=
.else
MUSL_LDEMULATION   ?= elf_i386
MUSL_LDSO          ?= /lib/ld-musl-i386.so.1
MUSL_ARCH_INC      ?= -I${MUSL_SRC}/arch/i386
MUSL_LIBGCC_COMPAT ?= ${OBJ_DIR}/lib/libgcc32.a
.endif

MUSL_BASE_INC = -I${MUSL_SRC}/include \
                -I${MUSL_OBJ}/obj/include \
                ${MUSL_ARCH_INC} \
                -I${MUSL_SRC}/arch/generic

LIBCXX_INC = -I${LIBCXX_SRC}/include \
             -I${SRCTOP}/contrib/libcxxabi/include

.endif # _UBIX_MUSL_VARS_MK
