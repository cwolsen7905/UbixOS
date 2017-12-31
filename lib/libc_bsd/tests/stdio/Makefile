# $FreeBSD: releng/10.2/lib/libc/tests/stdio/Makefile 277446 2015-01-20 21:59:48Z ngie $

TESTSDIR=	${TESTSBASE}/lib/libc/stdio

ATF_TESTS_C=		fmemopen2_test

NETBSD_ATF_TESTS_C=	clearerr_test
NETBSD_ATF_TESTS_C+=	fflush_test
NETBSD_ATF_TESTS_C+=	fmemopen_test
NETBSD_ATF_TESTS_C+=	fopen_test
NETBSD_ATF_TESTS_C+=	fputc_test
NETBSD_ATF_TESTS_C+=	mktemp_test
NETBSD_ATF_TESTS_C+=	popen_test
NETBSD_ATF_TESTS_C+=	printf_test
NETBSD_ATF_TESTS_C+=	scanf_test

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
