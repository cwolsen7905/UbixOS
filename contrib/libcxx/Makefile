# (C) 2002-2026 The UbixOS Project
# LLVM libc++ 18.1.8 — views-first subset for UbixOS/i386 + musl.
# No exceptions, no threads, no locale, no filesystem, no wide chars.

include ../../Makefile.incl
include ../../lib/Makefile.incl

LIBCXX_SRC = ${.CURDIR}
MUSL_SRC   = ${SRCTOP}/contrib/musl
MUSL_OBJ   = ${OBJ_DIR}/obj/musl
OBJDIR     = ${OBJ_DIR}/obj/libcxx
OUTPUT     = ${OBJ_DIR}/lib/libcxx.a

# Flags that tell libc++ what we support (and don't)
# __config_site already provides the feature-flag defines;
# only add build-time defines not present there.
LIBCXX_DEFS = \
	-D_LIBCPP_BUILDING_LIBRARY \
	-D_LIBCPP_HAS_NO_EXCEPTIONS \
	-D_LIBCPP_REMOVE_TRANSITIVE_INCLUDES

INCLUDE = \
	-I${LIBCXX_SRC}/include \
	-I${LIBCXX_SRC}/src \
	-I${SRCTOP}/contrib/libcxxabi/include \
	-I${MUSL_SRC}/include \
	-I${MUSL_OBJ}/obj/include \
	-I${MUSL_SRC}/arch/i386 \
	-I${MUSL_SRC}/arch/generic

CXXFLAGS = ${CROSS_M32} -std=c++20 \
	-Wall -Wno-error \
	-nostdinc -nostdinc++ \
	-fno-builtin -fno-rtti -fno-exceptions \
	-mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
	-fomit-frame-pointer -O2 \
	${LIBCXX_DEFS} ${INCLUDE}

# Core compile units for the views-first subset.
# All containers, algorithms, optional, variant, tuple are header-only.
SRCS = \
	src/string.cpp \
	src/charconv.cpp \
	src/memory.cpp \
	src/stdexcept.cpp \
	src/typeinfo.cpp \
	src/verbose_abort.cpp \
	src/hash.cpp \
	src/any.cpp \
	src/legacy_pointer_safety.cpp \
	src/new.cpp \
	src/new_handler.cpp \
	src/new_helpers.cpp \
	src/ryu/d2fixed.cpp \
	src/ryu/d2s.cpp \
	src/ryu/f2s.cpp

OBJS = ${SRCS:S|src/ryu/||:S|src/||:S|.cpp|.o|}

.PATH: ${LIBCXX_SRC}/src ${LIBCXX_SRC}/src/ryu
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .cpp

.cpp.o:
	@mkdir -p ${OBJDIR}
	${CXX} ${CXXFLAGS} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OUTPUT}

${OUTPUT}: ${OBJS}
	@mkdir -p ${OBJ_DIR}/lib
	${AR} rcs ${OUTPUT} ${OBJS:S|^|${OBJDIR}/|}

clean:
	${REMOVE} ${OBJDIR} ${OUTPUT}
