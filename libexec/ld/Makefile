# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OUTPUT = ld.so
OBJS   = main.o findlibrary.o findfunc.o addlibrary.o entry.o
OBJDIR = ${OBJ_DIR}/obj/libexec/ld

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

$(OUTPUT): $(OBJS)
	${CROSS_PREFIX}ld -m elf_i386 -shared --soname=$(OUTPUT) -e _ld \
		-o ../../build/libexec/$(OUTPUT) ${_OBJS_FULL} $(LIBRARIES)

.SUFFIXES: .o .c .cc .S

.cc.o .cpp.o:
	@mkdir -p ${OBJDIR}
	$(CXX) -Wall -O $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) -Wall -O $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) -Wall $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.PATH.o: ${OBJDIR}

all: $(OUTPUT)

clean:
	$(REMOVE) ${OBJDIR} ../../build/libexec/$(OUTPUT)

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
