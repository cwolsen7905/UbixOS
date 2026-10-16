# tools/ubpkg/Makefile — the ubpkg host tool (W-070).
#
# Native host build only (never cross-compiled, never part of the image):
#   bmake -C tools/ubpkg          -> build/host/ubpkg/ubpkg
# libpkg, zlib and BearSSL are compiled from the vendored sources, so no world
# build is needed.  BearSSL's TLS and X.509 code is left out.

TOP      != cd ${.CURDIR}/../.. && pwd
OUT      = ${TOP}/build/host/ubpkg
INCDIR   = ${OUT}/include
BEARSSL  = ${TOP}/contrib/bearssl
ZLIB     = ${TOP}/contrib/zlib
LIBPKG   = ${TOP}/lib/libpkg
HOSTCC  ?= cc
CFLAGS  ?= -O2 -g

UBSRCS   = main.c util.c keygen.c build.c index.c install.c pkgfile.c
UBHDRS   = ubpkg.h
LIBSRCS  != ls ${LIBPKG}/*.c

WFLAGS   = -Wall -Wextra -Wno-unused-parameter
INCLUDES = -I${INCDIR} -I${BEARSSL}/inc -I${BEARSSL}/src -I${ZLIB} -I${LIBPKG} -I${.CURDIR}

all: ${OUT}/ubpkg

# third-party objects change rarely: archive them once
${OUT}/libdeps.a:
	@mkdir -p ${OUT}/deps
	@( ls ${BEARSSL}/src/*/*.c | grep -v -e /ssl/ -e /x509/ -e /rsa/; \
	   ls ${ZLIB}/*.c | grep -v -e /gz -e infback ) | \
	  xargs -n 1 -P 8 sh -c 'o=${OUT}/deps/$$(echo "$$1" | cksum | cut -d" " -f1).o; \
	  ${HOSTCC} ${CFLAGS} -w -I${BEARSSL}/inc -I${BEARSSL}/src -I${ZLIB} -c -o $$o "$$1"' _
	ar rcs ${OUT}/libdeps.a ${OUT}/deps/*.o

${OUT}/ubpkg: ${UBSRCS:S|^|${.CURDIR}/|} ${UBHDRS:S|^|${.CURDIR}/|} ${LIBSRCS} ${OUT}/libdeps.a
	@mkdir -p ${INCDIR}/ubix
	@ln -sf ${TOP}/include/ubix/pkg.h ${INCDIR}/ubix/pkg.h
	${HOSTCC} ${CFLAGS} ${WFLAGS} ${INCLUDES} -o ${.TARGET} \
	  ${UBSRCS:S|^|${.CURDIR}/|} ${LIBSRCS} ${OUT}/libdeps.a

clean:
	rm -rf ${OUT}

.PHONY: all clean
