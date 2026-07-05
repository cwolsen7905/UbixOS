# Host build for the UbixFS tools + milestone tests.
# These are plain host (macOS/FreeBSD/Linux) programs that link the portable
# UbixFS C core (lib/ubixfs_core) — no cross compiler, no kernel.
#
#   make            build the ubfs CLI + all *_test harnesses
#   make test       build and run every milestone test
#   make clean

.CURDIR ?= ${.PARSEDIR}
CORE     = ${.CURDIR}/../../lib/ubixfs_core
INC      = ${.CURDIR}/../../include/fs/ubixfs

HOSTCC  ?= cc
# -idirafter (not -I) so include/ does not shadow host system headers
# (UbixOS ships include/machine/signal.h etc.).
CFLAGS  += -O2 -g -Wall -Wextra -std=c11 -I${CORE} -I${INC} -idirafter ${.CURDIR}/../../include

CORE_SRCS = ${CORE}/fletcher.c ${CORE}/ubfs_spa.c ${CORE}/ubfs_dmu.c \
            ${CORE}/ubfs_dir.c ${CORE}/ubfs_dsl.c ${CORE}/ubfs_fs.c

TESTS = spa_test dmu_test dsl_test fs_test recordsize_test leak_test
PROGS = ubfs ${TESTS}

all: ${PROGS}

ubfs: ubfs.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}

spa_test: spa_test.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}
dmu_test: dmu_test.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}
dsl_test: dsl_test.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}
fs_test: fs_test.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}
recordsize_test: recordsize_test.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}
leak_test: leak_test.c ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}

test: ${TESTS}
	@for t in ${TESTS}; do echo "== $$t =="; ./$$t || exit 1; echo; done
	@echo "ALL UBIXFS TESTS PASS"

clean:
	rm -f ${PROGS} *.o
