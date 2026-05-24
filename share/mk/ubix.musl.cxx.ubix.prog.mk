# (C) 2002-2026 The UbixOS Project
# ubix.musl.cxx.ubix.prog.mk — C++ programs with dynamic musl libc, dynamic
#                               libobjgfx, static libcxx/libcxxabi, and static
#                               ubix_api (MPI, syscall wrappers).
#
# Use for programs that need graphics (objgfx), C++ stdlib, MPI, and POSIX.
# libc and libobjgfx are resolved at runtime from /lib/.
#
# Required variables set before .include:
#   BINARY   — output binary name
#   OBJS     — list of .o files to compile (.cc/.cpp sources)
#
# Optional:
#   EXTRA_LDFLAGS — appended to the link command
#   EXTRA_CFLAGS  — appended to per-file compilation

EXTRA_LDFLAGS ?=
EXTRA_CFLAGS  ?=

MUSL_SRC   = ${SRCTOP}/contrib/musl
MUSL_OBJ   = ${OBJ_DIR}/obj/musl
MUSL_LIB   = ${MUSL_OBJ}/lib
LIBCXX_SRC = ${SRCTOP}/contrib/libcxx

MUSL_INC = -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic

LIBCXX_INC = -I${LIBCXX_SRC}/include \
             -I${SRCTOP}/contrib/libcxxabi/include

CXX_CFLAGS = ${CROSS_M32} -std=c++20 \
             -nostdlib -nostdinc -nostdinc++ -fno-builtin \
             -fno-rtti -fno-exceptions \
             -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -fPIC -MMD -MP \
             -Wall -O \
             -D_LIBCPP_HAS_NO_EXCEPTIONS

OBJDIR ?= ${OBJ_DIR}/obj/bin/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c .cc .cpp .S

.cc.o:
	@mkdir -p ${OBJDIR}
	$(CXX) $(CXX_CFLAGS) $(LIBCXX_INC) $(MUSL_INC) -I${SRCTOP}/include \
		${EXTRA_CFLAGS} -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.cpp.o:
	@mkdir -p ${OBJDIR}
	$(CXX) $(CXX_CFLAGS) $(LIBCXX_INC) $(MUSL_INC) -I${SRCTOP}/include \
		${EXTRA_CFLAGS} -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
		-mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -Wall -O \
		$(MUSL_INC) -I${SRCTOP}/include \
		${EXTRA_CFLAGS} -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -nostdlib -nostdinc -Wall \
		-MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

$(BINARY): $(OBJS)
	$(CC) ${CROSS_M32} -nostdlib -Wl,-m,elf_i386 \
		-Wl,-dynamic-linker,/lib/ld-musl-i386.so.1 \
		-Wl,-rpath,/lib \
		${EXTRA_LDFLAGS} \
		${MUSL_LIB}/crt1.o \
		${MUSL_LIB}/crti.o \
		${_OBJS_FULL} \
		${OBJ_DIR}/lib/ubix_api.a \
		-Wl,--start-group \
		-L${OBJ_DIR}/lib -lc -lobjgfx \
		${OBJ_DIR}/lib/libcxx.a \
		${OBJ_DIR}/lib/libcxxabi.a \
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
