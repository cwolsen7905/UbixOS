# tools/ports/bmake/Makefile — port recipe for NetBSD portable bmake.
#
# bmake (Simon Gerraty's portable distribution of the NetBSD make) is the build
# system uBixOS itself uses; self-hosting needs it running on-device.  Pulled in
# as a PORT (pinned fetch + SHA + patches) per docs/design/third-party-ports-plan.md,
# not vendored into contrib/ (and not GNU make — bmake is BSD-licensed).
#
# License: 2-/3-clause BSD + Berkeley/Adam de Boor/NetBSD Foundation copyrights
# (LICENSE in the tarball).  Compatible with uBixOS's BSD license.  The bundled
# GNU `configure` is GPL but is a host build-tool, not part of the shipped binary.
#
# Build:   bmake -C tools/ports/bmake            (default TARGET=aarch64)
#          bmake -C tools/ports/bmake TARGET=x86_64

PORT_NAME    = bmake
PORT_VERSION = 20240212
PORT_URL     = https://www.crufty.net/ftp/pub/sjg/bmake-${PORT_VERSION}.tar.gz
PORT_SHA256  = 971d5a364035349e984d82c2a48d546a0c73e52f3b8f2aa29a3be3d2408ffaa8

# ── Cross-build settings (uBixOS musl world toolchain) ───────────────────────
TARGET      ?= aarch64
.if ${TARGET} == "x86_64"
CROSS_PREFIX = x86_64-elf-
MUSL_ARCH    = x86_64
.else
CROSS_PREFIX = aarch64-elf-
MUSL_ARCH    = aarch64
.endif

CC      = ${CROSS_PREFIX}gcc
BUILD   = ${SRCTOP}/build/${TARGET}
MUSL    = ${SRCTOP}/contrib/musl
MUSL_INC = -I${MUSL}/include -I${BUILD}/obj/musl/obj/include \
           -I${MUSL}/arch/${MUSL_ARCH} -I${MUSL}/arch/generic
# A sys/cdefs.h shim (musl ships none) for the BSD-ism __BEGIN_DECLS/__P etc.
SHIM_INC = -I${.CURDIR}/shim

PORT_CFLAGS = -nostdlib -nostdinc -fno-builtin -fPIC -O -DMAKE_NATIVE \
              ${SHIM_INC} ${MUSL_INC}

# Cross-build: bmake's source set is selected by its autoconf config.h (the
# bundled compat shims — getopt/realpath/setenv/strlcpy/sigaction/… — compile
# only where the libc lacks them, which for musl is a curated subset).  So the
# build is gated on a musl-targeted config.h.  Generating that (autoconf can't
# run target test-programs under -nostdlib, so it's authored from config.h.in +
# musl's feature set) + the per-file compile iteration is the remaining port
# work; see README.md.  fetch / extract / patch (the framework) are complete.
do-build:
	@echo "==> cross-build bmake for ${TARGET}"
	@cp ${.CURDIR}/config.h ${WRKSRC}/config.h
	@WRKSRC="${WRKSRC}" CC="${CC}" PORT_CFLAGS="${PORT_CFLAGS}" \
	    BUILD="${BUILD}" MUSL_ARCH="${MUSL_ARCH}" \
	    sh ${.CURDIR}/build.sh

.include "${.CURDIR}/../../../share/mk/ports.mk"
