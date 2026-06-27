# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJDIR  = ${OBJ_DIR}/obj/ubix_api

# The full object set.  Every symbol here is ubix-specific (ubistry_*, mpi_*, ubix_*,
# pidStatus, pty_*, tty_*, ubix_getcwd, …) with NO musl libc collision, so the whole
# set ships in the shared object.  mount()/unmount() — which DID collide with musl's
# mount() — moved to bin/automountd (its only caller) as fsmount.c.
OBJS    = ubixcwd.o ttyctrl.o settty.o pty.o ubistry.o ulog.o views.o \
          pidstatus.o netcfg.o hostname.o display.o ubfspool.o ubixdisk.o \
          mpi_creatembox.o mpi_postmessage.o mpi_fetchmessage.o mpi_waitmessage.o mpi_destroymbox.o \
          mpi_rpc.o \
          sys_gettimeofday.o gettime.o sysinfo.o

# Programs link -lubix_api → libubix_api.so (resolved from /lib at runtime via rpath),
# so a fix to any of these objects is picked up by rebuilding the .so alone, with no
# app relink.  The static .a is still built for the rare consumer that bakes it in.
OBJS_SO = ${OBJS}
OBJS_A  = ${OBJS}

OUTPUT  = libubix_api.so

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
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m ${MUSL_LDEMULATION} -shared -soname libubix_api.so \
		${OBJS_SO:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lc \
		-o ${OBJ_DIR}/lib/$(OUTPUT)


lib.a: $(OBJS_A)
	$(AR) rcs ${OBJ_DIR}/lib/ubix_api.a \
		${OBJS_A:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} \
		${OBJ_DIR}/lib/$(OUTPUT) ${OBJ_DIR}/lib/ubix_api.a

.for _d in ${OBJS_A:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
