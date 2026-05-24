# Kernel Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJDIR   = ${OBJ_DIR}/obj/sys/compile
KERN_OUT = ${OBJ_DIR}/boot/kernel

#Objects
OBJS = null.o

#Kernel Parts — one glob per component OBJDIR (basename of source dir)
KPARTS = \
	${OBJ_DIR}/obj/sys/${_ARCH}/*.o \
	${OBJ_DIR}/obj/sys/init/*.o \
	${OBJ_DIR}/obj/sys/sys/*.o \
	${OBJ_DIR}/obj/sys/vmm/*.o \
	${OBJ_DIR}/obj/sys/lib/*.o \
	${OBJ_DIR}/obj/sys/kernel/*.o \
	${OBJ_DIR}/obj/sys/isa/*.o \
	${OBJ_DIR}/obj/sys/vfs/*.o \
	${OBJ_DIR}/obj/sys/pci/*.o \
	${OBJ_DIR}/obj/sys/devfs/*.o \
	${OBJ_DIR}/obj/sys/procfs/*.o \
	${OBJ_DIR}/obj/sys/mpi/*.o \
	${OBJ_DIR}/obj/sys/ufs/*.o \
	${OBJ_DIR}/obj/sys/fat/*.o \
	${OBJ_DIR}/obj/sys/common/*.o \
	${OBJ_DIR}/obj/sys/net/*.o \
	${OBJ_DIR}/obj/sys/netif/*.o \
	${OBJ_DIR}/obj/sys/api/*.o \
	${OBJ_DIR}/obj/sys/core/*.o \
	${OBJ_DIR}/obj/sys/ipv4/*.o \
	${OBJ_DIR}/obj/sys/usb/*.o
# ${OBJ_DIR}/obj/sys/ipv6/*.o
# ${OBJ_DIR}/obj/sys/ubixfs/*.o

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

all: ${KERN_OUT}

# Link the kernel statically with fixed text+data address
${KERN_OUT}: $(OBJS)
	@mkdir -p ${OBJ_DIR}/boot
	$(LD) -T ./ldscript.${_ARCH} -o ${KERN_OUT} ${_OBJS_FULL} $(KPARTS)

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

clean:
	$(REMOVE) ${OBJDIR} ${KERN_OUT} null.c
