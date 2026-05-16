# (C) 2002-2026 The UbixOS Project
# ubix.kern.mk — Compile rules for UbixOS kernel component Makefiles.
#
# Include this at the END of a kernel component Makefile, after OBJS is defined.
# Variables supplied by sys/Makefile.incl (via include ../../Makefile.incl +
# include ../Makefile.incl): CC, CXX, CFLAGS, CXXFLAGS, INCLUDES, REMOVE,
#                            OBJ_DIR.
#
# Optional variables set in the component Makefile before .include:
#   CXXFLAGS  — C++ extra flags; sys/Makefile.incl sets -DNOBOOL by default
#   INCLUDES  — append with INCLUDES += to add subsystem-specific paths
#
# Object files and dependency files are written to OBJDIR so that the source
# tree stays clean.  OBJDIR defaults to ${OBJ_DIR}/obj/sys/<component-basename>.

OBJDIR ?= ${OBJ_DIR}/obj/sys/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .s .S .c .cc .C .cpp

.cc.o .C.o .cpp.o:
	@mkdir -p ${OBJDIR}
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: $(OBJS)

clean:
	$(REMOVE) ${OBJDIR}

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
