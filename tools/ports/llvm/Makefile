# tools/ports/llvm/Makefile — port recipe for LLVM/Clang/lld (Phase 3 of
# self-hosting: the compiler half).  Cross-builds a static Clang + lld + LLVM
# binutils that RUN on uBixOS, targeting aarch64 + x86_64.
#
# Per docs/design/third-party-ports-plan.md, LLVM comes in as a PORT (pinned
# tarball + SHA + gitignored cache), NOT vendored into contrib/ (~GB of source).
#
# Build (HEAVY — GB of objects, GB-class RAM, hours; do this deliberately):
#   bmake -C tools/ports/llvm            (default TARGET=aarch64)
#   bmake -C tools/ports/llvm TARGET=x86_64
#
# Status: SCAFFOLDING.  The recipe + toolchain file (ubixos.cmake) + build.sh
# establish the Stage-0 cross-build; expect iteration on the C++ runtime, the
# tblgen bootstrap, and memory.  See README.md.

PORT_NAME     = llvm
PORT_VERSION  = 18.1.8
PORT_DISTFILE = llvm-project-${PORT_VERSION}.src.tar.xz
PORT_URL      = https://github.com/llvm/llvm-project/releases/download/llvmorg-${PORT_VERSION}/${PORT_DISTFILE}
# SHA pinned once the distfile is fetched (see README "Pinning the SHA").
PORT_SHA256   = 0b58557a6d32ceee97c8d533a59b9212d87e0fc4d2833924eb6c611247db2f2a

TARGET     ?= aarch64

CMAKE      ?= cmake
NINJA      ?= ninja
JOBS       ?= 4

do-build:
	@echo "==> LLVM ${PORT_VERSION} Stage-0 cross-build for ${TARGET} (HEAVY)"
	@TARGET="${TARGET}" SRCTOP="${SRCTOP}" WRKSRC="${WRKSRC}" \
	    BUILD="${SRCTOP}/build/${TARGET}" CMAKE="${CMAKE}" NINJA="${NINJA}" \
	    JOBS="${JOBS}" PORTDIR="${.CURDIR}" \
	    sh ${.CURDIR}/build.sh

.include "${.CURDIR}/../../../share/mk/ports.mk"
