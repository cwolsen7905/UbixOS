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

EXTRA_LDFLAGS ?=

MUSL_SRC  = ${SRCTOP}/contrib/musl
MUSL_OBJ  = ${OBJ_DIR}/obj/musl
MUSL_LIB  = ${MUSL_OBJ}/lib

LIBCXX_SRC = ${SRCTOP}/contrib/libcxx

MUSL_INC  = -I${MUSL_SRC}/include \
            -I${MUSL_OBJ}/obj/include \
            -I${MUSL_SRC}/arch/i386 \
            -I${MUSL_SRC}/arch/generic

LIBCXX_INC = -I${LIBCXX_SRC}/include \
             -I${SRCTOP}/contrib/libcxxabi/include

CXX_CFLAGS = ${CROSS_M32} -std=c++20 \
             -nostdlib -nostdinc -nostdinc++ -fno-builtin \
             -fno-rtti -fno-exceptions \
             -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP \
             -Wall -O \
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
	$(CC) ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
		-mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -Wall -O \
		$(MUSL_INC) -I${SRCTOP}/include -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib -nostdinc -Wall \
		-MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

$(BINARY): $(OBJS)
	$(CC) ${CROSS_M32} -nostdlib -static -Wl,-m,elf_i386 ${EXTRA_LDFLAGS} \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		-Wl,--start-group \
		${_OBJS_FULL} \
		${OBJ_DIR}/lib/libcxx.a \
		${OBJ_DIR}/lib/libcxxabi.a \
		${OBJ_DIR}/lib/musl.a \
		-Wl,--end-group \
		${MUSL_LIB}/crtn.o \
		-o ${OBJ_DIR}/bin/${BINARY}

all: $(BINARY)

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/bin/${BINARY}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
