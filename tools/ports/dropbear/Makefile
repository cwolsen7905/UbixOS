# tools/ports/dropbear/Makefile — port recipe for Dropbear as uBixOS's SSH server.
#
# Dropbear is a small SSH-2 server/client written for low-resource embedded
# systems, **MIT-licensed** (BSD-compatible) with self-contained crypto
# (bundled libtomcrypt + libtommath — no OpenSSL).  That fits uBixOS's
# console-first, IoT-class identity, so it's our sshd, started by initd.
# Pulled in as a PORT per docs/design/third-party-ports-plan.md — a sibling to
# the bmake/sh ports, NOT vendored into contrib/.
#
# Build:  bmake -C tools/ports/dropbear              (default TARGET=aarch64)
#         bmake -C tools/ports/dropbear TARGET=x86_64
#
# Produces build/${TARGET}/usr/sbin/{dropbear,dropbearkey} (sshd homes in
# /usr/sbin — see docs/design/filesystem-hierarchy-plan.md).
#
# NOTE: the two arches must build SERIALLY — the port worktree
# (build/ports/dropbear-*) is shared, not arch-homed, so a concurrent
# aarch64 + x86_64 build corrupts the shared object files.

PORT_NAME     = dropbear
PORT_VERSION  = 2025.88
PORT_URL      = https://matt.ucc.asn.au/dropbear/releases/dropbear-${PORT_VERSION}.tar.bz2
PORT_DISTFILE = dropbear-${PORT_VERSION}.tar.bz2
PORT_SHA256   = 783f50ea27b17c16da89578fafdb6decfa44bb8f6590e5698a4e4d3672dc53d4

# ── Cross-build settings (uBixOS musl world toolchain) ───────────────────────
TARGET      ?= aarch64
.if ${TARGET} == "x86_64"
CROSS_PREFIX = x86_64-elf-
MUSL_ARCH    = x86_64
.else
CROSS_PREFIX = aarch64-elf-
MUSL_ARCH    = aarch64
.endif

CC       = ${CROSS_PREFIX}gcc
AR       = ${CROSS_PREFIX}ar
BUILD    = ${SRCTOP}/build/${TARGET}
MUSL     = ${SRCTOP}/contrib/musl
MUSL_INC = -I${MUSL}/include -I${BUILD}/obj/musl/obj/include \
           -I${MUSL}/arch/${MUSL_ARCH} -I${MUSL}/arch/generic
SHIM_INC = -I${.CURDIR}/../bmake/shim   # reuse the sys/cdefs.h shim

PORT_CFLAGS = -nostdlib -nostdinc -fno-builtin -fPIC -O2 ${SHIM_INC} ${MUSL_INC}

do-build:
	@echo "==> cross-build Dropbear (sshd) for ${TARGET}"
	@WRKSRC="${WRKSRC}" CC="${CC}" AR="${AR}" PORT_CFLAGS="${PORT_CFLAGS}" \
	    BUILD="${BUILD}" MUSL_ARCH="${MUSL_ARCH}" PORTDIR="${.CURDIR}" \
	    sh ${.CURDIR}/build.sh

.include "${.CURDIR}/../../../mk/ports.mk"
