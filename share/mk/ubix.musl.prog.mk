# (C) 2002-2026 The UbixOS Project
# ubix.musl.prog.mk — Compile and link rules for musl-linked UbixOS userland programs.
#
# Drop-in replacement for ubix.prog.mk that links against musl libc (static)
# instead of the native dynamic libc.so.  Use for programs that need POSIX
# compliance via musl rather than the FreeBSD-derived native libc.
#
# Usage: identical to ubix.prog.mk — just change the .include at the bottom
# of your program Makefile from ubix.prog.mk to ubix.musl.prog.mk.
#
# Required variables set before .include:
#   BINARY    — output binary name
#   OBJS      — list of .o files to compile
#
# Optional:
#   EXTRA_LDFLAGS — appended to the link command

EXTRA_LDFLAGS ?=

MUSL_SRC  = ${SRCTOP}/contrib/musl
MUSL_OBJ  = ${OBJ_DIR}/obj/musl
MUSL_LIB  = ${MUSL_OBJ}/lib

MUSL_INC  = -I${MUSL_SRC}/include \
            -I${MUSL_OBJ}/obj/include \
            -I${MUSL_SRC}/arch/i386 \
            -I${MUSL_SRC}/arch/generic \
            -I${SRCTOP}/include

MUSL_CFLAGS = ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
              -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP \
              -Wall -O

OBJDIR ?= ${OBJ_DIR}/obj/bin/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c .S

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(MUSL_CFLAGS) $(MUSL_INC) -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(MUSL_CFLAGS) $(MUSL_INC) -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

$(BINARY): $(OBJS)
	$(CC) ${CROSS_M32} -nostdlib -static -Wl,-m,elf_i386 ${EXTRA_LDFLAGS} \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		${_OBJS_FULL} \
		${OBJ_DIR}/lib/musl.a \
		${OBJ_DIR}/lib/crt.a \
		${LIBGCC} \
		${MUSL_LIB}/crtn.o \
		-o ${OBJ_DIR}/bin/${BINARY}

all: $(BINARY)

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/bin/${BINARY}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
