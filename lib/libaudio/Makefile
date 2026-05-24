# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJDIR  = ${OBJ_DIR}/obj/libaudio

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

MUSL_INC = -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic \
           -I${SRCTOP}/include

MUSL_CFLAGS = ${CROSS_M32} -nostdlib -nostdinc -fno-builtin \
              -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP \
              -Wall -O

OBJS = audio.o

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(MUSL_CFLAGS) $(MUSL_INC) -MT ${.TARGET:T} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libaudio.a

${OBJ_DIR}/lib/libaudio.a: $(OBJS)
	$(AR) rcs ${OBJ_DIR}/lib/libaudio.a ${OBJS:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libaudio.a

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
