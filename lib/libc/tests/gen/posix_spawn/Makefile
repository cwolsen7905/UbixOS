# $FreeBSD: releng/11.1/lib/libc/tests/gen/posix_spawn/Makefile 299094 2016-05-04 23:20:53Z ngie $

.include <ubix.own.mk>

BINDIR=		${TESTSDIR}

NETBSD_ATF_TESTS_C=	fileactions_test
NETBSD_ATF_TESTS_C+=	spawn_test
NETBSD_ATF_TESTS_C+=	spawnattr_test

PROGS=		h_fileactions
PROGS+=		h_spawn
PROGS+=		h_spawnattr

SCRIPTS=	h_nonexec
SCRIPTS+=	h_zero

CLEANFILES+=	h_nonexec

.include "../../Makefile.netbsd-tests"

h_zero:
	dd if=/dev/zero of=h_zero bs=1k count=2 status=none
	chmod a+x h_zero

CLEANFILES+=	h_zero

WARNS?=3

.include <ubix.test.mk>
