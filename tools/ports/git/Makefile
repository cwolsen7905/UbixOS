# tools/ports/git/Makefile — port recipe for Git as uBixOS's version-control system.
#
# Git is the de-facto SCM, GPLv2, with a hand-written (non-autoconf) Makefile
# that cross-compiles cleanly once you (a) stop it autodetecting the *build*
# host (macOS/Darwin) and (b) tell it which POSIX features the *target* libc
# (our musl world) actually has.  We do both by registering uBixOS as a brand
# new platform: `uname_S=UbixOS` selects NO built-in block in git's
# config.mak.uname, and our config.mak then declares musl's capabilities
# explicitly.  See docs/design/third-party-ports-plan.md — a sibling to the
# dropbear/bmake/sh ports, NOT vendored into contrib/.
#
# Build:  bmake -C tools/ports/git                 (default TARGET=aarch64)
#         bmake -C tools/ports/git TARGET=x86_64
#
# M0 scope (docs/design/git-port-plan.md): local-only git — no network
# transport.  Produces build/${TARGET}/usr/bin/git (the multi-call binary).
# Remote-over-ssh (dbclient) and http(s) are later milestones.
#
# NOTE: the two arches must build SERIALLY — the port worktree
# (build/ports/git-*) is shared, not arch-homed, so a concurrent
# aarch64 + x86_64 build corrupts the shared object files.

# do-build is defined below (before the ports.mk include, as the framework
# requires), which would otherwise become the default goal and skip
# fetch/extract/patch from a clean tree.  Pin the default to ports.mk's `all`.
.MAIN: all

PORT_NAME     = git
PORT_VERSION  = 2.39.5
PORT_URL      = https://mirrors.edge.kernel.org/pub/software/scm/git/git-${PORT_VERSION}.tar.gz
PORT_DISTFILE = git-${PORT_VERSION}.tar.gz
PORT_SHA256   = ca0ec03fb2696f552f37135a56a0242fa062bd350cb243dc4a15c86f1cafbc99

# ── Cross-build settings (uBixOS musl world toolchain) ───────────────────────
TARGET      ?= aarch64
.if ${TARGET} == "x86_64"
CROSS_PREFIX = x86_64-elf-
MUSL_ARCH    = x86_64
LDEMUL       = elf_x86_64
.else
CROSS_PREFIX = aarch64-elf-
MUSL_ARCH    = aarch64
LDEMUL       = aarch64elf
.endif

CC       = ${CROSS_PREFIX}gcc
AR       = ${CROSS_PREFIX}ar
STRIP    = ${CROSS_PREFIX}strip
BUILD    = ${SRCTOP}/build/${TARGET}

do-build:
	@echo "==> cross-build Git ${PORT_VERSION} for ${TARGET}"
	@WRKSRC="${WRKSRC}" CC="${CC}" AR="${AR}" LDEMUL="${LDEMUL}" \
	    BUILD="${BUILD}" MUSL_ARCH="${MUSL_ARCH}" PORTDIR="${.CURDIR}" \
	    SRCTOP="${SRCTOP}" \
	    sh ${.CURDIR}/build.sh

do-install:
	@echo "==> stage git -> ${BUILD}/usr/bin/git (stripped)"
	@mkdir -p ${BUILD}/usr/bin
	@cp ${WRKSRC}/git ${BUILD}/usr/bin/git
	@${STRIP} ${BUILD}/usr/bin/git
	@echo "==> staged $$(ls -la ${BUILD}/usr/bin/git | awk '{print $$5}') byte git (stripped)"

.include "${.CURDIR}/../../../share/mk/ports.mk"
