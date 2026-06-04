# bin/ssltest/Makefile — BearSSL smoke test for UbixOS/i386.
# Dynamically links libbearssl.so (resolved from /lib at runtime via ld-musl).

include ../../Makefile.incl
include ../Makefile.incl

BINARY = ssltest
OBJS   = main.o

# BearSSL public API headers.
EXTRA_CFLAGS = -I${SRCTOP}/contrib/bearssl/inc

# Link the BearSSL shared object by path; the linker records its soname
# (libbearssl.so) as a NEEDED entry, resolved from /lib at load time.
EXTRA_LIBS = ${OBJ_DIR}/lib/libbearssl.so

.include "${UBIX_MK}/ubix.musl.prog.mk"
