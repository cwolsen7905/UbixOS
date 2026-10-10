# bin/sh/Makefile — the POSIX shell /bin/sh: dash (contrib/dash, BSD licence,
# see UBIXOS_UPSTREAM_REV).  make, configure and build recipes need it.

include ../../Makefile.incl
include ../Makefile.incl

DASH_SRC = ${SRCTOP}/contrib/dash/src

BINARY = sh

.PATH.c: ${DASH_SRC} ${DASH_SRC}/bltin

EXTRA_CFLAGS = -include ${SRCTOP}/contrib/dash/config.h -DBSD=1 -DSHELL \
               -I${DASH_SRC} -w

OBJS = alias.o arith_yacc.o arith_yylex.o cd.o error.o eval.o exec.o expand.o \
       histedit.o input.o jobs.o mail.o main.o memalloc.o miscbltin.o \
       mystring.o options.o parser.o redir.o show.o trap.o output.o \
       printf.o system.o test.o times.o var.o \
       builtins.o init.o nodes.o signames.o syntax.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
