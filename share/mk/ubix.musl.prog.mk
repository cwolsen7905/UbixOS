# (C) 2002-2026 The UbixOS Project
# ubix.musl.prog.mk — Compile and link rules for musl-linked UbixOS userland programs.
#
# Produces a dynamically-linked ET_EXEC binary: musl libc is resolved from
# /lib/libc.so at runtime via /lib/ld-musl-i386.so.1.
#
# Required variables set before .include:
#   BINARY    — output binary name
#   OBJS      — list of .o files to compile
#
# Optional:
#   EXTRA_LDFLAGS — appended to the link command
#   EXTRA_LIBS    — extra archives/objects inserted before -lc

EXTRA_LDFLAGS ?=
EXTRA_LIBS    ?=
EXTRA_CFLAGS  ?=

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
              -Wa,--noexecstack -Wall -O

OBJDIR ?= ${OBJ_DIR}/obj/bin/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c .S

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(MUSL_CFLAGS) $(MUSL_INC) ${EXTRA_CFLAGS} -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(MUSL_CFLAGS) $(MUSL_INC) ${EXTRA_CFLAGS} -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

$(BINARY): $(OBJS)
	$(CC) ${CROSS_M32} -nostdlib -Wl,-m,elf_i386 \
		-Wl,-dynamic-linker,/lib/ld-musl-i386.so.1 \
		-Wl,-rpath,/lib \
		-Wl,-z,noexecstack \
		${EXTRA_LDFLAGS} \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		${_OBJS_FULL} \
		${EXTRA_LIBS} \
		-Wl,--start-group \
		-L${OBJ_DIR}/lib -lc \
		${OBJ_DIR}/lib/libgcc32.a \
		${LIBGCC} \
		-Wl,--end-group \
		${MUSL_LIB}/crtn.o \
		-o ${OBJ_DIR}/bin/${BINARY}

all: $(BINARY)

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/bin/${BINARY}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
