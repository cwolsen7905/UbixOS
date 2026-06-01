# (C) 2002-2026 The UbixOS Project
# sys/exec — ELF binary loader and exec layer
# ELF parsing, execve syscall, dynamic linker integration.

include ../../Makefile.incl
include ../Makefile.incl

OBJS = \
	elf.o \
	execve.o \
	kern_execve.o \
	ld.o

.include "${UBIX_MK}/ubix.kern.mk"
