# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY = mp3play
OBJS   = mp3play.o

EXTRA_LIBS = ${OBJ_DIR}/lib/libaudio.a ${OBJ_DIR}/lib/ubix_api.a

.include "${UBIX_MK}/ubix.musl.prog.mk"

# minimp3 is a single-header library; append contrib/ to the include path
# that the template already constructed.
MUSL_INC += -I${SRCTOP}/contrib

# aarch64: minimp3's NEON path needs <arm_neon.h>, which the freestanding
# world build does not ship — use the portable scalar decoder.
.if ${_ARCH} == "aarch64"
MUSL_INC += -DMINIMP3_NO_SIMD
.endif
