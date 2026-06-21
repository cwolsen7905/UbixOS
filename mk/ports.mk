# mk/ports.mk — reusable third-party "port" recipe.
#
# Fetch a pinned upstream tarball, verify its SHA-256, extract it into a
# gitignored cache, apply a committed patch series, then hand off to the port's
# own do-build / do-install rules.  This is the framework the ports policy
# (docs/design/third-party-ports-plan.md) is built on — NOT a vendored copy.
#
# A port lives in tools/ports/<name>/Makefile and sets, before .include'ing this:
#
#   PORT_NAME      short name (e.g. bmake)
#   PORT_VERSION   upstream version pin (e.g. 20240212)
#   PORT_URL       full tarball URL
#   PORT_SHA256    expected SHA-256 of the tarball
#   PORT_DISTFILE  tarball basename (default: ${PORT_URL:T})
#   do-build:      the port's own build rule (cross-compile against the UbixOS
#                  musl toolchain)
#   do-install:    (optional) stage artifacts (e.g. into the image / a bindir)
#
# then:  .include "${.CURDIR}/../../../mk/ports.mk"
#
# Cache (all gitignored, under build/ports/):
#   build/ports/distfiles/<tarball>          the verified download
#   build/ports/<name>-<version>/            the extracted + patched worktree
#
# Targets:  fetch  extract  patch  build(default)  install  clean

SRCTOP        ?= ${.CURDIR}/../../..
PORT_DISTFILE ?= ${PORT_URL:T}
PORT_DISTDIR  ?= ${SRCTOP}/build/ports/distfiles
PORT_WRKDIR   ?= ${SRCTOP}/build/ports/${PORT_NAME}-${PORT_VERSION}
PORT_PATCHDIR ?= ${.CURDIR}/patches

FETCH  ?= curl -fsSL
SHA256 ?= shasum -a 256

_DISTFILE = ${PORT_DISTDIR}/${PORT_DISTFILE}

# Expose the worktree to the port's rules.
WRKSRC = ${PORT_WRKDIR}

all: build

fetch:
	@mkdir -p ${PORT_DISTDIR}
	@if [ ! -f ${_DISTFILE} ]; then \
		echo "==> fetch ${PORT_URL}"; \
		${FETCH} -o ${_DISTFILE} '${PORT_URL}'; \
	fi
	@got=`${SHA256} ${_DISTFILE} | awk '{print $$1}'`; \
	if [ "$$got" != "${PORT_SHA256}" ]; then \
		echo "==> SHA-256 MISMATCH for ${PORT_DISTFILE}"; \
		echo "    got  $$got"; echo "    want ${PORT_SHA256}"; \
		exit 1; \
	fi; \
	echo "==> verified ${PORT_DISTFILE} (sha256 ok)"

extract: fetch
	@if [ ! -f ${PORT_WRKDIR}/.extracted ]; then \
		echo "==> extract -> ${PORT_WRKDIR}"; \
		rm -rf ${PORT_WRKDIR}; mkdir -p ${PORT_WRKDIR}; \
		tar xzf ${_DISTFILE} -C ${PORT_WRKDIR} --strip-components=1; \
		touch ${PORT_WRKDIR}/.extracted; \
	fi

patch: extract
	@if [ ! -f ${PORT_WRKDIR}/.patched ]; then \
		if [ -d ${PORT_PATCHDIR} ]; then \
			for p in ${PORT_PATCHDIR}/*.patch; do \
				[ -f "$$p" ] || continue; \
				echo "==> apply patch $${p##*/}"; \
				( cd ${PORT_WRKDIR} && patch -p1 < "$$p" ) || exit 1; \
			done; \
		fi; \
		touch ${PORT_WRKDIR}/.patched; \
	fi

build: patch do-build

install: build do-install

# Default no-op hooks if the port doesn't override them.
.if !target(do-build)
do-build:
	@echo "==> ${PORT_NAME}: no do-build rule"
.endif

.if !target(do-install)
do-install:
	@:
.endif

clean:
	rm -rf ${PORT_WRKDIR}

.PHONY: all fetch extract patch build install clean do-build do-install
