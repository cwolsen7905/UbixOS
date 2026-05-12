# Kernel Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJDIR = ${OBJ_DIR}/sys/compile

#Objects
OBJS = null.o

#Kernel Parts — one glob per component OBJDIR (basename of source dir)
KPARTS = \
	${OBJ_DIR}/sys/${_ARCH}/*.o \
	${OBJ_DIR}/sys/init/*.o \
	${OBJ_DIR}/sys/sys/*.o \
	${OBJ_DIR}/sys/vmm/*.o \
	${OBJ_DIR}/sys/lib/*.o \
	${OBJ_DIR}/sys/kernel/*.o \
	${OBJ_DIR}/sys/isa/*.o \
	${OBJ_DIR}/sys/vfs/*.o \
	${OBJ_DIR}/sys/pci/*.o \
	${OBJ_DIR}/sys/devfs/*.o \
	${OBJ_DIR}/sys/mpi/*.o \
	${OBJ_DIR}/sys/ufs/*.o \
	${OBJ_DIR}/sys/fat/*.o \
	${OBJ_DIR}/sys/common/*.o \
	${OBJ_DIR}/sys/net/*.o \
	${OBJ_DIR}/sys/netif/*.o \
	${OBJ_DIR}/sys/api/*.o \
	${OBJ_DIR}/sys/core/*.o \
	${OBJ_DIR}/sys/ipv4/*.o \
	../sde/*.o
# ${OBJ_DIR}/sys/ipv6/*.o
# ${OBJ_DIR}/sys/ubixfs/*.o

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

# Link the kernel statically with fixed text+data address
$(KERNEL): $(OBJS)
	$(LD) -T ./ldscript.i386 -o $@ ${_OBJS_FULL} $(KPARTS)

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

clean:
	$(REMOVE) ${OBJDIR} $(KERNEL) null.c
