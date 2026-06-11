# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJDIR  = ${OBJ_DIR}/obj/ubix_api

# Objects for the dynamic .so (original ABI — picked up by old-libc programs)
OBJS_SO = ubixcwd.o ttyctrl.o

# Objects for the static ubix_api.a used by musl-linked programs.
# pidStatus and MPI stubs duplicate symbols in the old libc; they must NOT
# land in the source tree where LIBRARIES globs would pick them up.
OBJS_A  = ubixcwd.o ttyctrl.o settty.o pty.o ubistry.o ulog.o views.o \
          pidstatus.o netcfg.o display.o ubfspool.o \
          mpi_creatembox.o mpi_postmessage.o mpi_fetchmessage.o mpi_destroymbox.o \
          sys_gettimeofday.o gettime.o mount.o sysinfo.o

OUTPUT  = ubix_api.so

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c .S .cc

.cc.o:
	@mkdir -p ${OBJDIR}
	$(CXX) -Wall -nostdinc -O -I./include \
		-MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) ${CROSS_M32} -Wall -nostdinc -O \
		-I${SRCTOP}/contrib/musl/include \
		-I${OBJ_DIR}/obj/musl/obj/include \
		-I${SRCTOP}/contrib/musl/arch/${MUSL_ARCH} \
		-I${SRCTOP}/contrib/musl/arch/generic \
		-I./include -I../../include \
		-MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) ${CROSS_M32} -Wall -nostdinc \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: lib.so lib.a

lib.so: $(OBJS_SO)
	$(CC) ${CROSS_M32} -nostdlib -shared -Wl,-soname,ubix_api.so -Wl,-m,${MUSL_LDEMULATION} \
		-o ${OBJ_DIR}/lib/$(OUTPUT) \
		${OBJS_SO:S|^|${OBJDIR}/|}

lib.a: $(OBJS_A)
	$(AR) rcs ${OBJ_DIR}/lib/ubix_api.a \
		${OBJS_A:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} \
		${OBJ_DIR}/lib/$(OUTPUT) ${OBJ_DIR}/lib/ubix_api.a

.for _d in ${OBJS_A:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
