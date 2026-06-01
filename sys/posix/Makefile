# (C) 2002-2026 The UbixOS Project
# sys/posix — POSIX compatibility layer
# Signals, TTY/termios, pipes, semaphores, time, VFS syscalls,
# POSIX syscall table and dispatcher.

include ../../Makefile.incl
include ../Makefile.incl

OBJS = \
	gen_calls.o \
	kern_pipe.o \
	kern_sig.o \
	pipe.o \
	sem.o \
	signal.o \
	syscall_posix.o \
	syscalls_posix.o \
	time.o \
	tty.o \
	vfs_calls.o

.include "${UBIX_MK}/ubix.kern.mk"
