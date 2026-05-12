# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

CXXFLAGS = -DNOBOOL -D__UBIXOS_KERNEL__ -fno-rtti
INCLUDES += -I../../lib/objgfx40/

OBJS = strstr.o strchr.o strlen.o strncpy.o assert.o sqrt.o atan.o divdi3.o libcpp.o strtok.o kmalloc.o kprintf.o vsprintf.o string.o net.o strtol.o ogprintf.o kern_trie.o

.include "${UBIX_MK}/ubix.kern.mk"
