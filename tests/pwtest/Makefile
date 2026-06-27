# bin/pwtest/Makefile — libpw (PBKDF2-HMAC-SHA256) known-answer test.
# Links libpw.so; libbearssl.so + libc.so come in transitively at load time.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = pwtest
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include

EXTRA_LIBS = ${OBJ_DIR}/lib/libpw.so ${OBJ_DIR}/lib/libbearssl.so

.include "${UBIX_MK}/ubix.musl.prog.mk"
