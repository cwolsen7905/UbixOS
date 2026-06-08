# (C) 2002-2026 The UbixOS Project
# ubix.musl.prog.mk — Compile and link rules for musl-linked UbixOS userland programs.
#
# Produces a dynamically-linked ET_EXEC binary: musl libc is resolved from
# /lib/libc.so at runtime via the per-arch ld-musl (see MUSL_LDSO).
#
# Required variables set before .include:
#   BINARY    — output binary name
#   OBJS      — list of .o files to compile
#
# Optional:
#   EXTRA_LDFLAGS — appended to the link command
#   EXTRA_LIBS    — extra archives/objects inserted before -lc

.include "${UBIX_MK}/ubix.musl.vars.mk"

EXTRA_LDFLAGS ?=
EXTRA_LIBS    ?=
EXTRA_CFLAGS  ?=

MUSL_INC = ${MUSL_BASE_INC} -I${SRCTOP}/include

MUSL_CFLAGS = ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
              ${ARCH_NOSIMD} -MMD -MP \
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
	$(CC) ${CROSS_M32} -nostdlib -Wl,-m,${MUSL_LDEMULATION} \
		-Wl,-dynamic-linker,${MUSL_LDSO} \
		-Wl,-rpath,/lib \
		-Wl,-z,noexecstack \
		${EXTRA_LDFLAGS} \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		${_OBJS_FULL} \
		${EXTRA_LIBS} \
		-Wl,--start-group \
		-L${OBJ_DIR}/lib -lc \
		${MUSL_LIBGCC_COMPAT} \
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
