# $FreeBSD: releng/11.1/lib/libc/powerpc64/sys/Makefile.inc 305329 2016-09-03 09:03:40Z kib $

MDASM+=	brk.S cerror.S exect.S sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o getlogin.o openbsd_poll.o sstk.o yield.o

PSEUDO=	_getlogin.o _exit.o
