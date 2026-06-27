# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY        = authd
OBJS          = main.o

# Password verification via libpw (PBKDF2) over libbearssl; both resolved
# from /lib at load time.
EXTRA_CFLAGS  = -I${SRCTOP}/include
EXTRA_LIBS    = ${OBJ_DIR}/lib/libpw.so ${OBJ_DIR}/lib/libbearssl.so

.include "${UBIX_MK}/ubix.musl.ubix.prog.mk"
