# (C) 2002-2026 The UbixOS Project
# ubix.musl.cxx.prog.mk — Compile and link rules for musl + libc++ C++ programs.
#
# Like ubix.musl.prog.mk but adds:
#   - libcxx.a   (LLVM libc++ — <string>, <vector>, <map>, <memory>, ...)
#   - libcxxabi.a (Itanium C++ ABI — new/delete, guards, pure virtual)
#   - C++20 compilation via g++
#
# Usage: set BINARY, OBJS (and optionally EXTRA_LDFLAGS), then:
#   .include "${UBIX_MK}/ubix.musl.cxx.prog.mk"

.include "${UBIX_MK}/ubix.musl.vars.mk"

EXTRA_LDFLAGS ?=
EXTRA_LIBS    ?=
EXTRA_CFLAGS  ?=

# Install subdir under ${OBJ_DIR} (see ubix.musl.prog.mk); per-tree Makefile.incl overrides.
BINDIR ?= bin

MUSL_INC = ${MUSL_BASE_INC}

CXX_CFLAGS = ${CROSS_M32} -std=c++20 \
             -nostdlib ${TC_NOSTDINC} -nostdinc++ -fno-builtin \
             -fno-rtti -fno-exceptions \
             ${ARCH_NOSIMD} -MMD -MP \
             -Wa,--noexecstack -Wall -Wno-unused-command-line-argument -O \
             -D_LIBCPP_HAS_NO_EXCEPTIONS

OBJDIR ?= ${OBJ_DIR}/obj/bin/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c .cc .cpp .S

.cc.o:
	@mkdir -p ${OBJDIR}
	$(CXX) $(CXX_CFLAGS) $(LIBCXX_INC) $(MUSL_INC) -I${SRCTOP}/include -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.cpp.o:
	@mkdir -p ${OBJDIR}
	$(CXX) $(CXX_CFLAGS) $(LIBCXX_INC) $(MUSL_INC) -I${SRCTOP}/include -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib ${TC_NOSTDINC} -fno-builtin \
		${ARCH_NOSIMD} -MMD -MP -Wall -O \
		$(MUSL_INC) -I${SRCTOP}/include -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib ${TC_NOSTDINC} -Wall \
		-MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

$(BINARY): $(OBJS)
	@mkdir -p ${OBJ_DIR}/${BINDIR}
	$(CC) ${CROSS_M32} -nostdlib -static -Wl,-m,${MUSL_LDEMULATION} -Wl,-z,noexecstack ${EXTRA_LDFLAGS} \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		-Wl,--start-group \
		${_OBJS_FULL} \
		${EXTRA_LIBS} \
		${OBJ_DIR}/lib/libcxx.a \
		${OBJ_DIR}/lib/libcxxabi.a \
		${OBJ_DIR}/lib/musl.a \
		-Wl,--end-group \
		${MUSL_LIB}/crtn.o \
		-o ${OBJ_DIR}/${BINDIR}/${BINARY}

all: $(BINARY)

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/${BINDIR}/${BINARY}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
