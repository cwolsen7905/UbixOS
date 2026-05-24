# bin/tcsh/Makefile — build tcsh 6.24.16 for UbixOS/i386
# Sources live in contrib/tcsh-6.24.16; objects go to build/obj/bin/tcsh/.

include ../../Makefile.incl
include ../Makefile.incl

TCSH_SRC = ${SRCTOP}/contrib/tcsh-6.24.16

BINARY = tcsh

# Tell bmake where to find the .c sources.
.PATH.c: ${TCSH_SRC}

# Extra include path so tcsh headers (config.h, sh.h, etc.) are found.
EXTRA_CFLAGS = -I${TCSH_SRC}

OBJS = \
	sh.o sh.dir.o sh.dol.o sh.err.o sh.exec.o sh.char.o \
	sh.exp.o sh.file.o sh.func.o sh.glob.o sh.hist.o sh.init.o \
	sh.lex.o sh.misc.o sh.parse.o sh.print.o sh.proc.o \
	sh.sem.o sh.set.o sh.time.o \
	glob.o dotlock.o mi.termios.o ma.setp.o vms.termcap.o \
	tw.help.o tw.init.o tw.parse.o tw.spell.o tw.comp.o tw.color.o \
	ed.chared.o ed.refresh.o ed.screen.o ed.init.o ed.inputl.o \
	ed.defns.o ed.xmap.o ed.term.o \
	tc.alloc.o tc.bind.o tc.const.o tc.defs.o tc.disc.o tc.func.o \
	tc.nls.o tc.os.o tc.printf.o tc.prompt.o tc.sched.o tc.sig.o \
	tc.str.o tc.vers.o tc.who.o

.include "${UBIX_MK}/ubix.musl.ubix.prog.mk"
