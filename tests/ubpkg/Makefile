# tests/ubpkg/Makefile — ubpkg host tests (W-070).
#
# Host-only: `bmake host` builds tools/ubpkg and the test driver natively and
# runs the round trip plus the failure cases.  Nothing here is built into the
# OS image, and the world build skips this directory.

TOP     != cd ${.CURDIR}/../.. && pwd
OUT     = ${TOP}/build/host/ubpkg
HOSTCC ?= cc

host: ${OUT}/ubpkgtest
	${OUT}/ubpkgtest ${OUT}/ubpkg

${OUT}/ubpkg: .PHONY
	${MAKE} -C ${TOP}/tools/ubpkg

${OUT}/ubpkgtest: ${.CURDIR}/main.c ${OUT}/ubpkg
	${HOSTCC} -O1 -g -Wall -Wextra -I${OUT}/include -I${TOP}/contrib/zlib -I${TOP}/contrib/bearssl/inc -I${TOP}/lib/libpkg -o ${.TARGET} ${.CURDIR}/main.c \
	  ${TOP}/lib/libpkg/*.c ${OUT}/libdeps.a

clean:
	rm -f ${OUT}/ubpkgtest

.PHONY: host clean
