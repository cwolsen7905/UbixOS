# Copyright (c) 2002-2026 The UbixOS Project.

include ../../Makefile.incl
include ../Makefile.incl

BINARY  = taskbar
OBJS    = taskbar.o
OBJDIR  = ${OBJ_DIR}/obj/bin/taskbar

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_LIB = ${OBJ_DIR}/obj/musl/lib

LIBCXX_SRC = ${SRCTOP}/contrib/libcxx

INCLUDES = \
	-I${LIBCXX_SRC}/include \
	-I${SRCTOP}/contrib/libcxxabi/include \
	-I${MUSL_SRC}/include \
	-I${OBJ_DIR}/obj/musl/obj/include \
	-I${MUSL_SRC}/arch/i386 \
	-I${MUSL_SRC}/arch/generic \
	-I../../include

OBJGFX_OBJS != ls ${OBJ_DIR}/obj/gfx/objgfx/*.o 2>/dev/null

.SUFFIXES: .o .cc
.cc.o:
	@mkdir -p ${OBJDIR}
	$(CXX) -std=c++20 -Wall -O $(CFLAGS) \
		-nostdinc -nostdinc++ -fno-builtin \
		-fno-rtti -fno-exceptions \
		-mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
		-D_LIBCPP_HAS_NO_EXCEPTIONS \
		-MMD -MP \
		$(INCLUDES) -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${BINARY}

${BINARY}: ${OBJS}
	$(CC) $(CFLAGS) -Wl,-m,elf_i386 \
		-o ${BUILD_DIR}/bin/${BINARY} \
		${MUSL_LIB}/crt1.o ${MUSL_LIB}/crti.o \
		-Wl,--start-group \
		${OBJ_DIR}/obj/ubix_api/mpi_creatembox.o \
		${OBJ_DIR}/obj/ubix_api/mpi_fetchmessage.o \
		${OBJ_DIR}/obj/ubix_api/mpi_postmessage.o \
		${OBJ_DIR}/obj/ubix_api/mpi_destroymbox.o \
		${OBJ_DIR}/obj/ubix_api/gettime.o \
		${OBJ_DIR}/obj/ubix_api/sys_gettimeofday.o \
		${OBJGFX_OBJS} \
		${OBJ_DIR}/lib/libcxx.a \
		${OBJ_DIR}/lib/libcxxabi.a \
		${OBJDIR}/taskbar.o \
		${OBJ_DIR}/lib/musl.a \
		${OBJ_DIR}/lib/crt.a \
		-Wl,--end-group \
		${LIBGCC} \
		${MUSL_LIB}/crtn.o

clean:
	$(REMOVE) ${OBJDIR} ${BUILD_DIR}/bin/${BINARY}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
