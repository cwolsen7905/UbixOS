# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../../Makefile.inc
#include ../../../Makefile.inc
#include ../Makefile.inc
include ../../libc.inc

#INCLUDES += -I../

#CFLAGS += -fpic -DPIC
#CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe  -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Werror -Wall -Wno-format-y2k -Wno-uninitialized -nostdinc -nostdlib

#Objects
OBJS = Ovfork.o brk.o cerror.o exect.o getcontext.o i386_clr_watch.o i386_get_fsbase.o i386_get_gsbase.o i386_get_ioperm.o i386_get_ldt.o i386_set_fsbase.o i386_set_gsbase.o i386_set_ioperm.o i386_set_ldt.o i386_set_watch.o i386_vm86.o pipe.o ptrace.o reboot.o sbrk.o setlogin.o sigreturn.o syscall.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

