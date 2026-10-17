# tests/pkgdev/Makefile — host-side tests of the on-device pkg client (W-071).
#
# `bmake host` builds pkg (usr.sbin/pkg), the mkpkg helper and run.sh's
# fixtures natively with the host cc, from the vendored zlib and BearSSL
# sources and lib/libpkg, then runs run.sh.  No world build needed.  mkpkg is
# also how the QEMU test package is made (see docs/design/packages-plan.md).

include ../../Makefile.incl

HOST_OUT = ${SRCTOP}/build/host/pkgdev
LIBSRC   = $$(ls ${SRCTOP}/contrib/bearssl/src/*/*.c | grep -v -e /ssl/ -e /x509/ -e /rsa/) \
           $$(ls ${SRCTOP}/contrib/zlib/*.c | grep -v -e /gz -e infback) \
           ${SRCTOP}/lib/libpkg/*.c
HOSTINC  = -I${HOST_OUT}/include -I${SRCTOP}/contrib/bearssl/inc -I${SRCTOP}/contrib/bearssl/src \
           -I${SRCTOP}/contrib/zlib -I${SRCTOP}/lib/libpkg

all:
	@echo "pkgdev: host-side only; run 'bmake host'"

host:
	@mkdir -p ${HOST_OUT}/include/ubix ${HOST_OUT}/obj
	@ln -sf ${SRCTOP}/include/ubix/pkg.h ${HOST_OUT}/include/ubix/pkg.h
	cd ${HOST_OUT}/obj && cc -O1 -w ${HOSTINC} -c ${LIBSRC} && ar rcs ../libhost.a *.o
	cc -O1 -g -Wall ${HOSTINC} -I${SRCTOP}/usr.sbin/pkg -o ${HOST_OUT}/pkg \
		${SRCTOP}/usr.sbin/pkg/*.c ${HOST_OUT}/libhost.a
	cc -O1 -g -Wall ${HOSTINC} -o ${HOST_OUT}/mkpkg ${.CURDIR}/mkpkg.c ${HOST_OUT}/libhost.a
	sh ${.CURDIR}/run.sh ${HOST_OUT}/pkg ${HOST_OUT}/mkpkg ${HOST_OUT}/work

clean:
	rm -rf ${HOST_OUT}

.PHONY: all host clean
