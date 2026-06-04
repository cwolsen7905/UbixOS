# (C) 2002-2026 The uBixOS Project
# libutf8proc — Unicode normalisation (JuliaStrings, MIT/Unicode). NetSurf core
# dep. utf8proc.c #includes the shipped utf8proc_data.c table (no codegen).
include ../../Makefile.incl
include ../Makefile.incl
MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
U8P      = ${SRCTOP}/contrib/libutf8proc
INCLUDES = -I${U8P} -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include -I${MUSL_SRC}/arch/i386 -I${MUSL_SRC}/arch/generic
CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon -DUTF8PROC_STATIC -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -Wall -O2
OBJDIR = ${OBJ_DIR}/obj/lib/libutf8proc
OBJS   = utf8proc.o
.PATH: ${U8P}
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}
all: ${OBJ_DIR}/lib/libutf8proc.so
${OBJ_DIR}/lib/libutf8proc.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libutf8proc.so ${OBJS:S|^|${OBJDIR}/|} -L${OBJ_DIR}/lib -lc -o ${OBJ_DIR}/lib/libutf8proc.so
clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libutf8proc.so
.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
