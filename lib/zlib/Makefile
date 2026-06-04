# (C) 2002-2026 The uBixOS Project
# zlib — compression (Jean-loup Gailly/Mark Adler, zlib license). NetSurf reads
# gzipped resources/messages via inflate. Vendored 1.3.1.
include ../../Makefile.incl
include ../Makefile.incl
MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
Z        = ${SRCTOP}/contrib/zlib
INCLUDES = -I${Z} -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include -I${MUSL_SRC}/arch/i386 -I${MUSL_SRC}/arch/generic
CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon -DHAVE_HIDDEN -DZ_HAVE_UNISTD_H -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -O2
OBJDIR = ${OBJ_DIR}/obj/lib/zlib
OBJS   = adler32.o crc32.o deflate.o infback.o inffast.o inflate.o inftrees.o trees.o zutil.o compress.o uncompr.o gzclose.o gzlib.o gzread.o gzwrite.o
.PATH: ${Z}
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}
all: ${OBJ_DIR}/lib/libz.so
${OBJ_DIR}/lib/libz.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libz.so ${OBJS:S|^|${OBJDIR}/|} -L${OBJ_DIR}/lib -lc -o ${OBJ_DIR}/lib/libz.so
clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libz.so
.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
