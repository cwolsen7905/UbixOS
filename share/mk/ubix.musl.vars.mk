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
#   MUSL_CRT1           C-runtime startup object (PIE Scrt1.o vs fixed crt1.o)
#   MUSL_PIE_CFLAGS     position-independent codegen flags
#   MUSL_PIE_LDFLAGS    position-independent link flags
#
# aarch64 builds PIE (ET_DYN): user VA must be >= 4 GB (the low 4 GB is the kernel
# identity map), so the kernel's dynamic loader places the image at a high base —
# a fixed-address ET_EXEC at musl's default 0x400000 cannot be mapped as user.
# i386 keeps the fixed-address ET_EXEC layout it has always used.
.if ${_ARCH} == "aarch64"
MUSL_LDEMULATION   ?= aarch64elf
MUSL_LDSO          ?= /lib/ld-musl-aarch64.so.1
MUSL_ARCH_INC      ?= -I${MUSL_SRC}/arch/aarch64
MUSL_LIBGCC_COMPAT ?=
MUSL_CRT1          ?= ${MUSL_LIB}/Scrt1.o
MUSL_PIE_CFLAGS    ?= -fPIC
MUSL_PIE_LDFLAGS   ?= -pie
.elif ${_ARCH} == "x86_64"
# x86_64 mirrors aarch64: PIE (ET_DYN) loaded high, since a fixed ET_EXEC at
# musl's default 0x400000 falls inside the kernel low-1 GB identity window and
# cannot be mapped as user.  Real libgcc (no -m32 shim).
MUSL_LDEMULATION   ?= elf_x86_64
MUSL_LDSO          ?= /lib/ld-musl-x86_64.so.1
MUSL_ARCH_INC      ?= -I${MUSL_SRC}/arch/x86_64
MUSL_LIBGCC_COMPAT ?=
MUSL_CRT1          ?= ${MUSL_LIB}/Scrt1.o
MUSL_PIE_CFLAGS    ?= -fPIC
MUSL_PIE_LDFLAGS   ?= -pie
.else
MUSL_LDEMULATION   ?= elf_i386
MUSL_LDSO          ?= /lib/ld-musl-i386.so.1
MUSL_ARCH_INC      ?= -I${MUSL_SRC}/arch/i386
MUSL_LIBGCC_COMPAT ?= ${OBJ_DIR}/lib/libgcc32.a
MUSL_CRT1          ?= ${MUSL_LIB}/crt1.o
MUSL_PIE_CFLAGS    ?=
MUSL_PIE_LDFLAGS   ?=
.endif

MUSL_BASE_INC = -I${MUSL_SRC}/include \
                -I${MUSL_OBJ}/obj/include \
                ${MUSL_ARCH_INC} \
                -I${MUSL_SRC}/arch/generic

LIBCXX_INC = -I${LIBCXX_SRC}/include \
             -I${SRCTOP}/contrib/libcxxabi/include

.endif # _UBIX_MUSL_VARS_MK
