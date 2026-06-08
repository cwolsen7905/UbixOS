# (C) 2002-2026 The UbixOS Project
# ubix.musl.ubix.prog.mk — musl libc (dynamic) + UbixOS native API (static).
#
# Use for programs that need both POSIX (via musl) and UbixOS-specific APIs:
# pidStatus, mpi_createMbox, mpi_postMessage, mpi_fetchMessage, ubix_getcwd.
#
# musl is resolved from /lib/libc.so at runtime.  ubix_api.a is linked
# statically because only a subset of its symbols are in ubix_api.so.
#
# The UbixOS-specific headers (<sys/mpi.h>, <sys/sys.h>, <api/ubix.h>, etc.)
# are exposed via -I${SRCTOP}/include appended AFTER the musl include paths,
# so musl's own stdio.h / unistd.h always take precedence.

.include "${UBIX_MK}/ubix.musl.vars.mk"

EXTRA_LDFLAGS ?=
EXTRA_LIBS    ?=
EXTRA_CFLAGS  ?=

MUSL_INC = ${MUSL_BASE_INC} -I${SRCTOP}/include

MUSL_CFLAGS = ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
              ${ARCH_NOSIMD} ${MUSL_PIE_CFLAGS} -MMD -MP \
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
	$(CC) ${CROSS_M32} -nostdlib ${MUSL_PIE_LDFLAGS} -Wl,-m,${MUSL_LDEMULATION} \
		-Wl,-dynamic-linker,${MUSL_LDSO} \
		-Wl,-rpath,/lib \
		-Wl,-z,noexecstack \
		${EXTRA_LDFLAGS} \
		${MUSL_CRT1} \
		${MUSL_LIB}/crti.o \
		${_OBJS_FULL} \
		${OBJ_DIR}/lib/ubix_api.a \
		-Wl,--start-group \
		${EXTRA_LIBS} \
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
