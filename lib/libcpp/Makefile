#  (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

OBJS = libcpp.o

INCLUDE = -I../libcpp/include/ \
          -I${MUSL_SRC}/include \
          -I${MUSL_OBJ}/obj/include \
          -I${MUSL_SRC}/arch/i386 \
          -I${MUSL_SRC}/arch/generic

all: $(OBJS)

.cc.o:
	$(CXX) ${CROSS_M32} -Wall -nostdinc -fno-builtin -fno-rtti -fno-exceptions \
		-mno-sse -mno-sse2 -mno-mmx -mno-3dnow \
		-fomit-frame-pointer -O $(INCLUDE) -c -o $@ $<

clean:
	$(REMOVE) $(OBJS) ${OBJS:.o=.d}
