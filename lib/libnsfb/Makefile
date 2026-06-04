# (C) 2002-2026 The uBixOS Project
# libnsfb — NetSurf framebuffer abstraction (vendored, MIT).  Core + RAM
# surface + software plotters only; the SDL/X11/VNC/Wayland backends are
# dropped (host-lib deps).  A uBixOS objGFX surface backend is added next.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
NSFB     = ${SRCTOP}/contrib/libnsfb

INCLUDES = -I${NSFB} -I${NSFB}/include -I${NSFB}/src \
           -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 -I${MUSL_SRC}/arch/generic \
           -I${SRCTOP}/include
# (uBixOS userland headers LAST so musl's sys/* wins over include/sys/* — the
#  ubix surface needs <sys/mpi.h>, <sys/kbd.h>, <sys/mouse.h>, <views/...>.)

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon \
         -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -Wall -Wno-incompatible-pointer-types -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libnsfb
# common.c and 32bpp-common.c are #include templates, not compilation units.
OBJS   = cursor.o dump.o libnsfb.o palette.o surface.o ram.o ubix.o \
         api.o util.o generic.o 32bpp-xrgb8888.o 32bpp-xbgr8888.o 16bpp.o 8bpp.o

.PATH: ${NSFB}/src ${NSFB}/src/surface ${NSFB}/src/plot
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libnsfb.so
${OBJ_DIR}/lib/libnsfb.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libnsfb.so ${OBJS:S|^|${OBJDIR}/|} -L${OBJ_DIR}/lib -lc -o ${OBJ_DIR}/lib/libnsfb.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libnsfb.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
