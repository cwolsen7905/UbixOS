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

MUSL_BASE_INC = -I${MUSL_SRC}/include \
                -I${MUSL_OBJ}/obj/include \
                -I${MUSL_SRC}/arch/i386 \
                -I${MUSL_SRC}/arch/generic

LIBCXX_INC = -I${LIBCXX_SRC}/include \
             -I${SRCTOP}/contrib/libcxxabi/include

.endif # _UBIX_MUSL_VARS_MK
