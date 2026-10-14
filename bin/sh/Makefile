# bin/sh/Makefile (C) 2002-2026 The UbixOS Project
# /bin/sh: oksh (portable OpenBSD ksh, public domain).  bmake runs every recipe
# through it.  Sources live in contrib/oksh-7.5 (pristine upstream); pconfig.h
# here is oksh's configure output set for musl.
#
# -DNO_CURSES: no terminfo library on uBixOS.  EMACS + VI: oksh needs one.
# oksh's headers rely on a system header chain musl does not provide, so
# sys/cdefs.h (the bmake shim), sys/types.h, stdint.h and sys/file.h are
# force-included; _GNU_SOURCE exposes the BSD types (u_char, u_int, ...).

include ../../Makefile.incl
include ../Makefile.incl

OKSH_SRC = ${SRCTOP}/contrib/oksh-7.5

BINARY = sh

.PATH.c: ${OKSH_SRC}

EXTRA_CFLAGS = -I${.CURDIR} -I${.CURDIR}/../bmake/shim -I${OKSH_SRC} -std=gnu99 -w \
	-D_GNU_SOURCE -include sys/cdefs.h -include sys/types.h \
	-include stdint.h -include sys/file.h \
	-DMAXLOGNAME=32 -DEMACS -DVI -DNO_CURSES

OBJS = alloc.o asprintf.o c_ksh.o c_sh.o c_test.o c_ulimit.o edit.o emacs.o \
	eval.o exec.o expr.o history.o io.o jobs.o lex.o mail.o main.o misc.o \
	path.o shf.o syn.o table.o trap.o tree.o tty.o var.o version.o vi.o \
	confstr.o reallocarray.o siglist.o signame.o strlcat.o strlcpy.o \
	strtonum.o unvis.o vis.o issetugid.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
