# bin/httpsget/Makefile — HTTP(S) fetch client over libhttp.
include ../../Makefile.incl
include ../Makefile.incl

BINARY = httpsget
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include

# libbearssl.so listed explicitly so the static linker can resolve libhttp.so's
# transitive TLS symbols; both are NEEDED at load and resolved from /lib.
EXTRA_LIBS = ${OBJ_DIR}/lib/libhttp.so ${OBJ_DIR}/lib/libbearssl.so

.include "${UBIX_MK}/ubix.musl.prog.mk"
