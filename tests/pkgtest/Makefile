# tests/pkgtest/Makefile — libpkg unit tests (W-069).
#
# `bmake` builds the in-OS binary (libpkg.a + libz + libbearssl).
# `bmake host` builds and runs the same tests natively with the host cc, from
# the vendored zlib and BearSSL sources; no world build needed.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = pkgtest
OBJS   = main.o

EXTRA_CFLAGS = -I${SRCTOP}/include -I${SRCTOP}/contrib/bearssl/inc -I${SRCTOP}/contrib/zlib
EXTRA_LIBS   = ${OBJ_DIR}/lib/libpkg.a -L${OBJ_DIR}/lib -lbearssl -lz

.include "${UBIX_MK}/ubix.musl.prog.mk"

HOST_DIR = ${.CURDIR}/../../lib/libpkg
HOST_OUT = ${.CURDIR}/../../build/host/pkgtest

host:
	@mkdir -p ${HOST_OUT}/include/ubix
	@ln -sf ${SRCTOP}/include/ubix/pkg.h ${HOST_OUT}/include/ubix/pkg.h
	cc -O1 -g -Wall -DPKGTEST_HOST -I${HOST_OUT}/include -I${SRCTOP}/contrib/bearssl/inc \
		-I${SRCTOP}/contrib/bearssl/src -I${SRCTOP}/contrib/zlib -I${HOST_DIR} -o ${HOST_OUT}/pkgtest \
		${.CURDIR}/main.c ${HOST_DIR}/*.c \
		$$(ls ${SRCTOP}/contrib/bearssl/src/*/*.c | grep -v -e /ssl/ -e /x509/ -e /rsa/) \
		$$(ls ${SRCTOP}/contrib/zlib/*.c | grep -v -e /gz -e infback)
	${HOST_OUT}/pkgtest

.PHONY: host
