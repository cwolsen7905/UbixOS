# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY = mp3play
OBJS   = mp3play.o

EXTRA_LIBS = ${OBJ_DIR}/lib/libaudio.a -L${OBJ_DIR}/lib -lubix_api

.include "${UBIX_MK}/ubix.musl.prog.mk"

# minimp3 is a single-header library; append contrib/ to the include path
# that the template already constructed.
MUSL_INC += -I${SRCTOP}/contrib
