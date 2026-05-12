# (C) 2002-2026 The UbixOS Project
# ubix.sublib.mk — Compile rules for libc sub-directory Makefiles.
#
# Include this at the END of a libc sub-directory Makefile, after OBJS is defined.
# Variables supplied by lib/Makefile.incl: CC, CXX, CFLAGS, REMOVE, OBJ_DIR.
# INCLUDES defaults to -I${SRCTOP}/include (from lib/Makefile.incl).
# Each sub-directory Makefile should append its local include paths:
#   INCLUDES += -I../include   (most sub-dirs)
#
# Object and dependency files are written to OBJDIR so the source tree stays
# clean.  OBJDIR defaults to ${OBJ_DIR}/lib/<sub-dir-basename>.

OBJDIR ?= ${OBJ_DIR}/obj/lib/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .s .S .c .cc .C .cpp

.cc.o .C.o .cpp.o:
	@mkdir -p ${OBJDIR}
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

all: $(OBJS)

clean:
	$(REMOVE) ${OBJDIR}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
