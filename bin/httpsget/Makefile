# bin/httpsget/Makefile — minimal HTTPS GET client (BearSSL over lwIP sockets).
include ../../Makefile.incl
include ../Makefile.incl

BINARY = httpsget
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/contrib/bearssl/inc

EXTRA_LIBS = ${OBJ_DIR}/lib/libbearssl.so

.include "${UBIX_MK}/ubix.musl.prog.mk"
