# (C) 2002-2026 The UbixOS Project
# sys/kern — native UbixOS kernel core
# Scheduler, logging, sysctl, fd table, native syscall dispatch.

include ../../Makefile.incl
include ../Makefile.incl

OBJS = \
	access.o \
	descrip.o \
	endtask.o \
	fb.o \
	kern_sysctl.o \
	klog.o \
	random.o \
	sched_core.o \
	shutdown.o \
	syscall.o \
	syscalls.o \
	ubthread.o \
	vitals.o

.include "${UBIX_MK}/ubix.kern.mk"
