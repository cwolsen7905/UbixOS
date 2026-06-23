# tools/ports/sh/Makefile — port recipe for oksh (portable OpenBSD ksh) as /bin/sh.
#
# uBixOS needs a POSIX /bin/sh so bmake can run recipes and the world can
# self-host — the tree's Makefiles are POSIX sh (the `!=`, for/do, [ ] tests),
# which tcsh (the login shell) can't run.  oksh is the portable OpenBSD ksh:
# small, **public domain** (pdksh lineage), and OpenBSD's /bin/sh.  Pulled in as
# a PORT per docs/design/third-party-ports-plan.md.
#
# Build:  bmake -C tools/ports/sh            (default TARGET=aarch64)
#         bmake -C tools/ports/sh TARGET=x86_64

PORT_NAME    = sh
PORT_VERSION = 7.5
PORT_URL     = https://github.com/ibara/oksh/releases/download/oksh-${PORT_VERSION}/oksh-${PORT_VERSION}.tar.gz
PORT_DISTFILE = oksh-${PORT_VERSION}.tar.gz
PORT_SHA256  = 40b895c3f8e9311bfe2b230e9b3786712550ef488ced33bfd7cd3f89fceeed5d

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
BUILD    = ${SRCTOP}/build/${TARGET}
MUSL     = ${SRCTOP}/contrib/musl
MUSL_INC = -I${MUSL}/include -I${BUILD}/obj/musl/obj/include \
           -I${MUSL}/arch/${MUSL_ARCH} -I${MUSL}/arch/generic
SHIM_INC = -I${.CURDIR}/../bmake/shim   # reuse the sys/cdefs.h shim

PORT_CFLAGS = -nostdlib -nostdinc -fno-builtin -fPIC -O ${SHIM_INC} ${MUSL_INC}

do-build:
	@echo "==> cross-build oksh -> /bin/sh for ${TARGET}"
	@cp ${.CURDIR}/pconfig.h ${WRKSRC}/pconfig.h
	@WRKSRC="${WRKSRC}" CC="${CC}" PORT_CFLAGS="${PORT_CFLAGS}" \
	    BUILD="${BUILD}" MUSL_ARCH="${MUSL_ARCH}" \
	    sh ${.CURDIR}/build.sh

.include "${.CURDIR}/../../../mk/ports.mk"
